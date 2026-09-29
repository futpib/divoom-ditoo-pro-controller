# Bluetooth keyboard from Lua

Firmware **306020** adds a native classic Bluetooth HID device to the existing
AVRCP, BLE and USB support. The Ditoo sends standard keyboard and Consumer
Control reports directly to the paired host. No laptop relay is involved.

```sh
python3 scripts/build-lua-app-runtime.py
cargo build --locked --release
divoom-ditoo-pro-controller --transport usb firmware-update firmware/306021-lua.MVA
divoom-ditoo-pro-controller --transport usb lua start examples/lua/tv-keyboard.lua
divoom-ditoo-pro-controller --transport usb lua send 'connect XX:XX:XX:XX:XX:XX'
```

Put the TV in its Bluetooth accessory screen and accept pairing if requested.
A host that cached the old audio-only service list may need to rediscover the
Ditoo. The device name remains `DitooPro-Audio`; the new SDP service is `Ditoo
Keyboard` (UUID `0x1124`). The profile advertises keyboard class after it starts.

Firmware **306021** adds explicit pairing, bond inspection/removal, encrypted
channel checks and a 120-second connection deadline. It also supports
`lua send 'listen XX:XX:XX:XX:XX:XX'`:
it waits for the selected host to initiate HID and resumes listening after
that host disconnects. `disconnect` stops listening. Use `keyboard.status(true)`
for native incoming/open/close counters, the last
close status and PSM, and authentication/encryption diagnostics. These describe
the native stack, not a TV app result.

The first three **different** physical keys bind Play/Pause, Mute and Space,
in that order. Binding does not send a key. Subsequent presses send the bound
action; release and hold events are ignored. `lua send toggle`, `mute` and
`space` also work. `lua send bind` resets bindings; `status` reports the peer,
connection state and press/release counters. Its compact status message contains
`state peer error sent released bonds_saved`. `disconnect` closes HID. The old
`tv-remote.lua` and all `bluetooth.*` AVRCP functions remain available.

There is one resident Lua app. Replacing it preserves the Bluetooth profile
and connection, while its outstanding key is released independently of Lua.
To switch hosts, disconnect HID and any audio/media link to the previous host.
A stock link may remain alive briefly after its profile disconnects; wait for
it to close before retrying. Connection attempts are limited to 32 per boot
and one every 10 seconds; listening, pairing windows and reusing a connection
do not consume outgoing attempts. Selecting Bluetooth as the native audio source is
necessary for the stock radio task; the TV script does that before connecting.

For autonomous startup, set `local target` in the example to the host's address.
It reuses a connection to that peer, or connects and retries up to three times
at 15-second intervals before falling back to listening. Without a configured
target it adopts an existing HID connection or waits for a message. Use
`lua send 'pair XX:XX:XX:XX:XX:XX'` to open a 120-second pairing window, then
select the Ditoo on the host. `lua send bonds` lists saved addresses;
`lua send forget` removes the selected target after disconnection. The app
does not erase bonds automatically. A host can still require its own pairing
confirmation; Lua cannot approve the TV's user interface.

A first outgoing connection can finish pairing before the host finishes HID
service discovery. BlueZ rejected that initial connection as an unknown input
device during testing. A retry reuses the newly paired link key; the app's
bounded retry handles this case without deleting the bond.

## Lua API

- `keyboard.connect(address)` queues a connection to one explicit Bluetooth
  address. Reconnecting to the already connected peer succeeds without pairing.
- `keyboard.disconnect()` queues a HID disconnection and stops listening.
- `keyboard.pair(address[, seconds])` advertises the keyboard and listens for
  the selected host. The window defaults to 120 seconds and accepts 1..120.
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
python3 scripts/check-lua-app-device.py --transport usb --firmware 306021 \
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
pairing and SmartTube input still require a test on that TV.

The committed [hardware results](../firmware/keyboard-evidence/verification.json)
cover all three real Linux key events, release after a Lua infinite loop, all
43 device guard/recovery checks, and a HID-only bond reused after a firmware
reflash/reboot without fresh pairing. A second build produced the identical
image. Native sanitizer tests also exercise the three physical key bindings,
packet ownership, stale commands, stack reconstruction, and bounded bond saves.
The TV connection returned to disconnected during handoff; TV pairing and
SmartTube behavior remain unverified. Linux input success does not establish
compatibility with that TV.

The profile follows the [Bluetooth HID 1.1.1 specification](https://www.bluetooth.com/specifications/specs/hid-1-1-1/).
The related [vendor SDK](https://github.com/leadercxn/bp1048_sdk_v0.1.12/tree/8105bd864b04995d81c9f9ae77cb158259f39015/MVsB1_Base_SDK/middleware/bluetooth)
contains HID setup code omitted from the Ditoo link. This implementation supplies
that profile in C, using function and structure layouts checked against the
pinned stock binary; it does not link SDK binary objects.
