# Ditoo-side fixes for ordinary Android TV pairing

Firmware 306035 contains the Ditoo-side BLE interoperability fixes: a stable remote
identity distinct from the Classic speaker, a nonblocking delay of the first
HID report-map response, and persistent report subscriptions for encrypted
bonded reconnects. It refreshes the stock flash-journal cache and verifies
subscription writes by reading them back. It also separates bounded Lua loading from callback budgets.
Android Settings and Bluetooth services are not modified.

[Hardware results](verification.json) on MiTV_MOEU0 / Android 14: normal picker
pairing completed, Disconnect/Connect restored both subscriptions without a
new pairing, and the saved remote automatically reconnected after a firmware
restart. Real HID press/release events changed SmartTube playback after each
test. Audio remained on the TV speakers. Physical buttons and a mains power
cycle were not actuated in this run.

## Reproduce

Install `firmware/306035-lua.MVA` and save `examples/lua/tv-keyboard.lua` with
`lua install`. On Ditoo select **M → Pair new device → lever**, wait for
**Ready to pair**, then use Android TV's ordinary **Remotes & accessories →
Pair accessory → Ditoo BLE Remote**. No pairing helper is needed.

The test-only receiver under `probe/` can send real Divoom control messages
through the TV's existing BLE link. It has no activity and no pairing API.
The Ditoo's saved app turns these messages into actual HID reports. The receiver
requests MTU 247 before using the stock control service and validates framing,
checksum and firmware version. This fixture is pinned to the tested Ditoo's
BLE address and firmware (306035/306036); edit those literals for another device/version.
It ignores repeated Android MTU/service-discovery callbacks so a second
diagnostic connection does not start two competing notification subscriptions.

```sh
bash firmware/ble-pairing-evidence/build-probe.sh "$HOME/Android/Sdk"
adb -s TV:5555 install -g target/tv-pairing-probe/probe.apk
adb -s TV:5555 shell am broadcast -n dev.ditoo.tvprobe/.Control --es message play_pause
adb -s TV:5555 logcat -d -s DitooTVControl:I '*:S'
```

Identify the Ditoo node with `adb shell getevent -il`, then capture its actual
press/release events with `getevent -lt NODE`. Check `dumpsys media_session`
and `dumpsys audio` for effects. Command acknowledgements and HID connected
state alone do not establish input delivery. Check `disconnect`, `connect`,
and a later key; repeat after a Ditoo firmware restart without forgetting bonds.
USB `lua send` can replace this fixture when a cable is available.

For a BLE-only restart test, the fixture's `@maintenance` message temporarily
replaces the running app in RAM with a tiny program that closes HID and restores
the public BLE control address. It does not save this program or alter the saved
remote's settings/bonds. Reflash the same pinned image with `--reflash`; its
firmware restart loads the saved remote again. Leave the TV's connection policy
alone to check automatic return. The diagnostic GATT client can report status
19 when the temporary app deliberately closes the link. This is a maintenance
test, not part of normal pairing or everyday remote use.
The updater's post-restart check still targets its original BLE address. If the
saved app changes to the remote identity, it can report a reconnect/version
verification failure after device success. In this test the TV receiver read
306035 at the remote identity before allowing any app message; USB is another
way to verify the running version without competing for the TV's radio link.

Remove the diagnostic receiver when finished:

```sh
adb -s TV:5555 uninstall dev.ditoo.tvprobe
```

Raw Bluetooth captures, TV logs, UI XML, APKs and temporary signing keys remain
in ignored `target/`. `reproducibility.json` records two identical rebuilds of
each maintained firmware profile, including the image and JSON build report.
The 306031–306034 files are intermediate diagnostic images with known
pairing/startup/subscription failures; use 306035 for this implementation.
