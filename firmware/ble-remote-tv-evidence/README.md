# Firmware 306030 on the real Android TV

`verification.json` records Android 14 / MiTV_MOEU0 results. These are TV input
events and SmartTube/audio state checks, not just successful Ditoo commands.
The firmware and saved remote app were not replaced during these tests.

Two failures remain: the ordinary accessory picker selected Classic transport,
and a bonded reconnect after the app's Disconnect/Connect sequence left Android
connected while the Ditoo waited for HID setup. A new explicit BLE pairing made
the initial key tests possible; it is not a standalone pairing or reconnect fix.

## Reproduce the TV-side probe

The small APK under `probe/` is a diagnostic fixture pinned to Ditoo address
`B1:21:81:DD:B8:9B` and firmware `306030`. Change those literals for another
device/version. It needs an Android SDK with platform 34 and build-tools 35.0.0,
a JDK, Python 3, and authorized ADB. It has no background service.

```sh
bash firmware/ble-remote-tv-evidence/build-probe.sh "$HOME/Android/Sdk"
adb -s TV:5555 install -g target/tv-probe/probe.apk
```

`Probe` explicitly chooses LE through `BluetoothDevice.createBond(TRANSPORT_LE)`.
First open Pair new device in the Ditoo app. Use this only when intentionally
pairing, with no existing Ditoo bond on the TV:

```sh
adb -s TV:5555 shell am start -n dev.ditoo.tvprobe/.Probe --es action pair-le
adb -s TV:5555 logcat -d -s DitooTVProbe:I '*:S'
```

`Control` uses the TV's existing BLE link to send a real Divoom Lua message to
the saved app. The Ditoo then emits a real HID report back to the TV. It never
injects Android input, and does not disconnect the system HID client when its
own GATT client closes. This allows device testing without a second BLE host
or a USB cable to the laptop.

```sh
adb -s TV:5555 shell am broadcast -n dev.ditoo.tvprobe/.Control --es message play_pause
adb -s TV:5555 logcat -d -s DitooTVControl:I '*:S'
adb -s TV:5555 shell dumpsys media_session
adb -s TV:5555 shell dumpsys audio
```

Also test `volume_up`, `volume_down`, `mute`, `space`, `left`, and `right`.
Use `getevent -il` to identify the Ditoo input node, then `getevent -lt NODE`
to distinguish actual remote events from command acceptance. Open a video
before checking Play/Pause. Space was delivered but did not toggle SmartTube.
Leave a delay between volume and mute checks: this TV's vendor audio code
also implements `doubleVolumeDownToMute`, which interfered with an immediate
mute assertion after the volume-down test.

For the failing reconnect case, send `disconnect`, wait four seconds, then
`connect`, and inspect both Android HID state and the Ditoo status. Android
automatically restored its LE connection about two seconds after disconnect,
before the app resumed listening. The test must include a subsequent key and
TV input event; Android's connected state alone did not prove recovery.

The probe's `refresh` action calls Android's per-device `BluetoothGatt.refresh()`.
It returned true, but clearing the GATT cache, reconnecting from TV Settings,
and repeating explicit LE pairing did not restore input in the subsequent
recovery attempts. This is a diagnostic action, not a documented fix.

The receiver's protocol response precedes the app's next update, so its result
text may describe the previous screen. Check a later status and actual TV
events. With USB available, ordinary CLI `lua send` can replace this fixture.

Remove the probe afterward; keep the Bluetooth bond:

```sh
adb -s TV:5555 uninstall dev.ditoo.tvprobe
```

Raw TV dumps, logcat, UI XML, pairing material, APKs, signing keys and input
captures stay in ignored `target/`. Only the curated results and probe sources
are committed. The APK uses a generated disposable test signing key.
