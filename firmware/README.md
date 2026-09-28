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
