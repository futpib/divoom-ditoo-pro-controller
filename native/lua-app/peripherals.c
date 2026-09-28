/* Included by runtime.c: Lua publishes values, never pointers into its arena.
 * Only the stock main-task hook enters configuration, recorder and audio code. */
extern volatile unsigned stock_battery_level;
extern volatile unsigned char stock_usb_power, stock_charge_pin, stock_alarm_state;
extern volatile unsigned char stock_alarm_slot;
extern unsigned char *volatile stock_noise_context, *volatile stock_memo_context;
extern void stock_indicator_write(unsigned);
extern void stock_noise_enable(unsigned);
extern void stock_noise_display(unsigned);
extern void *stock_config_read(unsigned, unsigned, uint16_t *);
extern void stock_config_write(unsigned, unsigned, const void *, unsigned);
extern void stock_alarm_reschedule(void);
extern void stock_wake_reschedule(const void *, unsigned);
extern unsigned stock_seconds(void);
extern void stock_alarm_cancel(unsigned);
extern void stock_alarm_snooze(void);
extern void stock_alarm_preview(unsigned, unsigned, unsigned);
extern unsigned stock_play(unsigned);
extern void stock_set_source(unsigned);
extern unsigned stock_get_source(void), stock_sd_present(void);
extern void stock_track_direction(unsigned);
extern void stock_track(unsigned);
extern void stock_seek(unsigned);
extern void stock_repeat(unsigned);
extern void *stock_sd_queue(void);
extern void stock_audio_status(void *);
extern void stock_memo_start(unsigned);
extern void stock_memo_stop(void);
extern void stock_sound_stop(void);
extern unsigned stock_sound_playing(void);
extern unsigned stock_fs_free(void);
extern void stock_fs_delete(unsigned, unsigned);

enum { JOB_EMPTY, JOB_QUEUED, JOB_RUNNING, JOB_DONE };
enum { ALARM_GET=1, ALARM_SET, WAKE_GET, WAKE_SET, ALARM_CANCEL,
       ALARM_SNOOZE, AUDIO_PLAY, AUDIO_DIRECTION, AUDIO_TRACK, AUDIO_SEEK,
       AUDIO_REPEAT, AUDIO_PREVIEW, AUDIO_STOP, NOISE_ENABLE,
       MEMO_START, MEMO_STOP, MEMO_PLAY, MEMO_DELETE, AUDIO_SOURCE };
static struct {
    volatile unsigned state, epoch, cleanup;
    unsigned ticket, sequence, job_epoch, op, slot, mask, value;
    unsigned char data[16];
    const char *error;
    unsigned last_job, had_job, last_write, writes, recordings;
    volatile unsigned indicator_owned, indicator_level, indicator_dirty, native_indicator;
    volatile unsigned noise_value, noise_time, noise_samples;
    unsigned noise_owned, noise_previous, recording, record_started, preview_owned;
    volatile unsigned recorded_bytes;
} peripheral;

static unsigned native_priority(void) {
    unsigned s = stock_alarm_state;
    return s == 1 || s == 2;
}

unsigned runtime_indicator_filter(unsigned level) {
    peripheral.native_indicator = level;
    return peripheral.indicator_owned ? peripheral.indicator_level : level;
}

void runtime_noise_sample(unsigned value) {
    peripheral.noise_value = value;
    peripheral.noise_time = stock_ticks();
    ++peripheral.noise_samples;
    stock_noise_display(value);
}

static void peripherals_release(void) {
    ++peripheral.epoch;
    peripheral.indicator_owned = 0;
    peripheral.indicator_dirty = 1;
    peripheral.cleanup = 1;
}

static void stop_recording(void) {
    if (peripheral.recording) {
        /* Native stop also closes the memo file; the hardware power key and
         * stock 60-second timeout continue to work independently. */
        if (stock_memo_context) stock_memo_stop();
        peripheral.recording = 0;
    }
}

static void release_noise(void) {
    if (peripheral.noise_owned) stock_noise_enable(peripheral.noise_previous);
    peripheral.noise_owned = 0;
}

static const char *config_job(void) {
    unsigned wake = peripheral.op == WAKE_GET || peripheral.op == WAKE_SET;
    unsigned write = peripheral.op == WAKE_SET || peripheral.op == ALARM_SET;
    unsigned model = wake ? 17 : 2, slot = wake ? 0 : peripheral.slot;
    unsigned bytes = wake ? 144 : 16;
    unsigned char data[144] = {0}, original[16];
    uint16_t allocated = 0;
    if (stock_free_heap() < 8192) return "insufficient native heap";
    unsigned char *p = stock_config_read(model,slot,&allocated);
    /* Alarm defaults are separately allocated 16-byte records and leave the
     * length argument zero. The wake model has no default allocation. */
    if (!p && !wake) return "alarm read failed";
    if (p) {
        if ((allocated && allocated < bytes) || (!allocated && wake)) {
            stock_free(p); return "invalid native config size";
        }
        memcpy(data,p,bytes); stock_free(p);
    }
    /* Old one-shot firmware wrote only 72 bytes. Ignore invalid tail records
     * from that format rather than passing garbage hours to the RTC helper. */
    if (wake) for (unsigned i=0;i<9;++i) {
        unsigned char *e = data+i*16;
        if (e[0]>1 || e[2]>1 || e[3]>23 || e[4]>59 || e[5]>127) memset(e,0,16);
    }
    unsigned char *entry = data + (wake ? peripheral.slot * 16 : 0);
    if (!write) { memcpy(peripheral.data,entry,16); return NULL; }
    memcpy(original,entry,16);
    for (unsigned i=0;i<10;++i)
        if (peripheral.mask & (1U<<i)) entry[i] = peripheral.data[i];
    if (!memcmp(original,entry,16)) { memcpy(peripheral.data,entry,16); return NULL; }
    unsigned now = stock_ticks();
    if (peripheral.writes >= 64) return "64 saved changes per boot exceeded";
    if (peripheral.writes && (unsigned)(now-peripheral.last_write)<1000)
        return "saved settings rate limited";
    if (!wake) { unsigned seconds = stock_seconds(); memcpy(entry+12,&seconds,4); }
    ++peripheral.writes; peripheral.last_write = now;
    stock_config_write(model,slot,data,bytes);
    allocated = 0; p = stock_config_read(model,slot,&allocated);
    unsigned ok = p && allocated >= bytes && !memcmp(p,data,bytes);
    stock_free(p);
    if (!ok) return "config readback mismatch";
    if (wake) stock_wake_reschedule(data,0);
    else stock_alarm_reschedule();
    memcpy(peripheral.data,entry,16);
    return NULL;
}

static const char *perform_job(void) {
    unsigned op = peripheral.op, n = peripheral.value;
    if (op <= WAKE_SET) return config_job();
    if (op == ALARM_CANCEL) { if (stock_alarm_state) stock_alarm_cancel(1); return NULL; }
    if (op == ALARM_SNOOZE) {
        if (stock_alarm_state != 2) return "no ringing alarm";
        stock_alarm_snooze(); return NULL;
    }
    if (native_priority()) return "native alarm has priority";
    if ((op == AUDIO_TRACK || op == AUDIO_SEEK || op == AUDIO_REPEAT) &&
            (stock_get_source() != 3 || !stock_sd_present() || !stock_sd_queue()))
        return "SD playback unavailable";
    switch (op) {
    case AUDIO_SOURCE:
        if (peripheral.recording) return "recording is active";
        if (n == 3 && !stock_sd_present()) return "no SD card";
        if (n == 7 && !stock_usb_power) return "USB disconnected";
        if (stock_free_heap() < 16384) return "insufficient native heap";
        stock_set_source(n); break;
    case AUDIO_PLAY: stock_play(n); break;
    case AUDIO_DIRECTION: stock_track_direction(n); break;
    case AUDIO_TRACK: stock_track(n); break;
    case AUDIO_SEEK: stock_seek(n); break;
    case AUDIO_REPEAT: stock_repeat(n); break;
    case AUDIO_PREVIEW:
        if (peripheral.recording) return "recording is active";
        if (stock_free_heap() < 16384) return "insufficient native heap";
        stock_alarm_preview(peripheral.slot,n,1);
        if (stock_alarm_state != 4) return "native mode blocks preview";
        peripheral.preview_owned = 1; break;
    case AUDIO_STOP:
        if (peripheral.preview_owned && stock_alarm_state == 4) stock_alarm_preview(0,0,0);
        stock_sound_stop(); stock_play(0); peripheral.preview_owned = 0; break;
    case NOISE_ENABLE:
        if (!stock_noise_context) return "noise monitor unavailable";
        if (n) {
            if (peripheral.recording) return "recording is active";
            if (!peripheral.noise_owned) {
                peripheral.noise_previous = stock_noise_context[3];
                peripheral.noise_owned = 1;
                peripheral.noise_samples = 0;
            }
            stock_noise_enable(1);
        } else release_noise();
        break;
    case MEMO_START:
        if (stock_memo_context || peripheral.recording) return "memo is busy";
        if (peripheral.recordings >= 32) return "32 recordings per boot exceeded";
        if (stock_free_heap() < 53248) return "recording needs 52 KiB native heap";
        if (stock_fs_free() < 65536) return "insufficient memo storage";
        release_noise(); ++peripheral.recordings;
        stock_memo_start(1);
        if (!stock_memo_context) return "native recorder failed";
        peripheral.recorded_bytes = 0;
        peripheral.recording = 1; peripheral.record_started = stock_ticks();
        break;
    case MEMO_STOP: stop_recording(); break;
    case MEMO_PLAY: {
        if (stock_memo_context || peripheral.recording) return "memo is busy";
        if (stock_free_heap() < 16384) return "insufficient native heap";
        struct { uint16_t a; uint8_t group,b,command,c; uint16_t d; void *data; }
            event = {0,10,0,0,0,0,NULL};
        stock_event(&event); peripheral.preview_owned = 1; break;
    }
    case MEMO_DELETE:
        if (stock_sound_playing()) return "sound playback is active";
        if (stock_memo_context || peripheral.recording) return "memo is busy";
        if (peripheral.writes >= 64) return "64 saved changes per boot exceeded";
        if (peripheral.writes && (unsigned)(stock_ticks()-peripheral.last_write)<1000)
            return "saved settings rate limited";
        ++peripheral.writes; peripheral.last_write = stock_ticks();
        stock_fs_delete(11,0); break;
    default: return "invalid native operation";
    }
    return NULL;
}

void runtime_native_service(void) {
    if (peripheral.recording && stock_memo_context) {
        unsigned count; memcpy(&count,stock_memo_context+12,4);
        peripheral.recorded_bytes = count;
    }
    if (peripheral.cleanup) {
        peripheral.cleanup = 0;
        stop_recording(); release_noise();
        if (peripheral.preview_owned) {
            if (stock_alarm_state == 4) stock_alarm_preview(0,0,0);
            if (!native_priority()) stock_sound_stop();
            peripheral.preview_owned = 0;
        }
    }
    if (peripheral.indicator_dirty) {
        peripheral.indicator_dirty = 0;
        stock_indicator_write(peripheral.indicator_owned ? peripheral.indicator_level : peripheral.native_indicator);
    }
    if (peripheral.recording && (!stock_memo_context || native_priority() ||
            (unsigned)(stock_ticks()-peripheral.record_started) >= 60000)) stop_recording();
    if (peripheral.state != JOB_QUEUED) return;
    if (peripheral.job_epoch == peripheral.epoch && peripheral.had_job &&
            (unsigned)(stock_ticks()-peripheral.last_job)<100) return;
    peripheral.had_job = 1; peripheral.last_job = stock_ticks();
    peripheral.state = JOB_RUNNING;
    if (peripheral.job_epoch != peripheral.epoch) peripheral.error = "app released operation";
    else peripheral.error = perform_job();
    __asm__ volatile ("" ::: "memory");
    peripheral.state = JOB_DONE;
}

static void field(lua_State *L, const char *name, unsigned value) {
    lua_pushinteger(L,value); lua_setfield(L,-2,name);
}
static void flag(lua_State *L, const char *name, unsigned value) {
    lua_pushboolean(L,value); lua_setfield(L,-2,name);
}
static int failure(lua_State *L, const char *why) {
    lua_pushnil(L); lua_pushstring(L,why); return 2;
}
static int submit(lua_State *L, unsigned op, unsigned slot, unsigned value,
                  const unsigned char *data, unsigned mask) {
    native_budget();
    if (!app.resident) return failure(L,"native requests require a resident app");
    if (peripheral.cleanup || peripheral.state == JOB_QUEUED || peripheral.state == JOB_RUNNING)
        return failure(L,"busy");
    peripheral.op = op; peripheral.slot = slot; peripheral.value = value; peripheral.mask = mask;
    if (data) memcpy(peripheral.data,data,16);
    peripheral.job_epoch = peripheral.epoch;
    peripheral.ticket = (++peripheral.sequence & 0x7fffffff) + 1;
    __asm__ volatile ("" ::: "memory");
    peripheral.state = JOB_QUEUED;
    lua_pushinteger(L,peripheral.ticket); return 1;
}
static void config_table(lua_State *L, unsigned wake) {
    const unsigned char *d = peripheral.data;
    lua_createtable(L,0,9); field(L,"slot",peripheral.slot); flag(L,"enabled",d[0]);
    field(L,"hour",d[wake ? 3 : 2]); field(L,"min",d[wake ? 4 : 3]);
    field(L,"days",d[wake ? 5 : 4]);
    if (wake) {
        lua_pushstring(L,d[2] ? "on" : "off"); lua_setfield(L,-2,"action");
        field(L,"color",(d[6]<<16)|(d[7]<<8)|d[8]);
    } else {
        field(L,"mode",d[5]); field(L,"trigger",d[6]); field(L,"volume",d[9]);
    }
}
static int operation_result(lua_State *L) {
    unsigned ticket = integer(L,1,1,0x7fffffff);
    if (ticket != peripheral.ticket || peripheral.state == JOB_EMPTY) return failure(L,"expired ticket");
    if (peripheral.state != JOB_DONE) { lua_pushnil(L); return 1; }
    if (peripheral.error) { lua_pushboolean(L,0); lua_pushstring(L,peripheral.error); return 2; }
    lua_pushboolean(L,1);
    if (peripheral.op <= WAKE_SET) {
        config_table(L,peripheral.op >= WAKE_GET); return 2;
    }
    return 1;
}
static int battery(lua_State *L) {
    unsigned level = stock_battery_level, usb = stock_usb_power != 0, pin = stock_charge_pin != 0;
    lua_createtable(L,0,5); field(L,"level",level); field(L,"max_level",7);
    flag(L,"external_power",usb); flag(L,"charging",usb && pin); flag(L,"full",usb && level==7);
    return 1;
}
static int indicator(lua_State *L) {
    native_budget();
    if (!app.resident) return failure(L,"indicator requires a resident app");
    if (lua_isnoneornil(L,1)) peripheral.indicator_owned = 0;
    else { peripheral.indicator_level = integer(L,1,0,5); peripheral.indicator_owned = 1; }
    peripheral.indicator_dirty = 1; lua_pushboolean(L,1); return 1;
}
static int alarm_status(lua_State *L) {
    lua_createtable(L,0,3); field(L,"state",stock_alarm_state);
    field(L,"slot",stock_alarm_slot); flag(L,"ringing",stock_alarm_state==2); return 1;
}
static int get_alarm(lua_State *L) { return submit(L,ALARM_GET,integer(L,1,0,9),0,NULL,0); }
static int get_wake(lua_State *L) { return submit(L,WAKE_GET,integer(L,1,0,8),0,NULL,0); }
static unsigned option(lua_State *L, const char *name, unsigned max, unsigned *value) {
    lua_pushstring(L,name); lua_rawget(L,2);
    if (lua_isnil(L,-1)) { lua_pop(L,1); return 0; }
    *value = integer(L,-1,0,max); lua_pop(L,1); return 1;
}
static int set_config(lua_State *L, unsigned wake) {
    unsigned slot = integer(L,1,0,wake ? 8 : 9), mask = 0, value;
    unsigned char d[16] = {0}; luaL_checktype(L,2,LUA_TTABLE);
    lua_pushliteral(L,"enabled"); lua_rawget(L,2);
    if (!lua_isnil(L,-1)) {
        luaL_checktype(L,-1,LUA_TBOOLEAN); d[0] = lua_toboolean(L,-1); mask |= 1;
    }
    lua_pop(L,1);
    const char *names[] = {"hour","min","days","mode","trigger","volume"};
    unsigned maxima[] = {23,59,127,13,5,100}, offsets[] = {2,3,4,5,6,9};
    for (unsigned i=0;i<(wake ? 3 : 6);++i) if (option(L,names[i],maxima[i],&value)) {
        unsigned offset = offsets[i]+wake; d[offset] = value; mask |= 1U<<offset;
    }
    if (wake) {
        lua_pushliteral(L,"action"); lua_rawget(L,2);
        if (!lua_isnil(L,-1)) {
            const char *actions[] = {"off","on",NULL};
            d[2] = luaL_checkoption(L,-1,NULL,actions); mask |= 1U<<2;
        }
        lua_pop(L,1);
        if (option(L,"color",0xffffff,&value)) {
            d[6]=value>>16; d[7]=value>>8; d[8]=value; mask |= 7U<<6;
        }
    }
    return submit(L,wake ? WAKE_SET : ALARM_SET,slot,0,d,mask);
}
static int set_alarm(lua_State *L) { return set_config(L,0); }
static int set_wake(lua_State *L) { return set_config(L,1); }
static int cancel_alarm(lua_State *L) { return submit(L,ALARM_CANCEL,0,0,NULL,0); }
static int snooze_alarm(lua_State *L) { return submit(L,ALARM_SNOOZE,0,0,NULL,0); }
static int audio_play(lua_State *L) { return submit(L,AUDIO_PLAY,0,1,NULL,0); }
static int audio_pause(lua_State *L) { return submit(L,AUDIO_PLAY,0,0,NULL,0); }
static int audio_source(lua_State *L) {
    const char *names[] = {"bluetooth","sd","usb",NULL};
    const unsigned ids[] = {0,3,7};
    return submit(L,AUDIO_SOURCE,0,ids[luaL_checkoption(L,1,NULL,names)],NULL,0);
}
static int audio_next(lua_State *L) { return submit(L,AUDIO_DIRECTION,0,1,NULL,0); }
static int audio_previous(lua_State *L) { return submit(L,AUDIO_DIRECTION,0,0,NULL,0); }
static int audio_direction(lua_State *L) { return submit(L,AUDIO_DIRECTION,0,integer(L,1,0,1),NULL,0); }
static int audio_track(lua_State *L) { return submit(L,AUDIO_TRACK,0,integer(L,1,0,65534),NULL,0); }
static int audio_seek(lua_State *L) { return submit(L,AUDIO_SEEK,0,integer(L,1,0,65535),NULL,0); }
static int audio_repeat(lua_State *L) {
    const char *names[] = {"all","one","shuffle",NULL};
    unsigned n = lua_type(L,1) == LUA_TSTRING ? (unsigned)luaL_checkoption(L,1,NULL,names) : (unsigned)integer(L,1,0,2);
    return submit(L,AUDIO_REPEAT,0,n,NULL,0);
}
static int audio_preview(lua_State *L) {
    return submit(L,AUDIO_PREVIEW,integer(L,1,0,13),integer(L,2,0,100),NULL,0);
}
static int audio_stop(lua_State *L) { return submit(L,AUDIO_STOP,0,0,NULL,0); }
static int noise_enable(lua_State *L) {
    luaL_checktype(L,1,LUA_TBOOLEAN); return submit(L,NOISE_ENABLE,0,lua_toboolean(L,1),NULL,0);
}
static int noise_level(lua_State *L) {
    if (!peripheral.noise_samples) return failure(L,"no sample yet");
    lua_pushinteger(L,peripheral.noise_value);
    lua_pushinteger(L,(stock_ticks()-peripheral.noise_time)&0x7fffffff); return 2;
}
static int record_start(lua_State *L) {
    /* Drop garbage before checking recorder headroom. Finalizers still run
     * inside the worker guard, so this cannot evade the instruction budget. */
    native_budget(); lua_gc(L,LUA_GCCOLLECT);
    return submit(L,MEMO_START,0,0,NULL,0);
}
static int record_stop(lua_State *L) { return submit(L,MEMO_STOP,0,0,NULL,0); }
static int record_play(lua_State *L) { return submit(L,MEMO_PLAY,0,0,NULL,0); }
static int record_delete(lua_State *L) { return submit(L,MEMO_DELETE,0,0,NULL,0); }
static int audio_status(lua_State *L) {
    unsigned char d[18]; stock_audio_status(d);
    lua_createtable(L,0,8);
    field(L,"position",d[0]|(d[1]<<8)); field(L,"duration",d[2]|(d[3]<<8));
    field(L,"track",d[4]|(d[5]<<8)); field(L,"tracks",d[6]|(d[7]<<8));
    field(L,"source",stock_get_source()); flag(L,"sd_present",stock_sd_present());
    flag(L,"playing",d[12]); field(L,"volume",d[13]); field(L,"repeat_mode",d[15]);
    field(L,"recorded_bytes",peripheral.recorded_bytes); flag(L,"recording",peripheral.recording); flag(L,"sound_playing",stock_sound_playing());
    return 1;
}
static void peripherals_modules(lua_State *L) {
    static const luaL_Reg power[] = {{"battery",battery},{"indicator",indicator},
        {"get_schedule",get_wake},{"set_schedule",set_wake},{NULL,NULL}};
    static const luaL_Reg alarms[] = {{"get",get_alarm},{"set",set_alarm},{"status",alarm_status},
        {"cancel",cancel_alarm},{"snooze",snooze_alarm},{NULL,NULL}};
    static const luaL_Reg audio[] = {{"play",audio_play},{"pause",audio_pause},{"source",audio_source},{"next",audio_next},{"previous",audio_previous},{"direction",audio_direction},
        {"track",audio_track},{"seek",audio_seek},{"repeat_mode",audio_repeat},{"preview",audio_preview},
        {"stop",audio_stop},{"status",audio_status},{"memo_play",record_play},{"memo_delete",record_delete},{NULL,NULL}};
    static const luaL_Reg mic[] = {{"noise",noise_enable},{"level",noise_level},
        {"record",record_start},{"stop",record_stop},{NULL,NULL}};
    module(L,"power",power); module(L,"alarm",alarms); module(L,"audio",audio); module(L,"microphone",mic);
    lua_getglobal(L,"device"); lua_pushcfunction(L,operation_result); lua_setfield(L,-2,"result"); lua_pop(L,1);
}
