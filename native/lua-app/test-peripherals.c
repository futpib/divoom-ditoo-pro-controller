static void request_test(const char *source) {
    load(source,1); assert(app.state==ACTIVE); runtime_native_service();
    tick();
    if (app.state!=ACTIVE || strcmp(app.result,"ok")) {
        fprintf(stderr,"native request: %s\nresult %s\n",source,app.result);abort();
    }
    app.cancel=1;service();runtime_native_service();
    assert(!allocations);
}
static void test_bt_mute(void) {
    struct bt_command peek;
    unsigned char *channel=bt_channel;
    memcpy(bt_context+0x9c,&channel,sizeof channel);bt_context[0xa6]=1;
    bt_media_state=2;
    load("return {init=function() assert(bluetooth.mute()) end}",1);
    runtime_native_service();
    assert(bt_queued.op==BT_MUTE_COMMAND && bt_queued.length==10);
    assert(bt_panel_calls==0); /* Only the Bluetooth task can send the key. */
    runtime_bt_peek(&peek);
    assert(peek.op==BT_MUTE_COMMAND && bt_panel_calls==2 && bt_pressed[0]==1 && bt_pressed[1]==0);
    /* Stock commands and malformed/private requests pass to stock cleanup. */
    bt_queued.op=0x14;runtime_bt_peek(&peek);assert(peek.op==0x14 && bt_panel_calls==2);
    bt_queued.op=BT_MUTE_COMMAND;bt_queued.length=9;runtime_bt_peek(&peek);
    bt_queued.length=10;bt_queued.data=NULL;runtime_bt_peek(&peek);bt_queued.data=bt_payload;
    bt_media_state=0;runtime_bt_peek(&peek);bt_media_state=2;
    stock_bt_context=NULL;runtime_bt_peek(&peek);stock_bt_context=bt_context;
    bt_context[0xa6]=0;runtime_bt_peek(&peek);bt_context[0xa6]=1;
    stock_bt_manager[0xe0]^=1;runtime_bt_peek(&peek);stock_bt_manager[0xe0]^=1;
    channel=NULL;memcpy(bt_context+0x9c,&channel,sizeof channel);runtime_bt_peek(&peek);
    channel=bt_channel;memcpy(bt_context+0x9c,&channel,sizeof channel);
    assert(bt_panel_calls==2);
    /* Saturated/corrupt rings must not produce a press without its release. */
    for (unsigned i=12;i<16;++i) { bt_channel[0x2bb]=i;runtime_bt_peek(&peek); }
    bt_channel[0x2bb]=0;bt_channel[0x2ba]=15;runtime_bt_peek(&peek);bt_channel[0x2ba]=0;
    assert(bt_panel_calls==2);
    bt_channel[0x2bb]=11;runtime_bt_peek(&peek);assert(bt_panel_calls==4);bt_channel[0x2bb]=0;
    bt_panel_ok=0;runtime_bt_peek(&peek);assert(bt_panel_calls==5);bt_panel_ok=1;
    app.cancel=1;service();runtime_native_service();runtime_bt_peek(&peek);
    assert(bt_panel_calls==5 && !allocations);
    bt_queue_ok=0;
    request_test("local t;return {init=function() t=assert(bluetooth.mute()) end,"
        "update=function() local ok,e=device.result(t);assert(ok==false and e=='Bluetooth queue full');print('ok') end}");
    bt_queue_ok=1;bt_media_state=0;
    request_test("local t;return {init=function() t=assert(bluetooth.mute()) end,"
        "update=function() local ok,e=device.result(t);assert(ok==false and e=='media peer not connected');print('ok') end}");
    puts("Mute queue copies payload, runs on Bluetooth task, reserves release space and rejects stale/disconnected peers");
}
static void test_tv_remote(void) {
    char source[SOURCE_LIMIT+1];
    FILE *file=fopen("examples/lua/tv-remote.lua","rb");assert(file);
    size_t n=fread(source,1,sizeof(source)-1,file);assert(!ferror(file));fclose(file);source[n]=0;
    bt_media_state=2;bt_audio_state=0;
    memcpy((void *)(stock_bt_manager+0xe0),"\x26\x0c\xd0\xb4\xcd\x0c",6);
    load(source,1);
    if (app.state!=ACTIVE) { fprintf(stderr,"TV app failed: %s (peak %u)\n",app.result,app.peak);abort(); }
    tick();tick();
    memcpy(app.work->message,"connect 0C:CD:B4:D0:0C:26",25);app.message_size=25;
    for (unsigned i=0;i<12;++i) { tick();runtime_native_service(); }
    assert(app.state==ACTIVE);
    clock_ms+=21000;tick();
    assert(strstr(app.result,"TV media connected"));
    unsigned commands=bt_commands;
    runtime_adc_result(1U<<16 | 2);tick();runtime_adc_result(2U<<16 | 2);tick();
    assert(bt_commands==commands);
    for (unsigned expected=2;expected>0;--expected) {
        clock_ms+=600;runtime_adc_result(1U<<16 | 2);tick();
        runtime_adc_result(2U<<16 | 2);
        for (unsigned i=0;i<8;++i) { tick();runtime_native_service(); }
        assert(app.state==ACTIVE && bt_last_action==expected);
        unsigned lit=0;for (unsigned i=0;i<FRAME_BYTES;++i) lit|=app.frame[i];
        assert(lit);
    }
    assert(bt_commands==commands+2);
    runtime_adc_result(4U<<16 | 2);tick();runtime_adc_result(5U<<16 | 2);tick();
    assert(bt_commands==commands+2);
    unsigned mute_calls=bt_panel_calls;
    runtime_adc_result(1U<<16 | 3);tick();runtime_adc_result(2U<<16 | 3);tick();
    assert(strstr(app.result,"mute toggle key 3") && bt_panel_calls==mute_calls);
    for (unsigned i=0;i<2;++i) {
        clock_ms+=600;runtime_adc_result(1U<<16 | 3);tick();runtime_adc_result(2U<<16 | 3);
        for (unsigned j=0;j<8;++j) { tick();runtime_native_service(); }
        struct bt_command peek;runtime_bt_peek(&peek);
        assert(app.state==ACTIVE && bt_panel_calls==mute_calls+2*(i+1));
    }
    runtime_adc_result(4U<<16 | 3);tick();runtime_adc_result(5U<<16 | 3);tick();
    runtime_adc_result(1U<<16 | 4);tick();runtime_adc_result(2U<<16 | 4);tick();
    assert(bt_panel_calls==mute_calls+4 && bt_commands==commands+2);
    /* Muting must not change the next play/pause action. */
    clock_ms+=600;runtime_adc_result(1U<<16 | 2);tick();runtime_adc_result(2U<<16 | 2);
    for (unsigned i=0;i<8;++i) { tick();runtime_native_service(); }
    assert(bt_commands==commands+3 && bt_last_action==2);
    app.cancel=1;service();runtime_native_service();assert(!allocations);
    bt_media_state=0;
    puts("TV remote binds two keys, keeps play/pause independent of mute toggle and ignores unrelated/repeat events");
}
static void test_peripherals(void) {
    check("local b=power.battery(); return b.level..':'..tostring(b.charging)",DONE,"6:true");
    check("local x,e=alarm.get(0); return e",DONE,"native requests require a resident app");
    check("return alarm.get(10)",ERROR,NULL);
    check("return power.indicator(6)",DONE,NULL);
    load("return {init=function() power.indicator(3) end}",1);
    assert(runtime_indicator_filter(5)==3);
    runtime_native_service();assert(indicator_output==3);
    app.cancel=1;service();runtime_native_service();assert(indicator_output==5);
    assert(runtime_indicator_filter(2)==2);
    request_test("local t; return {init=function() t=assert(alarm.get(0)) end,"
        "update=function() local ok,v=device.result(t); assert(ok and v.mode==2 and v.volume==50);print('ok') end}");
    request_test("local t; return {init=function() t=assert(alarm.set(2,{enabled=true,hour=8,min=30,days=62})) end,"
        "update=function() local ok,v=device.result(t); assert(ok and v.enabled and v.mode==2 and v.hour==8);print('ok') end}");
    assert(config_writes==1 && reschedules==1 && saved_alarms[2][4]==62);
    /* Identical saves do not wear flash, even inside the cooldown. */
    request_test("local t; return {init=function() t=assert(alarm.set(2,{hour=8})) end,"
        "update=function() local ok=device.result(t);assert(ok);print('ok') end}");
    assert(config_writes==1);
    request_test("local t; return {init=function() t=assert(alarm.set(2,{hour=9})) end,"
        "update=function() local ok,e=device.result(t);assert(ok==false and e=='saved settings rate limited');print('ok') end}");
    assert(config_writes==1);clock_ms+=1000;
    request_test("local t; return {init=function() t=assert(power.set_schedule(8,{action='on',hour=7,min=15,color=0x123456,enabled=true})) end,"
        "update=function() local ok,v=device.result(t);assert(ok and v.color==0x123456 and v.action=='on');print('ok') end}");
    assert(saved_wakes[8*16+2]==1 && saved_wakes[8*16+6]==0x12 && reschedules==2);
    config_bad_length=1;
    request_test("local t; return {init=function() t=assert(power.get_schedule(0)) end,"
        "update=function() local ok,e=device.result(t);assert(ok==false and e=='invalid native config size');print('ok') end}");
    config_bad_length=0;
    memset(saved_wakes+6*16,0xff,16);
    request_test("local t; return {init=function() t=assert(power.get_schedule(6)) end,"
        "update=function() local ok,v=device.result(t);assert(ok and not v.enabled and v.hour==0);print('ok') end}");
    memset(saved_wakes+6*16,0,16);
    clock_ms+=1000;config_fail_write=1;
    request_test("local t; return {init=function() t=assert(alarm.set(2,{hour=10})) end,"
        "update=function() local ok,e=device.result(t);assert(ok==false and e=='config readback mismatch');print('ok') end}");
    config_fail_write=0; assert(reschedules==2);
    load("return {init=function() assert(alarm.set(2,{hour=11})); error('cancel before native dispatch') end}",1);
    assert(app.state==ERROR); unsigned writes=config_writes;runtime_native_service();assert(config_writes==writes);
    load("local t;return {init=function() t=assert(microphone.noise(true)) end,"
        "update=function() while true do end end}",1);
    runtime_native_service();assert(noise_enabled);
    runtime_noise_sample(73);assert(peripheral.noise_value==73);
    tick();assert(app.state==ERROR);runtime_native_service();assert(!noise_enabled);
    /* Cleanup restores an existing stock monitor, rather than disabling it. */
    noise_context[3]=1;
    load("return {init=function() assert(microphone.noise(true)) end}",1);runtime_native_service();
    app.action=1;tick();runtime_native_service();assert(noise_enabled);
    app.cancel=1;service();noise_context[3]=0;
    load("return {init=function() assert(microphone.record()) end}",1);
    runtime_native_service();assert(peripheral.recording && stock_memo_context);
    clock_ms+=60000;runtime_native_service();assert(!peripheral.recording && memo_stops==1);
    app.cancel=1;service();
    load("return {init=function() assert(microphone.record()) end, update=function() while true do end end}",1);
    runtime_native_service();tick();runtime_native_service();assert(memo_stops==2 && !stock_memo_context);
    load("return {}",1);
    stock_alarm_state=2;
    assert(runtime_adc_result(1U<<16 | 2)==(1U<<16 | 2));
    assert(!runtime_screen_allowed(app.frame) && runtime_screen_allowed(saved_wakes));
    assert(!runtime_led_override());stock_alarm_state=0;
    app.cancel=1;service();runtime_native_service();
    request_test("local t;return {init=function() t=assert(audio.repeat_mode('all')) end,"
        "update=function() local ok,e=device.result(t);assert(ok==false and e=='SD playback unavailable');print('ok') end}");
    preview_blocked=1;
    request_test("local t;return {init=function() t=assert(audio.preview(2,15)) end,"
        "update=function() local ok,e=device.result(t);assert(ok==false and e=='native mode blocks preview');print('ok') end}");
    assert(!peripheral.preview_owned);preview_blocked=0;
    unsigned old_limit=peripheral.writes; peripheral.writes=64;
    request_test("local t; return {init=function() t=assert(alarm.set(2,{hour=11})) end,"
        "update=function() local ok,e=device.result(t);assert(ok==false and e=='64 saved changes per boot exceeded');print('ok') end}");
    peripheral.writes=old_limit;
    check("return bluetooth.connect_media('00:00:00:00:00:00')",ERROR,NULL);
    check("return bluetooth.connect_media('FF:FF:FF:FF:FF:FF')",ERROR,NULL);
    check("return bluetooth.connect_media('0C:CD:B4:D0:0C:2Z')",ERROR,NULL);
    check("return bluetooth.connect_media('0C:CD:B4')",ERROR,NULL);
    request_test("local t;return {init=function() t=assert(bluetooth.connect_media('0C:CD:B4:D0:0C:26')) end,"
        "update=function() local ok=device.result(t);assert(ok);print('ok') end}");
    assert(bt_connects==1 && !memcmp(bt_address,"\x26\x0c\xd0\xb4\xcd\x0c",6));
    request_test("local t;return {init=function() t=assert(bluetooth.connect_media('0C:CD:B4:D0:0C:26')) end,"
        "update=function() local ok,e=device.result(t);assert(ok==false and e=='Bluetooth connection rate limited');print('ok') end}");
    assert(bt_connects==1);
    load("return {init=function() assert(bluetooth.connect_media('0C:CD:B4:D0:0C:26'));error('cancel') end}",1);
    runtime_native_service();assert(bt_connects==1);
    request_test("local t;return {init=function() t=assert(bluetooth.media('play')) end,"
        "update=function() local ok,e=device.result(t);assert(ok==false and e=='media peer not connected');print('ok') end}");
    bt_media_state=2;memcpy((void *)(stock_bt_manager+0xe0),bt_address,6);
    check("local s=bluetooth.status();return s.peer..':'..tostring(s.media_connected)..':'..tostring(s.ble_connected)",DONE,"0C:CD:B4:D0:0C:26:true:true");
    request_test("local t;return {init=function() t=assert(bluetooth.connect_media('0C:CD:B4:D0:0C:27')) end,"
        "update=function() local ok,e=device.result(t);assert(ok==false and e=='another Bluetooth peer is active');print('ok') end}");
    request_test("local t;return {init=function() t=assert(bluetooth.media('pause')) end,"
        "update=function() local ok=device.result(t);assert(ok);print('ok') end}");
    assert(bt_commands==1);
    bt_queue_ok=0;
    request_test("local t;return {init=function() t=assert(bluetooth.media('play')) end,"
        "update=function() local ok,e=device.result(t);assert(ok==false and e=='Bluetooth queue full');print('ok') end}");
    bt_queue_ok=1;bt_media_state=0;
    stock_bt_context=NULL;
    request_test("local t;return {init=function() t=assert(bluetooth.connect_media('0C:CD:B4:D0:0C:26')) end,"
        "update=function() local ok,e=device.result(t);assert(ok==false and e=='Bluetooth unavailable');print('ok') end}");
    stock_bt_context=bt_context;
    test_bt_mute();
    test_tv_remote();
    puts("Bluetooth address validation, connection rate limits, cancellation, peer isolation and queue errors passed");
    puts("Native job cancellation, config readback, wear limits, alarm priority, indicator and microphone cleanup passed");
}

static void test_keyboard_bonds(void) {
    unsigned before=saved_bonds;
    peripheral.writes=0;stock_bt_manager[0]=1;stock_bt_manager[0x125]=0;
    fake_hid_status.state=0;save_keyboard_bond();clock_ms+=3000;save_keyboard_bond();
    assert(saved_bonds==before);
    fake_hid_status.state=2;save_keyboard_bond();clock_ms+=1999;save_keyboard_bond();
    assert(saved_bonds==before && stock_bt_manager[0]);
    ++clock_ms;save_keyboard_bond();assert(saved_bonds==before+1 && !stock_bt_manager[0]);
    clock_ms+=5000;save_keyboard_bond();assert(saved_bonds==before+1);
    stock_bt_manager[0]=1;save_keyboard_bond();clock_ms+=2000;
    peripheral.writes=64;save_keyboard_bond();assert(saved_bonds==before+1 && stock_bt_manager[0]);
    peripheral.writes=1;peripheral.last_write=clock_ms;save_keyboard_bond();
    assert(saved_bonds==before+1 && stock_bt_manager[0]);
    clock_ms+=1000;bond_count=9;save_keyboard_bond();assert(saved_bonds==before+1);
    bond_count=0;save_keyboard_bond();assert(saved_bonds==before+1);
    bond_count=1;stock_bt_manager[0x125]=1;save_keyboard_bond();assert(saved_bonds==before+1);
    stock_bt_manager[0x125]=0;save_keyboard_bond();clock_ms+=1999;save_keyboard_bond();
    assert(saved_bonds==before+1);++clock_ms;save_keyboard_bond();
    assert(saved_bonds==before+2 && !stock_bt_manager[0]);
    fake_hid_status.state=0;
    puts("HID-only bond flush is delayed, bounded and skips unchanged records");
}

static void tv_frame(unsigned stage) {
    const char *directory=getenv("DITOO_UI_FRAMES");if(!directory) return;
    char path[512];snprintf(path,sizeof path,"%s/%u.ppm",directory,stage);
    FILE *f=fopen(path,"wb");assert(f);fputs("P6\n16 16\n255\n",f);
    assert(fwrite(app.frame,1,FRAME_BYTES,f)==FRAME_BYTES);fclose(f);
}
static unsigned tv_ops[32],tv_value,tv_last;
static void tv_ticks(unsigned n) {
    for(unsigned i=0;i<n;++i) {
        tick();runtime_native_service();
        if(app.state!=ACTIVE) { fprintf(stderr,"TV app failed: %s peak %u\n",app.result,app.peak);abort(); }
        if(bt_queued.op==BT_HID_COMMAND) {
            unsigned v[4];memcpy(v,bt_payload,sizeof v);tv_last=v[2];tv_value=v[3];++tv_ops[tv_last];
            if(tv_last==HID_MODE) { fake_hid_status.transport=1;fake_hid_status.keyboard_only=1; }
            if(tv_last==HID_DISCONNECT) { fake_hid_status.state=0;fake_hid_status.pairing=0; }
            if(tv_last==HID_LISTEN || tv_last==HID_CONNECT || tv_last==HID_PAIR) {
                fake_hid_status.state=4;memcpy(fake_hid_status.peer,bt_payload+16,6);
                fake_hid_status.address_type=bt_payload[22];
                fake_hid_status.pairing=tv_last==HID_PAIR;fake_hid_status.pair_remaining_ms=120000;
            }
            if(tv_last==HID_FORGET) for(unsigned j=0;j<16;++j) {
                if(!memcmp(fake_le_bonds[j],bt_payload+16,7)) fake_le_bonds[j][7]=0;
            }
            bt_queued.op=0;
        }
    }
}
static void tv_key(unsigned key) {
    runtime_adc_result(1U<<16|key);tv_ticks(1);runtime_adc_result(2U<<16|key);tv_ticks(8);
}
static void tv_message(const char *message) {
    strcpy(app.work->message,message);app.message_size=strlen(message);tv_ticks(100);
}
static void tv_stop(void) {
    app.cancel=1;service();runtime_native_service();assert(!allocations);
}
static void test_tv_keyboard(void) {
    char source[SOURCE_LIMIT+1];
    FILE *file=fopen("target/lua-app/runtime/tv-keyboard.bundle.lua","rb");assert(file);
    size_t n=fread(source,1,sizeof source-1,file);assert(!ferror(file));fclose(file);source[n]=0;
    memset(fake_le_bonds,0,sizeof fake_le_bonds);
    for(unsigned i=0;i<16;++i) { memset(fake_le_bonds[i],i+1,6);fake_le_bonds[i][6]=i%2;fake_le_bonds[i][7]=1; }
    fake_hid_status=(struct hid_status){.state=2,.enabled=1,.transport=1};memset(fake_hid_status.peer,1,6);
    saved.initialized=saved.boot_done=1;stock_config_context=config_context;
    const char *setting="TV5|01:01:01:01:01:01|public|";
    memcpy(saved.settings,setting,strlen(setting));saved.settings_size=strlen(setting);
    /* The 4 KiB smaller static reservation returns this space to the native heap. */
    track_heap=1;free_heap=76000+(8192-4096);bt_queued.op=0;memset(tv_ops,0,sizeof tv_ops);
    load(source,1);tv_ticks(100);tv_frame(0);
    assert(strstr(app.result,"CONNECTED: 01:01:01:01:01:01 PUBLIC"));
    assert(!memcmp(saved.settings,"TV6|01:01:01:01:01:01|public|1",saved.settings_size));
    unsigned sent_message=tv_ops[HID_CONSUMER];
    tv_message("play_pause");assert(tv_ops[HID_CONSUMER]==sent_message+1 && tv_value==0xcd);
    const unsigned keys[]={4,10,7,1,9,2,3},usages[]={0xcd,0xe2,44,0xe9,0xea,80,79};
    for(unsigned i=0;i<7;++i) {
        clock_ms+=600;tv_key(keys[i]);assert(tv_value==usages[i]);
        assert(tv_last==(i==2 || i>=5 ? HID_KEY : HID_CONSUMER));
        unsigned before=tv_ops[HID_KEY]+tv_ops[HID_CONSUMER];
        for(unsigned e=3;e<=5;++e) { runtime_adc_result(e<<16|keys[i]);tv_ticks(1); }
        assert(tv_ops[HID_KEY]+tv_ops[HID_CONSUMER]==before);
    }
    unsigned reports=tv_ops[HID_KEY]+tv_ops[HID_CONSUMER];
    tv_key(0);assert(!strcmp(app.result,"DEVICES: REMOTE"));
    tv_key(2);assert(!strcmp(app.result,"BACK: REMOTE"));
    tv_key(3);tv_key(4);assert(strstr(app.result,"01:01:01:01:01:01 PUBLIC"));
    /* All sixteen native bonds, including address type, remain selectable. */
    for(unsigned i=0;i<16;++i) { tv_key(3);tv_ticks(20); }
    assert(strstr(app.result,"BACK: DEVICES"));tv_key(3);tv_frame(1);
    tv_key(4);assert(strstr(app.result,"DISCONNECT:"));tv_key(3);tv_key(4);
    assert(strstr(app.result,"CANCEL: FORGET 01:01:01:01:01:01 PUBLIC?"));tv_frame(2);
    tv_key(1);tv_key(7);assert(strstr(app.result,"CANCEL:"));
    tv_key(4);assert(!tv_ops[HID_FORGET]); /* Default selection cancels. */
    tv_key(10);tv_key(3);tv_key(4);assert(strstr(app.result,"CONNECT: 02:02:02:02:02:02 RANDOM"));
    tv_key(4);tv_ticks(100);assert(tv_last==HID_LISTEN && fake_hid_status.address_type==1);
    assert(strstr(app.result,"WAITING FOR DEVICE: 02:02:02:02:02:02 RANDOM"));tv_frame(3);
    tv_key(4);fake_hid_status.state=2;tv_ticks(8);assert(tv_last==HID_CONSUMER && tv_value==0xcd);
    tv_ticks(50);assert(strstr(app.result,"CONNECTED: 02:02:02:02:02:02 RANDOM"));
    /* Opening a menu discards a deferred action. A late connection does too. */
    fake_hid_status.state=4;tv_ticks(1);tv_key(7);tv_key(0);
    unsigned sent=tv_ops[HID_KEY];fake_hid_status.state=2;tv_ticks(50);assert(tv_ops[HID_KEY]==sent);tv_key(0);
    fake_hid_status.state=4;tv_ticks(1);tv_key(7);tv_ticks(150);fake_hid_status.state=2;tv_ticks(50);
    assert(tv_ops[HID_KEY]==sent);
    tv_message("disconnect");tv_ticks(200);assert(strstr(app.result,"DISCONNECTED:"));tv_frame(4);
    unsigned listens=tv_ops[HID_LISTEN];tv_key(4);tv_ticks(200);assert(tv_ops[HID_LISTEN]==listens);
    tv_stop();load(source,1);tv_ticks(200);assert(strstr(app.result,"DISCONNECTED:") && tv_ops[HID_LISTEN]==listens);
    /* Pairing is separate from deletion. Expiry is persistent and explicit. */
    tv_key(0);tv_key(3);tv_key(4);tv_ticks(100);
    assert(strstr(app.result,"READY TO PAIR: SELECT DITOO BLE REMOTE"));tv_frame(5);
    assert(tv_ops[HID_PAIR] && !tv_ops[HID_FORGET]);
    fake_hid_status.state=1;memset(fake_hid_status.peer,20,6);tv_ticks(1);assert(strstr(app.result,"PAIRING:"));
    fake_hid_status.pairing=0;fake_hid_status.state=4;tv_ticks(200);
    assert(strstr(app.result,"PAIRING TIMED OUT"));tv_frame(6);
    tv_ticks(200);assert(strstr(app.result,"PAIRING TIMED OUT"));
    tv_message("pair");assert(fake_hid_status.pairing);tv_key(10);tv_ticks(100);
    assert(!fake_hid_status.pairing && !tv_ops[HID_FORGET] && strstr(app.result,"DISCONNECTED:"));
    tv_message("pair");fake_hid_status.state=2;fake_hid_status.pairing=0;
    memset(fake_hid_status.peer,3,6);fake_hid_status.address_type=0;tv_ticks(100);
    assert(strstr(app.result,"CONNECTED: 03:03:03:03:03:03 PUBLIC"));
    /* Forget selects exactly one typed bond and reconnects the other selection. */
    tv_key(0);tv_key(4);tv_key(4);tv_key(3);tv_key(4);tv_key(3);tv_key(4);tv_ticks(100);
    assert(tv_ops[HID_FORGET]==1 && !fake_le_bonds[0][7] && fake_le_bonds[2][7]);
    tv_key(0);tv_ticks(100);assert(strstr(app.result,"WAITING FOR DEVICE: 03:03:03:03:03:03 PUBLIC"));
    for(unsigned i=0;i<20;++i) { tv_key(0);tv_key(4);tv_ticks(10);tv_key(0); }
    printf("Android-style remote: source %u, Lua peak %u, reserved %u, native free %u\n",(unsigned)n,app.peak,app.reserved,stock_free_heap());
    assert(app.peak<=MEMORY_LIMIT && stock_free_heap()>=STOCK_HEAP_RESERVE);
    assert(tv_ops[HID_KEY]+tv_ops[HID_CONSUMER]==reports+1);
    /* Distinct maximal names must fit under the same Lua/native limits.
     * Enumeration keeps only identities; resolve the displayed row on demand. */
    tv_stop();char names[16][BT_NAME_BYTES+1];
    for(unsigned i=0;i<16;++i) {
        memset(names[i],'A'+i,BT_NAME_BYTES);names[i][BT_NAME_BYTES]=0;
        fake_le_names[i]=names[i];fake_le_bonds[i][7]=1;
    }
    load(source,1);tv_ticks(100);fake_hid_status.state=2;tv_ticks(20);
    assert(strstr(app.result,names[2]));
    tv_key(0);tv_key(4);
    for(unsigned i=0;i<16;++i) { assert(strstr(app.result,names[i]));tv_key(3);tv_ticks(20); }
    assert(strstr(app.result,"BACK: DEVICES"));tv_key(3);tv_key(4);tv_key(3);tv_key(4);
    assert(strstr(app.result,"CANCEL: FORGET ") && strstr(app.result,names[0]));
    tv_key(4);assert(tv_ops[HID_FORGET]==1);
    printf("Named remote: Lua peak %u, reserved %u, native free %u\n",app.peak,app.reserved,stock_free_heap());
    assert(app.peak<=MEMORY_LIMIT && stock_free_heap()>=STOCK_HEAP_RESERVE);
    memset(fake_le_names,0,sizeof fake_le_names);
    tv_stop();saved.settings_size=0;fake_hid_status.state=0;memset(fake_le_bonds,0,sizeof fake_le_bonds);
    load(source,1);tv_ticks(100);assert(!strcmp(app.result,"PAIR NEW DEVICE: REMOTE"));
    tv_key(4);tv_ticks(100);assert(fake_hid_status.pairing && tv_ops[HID_FORGET]==1);
    tv_stop();stock_config_context=NULL;saved.settings_size=0;track_heap=0;free_heap=100000;
    fake_hid_status=(struct hid_status){0};bt_queued.op=0;
    puts("Remote UX: typed devices, navigation, default-cancel forgetting, pairing without deletion, persistent disconnect/timeout, deferred inputs and 16-bond memory passed");
}

static void test_keyboard_lifecycle(void) {
    memset((void *)(stock_bt_manager+7),0,8*26);
    fake_hid_status=(struct hid_status){0};
    check("return pcall(keyboard.mode,'speaker')",DONE,"false");
    request_test("local t;return {init=function() t=assert(keyboard.mode('keyboard')) end,"
        "update=function() assert(device.result(t));print('ok') end}");
    unsigned mode_values[4];memcpy(mode_values,bt_payload,sizeof mode_values);
    assert(mode_values[2]==HID_MODE && mode_values[3]==1);
    request_test("local t;return {init=function() t=assert(keyboard.mode('combined')) end,"
        "update=function() assert(device.result(t));print('ok') end}");
    memcpy(mode_values,bt_payload,sizeof mode_values);assert(mode_values[2]==HID_MODE && !mode_values[3]);
    request_test("local t;return {init=function() t=assert(keyboard.mode('ble-remote')) end,"
        "update=function() assert(device.result(t));print('ok') end}");
    memcpy(mode_values,bt_payload,sizeof mode_values);assert(mode_values[2]==HID_MODE && mode_values[3]==2);
    fake_hid_status.transport=1;
    check("return keyboard.status().transport",DONE,"ble");
    check("return pcall(keyboard.tap,4)",DONE,"false");
    check("return pcall(keyboard.tap,44,1)",DONE,"false");
    check("return pcall(keyboard.listen,'08:08:08:08:08:08','wrong')",DONE,"false");
    peripheral.bt_attempts=32;
    request_test("local t;return {init=function() t=assert(keyboard.connect('08:08:08:08:08:08','random')) end,"
        "update=function() assert(device.result(t));print('ok') end}");
    assert(bt_payload[22]==1 && peripheral.bt_attempts==32);
    /* A media key immediately after connection must not inherit its address
     * as keyboard modifiers. No preceding keyboard.tap clears this buffer. */
    fake_hid_status.state=2;
    request_test("local t;return {init=function() t=assert(keyboard.media('play_pause')) end,"
        "update=function() assert(device.result(t));print('ok') end}");
    for(unsigned i=16;i<32;++i) assert(bt_payload[i]==0);
    fake_hid_status.state=0;
    fake_hid_status.transport=0;peripheral.bt_attempts=0;
    check("local t={peer='stale'};assert(keyboard.status(t)==t and not t.peer);return t.state",DONE,"0");
    for (unsigned i=0;i<8;++i) {
        for (unsigned j=0;j<6;++j) stock_bt_manager[7+i*26+j]=i+1;
        stock_bt_manager[7+i*26+25]=1;
    }
    check("local b=keyboard.bonds();assert(#b==8);return b[8]",DONE,"08:08:08:08:08:08");
    fake_hid_status.enabled=1;memset(fake_hid_status.peer,8,6);
    check("return keyboard.status().paired",DONE,"true");
    fake_hid_status.state=2;fake_hid_status.encryption_state=2;
    check("return keyboard.status().encrypted",DONE,"true");
    peripheral.bt_attempts=32;bt_queue_ok=1;bt_media_state=bt_audio_state=0;
    request_test("local t;return {init=function() t=assert(keyboard.connect('08:08:08:08:08:08')) end,"
        "update=function() assert(device.result(t));print('ok') end}");
    fake_hid_status.state=0;
    check("return pcall(keyboard.pair,'08:08:08:08:08:08',121)",DONE,"false");
    request_test("local t;return {init=function() t=assert(keyboard.pair('08:08:08:08:08:08',5)) end,"
        "update=function() assert(device.result(t));print('ok') end}");
    unsigned values[4];memcpy(values,bt_payload,sizeof values);
    assert(values[2]==HID_PAIR && values[3]==5000);
    peripheral.writes=0;
    request_test("local t;return {init=function() t=assert(keyboard.forget('08:08:08:08:08:08')) end,"
        "update=function() assert(device.result(t));print('ok') end}");
    memcpy(values,bt_payload,sizeof values);assert(values[2]==HID_FORGET);
    fake_hid_status.state=2;
    request_test("local t;return {init=function() t=assert(keyboard.forget('08:08:08:08:08:08')) end,"
        "update=function() local ok,e=device.result(t);assert(ok==false);print('ok') end}");
    fake_hid_status.state=0;fake_hid_status.forgotten=1;stock_bt_manager[0]=1;
    bond_count=0;unsigned before=saved_bonds;save_keyboard_bond();clock_ms+=1000;save_keyboard_bond();
    assert(saved_bonds==before+1 && !stock_bt_manager[0]);
    save_keyboard_bond();assert(saved_bonds==before+1);
    fake_hid_status=(struct hid_status){0};memset((void *)(stock_bt_manager+7),0,8*26);
    peripheral.bt_attempts=0;bt_queued.op=0;
    puts("Keyboard pairing tickets, bond listing, connection reuse, table reuse and zero-bond persistence passed");
}
