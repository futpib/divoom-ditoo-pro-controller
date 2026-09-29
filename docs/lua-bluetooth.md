# Lua Bluetooth media connections

Firmware 306017 exposes the stock classic Bluetooth AVRCP connection API to Lua.
306018 adds a mute toggle through the Bluetooth task queue.
The computer can use the existing BLE control service or, with 306019,
[USB control](usb-control.md) while leaving Bluetooth to the remote peer. This does not add HID,
Bluetooth scanning, arbitrary sockets, custom GATT services or a general pairing
agent. The native stack handles link security; its stock bond persistence has an
A2DP-dependent limitation described below; a remote device may require confirmation.

| # | API | Behavior |
| --- | --- | --- |
| 1 | `bluetooth.status()` | `available`, `ble_connected`, `media_state`, `audio_state`, `media_connected`, `audio_connected`, `access_mode`, and `peer` when a media/audio profile is connected. |
| 2 | `bluetooth.connect_media('XX:XX:XX:XX:XX:XX')` | Queue a native outgoing AVRCP connection to a public classic Bluetooth address. Returns a ticket. |
| 3 | `bluetooth.disconnect_media()` | Queue AVRCP disconnection. Does not disconnect BLE or explicitly disconnect the audio profile. |
| 4 | `bluetooth.media('play'|'pause')` | Queue an AVRCP command, requiring a connected media profile. Returns a ticket. |
| 5 | `bluetooth.mute()` | 306018: queue AVRCP MUTE (`0x43`) press and release. A mute/unmute toggle, with no remote mute-state query or separate idempotent unmute command. |

Poll `device.result(ticket)` for native dispatch completion. Success means the
stock command queue accepted the request, **not** that pairing completed or the
TV obeyed a key. Check `bluetooth.status().media_connected` and `peer` separately.
Native media state 2 means connected; audio state 2 is connected and 3 is
streaming. `peer` identifies the native manager's shared classic media peer,
not the computer using BLE.

The native stack can connect related profiles automatically. An AVRCP connection
is therefore not a promise that TV sound will stay on the TV. For a keyboard
connection, use the separate [HID API](lua-keyboard.md) added in 306020.

## Example

```sh
divoom-ditoo-pro-controller --transport ble lua start examples/lua/tv-remote.lua
divoom-ditoo-pro-controller --transport ble lua send 'connect XX:XX:XX:XX:XX:XX'
divoom-ditoo-pro-controller --transport ble lua send status
divoom-ditoo-pro-controller --transport ble lua status
```

Use the TV's actual Bluetooth address and open its accessory pairing screen.
The app selects Bluetooth input and submits one outgoing connection attempt. It
reports a timeout after 20 seconds without claiming the native attempt was
cancelled. There is no automatic retry loop. Startup sends no play command.
The example requires 306018. The first physical key press binds play/pause;
the first different key binds mute/unmute. Binding sends nothing. Subsequent
presses alternate pause/play on the first key, and send MUTE on the second.
Hold/release/repeat events and other keys do nothing. Mute does not change the
play/pause sequence. The M icon confirms submission, not the TV mute state.
This is local state: `audio.status().playing` does not reflect TV playback on an
AVRCP-only connection. Other TV controls can put the sequence out
of sync; `lua send play` or `lua send pause` sets it explicitly.
`lua send toggle` and `lua send mute` exercise those paths; `bind` resets both
bindings and `disconnect` requests AVRCP disconnection. Reloading the app requires selecting
the target and binding both buttons again; an existing native connection is reused.

The outgoing connection produced a pairing prompt on the test Google TV, even
though the Ditoo did not appear in its accessory search list. After accepting it,
the Ditoo reported AVRCP state 2, A2DP state 0 and BLE connected while answering
computer commands. That verifies separate TV and computer connections without
an active TV audio profile. The owner reported that SmartTube ignored play/pause.
Pairing and command submission are not evidence of working TV controls.

The Android AVRCP target implementation gates most passthrough key events on
`IsActive()`, which compares the peer with the active A2DP audio device. Play has
a special path that tries to select that audio device. This is a likely cause of
the AVRCP-only TV failure; the TV's own implementation/logs were not inspected.
See [Android's device handler](https://android.googlesource.com/platform/packages/modules/Bluetooth/+/refs/heads/main/system/profile/avrcp/device.cc).
HID consumer-control reports use the keyboard/input path. Firmware 306020 and
later provide a separate [keyboard HID API](lua-keyboard.md) and a three-button
example with Play/Pause, Mute and Space. A literal Space key cannot be sent over
this AVRCP API; the AVRCP example has two bindings.

## Computer BLE selection

On a dual-mode device, BlueZ's generic `Device1.Connect` can select classic
Bluetooth and take the audio connection. The CLI uses `Bearer.LE1` to check,
connect and disconnect only LE, preserving a concurrent classic connection.
On older BlueZ it falls back to `Adapter1.ConnectDevice` and waits for GATT
services. These experimental APIs require BlueZ's
`Experimental = true` setting in `/etc/bluetooth/main.conf`; restart Bluetooth
after changing it, when it is safe to disconnect existing host devices.
The test host uses BlueZ 5.87 with this setting enabled. The CLI reports an error
if explicit bearer selection is unavailable for a known classic/audio device.
LE-only discovery retains compatibility with older BlueZ versions.
When per-bearer disconnect is unavailable, cleanup leaves a dual-mode device
connected rather than disconnecting unrelated classic profiles.

## Laptop media test

The silent test target registers a real BlueZ media player. It records the media
methods delivered by BlueZ without playing audio. It temporarily makes the
adapter discoverable/pairable, accepts only the selected Ditoo in its pairing
agent, and restores those adapter settings on exit. Requires Python D-Bus and
PyGObject.

```sh
python3 scripts/bluetooth-media-target.py DEVICE_MAC --seconds 180
# In another terminal, with tv-remote.lua running:
divoom-ditoo-pro-controller --transport ble lua send disconnect
# Wait for media_state=0, then select the laptop's adapter address:
divoom-ditoo-pro-controller --transport ble lua send 'connect LAPTOP_MAC'
divoom-ditoo-pro-controller --transport ble lua send pause
divoom-ditoo-pro-controller --transport ble lua send play
```

A stale Ditoo/laptop bond may require pairing again from the laptop. The helper
does not remove bonds. Capture with `btmon` to distinguish native queue acceptance
from real AVRCP traffic and host player delivery.

On the test laptop, the Ditoo sent PAUSE press/release (`0x46`/`0xc6`) and PLAY
press/release (`0x44`/`0xc4`). BlueZ accepted all four packets and invoked the test
player's `Pause` and `Play` methods. BLE control continued on a separate ACL
handle, with A2DP state 0. This verifies the Ditoo's command path, not TV behavior.

Firmware 306018 was also tested with `lua send pause`, two `lua send mute`
commands one second apart, then `lua send play`. The laptop accepted both MUTE
press/release pairs (`0x43`/`0xc3`); its `DitooPro-Audio (AVRCP)` input device
emitted `EV_KEY KEY_MUTE` (113), values 1 then 0, twice. Pause/Play still reached
the silent player. BLE remained connected and A2DP stayed disconnected.

For the input test, the Ditoo's specific Linux input device was exclusively
grabbed with `EVIOCGRAB`, preventing desktop mute shortcuts while capturing
`input_event` records. To reproduce with `evtest`, select the Ditoo AVRCP node
from `/proc/bus/input/devices` and start `sudo evtest --grab /dev/input/eventN`
**before** sending mute. Close the monitor afterward to release the grab.
The laptop's volume and mute setting were unchanged. BlueZ's
[input mapping](https://github.com/bluez/bluez/blob/5.87/profiles/audio/avctp.c)
explains why mute arrives as an input event rather than a player method.
[306018 verification](../firmware/lua-mute-evidence/verification.json) records
this test, the USB readback and the 43 passing on-device runtime checks.
The TV was reselected afterward. The owner accepted pairing but reports that
none of these controls work on the TV, including mute. Keep this as a verified
AVRCP transport API, not a working remote for this TV.

## Bounds and native bindings

Requests copy values into the existing single native job slot. The stock main
task dispatches at most one job per 100 ms; no Bluetooth wait runs on the Lua
worker. Connection attempts require 16 KiB of native heap, are separated by ten
seconds, and are limited to 32 per boot. Invalid, zero and broadcast addresses
are rejected. A connected/connecting media peer with a different address is not
replaced. Requests still waiting in the job slot are cancelled on app release;
already-submitted native play/pause requests, connections and saved bonds persist.
Mute also checks the app generation and original peer when the Bluetooth task
consumes its copied payload, dropping requests after app release or peer change.
It reserves room for both press and release in the native panel-key queue; a
saturated queue drops the whole toggle. Ticket success only means the first
queue accepted it, not that the panel queue or remote accepted it.

The stock 306007 command wrappers are pinned in the builder:

Native address arrays store the least significant byte first. The Lua boundary
reverses bytes when parsing and formatting conventional Bluetooth addresses.
This was checked against the connected computer's independently known address.

| # | Function/data | Address |
| --- | --- | --- |
| 1 | `AvrcpConnect` | `0x1139b0` |
| 2 | `AvrcpDisconnect` | `0x1139c0` |
| 3 | `AvrcpCtrlPlay` / `AvrcpCtrlPause` | `0x1139ce` / `0x1139e8` |
| 4 | `AddBtUserCommand` | `0x138b74`, copies the six address bytes into its queue |
| 5 | AVRCP / A2DP state getters | `0x7c954` / `0x7bac0` |
| 6 | Native manager | `gp + 0x5104`; shared peer at `+0xe0` |
| 7 | BLE connected flag | `gp - 30420`, maintained by stock connection callbacks |
| 8 | Command peek hook / original | `0x138c76` / `0x138bf6`; private command `0x80`, stock default path pops it |
| 9 | `AVRCP_SetPanelKey` | `0x11c52e`; Bluetooth-task-only call, instance at context `+0x9c`, ring indices `+0x2ba`/`+0x2bb` |

The related [SDK AVRCP header](https://github.com/leadercxn/bp1048_sdk_v0.1.12/blob/8105bd864b04995d81c9f9ae77cb158259f39015/MVsB1_Base_SDK/middleware/bluetooth/inc/bt_avrcp_api.h)
helps name these functions; bindings are checked against the actual Ditoo
binary rather than assuming every SDK structure matches.

## Concurrency test

```sh
python3 scripts/check-bluetooth-concurrency.py DEVICE_MAC \
  --output firmware/runs/bluetooth-concurrency
```

This opens a BLE command session and a classic SDP socket concurrently. It
requires successful SDP replies and continued BLE replies while the classic
socket remains open. It does not pair, play audio or change the running app.
Both links terminate on the computer; that test alone does not establish a
working TV remote. For independent transport confirmation, capture with
`sudo btmon -i 0 -w firmware/runs/concurrency.btsnoop` and check the LE-ACL and
BR-ACL handles. Raw traces remain ignored.

## Pairing persistence investigation

Bluetooth can reuse a saved bond without another pairing prompt. The stock
306007 routine at `0x7e4d4` writes dirty pairing records only after **both A2DP
and AVRCP reach connected state**: A2DP gate `0x7e4e0`, AVRCP gate `0x7e4ec`,
dirty flag `stock_bt_manager[0]` at `0x7e510`, save call `0x7d570` at `0x7e52a`.
This matches [`BtLinkStateConnect` in the related SDK](https://github.com/leadercxn/bp1048_sdk_v0.1.12/blob/8105bd864b04995d81c9f9ae77cb158259f39015/MVsB1_Base_SDK/middleware/bluetooth/src/bt_manager.c).
The exact stock instructions are retained in [the disassembly](firmware-analysis/306007-usb-control.nds32.S).
Our TV experiments had AVRCP state 2 and A2DP state 0. A new key retained only
in RAM can therefore be lost at reboot/firmware installation, explaining the
repeated-pairing symptom without implying that AVRCP inherently requires it.
This is a source-backed explanation; persistent TV reconnection has not yet
been verified. The USB bridge preserves flash partitions and does not clear
bonds. A separate fix must persist authenticated AVRCP-only bonds without
pretending an A2DP connection exists or writing on every reconnect.

Firmware 306020 implements that dirty-record flush for **HID-only** connections;
see [Bluetooth keyboard support](lua-keyboard.md). AVRCP-only connections retain
the stock persistence behavior described above.
