# Experimental firmware updater

The CLI implements the Android app's Bluetooth 98/99 updater. It currently
accepts only the exact archived vendor image for Ditoo Pro hardware family 306,
version 306007. **A complete flash has not been verified on hardware.**

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
downgrades and hardware-family mismatches. Image size and SHA-256 are pinned
before connecting; arbitrary images and other hardware families are unsupported.

## Live result

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
The streaming, retransmission, completion and post-reboot paths are implemented
but await a device-accepted update for E2E validation. Local tests cover the
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

The current BLE transport uses paced 20-byte writes, so this 1,877,379-byte image
would take over an hour at nominal pacing. The implementation does not increase
transfer speed by assuming a larger writable packet size.

The [offline decoder](firmware-format.md) now extracts the MVA container and
verifies its package and internal CRCs. The updater still requires exact equality
to the pinned vendor image and uses the app's transfer checksum; decoding does
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
