# On-demand memory: firmware 306036

The image built by `a6ded83` was flashed over BLE, with a 7.5 ms connection
interval. All 7,918 chunks were accepted, the device reported completion, and
the updater independently read running version 306036 after reboot. The image
and checksum are recorded in [verification.json](verification.json).

The [43 guard checks](device-guards.json) cover hostile scripts, independent
recovery after each abort, resident callbacks, pause/resume, messaging, display,
lights and a full 16 KiB upload. The [60 memory checks](device-memory.json)
exercise repeated worker lifetimes, first-draw framebuffer allocation, stopped
VMs, abandoned uploads, storage diagnostics and trace release/expiry. Each
cleanup returned to the same probe's native heap baseline. The probe itself
needs a worker and Lua arena; its readings are not an idle-stack measurement.

```sh
python3 scripts/firmware-manifest.py firmware/306036-lua.MVA --check
target/release/divoom-ditoo-pro-controller --transport ble --device CONTROL_ADDRESS \
  firmware-update firmware/306036-lua.MVA
python3 scripts/check-lua-app-device.py CONTROL_ADDRESS --firmware 306036 \
  --output firmware/runs/demand-guards
python3 scripts/check-demand-memory-device.py --device CONTROL_ADDRESS \
  --output firmware/runs/demand-memory
```

These tests replace the running Lua program in RAM. They do not write the
saved app, settings or bonds. Restore the installed app through the native
Saved app launcher afterward. Its diagnostic navigation must stay in one
`raw run` session so the stock menu does not time out: opcode 14 open, left,
left, read (native ID 30), then select, with 250 ms between keys. See
[menu diagnostics](../../docs/device-menu.md#build-and-diagnostics).

When testing through the public control identity on Linux, explicitly disconnect
the laptop afterward (`bluetoothctl disconnect CONTROL_ADDRESS`). BlueZ can
retain the link after the CLI exits; during this run that prevented the waiting
TV from reconnecting until the laptop disconnected. No bond was deleted.
The [temporary TV fixture](../ble-pairing-evidence/README.md) can then check the
version and deliver Lua messages through the existing TV BLE link.

The restored app delivered two real HID Play/Pause press/release pairs to the
TV. SmartTube went from playing to paused and back to playing; audio stayed on
the TV speaker. [Input results](tv-input.json) record that path. The saved app,
app settings, system preferences, Ditoo volume/brightness and TV music volume
were preserved. The temporary diagnostic receiver was removed afterward.

Framebuffer measurements use the same source and identical fresh VM reservations
with and without drawing. Repeated `device.stats()` calls inside one VM can grow
the Lua arena; its native allocation overhead is additional to `lua_reserved`.
Raw captures, private settings and temporary APK/signing files stay in ignored
`firmware/runs/` and `target/`; only bounded results are committed.

USB session buffers and Classic HID unregister have host sanitizer coverage but
were not exercised on hardware in this run. No Ditoo USB cable was attached.
The BLE profile was disabled and re-enabled, and the saved remote was launched
both by firmware startup and the native menu. Physical buttons and a mains power
cycle were not actuated.
