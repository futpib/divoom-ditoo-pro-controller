# USB device control

Custom firmware **306019** exposes the native Divoom command protocol through
USB vendor HID. Stock firmware and earlier custom images still support USB
flashing, but require installing 306019 before normal USB control works.

```sh
cargo build --locked --release
target/release/divoom-ditoo-pro-controller --transport usb firmware-update firmware/306019-lua.MVA
target/release/divoom-ditoo-pro-controller --transport usb devices
target/release/divoom-ditoo-pro-controller --transport usb firmware
target/release/divoom-ditoo-pro-controller --transport usb brightness 20
target/release/divoom-ditoo-pro-controller --transport usb lua start examples/lua/tv-remote.lua
target/release/divoom-ditoo-pro-controller --transport usb lua send 'connect 0C:CD:B4:D0:0C:26'
target/release/divoom-ditoo-pro-controller --transport usb lua send status
```

Replace the TV address for another device. `--usb-port 1-6` selects a physical
USB port and is global: it works before or after any subcommand. Omit it when
one Ditoo is attached. No Bluetooth address, pairing, BlueZ connection or clock
synchronization is performed by USB control. `auto` retains RFCOMM/BLE selection.
See [USB permissions and recovery](usb.md).

The shared transport covers firmware/status/settings queries, display images,
animations/text/video, clocks, games and virtual keys, tools, audio controls,
alarms, raw commands/scripts/monitoring, and all Lua operations including
8 KiB source uploads and bidirectional app messages. Lua's Bluetooth controls
are unchanged: USB can manage the app while the Ditoo's Bluetooth talks to a TV.
`scan` and `devices` list attached USB devices when USB is selected.

This transports the existing native protocol; catalogue entries unsupported by
the Ditoo firmware do not gain an implementation. It does not add Bluetooth HID,
Lua filesystem access, an audio codec, or missing native features. Selecting
another native audio input can re-enumerate USB (`1719` versus `171e`); reconnect
the CLI after enumeration. In particular, Bluetooth audio selection is separate
from selecting USB as the **controller** transport.

Firmware 306028 adds **Settings → USB mode → Charge and control**, which removes
the USB audio interfaces while retaining this command bridge and flashing.
See the [device menu](device-menu.md).

## Wire format and lifetime

The application vendor HID has no interrupt endpoints. The host uses serialized
256-byte EP0 SET_REPORT(output) and GET_REPORT(input), class/interface requests
`21/09/027d` and `a1/01/017d`. The reserved report selector `7d` distinguishes the
bridge from stock audio-tuning reports. Firmware-entry **feature reports are
never used for control or discovery**. The host validates the existing HID
interface descriptor and a read-only capability reply before its first output.
Older firmware's input reply fails that check without receiving a command.

All multibyte fields are little endian. Both directions start with `DUSB`, ABI
byte `1`, and have a 32-byte header followed by at most 224 payload bytes.

| # | Offset | Output | Input |
| --- | --- | --- | --- |
| 1 | 5 | Operation: 1 open, 2 command fragment, 3 consume replies, 4 close | Status: 0 success; 1 malformed; 2 reserved busy; 3 heap shortage; 4 overflow; 5 wrong session; 6 bad native frame |
| 2 | 6 | Reserved | Flags: bit 0 session open, bit 1 mailbox pending |
| 3 | 8 | Nonzero host session ID, u32 | Active session ID, u32 |
| 4 | 12 | Request sequence, u32 | Last completed request sequence, u32 |
| 5 | 16 | Total command frame bytes, u16 | Partial command bytes received, u16 |
| 6 | 18 | Fragment offset, u16 | Partial command total, u16 |
| 7 | 20 | Fragment byte count, u16 | Maximum command frame bytes: 4100 |
| 8 | 22 | Reserved | Capability bits: bit 0 native command bridge |
| 9 | 24 | Consumed response stream cursor, u32 | First returned response stream cursor, u32 |
| 10 | 28 | Reserved | Response chunk byte count, u16 |
| 11 | 30 | Reserved | Response ring bytes: 8192 |

Open uses sequence 1 and a new session ID. Subsequent requests increment by one.
The host waits for the completed sequence and cleared pending flag before the
next output. Repeating the last sequence never executes it twice. The CLI does
not automatically retry uncertain mutating writes. Native frames retain their
`01 / length / opcode / payload / checksum / 02` envelope; firmware validates
length, checksum, offsets and bounds before entering the stock dispatcher.
The stock command length limit is 4096, including opcode and checksum, or 4100
wire bytes. Replies can add two bytes to the corresponding command size.

Input reports do not consume data. The host acknowledges the absolute stream
cursor in its next output, making repeated reads harmless. Multiple native
replies and asynchronous events share the bounded stream. Non-ACK native
notifications are represented in the controller's response model with their
command as the original opcode. Ring/host queue overflow produces an explicit
error; responses are not silently discarded to make room.

The USB callback only fills a one-report mailbox or snapshots the reply ring.
Allocation and command dispatch run on the native main task. An independent
376-byte stock-layout context lets USB work immediately after boot, without
creating a Bluetooth connection. The existing native dispatcher handles every
opcode. Its synchronous event handlers finish before the command buffer is
reused. A reply hook mirrors native replies without disabling Bluetooth;
concurrent controllers can observe each other's notifications, so serialize
writes to the same device. One process exclusively claims the vendor interface.
The notification timer recognizes USB sessions without faking BLE state.

A session in firmware 306028 allocates an 8 KiB reply ring. Command assembly
allocates only the current frame (up to 4,100 bytes) and releases it after
dispatch; earlier versions retained all 12,292 bytes for the whole session.
Both allocations preserve at least another 24 KiB of stock heap. This leaves
4,100 more bytes available between commands, including while starting Lua.
Close releases both allocations, including a partially received command;
15 seconds without USB activity also releases it after a crashed host/unplug.
The host polls during long raw-script delays to keep the session alive and
preserve pending events.
The bridge never enters Lua from an interrupt, changes Lua's instruction/time
limits, or writes flash. Lua apps continue after the CLI disconnects. Raw
commands retain the same native firmware semantics and risks as Bluetooth raw
commands; transport validation is not a sandbox for arbitrary native opcodes.

Firmware hook sites in the pinned 306007 code: receive call `0x7a27e`, send call
`0x7a29c`, reply entry `0x38f00`, and app-notification connection check `0x387b2`.
Dispatch enters `0x3998c` with the original stack/register layout, bypassing
Bluetooth-only envelope handling. The bootloader and feature-report updater
entry are unchanged.

## Reproduce validation

```sh
python3 scripts/build-lua-app-runtime.py
python3 scripts/test-lua-app-runtime.py
cargo test --locked
cargo build --locked --release
python3 scripts/check-lua-app-device.py --transport usb --usb-port 1-6 \
  --output firmware/runs/usb-lua-check
python3 scripts/check-usb-control-device.py --usb-port 1-6 \
  --output firmware/runs/usb-control-check
```

Both hardware scripts stop the current Lua app. The USB smoke test changes the
screen temporarily and restores volume, brightness, hour-format setting and the
original clock face. It does not modify alarms or power schedules. Screen packet
delivery is checked through native dispatch, not an optical comparison.

Compact results are in [USB control evidence](../firmware/usb-control-evidence/verification.json).
Raw logs, traces and request arrays remain in ignored `firmware/runs/`.

The live scoreboard setter was acknowledged but its query still reported zero
scores. A comparison using BLE produced the identical result. This pre-existing
native behavior is recorded, not counted as a verified score-setting feature.
