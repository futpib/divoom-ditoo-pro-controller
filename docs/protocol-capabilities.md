# Device protocol capabilities

Scope: operations performed by the device, independent of Android editors,
accounts, galleries and other phone UI. Based on Divoom Android 3.8.40 (640),
inspected 2026-09-28, and a Ditoo Pro reporting firmware 306007.

The CLI now exposes arbitrary binary commands over RFCOMM and BLE, symbolic
command names, reply matching, JSON batches and incoming response capture.
This provides access to commands without adding a CLI subcommand for each one.
It does **not** implement every payload codec or multi-step transfer, nor prove
that commands for other Divoom products work on this device.

## Coverage and remaining work

All numbers below are hexadecimal opcodes. `BD/xx` means opcode BD followed by
selector xx. “Raw” means packet access exists but a complete convenience API,
payload codec or workflow is still missing. Existing display/media functions
are retained; their BLE transfer path has not been exercised on hardware.

| # | Device operation | Protocol | Current coverage / remaining gap |
|---|---|---|---|
| 1 | Firmware versions, device metadata | 36,37,97; BD/2B | `firmware` decodes version list; other metadata raw. Version 306007 read live. |
| 2 | Display/channel state, colors, brightness | 44,45,46,49,74 | Existing mode/brightness controls; `status` decodes mode/brightness and preserves all bytes. State read live. |
| 3 | Static pixels, GIFs, animation/movie streaming | 19,1A,25,58,5A–5C,6B–6F,8B | Existing image/animation/video/text renderers; raw access to other encodings and transfer variants. No new codec parity claim. |
| 4 | Persistent custom channels/playlists, slots, timing | B1,8C–8F; BD/13–17,1B | Raw. Need slot enumeration, upload/storage, delete, selection and playback-duration workflows. |
| 5 | Clock faces, time, language, temperature/clock units | 18,2B,2D; BD/26,2A,2E | Existing clock/time/language; `setting hour24` and `fahrenheit` read/set. Clock-format write/readback and restoration verified live. |
| 6 | Weather and temperature display | 59,5D–5F,73 | `weather CELSIUS CONDITION`; full forecast layout/data and sensor responses raw. |
| 7 | Keyboard lights and light effects | 23; BD/33 | Existing keyboard next/previous/toggle. Absolute keyboard-off state/readback unknown; extended effects raw. |
| 8 | Alarm slots and repeat schedules | 42,43 | `alarms` reads raw slots; `alarm on/off --time ...` writes all configurable fields and now honors enabled state. Alarm list read live. |
| 9 | Alarm artwork, audio, preview and preview volume | 51,82,A5,A6 | Raw; custom media transfer and preview workflow missing. |
| 10 | Sleep schedules, scenes, color, brightness, audio | 40,41,79,A2–A4,AD,AE | `setting sleep-mode`; other payloads/workflows raw. |
| 11 | Planner/reminders/time-management and artwork | 53–57 | Raw; schedule serialization, enumeration and attached artwork workflow missing. |
| 12 | Stopwatch, scoreboard, noise meter, countdown | 71,72 | Helpers `stopwatch`, `scoreboard`, `noise-meter`, `countdown`, `tool-status`. Android payloads implemented; tool queries received no reply in this test. Setters not physically exercised. |
| 13 | Built-in games and virtual key presses/releases | A0,17,21,88 | `game`, `game-key [--release]`; shake raw. Does not bind physical keys or install new games. |
| 14 | Volume and playback state | 08–0B; BD/34–36 | Existing volume/play/pause; `playback-status`. Volume read live. Newer extended variants raw. |
| 15 | TF/SD music listing, track selection, seek and repeat | 07,11,12,14,15,7D,B4,B8,B9 | Helpers `sd-status`, `sd-track`, `sd-seek`, `sd-play-mode`, `track`. SD status captured live; paginated names/IDs and response decoding remain raw. Seek units not established. |
| 16 | Saved volume, auto-connect, boot channel, idle shutdown | AB,AC,8A; BD/18–1A | `setting save-volume/auto-connect/startup-channel/idle-power-off`. First two queried live; latter two timed out. Timeout is not proof of lack of support. |
| 17 | Scheduled power on/off, energy and eye-care settings | 1F,22,B2,B3 | Raw; schedule/energy codecs missing. |
| 18 | Startup sound, volume, boot artwork | 1C,52,BB | Raw; media upload and startup settings helpers missing. |
| 19 | Notifications and custom notification icons | 3C,50,84; BD/27 | Raw; notification category/icon payload builders missing. Phone permission/UI features are out of scope. |
| 20 | Voice messages, microphone/recording, karaoke | 20,7E,7F,A1,A7–AA; BD/1D,1E,20 | Raw; audio codecs, transfer, recording/playback state and accessory-specific controls missing. |
| 21 | Mixer, rhythm patterns and audio-reactive visuals | 1B,6A,BC,B6,B7; BD/32 | Existing music display mode; other mixer/rhythm/equalizer payloads raw. |
| 22 | Key-function configuration and accessory control | 89; BD/11,12,21 | Raw. BD/12 read produced no reply. App builder concerns accessory single/long-press built-in functions; no proof of Ditoo keyboard remapping or arbitrary key events. |
| 23 | FM tuner, presets, region and scan | 60,61,63,64,67–70 | Raw; no evidence this unit has the required FM hardware. |
| 24 | Multi-screen drawing, orientation, mirroring | 3A,3B,49,77; BD/23,24 | Raw, model-dependent. Do not infer Ditoo support from the shared app. |
| 25 | Device name, connection flags, Bluetooth password | 75,AF,B0,27 | Raw; state codecs and model-specific behavior unverified. |
| 26 | Firmware, hot-content, fonts and resource transfers | 48,7C,93–99,9B,9D–9F,F7,BE; BD/28,29,2D,30,31,37,38,3A | Firmware version read and vendor image archive available. Update packaging, negotiation, resume and flashing workflow remain unimplemented. Raw packet access is not a firmware updater. |
| 27 | Wi-Fi provisioning, JSON command API, factory/configuration reset | 01,F0–F4; BD/25,2C | Shared app catalogue only. Device-specific JSON framing and provisioning workflow not implemented; resets untested. |

The complete [196-symbol catalogue](../data/protocol-commands.json) contains
156 base names (154 distinct base opcodes, including aliases) and 40 extended
selectors. [Builder references](protocol-command-sources.json) map each symbol
to matching Android command-construction methods and source lines. This is an
inventory of the inspected enums, not every feature in every app version.

## Using the API

```sh
# Explicit --device is useful when BLE is available but the device is unpaired.
divoom-ditoo-pro-controller --device B1:21:81:DD:B8:9B --transport ble firmware

# List names, or send an opcode. --query waits for a reply with that opcode.
divoom-ditoo-pro-controller raw list --filter ALARM
divoom-ditoo-pro-controller raw send 0x37 --data 00 --query

# Extended names insert the selector in both request and expected response.
divoom-ditoo-pro-controller raw send SPP_SECOND_SET_SAVE_VOLUME_CFG --data ff --query

# Wait for a different response opcode or filter an extra response prefix.
divoom-ditoo-pro-controller raw send 0xbd --data 19ff --expect 0xbd --prefix 19

# Collect replies/events after a command without assuming a reply opcode.
divoom-ditoo-pro-controller raw send 0xb4 --listen-ms 1500

# One connection, JSON lines of results. All requests validated before connecting.
divoom-ditoo-pro-controller raw run examples/device-info.json
divoom-ditoo-pro-controller raw run examples/device-settings.json --dry-run

# Inspect exact payloads without connecting or changing the device.
divoom-ditoo-pro-controller scoreboard 12 34 --dry-run
divoom-ditoo-pro-controller countdown 5 30 --dry-run
divoom-ditoo-pro-controller weather -5 8 --dry-run
divoom-ditoo-pro-controller alarm on --time 07:30 --repeat 62 --dry-run
```

Batch files are arrays of requests:

```json
[
  {"command":"0x37","payload_hex":"00","response":"0x37"},
  {"command":"SPP_SECOND_SET_SAVE_VOLUME_CFG","payload_hex":"ff",
   "response":"SPP_SECOND_SET_SAVE_VOLUME_CFG","delay_ms":250,"listen_ms":500}
]
```

`response` is optional. `response_prefix_hex` filters bytes following an extended
selector, if any. Delay and listen duration each range from 0 to 60000 ms.
Unknown JSON fields and invalid payloads fail before connecting. Batches stop
on the first error; they are not transactions and do not undo earlier writes.
`raw monitor --seconds 10` prints incoming framed responses; it does not
turn unsupported physical keys into events. BLE initialization synchronizes
the device clock even when monitoring.

Output distinguishes `command_reply`, `transport_ack`, and RFCOMM `written`.
Only a matching command reply confirms that the requested reply arrived;
a transport ACK does not establish that the device applied a setting.
Replies retain `data_hex` even when some fields are decoded. `rfcomm_frame_hex`
in dry-run output describes classic framing; BLE wraps opcode and payload in
its own transport envelope. Reads are not automatically retried after timeout.

Library clients can use `control::Request::prepare`, `control::Session::connect`,
`execute`, `receive`, and `disconnect`, with `with_transport` selecting BLE or
RFCOMM. `send_alarm(address, &Alarm)` replaces the former hard-coded alarm API.

## Payload details and evidence

Source root: `com/divoom/Divoom/bluetooth/` in the inspected APK. The command
enums are `SppProc$CMD_TYPE.java` and `SppProc$EXT_CMD_TYPE.java`; payload builders
are in `CmdManager.java`. APK SHA-256:
`d7ae490205cf71cc37f74948bd1ca7f1b2a446070565294b3ae835c0a51fde81`.
JADX reported 65 decompilation errors; unavailable method bodies limit coverage.

Implemented payloads follow those builders:

- Tool setter 72: `00 action`, `01 enabled redLE16 blueLE16`, `02 action`, or
  `03 enabled minutes seconds`. `ids stopwatch` lists 0=stop, 1=start, 2=reset; `ids noise-meter` lists 1=start, 2=stop.
- Game A0: `enabled gameID`; 17/21: `keyCode` down/up. Game IDs depend on firmware; `ids games` lists the inspected Ditoo Pro app mapping.
- Weather 5F: signed Celsius byte and condition byte.
- Alarm 43: index, enabled, hour, minute, repeat, mode, trigger, two FM bytes,
  volume. Repeat bits start at Sunday; 62 is Monday through Friday. CLI FM
  frequency is tenths of MHz, encoded remainder/division by 100 (not LE16).
  Full-slot writes require a time even when disabling; other fields use defaults.
- Settings 2B/2D and BD/19/1A use FF to query, 00/01 to set. AB uses LE16;
  its time unit has not been established. Startup channel uses 8A/00 to query
  and 8A/01/channel to set.

[Captured evidence](protocol-evidence/) contains successful queries and failures.
A timeout only means no complete expected acknowledgment/reply arrived within
the timeout: it does not distinguish unavailable features from device state,
transport behavior or an incomplete payload interpretation. Success on BLE
here does not establish new RFCOMM hardware coverage. No update, reset, game,
alarm or custom-media write was sent in this protocol audit.

Live write/readback: 2D/FF returned 00; 2D/01 followed by 2D/FF returned 01;
2D/00 followed by 2D/FF returned 00 again. SD status B4 also replied and final
46 state reported brightness 0. The original clock format was restored and
keyboard lighting was not changed. See `readback-batch.jsonl` and its matching
request file. Standalone reads in `readback.json` intermittently timed out;
that transport/session limitation remains unresolved. Single-session batches
worked in this test; there is no automatic replay of writes.
