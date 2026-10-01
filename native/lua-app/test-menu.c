static void menu_tick(void) { clock_ms+=120;runtime_native_service();fake_menu_timer(); }
static void menu_press(unsigned key) {
    if(menu.page) { const unsigned char raw[]={key,0,2};assert(runtime_key(raw)); }
    else menu.injected=key+1;
    menu_tick();
}
static void test_menu(void) {
    assert(!app.L && !allocations);
    memset(&app,0,sizeof app);memset(&saved,0,sizeof saved);memset(&menu,0,sizeof menu);
    memset(&preferences,0,sizeof preferences);memset(&peripheral,0,sizeof peripheral);
    fake_hid_status=(struct hid_status){0};bt_media_state=bt_audio_state=0;
    memset(persisted_size,0,sizeof persisted_size);memset(config_context,0,sizeof config_context);
    unsigned base=0x6000,payload=0x6100,changed;
    memcpy(config_context+12,&base,4);memcpy(config_context+8,&payload,4);stock_config_context=config_context;
    runtime_boot_service();saved.boot_done=1;
    menu_request(1);menu_tick();assert(stock_menu_context && menu_id()==0);
    menu_press(2);assert(menu_id()==31);menu_press(4);assert(menu.page==MENU_SETTINGS);
    assert(!runtime_screen_allowed(NULL) && runtime_screen_allowed(app.frame));
    menu_press(4);assert(menu.page==MENU_AUTOSTART && !menu.index);
    unsigned writes=persist_writes;
    menu_press(3);menu_press(0);assert(!preferences.autostart_off && persist_writes==writes);
    menu_press(4);menu_press(3);menu_press(4);assert(preferences.autostart_off && persist_writes==writes+1);
    clock_ms+=1001;
    struct system_preferences old=preferences;preferences.autostart_off=0;system_load();assert(preferences.autostart_off);
    struct system_preferences next=old;next.autostart_off=0;persist_torn=1;
    assert(system_save(&next));persist_torn=0;system_load();assert(preferences.autostart_off);
    assert(!persist_save(2,"KEEP TV SETTINGS",16,&changed));
    assert(!save_app("return {}",9,&changed));
    menu_press(0);menu_press(3);menu_press(4);assert(menu.page==MENU_REMOVE && !menu.index);
    writes=persist_writes;menu_press(4);assert(menu.page==MENU_SETTINGS && persist_writes==writes);
    /* Removing an app cannot erase the pairing/preferences/settings namespaces. */
    menu_press(4);menu_press(3);clock_ms+=1001;menu_press(4);assert(menu.page==MENU_SETTINGS);
    unsigned bank=0,foreign=0;struct saved_record *r=persist_latest(1,&bank,&foreign);
    assert(r && !r->length);stock_free(r);r=persist_latest(2,&bank,&foreign);
    assert(r && r->length==16 && !memcmp(r->data,"KEEP TV SETTINGS",16));stock_free(r);
    menu_root();menu_press(2);menu_press(2);assert(menu_id()==30);
    menu_press(4);menu_tick();assert(app.state==IDLE && !strcmp(menu.notice,"NO SAVED APP"));
    assert(!save_app("return {}",9,&changed));menu_press(4);menu_tick();
    assert(app.state==RUNNING && !stock_menu_context);launch();assert(app.state==ACTIVE);
    app.cancel=1;service();runtime_native_service();assert(!allocations);
    load("return {init=function() app.menu() end}",1);
    assert(app.state==DONE && menu.request==1);menu_tick();assert(stock_menu_context && !app.L);
    /* Autostart off still allows the launcher; releasing a five-second escape
     * cannot immediately choose the first menu entry. */
    load("return {}",1);unsigned char down[]={4,0,1};runtime_key(down);
    clock_ms+=5000;service();assert(app.state==DONE && (app.suppressed&(1U<<4)));
    menu_tick();unsigned char up[]={4,0,2};assert(runtime_key(up) && !app.suppressed);
    assert(stock_menu_context && menu_id()==0);
    /* The native Tools branch is intact, with Voice Memo appended. */
    menu_press(3);menu_press(3);menu_press(4);assert(menu_id()==8);
    menu_press(2);assert(menu_id()==32);menu_press(4);assert(menu.page==MENU_MEMO);
    writes=persist_writes;menu_press(2);menu_press(4);assert(menu.page==MENU_DELETE && !menu.index);
    menu_press(0);assert(menu.page==MENU_MEMO && persist_writes==writes);
    menu_press(3);menu_press(4);menu_tick();assert(peripheral.recording);
    menu_press(4);menu_tick();assert(!peripheral.recording);
    /* Indicator and lighting preferences override Lua, while stock alarms
     * continue to own native input. */
    preferences.indicator_off=1;assert(runtime_indicator_filter(2)==0);
    preferences.indicator_off=0;assert(runtime_indicator_filter(2)==2);
    preferences.lights=2;assert(runtime_led_override());
    for(unsigned i=0;i<36;++i) assert(!last_leds[i]);preferences.lights=0;
    stock_alarm_state=2;unsigned previous=menu.index;menu.input=4;menu_tick();
    assert(!menu.input && menu.index==previous);stock_alarm_state=0;
    /* USB control-only is descriptor selection, including early startup. */
    saved.initialized=0;assert(runtime_usb_mode(2)==7);saved.initialized=1;
    preferences.usb_noaudio=0;assert(runtime_usb_mode(2)==2);
    preferences.usb_noaudio=1;runtime_usb_select(2,0x8888,0x1719);
    assert(stock_usb_descriptor[10]==0x1e && runtime_usb_mode(8)==8);
    preferences.bt_mode=2;fake_hid_status.keyboard_only=1;menu.bt_at=0;system_bt_service();
    unsigned values[4];memcpy(values,bt_payload,16);assert(values[2]==HID_MODE && !values[3]);
    menu.page=MENU_SETTINGS;menu.parent=3;menu_bonds();assert(menu.page==MENU_DEVICES);
    /* A bond list is a snapshot. Changing native order while its confirmation
     * is open must not make Yes target a different device. */
    memset((void *)stock_bt_manager,0,sizeof stock_bt_manager);
    for(unsigned i=0;i<2;++i) {
        for(unsigned j=0;j<6;++j) stock_bt_manager[7+i*26+j]=i+1;
        stock_bt_manager[7+i*26+25]=1;
    }
    preferences.bt_mode=0;menu_bonds();menu_press(3);menu_press(4);
    assert(menu.page==MENU_FORGET && !menu.index && menu.peer[0]==2);
    menu_press(4);assert(menu.page==MENU_DEVICES && stock_bt_manager[7+26+25]);
    menu_press(3);menu_press(4);
    for(unsigned j=0;j<6;++j) { stock_bt_manager[7+j]=2;stock_bt_manager[7+26+j]=1; }
    menu_press(3);menu_press(4);menu_tick();
    memcpy(values,bt_payload,16);assert(values[2]==HID_FORGET);
    for(unsigned j=0;j<6;++j) assert(bt_payload[16+j]==2);
    assert(menu.operation==101);stock_bt_manager[7+25]=0;
    menu_tick();assert(!menu.operation && menu.page==MENU_DEVICES && menu.bond_count==1);
    /* Malformed diagnostics are bounded and do not enqueue input. */
    unsigned char request[12]={0x37,0x7f,'D','L','U','A',14};
    runtime_command(0,request,9);assert(reply[5]==1);
    request[7]=2;request[8]=255;runtime_command(0,request,12);assert(reply[5]==1 && !menu.injected);
    stock_menu_close(0);memset(&menu,0,sizeof menu);memset(&preferences,0,sizeof preferences);
    memset(&app,0,sizeof app);memset(&saved,0,sizeof saved);memset(&peripheral,0,sizeof peripheral);
    memset(persisted_size,0,sizeof persisted_size);stock_config_context=NULL;persist_writes=0;
    fake_hid_status=(struct hid_status){0};bt_queued.op=0;
    memset((void *)stock_bt_manager,0,sizeof stock_bt_manager);
    frames=led_writes=0;
    assert(!allocations);
    puts("Native menu: navigation, confirmations, independent journal, launch/escape, memo, USB and policy checks passed");
}
