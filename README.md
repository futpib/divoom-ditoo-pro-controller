# divoom-ditoo-pro-controller

A CLI tool to control a Divoom Ditoo Pro over Bluetooth (SPP/RFCOMM or BLE GATT).
The original app from the vendor is proprietary; this project reverse-engineers the protocol.

# Features

- **Display**: send images (PNG, JPEG, GIF, BMP, WebP), animations (Divoom 16x16 format), video (anything mpv can play, including YouTube URLs)
- **Text**: scrolling and static text with custom fonts, colors, and alignment
- **Audio**: get/set volume, play/pause SD card music
- **Settings**: brightness, clock face, date/time, language, keyboard backlight, alarm, display mode (light/hot/special/music)
- **Conversion**: convert between Divoom 16x16 and GIF formats
- **Device discovery**: scan for devices, list paired Ditoo Pro devices, auto-detect when only one is paired

# Blog post

Bluetooth Speaker with 16x16 Display (Divoom Ditoo Pro):
<https://andreas-mausch.de/blog/2023-08-14-divoom-ditoo-pro/>

# Install

## Arch Linux (AUR)

```bash
yay -S divoom-ditoo-pro-controller-git
```

https://aur.archlinux.org/packages/divoom-ditoo-pro-controller-git

## From source

```bash
cargo install --path .
```

### Feature flags

| Feature | Default | Description |
|---|---|---|
| `text` | yes | Scrolling/static text commands (requires fontconfig C library) |
| `video` | yes | Video playback (requires libmpv) |
| `all-image-formats` | yes | All image codecs; disable for faster builds with only GIF/PNG/JPEG/BMP/WebP |

To build without optional features (no fontconfig/libmpv system dependencies):

```bash
cargo install --path . --no-default-features
```

# How to run

## Find your device

```shell-session
$ divoom-ditoo-pro-controller scan
[INFO] Scanning bluetooth devices for 20s
[INFO] Found device: DitooPro-Audio (11:22:33:44:55:66)
```

Look for a line containing `DitooPro` and note the MAC address.

List already-paired Ditoo Pro devices:

```bash
divoom-ditoo-pro-controller devices
```

## Decode firmware offline

```sh
divoom-ditoo-pro-controller firmware-decode firmware/306007.MVA
divoom-ditoo-pro-controller firmware-decode firmware/306007.MVA --output firmware/decoded/306007
```

Validates the MVA package CRC and extracts records, firmware code and strings.
Use a new output directory. See [the decoded format and findings](docs/firmware-format.md).
This command does not connect to the device.

## Look up numeric IDs

```sh
divoom-ditoo-pro-controller ids
divoom-ditoo-pro-controller ids games
divoom-ditoo-pro-controller ids game-keys
divoom-ditoo-pro-controller ids weather
divoom-ditoo-pro-controller ids noise-meter
divoom-ditoo-pro-controller ids games --json
```

This is an offline reference: it does not connect, launch games or discover
firmware capabilities. The Ditoo Pro app maps 15 built-in games to IDs 1–15.
App-defined mappings are labeled separately from observed device data;
unresolved categories explain the gap. Command help points to the relevant
category. See [ID sources and storage](docs/device-ids.md).

## Send commands

If only one Ditoo Pro is paired, the device is auto-detected. Otherwise, pass `--device`:

```bash
divoom-ditoo-pro-controller --device 11:22:33:44:55:66 brightness 50
```

### Display

```bash
# Send a static image (PNG, JPEG, GIF, BMP, WebP -- auto-resized to 16x16)
divoom-ditoo-pro-controller image ./photo.jpg

# Send a Divoom 16x16 animation
divoom-ditoo-pro-controller animation ./images/witch.divoom16

# Play a video (anything mpv supports: local files, YouTube URLs, streams, etc.)
divoom-ditoo-pro-controller video ./clip.mp4
divoom-ditoo-pro-controller video 'https://www.youtube.com/watch?v=dQw4w9WgXcQ'
divoom-ditoo-pro-controller video 'https://www.youtube.com/watch?v=FtutLA63Cp8'

# Scrolling text with custom color and font
divoom-ditoo-pro-controller scrolling-text "Hello world" --color yellow --bg-color black
divoom-ditoo-pro-controller scrolling-text "Line1\nLine2" --font "Terminus" --align left

# Static text centered on the 16x16 display
divoom-ditoo-pro-controller static-text "Hi" --color red --font-size 12
```

### Settings

```bash
# Brightness (0-100)
divoom-ditoo-pro-controller brightness 50

# Volume
divoom-ditoo-pro-controller volume get
divoom-ditoo-pro-controller volume set 8

# Play/pause SD card music
divoom-ditoo-pro-controller play
divoom-ditoo-pro-controller pause

# Clock face
divoom-ditoo-pro-controller clock get
divoom-ditoo-pro-controller clock set 123

# Date and time (defaults to current local time if omitted)
divoom-ditoo-pro-controller set-datetime
divoom-ditoo-pro-controller set-datetime 2025-03-25T21:22:59

# Language
divoom-ditoo-pro-controller language en

# Keyboard backlight
divoom-ditoo-pro-controller keyboard-backlight toggle
divoom-ditoo-pro-controller keyboard-backlight next
divoom-ditoo-pro-controller keyboard-backlight prev

# Alarm
divoom-ditoo-pro-controller alarm on --time 07:30 --repeat 62
divoom-ditoo-pro-controller alarm off --time 07:30 --repeat 62
```

### Display modes

```bash
# Light mode (sub-modes: 0=clock, 1=temp, 2=color, 3=special, 4=sound, 5=sound-user, 6=music)
divoom-ditoo-pro-controller mode light 2 --color red --brightness 80
divoom-ditoo-pro-controller mode hot
divoom-ditoo-pro-controller mode special 0
divoom-ditoo-pro-controller mode music 0

# Raw mode payload for experimentation
divoom-ditoo-pro-controller raw send 0x45 --data "06 00 00"
```

### Format conversion

```bash
# Divoom 16x16 to GIF
divoom-ditoo-pro-controller convert to-gif ./images/witch.divoom16 ./out.gif

# GIF/image to Divoom 16x16
divoom-ditoo-pro-controller convert to-divoom16 ./images/witch.gif ./out.divoom16

# Inspect a Divoom 16x16 file
divoom-ditoo-pro-controller debug-image ./images/witch.divoom16
```

# Bluetooth adapter

Please note the Bluetooth adapter is chosen automatically.
There is currently no way to configure it.

# Development

See [Development.md](Development.md).

# Protocol

- Protocol introduction:
  <https://docin.divoom-gz.com/web/#/5/146>
- App new send gif cmd (0x8b):
  <https://docin.divoom-gz.com/web/#/5/293>
- Example images from the Pixoo64 to decode:
  <https://github.com/Grayda/pixoo64_example_images>
- node-divoom-timebox-evo: PROTOCOL
  <https://github.com/RomRider/node-divoom-timebox-evo/blob/0.3.0/PROTOCOL.md>

# Pixel art

- <https://pixeljoint.com/pixels/new_icons.asp?search=&dimo=%3D&dim=16&colorso=%3E%3D&colors=2&tran=&anim=&iso=&av=&owner=&d=&dosearch=1&ob=search&action=search>

## Bluetooth transport

`--transport auto` (the default) tries RFCOMM first, then falls back to BLE
for the same device address if connection setup fails or a read-only volume
probe receives no valid response. Each of the three
RFCOMM attempts is limited to eight seconds. Commands are never retried on
another transport after transmission, so a toggle cannot be sent twice.
Use `--transport rfcomm` to disable fallback or `--transport ble` to skip
RFCOMM entirely:

```sh
divoom-ditoo-pro-controller --device 11:22:33:44:55:66 --transport ble brightness 0
divoom-ditoo-pro-controller --device 11:22:33:44:55:66 --transport ble keyboard-backlight toggle
divoom-ditoo-pro-controller --device 11:22:33:44:55:66 volume get
```

Use the address advertised by your device's `DitooPro-Light` radio. BLE does
not require pairing when an explicit address is supplied; automatic device
selection still searches paired devices. Fallback does not search for a
different radio address. BLE session setup synchronizes the device clock.

BLE uses Divoom's wrapped packets, paced 20-byte GATT writes, and transport
acknowledgments. An acknowledgment confirms delivery, not the physical
result. Keyboard backlight control is a toggle, not an absolute off command.
BLE lighting and volume queries are tested on a Ditoo with firmware 306007;
image, animation, text, and video support on this transport is not yet
verified on hardware, and paced writes limit throughput.

Library callers can select a transport with
`with_transport(Transport::Ble, send_set_brightness(address, 0)).await`;
unscoped calls default to automatic fallback. The selection applies to
the scoped future and is not inherited by separately spawned Tokio tasks.

## Device protocol API

The [protocol capability map](docs/protocol-capabilities.md) tracks device
operations, their opcodes, implemented helpers, remaining gaps and actual
hardware results. The command catalogue contains 196 Android symbols,
including extended selectors; catalogue membership does not imply Ditoo support.

```sh
divoom-ditoo-pro-controller --transport ble firmware
divoom-ditoo-pro-controller --transport ble status
divoom-ditoo-pro-controller --transport ble setting hour24
divoom-ditoo-pro-controller raw list --filter ALARM
divoom-ditoo-pro-controller raw send 0x37 --data 00 --query
divoom-ditoo-pro-controller raw run examples/device-info.json
divoom-ditoo-pro-controller scoreboard 12 34 --dry-run
divoom-ditoo-pro-controller alarm on --time 07:30 --repeat 62 --dry-run
```

Friendly device commands now run directly at the top level: `status`, `firmware`,
`scoreboard`, `setting`, etc. The former `device` prefix is removed. Both
`--device` and `--transport` work before or after subcommands. Commands with
`--dry-run` accept it after their name, for example `scoreboard 12 34 --dry-run`.

Low-level commands live under `raw` (`list`, `send`, `run`, `monitor`), replacing
the former `protocol` group. Use `raw send 0x45 --data "…"` for the former
`mode raw` operation.

Raw and symbolic commands support arbitrary binary payloads, explicit response
opcodes/prefixes, JSON output, batch requests and bounded event collection.
The library exposes `control::{Request, Session}` for programs. Alarm writes
now honor the supplied fields; `--time` is required because a write replaces
the full slot. Library callers pass `&Alarm` to `send_alarm`.

## Firmware updates (experimental)

USB flashing is supported, including reinstalling stock firmware and recovery
from an interrupted application transfer:

```sh
divoom-ditoo-pro-controller --transport usb firmware-update firmware/306018-lua.MVA
# Reinstall the same image, or recover through the bootloader on this port:
divoom-ditoo-pro-controller --transport usb firmware-update firmware/306018-lua.MVA --usb-port 1-6 --reflash
```

The updater preserves the bootloader and checks device read-back acknowledgments
for every block. Bluetooth is optional; add `--device MAC` to also verify the
running version over BLE after USB completion. See [USB setup, protocol,
performance and recovery](docs/usb.md).

`firmware-update FILE --dry-run` validates the pinned stock or experimental image
and prints metadata for the selected transport without connecting. Bluetooth
flashing retains the Android protocol and version checks: stock rejects an
equal-version announcement, even with `--reflash`. `--restore-stock` allows
supported experimental versions 306008–306018 to return to pinned stock 306007.
The Bluetooth 306007 → modified 306008 → stock 306007 round trip is verified by
completion events and live version reads. See [Bluetooth protocol and
evidence](docs/firmware-update.md).

## Android feature comparison and firmware archive

The [Android feature audit](docs/android-feature-gaps.md) compares the inspected
Divoom 3.8.40 app with this CLI, including device-specific versus phone/cloud
features and partially implemented commands at the historical `ff5196c` baseline.
The alarm defect found there is fixed by the subsequent protocol work.

The [firmware archive](firmware/README.md) contains the vendor image matching
installed version 306007, also the latest returned by the checked test-channel
endpoints, with verified hashes and saved API responses. See the updater
documentation for subsequent flashing experiments.

### On-device Lua

Firmware 306018 runs resident Lua apps with drawing, physical keys, keyboard
RGB lighting, timers and messages. It adds [battery/charging status and indicator
control, native alarms and power schedules, playback, voice memos and microphone
levels](docs/lua-peripherals.md), plus [native Bluetooth media connections](docs/lua-bluetooth.md). Use `lua start FILE`, `lua pause`, `lua resume`, `lua stop`, `lua send`,
`lua receive`, or `lua status`. One-shot `lua run FILE` and `lua eval SOURCE` remain
available. See [installation, APIs, limits and verification](docs/lua.md).
