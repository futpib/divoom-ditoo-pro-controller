#include <assert.h>
#include <stdio.h>
#include "bluetooth-hid.c"
static unsigned clock_ms,disconnects,sends,rc=2,connect_rc;
static unsigned char context[256],remote[256],sent[12],core[0x3200];
unsigned char *volatile stock_bt_core=core;
static unsigned sent_size;
void *volatile stock_bt_context=context;
unsigned stock_ticks(void) { return clock_ms; }
unsigned stock_free_heap(void) { return 100000; }
unsigned stock_l2_register(struct psm *p) { assert(p->mtu==672);memcpy(core+0x28fc+(p->id==0x13)*sizeof p,&p,sizeof p);return 0; }
unsigned stock_bt_class(unsigned cod) { assert(cod==0x2c0540);return 0; }
unsigned stock_sdp_add(struct record *p) { assert(p->count==19);return 0; }
unsigned stock_cmgr_register(void *p,void (*f)(void *,unsigned,unsigned)) { (void)p;(void)f;return 0; }
unsigned stock_cmgr_connect(void *p,const unsigned char *a) {
    void *r=remote;memcpy((unsigned char *)p+0x14,&r,sizeof r);memcpy(remote+0x54,a,6);return connect_rc;
}
unsigned stock_cmgr_remove(void *p) { (void)p;return 0; }
unsigned stock_l2_connect(struct psm *p,unsigned id,void *r,unsigned v,uint16_t *cid) {
    assert(id==p->id && r==remote && !v);*cid=id==0x11 ? 64 : 65;return 2;
}
unsigned stock_l2_accept(unsigned cid,unsigned status,unsigned n) { (void)cid;(void)status;(void)n;return 2; }
unsigned stock_l2_disconnect(unsigned cid) { assert(cid==64 || cid==65);++disconnects;return 2; }
unsigned stock_l2_send(unsigned cid,struct packet *p) {
    assert(cid==64 || cid==65);assert(p->length<=12);sent_size=p->length;
    memcpy(sent,p->data,p->length);++sends;return rc;
}
static void event(unsigned i,unsigned ev,void *data) {
    struct l2_event p={0};p.event=ev;p.psm=&hid.psm[i];p.remote=remote;p.data=data;
    l2_event(64+i,&p);
}
static void connected(void) {
    unsigned char addr[16]={1,2,3,4,5,6};
    memset(&hid,0,sizeof hid);rc=2;
    runtime_hid_command(HID_CONNECT,0,addr,1,0);
    assert(hid.status.enabled==1 && hid.status.state==1);
    event(0,2,NULL);event(1,2,NULL);assert(hid.status.state==2);
}
static void key(unsigned op,unsigned key) {
    unsigned char data[16]={0};runtime_hid_command(op,key,data,1,hid.status.generation);
}
int main(void) {
    connect_rc=2;
    unsigned char address[16]={1,2,3,4,5,6};
    runtime_hid_command(HID_CONNECT,0,address,1,0);
    assert(!hid.cid[0] && hid.active); /* Wait for ACL completion. */
    link_event(hid.manager,1,0);assert(hid.cid[0]==64);
    connect_rc=0;
    connected();key(HID_KEY,44);
    assert(sent_size==10 && sent[0]==0xa1 && sent[1]==1 && sent[4]==44);
    unsigned n=sends;key(HID_KEY,40);assert(sends==n);
    clock_ms+=80;runtime_hid_service(1);assert(sends==n); /* DMA still owns press. */
    event(1,6,&hid.tx[1].packet);runtime_hid_service(1);
    assert(sends==n+1 && sent[4]==0 && hid.status.released==1);
    event(1,6,&hid.tx[1].packet);key(HID_CONSUMER,0xcd);
    assert(sent_size==4 && sent[1]==2 && sent[2]==0xcd);
    event(1,6,&hid.tx[1].packet);runtime_hid_service(2); /* Lua stopped or failed. */
    assert(sent[2]==0 && hid.status.released==2);
    connected();key(HID_CONSUMER,0xe2);n=disconnects;
    clock_ms+=1000;runtime_hid_service(1);assert(disconnects>n && !hid.active);
    connected();rc=12;key(HID_KEY,44);assert(!hid.active && !hid.down);
    connected();n=sends;
    unsigned char data[16]={0};runtime_hid_command(HID_KEY,44,data,1,hid.status.generation-1);
    assert(sends==n);
    const unsigned char get[]={0x41,1};control(get,sizeof get);
    assert(sent_size==10 && sent[0]==0xa1 && sent[1]==1 && sent[4]==0);
    event(0,6,&hid.tx[0].packet);
    const unsigned char boot[]={0x70};control(boot,sizeof boot);assert(sent[0]==3);
    event(0,6,&hid.tx[0].packet);
    const unsigned char output[]={0x52,1,7};control(output,sizeof output);assert(sent[0]==0);
    connected();event(0,4,NULL);assert(!hid.active);
    connected();memset(core,0,sizeof core);runtime_hid_service(1);
    assert(!hid.status.enabled && !hid.active); /* Reused stack address. */
    puts("HID report, ownership, stale command, timeout and control tests passed");
}
