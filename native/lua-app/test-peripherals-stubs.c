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
    if (model==PERSIST_MODEL) {
        assert(id<4);if(!persisted_size[id]) return NULL;
        *n=(persisted_size[id]+4+255)&~255U;unsigned char *p=stock_alloc(*n);
        if(p) { memset(p,0,*n);memcpy(p,persisted[id],persisted_size[id]); }return p;
    }
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
    if(model==PERSIST_MODEL) {
        assert(id<4 && n==24+(id<2 ? SOURCE_LIMIT : SETTINGS_LIMIT));++persist_writes;
        persisted_size[id]=n;memcpy(persisted[id],p,n);
        if(persist_torn) persisted[id][20]^=1;
        return;
    }
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

static unsigned bt_media_state,bt_audio_state,bt_connects,bt_commands,bt_queue_ok=1,bt_last_action;
static unsigned char bt_context[256],bt_channel[0x1038],bt_payload[32];
void *volatile stock_bt_context=bt_context;
static struct bt_command bt_queued;
static unsigned bt_panel_calls,bt_panel_ok=1,bt_pressed[2];
void stock_bt_peek(struct bt_command *p) { *p=bt_queued; }
unsigned stock_bt_enqueue(unsigned op,const void *p,unsigned n) {
    assert((op==BT_MUTE_COMMAND && n==10) || (op==BT_HID_COMMAND && n==32));
    if (!bt_queue_ok) return 0;
    memcpy(bt_payload,p,n);bt_queued=(struct bt_command){op,n,0,bt_payload};return 1;
}
unsigned stock_avrcp_panel(void *p,unsigned key,unsigned pressed) {
    assert(p==bt_channel && key==0x43 && pressed<=1);
    bt_pressed[bt_panel_calls++%2]=pressed;return bt_panel_ok ? 2 : 12;
}
volatile unsigned char stock_bt_manager[0x1c4],stock_ble_connected=1;
static unsigned char bt_address[6];
unsigned stock_avrcp_state(void) { return bt_media_state; }
unsigned stock_a2dp_state(void) { return bt_audio_state; }
unsigned stock_avrcp_connect(const unsigned char *p) { ++bt_connects;memcpy(bt_address,p,6);return bt_queue_ok; }
unsigned stock_avrcp_disconnect(void) { ++bt_commands;return bt_queue_ok; }
unsigned stock_avrcp_play(void) { ++bt_commands;bt_last_action=1;return bt_queue_ok; }
unsigned stock_avrcp_pause(void) { ++bt_commands;bt_last_action=2;return bt_queue_ok; }

void runtime_hid_service(unsigned epoch) { (void)epoch; }
void runtime_hid_command(unsigned op,unsigned value,const unsigned char *data,unsigned epoch,unsigned generation) {
    (void)op;(void)value;(void)data;(void)epoch;(void)generation;
}
static struct hid_status fake_hid_status;
static unsigned saved_bonds,bond_count=1;
void runtime_hid_status(struct hid_status *s) { *s=fake_hid_status; }
unsigned stock_bt_record_count(void) { return bond_count; }
void stock_bt_save_records(unsigned size,unsigned count) { assert(size==0x1f9 && count==bond_count);++saved_bonds; }
