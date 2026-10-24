# Bounded BLE advertising (306040)

Lua constructs legacy BLE advertising data and requests a timed burst. This is
a generic radio primitive: no Xiaomi wake packet or TV wake guarantee is built
in. Replaying the captured stock-remote payload has not woken this TV.
The existing HID `wake=true` flag retains its separate meaning.

```lua
local ticket, reason = bluetooth.advertise({
  data = string.char(8, 255, 255, 255) .. 'DITOO', -- demonstration, not wake
  type = 'nonconnectable',
  interval_ms = 100,
  duration_ms = 1500,
})
-- Later, from an update callback:
local ok, error = device.result(ticket)
-- Optional early cancellation; uses the original completion ticket:
bluetooth.advertise_cancel(ticket)
```

The [complete example](../examples/lua/advertise.lua) waits five seconds before
submitting, giving a BLE uploader time to disconnect. USB control can remain
connected. Installing this example persistently is unnecessary; use `lua start`.
An existing TV/HID connection also causes `busy`; the example does not close it.

The [TV wake app](../examples/lua/tv-wake.lua) constructs the Xiaomi
APK wake service UUID in Lua with `wake.xiaomi_app(address)`. It does not send
anything on startup. Sun or the `wake` message uses the public TV address saved
by `tv-keyboard.lua`; `wake ADDRESS [ADDRESS ...]` accepts up to six distinct
public TV Bluetooth addresses. These are target parameters, not firmware constants.
All targets are validated before sending. Duplicate addresses are sent once.

```sh
# DITOO_ADDRESS is your device's Classic Bluetooth address.
# TV_ADDRESS is the TV's Bluetooth address, not its Wi-Fi or Ethernet address.
divoom-ditoo-pro-controller --transport rfcomm --device "$DITOO_ADDRESS" lua start examples/lua/tv-wake.lua
divoom-ditoo-pro-controller --transport rfcomm --device "$DITOO_ADDRESS" lua send "wake $TV_ADDRESS"
```

Each request waits twelve seconds, allowing a BLE uploader to disconnect, then
sends one three-second nonconnectable burst per target at 100 ms intervals.
Classic serial control can stay connected. `cancel` cancels the current burst
and remaining targets; M exits to the native menu. A busy BLE connection causes
failure without disconnecting that host. `WAKE SENT` reports local completion,
not a TV acknowledgement. This temporary app preserves saved source, settings
and bonds. Restart `tv-keyboard.lua` with `lua start` to resume its callbacks.
The ordinary remote's Sun binding remains HID Power.

This packet woke the physical MiTV-MOEU0 / Android 14 TV in one controlled trial
on Ditoo firmware 306043: a laptop capture received the expected payload, the
TV reported `bt-gpio` wake before the fallback alarm, and the owner confirmed
the screen was on. Other TV models and repeated-wake reliability remain untested.

The matching MiTV-MOEU0 Android 14 driver has a separate APK wake filter at
index `0x1b`: a full 128-bit service UUID match, without a sender-address filter.
The UUID bytes are `00 01 02 01 05 03 ff 00 01`, the six target address bytes in
reverse order, then `00`. Flags `02 01 06` and complete service-UUID AD framing
`11 07` produce a 21-byte advertisement. The same algorithm accepts different
TV addresses and does not require pairing with each target to construct or
transmit the packet. Acceptance by a TV still depends on its standby firmware.
See [driver and hardware evidence](../firmware/xiaomi-app-wake-evidence/README.md).

The host-bundled [`wake.xiaomi_rc(address)`](../lua/wake.lua) helper reproduces
the 31-byte advertisement captured from the stock Xiaomi RC while the user
successfully woke the TV. The payload contains `MI RC`, the HID service,
manufacturer field `03 ff 00 01`, and a vendor field whose three bytes match
the end of the target TV address in reverse order. A later stock-remote burst
used `03 ff 00 00` instead. See the [capture](../firmware/lua-advertising-evidence/stock-remote-capture.json).
The Ditoo retains its own source address and uses a 100 ms interval; this
reproduces the payload, not the stock remote's identity or exact radio timing.
The matching Xiaomi driver requires an allowlisted public sender address for
this RC filter. Reproducing the payload with Ditoo's identity does not satisfy it.
The user reported that this replay left the TV asleep, despite the laptop
receiving the expected payload and the app completing successfully.

The alternative `wake.mediatek(address)` helper constructs a candidate from the
legacy manufacturer-data filter in
[MediaTek's published bootloader driver](https://android.googlesource.com/platform/external/u-boot/+/refs/heads/android-tv-s-beta3/board/amlogic/tl1_x301_v1/mtk-bt/LD_btmtk_usb.c#976):
company bytes `46 00`, one masked byte, the target TV address in HCI byte order,
four masked bytes, and `43 52 4B 54 4D` (`CRKTM`). Flags `02 01 06` precede the
manufacturer AD structure. This is a best guess: `woble_setting.bin` or
`wake_on_ble.conf` can replace that driver's default, and the Xiaomi's actual
filter differs from this legacy manufacturer filter. That candidate was transmitted
successfully but did not wake this TV. No generic HID Power event is sent by this example.

| # | Field | Accepted value |
| --- | --- | --- |
| 1 | `data` | Required binary string, 1–31 bytes of complete AD structures: length byte, type byte, value bytes. Length includes type and value, excluding itself. Embedded zero bytes in values are allowed; zero-length or truncated structures are rejected. |
| 2 | `type` | `nonconnectable` (default), `scannable`, or `connectable`. Undirected legacy advertising only. |
| 3 | `scan_response` | Optional binary string, 0–31 bytes with the same AD framing. Must be empty for `nonconnectable`. |
| 4 | `interval_ms` | Integer 100–1000; default 100. Converted down to the controller's 0.625 ms units; actual radio timing includes BLE advertising delay. |
| 5 | `duration_ms` | Integer from `interval_ms` through 5000; default 1500. Native timing starts after advertising is enabled. |

The API uses the current Ditoo Bluetooth identity and all three advertising
channels. It exposes advertising payloads, not HCI commands, arbitrary source
addresses, raw radio frames, or new GATT services. A connectable burst exposes
the existing control/HID services under their existing security/peer rules.

Requests require a resident app. Invalid arguments raise a Lua error; an occupied
native job slot or insufficient memory returns `nil, reason`. An accepted ticket
remains pending until restoration completes. A later `false, reason` can report
busy radio state, cancellation, controller rejection, reset, or timeout. `true`
means the local advertising operation completed; it does not acknowledge
reception or prove that a TV woke. As with other device operations, submitting
another job expires the previous completed ticket.

The stock stack provides one legacy advertising set. An existing/pending BLE
connection, active HID pairing, identity transition, or pending advertising
configuration causes `busy`; existing connections are never disconnected to
make room. A new incoming BLE connection during a connectable burst ends the
burst and restores advertising settings, leaving the connection intact.

`bluetooth.advertise_cancel(ticket)` returns true when cancellation is requested,
false if that operation already finished, or `nil, 'expired ticket'` for another
operation/app. Keep polling `device.result(ticket)` for cleanup completion.
The job slot remains occupied during the burst; cancellation needs no new slot.

The Bluetooth task copies data into an on-demand native allocation. It restores
the previous payload, scan response, parameters and enabled/disabled state at
the deadline, on cancellation, or after Lua failure, OOM, stop/replacement.
Lua cannot extend an active deadline. Setup and restoration each have a one-second
timeout. Stopping the app invalidates queued requests by generation; no VM
pointer enters the Bluetooth queue. If stock code replaces the advertiser, the
engine preserves that newer state while detaching its own buffers. No flash
writes or pairing changes are performed by this API.

Native tests use the actual Lua API and engine with a controller that retains
buffer pointers and processes configuration later, under 32-bit ASan/UBSan.
This checks packet framing, full-size payloads, all three advertising types,
busy connections, enabled and disabled restoration, cancellation before/after
dispatch, stale queues, loop/OOM cleanup, stack replacement and controller errors.
See [build and validation evidence](../firmware/lua-advertising-evidence/README.md)
for the current hardware-verification status.
