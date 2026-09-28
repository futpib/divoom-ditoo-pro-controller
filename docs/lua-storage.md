# Read-only filesystem research

Firmware 306015 preserves the 306014 resident Lua API and adds a host-only
diagnostic for backing up the stock filesystem's 128 KiB metadata region.
It does not implement persistent apps, assets, modules, settings or autostart.
Lua scripts have no access to this diagnostic or to raw flash addresses.

```sh
python3 scripts/build-lua-app-runtime.py
cargo build --locked --release
divoom-ditoo-pro-controller --transport usb firmware-update firmware/306015-lua.MVA
python3 scripts/backup-lua-storage.py B1:21:81:DD:B8:9B \
  --output firmware/runs/lua-storage-backup
```

Use a new output directory. The script first queries ordinary firmware version
and requires exactly 306015 before sending any extension command. It reads 1,024
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
