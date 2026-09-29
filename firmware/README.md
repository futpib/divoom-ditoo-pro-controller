# Firmware for hardware family 306

Downloaded on 2026-09-28 from Divoom's own file server.
A subsequent [same-version update attempt](../docs/firmware-update.md) was rejected
by the device with ready status 2 before any firmware data chunks were sent.

The installed version at download and the newest image returned by the checked endpoints
are both **306007**, so [306007.MVA](306007.MVA) satisfies both requests.

- Device version evidence: `37 00` reply `01 0a 00 04 37 55 01 57 ab 04 00 a1 01 02`.
- Hardware flag: `306007 / 1000 = 306`, matching the Android app's calculation.
- File length: **1,877,379 bytes**.
- SHA-1: `645521a92a2bfb1149239312dbb3cc2f9e1ebd81` — matches the vendor API.
- SHA-256: `fc16341c005b11d0ac476917dc2fd98c9bc7481b92a183e64bfe209801566544`.
- [Vendor download](https://f.divoom-gz.com/group1/M00/01/71/rBAAM2X9G_yEQp1wAAAAABB41K4367.MVA).

The important discovery is **`IsTest: true`**. Both V2 and V3 services on
`appin.divoom-gz.com`, `app.divoom-gz.com`, and `apptest.divoom-gz.com`
returned this same file and version with that flag. Production requests
returned no FileId; the production list returned `306000`, whereas the test
list returned `306007`. `306000` is not a downloadable older image.
The returned changelog decodes to `fixbug!`.

This is a vendor image **matching the installed version number**, not a
byte-for-byte backup read from this particular device. The internal MVA
format was subsequently [decoded](../docs/firmware-format.md), including package
and code CRCs. This alone does not establish flashing success. It does not have the
Ditoo Plus `DIVOOMUPDATE` trailer, so the Plus container parser is not applied.
A test-channel lookup does not prove the shipped device runs beta firmware.
No newer image was exposed by this bounded set of public queries; regional,
account-specific, or unreleased images are outside that conclusion.

The exact requests and responses are in [service-checks.jsonl](service-checks.jsonl).
[manifest.json](manifest.json) records version roles, provenance, and hashes.

A reproducible metadata request (no account credentials needed):

```sh
curl --fail --json '{"Hardware":306,"IsTest":true,"Language":"EN","UpdateFlag":2,"DeviceId":0,"UserId":0,"Token":0}' \
  https://appin.divoom-gz.com/GetUpdateFileV3
```

The downloaded bytes remain proprietary vendor material, separate from this
repository's controller code license. They are preserved here for local
analysis; this commit has not been pushed.

`306008-reflash-probe.MVA` is a locally patched experiment, not a vendor release.
Its adjacent JSON report and `scripts/build-reflash-probe.py` specify its exact
changes and provenance. See [the update experiment](../docs/firmware-update.md#reproducible-version-gate-experiment)
for verification limits and the stock restoration procedure.

`306009-lua-probe.MVA` is the native heap diagnostic, built by
`scripts/build-lua-probe.py`. `306012-lua.MVA` is the bounded Lua runtime, built by
`scripts/build-lua-runtime.py`. Both derive from the pinned stock bytes; neither
is an official vendor release. See [Lua build, protocol and verification](../docs/lua.md).

`306013-lua.MVA` adds resident apps, framebuffer ownership, physical keyboard
events, timers, messages and guards that cannot be caught by Lua protected calls.
Build it with `scripts/build-lua-app-runtime.py`; its offline report is adjacent
and compact device results are in `lua-app-evidence/verification.json`. Full
working logs stay in ignored `firmware/runs/`.

`306014-lua.MVA` adds independently controlled keyboard RGB LEDs, native heap
telemetry and a launch headroom check. It also flushes one-shot display presents.
Build this image from source at commit `59b5e28`; use source at
commit `9a9d89c` for the earlier 306013 image. Its device evidence is in
`lua-io-evidence/verification.json`.

`306015-lua.MVA` retains the resident runtime and adds a bounded, read-only
filesystem metadata diagnostic for persistence research. Build this image from
source at commit `842115b`. It exposes no Lua file API
and performs no filesystem writes. See [storage research](../docs/lua-storage.md)
and `lua-storage-evidence/verification.json` for protocol and verification.

`306016-lua.MVA` adds native battery/charging status, indicator control, alarms,
power schedules, player controls, voice memos and microphone noise readings.
It fixes native wake-table persistence/selection and returns unused Lua memory
to the native audio heap. Build it from source at commit `ae62a69`.
See the [peripheral API](../docs/lua-peripherals.md) and
`lua-peripherals-evidence/verification.json`. Raw transport logs remain ignored.

`306017-lua.MVA` adds bounded native AVRCP connections and explicit play/pause
commands for Lua. Build it from source at commit `95d7f6e`.
See the [Bluetooth API and TV example](../docs/lua-bluetooth.md) and
`lua-bluetooth-evidence/verification.json` for verified behavior and limitations.

`306018-lua.MVA` adds the queued AVRCP mute toggle. Build it from `ac82bb9`;
see `lua-mute-evidence/verification.json`.

`306019-lua.MVA` adds full USB native protocol control and Lua upload/messages.
Build it from `606eb91` with `scripts/build-lua-app-runtime.py`. See
[USB control](../docs/usb-control.md) and `usb-control-evidence/verification.json`.

`306020-lua.MVA` adds the native Bluetooth HID keyboard and consumer-key profile,
automatic key release, and dirty-bond saving for HID-only connections. The
`scripts/build-lua-app-runtime.py` at `b703bc0` builds it from pinned stock 306007.
See [API and verification](../docs/lua-keyboard.md) and
`keyboard-evidence/verification.json` for Linux input, loop recovery and bond
persistence results. TV behavior remains unverified.

`306021-lua.MVA` adds Lua-controlled pairing windows, incoming HID connections,
bond listing/removal, encrypted-channel checks and connection diagnostics.
The current `scripts/build-lua-app-runtime.py` builds it from pinned stock 306007.
The Lua heap ceiling is 48 KiB, subject to a 24 KiB native reserve on every growth.
See `keyboard-pairing-evidence/verification.json` for laptop lifecycle tests,
firmware readback and hostile-Lua recovery. TV behavior remains unverified.
