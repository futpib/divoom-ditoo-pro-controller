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
status 2. Same-version rejection is a possibility, not an established meaning.

Logs are in `firmware/reflash-306007*`. The repeated attempt with enhanced
logging captured the same response: opcode 98, payload `00 02`. No update-success
event was received. Follow-up reads in `firmware/post-reflash-health.jsonl`
confirm version 306007, brightness 0, volume 3 and normal command responses.
Do not interpret the installed version remaining 306007 as proof of a reflash.
The streaming, retransmission, completion and post-reboot paths are implemented
but await a device-accepted update for E2E validation. Local tests cover the
image metadata/checksum/padding, target-version guards and response decoding. RFCOMM connected during this session but timed out on a version
query. The live update announcement used BLE.

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

The internal MVA structure has not been reverse-engineered. Integrity validation
here means exact equality to the vendor image plus the app's transfer checksum,
not a parser for arbitrary MVA firmware or signature verification.
