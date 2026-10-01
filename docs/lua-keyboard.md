# Standalone TV keyboard remote

For pairing failures, firmware 306024 adds [device-side Bluetooth tracing](bluetooth-trace.md) over USB without stopping this app.

Firmware **306028** runs the remote directly on the Ditoo with its speaker
profiles disabled and prevents stock audio policy from disconnecting the keyboard.
After one installation,
setup, pairing, reconnection and ordinary use need only the Ditoo and TV.
Bluetooth HID sends Play/Pause, Mute, Volume Up/Down, Space and Left/Right reports. AVRCP and the
older `tv-remote.lua` remain available separately.

```sh
# One-time setup from a computer.
divoom-ditoo-pro-controller --transport usb firmware-update firmware/306028-lua.MVA
divoom-ditoo-pro-controller --transport usb lua install examples/lua/tv-keyboard.lua
```

## On the Ditoo and TV

1. Power on the Ditoo. It starts the saved remote automatically.
2. Controls already match the printed keys; there is no button setup.
3. For a **new pairing**, forget the old **DitooPro-Audio** accessory on the TV
   if it exists, then open **Pair accessory**. Keep an existing working bond
   when upgrading this app.
4. On the Ditoo, press the lever to confirm **PAIR / RESET SAVED TV?**.
   If a target is already saved, use M → Pair → lever, then lever to confirm.
   Choose **DitooPro-Audio** on the TV and accept the request. No MAC address is
   needed. A blue bar shows the two-minute pairing window.
5. **TV / READY** with a green dot means the keyboard connection is established.

| # | Physical control | Remote action | Stock ADC ID |
| --- | --- | --- | --- |
| 1 | Lever | Play/Pause | 4 |
| 2 | + | TV volume up | 1 |
| 3 | − | TV volume down | 9 |
| 4 | ← | Left arrow (seek or navigate) | 2 |
| 5 | → | Right arrow (seek or navigate) | 3 |
| 6 | M | Open/close local menu | 0 |
| 7 | ☀ Sun key (top-right) | Space | 7 |
| 8 | Audio-source button (beside lever base) | Mute/unmute | 10 |

The fixed IDs come from the [stock firmware key tables](stock-ux.md#physical-key-ids).
Left/Right send ordinary keyboard arrows (HID usages 80/79), so the TV app
and focus determine whether they seek or navigate. Volume sends Consumer
Volume Increment/Decrement (233/234), not Ditoo speaker volume. A sent report
does not prove that every TV app handles it. SmartTube Play/Pause is
owner-confirmed; see the verification notes below for other coverage.

The remote remembers the connected TV. Later boots try the saved TV up to
three times, then listen for it. If necessary use LINK in the menu or the
TV's Connect action. After a connected TV disconnects, the remote automatically
resumes listening for that saved TV. This also works when the Ditoo initiated
the previous connection. Listening does not consume outgoing connection
attempts. A remote-action key pressed while disconnected also initiates a
connection to the saved TV. The latest action is retained for at most five
seconds and sent once if the connection opens in time; stale actions expire.
This reuses the bond and obeys the native connection-attempt limits. Opening
the local menu cancels a pending action. OFF stays offline until LINK or PAIR
is selected; ordinary keys do not override it. Reconnection reuses
the saved bond; PAIR deliberately resets the selected bond after confirmation.

## Menu and recovery

Press **M** to open or close the menu. **←/→** browse with wraparound and the
**lever** selects. Source also backs out. Volume and the sun key do nothing in
menus; menu/confirmation presses never send TV reports. Pairing reset needs a
separate lever confirmation; M or Source cancels it. Items are:

| # | Screen | Action |
| --- | --- | --- |
| 1 | LINK | Connect to the saved TV, or start pairing if none is saved. |
| 2 | PAIR | Confirm, then open a pairing window for the first incoming keyboard host. Forget the old accessory on the TV first. |
| 3 | OFF | Disconnect the keyboard and stop listening; the Ditoo stays powered on. |
| 4 | EXIT | Stop the app and open the native device menu. |
| 5 | BACK | Return to the remote. |

Each short press sends one automatically released report; long-down, repeat
and release events do not repeat actions. The five-second recovery hold opens
the native menu on firmware 306028. Brief action symbols (`>II`, `X`, `_`, `+`, `-`, `<`, `>`) and a
scrolling action label with **SENT** identify a submitted report, not measured
TV playback or volume state. The screen shows menu items, connection state,
pairing instructions for the TV, and save progress. It has no keybinding
prompts or persistent button guide.

The app selects keyboard-only mode automatically, including when reusing an
existing connection, unless Settings has a saved Bluetooth-mode override.
Keyboard-only mode closes native audio connections and prevents new ones.
If a link is still closing, wait briefly before retrying Pair. A TV may retain
the old speaker services in its accessory cache; forgetting the accessory and
pairing again refreshes that cache. The advertised name remains DitooPro-Audio.

Hold any keyboard key for five seconds to stop the app and return to stock
controls and the native menu. Hold a key during power-on to skip the app for
that boot. Power cycling starts the saved remote when Settings → Autostart is on. `lua uninstall` removes autostart; `lua start`
is only temporary and does not replace the saved app. See [storage and boot
recovery](lua-storage.md).

The developer messages `toggle`/`play_pause`, `mute`, `space`, `volume_up`,
`volume_down`, `left`, `right`, `menu`,
`pair`, `connect`, `listen`, `disconnect`, `status`, and
`target XX:XX:XX:XX:XX:XX` remain available.
`status` reports HID state and peer; `target` saves a peer without connecting.
`connect` initiates a connection to the saved TV; `listen` waits for that TV to
connect. These reuse its bond. Control commands also work with
`--transport ble --device DEVICE_MAC`; the TV remains the classic HID peer.
`menu` opens/closes the local menu. Ordinary use does not require these messages.
Settings use `TV4|peer|`. Loading TV2 or TV3 preserves the saved TV address
and discards the old chosen bindings. Controls are immediately usable even
when the old binding list was incomplete or malformed. Upgrading the app does
not reset the Bluetooth bond.

There is one resident Lua app. Replacing it preserves the Bluetooth profile
and connection; native code releases any outstanding key independently of Lua.
Connection attempts are limited to 32 per boot and one every 10 seconds.
Listening and pairing windows do not consume outgoing attempts. Pairing
approval on the TV is still performed on the TV itself.

## Idle links and reconnecting

Firmware 306027 preserves the keyboard link against two stock application
disconnect paths. A device trace caught `BtDisconnectCtrl` callers `0x1759c`
and `0x170a8` immediately before local HCI disconnects; the second closed both
open TV HID channels. The first belongs to the stock `uac divoom disconnect a2dp`
path. Both act on the whole Bluetooth link even with speaker profiles disabled.
This was an explicit native disconnect, not a missing HID keepalive.

The fix skips only these callers while keyboard-only mode is enabled on the
current Bluetooth stack. It also protects connection setup. Combined mode,
explicit keyboard OFF, stock power-off, link loss and HID error cleanup keep
their disconnect paths. Suppressed requests appear as `application_suppressed`
in the device trace; ordinary requests remain visible as `application`.

HID does not require periodic fake key reports. The Bluetooth HID 1.1.1
specification deprecates idle-rate commands; input is normally reported when it
changes. Its `HIDReconnectInitiate` behavior permits reconnecting when the owner
has new input. This app now does that while retaining passive listening between
presses. See [HID 1.1.1, sections 3.1.2.7–8 and 5.3.4.6](https://www.bluetooth.org/docman/handlers/downloaddoc.ashx?doc_id=309012).

Read [Bluetooth traces](bluetooth-trace.md) before restarting the device when a
link drops. A `local host terminated` HCI event establishes which endpoint issued
the final disconnect, but does not identify the caller or prove an idle timeout.
Pairing again is not needed for an ordinary dropped connection. Explicit OFF
remains an exception: use LINK to re-enable it.

[Recovery evidence](../firmware/tv-key-reconnect-evidence/verification.json)
records a forced disconnect from the real TV followed by reconnection from an
injected control-key callback, with one submitted and released report and no
new pairing. The previous app fails the corresponding native regression.
That earlier observation did not reproduce the original disconnect. The later
306027 diagnostic build identified the stock application callers described above.

The [306027 verification](../firmware/tv-persistent-link-evidence/verification.json)
records a BLE firmware update and automatic reconnection using the saved TV bond.
The same encrypted HID link survived a 12-minute interval with the laptop's BLE
connection closed, no input reports, and zero channel closures. The trace caught
the stock disconnect policy firing again at roughly eleven minutes and being
suppressed. Injected Volume Up/Down callbacks then submitted and released reports
without reconnecting. Explicit OFF still closed both channels, ignored control
keys, and allowed a subsequent LINK to reuse the bond. These are device-side
observations; no new TV-side input confirmation or physical power-off test was
performed. The ordinary startup app was restored after the temporary probe.

## Lua API

- `keyboard.mode('keyboard'|'combined')` (306025) queues a reversible profile
  change. Keyboard-only mode hides native A2DP, AVRCP, HFP/HSP and serial SDP
  records, disables their L2CAP registrations, closes existing audio/serial
  channels, and advertises keyboard class `0x000540`. HID, SDP, BLE and USB stay
  available. This applies to all classic peers; it is not a per-TV audio switch.
  `combined` restores the native records, registrations and previous class.
  Mode changes preserve HID connections and bonds. Like those connections, the
  mode survives Lua stop/failure; restore it explicitly or restart the Bluetooth
  stack/device. The TV app reapplies keyboard-only mode after stack recreation.
  Ticket success means queue acceptance: check `keyboard.status().keyboard_only`
  and `keyboard.status(true).audio_channels` for the asynchronous result.
- `keyboard.connect(address)` queues a connection to one explicit Bluetooth
  address. Reconnecting to the already connected peer succeeds without pairing.
- `keyboard.disconnect()` queues a HID disconnection and stops listening.
- `keyboard.pair([address[, seconds]])` advertises the keyboard and listens for
  the selected host. In 306022, omit the address (or pass nil) to select the
  first incoming HID control-channel peer. Other peers are then rejected; an
  interrupt channel alone cannot select a peer. The window defaults to 120 seconds and accepts 1..120.
  The previous radio access mode is restored on connection, expiration,
  explicit disconnect, or Lua stop/failure. If the stock radio was already
  discoverable, restoring it preserves that pre-existing state.
- `keyboard.bonds()` lists up to eight native paired addresses, without keys.
- `keyboard.forget(address)` removes only that bond. Disconnect all classic
  profiles first; an active underlying link is also rejected. The native bond
  file is saved under the existing write budget. The remote host may also need
  to forget its copy before pairing again. No bond is erased automatically.
- `keyboard.listen(address)` (306021) waits for incoming HID channels from
  one explicit peer without starting an outgoing connection. State 4 means
  listening; a partial incoming connection still has a 120-second timeout.
- `keyboard.tap(usage[, modifiers])` sends one USB HID Keyboard/Keypad usage
  (`4..231`), with an optional modifier bitmask (`0..255`). Space is `44`,
  Enter `40`, Escape `41`, Right `79`, Left `80`, Down `81`, Up `82`.
  Modifier bits follow HID: left Ctrl, Shift, Alt, GUI, then their right variants.
- `keyboard.media(name)` accepts `play_pause`, `mute`, `volume_up`,
  `volume_down`, `next`, `previous` or `stop`.
- `keyboard.status()` returns `enabled`, `connected`, `state`, `peer`, `busy`,
  `sent`, `released`, `errors`, `error`, and `bonds_saved`.
  State is 0 disconnected, 1 connecting, 2 connected, 3 disconnecting, or
  4 listening. 306021 adds `paired`, `encrypted`, `pairing`, `pair_remaining_ms`,
  `access_mode` and `forgotten`. `paired` means a local native bond exists;
  only `connected` establishes an active HID link.
  306025 adds `keyboard_only`; diagnostics also include `hidden_services`,
  `blocked_psms` and the remaining `audio_channels` (zero once drained).
  `keyboard.status(table)` reuses a table for frequent polling.
  `keyboard.status(true)` or `keyboard.status(table, true)` additionally fills
  `incoming`, `opened`, `closed`, `close_status`, `close_channel`, `control`,
  `authentication_state`, `encryption_state`, `key_type`, `security_mode`,
  and `ssp`. Reused tables retain fields not requested by the current call.

Mutations return a ticket or `nil,error`. Poll `device.result(ticket)` for
native queue acceptance, then status for connection/report outcome. A sent
report does not establish that a particular TV application acted on it.
Numeric `error` categories are setup/guard errors 4–19, send status `0x100+n`,
channel status `0x200+n`, link status `0x300+n`, transmit status `0x400+n`, and
connection-manager status `0x500+n`, and radio-access status `0x600+n`.
`errors` counts failures; a successful
connection clears the last error code.
Error 18 refuses an oversized or malformed SDP list before changing profiles;
19 refuses restoration if another registration has occupied a saved PSM slot.

Keyboard-only mode uses the pinned stock SDP remove/add routines at `0x12eb7a`
and `0x12eafa`. It retains native profile objects and callbacks: packet ownership
and disconnection cleanup remain with the stock stack. Removing the audio PSMs
from the eight-slot registration array blocks both new incoming requests and
native outgoing attempts, including peers with cached SDP. Up to eight existing
channels are drained through stock `L2CAP_DisconnectReq`; HID channels are skipped.
The mode adds 84 bytes inside the existing 8 KiB global reservation and allocates
no heap. Stack recreation invalidates all saved pointers before further use.

## Limits and recovery

The Bluetooth task owns static report buffers. Only one interrupt report may
be in flight. Each press schedules an all-zero release after 80 ms. Stopping,
replacing or aborting Lua advances its generation and requests release sooner.
A send stalled for one second closes the HID connection. No Lua-supplied
buffer, callback or pointer enters the Bluetooth stack. Stale queued commands
cannot target a new connection. The existing Lua instruction/time/heap limits
remain in effect. Native alarms still take priority over queued app operations.

HID setup uses two L2CAP PSM registrations, one SDP record and one connection
manager registration. They live until the stock Bluetooth stack is rebuilt;
the runtime detects that rebuild even if the allocator reuses its address.
Unknown or mismatched incoming peers are rejected. SSP level 2 is registered
for both PSMs, allowing the stock NoInputNoOutput
pairing method. HID channels are accepted as open only after native link
authentication and encryption succeed. Host-side requirements remain in place.

The SDP descriptor has an eight-byte keyboard report with ID 1 and a two-byte
Consumer usage report with ID 2. The HIDP transport prefixes each with `0xa1`.
Report protocol, GET_REPORT, GET_PROTOCOL and keyboard LED SET_REPORT are
handled. Boot protocol is explicitly unsupported. LED output is acknowledged
without overriding the Ditoo's RGB lights. There is no API for indefinitely
holding keys or for uploading arbitrary descriptors.

Stock firmware saves dirty link-key records only when both AVRCP and A2DP
are connected. HID-only operation additionally saves the same native dirty
records after a stable two-second HID connection, on the stock main task.
Unchanged records are not rewritten; saved settings share the existing
64-writes-per-boot cap and one-second minimum interval. `bonds_saved` counts
these additional native save calls. It does not disclose keys.

## Verification and reproduction

```sh
python3 scripts/test-lua-app-runtime.py
cargo test --locked
# Pair and connect the laptop as a keyboard host first. This grabs only the
# Ditoo's Bluetooth event node; test presses cannot reach desktop applications.
sudo python3 scripts/check-bluetooth-keyboard.py --device DEVICE_MAC --guard
python3 scripts/check-lua-app-device.py --transport usb --firmware 306022 \
  --output firmware/runs/keyboard-guards
```

The input test requires real Linux HID input events for Play/Pause (`164`),
Mute (`113`) and Space (`57`), including both press and release. The guard test
starts an infinite loop after a real Space press and requires its release and
a responsive USB error query. Raw Bluetooth captures and JSONL runs are ignored;
compact evidence belongs in `firmware/keyboard-evidence/`.

The lifecycle test requires a Linux host with classic Bluetooth HID support
(the `hidp` module or BlueZ's configured userspace HID backend). Run the silent
agent in a separate terminal, then exercise pairing and both connection
directions. `--fresh-pair` deliberately forgets only this laptop/Ditoo bond on
both sides and checks that other Ditoo bonds survive; omit it to test reuse
after a device reboot. The test replaces and stops the Lua app, never sends
keys, and uses USB for all Ditoo control.

```sh
python3 scripts/bluetooth-media-target.py DEVICE_MAC --seconds 180
python3 scripts/check-bluetooth-pairing.py --device DEVICE_MAC --fresh-pair
# Exercise Lua initiating fresh pairing as well:
python3 scripts/check-bluetooth-pairing.py --device DEVICE_MAC --fresh-pair --outgoing-pair
# After rebooting the Ditoo, with the agent still running:
python3 scripts/check-bluetooth-pairing.py --device DEVICE_MAC
```

The [306021 results](../firmware/keyboard-pairing-evidence/verification.json)
record fresh pairing initiated from either end, incoming/outgoing encrypted HID with audio and BLE
disconnected, connection reuse, targeted bond deletion, pairing-window expiry
and cleanup after a Lua infinite loop. The same saved bond was reused after
reflash/reboot. These extend the earlier key-input tests below; TV accessory
pairing and SmartTube input were not tested in that earlier run.

The committed [hardware results](../firmware/keyboard-evidence/verification.json)
cover all three real Linux key events, release after a Lua infinite loop, all
43 device guard/recovery checks, and a HID-only bond reused after a firmware
reflash/reboot without fresh pairing. A second build produced the identical
image. Native sanitizer tests also exercise the three physical key bindings,
packet ownership, stale commands, stack reconstruction, and bounded bond saves.
The TV connection returned to disconnected during that earlier handoff.
A later [306024 TV test](../firmware/bluetooth-trace-evidence/verification.json)
recorded successful fresh pairing, encryption and both HID channels opening.
The owner confirmed that the physical Play/Pause button pauses/resumes SmartTube.
The TV also connected the stock speaker profile, which the owner disabled on
the TV. Mute, Space and reconnection after a TV/Ditoo reboot were not checked
in that follow-up.

The [306025 TV results](../firmware/keyboard-only-evidence/verification.json)
verify five hidden native service records, four disabled audio/serial PSMs,
zero audio channels, A2DP/AVRCP state zero and a retained encrypted HID connection.
The owner confirmed sound returned to the TV and physical SmartTube Play/Pause
still worked. A USB-triggered firmware restart then autostarted the saved app
and reconnected to the TV using its existing bond, with keyboard-only mode
enabled and the same saved buttons. An infinite Lua loop hit the instruction
guard; USB and HID remained usable and audio stayed disabled. The original
remote was restored afterward. No laptop Bluetooth pairing was used for these
306025 checks. The native sanitizer suite covers reversible profile restoration;
combined-mode restoration was not exercised against the TV.

During the initial upgrade, the old saved app reconnected A2DP before it was
replaced. Installing the larger app while audio was streaming hit the unchanged
50 ms startup guard. A small temporary app enabled keyboard-only mode first,
then installation succeeded. The saved updated app started successfully on the
subsequent restart. The guard and heap limits have not been relaxed.

The [reconnection fix](../firmware/tv-reconnect-evidence/verification.json) was
installed over BLE on 306025 without changing firmware. Previously, losing a
Ditoo-initiated connection left native HID inactive: the TV could authenticate
with its saved bond but its incoming HID channel was rejected. The app now
resumes listening after link loss. The native runtime regression fails with the
old bundle and passes with the fix, including explicit OFF and subsequent Listen.
A live forced disconnect returned to listening, and the TV's Connect action
reused the existing pairing. Installation saved the corrected startup app with
native readback verification; a power cycle was not repeated in this test.
The owner confirmed the physical Play/Pause key pauses and resumes SmartTube
after installation and reconnection.
The TV did not automatically reconnect within 30 seconds of the forced
disconnect; listening allows incoming connections but does not initiate them.

The [expanded controls evidence](../firmware/tv-controls-evidence/verification.json)
records the previous revision's seven report usages, menu and reconnect checks,
and BLE installation. That revision asked for eight button assignments. The
[current fixed-controls evidence](../firmware/tv-fixed-controls-evidence/verification.json)
covers removal of that setup, the stock ADC map, migration to peer-only settings,
and live callback dispatch over BLE. The owner confirmed that the updated remote
worked. Individual-key behavior was not separately reported, and the Linux input
harness was not run against the laptop.

The profile follows the [Bluetooth HID 1.1.1 specification](https://www.bluetooth.com/specifications/specs/hid-1-1-1/).
The related [vendor SDK](https://github.com/leadercxn/bp1048_sdk_v0.1.12/tree/8105bd864b04995d81c9f9ae77cb158259f39015/MVsB1_Base_SDK/middleware/bluetooth)
contains HID setup code omitted from the Ditoo link. This implementation supplies
that profile in C, using function and structure layouts checked against the
pinned stock binary; it does not link SDK binary objects.

The standalone test additionally drives the actual app's key callbacks through
a temporary message wrapper, uses the fixed physical key IDs and local menu to
pair a fresh Linux host, checks real input press/release events, and disconnects
and reconnects. This replaces shared app settings and forgets only the laptop
bond. It requires an active scoped pairing agent and permission to grab the
Ditoo event node. Physical ADC routing is tested by the native sanitizer suite.
The current harness includes Volume Up/Down and Left/Right as well as the
original Play/Pause, Mute and Space; older committed Linux evidence covers the
original three actions. The TV can be tested directly using BLE control without
pairing the laptop as an input host.

```sh
sudo python3 scripts/check-standalone-remote.py --device DEVICE_MAC
# Reboot the Ditoo with the original app installed, then observe without uploading:
python3 scripts/check-standalone-remote.py --device DEVICE_MAC --after-reboot
```

See [306022 device evidence](../firmware/standalone-evidence/verification.json).
