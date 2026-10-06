#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "bluetooth-hogp.c"
static unsigned clock_ms,confirmations,disconnects,notifications,notify_rc,encrypted,paired,adv,forgotten;
static unsigned char connection[0x500],second[0x500],last_report;
static unsigned last_attribute,last_connection,forwarded;
static const unsigned char address[]={0x84,0x5c,0xf3,0xef,0x87,0x78};
static unsigned char hci_context[0x500];
unsigned char *volatile stock_hci_stack=hci_context;
static const unsigned char *selected_db;
void stock_att_set_db(const unsigned char *p) { selected_db=p; }
uint16_t stock_att_request(void *c,unsigned char *p,uint16_t n,unsigned char *r) {
    (void)c;(void)p;(void)n;(void)r;return selected_db==hogp_database ? 1 : 2;
}
volatile unsigned stock_battery_level=6;
unsigned stock_ticks(void) { return clock_ms; }
unsigned stock_free_heap(void) { return 100000; }
void *stock_alloc(unsigned n) { return calloc(1,n); }
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
}
static void key(unsigned op,unsigned value) {
    unsigned char data[16]={0};runtime_hogp_command(op,value,data,1,ble->status.generation);runtime_hogp_service(1);
}
static void subscribe(unsigned connection_handle) {
    unsigned char on[]={1,0};
    assert(write_value(connection_handle,0x29,0,0,on,2)==0);
    assert(write_value(connection_handle,0x2d,0,0,on,2)==0);
    runtime_hogp_service(1);
}
int main(void) {
    memcpy(connection+4,address,6);memset(second+4,0x77,6);
    unsigned state=7;memcpy(connection+20,&state,4);memcpy(second+20,&state,4);
    runtime_hogp_init(NULL,read_original,write_original);
    assert(runtime_hogp_mode(1));
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
    runtime_hogp_command(HID_FORGET,0,target,1,0);assert(forgotten==1 && !runtime_hogp_bond(0,a,&type));
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
    free(ble);ble=NULL;
    puts("HOGP: bounded attributes, peer/security gates, all remote reports, bond reuse, timeout/abort releases, stale jobs and control routing passed");
}
