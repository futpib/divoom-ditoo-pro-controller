#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "bluetooth-hid.c"
void runtime_bt_trace(unsigned k,unsigned e,const void *p,unsigned n) {
    (void)k;(void)e;assert(p && n<=12);
}
static unsigned allocated,fail_alloc,unregister_busy,security_busy;
void *stock_alloc(unsigned n) { if(fail_alloc)return NULL;void *p=calloc(1,n);if(p)++allocated;return p; }
void stock_free(void *p) { if(p){--allocated;free(p);} }
unsigned runtime_irq_save(void) { return 1; }
void runtime_irq_restore(unsigned x) { (void)x; }
static unsigned clock_ms,disconnects,sends,rc=2,connect_rc;
static unsigned char context[256],remote[256],sent[12],core[0x3800],spp[32];
unsigned char *volatile stock_spp_context=spp;
void stock_spp_callback(void) {}
static unsigned rf_closed;
unsigned stock_rf_close(void *channel) {
    assert(channel && !serial_channel(channel));++rf_closed;return 2;
}
volatile unsigned char stock_bt_manager[0x1c4];
static unsigned char device_entry[0x40];
static unsigned access_mode=3,access_rc;
static unsigned hogp_enabled;
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
unsigned stock_bt_class(unsigned cod) { assert(cod==0x2c0540 || cod==0x540 || cod==0);memcpy(core+0x9a8,&cod,4);return 0; }
unsigned stock_sec_register(struct security *s) {
    assert(s->callback==stock_l2_security && (s->psm==0x11 || s->psm==0x13));
    assert(s->level==0x22 && !s->min_key);return 0;
}
unsigned stock_sec_unregister(struct security *s) { (void)s;return security_busy; }
unsigned stock_cmgr_unregister(void *p) { (void)p;return unregister_busy; }
void stock_l2_security(void *p) { (void)p; }
static struct record *sdp_head(void) { return (struct record *)(core+0x3068); }
unsigned stock_sdp_add(struct record *p) {
    struct record *head=sdp_head();if (!head->next) head->next=head->previous=head;
    struct record *tail=head->previous;
    p->next=head;p->previous=tail;tail->next=p;head->previous=p;return 0;
}
unsigned stock_sdp_remove(struct record *p) {
    struct record *next=p->next,*previous=p->previous;
    previous->next=next;next->previous=previous;p->next=p->previous=NULL;return 0;
}
unsigned stock_cmgr_register(void *p,void (*f)(void *,unsigned,unsigned)) { (void)p;(void)f;return 0; }
unsigned stock_cmgr_connect(void *p,const unsigned char *a) {
    void *r=remote;memcpy((unsigned char *)p+0x14,&r,sizeof r);memcpy(remote+0x54,a,6);return connect_rc;
}
unsigned stock_cmgr_remove(void *p) { (void)p;return 0; }
unsigned stock_l2_connect(struct psm *p,unsigned id,void *r,unsigned v,uint16_t *cid) {
    assert(id==p->id && r==remote && !v);*cid=id==0x11 ? 64 : 65;return 2;
}
unsigned stock_l2_accept(unsigned cid,unsigned status,unsigned n) { (void)cid;(void)status;(void)n;return 2; }
unsigned stock_l2_disconnect(unsigned cid) { assert(cid>=64 && cid<72);++disconnects;return 2; }
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
    if(hid_context) { release(); }
    memset(&idle_status,0,sizeof idle_status);assert(acquire());
    memset(&hid,0,sizeof hid);memset(core,0,sizeof core);rc=2;
    remote[0xb1]=8;remote[0xb3]=2;
    runtime_hid_command(HID_CONNECT,0,addr,1,0);
    assert(hid.status.enabled==1 && hid.status.state==1);
    event(0,2,NULL);event(1,2,NULL);assert(hid.status.state==2);
}
static void key(unsigned op,unsigned key) {
    unsigned char data[16]={0};runtime_hid_command(op,key,data,1,hid.status.generation);
}
static void modes(void) {
    connected();unsigned generation=hid.status.generation;
    static const unsigned char sink[]={0x35,3,0x19,0x11,0x0b};
    static const unsigned char hfp[]={0x35,6,0x19,0x11,0x1e,0x19,0x12,3};
    static const unsigned char pnp[]={0x35,3,0x19,0x12,0};
    static const unsigned char serial[]={0x35,3,0x19,0x11,0x01};
    struct attribute attr[]={A(1,sink),A(1,hfp),A(1,pnp),A(1,serial)};
    struct record records[4]={0};
    for (unsigned i=0;i<4;++i) { records[i].count=1;records[i].attributes=&attr[i];stock_sdp_add(&records[i]); }
    struct psm profiles[4]={{.id=0x19},{.id=0x17},{.id=3},{.id=1}};
    for (unsigned i=0;i<4;++i) registered_psms()[i+2]=&profiles[i];
    unsigned char *channel=core+0x291c+2*0x7c;
    uint16_t cid=66;struct psm *p=&profiles[0];
    memcpy(channel+0x2c,&cid,2);memcpy(channel+0x28,&p,sizeof p);channel[2]=5;
    unsigned before=disconnects;
    runtime_hid_command(HID_MODE,1,NULL,1,0);
    assert(hid.status.keyboard_only && hid.status.hidden_services==2 && hid.status.blocked_psms==2);
    assert(hid.status.audio_channels==1 && disconnects==before+1);
    assert(hid.status.state==2 && hid.status.generation==generation);
    assert(sdp_head()->next==&hid.record && hid.record.next==&records[2] && records[2].next==&records[3]);
    assert(records[3].next==sdp_head());
    assert(!registered_psms()[2] && !registered_psms()[3] && registered_psms()[4]==&profiles[2] && registered_psms()[5]==&profiles[3]);
    assert(*(unsigned *)(core+0x9a8)==0x540);
    channel[2]=6;runtime_hid_service(1);assert(disconnects==before+1);
    memset(channel,0,0x7c);runtime_hid_service(2);assert(!hid.status.audio_channels && hid.status.keyboard_only);
    key(HID_CONSUMER,0xcd);assert(sent[2]==0xcd); /* Existing HID connection survives. */
    runtime_hid_command(HID_MODE,1,NULL,2,0);assert(hid.status.hidden_services==2);
    registered_psms()[2]=&profiles[3];runtime_hid_command(HID_MODE,0,NULL,2,0);
    assert(hid.status.error==19 && hid.status.keyboard_only); /* Never overwrite a new registration. */
    registered_psms()[2]=NULL;runtime_hid_command(HID_MODE,0,NULL,2,0);
    assert(!hid.status.keyboard_only && !hid.status.hidden_services && !hid.status.blocked_psms);
    assert(*(unsigned *)(core+0x9a8)==0x2c0540);
    for (unsigned i=0;i<4;++i) assert(registered_psms()[i+2]==&profiles[i]);
    assert(records[2].next==&records[3] && records[3].next==&records[0] && records[0].next==&records[1] && records[1].next==sdp_head());
    runtime_hid_command(HID_MODE,1,NULL,2,0);memset(core,0,sizeof core);runtime_hid_service(2);
    assert(!hid_context && !idle_status.enabled && !idle_status.keyboard_only); /* No stale pointers after stack recreation. */
    connected();hid.record.next=&hid.record;
    runtime_hid_command(HID_MODE,1,NULL,2,0);
    assert(hid.status.error==18 && !hid.status.keyboard_only); /* Corrupt list stays bounded and unchanged. */
    connected();
    puts("Keyboard-only mode hides audio SDP/PSMs, drains audio, preserves HID, restores and resets safely");
}
static void serial_control(void) {
    connected();
    unsigned char control[0x84]={0},handsfree[0x84]={0};
    uintptr_t callback=(uintptr_t)stock_spp_callback;memcpy(control+8,&callback,sizeof callback);
    assert(runtime_hid_rfcomm_accept(1) && runtime_hid_rfcomm_client(handsfree));
    runtime_hid_command(HID_MODE,1,NULL,1,0);
    for(unsigned server=1;server<=4;++server) {
        spp[8]=server;
        for(unsigned peer=0;peer<=5;++peer) assert(runtime_hid_rfcomm_accept(peer)==(peer==server));
    }
    stock_spp_context=NULL;assert(!runtime_hid_rfcomm_accept(4));stock_spp_context=spp;
    assert(runtime_hid_rfcomm_client(control) && !runtime_hid_rfcomm_client(handsfree));
    assert(!runtime_hid_rfcomm_client(NULL));
    unsigned before=rf_closed;
    unsigned char *channels[]={control,handsfree};
    for(unsigned i=0;i<2;++i) {
        unsigned char *dlc=core+0x32a0+i*16;dlc[8]=1;memcpy(dlc+12,&channels[i],sizeof channels[i]);
    }
    runtime_hid_service(1);assert(rf_closed==before+1 && hid.status.audio_channels==1);
    core[0x32b8]=0;runtime_hid_service(1);assert(rf_closed==before+1 && !hid.status.audio_channels);
    runtime_hid_command(HID_MODE,0,NULL,1,0);
    assert(runtime_hid_rfcomm_client(handsfree) && runtime_hid_rfcomm_accept(1));
    runtime_hid_command(HID_MODE,1,NULL,1,0);
    stock_bt_context=NULL;assert(runtime_hid_rfcomm_client(handsfree));stock_bt_context=context;
    memset(core,0,sizeof core);runtime_hid_service(1);
    assert(!hid_context && runtime_hid_rfcomm_accept(1));
    puts("Remote mode keeps serial SDP/RFCOMM, rejects hands-free in both directions and drains only audio DLCs");
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
    connected();unsigned char hold[16]={0,0,0xd0,7};
    runtime_hid_command(HID_CONSUMER,0x224,hold,1,hid.status.generation);
    assert(sent[2]==0x24 && sent[3]==2);event(1,6,&hid.tx[1].packet);n=sends;
    clock_ms+=1999;runtime_hid_service(1);assert(sends==n && hid.down);
    ++clock_ms;runtime_hid_service(1);assert(sends==n+1 && !hid.down);
    event(1,6,&hid.tx[1].packet);
    runtime_hid_command(HID_KEY,44,hold,1,hid.status.generation);event(1,6,&hid.tx[1].packet);
    runtime_hid_service(2);assert(!hid.down); /* Cancellation shortens even a long hold. */
    event(1,6,&hid.tx[1].packet);hold[2]=0xd1;n=sends;
    runtime_hid_command(HID_KEY,44,hold,1,hid.status.generation);assert(sends==n && hid.status.error==14);
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
    assert(!hid_context && !allocated); /* Reused stack address. */
    modes();
    serial_control();
    connected();
    assert(!runtime_hid_preserve_link(0x170a8));
    runtime_hid_command(HID_MODE,1,address,1,hid.status.generation);
    assert(runtime_hid_preserve_link(0x170a8) && runtime_hid_preserve_link(0x1759c));
    const unsigned allowed[]={0x16e40,0x18f88,0x1938c,0x77e3a,0};
    for(unsigned i=0;i<sizeof allowed/sizeof *allowed;++i) assert(!runtime_hid_preserve_link(allowed[i]));
    unsigned before_disconnects=disconnects;
    runtime_hid_command(HID_DISCONNECT,0,address,1,hid.status.generation);
    assert(disconnects>before_disconnects); /* OFF remains an actual disconnect. */
    runtime_hid_command(HID_MODE,0,address,1,hid.status.generation);
    assert(!runtime_hid_preserve_link(0x170a8));
    runtime_hid_command(HID_MODE,1,address,1,hid.status.generation);
    stock_bt_context=NULL;assert(!runtime_hid_preserve_link(0x170a8));stock_bt_context=context;
    stock_bt_core=NULL;assert(!runtime_hid_preserve_link(0x170a8));stock_bt_core=core;
    memset(core,0,sizeof core);runtime_hid_service(1);
    assert(!runtime_hid_preserve_link(0x170a8));
    connected();runtime_hid_command(HID_DISCONNECT,0,address,1,0);
    event(0,4,NULL);event(1,4,NULL);core[0x792]=3;
    runtime_hid_command(HID_MODE,2,NULL,1,0);
    assert(hogp_enabled && hid.le_access_owned && access_mode==2);
    core[0x792]=3;runtime_hid_service(1);assert(access_mode==2 && core[0x792]==2);
    core[0x792]=3;access_rc=19;runtime_hid_service(1);assert(core[0x792]==3);
    access_rc=0;runtime_hid_service(1);assert(core[0x792]==2);
    runtime_hid_command(HID_MODE,1,NULL,1,0);
    assert(!hogp_enabled && !hid.le_access_owned && access_mode==3);
    runtime_hid_command(HID_MODE,0,NULL,1,0);runtime_hid_service(1);
    assert(!hid_context && !allocated);
    fail_alloc=1;runtime_hid_command(HID_CONNECT,0,address,1,0);
    struct hid_status snapshot;runtime_hid_status(&snapshot);
    assert(!allocated && !hid_context && snapshot.error==7);fail_alloc=0;
    for(unsigned i=0;i<12;++i) {
        connected();key(HID_KEY,44);
        runtime_hid_command(HID_DISCONNECT,0,address,1,hid.status.generation);
        runtime_hid_service(1);assert(hid_context && allocated==1);
        event(0,4,NULL);runtime_hid_service(1);assert(hid_context);
        event(1,4,NULL);unregister_busy=5;runtime_hid_service(1);assert(hid_context);
        unregister_busy=0;security_busy=1;runtime_hid_service(1);assert(hid_context && hid.status.enabled==2);
        runtime_hid_service(1);assert(hid_context); /* Partial unregister keeps native pointers alive. */
        security_busy=0;runtime_hid_service(1);assert(!hid_context && !allocated);
        for(unsigned j=0;j<8;++j) assert(!registered_psms()[j]);
        assert(sdp_head()->next==sdp_head() && !*(unsigned *)(core+0x9a8));
        struct l2_event late={.event=6};l2_event(65,&late);link_event(NULL,1,0);
        runtime_hid_status(&snapshot);assert(!snapshot.enabled && !snapshot.state);
    }
    puts("HID report, ownership, stale command, timeout, LE visibility restoration and control tests passed");
}

unsigned runtime_hogp_enabled(void) { return hogp_enabled; }
unsigned runtime_hogp_mode(unsigned enabled) { hogp_enabled=enabled;return 1; }
void runtime_hogp_service(unsigned epoch) { (void)epoch; }
void runtime_hogp_status(struct hid_status *s) { memset(s,0,sizeof *s); }
void runtime_hogp_command(unsigned op,unsigned value,const unsigned char *data,unsigned epoch,unsigned generation) {
    (void)op;(void)value;(void)data;(void)epoch;(void)generation;assert(0);
}
