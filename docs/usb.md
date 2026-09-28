# USB on Ditoo Pro

## Flash firmware

```sh
cargo build --locked --release

# Offline validation; no USB access:
target/release/divoom-ditoo-pro-controller --transport usb \
  firmware-update firmware/306012-lua.MVA --dry-run

# Flash the only attached Ditoo. Bluetooth is not required:
target/release/divoom-ditoo-pro-controller --transport usb \
  firmware-update firmware/306012-lua.MVA

# Reinstall the identical image; select a physical port explicitly:
target/release/divoom-ditoo-pro-controller --transport usb \
  firmware-update firmware/306012-lua.MVA --usb-port 1-6 --reflash

# Optionally also verify the running version over BLE after USB completion:
target/release/divoom-ditoo-pro-controller --transport usb \
  --device B1:21:81:DD:B8:9B firmware-update firmware/306007.MVA --restore-stock
```

Use a USB data cable. `--usb-port` is the bus and physical port chain, not the
changing device address printed by `lsusb`; `lsusb -t` shows that topology.
For example, bus 1, root port 2, downstream hub port 3 is `1-2.3`.
The updater automatically selects a single normal Ditoo; multiple devices require
an explicit port. It follows that port when the USB identity changes.
`--device` is optional and adds a BLE version check after flashing; it does not
select the USB device. Pairing, Bluetooth, Python, a vendor PC utility and a
libusb installation are not required for the USB transfer. The Rust `nusb`
backend uses Linux usbfs directly. Linux is the tested host platform.

The default `auto` transport retains the existing RFCOMM/BLE behavior; select
`--transport usb` for USB flashing. USB currently supports `firmware-update`,
not normal display commands or Lua source upload.

### Permissions

Install the supplied udev rules once to grant USB access to the active desktop
user, then reconnect the cable (or trigger a change event on this USB device):

```sh
sudo install -m 0644 contrib/70-divoom-usb.rules /etc/udev/rules.d/70-divoom-usb.rules
sudo udevadm control --reload-rules
```

Headless sessions without a local active seat can run the updater with `sudo`
or configure a local user/group rule. The bootloader ID `0000:2244` is shared by
other MVsilicon products; the rule grants access, whereas the updater separately
checks the physical port, report descriptor, pinned image and bootloader match.
Only the vendor HID interface is claimed. Its kernel driver is reattached on
release; the normal audio and media-key interfaces are not detached individually.
The device reboot naturally interrupts its USB audio connection.

### What is written and how completion is checked

Only SHA-256-pinned images accepted by `firmware::Image` can be flashed: stock
306007, gate probe 306008, native-memory probe 306009 and Lua runtimes 306012–306016. The USB
updater extracts the MVA code payload and writes **only the application at
`0x10000`**. The installed bootloader, its header, and the user-data partition
starting at `0x1f0000` are preserved. This is not a byte-for-byte replacement of
the entire MVA, which also contains the bootloader and packaging metadata.
Images requiring another bootloader or extending into user data are rejected.

USB uses the bootloader's application change marker, not the Bluetooth
application's Divoom version gate. It supports stock restoration and same-image
reflashing even when the stock Bluetooth updater rejects an equal/older version.
`--reflash` complements the **announced** marker to force the bootloader to
write; the actual application bytes, version and marker remain unchanged.
Without it, an unchanged marker causes a no-write exit and an instruction to
use `--reflash`. The CLI follows the bootloader reset and sends its finish
command to return to the application. Some no-change resets interrupt the
metadata reply itself; this is still an error with no flash writes, and the
CLI returns to the previously running application before reporting it. USB does not impose the Bluetooth host-side downgrade check;
`--restore-stock` can be supplied for an explicit stock restoration and requires
the pinned 306007 image.

Every 4 KiB block requires an acknowledgment **after device-side flash read-back
and comparison**. Block zero, containing the application header, is committed
last. The updater then sends the completion command and requires the normal
Ditoo to reappear on the same port. `usb_complete` records those facts and timing.
USB has no implemented running-version query: add `--device MAC` to require a
separate live BLE version reply and emit `verified` as well. A write error,
missing/bad block acknowledgment or failed re-enumeration is an error exit.

### Recovery

An interrupted USB transfer was recovered on the tested device by restarting
the entire application transfer through the intact bootloader. When it appears
as `0000:2244`, explicitly select its physical port:

```sh
divoom-ditoo-pro-controller --transport usb \
  firmware-update firmware/306012-lua.MVA --usb-port 1-6 --reflash
```

Do not resume at a guessed block or retry an individual report: this protocol
has an implicit cursor and no report sequence number. The CLI aborts on a
transfer failure. Wait for fresh bootloader enumeration before rerunning; if
necessary reconnect the cable/restart the device. If a healthy application
returns instead, the same command can enter its updater again. This recovery
was demonstrated after an interrupted transfer, not after every possible form
of application corruption. A generic bootloader is never selected implicitly. Starting in the bootloader
requires `--reflash`: a matching old header alone cannot establish that a
previously interrupted application image is intact.

During a 306016 reflash, upgrade entry left Linux showing `8888:171e` with
failing control requests. A targeted `sudo usbreset BBB/DDD` using the Ditoo's
bus/device numbers from `lsusb` made it enumerate as `0000:2244`; restarting
the command with `--reflash` then verified all 479 blocks. `usbreset` itself
reported "No such device" during this successful identity change, so check
the new enumeration. No erase command had been sent before that entry failure.

## Performance and verification

Measured on the attached Ditoo Pro using Linux, `nusb` 0.2.7, USB full speed,
and the pinned Lua 306012 image (1,958,608 application bytes, 479 blocks):

- **Default queue of 16 reports:** erase, transfer and read-back verification
  **11.296 s**; **18.938 s** including entering the updater and application
  re-enumeration. A separate BLE query confirmed 306012 on the first attempt.
- **Sequential reports (`--usb-queue-depth 1`):** **12.130 s**, **19.749 s**
  including reboots. This also verified a same-image `--reflash`.
- **Rust recovery entry, already in the bootloader:** **11.205 s**, **16.999 s**
  including application re-enumeration; BLE again confirmed 306012. This began
  in the bootloader left by the unchanged-image check, not an injected failure.

The default queues a complete block's sixteen reports on ordered EP0, without
artificial delays, and waits for its read-back acknowledgment before submitting
the next block. The device consumes exactly four 64-byte USB packets per report;
larger reports are not supported. Erase plus the first block took 5.61 seconds,
then the remaining 478 blocks took 5.69 seconds (about 336 KiB/s). Queueing
reduced that latter phase from 6.43 seconds. These are measurements, not a claim
that the theoretical 12 Mbps bus rate is attainable by this bootloader.

The earlier BLE installation of this same Lua package took 287.14 seconds,
roughly 25 times the USB erase/transfer/verify duration. BLE sends the entire
2,025,699-byte MVA; USB preserves the bootloader and writes the smaller
application region. Optional BLE verification time is excluded from
`total_usb_seconds` and can be affected by host BlueZ disconnect delays.

A first ctypes/libusb prototype disconnected at block 361 of a stock transfer
(error `LIBUSB_ERROR_NO_DEVICE`). Its cause was not established. The device
reappeared in its bootloader; restarting the complete stock application transfer
succeeded in 11.08 seconds, followed by a live 306007 version query. The Rust
queued flash, sequential reflash and bootloader-entry runs above completed
without a transfer error. The unchanged-image test also exposed a reset back
into USB mode; the corrected CLI was then tested returning to the application
with `flash_writes: false`, without sending an erase/data command. It waits
for configuration and udev access when a new USB identity appears.

Reproduce the queue comparison with the normal application running before each
command (both commands perform a real same-image flash):

```sh
divoom-ditoo-pro-controller --transport usb firmware-update firmware/306012-lua.MVA --reflash --usb-queue-depth 16
divoom-ditoo-pro-controller --transport usb firmware-update firmware/306012-lua.MVA --reflash --usb-queue-depth 1
```

Raw JSONL/stdout/stderr from local runs stays under the ignored
`firmware/runs/usb/` directory. Git retains this compact result summary,
reproduction commands, protocol evidence and tests.

Final device check: the installed CLI uploaded
`brightness(0); volume(3); return 6*7` over BLE to the USB-installed Lua runtime.
It returned state `done`, result `42`, and a 3,950-byte peak Lua allocation.

Local validation: `cargo test --locked` passed 45 tests. Production-code Clippy
passed with `-D warnings -A clippy::too_many_arguments`; that exception covers
the two existing text-rendering functions. The new USB module passes rustfmt.

## Transfer protocol

All offsets below refer to the decoded stock `code.bin`, not the MVA file.
MVA code starts at file offset `0x60f`; the final four bytes are package metadata.
See the [entry disassembly](firmware-analysis/306007-usb-upgrade.nds32.S) and
[bootloader disassembly](firmware-analysis/306007-usb-bootloader.nds32.S).

1. In normal `8888:1719` (audio + HID) or `8888:171e` (HID-only) mode, locate the
   vendor HID interface with no non-control endpoints. Validate its exact
   36-byte report descriptor. Send eight-byte HID SET_REPORT(feature), starting
   with `aa`, then GET_REPORT(feature, eight bytes). The `55` reply enters the
   updater. These are class/interface requests `21/09/0300` and `a1/01/0300`.
   Even GET_FEATURE can enter the updater if a previous request armed it.
2. The same physical port re-enumerates as `0000:2244`, interface 0, full speed,
   EP0 maximum packet size 64. Validate its bootloader HID descriptor. Commands
   and data use **256-byte** SET_REPORT(output), `21/09/0200`; replies use
   GET_REPORT(input), `a1/01/0100`, length 256. All use interface index zero.
3. Send a zero-filled 256-byte metadata packet beginning with `cxxx`. Little
   endian words: offset 8 application length, 24 SDK application version from
   code `0x100b8`, 32 boot used length from `code[0xe0] & 0xffffff`, 36 boot CRC
   from code `0xbc`, 48 application change marker from code `0x100cc`.
   Byte 20 is the encryption flag from **code `0xff`**, not `0x100`; byte 52 is
   2 (application code present). Const/config fields stay zero.
4. Require reply magic `cxxx` and bytes 8–11 **`55 ff ff ff`**, meaning write
   application, no constant/config/bootloader update. `ff ff ff ff` means
   unchanged; `33` indicates rejection. Any other combination aborts before
   erase. The supported bootloader used length is `0x9e80`, CRC `0x5f08`.
5. Send `codedata` in another padded report. The bootloader erases the rounded
   application region. Send blocks **1, 2, …, N−1, 0**, each as sixteen 256-byte
   output reports; pad the last partial block with `ff`. Read a 256-byte reply
   after every block and require byte 8 to equal `55`. The initial erase can
   delay the first data transfer, which gets a 20-second timeout.
6. After every block passed, send a report starting with `upinfo`, bytes 8–9
   `ok`. There is no input reply. The bootloader resets into the application.

Stock addresses: feature OUT `0x7a3b8`, feature IN `0x7a34c`, updater entry
`0x2e4d0` with resource `0x40`; boot report reception `0x2910`, block reception
`0x33fc`, metadata dispatcher `0x3432`, code write `0x3ab0`, completion `0x3fb8`,
flash write/read-back comparison `0x2f06`. Block zero writes its 256-byte pages
in reverse order before comparison. No `bootdat`, `chiperas`, constant/config
or Bluetooth-information write command is used.

The related [SDK handler](https://github.com/leadercxn/bp1048_sdk_v0.1.12/blob/8105bd864b04995d81c9f9ae77cb158259f39015/MVsB1_Base_SDK/driver/driver_api/src/otg/device/otg_device_standard_request.c)
names the application entry `start_up_grate(AppResourceUsbDevice)`. The wire
protocol above comes from the exact shipped bootloader and live transfers.

## Other exposed USB features

The observed `8888:1719` configuration exposes USB speaker output, microphone
input, consumer/media-key HID and the vendor control HID. The audio streaming
alternate settings advertise stereo 16-bit PCM at 44.1 and 48 kHz; descriptors
establish the interfaces, not a tested microphone recording path. See
[retained descriptors](firmware-analysis/usb-8888-1719.txt).

The observed `8888:171e` configuration exposes only the vendor HID with 256-byte
input/output and eight-byte feature reports. See [its descriptors](firmware-analysis/usb-8888-171e.txt).
Neither configuration exposes CDC serial, mass storage or an ordinary computer
keyboard. USB forwarding of every physical key has not been demonstrated.
The vendor channel is also used for online audio tuning in the related SDK.

Divoom advertises [USB-C audio playback](https://divoom.com/products/divoom-pro).
Other configurations in the related SDK do not prove their availability on this
model.
