#include "bluetooth-hogp.h"
#include "bluetooth-advertising.h"
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
extern void *volatile stock_bt_context;
extern volatile unsigned char stock_bt_manager[], stock_ble_connected;
extern unsigned stock_avrcp_state(void), stock_a2dp_state(void);
extern unsigned stock_bt_record_count(void);
extern void stock_bt_save_records(unsigned, unsigned);
extern unsigned stock_avrcp_connect(const unsigned char *);
extern unsigned stock_avrcp_disconnect(void), stock_avrcp_play(void), stock_avrcp_pause(void);
struct bt_command { unsigned op; uint16_t length, reserved; unsigned char *data; };
extern void stock_bt_peek(struct bt_command *);
extern unsigned stock_bt_enqueue(unsigned, const void *, unsigned);
extern unsigned stock_avrcp_panel(void *, unsigned, unsigned);
#include "bluetooth-hid.h"
enum { BT_MUTE_COMMAND = 0x80, BT_HID_COMMAND = 0x81, BT_ADVERTISE_COMMAND = 0x82 };

enum { JOB_EMPTY, JOB_QUEUED, JOB_RUNNING, JOB_DONE };
enum { ALARM_GET=1, ALARM_SET, WAKE_GET, WAKE_SET, ALARM_CANCEL,
       ALARM_SNOOZE, AUDIO_PLAY, AUDIO_DIRECTION, AUDIO_TRACK, AUDIO_SEEK,
       AUDIO_REPEAT, AUDIO_PREVIEW, AUDIO_STOP, NOISE_ENABLE,
       MEMO_START, MEMO_STOP, MEMO_PLAY, MEMO_DELETE, AUDIO_SOURCE,
       BT_CONNECT, BT_DISCONNECT, BT_MEDIA, BT_MUTE, BT_HID, SETTINGS_SAVE, BT_ADVERTISE };
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
    unsigned bt_attempts, bt_last_attempt, bt_bond_since, bt_bond_pending, bt_bonds_saved, bt_forget_saved;
    struct hid_profile *profile;
    unsigned profile_ticket,profile_at;
    struct ble_advertisement *advertisement;
    unsigned advertisement_ticket,advertisement_at;
    volatile unsigned advertisement_cancel;
} peripheral;

static struct ble_advertisement *take_advertisement(unsigned ticket) {
    unsigned irq=runtime_irq_save();struct ble_advertisement *p=NULL;
    if(peripheral.advertisement_ticket==ticket) { p=peripheral.advertisement;peripheral.advertisement=NULL; }
    runtime_irq_restore(irq);return p;
}
void runtime_advertising_complete(unsigned ticket,const char *error) {
    if(peripheral.ticket!=ticket || peripheral.op!=BT_ADVERTISE) return;
    peripheral.error=error;
    __asm__ volatile ("" ::: "memory");peripheral.state=JOB_DONE;
}

static struct hid_profile *take_profile(unsigned ticket) {
    unsigned irq=runtime_irq_save();struct hid_profile *p=NULL;
    if(peripheral.profile_ticket==ticket) { p=peripheral.profile;peripheral.profile=NULL; }
    runtime_irq_restore(irq);return p;
}

/* Called only on the stock Bluetooth task. Unknown commands still pass through
 * its normal dispatcher/pop path; no Lua pointer enters this queue. */
void runtime_bt_peek(struct bt_command *command) {
    runtime_advertising_service(peripheral.epoch,peripheral.advertisement_cancel);
    runtime_hid_service(peripheral.epoch);
    stock_bt_peek(command);
    if(command->op==BT_ADVERTISE_COMMAND && command->length==8 && command->data) {
        unsigned values[2];memcpy(values,command->data,sizeof values);
        struct ble_advertisement *p=take_advertisement(values[1]);
        if(p) {
            const char *error=values[0]!=peripheral.epoch || values[1]==peripheral.advertisement_cancel ? "cancelled" :
                runtime_advertising_start(p,values[1],values[0]);
            stock_free(p);if(error) runtime_advertising_complete(values[1],error);
        }
        return;
    }
    if (command->op==BT_HID_COMMAND && command->length==32 && command->data) {
        unsigned values[4]; memcpy(values,command->data,16);
        if(values[2]==HID_CONFIGURE) {
            struct hid_profile *p=take_profile(values[3]);
            if(p) {
                unsigned ok=values[0]==peripheral.epoch && runtime_hogp_configure(p);
                stock_free(p);
                if(peripheral.ticket==values[3]) {
                    peripheral.error=ok ? NULL : "profile change failed; disconnect first";
                    __asm__ volatile ("" ::: "memory");peripheral.state=JOB_DONE;
                }
            }
            return;
        }
        if(values[2]==HID_MODE && preferences.bt_mode && !(preferences.bt_mode==1 && values[3]==2))
            values[3]=preferences.bt_mode==1;
        if (values[0]==peripheral.epoch)
            runtime_hid_command(values[2],values[3],command->data+16,values[0],values[1]);
        return;
    }
    if (command->op != BT_MUTE_COMMAND || command->length != 10 || !command->data) return;
    unsigned epoch;
    memcpy(&epoch,command->data,4);
    unsigned char *context=stock_bt_context, *channel;
    if (epoch != peripheral.epoch || !context || !context[0xa6] || stock_avrcp_state()!=2) return;
    for (unsigned i=0;i<6;++i)
        if (command->data[4+i] != stock_bt_manager[0xe0+i]) return;
    memcpy(&channel,context+0x9c,sizeof channel);
    if (!channel) return;
    unsigned read=channel[0x2ba],write=channel[0x2bb];
    /* Reserve a previous-key release plus this press/release, on the same task.
     * Never enqueue a press if its release could overflow the 14-entry ring. */
    if (read>=15 || write>=15 || (write+15-read)%15>11) return;
    if (stock_avrcp_panel(channel,0x43,1)==2) stock_avrcp_panel(channel,0x43,0);
}

static unsigned native_priority(void) {
    unsigned s = stock_alarm_state;
    return s == 1 || s == 2;
}

unsigned runtime_indicator_filter(unsigned level) {
    peripheral.native_indicator = level;
    return preferences.indicator_off ? 0 : peripheral.indicator_owned ? peripheral.indicator_level : level;
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
    if (peripheral.op==SETTINGS_SAVE) {
        if (native_priority()) return "native alarm has priority";
        if (!saved.initialized) return "storage not ready";
        if (saved.settings_size==saved.pending_size && !memcmp(saved.settings,saved.pending,saved.pending_size)) return NULL;
        if (peripheral.writes>=64) return "64 saved changes per boot exceeded";
        if (peripheral.writes && (unsigned)(stock_ticks()-peripheral.last_write)<1000) return "saved settings rate limited";
        unsigned changed=0;const char *error=persist_save(2,saved.pending,saved.pending_size,&changed);
        if (changed) { ++peripheral.writes;peripheral.last_write=stock_ticks(); }
        if (!error) { saved.settings_size=saved.pending_size;memcpy(saved.settings,saved.pending,saved.pending_size); }
        return error;
    }
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
    case BT_ADVERTISE: {
        if(!stock_bt_context) return "Bluetooth unavailable";
        if(peripheral.advertisement_cancel==peripheral.ticket) return "cancelled";
        unsigned data[2]={peripheral.epoch,peripheral.ticket};
        return stock_bt_enqueue(BT_ADVERTISE_COMMAND,data,sizeof data) ? NULL : "Bluetooth queue full";
    }
    case BT_HID: {
        if (!stock_bt_context) return "Bluetooth unavailable";
        struct hid_status status; runtime_hid_status(&status);
        if(peripheral.slot==HID_CONFIGURE) {
            if(!status.transport) return "BLE profile required";
            if(status.state && status.state!=4 && !runtime_hogp_profile_equal(peripheral.profile))
                return "disconnect before changing profile";
            unsigned data[8]={peripheral.epoch,status.generation,HID_CONFIGURE,peripheral.ticket};
            if(!stock_bt_enqueue(BT_HID_COMMAND,data,sizeof data)) return "Bluetooth queue full";
            return NULL;
        }
        if (peripheral.slot==HID_CONNECT || peripheral.slot==HID_LISTEN || peripheral.slot==HID_PAIR) {
            if ((status.state==1 || status.state==2) && !memcmp(status.peer,peripheral.data,6) &&
                    (!status.transport || status.address_type==peripheral.data[6])) return NULL;
            if (status.state && (status.state!=4 || memcmp(status.peer,peripheral.data,6)))
                return "disconnect the previous keyboard peer first";
            if (stock_avrcp_state() || stock_a2dp_state())
                for (unsigned i=0;i<6;++i)
                    if (stock_bt_manager[0xe0+i]!=peripheral.data[i])
                        return "disconnect the previous Bluetooth peer first";
            if (stock_free_heap()<16384) return "insufficient native heap";
            unsigned now=stock_ticks();
            if (peripheral.slot==HID_CONNECT && !status.transport) {
                if (peripheral.bt_attempts>=32) return "32 connection attempts per boot exceeded";
                if (peripheral.bt_attempts && (unsigned)(now-peripheral.bt_last_attempt)<10000)
                    return "Bluetooth connection rate limited";
                ++peripheral.bt_attempts;peripheral.bt_last_attempt=now;
            }
        } else if (peripheral.slot==HID_FORGET) {
            if (status.state || stock_avrcp_state() || stock_a2dp_state()) return "disconnect Bluetooth profiles before forgetting a bond";
            if (peripheral.writes>=64) return "64 saved changes per boot exceeded";
            if (peripheral.writes && (unsigned)(stock_ticks()-peripheral.last_write)<1000)
                return "saved settings rate limited";
        } else if (peripheral.slot!=HID_DISCONNECT && peripheral.slot!=HID_MODE && (status.state!=2 || status.busy))
            return "keyboard disconnected or busy";
        unsigned char data[32];
        unsigned values[4]={peripheral.epoch,status.generation,peripheral.slot,n};
        memcpy(data,values,16);memcpy(data+16,peripheral.data,16);
        if (!stock_bt_enqueue(BT_HID_COMMAND,data,sizeof data)) return "Bluetooth queue full";
        break;
    }
    case BT_CONNECT: {
        if (!stock_bt_context) return "Bluetooth unavailable";
        unsigned state = stock_avrcp_state(), audio = stock_a2dp_state();
        if (state || audio) {
            for (unsigned i=0;i<6;++i)
                if (stock_bt_manager[0xe0+i] != peripheral.data[i])
                    return "another Bluetooth peer is active";
            if (state == 2) return NULL;
            if (state) return "Bluetooth connection pending";
        }
        unsigned now = stock_ticks();
        if (peripheral.bt_attempts >= 32) return "32 connection attempts per boot exceeded";
        if (peripheral.bt_attempts && (unsigned)(now-peripheral.bt_last_attempt)<10000)
            return "Bluetooth connection rate limited";
        if (stock_free_heap() < 16384) return "insufficient native heap";
        ++peripheral.bt_attempts; peripheral.bt_last_attempt = now;
        /* The stock command queue copies the six address bytes before return. */
        if (!stock_avrcp_connect(peripheral.data)) return "Bluetooth queue full";
        break;
    }
    case BT_DISCONNECT:
        if (!stock_bt_context) return "Bluetooth unavailable";
        if (!stock_avrcp_disconnect()) return "Bluetooth queue full";
        break;
    case BT_MEDIA:
        if (!stock_bt_context || stock_avrcp_state()!=2) return "media peer not connected";
        if (!(n ? stock_avrcp_play() : stock_avrcp_pause())) return "Bluetooth queue full";
        break;
    case BT_MUTE: {
        if (!stock_bt_context || stock_avrcp_state()!=2) return "media peer not connected";
        unsigned char data[10];
        unsigned epoch=peripheral.epoch;
        memcpy(data,&epoch,4);
        for (unsigned i=0;i<6;++i) data[4+i]=stock_bt_manager[0xe0+i];
        if (!stock_bt_enqueue(BT_MUTE_COMMAND,data,sizeof data)) return "Bluetooth queue full";
        break;
    }
    case AUDIO_SOURCE:
        if(n==7 && preferences.usb_noaudio) return "USB audio disabled in Settings";
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

/* Stock persists bonds only with both AVRCP and A2DP connected. HID-only peers
 * need the same dirty-record flush; never rewrite an unchanged bond. */
static void save_keyboard_bond(void) {
    struct hid_status s;runtime_hid_status(&s);
    if (s.transport) {
        if (s.forgotten!=peripheral.bt_forget_saved) {
            ++peripheral.writes;peripheral.last_write=stock_ticks();
            peripheral.bt_forget_saved=s.forgotten;
        }
        return; /* Native LE TLV already persists bonds on the Bluetooth task. */
    }
    unsigned forgotten=s.forgotten!=peripheral.bt_forget_saved;
    if ((!forgotten && s.state!=2) || !stock_bt_context || !stock_bt_manager[0] || stock_bt_manager[0x125]) {
        peripheral.bt_bond_pending=0;return;
    }
    unsigned now=stock_ticks();
    if (!peripheral.bt_bond_pending) {
        peripheral.bt_bond_pending=1;peripheral.bt_bond_since=now;return;
    }
    if ((!forgotten && (unsigned)(now-peripheral.bt_bond_since)<2000) || peripheral.writes>=64 ||
            (peripheral.writes && (unsigned)(now-peripheral.last_write)<1000)) return;
    unsigned count=stock_bt_record_count();
    if ((!forgotten && !count) || count>8) return;
    stock_bt_manager[0]=0;
    ++peripheral.writes;peripheral.last_write=now;
    stock_bt_save_records(0x1f9,count);++peripheral.bt_bonds_saved;
    peripheral.bt_forget_saved=s.forgotten;
    peripheral.bt_bond_pending=0;
}

void runtime_native_service(void) {
    worker_reap();
    runtime_bt_trace_service();
    unsigned irq=runtime_irq_save();struct source *expired=NULL;
    if(app.state==UPLOADING && (unsigned)(stock_ticks()-app.upload_seen)>=30000) {
        expired=app.source;app.source=NULL;app.state=IDLE;result("upload expired");
    }
    runtime_irq_restore(irq);source_free(expired);
    runtime_boot_service();
    save_keyboard_bond();
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
    irq=runtime_irq_save();struct hid_profile *expired_profile=NULL;
    if(peripheral.profile && (peripheral.job_epoch!=peripheral.epoch || !stock_bt_context ||
            stock_ticks()-peripheral.profile_at>=1000)) {
        expired_profile=peripheral.profile;peripheral.profile=NULL;
        peripheral.error="profile request expired";peripheral.state=JOB_DONE;
    }
    runtime_irq_restore(irq);stock_free(expired_profile);
    irq=runtime_irq_save();struct ble_advertisement *expired_advertisement=NULL;
    if(peripheral.advertisement && (peripheral.job_epoch!=peripheral.epoch || !stock_bt_context ||
            peripheral.advertisement_cancel==peripheral.advertisement_ticket ||
            stock_ticks()-peripheral.advertisement_at>=1000)) {
        expired_advertisement=peripheral.advertisement;peripheral.advertisement=NULL;
        runtime_advertising_complete(peripheral.advertisement_ticket,"advertising request cancelled or expired");
    }
    runtime_irq_restore(irq);stock_free(expired_advertisement);
    if (peripheral.indicator_dirty) {
        peripheral.indicator_dirty = 0;
        stock_indicator_write(preferences.indicator_off ? 0 : peripheral.indicator_owned ? peripheral.indicator_level : peripheral.native_indicator);
    }
    if (peripheral.recording && (!stock_memo_context || native_priority() ||
            (unsigned)(stock_ticks()-peripheral.record_started) >= 60000)) stop_recording();
    menu_service();
    if(!app.L && app.state!=RUNNING && !menu_visible()) frame_release();
    if (peripheral.state != JOB_QUEUED) return;
    if (peripheral.job_epoch == peripheral.epoch && peripheral.had_job &&
            (unsigned)(stock_ticks()-peripheral.last_job)<100) return;
    peripheral.had_job = 1; peripheral.last_job = stock_ticks();
    peripheral.state = JOB_RUNNING;
    const char *error=peripheral.job_epoch!=peripheral.epoch ? "app released operation" : perform_job();
    /* Configuration completes on the Bluetooth task. Its queue carries only a
     * ticket: cancelling a script or resetting Bluetooth cannot strand a buffer. */
    if(!error && peripheral.op==BT_HID && peripheral.slot==HID_CONFIGURE) return;
    if(!error && peripheral.op==BT_ADVERTISE) return;
    peripheral.error=error;
    if(peripheral.op==BT_HID && peripheral.slot==HID_CONFIGURE)
        stock_free(take_profile(peripheral.ticket));
    if(peripheral.op==BT_ADVERTISE) stock_free(take_advertisement(peripheral.ticket));
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
static unsigned queue_request(unsigned op,unsigned slot,unsigned value,const unsigned char *data,unsigned mask) {
    peripheral.op = op; peripheral.slot = slot; peripheral.value = value; peripheral.mask = mask;
    if (data) memcpy(peripheral.data,data,16);
    peripheral.job_epoch = peripheral.epoch;
    peripheral.ticket = (++peripheral.sequence & 0x7fffffff) + 1;
    __asm__ volatile ("" ::: "memory");
    peripheral.state = JOB_QUEUED;
    return peripheral.ticket;
}
static int submit(lua_State *L, unsigned op, unsigned slot, unsigned value,
                  const unsigned char *data, unsigned mask) {
    native_budget();
    if (!app.resident) return failure(L,"native requests require a resident app");
    if (peripheral.cleanup || peripheral.state == JOB_QUEUED || peripheral.state == JOB_RUNNING)
        return failure(L,"busy");
    lua_pushinteger(L,queue_request(op,slot,value,data,mask));return 1;
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
static int hex_digit(unsigned char c) {
    if (c>='0' && c<='9') return c-'0';
    if (c>='a' && c<='f') return c-'a'+10;
    if (c>='A' && c<='F') return c-'A'+10;
    return -1;
}
static void parse_bt_address(lua_State *L,unsigned char *address) {
    size_t len; const char *s=luaL_checklstring(L,1,&len);
    unsigned any=0,all=255;
    luaL_argcheck(L,len==17,1,"expected XX:XX:XX:XX:XX:XX");
    for (unsigned i=0;i<6;++i) {
        int hi=hex_digit(s[i*3]),lo=hex_digit(s[i*3+1]);
        luaL_argcheck(L,hi>=0 && lo>=0 && (i==5 || s[i*3+2]==':'),1,"invalid Bluetooth address");
        /* Native BdAddr stores the least significant address byte first. */
        address[5-i]=(hi<<4)|lo; any|=address[5-i]; all&=address[5-i];
    }
    luaL_argcheck(L,any && all!=255,1,"invalid Bluetooth address");
}
static int bt_address_connect(lua_State *L,unsigned hid) {
    if (hid==HID_PAIR && lua_isnoneornil(L,1)) {
        unsigned seconds=(unsigned)luaL_optinteger(L,2,120);unsigned char address[16]={0};
        luaL_argcheck(L,seconds>=1 && seconds<=120,2,"pairing window must be 1..120 seconds");
        return submit(L,BT_HID,HID_PAIR,seconds*1000,address,0);
    }
    unsigned char address[16]={0};parse_bt_address(L,address);
    unsigned duration=hid==HID_PAIR ? (unsigned)luaL_optinteger(L,2,120) : 0;
    if (hid==HID_PAIR) luaL_argcheck(L,duration>=1 && duration<=120,2,"pairing window must be 1..120 seconds");
    if (hid) {
        const char *types[]={"public","random",NULL};
        address[6]=luaL_checkoption(L,hid==HID_PAIR ? 3 : 2,"public",types);
    }
    return submit(L,hid ? BT_HID : BT_CONNECT,hid,duration*1000,address,0);
}
static int bt_connect(lua_State *L) { return bt_address_connect(L,0); }
static int keyboard_connect(lua_State *L) { return bt_address_connect(L,HID_CONNECT); }
static int keyboard_listen(lua_State *L) { return bt_address_connect(L,HID_LISTEN); }
static int keyboard_pair(lua_State *L) { return bt_address_connect(L,HID_PAIR); }
static int keyboard_forget(lua_State *L) { return bt_address_connect(L,HID_FORGET); }
static unsigned keyboard_bonded(const unsigned char *address,unsigned address_type) {
    struct hid_status status;runtime_hid_status(&status);
    if (status.transport) {
        for (unsigned i=0;i<16;++i) {
            unsigned char a[6];unsigned type;
            if (runtime_hogp_bond(i,a,&type) && type==address_type && !memcmp(a,address,6)) return 1;
        }
        return 0;
    }
    for (unsigned i=0;i<8;++i) {
        volatile unsigned char *r=stock_bt_manager+7+i*26;
        unsigned match=r[25]!=0;
        for (unsigned j=0;j<6;++j) if (r[j]!=address[j]) match=0;
        if (match) return 1;
    }
    return 0;
}
static void push_bt_address(lua_State *L,const unsigned char *a) {
    const char *digits="0123456789ABCDEF";char address[18];
    for (unsigned i=0;i<6;++i) { unsigned b=a[5-i];
        address[i*3]=digits[b>>4];address[i*3+1]=digits[b&15];address[i*3+2]=':'; }
    address[17]=0;lua_pushstring(L,address);
}
static int keyboard_bonds(lua_State *L) {
    unsigned details=lua_toboolean(L,1);
    struct hid_status status;runtime_hid_status(&status);
    lua_createtable(L,8,0);unsigned n=0;
    if (status.transport) {
        for (unsigned i=0;i<16;++i) {
            unsigned char a[6];unsigned type;
            if (!runtime_hogp_bond(i,a,&type)) continue;
            if (details) {
                lua_createtable(L,0,2);push_bt_address(L,a);lua_setfield(L,-2,"address");
                lua_pushstring(L,type ? "random" : "public");lua_setfield(L,-2,"address_type");
            } else push_bt_address(L,a);
            lua_rawseti(L,-2,++n);
        }
        return 1;
    }
    for (unsigned i=0;i<8;++i) if (stock_bt_manager[7+i*26+25]) {
        unsigned char a[6];for (unsigned j=0;j<6;++j) a[j]=stock_bt_manager[7+i*26+j];
        push_bt_address(L,a);lua_rawseti(L,-2,++n);
    }
    return 1;
}
static int keyboard_name(lua_State *L) {
    struct hid_status s;runtime_hid_status(&s);
    unsigned char address[6];unsigned type=s.address_type;
    if(lua_isnoneornil(L,1)) memcpy(address,s.peer,6);
    else {
        const char *types[]={"public","random",NULL};
        parse_bt_address(L,address);type=luaL_checkoption(L,2,"public",types);
    }
    char name[BT_NAME_BYTES+1];
    if(s.transport && runtime_hogp_name(address,type,name)) lua_pushstring(L,name);
    else lua_pushnil(L);
    return 1;
}
static int keyboard_disconnect(lua_State *L) { return submit(L,BT_HID,HID_DISCONNECT,0,NULL,0); }
static int keyboard_mode(lua_State *L) {
    const char *names[]={"combined","keyboard","ble-remote",NULL};
    return submit(L,BT_HID,HID_MODE,luaL_checkoption(L,1,NULL,names),NULL,0);
}
static int keyboard_configure(lua_State *L) {
    native_budget();luaL_checktype(L,1,LUA_TTABLE);
    struct hid_profile p={0};
    lua_getfield(L,1,"name");size_t size;const char *name=luaL_checklstring(L,-1,&size);
    luaL_argcheck(L,size>=1 && size<=29,1,"name must contain 1..29 bytes");
    for(unsigned i=0;i<size;++i) luaL_argcheck(L,(unsigned char)name[i]>=32 && name[i]!=127,1,"invalid name");
    memcpy(p.name,name,size);p.name_size=size;lua_pop(L,1);
    lua_getfield(L,1,"appearance");p.appearance=integer(L,-1,0,65535);lua_pop(L,1);
    lua_getfield(L,1,"wake");luaL_checktype(L,-1,LUA_TBOOLEAN);p.wake=lua_toboolean(L,-1);lua_pop(L,1);
    for(unsigned report=0;report<2;++report) {
        lua_getfield(L,1,report ? "consumer" : "keys");luaL_checktype(L,-1,LUA_TTABLE);
        unsigned count=lua_rawlen(L,-1);luaL_argcheck(L,count>=1 && count<=16,1,"each report needs 1..16 usages");
        if(report) p.media=count;else p.keys=count;
        for(unsigned i=0;i<count;++i) {
            lua_rawgeti(L,-1,i+1);unsigned v=integer(L,-1,report ? 1 : 4,report ? 65535 : 231);lua_pop(L,1);
            for(unsigned j=0;j<i;++j) luaL_argcheck(L,p.usage[report*16+j]!=v,1,"duplicate usage");
            p.usage[report*16+i]=v;
        }
        lua_pop(L,1);
    }
    if(!app.resident) return failure(L,"native requests require a resident app");
    if(peripheral.profile || peripheral.cleanup || peripheral.state==JOB_QUEUED || peripheral.state==JOB_RUNNING)
        return failure(L,"busy");
    if(stock_free_heap()<sizeof p+32+STOCK_HEAP_RESERVE) return failure(L,"insufficient native heap");
    struct hid_profile *copy=stock_alloc(sizeof p);if(!copy) return failure(L,"insufficient native heap");
    *copy=p;unsigned irq=runtime_irq_save();
    peripheral.profile=copy;peripheral.profile_at=stock_ticks();
    unsigned ticket=queue_request(BT_HID,HID_CONFIGURE,0,NULL,0);peripheral.profile_ticket=ticket;
    runtime_irq_restore(irq);lua_pushinteger(L,ticket);return 1;
}
static int settings_get(lua_State *L) {
    if (!saved.initialized || !saved.settings_size) { lua_pushnil(L);return 1; }
    lua_pushlstring(L,(const char *)saved.settings,saved.settings_size);return 1;
}
static int settings_set(lua_State *L) {
    size_t n;const char *s=luaL_checklstring(L,1,&n);
    luaL_argcheck(L,n<=SETTINGS_LIMIT,1,"settings exceed 128 bytes");
    if (peripheral.cleanup || peripheral.state==JOB_QUEUED || peripheral.state==JOB_RUNNING) return failure(L,"busy");
    memcpy(saved.pending,s,n);saved.pending_size=n;
    return submit(L,SETTINGS_SAVE,0,0,NULL,0);
}
static int keyboard_tap(lua_State *L) {
    unsigned key=luaL_checkinteger(L,1),modifiers=luaL_optinteger(L,2,0);
    luaL_argcheck(L,key>=4 && key<=0xe7,1,"keyboard usage must be 4..231");
    luaL_argcheck(L,modifiers<=255,2,"modifiers must be 0..255");
    struct hid_status status;runtime_hid_status(&status);
    luaL_argcheck(L,!status.transport || runtime_hogp_key(HID_KEY,key,modifiers),1,"unsupported BLE remote key or modifier");
    unsigned char data[16]={0};data[0]=modifiers;
    unsigned ms=lua_isnoneornil(L,3) ? 0 : integer(L,3,10,2000);data[2]=ms;data[3]=ms>>8;
    return submit(L,BT_HID,HID_KEY,key,data,0);
}
static int keyboard_consumer(lua_State *L) {
    unsigned usage=integer(L,1,1,65535),ms=lua_isnoneornil(L,2) ? 0 : integer(L,2,10,2000);
    struct hid_status s;runtime_hid_status(&s);
    luaL_argcheck(L,s.transport ? runtime_hogp_key(HID_CONSUMER,usage,0)!=0 : usage<=1023,1,"usage is not advertised by this profile");
    unsigned char data[16]={0};data[2]=ms;data[3]=ms>>8;
    return submit(L,BT_HID,HID_CONSUMER,usage,data,0);
}
static int keyboard_media(lua_State *L) {
    const char *names[]={"play_pause","mute","volume_up","volume_down","next","previous","stop","power",NULL};
    static const unsigned usages[]={0xcd,0xe2,0xe9,0xea,0xb5,0xb6,0xb7,0x30};
    unsigned char data[16]={0};
    return submit(L,BT_HID,HID_CONSUMER,usages[luaL_checkoption(L,1,NULL,names)],data,0);
}
static int keyboard_status(lua_State *L) {
    unsigned reuse=lua_istable(L,1),diagnostics=lua_toboolean(L,reuse ? 2 : 1);
    struct hid_status s;runtime_hid_status(&s);
    if (reuse) lua_pushvalue(L,1);else lua_createtable(L,0,16);
    field(L,"state",s.state);flag(L,"connected",s.state==2);
    flag(L,"enabled",s.enabled==1);flag(L,"busy",s.busy);field(L,"sent",s.sent);
    flag(L,"keyboard_only",s.keyboard_only);
    flag(L,"mode_locked",preferences.bt_mode==2);
    lua_pushstring(L,s.transport ? "ble" : "classic");lua_setfield(L,-2,"transport");
    lua_pushstring(L,s.address_type ? "random" : "public");lua_setfield(L,-2,"address_type");
    field(L,"released",s.released);field(L,"errors",s.errors);field(L,"error",s.error);
    field(L,"bonds_saved",peripheral.bt_bonds_saved);
    flag(L,"paired",s.enabled && keyboard_bonded(s.peer,s.address_type));flag(L,"encrypted",s.state==2 && s.encryption_state==2);
    flag(L,"pairing",s.pairing);field(L,"pair_remaining_ms",s.pair_remaining_ms);
    field(L,"access_mode",s.access_mode);field(L,"forgotten",s.forgotten);
    if (diagnostics) {
        field(L,"hidden_services",s.hidden_services);field(L,"blocked_psms",s.blocked_psms);
        field(L,"audio_channels",s.audio_channels);
        field(L,"incoming",s.incoming);field(L,"opened",s.opened);field(L,"closed",s.closed);
        field(L,"close_status",s.close_status);field(L,"close_channel",s.close_channel);field(L,"control",s.control);
        field(L,"authentication_state",s.authentication_state);field(L,"encryption_state",s.encryption_state);
        field(L,"key_type",s.key_type);field(L,"security_mode",s.security_mode);field(L,"ssp",s.ssp);
    }
    unsigned any=0;for(unsigned i=0;i<6;++i) any|=s.peer[i];
    if (s.enabled && any) {
        push_bt_address(L,s.peer);lua_setfield(L,-2,"peer");
    } else { lua_pushnil(L);lua_setfield(L,-2,"peer"); }
    return 1;
}
static int bt_disconnect(lua_State *L) { return submit(L,BT_DISCONNECT,0,0,NULL,0); }
static int bt_mute(lua_State *L) { return submit(L,BT_MUTE,0,0,NULL,0); }
static int bt_advertise(lua_State *L) {
    native_budget();luaL_checktype(L,1,LUA_TTABLE);
    struct ble_advertisement p={0};size_t size;
    lua_getfield(L,1,"data");luaL_checktype(L,-1,LUA_TSTRING);
    const char *data=lua_tolstring(L,-1,&size);
    luaL_argcheck(L,size>=1 && size<=31,1,"data must contain 1..31 bytes");
    memcpy(p.data,data,size);p.size=size;lua_pop(L,1);
    lua_getfield(L,1,"scan_response");
    if(!lua_isnil(L,-1)) {
        luaL_checktype(L,-1,LUA_TSTRING);data=lua_tolstring(L,-1,&size);
        luaL_argcheck(L,size<=31,1,"scan response exceeds 31 bytes");
        memcpy(p.scan,data,size);p.scan_size=size;
    }
    lua_pop(L,1);
    const char *types[]={"nonconnectable","scannable","connectable",NULL};
    const unsigned char values[]={3,2,0};
    lua_getfield(L,1,"type");p.type=values[luaL_checkoption(L,-1,"nonconnectable",types)];lua_pop(L,1);
    lua_getfield(L,1,"interval_ms");p.interval_ms=lua_isnil(L,-1) ? 100 : integer(L,-1,100,1000);lua_pop(L,1);
    lua_getfield(L,1,"duration_ms");p.duration_ms=lua_isnil(L,-1) ? 1500 : integer(L,-1,100,5000);lua_pop(L,1);
    luaL_argcheck(L,runtime_advertising_valid(&p),1,"invalid advertisement");
    if(!app.resident) return failure(L,"native requests require a resident app");
    if(peripheral.advertisement || runtime_advertising_busy() || peripheral.cleanup ||
            peripheral.state==JOB_QUEUED || peripheral.state==JOB_RUNNING) return failure(L,"busy");
    if(stock_free_heap()<sizeof p+32+STOCK_HEAP_RESERVE) return failure(L,"insufficient native heap");
    struct ble_advertisement *copy=stock_alloc(sizeof p);if(!copy) return failure(L,"insufficient native heap");
    *copy=p;unsigned irq=runtime_irq_save();
    peripheral.advertisement=copy;peripheral.advertisement_at=stock_ticks();peripheral.advertisement_cancel=0;
    unsigned ticket=queue_request(BT_ADVERTISE,0,0,NULL,0);peripheral.advertisement_ticket=ticket;
    runtime_irq_restore(irq);lua_pushinteger(L,ticket);return 1;
}
static int bt_advertise_cancel(lua_State *L) {
    native_budget();unsigned ticket=integer(L,1,1,0x7fffffff);
    if(peripheral.op!=BT_ADVERTISE || peripheral.ticket!=ticket || peripheral.job_epoch!=peripheral.epoch)
        return failure(L,"expired ticket");
    if(peripheral.state==JOB_DONE) { lua_pushboolean(L,0);return 1; }
    peripheral.advertisement_cancel=ticket;lua_pushboolean(L,1);return 1;
}
static int bt_media(lua_State *L) {
    const char *actions[]={"pause","play",NULL};
    return submit(L,BT_MEDIA,0,luaL_checkoption(L,1,NULL,actions),NULL,0);
}
static int bt_status(lua_State *L) {
    unsigned available=stock_bt_context!=NULL;
    unsigned media=available ? stock_avrcp_state() : 0;
    unsigned audio=available ? stock_a2dp_state() : 0;
    lua_createtable(L,0,7);
    flag(L,"available",available); flag(L,"ble_connected",stock_ble_connected);
    field(L,"media_state",media); field(L,"audio_state",audio);
    flag(L,"media_connected",media==2); flag(L,"audio_connected",audio>=2);
    field(L,"access_mode",available ? stock_bt_manager[0xdc] : 0);
    if (media==2 || audio>=2) {
        const char *digits="0123456789ABCDEF"; char address[18];
        for (unsigned i=0;i<6;++i) {
            unsigned b=stock_bt_manager[0xe0+5-i];
            address[i*3]=digits[b>>4];address[i*3+1]=digits[b&15];address[i*3+2]=':';
        }
        address[17]=0;lua_pushstring(L,address);lua_setfield(L,-2,"peer");
    }
    return 1;
}
/* Native API tables are cached on first access and reclaimed with the VM. */
static int peripheral_lazy_module(lua_State *L) {
    const char *name=lua_tostring(L,2);if(!name) return 0;
    static const luaL_Reg storage[]={{"get",settings_get},{"set",settings_set},{NULL,NULL}};
    static const luaL_Reg keyboard[]={{"connect",keyboard_connect},{"listen",keyboard_listen},{"disconnect",keyboard_disconnect},
        {"pair",keyboard_pair},{"forget",keyboard_forget},{"bonds",keyboard_bonds},{"mode",keyboard_mode},
        {"configure",keyboard_configure},{"tap",keyboard_tap},{"consumer",keyboard_consumer},
        {"media",keyboard_media},{"status",keyboard_status},{"name",keyboard_name},{NULL,NULL}};
    static const luaL_Reg bluetooth[] = {{"status",bt_status},{"connect_media",bt_connect},
        {"disconnect_media",bt_disconnect},{"media",bt_media},{"mute",bt_mute},
        {"advertise",bt_advertise},{"advertise_cancel",bt_advertise_cancel},{NULL,NULL}};
    static const luaL_Reg power[] = {{"battery",battery},{"indicator",indicator},
        {"get_schedule",get_wake},{"set_schedule",set_wake},{NULL,NULL}};
    static const luaL_Reg alarms[] = {{"get",get_alarm},{"set",set_alarm},{"status",alarm_status},
        {"cancel",cancel_alarm},{"snooze",snooze_alarm},{NULL,NULL}};
    static const luaL_Reg audio[] = {{"play",audio_play},{"pause",audio_pause},{"source",audio_source},{"next",audio_next},{"previous",audio_previous},{"direction",audio_direction},
        {"track",audio_track},{"seek",audio_seek},{"repeat_mode",audio_repeat},{"preview",audio_preview},
        {"stop",audio_stop},{"status",audio_status},{"memo_play",record_play},{"memo_delete",record_delete},{NULL,NULL}};
    static const luaL_Reg mic[] = {{"noise",noise_enable},{"level",noise_level},
        {"record",record_start},{"stop",record_stop},{NULL,NULL}};
    static const struct { const char *name;const luaL_Reg *functions; } libs[]={
        {"keyboard",keyboard},{"storage",storage},{"bluetooth",bluetooth},{"power",power},{"alarm",alarms},{"audio",audio},{"microphone",mic}};
    for(unsigned i=0;i<sizeof libs/sizeof *libs;++i) if(!strcmp(name,libs[i].name)) {
        module(L,name,libs[i].functions);lua_getglobal(L,name);return 1;
    }
    return 0;
}
