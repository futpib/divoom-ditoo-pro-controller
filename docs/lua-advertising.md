# Bounded BLE advertising (306040)

Lua constructs legacy BLE advertising data and requests a timed burst. This is
a generic radio primitive: no Xiaomi wake packet or TV wake guarantee is built
in. The stock remote's long-standby wake format still needs capture/verification.
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
