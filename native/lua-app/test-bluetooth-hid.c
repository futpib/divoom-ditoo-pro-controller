#include <assert.h>
#include <stdio.h>
#include "bluetooth-hid.c"
static unsigned clock_ms,disconnects,sends,rc=2,connect_rc;
static unsigned char context[256],remote[256],sent[12],core[0x3200];
volatile unsigned char stock_bt_manager[0x1c4];
static unsigned char device_entry[0x40];
static unsigned access_mode=3,access_rc;
unsigned stock_bt_access(unsigned mode,const void *p) {
    assert(!p && mode<=3);if (access_rc) return access_rc;
    access_mode=mode;core[0x792]=mode;return 2;
}
void *stock_bt_find_device(const unsigned char *a) { (void)a;return device_entry; }
unsigned char *volatile stock_bt_core=core;
static unsigned sent_size;
void *volatile stock_bt_context=context;
unsigned stock_ticks(void) { return clock_ms; }
unsigned stock_free_heap(void) { return 100000; }
unsigned stock_l2_register(struct psm *p) { assert(p->mtu==672);memcpy(core+0x28fc+(p->id==0x13)*sizeof p,&p,sizeof p);return 0; }
unsigned stock_bt_class(unsigned cod) { assert(cod==0x2c0540);return 0; }
unsigned stock_sec_register(struct security *s) {
    assert(s->callback==stock_l2_security && (s->psm==0x11 || s->psm==0x13));
    assert(s->level==0x22 && !s->min_key);return 0;
}
void stock_l2_security(void *p) { (void)p; }
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
    remote[0xb1]=8;remote[0xb3]=2;
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
    assert(hid.status.closed==1 && hid.status.close_channel==0x11);
    event(1,4,NULL);assert(!hid.status.state);
    runtime_hid_command(HID_LISTEN,0,address,1,hid.status.generation);
    assert(hid.active && hid.status.state==4);
    clock_ms+=60000;runtime_hid_service(1);assert(hid.status.state==4);
    remote[0x54]^=1;event(0,1,NULL);assert(hid.status.state==4 && !hid.cid[0]);remote[0x54]^=1;
    event(0,1,NULL);assert(hid.status.state==1 && hid.status.incoming==2);
    event(0,2,NULL);assert(!hid.cid[1]); /* Incoming host opens both channels. */
    event(1,1,NULL);event(1,2,NULL);assert(hid.status.state==2);
    key(HID_KEY,44);event(1,6,&hid.tx[1].packet);clock_ms+=80;runtime_hid_service(1);
    assert(!hid.down);event(1,6,&hid.tx[1].packet);
    event(0,4,NULL);event(1,4,NULL);assert(hid.status.state==4 && hid.active);
    runtime_hid_command(HID_DISCONNECT,0,address,1,hid.status.generation);
    assert(!hid.active && !hid.status.state);
    event(0,1,NULL);assert(!hid.cid[0]);
    runtime_hid_command(HID_CONNECT,0,address,1,hid.status.generation);
    clock_ms+=119999;runtime_hid_service(1);assert(hid.status.state==1 && hid.active);
    ++clock_ms;runtime_hid_service(1);assert(hid.status.error==9 && !hid.active);
    event(0,4,NULL);assert(!hid.status.state);
    runtime_hid_command(HID_LISTEN,0,address,1,hid.status.generation);
    event(0,1,NULL);clock_ms+=120000;runtime_hid_service(1);
    assert(hid.status.error==9 && hid.active);
    event(0,4,NULL);assert(hid.status.state==4);
    runtime_hid_command(HID_DISCONNECT,0,address,1,hid.status.generation);
    core[0x792]=2;runtime_hid_command(HID_PAIR,1000,address,1,hid.status.generation);
    assert(hid.status.pairing && access_mode==3 && hid.status.state==4);
    clock_ms+=999;runtime_hid_service(1);assert(hid.status.pair_remaining_ms==1);
    ++clock_ms;runtime_hid_service(1);assert(!hid.status.pairing && access_mode==2);
    runtime_hid_command(HID_PAIR,120000,address,1,hid.status.generation);
    runtime_hid_service(2);assert(!hid.status.pairing && access_mode==2);
    runtime_hid_command(HID_PAIR,120000,address,2,hid.status.generation);
    access_rc=19;runtime_hid_service(3);assert(hid.status.pairing);
    access_rc=0;runtime_hid_service(3);assert(!hid.status.pairing);
    runtime_hid_command(HID_DISCONNECT,0,address,3,hid.status.generation);
    for (unsigned i=0;i<8;++i) {
        for (unsigned j=0;j<26;++j) stock_bt_manager[7+i*26+j]=i+1;
        stock_bt_manager[7+i*26+25]=1;
    }
    unsigned char last[16]={8,8,8,8,8,8};device_entry[8]=7;
    runtime_hid_command(HID_FORGET,0,last,3,hid.status.generation);
    assert(hid.status.forgotten==1 && stock_bt_manager[0] && device_entry[8]==1);
    for (unsigned i=0;i<7;++i) assert(stock_bt_manager[7+i*26]==i+1);
    for (unsigned j=0;j<26;++j) assert(!stock_bt_manager[7+7*26+j]);
    unsigned char first[16]={1,1,1,1,1,1};
    runtime_hid_command(HID_FORGET,0,first,3,hid.status.generation);
    assert(hid.status.forgotten==2);
    for (unsigned i=0;i<6;++i) assert(stock_bt_manager[7+i*26]==i+2);
    runtime_hid_command(HID_FORGET,0,first,3,hid.status.generation);assert(hid.status.forgotten==2);
    connected();unsigned before=hid.status.forgotten;
    runtime_hid_command(HID_FORGET,0,address,1,hid.status.generation);
    assert(hid.status.error==17 && hid.status.forgotten==before);
    connected();remote[0xb3]=0;event(1,2,NULL);assert(hid.status.error==6);
    connected();runtime_hid_command(HID_DISCONNECT,0,address,1,hid.status.generation);
    event(0,4,NULL);event(1,4,NULL);
    unsigned char unknown[16]={0};
    runtime_hid_command(HID_PAIR,120000,unknown,1,hid.status.generation);
    event(1,1,NULL);assert(!hid.cid[1]); /* Control channel selects the peer. */
    event(0,1,NULL);assert(hid.cid[0] && !memcmp(hid.status.peer,remote+0x54,6));
    remote[0x54]^=1;event(1,1,NULL);assert(!hid.cid[1]);remote[0x54]^=1;
    event(0,2,NULL);event(1,1,NULL);event(1,2,NULL);assert(hid.status.state==2 && !hid.status.pairing);
    runtime_hid_command(HID_DISCONNECT,0,address,1,hid.status.generation);event(0,4,NULL);event(1,4,NULL);
    runtime_hid_command(HID_PAIR,1000,unknown,1,hid.status.generation);
    clock_ms+=1000;event(0,1,NULL);assert(!hid.cid[0]); /* Deadline applies even before the service tick. */
    connected();memset(core,0,sizeof core);runtime_hid_service(1);
    assert(!hid.status.enabled && !hid.active); /* Reused stack address. */
    puts("HID report, ownership, stale command, timeout and control tests passed");
}
