/* Bounded metadata only: no link keys, PINs, passkeys, ACL payloads or allocation. */
#include <stdint.h>
#include <string.h>
#include "bluetooth-trace.h"
#include "bluetooth-hid.h"
extern unsigned stock_ticks(void), runtime_irq_save(void);
extern void runtime_irq_restore(unsigned);
extern void stock_reply(unsigned, unsigned, const void *, unsigned);
#define CAPACITY 32
struct trace_event {
    uint32_t sequence, ms;
    uint8_t kind, event, length, reserved, data[12];
};
_Static_assert(sizeof(struct trace_event)==24,"Bluetooth trace wire ABI");
static struct trace_event history[CAPACITY];
static uint32_t sequence;

void runtime_bt_trace(unsigned kind,unsigned event,const void *data,unsigned length) {
    if (length>12 || (length && !data)) return;
    struct trace_event row={.ms=stock_ticks(),.kind=kind,.event=event,.length=length};
    if (length) memcpy(row.data,data,length);
    unsigned irq=runtime_irq_save();
    /* Restart the cursor at the practically unreachable 2^32-event boundary. */
    if (++sequence==0) { memset(history,0,sizeof history);sequence=1; }
    row.sequence=sequence;history[(sequence-1)%CAPACITY]=row;
    runtime_irq_restore(irq);
}
void runtime_bt_trace_hci(const unsigned char *packet) {
    if (!packet) return;
    uint16_t size;const unsigned char *p;
    memcpy(&size,packet+8,2);memcpy(&p,packet+12,sizeof p);
    if (!p || size<2 || size!=(unsigned)p[1]+2) return;
    unsigned event=p[0],n=p[1];p+=2;
    switch (event) {
    case 3: if(n!=11)return;break; /* Connection complete. */
    case 4: if(n!=10)return;break; /* Connection request. */
    case 5: case 8: if(n!=4)return;break; /* Disconnect / encryption. */
    case 6: if(n!=3)return;break; /* Authentication complete. */
    case 15: if(n!=4 || !p[0])return;break; /* Failed command status. */
    case 0x16: case 0x17: case 0x31: case 0x34: case 0x35:
        if(n!=6)return;
        break;
    case 0x18: { /* Link key notification: address and type, never the key. */
        if(n!=23)return;
        unsigned char safe[7];memcpy(safe,p,6);safe[6]=p[22];
        runtime_bt_trace(1,event,safe,sizeof safe);return;
    }
    case 0x32: if(n!=9)return;break; /* Remote IO capability / auth requirements. */
    case 0x33: if(n!=10)return;n=6;break; /* Omit numeric confirmation value. */
    case 0x36: if(n!=7)return;break; /* Simple pairing complete. */
    default:return;
    }
    runtime_bt_trace(1,event,p,n);
}
void runtime_bt_trace_stack(unsigned event,const unsigned char *params) {
    if (event<1 || event>16 || event==3 || event==5 || event==9) return;
    /* Most callbacks leave the common status/error fields uninitialized. */
    unsigned char data[2]={0};unsigned n=0;
    if(params && (event==8 || event==10)) { data[0]=params[8];n=1; }
    else if(params && event==15) { memcpy(data,params+4,2);n=2; }
    runtime_bt_trace(2,event,data,n);
}
void runtime_bt_trace_read(unsigned context,const unsigned char *data,unsigned length) {
    unsigned char reply[16+6*sizeof(struct trace_event)]={'D','B','T','R',1,0,0,CAPACITY};
    uint32_t after=0,latest,oldest;unsigned count=0;
    if(length!=13) reply[5]=1;
    else memcpy(&after,data+7,4);
    unsigned irq=runtime_irq_save();
    latest=sequence;oldest=latest>CAPACITY ? latest-CAPACITY+1 : 1;
    if (!reply[5]) {
        uint32_t next=after>=latest ? latest+1 : after+1;
        if(next<oldest) next=oldest;
        while(next && next<=latest && count<6) {
            memcpy(reply+16+count*sizeof(struct trace_event),&history[(next-1)%CAPACITY],sizeof(struct trace_event));
            ++next;++count;
        }
    }
    runtime_irq_restore(irq);
    reply[6]=count;memcpy(reply+8,&oldest,4);memcpy(reply+12,&latest,4);
    stock_reply(context,0x37,reply,16+count*sizeof(struct trace_event));
}

unsigned runtime_bt_trace_app_disconnect(unsigned caller) {
    unsigned blocked=runtime_hid_preserve_link(caller);
    runtime_bt_trace(6,blocked ? 4 : 1,&caller,4);
    return blocked;
}
void runtime_bt_trace_force_disconnect(const unsigned char *remote,unsigned reason,unsigned force,unsigned caller) {
    unsigned char data[12]={0};memcpy(data,&caller,4);
    if(remote) memcpy(data+4,remote+0x54,6);
    data[10]=reason;data[11]=force;
    runtime_bt_trace(6,2,data,sizeof data);
}
void runtime_bt_trace_link_disconnect(const unsigned char *remote,unsigned caller) {
    unsigned char data[12]={0};memcpy(data,&caller,4);
    if(remote) { memcpy(data+4,remote+0x54,6);data[10]=remote[0x99];data[11]=remote[0x96]; }
    runtime_bt_trace(6,3,data,sizeof data);
}
