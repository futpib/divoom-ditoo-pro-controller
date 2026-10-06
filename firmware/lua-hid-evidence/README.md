# Lua-defined HID: firmware 306039

The running Ditoo version was confirmed after a BLE update. The current
`examples/lua/tv-keyboard.lua` was installed as the saved startup app and ended
connected to the TV without a Lua error. Its Sun binding is now Power.

On MiTV_MOEU0 / Android 14, the existing bond initially retained the old report
map without Power. Forgetting **only Ditoo** and using ordinary **Pair accessory**
succeeded. Android then exposed all 18 usages in the Lua preset, including
Power, Home, Back and Menu. No Android Bluetooth/framework patch or explicit
transport pairing override was used. The original Xiaomi remote stayed paired.

The temporary control receiver sent `power` twice to the Lua app over GATT.
Actual Ditoo input produced two `KEY_POWER` press/release pairs, with releases
at 74.222 and 75.498 ms. TV wakefulness changed **Awake → Asleep → Awake**.
This tests short standby, not deep sleep or cold power-on. Physical Ditoo button
presses were not part of the test; the ADC-to-Sun binding is covered on the host.

Two `play_pause` messages produced actual `KEY_PLAYPAUSE` press/release pairs
and SmartTube **PLAYING → PAUSED → PLAYING**. Audio remained on the TV speaker,
with no active A2DP device. SmartTube's playback screen was restored. The
temporary APK and UI dump were removed; Ditoo's input device remained present.

All **43 live guard/recovery checks** passed, including loops, caught loops,
coroutines, OOM, cancellation and subsequent execution. The 32-bit ASan/UBSan
suite additionally covers configuration validation, asynchronous completion,
stale tickets, cancellation/reset/timeout cleanup, descriptor decoding, 16-bit
Consumer usages, modifiers, configurable holds and release after failure.
The complete remote fits its existing 48 KiB arena with all 16 simulated bonds.
Native API tables are lazy and unused compiler debug names are reclaimed.

The image was reproduced byte-for-byte by two isolated builds. Rust tests (77),
Python tests (30), Lua formatting/lint, generated GATT database and manifest
checks passed. The builder's image report deliberately says offline-built;
hardware claims and their limits are in [verification.json](verification.json).
Bounded live safety results are in [device-guards.json](device-guards.json).
Raw packet logs, TV dumps and device identities remain ignored under
`firmware/runs/lua-hid/`.

Reproduce the firmware and host checks:

```sh
python3 scripts/build-lua-app-runtime.py
python3 scripts/test-lua-app-runtime.py
python3 scripts/check-firmware-repro.py
python3 scripts/firmware-manifest.py firmware/306039-lua.MVA --check
cargo test --locked
python3 -m unittest discover -s scripts -p 'test_*.py'
scripts/check-lua.sh
```

After updating, the device guard script replaces the running app in RAM. Restore
the remote afterward with `lua install examples/lua/tv-keyboard.lua`. For TV
input observation, use the existing [probe](../ble-pairing-evidence/probe/Control.java)
to send `power` or `play_pause`, and record only Ditoo's `getevent -lt` node.
Inspect `dumpsys power` and `dumpsys media_session` before/after. A GATT command
acknowledgement alone does not prove either input delivery or a TV action.
