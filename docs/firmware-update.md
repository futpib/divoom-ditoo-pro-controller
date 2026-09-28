# Bluetooth firmware updater

For fast USB flashing, same-image reinstall and interrupted-transfer recovery,
see [the USB updater](usb.md). The version gates and full-MVA transfer described
below apply to Bluetooth.

The CLI implements the Android app's Bluetooth 98/99 updater. It currently
accepts the exact archived vendor image for Ditoo Pro hardware family 306,
version 306007, and the reproducible 306008 experiment described below.
**The 306007 → modified 306008 → stock 306007 round trip is verified on hardware.**

```sh
# Offline image validation and packet metadata:
divoom-ditoo-pro-controller firmware-update firmware/306007.MVA --dry-run

# Attempt a same-version reinstall on the explicit device:
divoom-ditoo-pro-controller --device B1:21:81:DD:B8:9B --transport ble \
  firmware-update firmware/306007.MVA --reflash
```

`--reflash` removes only the host's same-version check. It does not override
a device rejection, change the advertised version, or modify the image.
Without it, the command refuses to reinstall an equal version. It also rejects
downgrades (except the explicit probe-to-stock restoration below) and
hardware-family mismatches. Image size and SHA-256 are pinned
before connecting; arbitrary images and other hardware families are unsupported.

## Initial stock same-version attempts

The current-version attempt on firmware 306007 reached the metadata handshake.
The device responded with firmware file type 0 and ready status **2**. The
updater stopped without transmitting a 99 firmware-data packet. No reflash was
completed. The APK labels nonzero ready values as not ready but does not decode
status 2. The subsequent binary analysis below establishes an explicit same-version rejection.

Logs are in `firmware/reflash-306007*`. The repeated attempt with enhanced
logging captured the same response: opcode 98, payload `00 02`. No update-success
event was received. Follow-up reads in `firmware/post-reflash-health.jsonl`
confirm version 306007, brightness 0, volume 3 and normal command responses.
Do not interpret the installed version remaining 306007 as proof of a reflash.
At that stage the streaming, retransmission, completion and post-reboot paths
had not been exercised by a device-accepted update. Local tests cover the
image metadata/checksum/padding, target-version guards and response decoding. RFCOMM connected during this session but timed out on a version
query. The live update announcement used BLE.

The retry after offline decoding used the unchanged vendor file and again received
opcode `0x98`, payload `00 02`, before any data chunks were sent. See
`firmware/reflash-306007-after-decode.{jsonl,log}`. Subsequent reads in
`firmware/post-decode-reflash-health.{jsonl,log}` show a successful version
read of 306007. Two mode/brightness queries timed out; a later listen-only status
request received a transport acknowledgment without a status event. Consequently
this attempt confirms version-query responsiveness, not a complete settings check.

## Protocol and source evidence

The inspected Android APK is Divoom 3.8.40 (640), SHA-256
`d7ae490205cf71cc37f74948bd1ca7f1b2a446070565294b3ae835c0a51fde81`.

- `p166s1/b.java::c` loads the downloaded file unchanged, computes the unsigned
  sum of all file bytes, and constructs metadata. `p166s1/c.java` chooses a
  four-byte version for values greater than 65535. File type defaults to 0.
- Metadata sent to opcode 98: file type (one byte), version (LE32 for 306007),
  byte length (LE32), checksum (LE32). For this image:
  `00 57 ab 04 00 83 a5 1c 00 c8 8f a1 09`.
- `p166s1/a.java` starts sending only after the ready event and checks hardware
  family (`version / 1000`) before starting. `p166s1/b.java::i` pads the final
  256-byte chunk with zeros. Checksum and advertised length exclude that padding.
- `CmdManager.a1` builds opcode 99: chunk index LE16 followed by 256 data bytes.
  `CmdManager.b1` sends the metadata through opcode 98.
- `bluetooth/u.java` decodes 98 payload `[fileType,ready,indexLE16]`. Ready 0
  permits sending from the device-specified index. Any nonzero value stops.
- The same dispatcher decodes 99 payload `[fileType,status,...]`: 0 requests
  a chunk index, 1 signals success, 2 signals failure. Opcode 48 requests stop.
- The host services device-requested valid chunk retransmissions, bounds retries,
  and requires explicit device completion before reconnect/version verification.
  Transport acknowledgments or simply sending all bytes are not success.
- `p166s1/b.java` offers an update only for `serverVersion > installedVersion`.
  No same-version override was found in this inspected update path.

BLE streaming preserves asynchronous update responses received while waiting
for a transport ACK. Regular control commands retain their existing reply
matching behavior. No write is automatically repeated on another transport.
A disconnect/failure aborts; a new invocation may resume only at an index
supplied by a fresh valid ready response. Progress and device events are JSON
lines on stdout; diagnostics/errors go to stderr. Keep USB power connected and
capture output to a file rather than terminating the process midway.

Firmware streaming uses the characteristic's negotiated write capacity (capped
at 512 bytes), with acknowledged GATT writes and a protocol acknowledgment per
firmware packet. It falls back to 20 bytes if capacity cannot be read. The
`bluer` API already removes ATT overhead from the reported MTU. Streaming adds
no artificial delay after a completed write; ordinary control commands retain
their existing pacing. Progress includes elapsed time and effective byte rate.

The initial probe attempt used the old fixed 20-byte writes plus 50 ms sleeps
and was stopped at roughly 18%. The reconnected device requested chunk zero;
saved-index resume was not demonstrated in this attempt. With negotiated MTU
517 (512-byte write capacity), each 272-byte firmware envelope fits in one ATT
write. Live throughput initially increased to approximately 2.7 KB/s, before
shortening the new connection's interval. These measurements validate transport
improvement, not firmware installation or boot.

The [offline decoder](firmware-format.md) now extracts the MVA container and
verifies its package and internal CRCs. The updater requires exact equality
to one of the two pinned images and uses the app's transfer checksum; decoding does
not enable arbitrary-image flashing or establish signature enforcement.

## Device-side rejection decoded

Tracing the unmodified image establishes why announcing version 306007 fails.
The metadata dispatcher at code address `0x3abb4` parses version, length and byte
sum. Its internal event reaches `divoom_update_device_init` at `0x4b4a4`.
The version getter at `0x47924` returns the constant `0x4ab57` (306007).

Equivalent logic for the initial gates is:

```c
reply = {0, 2};
if (announced_version / 1000 != current_version() / 1000)
    return send_98(reply), NULL;
if (announced_version <= current_version())
    return send_98(reply), NULL;
context = allocate(300);
if (context == NULL)
    return send_98(reply), NULL;
```

The equal/older-version branch is `0x4b548` through `0x4b578`: it proceeds only
when current version is less than announced version, otherwise sends the two
bytes initialized at `0x4b4b2` through `0x4b4b8`. The family comparison precedes
it at `0x4b4bc` through `0x4b50c`; its multiply/shift sequence implements division
by 1000. Allocation follows at `0x4b57c`, so our equal-version attempt never
reaches memory allocation, image reception, CRC checking or signature inspection.

Status 2 is therefore not universally synonymous with same-version rejection:
family mismatch and allocation failure share it. But our known announcement and
the hardcoded current version determine the failing gate. A later capacity check
uses status 1. No image-content hash or digital signature can explain this
particular pre-transfer refusal.

See [the retained disassembly](firmware-analysis/306007-update-gates.nds32.S).
No advertised version was falsified and no gate was bypassed during the retry.
Changing only the advertised version might pass the first gates, but does not
prove that downstream bootloader checks permit reinstalling the same code.

## Reproducible version-gate experiment

### Run from this repository

Prerequisites: Linux with BlueZ running, a Rust/Cargo toolchain, Python 3,
`pkg-config` and libdbus development files, and `systemd-inhibit` for the live
run. The runner builds with `--locked --no-default-features`; fontconfig and
libmpv are not required. Cargo may need network access to obtain locked crates
on the first build. Both firmware images are committed; no firmware server,
APK, SDK checkout, disassembler or files outside this repository are required.

```sh
# Build the host CLI and reproduce/validate both pinned images; no Bluetooth I/O:
python3 scripts/firmware-roundtrip.py --dry-run

# Actual two-flash round trip; use the explicit address of the family-306 device:
python3 scripts/firmware-roundtrip.py B1:21:81:DD:B8:9B --kernel-disconnect
```

The runner refuses to start unless a live query reads 306007, then requires both
completion and verified 306008 boot before restoring stock. The final phase
requires completion and verified 306007 boot. Each run creates a new directory
under `firmware/runs/` with stdout, stderr and `result.json`; use `--output` for
another new directory. A sleep inhibitor covers the live run. An unverified
flash stops the sequence instead of guessing the device's state.

`--kernel-disconnect` uses `btmgmt` to clear the BlueZ disconnect stall observed
on this host between phases. It requires root or passwordless `sudo`; omit it
on hosts whose normal BlueZ disconnect works. A nonzero cleanup exit is logged
separately, since an already disconnected bearer is harmless. This is a host
workaround, not a change to the firmware protocol. If a run stops after verified
306008, the manual `--restore-stock` command below remains the recovery path.

The recorded transfers also used a manually negotiated 15 ms BLE connection
interval. That optimization is not required by the runner. At this host's
default interval, the corrected MTU-aware sender measured about 2.7 KB/s
(roughly eleven minutes per full image); the recorded six-minute timings include
the shorter interval. Both paths use complete acknowledged ATT writes.

After a successful round trip, optional recorded checks can be repeated with:

```sh
target/firmware-roundtrip/release/divoom-ditoo-pro-controller \
  --device B1:21:81:DD:B8:9B --transport ble \
  raw run firmware/roundtrip-health-requests.json
```

That sends version/settings queries and one stock metadata announcement; it
does not transmit firmware chunks. The expected stock gate reply is `00 02`.
Firmware updates may reset display brightness. The recorded run restored
brightness to 0 with `firmware/restore-brightness-requests.json`; the runner does
not assume that value is appropriate for another owner.

### Image construction

`scripts/build-reflash-probe.py` takes the pinned 306007 image and builds
`firmware/306008-reflash-probe.MVA`. Its JSON report records every changed byte.
This is a binary patch, not a rebuild from vendor source. Eight bytes change
across five fields:

- The instruction at code offset `0x47924` returns 306008 instead of 306007.
- The conditional branch at `0x4b550` becomes an unconditional branch to
  `0x4b57c`, bypassing the equal/older-version rejection. The family gate remains.
- The application header change marker at `0x100cc` changes from `0xe05a` to
  `0x4a59`. The inspected bootloader compares this word for inequality; the
  vendor's original marker algorithm has not been established. The replacement
  is content-derived, but is not claimed to reproduce that algorithm.
- The full-code CRC becomes `0x4751`; the package CRC becomes `0xd5c5`.

The bootloader's executable bytes and CRC `0x5f08` remain unchanged. The script
checks the stock hash, instruction bytes, allowed differences, length, and
package/bootloader/full-code CRCs. The output SHA-256 is
`6cef319b7b7f2dceb56f370cf79d74b27489aca55b9f78bca1ca756f6cf9281b`.

```sh
python3 scripts/build-reflash-probe.py
divoom-ditoo-pro-controller firmware-update firmware/306008-reflash-probe.MVA --dry-run
divoom-ditoo-pro-controller --device B1:21:81:DD:B8:9B --transport ble \
  firmware-update firmware/306008-reflash-probe.MVA

# Only after successful completion and a live version read of 306008:
divoom-ditoo-pro-controller --device B1:21:81:DD:B8:9B --transport ble \
  firmware-update firmware/306007.MVA --restore-stock
```

`--restore-stock` permits only installed version 306008 to the exact pinned
306007 image. It cannot override the device's gate: successful restoration
depends on the patched firmware actually running. Each transfer requires a
device success event followed by a live version check. Acceptance of the initial
announcement alone is not evidence that the modified image booted.

### Live probe result

The corrected transfer sent all 7,334 chunks in 380.07 seconds and received
`0x99` payload `00 01` (device completion). The automatic post-update check hit
a BlueZ disconnect timeout. An independent query returned payload
`01 58 ab 04 00`, decoded as **306008**, establishing that the modified image
booted. Its command result was recorded before cleanup timed out.

The subsequent stock restoration preflight independently read 306008 and
received `0x98` payload `00 00 00 00` when announcing the older 306007 image.
This establishes execution of the patched version-gate branch. Stock restoration
sent all chunks in 353.60 seconds, received `0x99` payload `00 01`, and read
**306007** on the first reconnect attempt. The updater recorded `verified` and
exited successfully. A disconnect timeout was retained as a warning rather than
discarding the already received version reply.

A final independent query again read 306007. A metadata-only same-version
announcement returned `0x98` payload `00 02`, confirming the stock gate was
restored; that check sent no firmware data. Volume remained 3. Brightness had
reset to 35 and was set back to its pre-experiment value of 0.

Evidence: `firmware/flash-306008-probe.{jsonl,log}`,
`firmware/post-probe-version.{jsonl,log}` and
`firmware/restore-stock-306007.{jsonl,log}`. The interrupted slow attempt is
preserved separately as `firmware/flash-306008-probe-slow.{jsonl,log}`.
Final checks are in `firmware/post-roundtrip-health.{jsonl,log}` and
`firmware/post-roundtrip-brightness.{jsonl,log}`. Their request arrays are retained.

The updater now records a version reply before cleanup, bounds disconnect to
ten seconds, and retains a successful version check if cleanup fails. It also
refreshes streaming capacity after reconnect because cached GATT objects can
temporarily lack the MTU property.
