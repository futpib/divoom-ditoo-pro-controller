# Classic control alongside the BLE remote

Experimental firmware 306042 keeps the native Serial Port Profile (SPP/RFCOMM) available in
`keyboard.mode('ble-remote')` and `keyboard.mode('keyboard')`. The TV remote app
selects its existing BLE mode; no additional Lua setting or TV pairing is needed.
The intended arrangement is TV → BLE HID and computer → Classic SPP. Basic
simultaneous reads pass, but sustained HID input is not yet verified: the laptop
stress test lost BLE and exposed stale connection state on the Ditoo.

Use the device's **Classic address**, not the random BLE remote identity:

```sh
divoom-ditoo-pro-controller --transport rfcomm --device B1:21:81:DD:B8:9B firmware
divoom-ditoo-pro-controller --transport rfcomm --device B1:21:81:DD:B8:9B lua status
divoom-ditoo-pro-controller --transport rfcomm --device B1:21:81:DD:B8:9B lua install examples/lua/tv-keyboard.lua
```

These addresses identify the development unit. Its TV-facing BLE identity is
`D1:21:81:DD:B8:9B`. Other devices have different addresses. The native Classic
name remains DitooPro-Audio even when its speaker profiles are disabled.

The CLI connects the SPP profile directly, which selects BR/EDR even when BlueZ
only knows the address from BLE discovery. If the address has expired from
BlueZ's cache, it first creates the device with an explicit BR/EDR connection.
That BlueZ bootstrap may also connect other available profiles; firmware remote
mode hides and blocks audio profiles. Neither path selects BLE. It releases only
the owned Classic bearer on BlueZ versions supporting that API. Older BlueZ
closes the SPP stream without disconnecting a separate BLE bearer.
See [BlueZ ConnectProfile](https://github.com/bluez/bluez/blob/master/doc/org.bluez.Device.rst)
and its [per-bearer interfaces](https://github.com/bluez/bluez/blob/master/src/bearer.h).

BlueZ's `ConnectProfile` waits for the application's `NewConnection` reply.
The CLI accepts that stream concurrently with the pending method call, avoiding
a circular wait and the following retry's cleanup disconnecting a new link.

All existing controller operations use the serial transport. Installing a Lua
app replaces the single running app; that app's own code can change connections.
Flashing firmware still reboots the device and interrupts both connections.

## Firmware policy

The previous keyboard-only policy removed RFCOMM entirely because hands-free
audio and serial control share its L2CAP PSM. BLE remote mode additionally
disabled Classic paging. The new policy:

- Retains the SPP SDP record and RFCOMM PSM while hiding native audio services.
- Allows Classic paging in BLE remote mode, without Classic inquiry visibility.
  A computer with the known address can connect; the TV's accessory picker
  continues to discover the separate BLE remote.
- Accepts incoming RFCOMM channels only for the server number actually
  registered by native SPP. It rejects outgoing non-SPP RFCOMM connections.
- Closes existing non-SPP RFCOMM channels individually. An SPP channel on the
  same RFCOMM multiplexer survives this cleanup.
- Restores native audio services and the previous access policy when leaving
  remote mode. Stock pairing, security and bond storage are retained.

Stock firmware also reserves one Divoom command parser for the first Bluetooth
connection, including a BLE HID host that sends no Divoom commands. Firmware
306042 hands that unused session to serial control on the native main task.
It retains a BLE session that has received a command or has an incomplete frame.
When serial disconnects, the existing BLE connection regains its control
service. This handoff performs no HID reconfiguration or pairing changes.
Simultaneous Divoom command writers remain subject to the single native parser's
ownership. Sustained radio concurrency still needs the hardware checks below.

The radio policy hooks run on the Bluetooth task. They allocate no additional connection
buffers and retain the existing demand-allocated profile lifetime. Native ABI
guards pin the SPP context, callback, server-number field, RFCOMM connection
table and hook continuations against stock 306007.

## Why this does not use two BLE connections

Stock 306007 and our 306040 image have an unchanged host peripheral limit of
one (`hci_stack + 0x4ff`, initialized at `0x10dc9c`). The controller's ordinary
incoming connection allocator at `0x13c50e` also selects only connection slot 1:
it tests task state `0x106`, returns that slot when free, and returns error 9
otherwise. The incoming connection request path calls it at `0x13faa2`.
Another internal slot appears in controller accounting; its suitability for a
second ordinary BLE connection is unproven. Raising the host limit alone is
insufficient. These findings do not establish a silicon limitation.

The symbols were identified by comparing the actual firmware with the
[related MVsilicon SDK](https://github.com/leadercxn/bp1048_sdk_v0.1.12/tree/8105bd864b04995d81c9f9ae77cb158259f39015).
Classic plus BLE concurrency has earlier [hardware evidence](lua-bluetooth.md).
That evidence predates this particular serial-plus-remote policy.

## Verification status

306042 was flashed over RFCOMM and read back after reboot. The compact
[hardware report](../firmware/serial-control-evidence/verification.json) separates
passing checks from the unresolved input failure. Build metadata intentionally
keeps its deterministic `offline-built; hardware-unverified` label.

- The complete 2,031,559-byte serial firmware transfer took 416.629 seconds;
  post-reboot version verification returned 306042.
- Opening serial **after** an encrypted BLE HID connection returned firmware
  and Lua status while the HID connection remained up. The same order on 306041
  opened RFCOMM but received no command response.
- SDP retained SPP server 2 and HID, with no native audio services. Direct
  connections to RFCOMM servers 1, 3 and 4 and audio PSMs 0x17, 0x19 and 0x1b
  all returned connection-refused. Firmware queries on the same serial session
  succeeded before and after those probes.
- All 43 on-device runtime checks passed over RFCOMM, including infinite-loop,
  caught-OOM, recovery, memory reclamation and a full 16,384-byte upload.
- Real input verification **failed**. After refreshing the temporary laptop
  bond for the app's report descriptor, the first key sequence lost BLE with
  HCI reason 0x08. Linux used a 45 ms interval and 420 ms supervision timeout.
  The collector recorded no keys and aborted on input-node removal. A subsequent
  Classic link also timed out, but a later serial read succeeded with the same
  Lua generation: no device reboot was observed. The Ditoo retained stale HID
  connection state; its explicit Disconnect received HCI status 0x02, unknown
  connection. The timeout cause is unresolved. A longer supervision timeout is
  a proposed diagnostic, not an established fix.
- The TV setting was restored to enabled and the current Media/Nav app was
  installed for startup. Native readback verified both saves. Reboot, temporary
  laptop-bond cleanup and TV concurrency verification remain pending. No TV ADB
  session was opened.

- 32-bit ASan/UBSan tests cover retained SPP discovery/registration, rejection
  of every non-SPP server number, outgoing hands-free rejection, selective
  channel cleanup, mode restoration, stack recreation and memory reclamation.
- The full native Lua, USB, HID and HOGP tests pass, including hostile scripts
  and the TV app's two layouts.
- Two isolated rebuilds match the retained image byte-for-byte. The extension
  now starts exactly at the pinned stock image end, reclaiming 144 bytes of
  alignment padding without moving or deleting stock data.
- Package, bootloader and application CRCs and stock ABI guards validate.
  The package is 2,031,559 bytes, 57 bytes below the Bluetooth staging limit.

Remaining acceptance requires actual input on a test host while serial remains
open, BLE survival across serial reconnects, clean restart and TV reconnect
checks. Successful serial commands alone do not establish remote operation.

Reproduce local checks with:

```sh
python3 scripts/build-lua-app-runtime.py
python3 scripts/firmware-manifest.py firmware/306042-lua.MVA --check
python3 scripts/test-lua-app-runtime.py
python3 scripts/check-firmware-repro.py
cargo test --locked --no-default-features
python3 -m unittest discover -s scripts -p 'test_*.py'
scripts/check-lua.sh
```

Once connected over serial, the existing on-device guard suite also accepts
`--transport rfcomm`. It temporarily replaces the running app; restart the
remote afterward. Full working captures belong in ignored `firmware/runs/`.

The hardware probes below require a Linux host already paired to the BLE remote.
They grab its input devices temporarily, run the real TV app, and restore its
saved target afterward. The service probe uses read-only firmware queries and
rejects a timeout as inconclusive evidence of blocked audio:

```sh
python3 scripts/check-serial-hid-input.py \
  --control B1:21:81:DD:B8:9B --hid D1:21:81:DD:B8:9B \
  --host 84:5C:F3:EF:87:78 \
  --binary target/debug/divoom-ditoo-pro-controller \
  --output firmware/runs/serial-input.json
python3 scripts/check-serial-services.py B1:21:81:DD:B8:9B
python3 scripts/check-lua-app-device.py B1:21:81:DD:B8:9B \
  --transport rfcomm --firmware 306042 \
  --binary target/debug/divoom-ditoo-pro-controller \
  --output firmware/runs/serial-guards
```
