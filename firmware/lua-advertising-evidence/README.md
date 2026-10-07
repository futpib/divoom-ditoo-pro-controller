# Lua advertising: firmware 306040

This image is built and host-tested. It has **not been flashed or verified on
hardware**. The control-address BLE version query failed with
`org.freedesktop.DBus.Error.NoReply`; a bounded discovery did not find the Ditoo,
and USB enumeration did not show it. No TV ADB connection was opened.

The image is 2,031,319 bytes, leaving 297 bytes below the stock BLE staging limit.
SHA-256: `937a160b3a3e7c5129d5d2ebc9ccdcdb640b808ef2b66aab9afb34df5d186064`.
Two isolated builds reproduced it byte-for-byte, alongside the three retained
historical build profiles. The new advertising setter ABI guards passed.

Host validation passed:

- 32-bit ASan/UBSan runtime and Bluetooth suites, including the real Lua
  advertising API and native engine with deferred controller processing.
- Cancellation before dispatch and during a burst; loop/OOM and app-stop cleanup;
  failed allocation and expired queues; tick wrap; live connection refusal and
  incoming-connection preservation; enabled/disabled advertising restoration;
  controller rejection through the real HCI hook with tracing disabled; stack
  reset and replacement of advertising by stock code.
- A retained HID advertising pointer checked after its setter returned, with
  ASan stack-use-after-return detection enabled.
- 10,000 integer/pointer formatting comparisons plus signed-boundary cases.
- All 77 Rust tests, 30 Python tests, and Lua formatting/lint checks.

To fit the API without changing partitions, general integer/pointer formatting
and locale lookup were replaced with bounded converters and the existing fixed
C decimal point. Lua limits and features remain unchanged. The HID advertisement
now has a stable buffer: the stock setter retains its pointer rather than
copying the bytes.

Reproduce locally:

```sh
python3 scripts/build-lua-app-runtime.py
python3 scripts/test-lua-app-runtime.py
python3 scripts/check-firmware-repro.py
python3 scripts/firmware-manifest.py firmware/306040-lua.MVA --check
cargo test --locked
python3 -m unittest discover -s scripts -p 'test_*.py'
scripts/check-lua.sh
```

Hardware work remaining: flash and read back 306040; observe actual advertising
payloads and timing on a separate receiver; exercise cancellation and Lua failure
during transmission; verify normal control/HID advertising and saved-app recovery.
Use USB control for these tests, since the first API version refuses active BLE
links. The [example](../../examples/lua/advertise.lua) sends demonstration bytes;
it does not send a TV wake command. Determining and testing the stock remote's
long-standby wake packet remains separate work.
