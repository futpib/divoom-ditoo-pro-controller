# Standalone TV keyboard remote

For pairing failures, firmware 306024 adds [device-side Bluetooth tracing](bluetooth-trace.md) over USB without stopping this app.

Firmware **306022** runs the remote directly on the Ditoo. After one installation,
setup, pairing, reconnection and ordinary use need only the Ditoo and TV.
Bluetooth HID sends standard Play/Pause, Mute and Space reports. AVRCP and the
older `tv-remote.lua` remain available separately.

```sh
# One-time setup from a computer.
divoom-ditoo-pro-controller --transport usb firmware-update firmware/306022-lua.MVA
divoom-ditoo-pro-controller --transport usb lua install examples/lua/tv-keyboard.lua
```

## On the Ditoo and TV

1. Power on the Ditoo. It starts the saved remote automatically.
2. Follow the four screen prompts: **PLAY**, **MUTE**, **SPC**, **MENU**.
   Press a different keyboard key for each role. These setup presses are silent;
   choose whichever layout feels natural. The assignments are saved.
3. On the TV, forget the old **DitooPro-Audio** accessory if it exists, then open
   **Pair accessory**. This removes the cached audio-only profile and stale bond.
4. On the Ditoo, press your Play key and confirm pairing with Play again.
   If a target is already saved, use Menu → Pair → Play to confirm instead.
   Choose **DitooPro-Audio** on the TV and accept the request. No MAC address is
   needed. A blue bar shows the two-minute pairing window.
5. **TV / READY** with a green dot means the keyboard connection is established.
   Play, Mute and Space now send their corresponding keys. A sent report does
   not prove that every TV app handles it. SmartTube Play/Pause was confirmed
   by the owner on the test TV with firmware 306024; Mute and Space still need
   TV-specific verification.

The top line shows the current step; the lower line scrolls its instructions.
The remote remembers both the key assignments and the connected TV. Later boots
try the saved TV up to three times, then listen for it. If necessary use Connect
in the menu or the TV's Connect action. Reconnection reuses the saved bond;
Pair deliberately resets the selected bond after confirmation.

## Menu and recovery

Press **Menu** to open the menu. Menu (or Space) advances, Play selects, and
Mute goes back. Items are:

| # | Screen | Action |
| --- | --- | --- |
| 1 | LINK | Connect to the saved TV, or start pairing if none is saved. |
| 2 | PAIR | Confirm, then open a pairing window for the first incoming keyboard host. Forget the old accessory on the TV first. |
| 3 | KEYS | Choose the four button assignments again. |
| 4 | OFF | Disconnect the keyboard and stop listening; the Ditoo stays powered on. |
| 5 | BACK | Return to the remote. |

If the screen asks to disconnect other audio, disconnect Ditoo on the phone,
computer or TV that currently has its audio connection, then try Pair again.
The remote can close its HID and AVRCP connections; it does not forcibly remove
another device's A2DP audio connection. A native link can take a moment to close.

Hold any keyboard key for five seconds to stop the app and return to stock
controls. Hold a key during power-on to skip the app for that boot. Power cycling
starts the saved remote again. `lua uninstall` removes autostart; `lua start`
is only temporary and does not replace the saved app. See [storage and boot
recovery](lua-storage.md).

The developer messages `toggle`/`play_pause`, `mute`, `space`, `bind`, `menu`,
`pair`, `disconnect`, `status`, and `target XX:XX:XX:XX:XX:XX` remain available.
`status` reports HID state and peer; `target` saves a peer without connecting.
Ordinary use does not require these messages.

There is one resident Lua app. Replacing it preserves the Bluetooth profile
and connection; native code releases any outstanding key independently of Lua.
Connection attempts are limited to 32 per boot and one every 10 seconds.
Listening and pairing windows do not consume outgoing attempts. Pairing
approval on the TV is still performed on the TV itself.

## Lua API

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
  `keyboard.status(table)` reuses a table for frequent polling.
  `keyboard.status(true)` or `keyboard.status(table, true)` additionally fills
  `incoming`, `opened`, `closed`, `close_status`, `close_channel`, `control`,
  `authentication_state`, `encryption_state`, `key_type`, `security_mode`,
  and `ssp`. Reused tables retain fields not requested by the current call.

Mutations return a ticket or `nil,error`. Poll `device.result(ticket)` for
native queue acceptance, then status for connection/report outcome. A sent
report does not establish that a particular TV application acted on it.
Numeric `error` categories are setup/guard errors 4–17, send status `0x100+n`,
channel status `0x200+n`, link status `0x300+n`, transmit status `0x400+n`, and
connection-manager status `0x500+n`, and radio-access status `0x600+n`.
`errors` counts failures; a successful
connection clears the last error code.

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

The profile follows the [Bluetooth HID 1.1.1 specification](https://www.bluetooth.com/specifications/specs/hid-1-1-1/).
The related [vendor SDK](https://github.com/leadercxn/bp1048_sdk_v0.1.12/tree/8105bd864b04995d81c9f9ae77cb158259f39015/MVsB1_Base_SDK/middleware/bluetooth)
contains HID setup code omitted from the Ditoo link. This implementation supplies
that profile in C, using function and structure layouts checked against the
pinned stock binary; it does not link SDK binary objects.

The standalone test additionally drives the actual app's key callbacks through
a temporary message wrapper, learns four buttons, uses the on-screen menu to
pair a fresh Linux host, checks real input press/release events, and disconnects
and reconnects. This replaces shared app settings and forgets only the laptop
bond. It requires an active scoped pairing agent and permission to grab the
Ditoo event node. Physical ADC routing is tested by the native sanitizer suite.

```sh
sudo python3 scripts/check-standalone-remote.py --device DEVICE_MAC
# Reboot the Ditoo with the original app installed, then observe without uploading:
python3 scripts/check-standalone-remote.py --device DEVICE_MAC --after-reboot
```

See [306022 device evidence](../firmware/standalone-evidence/verification.json).
