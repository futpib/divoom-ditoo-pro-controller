# Native peripherals in Lua

Firmware **306016** adds the APIs below. They use the Ditoo Pro's existing
battery monitor, scheduler, player, recorder and noise detector. Alarm and power
schedule settings survive a reboot; uploaded Lua programs still live in RAM.

## Requests and completion

Battery, audio status and microphone level reads return immediately. Native
configuration and audio operations run on the stock main task, through one
fixed-size request slot. They return a ticket, or `nil, reason` if busy. Poll `device.result(ticket)` from `update`:

```lua
local ticket
return {
  init = function() ticket = assert(alarm.get(0)) end,
  update = function()
    local ok, value = device.result(ticket)
    if ok == nil then return end -- still pending
    assert(ok, value)           -- false, reason means the operation failed
    app.log(value.hour .. ':' .. value.min)
  end,
}
```

A successful get/set returns `true, settings`. Other successes return `true`:
the native handler has run, which does not establish that an external player
responded or a sound was audible. Check `audio.status()` for current state.
A ticket remains readable until the next request replaces it. An old ticket
returns `nil, "expired ticket"`. Requests require a resident app; a one-shot
program cannot leave an unowned operation queued.

The main task services at most one request per 100 ms; queued requests wait automatically. Changed saved settings have a one-second
cooldown and a 64-change budget per boot. Identical settings do not write flash.
Recorder starts have a separate 32-per-boot limit. Invalid IDs, times, sizes and
types are rejected before dispatch. No API accepts a native pointer or raw
firmware command.

## Battery and charge bar

| # | API | Result |
| --- | --- | --- |
| 1 | `power.battery()` | `{level, max_level=7, external_power, charging, full}`. `level` is the native coarse 0–7 measurement, not a percentage. |
| 2 | `power.indicator(0..5)` | Override the five battery LEDs. Zero turns them off. |
| 3 | `power.indicator()` | Release the bar to stock firmware. |

Charging uses the stock USB-power and charge-pin readings. `full` requires
external power and the native full-charge level. Battery voltage and calibrated
percentage are not exposed. Indicator control changes the LEDs only.
Pause, stop, replacement and script failure release the override.

## Alarms and scheduled power

| # | API | Behavior |
| --- | --- | --- |
| 1 | `alarm.get(slot)` | Read native slot 0–9. |
| 2 | `alarm.set(slot, fields)` | Merge fields into a slot, save, read back and reschedule. |
| 3 | `alarm.status()` | Native `state`, `slot` (255 if none) and `ringing`. |
| 4 | `alarm.cancel()` | Dismiss the active native alarm. |
| 5 | `alarm.snooze()` | Use native snooze; fails unless an alarm is ringing. |
| 6 | `power.get_schedule(slot)` | Read native power-schedule slot 0–8. |
| 7 | `power.set_schedule(slot, fields)` | Merge, save, read back and recalculate the native power schedule. |

Alarm fields: `enabled` (boolean), `hour` (0–23), `min` (0–59), `days` (0–127),
`mode` (0–13), `trigger` (0–5), `volume` (0–100). Omitted fields remain unchanged,
including native padding and uploaded alarm resources. Disable with
`alarm.set(slot, {enabled=false})`. Setting an alarm does not delete its artwork
or recording. Mode numbers use the stock firmware's 14-entry sound table; the
individual sound names and trigger labels have not all been decoded.

Power fields: `enabled`, `action` (`"on"` or `"off"`), `hour`, `min`, `days`,
`color` (`0xRRGGBB`). Sunday is bit 0 of `days`; 62 means weekdays, 127 every day,
and 0 means a one-shot schedule. For example:

```lua
-- Issue separately, after the preceding request completes and cooldown expires.
alarm.set(9, {enabled=true, hour=7, min=30, days=62, mode=2, trigger=1, volume=25})
power.set_schedule(8, {enabled=true, action='on', hour=7, min=0, days=62})
```

The stock RTC/scheduler owns timing; a Lua timer is not needed to keep an alarm
alive. Native ringing alarms take display, RGB-light and keyboard priority over
the app. Native power keys remain available. Waking from power-off boots the
native interface; Lua autostart is not implemented.

## Speaker and microphone

| # | API | Behavior |
| --- | --- | --- |
| 1 | `audio.source('bluetooth'|'sd'|'usb')` | Select a native input; absent SD/USB inputs are rejected. |
| 2 | `audio.play()`, `pause()`, `next()`, `previous()` | Native playback/media controls; Bluetooth playback requires a connected audio source. |
| 3 | `audio.track(index)`, `seek(seconds)` | Zero-based SD track selection and seek. Both are bounded 16-bit values; tracks depend on the inserted card. These controls and repeat mode require a ready SD player. |
| 4 | `audio.repeat_mode('all'|'one'|'shuffle')` | SD repeat mode; numeric 0/1/2 are also accepted. |
| 5 | `audio.preview(mode, volume)` | Native alarm-sound preview, mode 0–13, volume 0–100, native 60-second limit. Returns a failed ticket if the native mode/priority arbiter refuses it. |
| 6 | `audio.stop()` | Stop native sound playback and pause music. |
| 7 | `audio.status()` | Source ID (Bluetooth 0, SD 3, USB 7), SD presence, playing, volume, position/duration in seconds, track/count, repeat mode, recording, recorded-byte count and sound-playing state. |
| 8 | `microphone.noise(bool)` | Acquire/release the native noise monitor without entering its stock display mode. |
| 9 | `microphone.level()` | Latest native noise-meter value and sample age in ms; `nil, reason` until sampled. It is not a calibrated SPL measurement. |
| 10 | `microphone.record()`, `stop()` | Record into the native voice-memo slot; maximum 60 seconds. Starting replaces the previous memo, as the native feature does. |
| 11 | `audio.memo_play()`, `memo_delete()` | Native memo playback or deletion. Native playback cleanup deletes the memo after use. |

Noise monitoring and recording are exclusive. Audio allocations must leave
enough native heap: recorder start currently requires 52 KiB. Large scripts may
receive a headroom error. `device.stats()` exposes `free_heap`, `lua_used` and
`lua_reserved` to diagnose this. Microphone ownership, Lua-started previews and
recording are released on pause/stop/error; an already-active stock noise monitor
is restored. Music playback and explicitly saved schedules are native settings
and persist independently of the script.

There is no raw PCM streaming, synthesis, DSP, speech recognition or arbitrary
audio-file loading API. Scope is the device's existing player, native sounds,
voice memo and noise meter.

Examples: `examples/lua/battery.lua`, `noise-meter.lua` and `voice-memo.lua`.
The memo example accepts `lua send record`, `stop`, `play`, `delete` and `status`.

For a short sound, `examples/lua/short-sound.lua` starts silently and accepts
`lua send play`. It selects mode 6 at preview volume 15/100, waits for the native
playing flag, then requests a stop after 300 ms. A two-second startup timeout
also stops the preview. App ticks and asynchronous native dispatch add latency;
this is not a sample-exact 300 ms output. Stop/error releases preview ownership.

Preview volume is independent of the music-volume setting. Mode 6 at 60/100
for 1.5 seconds was confirmed audible by the owner and was too loud for this
test. The quieter short example has not yet been confirmed audibly. Earlier
300 ms attempts with mode 2 were silent to the owner despite `sound_playing`;
the changed sound and duration do not isolate the cause. That flag reports a
decoder assignment, not measured speaker output.

## Implementation and checks

`native/lua-app/peripherals.c` contains the bounded request bridge and Lua
bindings. The main-loop call at `0x47838` services requests; the battery-bar entry
at `0x2d6e8` substitutes an LED level, and the noise-meter call at `0x72192`
captures native readings. Addresses refer to the pinned stock 306007 code.
Native settings models are 2 (16-byte alarms) and 17 (nine 16-byte schedules).
The bridge never retains Lua-managed memory after returning to a script.

306016 also fixes two native scheduler defects: one-shot cleanup at `0x47fdc`
and `0x4818e` saved 72 bytes of a 144-byte table, discarding later wake slots;
and the earliest-deadline search remembered the last eligible slot rather than
the selected slot. All nine records are now retained, and the selected slot is
updated together with its deadline. Invalid tail records from an older truncated
table read as disabled instead of being passed to the RTC helper.

Uploaded source uses a bounded heap allocation, freed after compilation or
cancellation. Globals reserve 8 KiB; the reclaimed 8 KiB remains available to
native audio while apps run. The allocator keeps the 40 KiB total quota, with nonmoving allocations in KiB
units and immediate release of empty pages. Stop/error still frees all pages
without invoking Lua finalizers. Instruction/time/native-call limits and the
five-second physical escape remain enabled.

```sh
python3 scripts/build-lua-app-runtime.py
python3 scripts/test-lua-app-runtime.py
python3 scripts/check-lua-app-device.py B1:21:81:DD:B8:9B --firmware 306016 \
  --output firmware/runs/lua-peripherals/runtime-check
python3 scripts/check-lua-peripherals.py B1:21:81:DD:B8:9B \
  --output firmware/runs/lua-peripherals/settings-check --settings-test
# Replaces the native memo, records briefly, plays and deletes it, then tests recovery.
python3 scripts/check-lua-peripheral-audio.py B1:21:81:DD:B8:9B \
  --output firmware/runs/lua-peripherals/audio-check --record-memo
# Requires unused alarm 9 and power slots 7/8; sounds an alarm and cycles power.
python3 scripts/check-lua-peripheral-schedules.py B1:21:81:DD:B8:9B \
  --output firmware/runs/lua-peripherals/schedule-check --usb-port 1-6 \
  --exercise-schedules
```

The peripheral checker backs up exposed settings, changes disabled slots,
checks readback and restores them. It does not record audio or schedule an
audible alarm. Physical and live-runtime results are recorded separately in
the [firmware evidence](../firmware/lua-peripherals-evidence/verification.json),
with the image hash used for each group, rather than inferred from host stubs.
`firmware/lua-peripherals-evidence/audio-controls.lua` additionally exercises
USB/Bluetooth source changes, absent-SD rejection and native preview priority;
run it with `lua start`, then inspect `lua status` after eight seconds.

The device has no SD card installed, so actual SD decoding/seek/repeat remains
unverified. Recorder byte counts, memo playback state and live noise readings
were observed. Native preview output was confirmed audible for mode 6;
memo audibility and the battery bar's physical appearance remain unconfirmed.
The native preview arbiter can refuse
a request while another native mode has priority; this is reported as a failed
ticket, and the bridge does not force an override.
