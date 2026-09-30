#include <assert.h>
#include <stdio.h>
#include "bluetooth-trace.c"
static unsigned irq_disabled;
static unsigned char response[160];
static unsigned response_size;
static unsigned preserve;
unsigned runtime_hid_preserve_link(unsigned caller) { (void)caller;return preserve; }
unsigned stock_ticks(void) { return 1234; }
unsigned runtime_irq_save(void) { unsigned was=irq_disabled;irq_disabled=1;return !was; }
void runtime_irq_restore(unsigned enabled) { if(enabled) irq_disabled=0; }
void stock_reply(unsigned context,unsigned command,const void *p,unsigned size) {
    assert(context==7 && command==0x37 && size<=sizeof response && !irq_disabled);
    memcpy(response,p,size);response_size=size;
}
static void read_after(unsigned after) {
    unsigned char request[13]={0};memcpy(request+7,&after,4);
    runtime_bt_trace_read(7,request,sizeof request);
    assert(!memcmp(response,"DBTR\1\0",6));
}
static void hci(unsigned char *p,unsigned size) {
    unsigned char packet[16]={0};uint16_t n=size;
    memcpy(packet+8,&n,2);memcpy(packet+12,&p,sizeof p);runtime_bt_trace_hci(packet);
}
int main(void) {
    read_after(0);assert(response_size==16 && !response[6]);
    unsigned char key[25]={0x18,23};memset(key+2,0xa5,23);key[24]=4;
    hci(key,sizeof key);assert(sequence==1 && history[0].length==7);
    assert(history[0].data[6]==4 && history[0].data[7]==0);
    unsigned char confirm[12]={0x33,10};memset(confirm+2,0xa5,10);
    hci(confirm,sizeof confirm);assert(sequence==2 && history[1].length==6);
    for(unsigned i=6;i<12;++i) assert(!history[1].data[i]);
    hci(confirm,11);assert(sequence==2); /* Truncated / inconsistent framing. */
    confirm[1]=9;hci(confirm,11);assert(sequence==2); /* Wrong event size. */
    unsigned char fail[]={0x36,7,5,1,2,3,4,5,6};hci(fail,sizeof fail);
    assert(sequence==3 && history[2].data[0]==5);
    unsigned char params[12]={0,0,1,0,0x18,0};runtime_bt_trace_stack(15,params);
    assert(sequence==4 && history[3].data[0]==0x18 && history[3].length==2);
    runtime_bt_trace_stack(3,params);assert(sequence==4); /* No periodic memory spam. */
    read_after(0);assert(response[6]==4 && response_size==112);
    read_after(4);assert(response[6]==0);
    runtime_bt_trace_stack(16,params);assert(!history[4].length);
    runtime_bt_trace_stack(10,params);assert(history[5].length==1 && !history[5].data[0]);
    unsigned char remote[0xd8];memset(remote,0xa5,sizeof remote);
    runtime_bt_trace_app_disconnect(0x1759c);
    assert(history[6].kind==6 && history[6].event==1 && history[6].length==4);
    unsigned caller;memcpy(&caller,history[6].data,4);assert(caller==0x1759c);
    runtime_bt_trace_force_disconnect(remote,0x13,1,0x11df7a);
    assert(history[7].length==12 && history[7].data[10]==0x13 && history[7].data[11]==1);
    assert(!memcmp(history[7].data+4,remote+0x54,6));
    remote[0x99]=0x15;remote[0x96]=3;
    runtime_bt_trace_link_disconnect(remote,0x1283a4);
    assert(history[8].data[10]==0x15 && history[8].data[11]==3);
    runtime_bt_trace_link_disconnect(NULL,1);
    for(unsigned i=4;i<12;++i) assert(!history[9].data[i]);
    preserve=1;assert(runtime_bt_trace_app_disconnect(0x170a8));
    assert(history[10].event==4 && history[10].length==4);preserve=0;
    sequence=4;
    for(unsigned i=0;i<100;++i) runtime_bt_trace(5,0,&i,4);
    read_after(1);assert(response[6]==6);
    uint32_t oldest,latest;memcpy(&oldest,response+8,4);memcpy(&latest,response+12,4);
    assert(oldest==73 && latest==104);
    for(unsigned i=0;i<6;++i) { struct trace_event row;
        memcpy(&row,response+16+i*sizeof row,sizeof row);assert(row.sequence==73+i);
    }
    read_after(103);assert(response[6]==1);
    read_after(200);assert(!response[6]); /* Host notices reboot from latest cursor. */
    unsigned char bad[12]={0};runtime_bt_trace_read(7,bad,sizeof bad);
    assert(response_size==16 && response[5]==1);
    unsigned before=sequence;runtime_bt_trace(1,1,NULL,1);runtime_bt_trace(1,1,bad,13);
    assert(sequence==before && !irq_disabled);
    puts("Bluetooth trace framing, redaction, overwrite and cursor tests passed");
}
