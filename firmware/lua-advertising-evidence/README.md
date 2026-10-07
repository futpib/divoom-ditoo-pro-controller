# Lua advertising: firmware 306040

This image was flashed over BLE from **306039 to 306040**. The device reported
update completion and the first reconnect read back 306040. Transfer took
755.51 seconds, averaging 2,688.74 bytes/s. The negotiated ATT MTU was 517;
the captured updater sent one 256-byte firmware chunk per acknowledged request,
typically completing a chunk every 90 ms. See [verification.json](verification.json).

The Ditoo initially disappeared from laptop discovery while connected to the
TV. After the user disconnected that link, `Ditoo BLE Remote` appeared at
`D1:21:81:DD:B8:9B` and control worked. The laptop's scan/controller commands and
an unrelated existing BLE device connection worked throughout. No TV ADB
connection was opened.

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

The first candidate wake app was uploaded temporarily using `wake.mediatek`.
Its saved TV address was `0C:CD:B4:D0:0C:26`. It requested a three-second,
100 ms interval connectable burst with data:

```text
02 01 06 13 ff 46 00 00 26 0c d0 b4 cd 0c 00 00 00 00 43 52 4b 54 4d
```

The laptop received ten HCI advertising reports containing that payload from
the Ditoo, spanning 2.106 seconds of the requested burst. Afterward it received
187 ordinary HID advertising reports; reconnecting to the control service
returned an active Lua app with result `SENT`. The existing saved remote app,
settings and bonds were retained. The user reported that the TV stayed asleep.
This candidate did not wake the TV in this trial; the advertising API's successful transmission is a separate
result.

The stock Xiaomi RC was then captured during a user-confirmed successful wake.
Its 31-byte payload and report counts are retained in
[stock-remote-capture.json](stock-remote-capture.json). The
[current candidate app](../../examples/lua/tv-wake.lua) replays that payload
with `wake.xiaomi_rc`, using the same three-second, 100 ms burst settings.
The laptop received ten matching reports over 1.478 seconds, followed by 130
ordinary HID reports. The app remained active with `SENT`, but the user again
reported that the TV stayed asleep. This reproduces the payload, not the stock
remote's source identity or precise timing; the rejecting condition is unknown.
Further wake testing is parked. Neither trial replaced the saved startup app,
TV settings or bonds.

BlueZ's per-bearer disconnect timed out and left the uploader connected.
An explicit `bluetoothctl disconnect D1:21:81:DD:B8:9B` released the laptop link
before the app's twelve-second delay expired. Capture used `btmon` alongside
an LE scan; raw logs stay in ignored `firmware/runs/lua-advertising-306040/`.

Hardware work remaining: cancellation and Lua failure during transmission,
other advertising types, and saved-app recovery. All 43 general runtime device
guards passed on 306040, including infinite loops, OOM, full-size source upload,
and subsequent recovery/version checks. Host safety coverage is listed above.
The [harmless example](../../examples/lua/advertise.lua) still uses test bytes;
both TV-specific payload helpers remain experiments, not working wake support.
