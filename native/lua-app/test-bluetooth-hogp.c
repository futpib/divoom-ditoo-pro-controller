#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "bluetooth-hogp.c"
static unsigned allocated,fail_alloc;
static unsigned clock_ms,confirmations,disconnects,notifications,notify_rc,encrypted,paired,adv,forgotten;
static unsigned char connection[0x500],second[0x500],last_report;
static unsigned last_attribute,last_connection,forwarded;
static unsigned setup_sent,setup_busy,setup_rc;
static unsigned char address[]={0x84,0x5c,0xf3,0xef,0x87,0x78};
static unsigned char hci_context[0x500];
static const unsigned char local_address[]={0xb1,0x21,0x81,0xdd,0xb8,0x9b};
static unsigned address_sets,address_deferred;
volatile unsigned stock_le_address_mode;
unsigned char *volatile stock_hci_stack=hci_context;
static const unsigned char *selected_db;
static unsigned char saved_ccc[16][8];
static unsigned char cached_ccc[16][8];
static unsigned cached_valid[16];
static unsigned ccc_valid[16],ccc_writes,ccc_error,ccc_drop;
static unsigned char saved_names[16][8+BT_NAME_BYTES],cached_names[16][8+BT_NAME_BYTES];
static unsigned saved_name_size[16],cached_name_size[16],name_writes;
static struct gatt_prefix gatt;
static unsigned gatt_enabled,gatt_prepared,gatt_started,gatt_failed;
struct gatt_prefix *stock_gatt_context(unsigned handle) { assert(handle==4);return gatt_enabled ? &gatt : NULL; }
struct gatt_prefix *stock_gatt_prepare(unsigned handle) {
    assert(handle==4);++gatt_prepared;return gatt_enabled && !gatt_failed ? &gatt : NULL;
}
void stock_gatt_run(void) {
    assert(gatt.state==19 && gatt.uuid==0x2a00 && gatt.start==1 && gatt.end==0xffff);
    assert(gatt.query_start==1 && gatt.query_end==0xffff && gatt.callback==name_reply);++gatt_started;
}
static int tlv_get(void *context,unsigned tag,void *data,unsigned size) {
    if(tag>=0x44484e00 && tag<0x44484e10) {
        assert(!context && size==sizeof saved_names[0]);unsigned i=tag-0x44484e00;
        memcpy(data,cached_names[i],cached_name_size[i]>size ? size : cached_name_size[i]);return cached_name_size[i];
    }
    assert(!context && tag>=0x44485200 && tag<0x44485210 && size==8);
    unsigned i=tag-0x44485200;if (!cached_valid[i]) return 0;
    memcpy(data,cached_ccc[i],size);return size;
}
void stock_cache_invalidate(unsigned address,unsigned size) {
    assert(address==0x1f0000 && size==0x2000);
    memcpy(cached_ccc,saved_ccc,sizeof saved_ccc);memcpy(cached_valid,ccc_valid,sizeof ccc_valid);
    memcpy(cached_names,saved_names,sizeof saved_names);memcpy(cached_name_size,saved_name_size,sizeof saved_name_size);
}
static int tlv_store(void *context,unsigned tag,const void *data,unsigned size) {
    if(tag>=0x44484e00 && tag<0x44484e10) {
        assert(!context && size<=sizeof saved_names[0]);unsigned i=tag-0x44484e00;
        memcpy(saved_names[i],data,size);saved_name_size[i]=size;++name_writes;return 0;
    }
    assert(!context && tag>=0x44485200 && tag<0x44485210 && size==8);
    if (ccc_error) return 1;
    if (ccc_drop) return 0;
    unsigned i=tag-0x44485200;memcpy(saved_ccc[i],data,size);ccc_valid[i]=1;++ccc_writes;return 0;
}
static void tlv_remove(void *context,unsigned tag) {
    if(tag>=0x44484e00 && tag<0x44484e10) { assert(!context);saved_name_size[tag-0x44484e00]=0;return; }
    assert(!context && tag>=0x44485200 && tag<0x44485210);ccc_valid[tag-0x44485200]=0;
}
static const struct tlv tlv={tlv_get,tlv_store,tlv_remove};
const struct tlv *volatile stock_le_tlv=&tlv;
void *volatile stock_le_tlv_context;
void stock_att_set_db(const unsigned char *p) { selected_db=p; }
uint16_t stock_att_request(void *c,unsigned char *p,uint16_t n,unsigned char *r) {
    (void)c;
    if (selected_db==hogp_database && n==3 && p[0]==0x0a && p[1]==0x24) {
        memset(r,0xa5,77);r[0]=0x0b;return 77;
    }
    return selected_db==hogp_database ? 1 : 2;
}
unsigned stock_att_can_send(unsigned handle) { assert(handle==4);return !setup_busy; }
unsigned stock_att_send(unsigned handle,unsigned cid,const void *p,unsigned n) {
    assert(handle==4 && cid==4 && n==77 && ((const unsigned char *)p)[0]==0x0b);
    if (!setup_rc) ++setup_sent;
    return setup_rc;
}
volatile unsigned stock_battery_level=6;
unsigned stock_ticks(void) { return clock_ms; }
unsigned stock_free_heap(void) { return 100000; }
void *stock_alloc(unsigned n) { if(fail_alloc)return NULL;void *p=calloc(1,n);if(p)++allocated;return p; }
void stock_free(void *p) { if(p){--allocated;free(p);} }
unsigned runtime_irq_save(void) { return 1; }
void runtime_irq_restore(unsigned x) { (void)x; }
void stock_att_init(const unsigned char *db,read_fn read,write_fn write) {
    assert(db!=hogp_database && read==read_value && write==write_value);
}
void stock_ble_event(unsigned type,unsigned channel,unsigned char *p,unsigned n) {
    (void)type;(void)channel;(void)p;(void)n;++forwarded;
}
unsigned char *stock_hci_connection(unsigned handle) { return handle==4 ? connection : handle==5 ? second : NULL; }
void stock_hci_run(void) { unsigned state;memcpy(&state,connection+20,4);if(state==8) ++disconnects; }
unsigned stock_le_key_size(unsigned handle) { (void)handle;return encrypted ? 16 : 0; }
int stock_le_device_index(unsigned handle) { return handle==4 && paired ? 0 : -1; }
void stock_le_device_info(unsigned i,int *type,unsigned char *a,unsigned char *irk) {
    assert(!irk);*type=i==0 && paired ? 0 : -1;memcpy(a,address,6);
}
void stock_le_delete_bond(unsigned type,const unsigned char *a) { assert(!type && !memcmp(a,address,6));paired=0;++forgotten; }
void stock_sm_confirm(unsigned handle) { assert(handle==4);++confirmations; }
unsigned stock_att_notify(unsigned handle,unsigned attribute,const void *data,unsigned n) {
    last_connection=handle;last_attribute=attribute;
    if (!notify_rc) { assert(n==1);last_report=*(const unsigned char *)data;++notifications; }
    return notify_rc;
}
void stock_le_adv_data(unsigned n,const unsigned char *p) { assert(n>0 && n<=31 && p); }
void stock_le_scan_data(unsigned n,const unsigned char *p) { assert(n<=31 && p); }
void stock_le_adv_enable(unsigned n) { adv=n; }
void stock_le_address(unsigned type,unsigned char *out) {
    memcpy(out,type ? hci_context+0x4b9 : local_address,6);
}
void stock_le_random_mode(unsigned mode) { stock_le_address_mode=mode; }
void stock_le_random_address(const unsigned char *address) {
    assert(!hci_context[0x4e5]);++address_sets;stock_le_address_mode=1;
    if (!address_deferred) { memcpy(hci_context+0x4b9,address,6);hci_context[0x4bf]=1; }
}
void stock_le_adv_params(unsigned a,unsigned b,unsigned c,unsigned d,const unsigned char *e,unsigned f,unsigned g) {
    assert(a==48 && b==80 && !c && !d && e && f==7 && !g);
}
void runtime_bt_trace(unsigned kind,unsigned event,const void *p,unsigned n) { (void)event;assert(kind==5 && p && n==4); }
static uint16_t read_original(uint16_t a,uint16_t b,uint16_t c,unsigned char *d,uint16_t e) {
    (void)a;(void)b;(void)c;(void)d;(void)e;return 17;
}
static int write_original(uint16_t a,uint16_t b,uint16_t c,uint16_t d,unsigned char *e,uint16_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;return 0;
}
static void event(unsigned code,unsigned handle) {
    unsigned char p[14]={code,12,handle,handle>>8};
    runtime_hogp_event(4,0,p,sizeof p);
}
static void closed(void) {
    unsigned char p[]={5,4,0,4,0,0x13};runtime_hogp_event(4,0,p,sizeof p);
    unsigned state=7;memcpy(connection+20,&state,4);
    memset(hci_context+16,0,sizeof(void *));
}
static void link(void) {
    unsigned char *p=connection;memcpy(hci_context+16,&p,sizeof p);
    connection[10]=4;second[10]=5;
}
static void key(unsigned op,unsigned value) {
    unsigned char data[16]={0};runtime_hogp_command(op,value,data,1,ble->status.generation);runtime_hogp_service(1);
}
static void subscribe(unsigned connection_handle) {
    link();
    unsigned char on[]={1,0};
    assert(write_value(connection_handle,0x29,0,0,on,2)==0);
    assert(write_value(connection_handle,0x2d,0,0,on,2)==0);
    runtime_hogp_service(1);
}
static void name_value(const char *value,unsigned size) {
    unsigned char packet[128]={0xa5,0,4,0,3,0,size,0};assert(size<=sizeof packet-8);
    memcpy(packet+8,value,size);gatt.callback(4,0,packet,size+8);
}
int main(void) {
    memcpy(connection+4,address,6);memset(second+4,0x77,6);
    unsigned state=7;memcpy(connection+20,&state,4);memcpy(second+20,&state,4);
    runtime_hogp_init(NULL,read_original,write_original);
    hci_context[0x4e5]=1;
    assert(runtime_hogp_mode(1));
    runtime_hogp_service(1);assert(!address_sets && !adv && ble->identity_pending==1);
    hci_context[0x4e5]=0;address_deferred=1;runtime_hogp_service(1);
    assert(address_sets==1 && !adv && ble->identity_pending==2);
    memcpy(hci_context+0x4b9,ble->identity,6);hci_context[0x4bf]=1;address_deferred=0;
    runtime_hogp_service(1);assert(!ble->identity_pending && adv);
    assert(hci_context[0x4b9]==0xd1 && !memcmp(hci_context+0x4ba,local_address+1,5));
    assert(runtime_hogp_mode(0));runtime_hogp_service(1);assert(!stock_le_address_mode && adv);
    assert(runtime_hogp_mode(1));runtime_hogp_service(1);assert(stock_le_address_mode==1 && adv);
    unsigned char att_connection[2]={4,0},other_connection[2]={5,0};
    assert(runtime_hogp_request(att_connection,NULL,0,NULL)==2 && selected_db==original_db);
    unsigned char target[16]={0},on[]={1,0},out[4]={0xa5,0xa5,0xa5,0xa5};
    assert(read_value(4,3,0,NULL,0)==16);
    assert(read_value(4,3,15,out+1,1)==1 && out[1]=='e' && out[0]==0xa5 && out[2]==0xa5);
    assert(read_value(4,3,16,out,1)==0 && read_value(4,0x32,0,out,1)==1 && out[0]==85);
    assert(read_value(4,8,0,out,1)==17);
    assert(write_value(4,0x29,1,0,on,2)==6);
    assert(write_value(4,0x29,0,1,on,2)==7);
    assert(write_value(4,0x29,0,0,on,1)==13);
    assert(write_value(4,0x28,0,0,on,2)==3);
    assert(write_value(4,0x29,0,0,on,2)==8);
    runtime_hogp_command(HID_PAIR,120000,target,1,0);
    assert(adv && ble->status.state==4);
    assert(runtime_hogp_request(att_connection,NULL,0,NULL)==1 && selected_db==original_db);
    assert(write_value(4,0x29,0,0,on,2)==15);
    event(0xc8,4);assert(confirmations==1 && ble->handle==4);
    encrypted=paired=1;subscribe(4);assert(ble->status.state==2);
    assert(runtime_hogp_request(att_connection,NULL,0,NULL)==1 && selected_db==original_db);
    assert(runtime_hogp_request(other_connection,NULL,0,NULL)==2 && selected_db==original_db);
    assert(write_value(5,0x29,0,0,on,2)==8);
    unsigned char a[6];unsigned type;
    assert(runtime_hogp_bond(0,a,&type) && !type && a[5]==0x84 && a[0]==0x78);
    assert(!runtime_hogp_bond(16,a,&type));
    /* The first report-map response waits without blocking other ATT traffic.
     * Backpressure retries are bounded, and reconnect discards stale output. */
    unsigned char request[]={0x0a,0x24,0},response[80];
    assert(!runtime_hogp_request(att_connection,request,sizeof request,response));
    assert(selected_db==original_db && ble->setup_size==77);
    memset(response,0,sizeof response);
    clock_ms+=999;runtime_hogp_service(1);assert(!setup_sent);
    setup_busy=1;++clock_ms;runtime_hogp_service(1);assert(!setup_sent);
    setup_busy=0;setup_rc=0x57;runtime_hogp_service(1);assert(!setup_sent && ble->setup_size);
    setup_rc=0;runtime_hogp_service(1);assert(setup_sent==1 && !ble->setup_size);
    assert(runtime_hogp_request(att_connection,request,sizeof request,response)==77);
    const unsigned usages[]={40,41,44,79,80,81,82,0xcd,0xe2,0xe9,0xea,0xb5,0xb6,0xb7};
    for (unsigned i=0;i<14;++i) {
        key(i<7 ? HID_KEY : HID_CONSUMER,usages[i]);
        assert(last_report==(1U<<(i%7)) && last_attribute==(i<7 ? 0x28U : 0x2cU));
        assert(ble->status.busy);clock_ms+=80;runtime_hogp_service(1);
        assert(!last_report && !ble->status.busy);
    }
    unsigned sent=ble->status.sent;key(HID_KEY,4);assert(ble->status.sent==sent && ble->status.error==14);
    assert(!runtime_hogp_key(HID_KEY,44,1));
    runtime_hogp_command(HID_KEY,44,target,1,ble->status.generation-1);assert(ble->status.sent==sent);
    notify_rc=0x57;key(HID_KEY,44);assert(ble->phase==1 && ble->status.sent==sent);
    runtime_hogp_service(2);assert(!ble->phase && !ble->status.busy && ble->status.sent==sent);
    key(HID_KEY,44);assert(ble->phase==1);notify_rc=0;runtime_hogp_service(1);
    assert(ble->phase==2 && ble->status.sent==sent+1);
    clock_ms+=80;runtime_hogp_service(1);
    key(HID_KEY,44);runtime_hogp_service(2);assert(!last_report && !ble->status.busy);
    key(HID_CONSUMER,0xcd);notify_rc=0x57;clock_ms+=80;runtime_hogp_service(1);assert(ble->status.busy);
    clock_ms+=920;runtime_hogp_service(1);assert(disconnects==1 && ble->status.state==3);
    closed();notify_rc=0;assert(ble->status.state==0);
    reverse(target,address);runtime_hogp_command(HID_CONNECT,0,target,1,0);subscribe(4);
    assert(ble->status.state==2 && confirmations==1); /* Bond reuse needs no new Just Works. */
    key(HID_DISCONNECT,0);closed();
    runtime_hogp_command(HID_CONNECT,0,target,1,0);subscribe(4);
    assert(!runtime_hogp_request(att_connection,request,sizeof request,response));
    key(HID_DISCONNECT,0);closed();clock_ms+=1000;runtime_hogp_service(1);
    assert(setup_sent==1 && !ble->setup_size);
    runtime_hogp_command(HID_CONNECT,0,target,1,0);subscribe(4);
    assert(!runtime_hogp_request(att_connection,request,sizeof request,response));
    setup_busy=1;clock_ms+=3000;runtime_hogp_service(1);
    assert(setup_sent==1 && !ble->setup_size && ble->status.state==3);
    setup_busy=0;closed();
    /* Android reconnects from its cached report map and sends no CCCD writes.
     * Recreate native RAM as on boot and recover the journaled subscriptions. */
    assert(ccc_valid[0] && saved_ccc[0][7]==3);
    unsigned writes=ccc_writes;
    runtime_hogp_init(NULL,read_original,write_original);assert(runtime_hogp_mode(1));runtime_hogp_service(1);
    runtime_hogp_command(HID_LISTEN,0,target,1,0);link();encrypted=0;
    runtime_hogp_service(1);assert(ble->status.state==4);
    encrypted=1;connection[4]^=1;paired=0;runtime_hogp_service(1);
    assert(ble->status.state==4);connection[4]^=1;paired=1;
    runtime_hogp_service(1);assert(ble->status.state==2 && ccc_writes==writes);
    key(HID_CONSUMER,0xcd);assert(last_report==1);clock_ms+=80;runtime_hogp_service(1);
    assert(!last_report);
    /* Explicit off rejects reports even if the host has reconnected. Connect
     * must adopt that existing encrypted connection without another pairing. */
    key(HID_DISCONNECT,0);closed();link();runtime_hogp_service(1);
    assert(ble->status.state==0 && !ble->listen);
    assert(write_value(4,0x29,0,0,on,2)==8);
    runtime_hogp_command(HID_CONNECT,0,target,1,0);runtime_hogp_service(1);
    assert(ble->status.state==2 && confirmations==1 && ccc_writes==writes);
    unsigned char off[]={0,0};ccc_error=1;
    assert(write_value(4,0x29,0,0,off,2)==17 && ble->cccd[0]);ccc_error=0;
    ccc_drop=1;assert(write_value(4,0x29,0,0,off,2)==17 && ble->cccd[0]);ccc_drop=0;
    assert(write_value(4,0x29,0,0,off,2)==0 && saved_ccc[0][7]==2);
    key(HID_DISCONNECT,0);closed();runtime_hogp_command(HID_CONNECT,0,target,1,0);
    link();runtime_hogp_service(1);assert(ble->status.state==1 && !ble->cccd[0] && ble->cccd[1]);
    subscribe(4);key(HID_DISCONNECT,0);closed();
    saved_ccc[0][0]^=1;runtime_hogp_init(NULL,read_original,write_original);assert(runtime_hogp_mode(1));runtime_hogp_service(1);
    runtime_hogp_command(HID_CONNECT,0,target,1,0);link();runtime_hogp_service(1);
    assert(ble->status.state==4); /* Reused bond slots cannot inherit another identity's CCCDs. */
    saved_ccc[0][0]^=1;subscribe(4);key(HID_DISCONNECT,0);closed();
    runtime_hogp_command(HID_FORGET,0,target,1,0);assert(forgotten==1 && !runtime_hogp_bond(0,a,&type));
    assert(!ccc_valid[0]);
    memset(target,0,16);runtime_hogp_command(HID_PAIR,1000,target,1,0);
    clock_ms+=1000;runtime_hogp_service(1);event(0xc8,4);assert(confirmations==1);
    closed();runtime_hogp_command(HID_PAIR,1000,target,1,0);runtime_hogp_service(2);
    assert(!ble->status.pairing);event(0xc8,4);assert(confirmations==1);closed();
    /* Control replies follow the command writer even if another host connects. */
    assert(write_value(5,0x0d,0,0,on,2)==0);
    runtime_hogp_control_notify(4,0xf,on,1);assert(last_connection==5);
    runtime_hogp_init(NULL,read_original,write_original);assert(!runtime_hogp_enabled());
    runtime_hogp_control_notify(4,0xf,on,1);assert(last_connection==4);
    assert(runtime_hogp_mode(1));stock_hci_stack=NULL;runtime_hogp_service(1);
    assert(!runtime_hogp_enabled() && !runtime_hogp_mode(1));
    assert(!ble && !allocated);stock_hci_stack=hci_context;
    fail_alloc=1;assert(!runtime_hogp_mode(1) && !allocated);fail_alloc=0;
    unsigned generation=next_generation;
    for(unsigned i=0;i<12;++i) {
        assert(runtime_hogp_mode(1));assert(ble->status.generation==generation+i);runtime_hogp_service(1);assert(allocated==1 && ble);
        assert(runtime_hogp_mode(0));hci_context[0x4e5]=1;
        runtime_hogp_service(1);assert(allocated==1 && ble); /* Address restoration still owns it. */
        hci_context[0x4e5]=0;runtime_hogp_service(1);assert(!ble && !allocated);
    }
    /* Friendly names are optional: they neither own the HID job queue nor
     * change the typed bond identity, and native client busy/failure is benign. */
    paired=encrypted=gatt_enabled=1;gatt.state=37;
    assert(runtime_hogp_mode(1));runtime_hogp_service(1);
    reverse(target,address);target[6]=0;
    runtime_hogp_command(HID_CONNECT,0,target,1,0);subscribe(4);
    unsigned prepared=gatt_prepared;clock_ms+=2001;runtime_hogp_service(1);
    assert(gatt_prepared==prepared && !ble->name_requested);
    gatt.state=0;gatt_failed=1;runtime_hogp_service(1);
    assert(!gatt_started && ble->name_requested==1);
    unsigned name_errors=ble->status.errors;
    key(HID_CONSUMER,0xcd);clock_ms+=80;runtime_hogp_service(1);
    assert(!ble->phase && ble->status.errors==name_errors);
    key(HID_DISCONNECT,0);closed();gatt_failed=0;
    runtime_hogp_command(HID_CONNECT,0,target,1,0);subscribe(4);clock_ms+=2001;
    runtime_hogp_service(1);assert(gatt_started==1 && ble->name_requested==1);
    char name[BT_NAME_BYTES+1];assert(!runtime_hogp_name(target,0,name));
    unsigned char not_found[]={0xa0,3,4,0,0x0a};
    name_reply(4,0,not_found,sizeof not_found);assert(!runtime_hogp_name(target,0,name));
    key(HID_CONSUMER,0xcd);clock_ms+=80;runtime_hogp_service(1);
    assert(!ble->phase && ble->status.errors==name_errors);
    unsigned char malformed[]={0xa5,0,5,0,3,0,1,0,'x'};
    name_reply(4,0,malformed,sizeof malformed);assert(!name_writes);
    malformed[2]=4;malformed[6]=2;name_reply(4,0,malformed,sizeof malformed);assert(!name_writes);
    name_reply(4,0,malformed,7);assert(!name_writes);
    encrypted=0;name_value("Living Room TV",14);assert(!name_writes);encrypted=1;
    name_value("Living Room TV",14);assert(name_writes==1);
    assert(runtime_hogp_name(target,0,name)==14 && !strcmp(name,"Living Room TV"));
    assert(!runtime_hogp_name(target,1,name));
    name_value("Unsolicited overwrite",21);assert(name_writes==1);
    unsigned errors=ble->status.errors;
    key(HID_CONSUMER,0xcd);clock_ms+=80;runtime_hogp_service(1);assert(!ble->phase && ble->status.errors==errors);
    key(HID_DISCONNECT,0);closed();gatt.state=0;
    assert(runtime_hogp_mode(0));runtime_hogp_service(1);assert(!ble && !allocated);
    assert(runtime_hogp_mode(1));runtime_hogp_service(1);
    assert(runtime_hogp_name(target,0,name)==14 && !strcmp(name,"Living Room TV"));
    assert(name_writes==1); /* The persisted name works before reconnecting. */
    runtime_hogp_command(HID_CONNECT,0,target,1,0);subscribe(4);clock_ms+=2001;runtime_hogp_service(1);
    name_value("Living Room TV",14);assert(name_writes==1); /* No redundant flash writes. */
    ble->name_requested=1;fail_alloc=1;name_value("New TV",6);fail_alloc=0;
    assert(runtime_hogp_name(target,0,name)==14 && name_writes==1);
    ble->name_requested=1;name_value("\n\t ",3);assert(name_writes==1);
    ble->name_requested=1;name_value("New\nTV",6);
    assert(runtime_hogp_name(target,0,name)==6 && !strcmp(name,"New TV"));
    char utf8[33];memset(utf8,'a',31);utf8[31]=(char)0xc3;utf8[32]=(char)0xa9;
    ble->name_requested=1;name_value(utf8,sizeof utf8);
    assert(runtime_hogp_name(target,0,name)==31 && name[30]=='a');
    unsigned name_write_count=name_writes;address[0]^=1;bonds();assert(!runtime_hogp_name(target,0,name));
    address[0]^=1;bonds();assert(runtime_hogp_name(target,0,name)==31 && name_writes==name_write_count);
    key(HID_DISCONNECT,0);closed();
    runtime_hogp_command(HID_FORGET,0,target,1,0);assert(!saved_name_size[0] && !runtime_hogp_name(target,0,name));
    assert(runtime_hogp_mode(0));runtime_hogp_service(1);assert(!allocated);
    puts("HOGP: peer/security gates, reports, bond reuse, name lookup/cache/lifecycle, failure isolation and memory cleanup passed");
}
