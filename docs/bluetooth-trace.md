# Ditoo-side Bluetooth diagnostics

Firmware 306024 records selected Bluetooth events independently of the Lua app.
Read the retained history or watch while the Ditoo connects to a TV:

```sh
divoom-ditoo-pro-controller --transport usb bluetooth trace --seconds 0
divoom-ditoo-pro-controller --transport usb bluetooth trace --seconds 120 \
  > firmware/runs/tv-trace.jsonl
```

The command checks the firmware version before sending the diagnostic. It does
not stop, replace, pause or message the Lua app, change discoverability, initiate
connections or remove bonds. BLE also supports the diagnostic; USB leaves the
radio free for the TV.

Each event has a sequence number and device uptime in milliseconds.

| # | Layer | Recorded information |
| --- | --- | --- |
| 1 | `hci` | Connection requests/completions, authentication and encryption, remote pairing capabilities, confirmation requests, pairing completion, failed command status and disconnect reasons. |
| 2 | `stack` | Initialization, memory errors, access changes, timeouts, bond changes and pairing failures. Only fields initialized by each callback are recorded; native errors remain raw. |
| 3 | `hid_l2cap` | Incoming, opened and closed HID channels: PSM, CID, peer and native status. These status values are not HCI error codes. |
| 4 | `hid_link` | Native HID connection-manager callbacks and raw status. |
| 5 | `hid_error` | Extension error codes matching `keyboard.status(true).error`. |

HCI status and disconnect reasons include numeric values and names. Correlate
handle-only events with the preceding connection-complete peer. IO capability
responses describe the remote device. Pairing success does not imply open HID
channels or keys accepted by the remote application. No HID events alone cannot
establish whether pairing or SDP failed.

The latest 32 events occupy 768 bytes plus a four-byte cursor inside the existing
8 KiB static reservation. Recording allocates no heap, writes no flash and uses
none of the 48 KiB Lua arena. Short interrupt-protected sections serialize records
and snapshot copies; USB replies happen after interrupts are restored. Reads are
non-destructive, with independent reader cursors and at most six records per
160-byte reply. The CLI reports overwritten events, polls every 200 ms by default,
and cleans up on completion or Ctrl-C. USB permits one host command connection
at a time: stop the watcher before running another USB CLI command. The app
and its physical buttons continue running while watching. `--interval-ms` accepts 50 through 5000.

Records disappear on reboot. A backwards cursor produces a reset notice; a
reboot cannot always be identified if new events have already overtaken the old
cursor. Reopen the command after a USB disconnection.

This selected event trace omits SDP/ACL payloads, outgoing HCI commands, audio
and keyboard reports. Link-key notifications retain only address and key type.
Keys, PINs, passkeys and numeric confirmation values are never copied. Addresses
remain in output. Keep raw captures in ignored `firmware/runs/`; commit concise
verification reports when useful.

## Laptop capture

For a connection involving the laptop itself:

```sh
sudo btmon -i hci0 -w /tmp/ditoo-laptop.btsnoop
```

[BlueZ btmon](https://github.com/bluez/bluez/wiki/btmon) observes the laptop
controller's HCI interface. An ordinary adapter does not passively capture a
separate TV–Ditoo connection. That needs appropriate external capture hardware
or endpoint logs. Ditoo's new trace provides endpoint metadata without TV ADB.
Laptop captures can contain link keys and unrelated devices' traffic.

## Build and ABI

```sh
python3 scripts/build-lua-app-runtime.py
python3 scripts/firmware-manifest.py firmware/306024-lua.MVA --check
python3 scripts/test-lua-app-runtime.py
cargo test --locked --no-default-features
divoom-ditoo-pro-controller --transport usb firmware-update firmware/306024-lua.MVA
```

The pinned HCI handler begins at decoded address `0x121c6c`; its packet has a
16-bit length at +8 and an event pointer at +12. The hook replays the original
six-byte prologue and resumes at `0x121c72`. The stock application callback hook
at `0x7ec20` replays four replaced bytes and resumes at `0x7ec24`. The builder
checks original bytes against pinned stock and the linker constrains hook and
memory sizes. These addresses were matched against the related SDK's
`HciProcessEvent` and `BT_STACK_CALLBACK_PARAMS`, then checked in stock assembly.
HCI fields follow the [Bluetooth HCI specification](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host-controller-interface/host-controller-interface-functional-specification.html).

Command 0x37 payload: `7f 44 4c 55 41 0d` followed by a little-endian u32 last-seen
sequence. Reply: `DBTR`, ABI 1, error byte, record count, capacity, oldest retained
and latest sequence (u32 each). Each 24-byte record contains u32 sequence, u32
milliseconds, u8 layer, u8 event, u8 length, a reserved zero byte, and 12 padded
metadata bytes. Wrong request length returns error 1. There is no raw address,
buffer resize, clear operation or Lua dependency.

## Hardware verification

[Verification report](../firmware/bluetooth-trace-evidence/verification.json):
USB flashed and read back 306024; real laptop pairing, encrypted HID, reconnection,
bond reuse, pairing-window expiry and Lua-crash recovery passed. Laptop `btmon`
and the Ditoo trace both recorded successful pairing. Observing the TV app left
its generation unchanged while callbacks continued. Temporary laptop bonds and
pairing settings were restored, and the saved TV settings were unchanged.

The TV connection recorded HCI authentication failure 0x05 and disconnection
before HID opened. This narrows the failing stage; it does not identify which
endpoint has incorrect pairing state or establish working TV controls.

42 of 43 hardware runtime checks passed. The maximum 8192-byte upload was refused
by the stock-heap headroom guard after Bluetooth lifecycle testing; a targeted
retry had the same result. The 7828-byte TV bundle still loaded and runs. The
trace fits the existing static reservation and the heap safety threshold was
not changed. This maximum-size stress failure remains in the report.
