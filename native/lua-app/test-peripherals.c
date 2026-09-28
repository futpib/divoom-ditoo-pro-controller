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
    char source[8193];
    FILE *file=fopen("examples/lua/tv-remote.lua","rb");assert(file);
    size_t n=fread(source,1,sizeof(source)-1,file);assert(!ferror(file));fclose(file);source[n]=0;
    bt_media_state=2;bt_audio_state=0;
    memcpy((void *)(stock_bt_manager+0xe0),"\x26\x0c\xd0\xb4\xcd\x0c",6);
    load(source,1);
    if (app.state!=ACTIVE) { fprintf(stderr,"TV app failed: %s (peak %u)\n",app.result,app.peak);abort(); }
    tick();tick();
    memcpy(app.message,"connect 0C:CD:B4:D0:0C:26",25);app.message_size=25;
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
        unsigned lit=0;for (unsigned i=0;i<sizeof app.frame;++i) lit|=app.frame[i];
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
