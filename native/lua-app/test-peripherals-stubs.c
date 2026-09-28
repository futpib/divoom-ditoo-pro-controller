volatile unsigned stock_battery_level = 6;
volatile unsigned char stock_usb_power = 1, stock_charge_pin = 1, stock_alarm_state, stock_alarm_slot = 255;
static unsigned char noise_context[28], memo_context[24];
unsigned char *volatile stock_noise_context = noise_context, *volatile stock_memo_context;
static unsigned indicator_output, noise_enabled, memo_stops, config_writes, reschedules, preview_blocked;
static unsigned char saved_alarms[10][16], saved_wakes[144];
static unsigned config_missing, config_bad_length, config_fail_write;
void stock_indicator_write(unsigned n) { indicator_output = n; }
void stock_noise_enable(unsigned n) { noise_enabled = n; noise_context[3] = n; }
void stock_noise_display(unsigned n) { (void)n; }
void *stock_config_read(unsigned model,unsigned id,uint16_t *n) {
    assert((model==2 && id<10) || (model==17 && id==0));
    if (config_missing) return NULL;
    if (model==2 && !saved_alarms[id][9]) {
        unsigned char *p=stock_alloc(16);memset(p,0,16);p[5]=2;p[6]=1;p[9]=50;return p;
    }
    unsigned char *p=stock_alloc(256); memset(p,0,256);
    memcpy(p,model==2 ? saved_alarms[id] : saved_wakes,model==2 ? 16 : 144);
    *n=config_bad_length ? 8 : 256; return p;
}
void stock_config_write(unsigned model,unsigned id,const void *p,unsigned n) {
    assert(n==(model==2 ? 16 : 144)); ++config_writes;
    if (!config_fail_write) memcpy(model==2 ? saved_alarms[id] : saved_wakes,p,n);
}
void stock_alarm_reschedule(void) { ++reschedules; }
void stock_wake_reschedule(const void *p,unsigned n) { assert(p && n==0);++reschedules; }
unsigned stock_seconds(void) { return 1000+clock_ms/1000; }
void stock_alarm_cancel(unsigned n) { assert(n==1);stock_alarm_state=0; }
void stock_alarm_snooze(void) { assert(stock_alarm_state==2);stock_alarm_state=3; }
void stock_alarm_preview(unsigned mode,unsigned volume,unsigned enabled) {
    assert(mode<=13 && volume<=100);
    if (!preview_blocked) stock_alarm_state=enabled ? 4 : 0;
}
unsigned stock_play(unsigned n) { assert(n<=1);return n; }
void stock_track_direction(unsigned n) { assert(n<=1); }
void stock_track(unsigned n) { assert(n<65535); }
void stock_seek(unsigned n) { assert(n<=65535); }
void stock_repeat(unsigned n) { assert(n<=2); }
void stock_audio_status(void *p) { memset(p,0,18); }
void stock_memo_start(unsigned n) { assert(n==1);stock_memo_context=memo_context; }
void stock_memo_stop(void) { ++memo_stops;stock_memo_context=NULL; }
void stock_sound_stop(void) {}
unsigned stock_sound_playing(void) { return 0; }
unsigned stock_fs_free(void) { return 128*4096; }
void stock_fs_delete(unsigned model,unsigned id) { assert(model==11 && id==0); }

void stock_set_source(unsigned n) { assert(n==0 || n==3 || n==7); }
unsigned stock_get_source(void) { return 0; }
unsigned stock_sd_present(void) { return 0; }

void *stock_sd_queue(void) { return NULL; }
