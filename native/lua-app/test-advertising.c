/* Exercise the real Lua boundary, job queue and advertising state machine.
 * The fake controller retains pointers and completes commands on a later tick. */
static unsigned char advertising_stack[0x520];
unsigned char *volatile stock_hci_stack=advertising_stack;
static unsigned advertising_ready=1,advertising_stalled;
static const unsigned char original_ad[]={2,1,6},original_scan[]={2,9,'X'};
unsigned runtime_hogp_advertising_ready(void) { return advertising_ready; }
void stock_le_adv_data(unsigned n,const unsigned char *p) {
    assert(n<=31);memcpy(advertising_stack+0x4d8,&p,sizeof p);advertising_stack[0x4dc]=n;advertising_stack[0x4e8]|=1;
}
void stock_le_scan_data(unsigned n,const unsigned char *p) {
    assert(n<=31);memcpy(advertising_stack+0x4e0,&p,sizeof p);advertising_stack[0x4e4]=n;advertising_stack[0x4e8]|=2;
}
void stock_le_adv_enable(unsigned on) { advertising_stack[0x4e6]=on; }
void stock_le_adv_params(unsigned min,unsigned max,unsigned type,unsigned peer_type,const unsigned char *peer,unsigned channels,unsigned filter) {
    unsigned char p[]={min,min>>8,max,max>>8,type,peer_type,channels,filter};
    memcpy(advertising_stack+0x4ea,p,sizeof p);memcpy(advertising_stack+0x4f2,peer,6);advertising_stack[0x4e8]|=0x84;
}
static void adv_pump(void) {
    struct bt_command command;runtime_bt_peek(&command);bt_queued.op=0;
    if(!advertising_stalled && stock_hci_stack) {
        /* Dereference the retained payloads after their setter returned. */
        const unsigned char *p;memcpy(&p,advertising_stack+0x4d8,sizeof p);
        assert(!advertising_stack[0x4dc] || ad_valid(p,advertising_stack[0x4dc]));
        memcpy(&p,advertising_stack+0x4e0,sizeof p);
        assert(!advertising_stack[0x4e4] || ad_valid(p,advertising_stack[0x4e4]));
        advertising_stack[0x4e8]&=~7;advertising_stack[0x4e5]=advertising_stack[0x4e6];
    }
    clock_ms+=20;
}
static void adv_original(unsigned enabled) {
    memset(advertising_stack,0,sizeof advertising_stack);
    const unsigned char peer[]={1,2,3,4,5,6};
    stock_le_adv_params(48,80,0,1,peer,5,0);stock_le_adv_data(sizeof original_ad,original_ad);
    stock_le_scan_data(sizeof original_scan,original_scan);stock_le_adv_enable(enabled);adv_pump();
}
static void adv_restored(unsigned enabled) {
    const unsigned char *p;memcpy(&p,advertising_stack+0x4d8,sizeof p);assert(p==original_ad);
    memcpy(&p,advertising_stack+0x4e0,sizeof p);assert(p==original_scan);
    const unsigned char params[]={48,0,80,0,0,1,5,0,1,2,3,4,5,6};
    assert(!memcmp(advertising_stack+0x4ea,params,sizeof params));
    assert(advertising_stack[0x4e6]==enabled && !runtime_advertising_busy());
}
static void adv_stop_app(void) {
    app.cancel=1;service();runtime_native_service();adv_pump();adv_pump();
    assert(!allocations && !peripheral.advertisement && !runtime_advertising_busy());
}
static void adv_load(void) {
    load("local t;return {init=function() t=assert(bluetooth.advertise {data=string.char(2,1,6),duration_ms=200}) end,"
         "update=function() local ok,e=device.result(t);if ok~=nil then app.log(ok and 'done' or e) end end,"
         "message=function(s) if s=='cancel' then assert(bluetooth.advertise_cancel(t)) "
         "elseif s=='loop' then while true do end "
         "elseif s=='oom' then local a={};while true do a[#a+1]=string.rep('x',100) end "
         "else assert(select(2,bluetooth.advertise {data=string.char(2,1,6)})=='busy') end end}",1);
    assert(app.state==ACTIVE);runtime_native_service();adv_pump();
}
static void adv_message(const char *s) {
    memcpy(app.work->message,s,strlen(s));app.message_size=strlen(s);tick();
}
static void test_advertising(void) {
    extern int runtime_integer(char *,unsigned,int);
    extern int runtime_pointer(char *,unsigned,const void *);
    char number[32],expected[32];unsigned random=5;
    for(unsigned i=0;i<10000;++i) {
        random=random*1664525U+1013904223U;
        snprintf(expected,sizeof expected,"%d",(int)random);
        assert(runtime_integer(number,sizeof number,(int)random)==(int)strlen(expected) && !strcmp(number,expected));
        snprintf(expected,sizeof expected,"0x%x",random);
        assert(runtime_pointer(number,sizeof number,(void *)(uintptr_t)random)==(int)strlen(expected) && !strcmp(number,expected));
    }
    assert(runtime_integer(number,sizeof number,INT32_MIN)==11 && !strcmp(number,"-2147483648"));
    assert(runtime_integer(number,sizeof number,0)==1 && !strcmp(number,"0"));
    assert(!runtime_integer(number,1,0) && !runtime_pointer(number,2,NULL));
    check("return tostring(-2147483647-1)..':'..tostring(2147483647)..':'..tostring(0)",DONE,"-2147483648:2147483647:0");
    const char *invalid[]={"data=''","data=string.rep('x',32)","data=string.char(0)",
        "data=string.char(3,1,6)","data=string.char(2,1,6),duration_ms=5001",
        "data=string.char(2,1,6),interval_ms=99","data=string.char(2,1,6),type='directed'",
        "data=string.char(2,1,6),scan_response=string.char(2,9,88)",
        "data=string.char(2,1,6),duration_ms=100,interval_ms=200",NULL};
    char source[256];
    for(unsigned i=0;invalid[i];++i) { snprintf(source,sizeof source,"return bluetooth.advertise {%s}",invalid[i]);check(source,ERROR,NULL); }
    check("return select(2,bluetooth.advertise {data=string.char(2,1,6)})",DONE,"native requests require a resident app");
    for(unsigned enabled=0;enabled<2;++enabled) {
        adv_original(enabled);adv_load();assert(advertising && advertising->phase==1);
        adv_message("busy");adv_pump();adv_pump();assert(advertising->phase==3);
        for(unsigned i=0;i<15;++i) adv_pump();tick();assert(!strcmp(app.result,"done"));
        adv_restored(enabled);adv_stop_app();
    }
    const char *actions[]={"cancel","loop","oom"};
    for(unsigned i=0;i<3;++i) {
        adv_original(1);adv_load();adv_pump();adv_pump();adv_message(actions[i]);
        adv_pump();adv_pump();adv_restored(1);
        if(i) { assert(app.state==ERROR);runtime_native_service();assert(!allocations); }
        else adv_stop_app();
    }
    adv_original(1);advertising_ready=0;adv_load();assert(!advertising);tick();
    assert(!strcmp(app.result,"busy"));advertising_ready=1;adv_stop_app();
    adv_original(1);adv_load();adv_pump();
    unsigned char rejection[]={0x0e,4,1,8,0x20,0x0c},hci[16]={0},*packet=rejection;
    uint16_t packet_size=sizeof rejection;memcpy(hci+8,&packet_size,2);memcpy(hci+12,&packet,sizeof packet);
    runtime_bt_trace_hci(hci); /* Failure observation works without an allocated trace ring. */
    adv_pump();adv_pump();tick();assert(!strcmp(app.result,"controller rejected advertising"));adv_restored(1);adv_stop_app();
    adv_original(1);adv_load();advertising_stalled=1;
    for(unsigned i=0;i<110;++i) adv_pump();advertising_stalled=0;
    assert(!advertising);adv_pump();adv_restored(1);adv_stop_app();
    /* A request cancelled before dispatch never starts, even from a stale queue entry. */
    adv_original(1);
    load("return {init=function() local t=assert(bluetooth.advertise {data=string.char(2,1,6)});assert(bluetooth.advertise_cancel(t)) end}",1);
    runtime_native_service();adv_pump();assert(!advertising);adv_stop_app();
    /* Connected/pending LE links are left intact. */
    adv_original(1);unsigned char link[32]={0},*ptr=link;memcpy(advertising_stack+16,&ptr,sizeof ptr);
    adv_load();assert(!advertising);tick();assert(!strcmp(app.result,"busy"));
    memset(advertising_stack+16,0,sizeof ptr);adv_stop_app();
    adv_original(1);adv_load();adv_pump();adv_pump();
    memcpy(advertising_stack+16,&ptr,sizeof ptr);adv_pump();adv_pump();
    assert(!advertising && !memcmp(advertising_stack+16,&ptr,sizeof ptr));
    memset(advertising_stack+16,0,sizeof ptr);adv_restored(1);adv_stop_app();
    /* Scannable/connectable data, including a full 31-byte AD structure. */
    for(unsigned type=0;type<3;type+=2) {
        adv_original(1);
        snprintf(source,sizeof source,"local t;return {init=function() t=assert(bluetooth.advertise {type='%s',data=string.char(30,255)..string.rep('a',29),scan_response=string.char(2,9,88),duration_ms=100}) end}",type ? "scannable" : "connectable");
        load(source,1);runtime_native_service();adv_pump();adv_pump();adv_pump();
        assert(advertising->packet.type==type && advertising_stack[0x4dc]==31 && advertising_stack[0x4e4]==3);
        adv_stop_app();adv_restored(1);
    }
    adv_original(1);adv_load();adv_pump();adv_pump();stock_le_adv_data(sizeof original_ad,original_ad);
    adv_pump();assert(!advertising);adv_stop_app();
    adv_original(1);adv_load();adv_pump();adv_pump();runtime_advertising_reset();
    adv_pump();adv_restored(1);adv_stop_app();
    adv_original(1);adv_load();adv_pump();adv_pump();
    stock_hci_stack=NULL;adv_pump();assert(!advertising);stock_hci_stack=advertising_stack;
    /* The destroyed stack cannot retain its old pointers. */
    adv_original(1);adv_stop_app();
    adv_original(1);bt_queue_ok=0;adv_load();assert(!advertising);bt_queue_ok=1;adv_stop_app();
    adv_original(1);
    load("return {init=function() assert(bluetooth.advertise {data=string.char(2,1,6)}) end}",1);
    runtime_native_service();clock_ms+=1001;runtime_native_service();adv_pump();assert(!advertising);adv_stop_app();
    adv_original(1);
    load("return {init=function() assert(bluetooth.advertise {data=string.char(2,1,6)}) end}",1);
    runtime_native_service();fail_alloc=1;adv_pump();fail_alloc=0;
    assert(!advertising && peripheral.state==JOB_DONE && !strcmp(peripheral.error,"insufficient native heap"));adv_stop_app();
    /* The unsigned duration arithmetic remains bounded across tick wrap. */
    adv_original(1);adv_load();adv_pump();adv_pump();
    clock_ms=0xffffffd0U;advertising->started=clock_ms;
    for(unsigned i=0;i<15;++i) adv_pump();adv_restored(1);adv_stop_app();
    puts("Advertising payload ownership, restoration, cancellation, Lua loop/OOM and controller deadlines passed");
}
