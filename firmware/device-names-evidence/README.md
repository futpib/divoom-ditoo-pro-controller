# Cached Bluetooth names: firmware 306037

The Ditoo was updated from 306036 to 306037 over BLE. The updater confirmed
306037 after reboot. The saved TV remote was then replaced with the current
`examples/lua/tv-keyboard.lua` bundle, preserving settings and pairing.

The Ditoo independently read the TV's GAP Device Name. Its name matched the TV
adapter's configured name; no name was injected by the laptop or Android probe.
After the BLE profile was disabled and re-enabled for app installation, the
app already displayed the cached name during CONNECTING. It subsequently
displayed CONNECTED with the same name and no Lua error.

Two Lua Play/Pause messages produced two key-down and two key-up events on
the TV's Ditoo BLE input device. SmartTube was STOPPED throughout this test,
so this run verifies input delivery, not playback toggling. Sound remained
routed to the TV speaker, with no active A2DP device. The temporary diagnostic
APK was removed and the saved remote remained connected.

[verification.json](verification.json) records the bounded results and limits;
[tv-input.json](tv-input.json) contains only the relevant key events. Raw
Bluetooth, input, audio, media-session and transfer logs remain ignored under
`firmware/runs/`. The deterministic build report retains its offline-build
status; this separate record supplies the physical-device evidence.

Rebuild and check before flashing:

```sh
python3 scripts/build-lua-app-runtime.py
python3 scripts/firmware-manifest.py firmware/306037-lua.MVA --check
python3 scripts/test-lua-app-runtime.py
python3 -m unittest discover -s scripts -p 'test_firmware*.py'
cargo test
python3 scripts/check-firmware-repro.py
```

The host sanitizer tests cover busy/failed GATT preparation, unavailable names,
malformed and unencrypted replies, typed identity checks, UTF-8 truncation,
allocation failure, unchanged-name write suppression, persisted cache reload,
Forget, profile cleanup, long native-menu labels and 16 named Lua peers.
The named remote peaked at 47,760 bytes under the unchanged 49,152-byte Lua
limit. The firmware adds 1,216 package bytes and no additional static RAM.

Flash and install using the existing control connection:

```sh
cargo build --release
target/release/divoom-ditoo-pro-controller --transport ble --device CONTROL_ADDRESS \
  firmware-update firmware/306037-lua.MVA
target/release/divoom-ditoo-pro-controller --transport ble --device CONTROL_ADDRESS \
  lua install examples/lua/tv-keyboard.lua
```

A TV-connected remote uses the separate BLE identity. For this run the existing
[temporary control probe](../ble-pairing-evidence/probe/Control.java) queried the
running version and status, entered RAM-only maintenance for laptop control,
and sent the two Lua messages over the TV's existing encrypted connection.
It did not create or delete bonds. After laptop operations, explicitly releasing
the laptop's public control link allowed the TV to reconnect. The installed
remote displayed the cached name before that reconnection.

Names are currently discovered for BLE peers. Unknown names and Classic peers
retain address labels; the existing display font limits non-ASCII rendering.
See [the API and cache format](../../docs/lua-keyboard.md#device-names-306037).
