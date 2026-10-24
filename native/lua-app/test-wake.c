/* Run the bundled apps through the patched VM and real advertising engine.
 * Controller completion and HID state use the existing native test doubles. */
static void wake_load(const char *name) {
    char path[128],source[SOURCE_LIMIT+1];
    snprintf(path,sizeof path,"target/lua-app/runtime/%s.bundle.lua",name);
    FILE *f=fopen(path,"rb");assert(f);size_t n=fread(source,1,sizeof source-1,f);
    assert(!ferror(f) && feof(f));fclose(f);source[n]=0;load(source,1);
}
static void wake_message(const char *s) {
    strcpy(app.work->message,s);app.message_size=strlen(s);tv_ticks(1);
}
static void wake_packet(unsigned last) {
    const unsigned char expected[]={2,1,6,17,7,0,1,2,1,5,3,255,0,1,last,12,208,180,205,12,0};
    assert(advertising && advertising->phase==3);
    assert(advertising->packet.size==sizeof expected && !memcmp(advertising->packet.data,expected,sizeof expected));
    assert(advertising->packet.type==3 && advertising->packet.interval_ms==100 && advertising->packet.duration_ms==3000);
}
static void test_wake(void) {
    adv_original(1);fake_hid_status=(struct hid_status){.transport=1};
    saved.initialized=saved.boot_done=1;stock_config_context=config_context;
    const char *setting="TV6|0C:CD:B4:D0:0C:26|public|0";
    memcpy(saved.settings,setting,strlen(setting));saved.settings_size=strlen(setting);
    unsigned writes=persist_writes;
    wake_load("tv-wake");tv_ticks(10);assert(!advertising && !strcmp(app.result,"READY"));
    wake_message("wake 0C:CD:B4:D0:0C:26 broken");clock_ms+=13000;tv_ticks(10);
    assert(!advertising && !strcmp(app.result,"INVALID TARGETS"));
    wake_message("wake");assert(!strcmp(app.result,"WAIT"));
    wake_message("cancel");clock_ms+=13000;tv_ticks(10);assert(!advertising && !strcmp(app.result,"CANCELLED"));
    wake_message("wake 0C:CD:B4:D0:0C:26 0c:cd:b4:d0:0c:26 0C:CD:B4:D0:0C:27");
    clock_ms+=12000;tv_ticks(5);wake_packet(38);
    clock_ms+=3000;tv_ticks(8);wake_packet(39);
    clock_ms+=3000;tv_ticks(8);assert(!advertising && !strcmp(app.result,"WAKE SENT"));adv_restored(1);
    tv_key(7);clock_ms+=12000;tv_ticks(5);wake_packet(38);
    wake_message("cancel");tv_ticks(8);assert(!advertising && !strcmp(app.result,"CANCELLED"));adv_restored(1);
    unsigned char link[32]={0},*ptr=link;memcpy(advertising_stack+16,&ptr,sizeof ptr);
    wake_message("wake");clock_ms+=12000;tv_ticks(8);
    assert(!advertising && !strcmp(app.result,"busy") && !memcmp(advertising_stack+16,&ptr,sizeof ptr));
    memset(advertising_stack+16,0,sizeof ptr);adv_stop_app();
    assert(persist_writes==writes && saved.settings_size==strlen(setting) && !memcmp(saved.settings,setting,strlen(setting)));
    stock_config_context=NULL;saved.settings_size=0;
    fake_hid_status=(struct hid_status){0};bt_queued.op=0;
    puts("Lua wake: target vectors, no startup transmission, multiple targets, cancellation, busy-link preservation passed");
}
