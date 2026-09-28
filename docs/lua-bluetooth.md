# Lua Bluetooth media connections

Firmware 306017 exposes the stock classic Bluetooth AVRCP connection API to Lua.
The computer still uses the existing BLE control service. This does not add HID,
Bluetooth scanning, arbitrary sockets, custom GATT services or a general pairing
agent. The native stack handles link security and stores bonds when pairing
succeeds; a remote device may require confirmation.

| # | API | Behavior |
| --- | --- | --- |
| 1 | `bluetooth.status()` | `available`, `ble_connected`, `media_state`, `audio_state`, `media_connected`, `audio_connected`, `access_mode`, and `peer` when a media/audio profile is connected. |
| 2 | `bluetooth.connect_media('XX:XX:XX:XX:XX:XX')` | Queue a native outgoing AVRCP connection to a public classic Bluetooth address. Returns a ticket. |
| 3 | `bluetooth.disconnect_media()` | Queue AVRCP disconnection. Does not disconnect BLE or explicitly disconnect the audio profile. |
| 4 | `bluetooth.media('play'|'pause')` | Queue an AVRCP command, requiring a connected media profile. Returns a ticket. |

Poll `device.result(ticket)` for native dispatch completion. Success means the
stock command queue accepted the request, **not** that pairing completed or the
TV obeyed a key. Check `bluetooth.status().media_connected` and `peer` separately.
Native media state 2 means connected; audio state 2 is connected and 3 is
streaming. `peer` identifies the native manager's shared classic media peer,
not the computer using BLE.

The native stack can connect related profiles automatically. An AVRCP connection
is therefore not a promise that TV sound will stay on the TV. This implementation
does not yet provide a remote-only HID connection.

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
The first physical key press selects the button; subsequent down events on that
button alternate pause and play, starting with pause. Hold/release/repeat events
do not toggle. This is local state: `audio.status().playing` does not reflect TV
playback on an AVRCP-only connection. Other TV controls can put the sequence out
of sync; `lua send play` or `lua send pause` sets it explicitly.
`lua send toggle` exercises the same path; `bind` chooses another button and
`disconnect` requests AVRCP disconnection. Reloading the app requires selecting
the target and binding the button again; an existing native connection is reused.

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
HID consumer-control reports use the keyboard/input path, but this firmware does
not yet implement a HID profile.

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

## Bounds and native bindings

Requests copy values into the existing single native job slot. The stock main
task dispatches at most one job per 100 ms; no Bluetooth wait runs on the Lua
worker. Connection attempts require 16 KiB of native heap, are separated by ten
seconds, and are limited to 32 per boot. Invalid, zero and broadcast addresses
are rejected. A connected/connecting media peer with a different address is not
replaced. Requests still waiting in the job slot are cancelled on app release;
already-submitted native requests, connections and saved bonds persist.

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
