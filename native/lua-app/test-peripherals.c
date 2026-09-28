static void request_test(const char *source) {
    load(source,1); assert(app.state==ACTIVE); runtime_native_service();
    tick();
    if (app.state!=ACTIVE || strcmp(app.result,"ok")) {
        fprintf(stderr,"native request: %s\nresult %s\n",source,app.result);abort();
    }
    app.cancel=1;service();runtime_native_service();
    assert(!allocations);
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
    puts("Native job cancellation, config readback, wear limits, alarm priority, indicator and microphone cleanup passed");
}
