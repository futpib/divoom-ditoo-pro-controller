# Saved apps and settings

Firmware 306022 stores one startup app (up to 8,192 source bytes) and one shared
settings string (up to 128 bytes). After a one-time installation, neither a USB
connection nor a Bluetooth controller is needed to run the app.

```sh
divoom-ditoo-pro-controller --transport usb lua install examples/lua/tv-keyboard.lua
divoom-ditoo-pro-controller --transport usb lua uninstall
```

`lua install` saves then starts the app. Reinstalling identical source skips the
flash write. `lua start` temporarily replaces the running app without changing
the saved source. Uninstall stops the app and writes an empty startup record;
it preserves the settings string. Firmware updates preserve these records.

`storage.get()` returns the cached string, or nil when empty. `storage.set(value)`
queues a native write and returns a ticket; wait for `device.result(ticket)`
before treating it as saved. An empty string clears it. Settings are shared
between apps, so use a format prefix. There is no general filesystem API.

The stock main task serializes writes with other configuration operations and
native alarm priority. Actual changes share the limit of 64 writes per boot
and one second between writes. Debounce changes and avoid periodic saves.

## Boot and recovery

Autostart waits three seconds after settings become available. Hold any keyboard
key during startup to skip it for that boot. The existing five-second held-key
escape stops a running app. An app that exceeds its time, instruction or memory
budget releases its controls and is not restarted until the next boot. USB
`lua stop` and `lua uninstall` work independently of Lua callbacks. An unavailable
storage layout disables autostart rather than indefinitely consuming keys; save
failure or cancellation cannot leave the runtime stuck in its saving state.

## Configuration journal

The native configuration partition is selected through `stock_partition(5)`.
On the tested device it occupies SPI `0x8b0000..0x8fffff`, preceding the larger
filesystem. The new namespace is model `0xd7`, slots 0/1 for app generations and
2/3 for settings. Every record has a magic, kind, generation, payload length and
CRC32. The two banks retain the previous valid generation; boot picks the newer
valid record and ignores a damaged newer record. Writes use fixed native record
sizes and are checked by readback. Unexpected allocations or foreign records
refuse writes. Scripts cannot choose addresses or native record identifiers.

This protects against an interrupted individual record write while the native
journal remains readable. It does not establish power-loss safety of the stock
journal garbage collector. Abrupt power loss during journal compaction has not
been tested. Keep a private backup before experimentation:

```sh
python3 scripts/backup-lua-config.py --output firmware/runs/config-backup
```

The read-only 306022 diagnostic backs up all five 64-KiB sectors and checks that
the native context remains unchanged. The backup can contain Bluetooth link
keys; it is private and ignored by Git. Opcode 12 uses the same framing as the
filesystem diagnostic below, with magic `DCFG`, a 20-byte context plus 12 bytes of padding, and indices
0..2559. It has no write or restore operation.

## Earlier read-only filesystem research

Firmware 306015 preserves the 306014 resident Lua API and adds a host-only
diagnostic for backing up the stock filesystem's 128 KiB metadata region.
That diagnostic does not provide writes to the large filesystem. The 306022
app/settings records use the separate configuration journal described above.
Lua scripts have no access to this diagnostic or to raw flash addresses.

```sh
python3 scripts/build-lua-app-runtime.py
cargo build --locked --release
divoom-ditoo-pro-controller --transport usb firmware-update firmware/306016-lua.MVA
python3 scripts/backup-lua-storage.py B1:21:81:DD:B8:9B \
  --output firmware/runs/lua-storage-backup
```

Use a new output directory. The script first queries ordinary firmware version
and requires a supported version from 306015 through 306022 before sending any extension command. It reads 1,024
128-byte chunks, checks every response and driver status, requires identical
filesystem context snapshots throughout, and checks firmware version again.
`--probe-only` reads the first 1 KiB. Backups, requests and raw logs stay ignored
under `firmware/runs/`; the committed evidence contains hashes and summaries.
An unchanged context is a consistency check, not a filesystem-wide snapshot lock.
The backup covers metadata only, not file contents or all device flash.

## Native layout and limits

The exact stock 306007 code selects a 7 MiB filesystem partition at page
`0x9000`, with 256-byte physical pages and payload beginning at `0x9200`.
The diagnostic only reads addresses `0x900000..0x91ffff` through native
`stock_page_read` at `0x7854c`. The relationship between this SPI address space
and the CPU's firmware mapping has not been established; these are not firmware
image offsets. The physical read path uses the stock SPI mutex.

The filesystem context at `gp - 33692` contains bitmap and index journals,
block counts and open-file state. Before reading, the diagnostic requires
the expected bitmap/payload bases. Invalid request lengths, ranges, absent
context or a different layout cause a response error without a flash read.
Only zero or one 128-byte unit may be requested. There is no write operation.

The captured filesystem reported 1,760 payload blocks, 107 used, and an active
index containing 21 files across models 1, 8 and 12. Model `0x4c55`, proposed for
Lua-private files, was unused. These are observations of the tested device,
not guarantees about other devices or reserved vendor namespaces.

Native open/write/close entry points have been traced, but the apparent
filesystem lock at `0x6bce4` is a no-op and replacing a file deletes its previous
entry first. Safe serialization, crash recovery and namespace isolation must be
resolved before adding persistent write APIs. The occupied CPU user-data region
at `0x1f0000` is not treated as free storage.

## Wire format

Command `0x37` payload: `7f DLUA 0a`, little-endian u16 unit index, u8 count.
Index must be below 1,024; count is zero for context only or one for 128 bytes.
The response payload is `DFSP`, ABI byte 1, error byte, u16 echoed index,
u32 native driver status, u32 data length, 32 context bytes, then data.
Error codes: 1 invalid request, 2 busy, 3 no context, 4 unexpected layout.
Busy responses contain only the first 16 bytes. Other replies are 48 bytes
plus data. All multibyte fields are little-endian.

A 304-byte payload produced a corrupted 313-byte BLE notification on this unit,
while the device remained responsive and ordinary version queries worked.
The diagnostic therefore uses 176-byte payloads, verified across the complete
metadata backup. The exact native transmit-buffer failure is not yet explained.
Host framing/checksum validation still rejects corrupted replies; trace logging
can retain the notification bytes for diagnosis.

See [compact device evidence](../firmware/lua-storage-evidence/verification.json).
