# Standalone TV keyboard remote

For pairing failures, firmware 306024 adds [device-side Bluetooth tracing](bluetooth-trace.md) over USB without stopping this app.

Firmware **306029** adds BLE HID over GATT (HOGP). The remote now uses this
standard BLE peripheral profile with a restricted, non-alphabetic remote
descriptor. Use firmware **306035** for the Android TV pairing and reconnect
fixes. Native speaker profiles stay disabled in this mode.
The app is designed for setup, pairing, reconnection and ordinary use on the
Ditoo and host after one installation; see the TV limitations below.
Bluetooth HID sends Play/Pause, Mute, Volume Up/Down, Power and Left/Right reports. AVRCP and the
older `tv-remote.lua` remain available separately.

Real MiTV_MOEU0 / Android 14 testing on 306035 passed ordinary **Pair accessory**
setup, saved-bond Disconnect/Connect, and actual Play/Pause input in SmartTube.
The saved app also restarted and reconnected automatically after a firmware
restart, with working Play/Pause and TV speaker output.
See [the pairing results](../firmware/ble-pairing-evidence/README.md).
The [306030 results](../firmware/ble-remote-tv-evidence/README.md) retain the
earlier failures; Android Settings and Bluetooth services have not been patched.

```sh
# One-time setup from a computer.
divoom-ditoo-pro-controller --transport usb firmware-update firmware/306039-lua.MVA
divoom-ditoo-pro-controller --transport usb lua install examples/lua/tv-keyboard.lua
```

## On the Ditoo and TV

1. Power on the Ditoo. It starts the saved remote automatically.
2. Controls already match the printed keys; there is no button setup.
3. For the first BLE pairing, disconnect the old **DitooPro-Audio** accessory
   and open **Pair accessory**. Classic and BLE bonds are separate; switching
   profiles requires one new pairing. Later BLE app upgrades reuse that bond.
4. On the Ditoo, use **M → Pair new device → lever**. With no selected device,
   the app opens directly on Pair new device. Wait for **Ready to pair**, then
   select **Ditoo BLE Remote** on the TV. Accept a confirmation if shown; the
   tested Android TV completed pairing without a separate confirmation dialog.
   A blue bar shows the two-minute window. Pairing preserves existing bonds.
5. **Connected**, a green dot, and the connected Bluetooth name identify the
   actual host. Firmware 306037+ caches peer names; unknown names use the address
   and address type as a fallback.

| # | Physical control | Remote action | Stock ADC ID |
| --- | --- | --- | --- |
| 1 | Lever | Play/Pause | 4 |
| 2 | + | TV volume up | 1 |
| 3 | − | TV volume down | 9 |
| 4 | ← | Left arrow (seek or navigate) | 2 |
| 5 | → | Right arrow (seek or navigate) | 3 |
| 6 | M | Open/close local menu | 0 |
| 7 | ☀ Sun key (top-right) | TV Power (306039+) | 7 |
| 8 | Audio-source button (beside lever base) | Mute/unmute | 10 |

The fixed IDs come from the [stock firmware key tables](stock-ux.md#physical-key-ids).
Left/Right send ordinary keyboard arrows (HID usages 80/79), so the TV app
and focus determine whether they seek or navigate. Volume sends Consumer
Volume Increment/Decrement (233/234), not Ditoo speaker volume. A sent report
does not prove that every TV app handles it. On the real Android 14 TV, BLE
Play/Pause toggled SmartTube, Left/Right sought backward/forward ten seconds,
and volume/mute changed the TV's speaker output. Space reached Android as a
press/release; it toggled SmartTube in one player state and did nothing in
another. App focus determines its behavior.

The remote saves the selected host identity, BLE address type, and whether to
accept reconnections. When enabled, later boots advertise for the saved host.
A BLE peripheral waits for the host to connect; **Waiting for device** means
listening, while **Connecting** means an incoming connection is being set up.
An action pressed while waiting is retained for at most five seconds. It is sent
once if the connection opens in time. Opening a menu or changing the connection
operation cancels that pending action. **Disconnect** stops listening and remains
in effect after restarting the app or device, until Connect or Pair new device.

## Menu and recovery

**M** opens/closes the local menu. **←/→** browse with wraparound, the **lever**
selects, and **Source** goes back one level. Volume and Sun are inactive in menus.
Menu and confirmation keys never send remote input. Long labels scroll in full
rather than being silently truncated to four characters. There are no keybinding
prompts.

| # | Menu path | Action |
| --- | --- | --- |
| 1 | Devices → address → Connect | Select that saved device and listen for it; public and random address types are preserved. |
| 2 | Devices → address → Disconnect | Disconnect and stop accepting reconnections. Does not forget the bond. |
| 3 | Devices → address → Forget | Open a confirmation naming the selected address. **Cancel is selected by default**; browse to Forget and select to erase exactly that bond. |
| 4 | Pair new device | Open a two-minute pairing window without deleting any saved device. Becomes **Cancel pairing** while the window is open. Source also cancels from the pairing screen. |
| 5 | Exit | Stop the app and open the native menu. The Bluetooth connection can remain active. |
| 6 | Back | Return to the parent menu or remote screen. |

Pairing displays **Setting up**, **Ready to pair**, **Pairing**, then **Connected**
as those conditions occur. A failed setup or expired window remains visible;
a timeout does not silently become a request to connect the old device.
The host appears by its cached name, or by address and address type while the
name is unknown.

Each short press sends one automatically released report. Long-down, repeat and
release events do not repeat actions. Brief symbols (`>II`, `X`, `_`, `+`, `-`, `<`,
`>`) and action labels identify the requested action; they do not claim measured
playback state or that a TV app acted on it. Forgetting a device temporarily
closes the current HID connection; if a different selected host was enabled,
the app resumes listening for that host afterward.

The app selects `keyboard.mode('ble-remote')` automatically, disconnecting an
old Classic keyboard connection first. Settings → Bluetooth must allow App or
Remote mode. The Audio override blocks switching, and the screen explains the
required setting. The BLE advertised name is **Ditoo BLE Remote**. The Classic
HID and AVRCP Lua APIs remain available to other scripts.
The stock Classic name remains **DitooPro-Audio**. From firmware 306032, BLE
remote mode has a separate stable identity and advertises **Ditoo BLE Remote**.
Pair this new entry once; subsequent updates keep its identity and saved bond.
Older firmware shared the Classic address and could inherit its cached name
and transport choice on Android.

Hold any keyboard key for five seconds to stop the app and return to stock
controls and the native menu. Hold a key during power-on to skip the app for
that boot. Power cycling starts the saved remote when Settings → Autostart is on. `lua uninstall` removes autostart; `lua start`
is only temporary and does not replace the saved app. See [storage and boot
recovery](lua-storage.md).

The developer messages `toggle`/`play_pause`, `mute`, `power`, `space`, `volume_up`,
`volume_down`, `left`, `right`, `menu`,
`pair`, `connect`, `listen`, `disconnect`, `status`, and
`target XX:XX:XX:XX:XX:XX` remain available.
`status` sends HID state, peer and address type to the outbox; read it with
`lua receive`. `target` saves a public address without connecting.
`connect` and `listen` both advertise for the saved BLE host. USB control and
the existing Divoom BLE control service remain available. Concurrent control
and HID on one laptop connection are covered by the HOGP test; two simultaneous
BLE hosts require separate hardware validation.
`menu` opens/closes the local menu. Ordinary use does not require these messages.
Settings use `TV6|peer|public-or-random|enabled`, where enabled is `0` or `1`.
TV5 settings are adopted as enabled. TV2/TV3/TV4 Classic targets are not treated
as BLE bonds; an already connected BLE peer may be adopted instead. Otherwise
the app offers Pair new device. Classic bond records are preserved.

There is one resident Lua app. Replacing it preserves the Bluetooth profile
and connection; native code releases outstanding keys independently of Lua.
BLE listening does not consume the Classic outgoing connection-attempt quota.

The laptop hardware test uses `scripts/check-bluetooth-hogp.py --device MAC`
(run with `sudo` for an exclusive input grab). It requires the BLE app to
target that laptop already; it refuses a different saved target. It checks all
14 usages, actual app key callbacks, bonded reconnect and key release after a
Lua infinite loop. Input is grabbed so the tests do not control the desktop.
It restores the ordinary remote app afterward. This is host input verification,
not a physical button or TV compatibility test.
The [306030 hardware record](../firmware/lua-16k-evidence/verification.json)
also covers the new app menu, cancellation of Forget, and a saved Disconnect
state that survives restarting the app. The saved remote autostarted after a
firmware reboot and sent Play/Pause using the existing laptop bond without
pairing again. The runtime limits remain unchanged.
306030 also clears media-command payloads before enqueueing them. Previously,
Play/Pause immediately after connecting could inherit the saved peer address as
keyboard modifiers and fail with HID error 14. Sending a keyboard key first
masked the bug. The regression test sends media before any keyboard report,
and `--reconnect-only` checks the saved app without replacing it after boot.
The earlier [306029 hardware record](../firmware/ble-remote-evidence/verification.json)
contains the exact image hash and all 27 checks. A first control request after
the final flash failed with ATT `0x0e`; disconnecting and reconnecting only the
Ditoo LE bearer recovered it without resetting Bluetooth or deleting a bond.
The later [306030 TV record](../firmware/ble-remote-tv-evidence/verification.json)
verifies Android input events and SmartTube/media-volume effects from app
messages using the actual Ditoo BLE link. It does not test physical buttons.
The normal TV accessory picker attempted Classic pairing and failed;
explicit `createBond(TRANSPORT_LE)` succeeded with no A2DP audio connection.
After app Disconnect/Connect, Android reconnected while the app was off and
then reported HID connected, but the app remained Waiting for device and no
new input arrived. A TV-menu reconnect did not resolve that state. These are
historical 306030 failures; the 306035 pairing/reconnection results are recorded
[separately](../firmware/ble-pairing-evidence/verification.json).
Reliable simultaneous control from a second BLE host remains unverified;
one laptop control attempt timed out while TV HID
remained connected.

## BLE remote API (306029+)

```lua
keyboard.mode('ble-remote')                  -- queued native operation
keyboard.pair(nil, 120)                     -- accept one host for up to two minutes
keyboard.pair('84:5C:F3:EF:87:78', 120, 'public') -- restrict a pairing window
keyboard.listen('84:5C:F3:EF:87:78', 'public') -- accept this saved identity
keyboard.connect('84:5C:F3:EF:87:78', 'public') -- same peripheral listening behavior
keyboard.disconnect()
keyboard.forget('84:5C:F3:EF:87:78', 'public') -- disconnected only; one LE bond
keyboard.bonds(true)                        -- {address=..., address_type=...} records
keyboard.name()                             -- cached name of the selected BLE peer, or nil (306037+)
keyboard.name('84:5C:F3:EF:87:78', 'public')  -- cached name of this saved identity, or nil
keyboard.status()                          -- transport='ble', address_type, paired, encrypted
```

Use `device.result(ticket)` to await queued operations, then `keyboard.status()`
to observe pairing/connection completion. `connected` requires encryption and
subscriptions to both input reports. `keyboard.bonds()` retains the simple list
of address strings; use `bonds(true)` to preserve public/random address types.
Resolved identities are saved rather than temporary private radio addresses.

### Device names (306037+)

The remote app and native Saved devices menu prefer the peer's Bluetooth name.
The firmware reads the standard GAP Device Name characteristic (`0x2a00`)
asynchronously over the existing encrypted BLE connection, once per connection,
after HID setup. It uses the stock GATT client's request timer and state machine;
a missing name, busy client, failed read or allocation failure does not fail HID.
The address remains the fallback until the peer supplies a usable name. Classic
HID and AVRCP peers currently retain their address labels.

`keyboard.name([address[, address_type]])` returns a cached name or `nil`; it
does not start a scan or block Lua. Names are limited to 32 UTF-8 bytes, without
splitting the last code point, and ASCII control characters are replaced with
spaces. Rendering still uses the display's existing font. Passing no address
selects the current peer; omitted address type defaults to `public`.

Names are saved alongside the bond under separate `0x44484e00 + bond slot` TLV
tags, with the address and type checked before reuse. They survive disconnects
and restarts, refresh when a changed name is received, and are deleted by
Forget. Cached strings are allocated only for known names and freed when the
BLE profile is released. The existing `bonds(true)` records remain small;
scripts can fetch just the name they are displaying. Connections and commands
always use address plus address type, including when names are duplicated.

The wire format follows the Bluetooth SIG
[GAP name discovery procedure](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-62/out/en/host/generic-access-profile.html).
Reinstall `examples/lua/tv-keyboard.lua` after upgrading to use the new labels.

### Lua-defined HID and Power (306039+)

The current app requires **306039** and declares its BLE HID capabilities using
[`lua/hid.lua`](../lua/hid.lua). Keys, Consumer usages, advertised name, GAP
appearance and wake permission are Lua data. See [Lua HID configuration](lua-hid.md)
for numeric input, bounded holds, repetition and compatibility rules.

The Sun key now sends Consumer Power (`0x0030`) with brief `PWR` feedback.
The `power` developer message invokes the same action; `space` still sends Space.
The Ditoo's own power control is unchanged. The app keeps its non-alphabetic
remote descriptor and disables speaker profiles.

The compatibility default (without `keyboard.configure`) has Enter (40), Escape
(41), Space (44), Right (79), Left (80), Down (81), Up (82), and the eight legacy
Consumer actions, including Power from 306038. The current Lua remote preset
also advertises Consumer Home, Back and Menu; they have no physical bindings.

A host can cache its old report map. When upgrading this app's capabilities,
forget only the Ditoo accessory on the TV and pair it again through ordinary
**Pair accessory**, with Ditoo **Pair new device** open. Later restarts using the
same profile reuse the bond. Power is a standby/wake input; deep-sleep wake
requires the TV to keep Bluetooth wake available.

See the USB-IF [HID Usage Tables](https://www.usb.org/sites/default/files/hut1_3_0.pdf)
and Android's [HID report cache](https://android.googlesource.com/platform/packages/modules/Bluetooth/+/refs/heads/android14-release/system/bta/hh/bta_hh_le.cc).

New pairing uses the stock Security Manager's encrypted, bonded Just Works
procedure, restricted to the Lua pairing window and selected host. This stock
stack does not implement Secure Connections or MITM-protected numeric entry.
LE bonds use the existing native flash TLV store; Classic bonds are untouched.
From 306031, report notification subscriptions (CCCDs) are saved per bonded
identity in the stock flash TLV journal. Disconnect clears the live connection
state; an encrypted reconnect of the selected host restores its subscriptions.
Connect also adopts a matching encrypted link if the host returned while the
app was disconnected. No new CCCD write is required from the host. Fresh pairing
and Forget clear those saved subscriptions. Other identities and unencrypted
connections cannot inherit them; failed journal writes reject the CCCD write.
The `0x44485200 + bond slot` records contain only address, address type
and two subscription bits. They do not replace or expose stock encryption keys.

Firmware 306035 invalidates the two pinned Bluetooth flash-journal cache banks
before reading bonds and after journal writes/deletions. The stock HAL programs
SPI flash but reads through a memory-mapped cache; without this refresh, a
successful write could still read as missing or return the previous mask until
reboot. Every subscription write is read back and compared before acknowledging
the CCCD write. The cache refresh covers only the two 4 KiB flash banks, never
RAM. `hogp_storage` trace records expose the requested and verified masks.

BLE remote mode also suppresses Classic inquiry/page scanning, including stock
UI requests to reenable it. Leaving this mode restores the earlier Classic
access setting. Firmware 306032 additionally gives this mode a stable
static-random BLE identity, distinct from the Classic speaker address. Android's
ordinary `createBond()` can otherwise keep choosing Classic for its cached
dual-mode identity even after Classic discovery is disabled. The remote derives
the address from the public address with `byte[0] = (byte[0] ^ 0x20) | 0xc0`;
all remaining bytes stay the same. It is stable across boots and updates.
Advertising stops before the stock Security Manager programs the address and
resumes only after the controller confirms it. Leaving BLE remote mode restores
the previous address mode. Stock Classic identity and bond records are unchanged.

The Divoom control GATT service remains available at the active BLE address.
Use `scan --transport ble` or the derivation above when migrating a controller
from the old address. USB control is unaffected. Classic HID and AVRCP remain
available in their respective modes.

Android 14's HID host restores cached report setup on encrypted reconnect and
does not necessarily write the CCCDs again; the peripheral must preserve them.
See [Android's HID host implementation](https://android.googlesource.com/platform/packages/modules/Bluetooth/+/refs/heads/android14-release/system/bta/hh/bta_hh_le.cc),
particularly `bta_hh_security_cmpl` and `bta_hh_process_cache_rpt`.
Device Bluetooth traces expose `hogp_subscription` write/restore metadata
without bond keys.

Firmware 306033 defers the first encrypted HID report-map response for one
second on the Bluetooth task, without sleeping or blocking that task. The
stock Android TV picker attaches its input-device listener asynchronously;
a fast HID connection can otherwise create the input device before that
listener exists, leaving the picker to time out and delete a successful bond.
The pending response uses 80 heap bytes, retries buffer backpressure for at most
three seconds, and is discarded on disconnect. Cached reconnects skip report-map
discovery and incur no setup delay. See Android's
[BluetoothInputDeviceConnector](https://android.googlesource.com/platform/packages/apps/TvSettings/+/refs/heads/android14-release/Settings/src/com/android/tv/settings/accessories/BluetoothInputDeviceConnector.java).

The same firmware gives compilation/API initialization a separate 250 ms
loading allowance. Top-level Lua code and `init` each start their normal 50 ms
execution budget; the 100,000-step and 48 KiB memory limits remain enforced.
This prevents cold-start compilation from consuming the saved remote's first
callback budget.

A single native slot queues a press while the BLE buffer is busy, for at most
one second. Cancellation drops an unsent press. A native timer releases each
sent report after 80 ms, including after Lua abort or replacement. If a release remains blocked for one second, the native code
requests a link disconnect. Unencrypted or unsubscribed connections cannot
send reports. Stock alarm/key-hold recovery and Lua instruction/time limits
remain in effect.

Both ATT views preserve all original Divoom handles. The selected bonded host
or an active pairing candidate also sees HIDS,
Battery, Device Information and GATT services. Battery percentage is an estimate
from the stock seven-step level. Regenerate it with
`scripts/build-hogp-database.py`; the firmware builder checks it for drift.
The ABI mapping and hooks are recorded in `native/patches/manifest.toml`
and `stock-306007.ld`. Design references: [HOGP 1.0](https://www.bluetooth.com/specifications/specs/hid-over-gatt-profile-1-0/)
and [BTstack v1.3.2 ATT server](https://github.com/bluekitchen/btstack/blob/v1.3.2/src/ble/att_server.c).
The stock binary uses earlier SM event numbers; it is not replaced with that source.

## Earlier Classic HID behavior and verification

## Idle links and reconnecting

Firmware 306027 preserves the keyboard link against two stock application
disconnect paths. A device trace caught `BtDisconnectCtrl` callers `0x1759c`
and `0x170a8` immediately before local HCI disconnects; the second closed both
open TV HID channels. The first belongs to the stock `uac divoom disconnect a2dp`
path. Both act on the whole Bluetooth link even with speaker profiles disabled.
This was an explicit native disconnect, not a missing HID keepalive.

The fix skips only these callers while keyboard-only mode is enabled on the
current Bluetooth stack. It also protects connection setup. Combined mode,
explicit keyboard OFF, stock power-off, link loss and HID error cleanup keep
their disconnect paths. Suppressed requests appear as `application_suppressed`
in the device trace; ordinary requests remain visible as `application`.

HID does not require periodic fake key reports. The Bluetooth HID 1.1.1
specification deprecates idle-rate commands; input is normally reported when it
changes. Its `HIDReconnectInitiate` behavior permits reconnecting when the owner
has new input. This app now does that while retaining passive listening between
presses. See [HID 1.1.1, sections 3.1.2.7–8 and 5.3.4.6](https://www.bluetooth.org/docman/handlers/downloaddoc.ashx?doc_id=309012).

Read [Bluetooth traces](bluetooth-trace.md) before restarting the device when a
link drops. A `local host terminated` HCI event establishes which endpoint issued
the final disconnect, but does not identify the caller or prove an idle timeout.
Pairing again is not needed for an ordinary dropped connection. Explicit OFF
remains an exception: use LINK to re-enable it.

[Recovery evidence](../firmware/tv-key-reconnect-evidence/verification.json)
records a forced disconnect from the real TV followed by reconnection from an
injected control-key callback, with one submitted and released report and no
new pairing. The previous app fails the corresponding native regression.
That earlier observation did not reproduce the original disconnect. The later
306027 diagnostic build identified the stock application callers described above.

The [306027 verification](../firmware/tv-persistent-link-evidence/verification.json)
records a BLE firmware update and automatic reconnection using the saved TV bond.
The same encrypted HID link survived a 12-minute interval with the laptop's BLE
connection closed, no input reports, and zero channel closures. The trace caught
the stock disconnect policy firing again at roughly eleven minutes and being
suppressed. Injected Volume Up/Down callbacks then submitted and released reports
without reconnecting. Explicit OFF still closed both channels, ignored control
keys, and allowed a subsequent LINK to reuse the bond. These are device-side
observations; no new TV-side input confirmation or physical power-off test was
performed. The ordinary startup app was restored after the temporary probe.

## Lua API

- `keyboard.mode('keyboard'|'combined')` (306025) queues a reversible profile
  change. Keyboard-only mode hides native A2DP, AVRCP, HFP/HSP and serial SDP
  records, disables their L2CAP registrations, closes existing audio/serial
  channels, and advertises keyboard class `0x000540`. HID, SDP, BLE and USB stay
  available. This applies to all classic peers; it is not a per-TV audio switch.
  `combined` restores the native records, registrations and previous class.
  Mode changes preserve HID connections and bonds. Like those connections, the
  mode survives Lua stop/failure; restore it explicitly or restart the Bluetooth
  stack/device. The TV app reapplies keyboard-only mode after stack recreation.
  Ticket success means queue acceptance: check `keyboard.status().keyboard_only`
  and `keyboard.status(true).audio_channels` for the asynchronous result.
- `keyboard.connect(address)` queues a connection to one explicit Bluetooth
  address. Reconnecting to the already connected peer succeeds without pairing.
- `keyboard.disconnect()` queues a HID disconnection and stops listening.
- `keyboard.pair([address[, seconds]])` advertises the keyboard and listens for
  the selected host. In 306022, omit the address (or pass nil) to select the
  first incoming HID control-channel peer. Other peers are then rejected; an
  interrupt channel alone cannot select a peer. The window defaults to 120 seconds and accepts 1..120.
  The previous radio access mode is restored on connection, expiration,
  explicit disconnect, or Lua stop/failure. If the stock radio was already
  discoverable, restoring it preserves that pre-existing state.
- `keyboard.bonds()` lists up to eight native paired addresses, without keys.
- `keyboard.forget(address)` removes only that bond. Disconnect all classic
  profiles first; an active underlying link is also rejected. The native bond
  file is saved under the existing write budget. The remote host may also need
  to forget its copy before pairing again. No bond is erased automatically.
- `keyboard.listen(address)` (306021) waits for incoming HID channels from
  one explicit peer without starting an outgoing connection. State 4 means
  listening; a partial incoming connection still has a 120-second timeout.
- `keyboard.tap(usage[, modifiers])` sends one USB HID Keyboard/Keypad usage
  (`4..231`), with an optional modifier bitmask (`0..255`). Space is `44`,
  Enter `40`, Escape `41`, Right `79`, Left `80`, Down `81`, Up `82`.
  Modifier bits follow HID: left Ctrl, Shift, Alt, GUI, then their right variants.
- `keyboard.media(name)` accepts `play_pause`, `mute`, `volume_up`,
  `volume_down`, `next`, `previous` or `stop`.
- `keyboard.status()` returns `enabled`, `connected`, `state`, `peer`, `busy`,
  `sent`, `released`, `errors`, `error`, and `bonds_saved`.
  State is 0 disconnected, 1 connecting, 2 connected, 3 disconnecting, or
  4 listening. 306021 adds `paired`, `encrypted`, `pairing`, `pair_remaining_ms`,
  `access_mode` and `forgotten`. `paired` means a local native bond exists;
  only `connected` establishes an active HID link.
  306025 adds `keyboard_only`; diagnostics also include `hidden_services`,
  `blocked_psms` and the remaining `audio_channels` (zero once drained).
  `keyboard.status(table)` reuses a table for frequent polling.
  `keyboard.status(true)` or `keyboard.status(table, true)` additionally fills
  `incoming`, `opened`, `closed`, `close_status`, `close_channel`, `control`,
  `authentication_state`, `encryption_state`, `key_type`, `security_mode`,
  and `ssp`. Reused tables retain fields not requested by the current call.

Mutations return a ticket or `nil,error`. Poll `device.result(ticket)` for
native queue acceptance, then status for connection/report outcome. A sent
report does not establish that a particular TV application acted on it.
Numeric `error` categories are setup/guard errors 4–19, send status `0x100+n`,
channel status `0x200+n`, link status `0x300+n`, transmit status `0x400+n`, and
connection-manager status `0x500+n`, and radio-access status `0x600+n`.
`errors` counts failures; a successful
connection clears the last error code.
Error 18 refuses an oversized or malformed SDP list before changing profiles;
19 refuses restoration if another registration has occupied a saved PSM slot.

Keyboard-only mode uses the pinned stock SDP remove/add routines at `0x12eb7a`
and `0x12eafa`. It retains native profile objects and callbacks: packet ownership
and disconnection cleanup remain with the stock stack. Removing the audio PSMs
from the eight-slot registration array blocks both new incoming requests and
native outgoing attempts, including peers with cached SDP. Up to eight existing
channels are drained through stock `L2CAP_DisconnectReq`; HID channels are skipped.
The mode adds 84 bytes inside the existing 8 KiB global reservation and allocates
no heap. Stack recreation invalidates all saved pointers before further use.

## Limits and recovery

The Bluetooth task owns static report buffers. Only one interrupt report may
be in flight. Each press schedules an all-zero release after 80 ms. Stopping,
replacing or aborting Lua advances its generation and requests release sooner.
A send stalled for one second closes the HID connection. No Lua-supplied
buffer, callback or pointer enters the Bluetooth stack. Stale queued commands
cannot target a new connection. The existing Lua instruction/time/heap limits
remain in effect. Native alarms still take priority over queued app operations.

HID setup uses two L2CAP PSM registrations, one SDP record and one connection
manager registration. They live until the stock Bluetooth stack is rebuilt;
the runtime detects that rebuild even if the allocator reuses its address.
Unknown or mismatched incoming peers are rejected. SSP level 2 is registered
for both PSMs, allowing the stock NoInputNoOutput
pairing method. HID channels are accepted as open only after native link
authentication and encryption succeed. Host-side requirements remain in place.

The SDP descriptor has an eight-byte keyboard report with ID 1 and a two-byte
Consumer usage report with ID 2. The HIDP transport prefixes each with `0xa1`.
Report protocol, GET_REPORT, GET_PROTOCOL and keyboard LED SET_REPORT are
handled. Boot protocol is explicitly unsupported. LED output is acknowledged
without overriding the Ditoo's RGB lights. There is no API for indefinitely
holding keys or for uploading arbitrary descriptors.

Stock firmware saves dirty link-key records only when both AVRCP and A2DP
are connected. HID-only operation additionally saves the same native dirty
records after a stable two-second HID connection, on the stock main task.
Unchanged records are not rewritten; saved settings share the existing
64-writes-per-boot cap and one-second minimum interval. `bonds_saved` counts
these additional native save calls. It does not disclose keys.

## Verification and reproduction

```sh
python3 scripts/test-lua-app-runtime.py
cargo test --locked
# Pair and connect the laptop as a keyboard host first. This grabs only the
# Ditoo's Bluetooth event node; test presses cannot reach desktop applications.
sudo python3 scripts/check-bluetooth-keyboard.py --device DEVICE_MAC --guard
python3 scripts/check-lua-app-device.py --transport usb --firmware 306022 \
  --output firmware/runs/keyboard-guards
```

The input test requires real Linux HID input events for Play/Pause (`164`),
Mute (`113`) and Space (`57`), including both press and release. The guard test
starts an infinite loop after a real Space press and requires its release and
a responsive USB error query. Raw Bluetooth captures and JSONL runs are ignored;
compact evidence belongs in `firmware/keyboard-evidence/`.

The lifecycle test requires a Linux host with classic Bluetooth HID support
(the `hidp` module or BlueZ's configured userspace HID backend). Run the silent
agent in a separate terminal, then exercise pairing and both connection
directions. `--fresh-pair` deliberately forgets only this laptop/Ditoo bond on
both sides and checks that other Ditoo bonds survive; omit it to test reuse
after a device reboot. The test replaces and stops the Lua app, never sends
keys, and uses USB for all Ditoo control.

```sh
python3 scripts/bluetooth-media-target.py DEVICE_MAC --seconds 180
python3 scripts/check-bluetooth-pairing.py --device DEVICE_MAC --fresh-pair
# Exercise Lua initiating fresh pairing as well:
python3 scripts/check-bluetooth-pairing.py --device DEVICE_MAC --fresh-pair --outgoing-pair
# After rebooting the Ditoo, with the agent still running:
python3 scripts/check-bluetooth-pairing.py --device DEVICE_MAC
```

The [306021 results](../firmware/keyboard-pairing-evidence/verification.json)
record fresh pairing initiated from either end, incoming/outgoing encrypted HID with audio and BLE
disconnected, connection reuse, targeted bond deletion, pairing-window expiry
and cleanup after a Lua infinite loop. The same saved bond was reused after
reflash/reboot. These extend the earlier key-input tests below; TV accessory
pairing and SmartTube input were not tested in that earlier run.

The committed [hardware results](../firmware/keyboard-evidence/verification.json)
cover all three real Linux key events, release after a Lua infinite loop, all
43 device guard/recovery checks, and a HID-only bond reused after a firmware
reflash/reboot without fresh pairing. A second build produced the identical
image. Native sanitizer tests also exercise the three physical key bindings,
packet ownership, stale commands, stack reconstruction, and bounded bond saves.
The TV connection returned to disconnected during that earlier handoff.
A later [306024 TV test](../firmware/bluetooth-trace-evidence/verification.json)
recorded successful fresh pairing, encryption and both HID channels opening.
The owner confirmed that the physical Play/Pause button pauses/resumes SmartTube.
The TV also connected the stock speaker profile, which the owner disabled on
the TV. Mute, Space and reconnection after a TV/Ditoo reboot were not checked
in that follow-up.

The [306025 TV results](../firmware/keyboard-only-evidence/verification.json)
verify five hidden native service records, four disabled audio/serial PSMs,
zero audio channels, A2DP/AVRCP state zero and a retained encrypted HID connection.
The owner confirmed sound returned to the TV and physical SmartTube Play/Pause
still worked. A USB-triggered firmware restart then autostarted the saved app
and reconnected to the TV using its existing bond, with keyboard-only mode
enabled and the same saved buttons. An infinite Lua loop hit the instruction
guard; USB and HID remained usable and audio stayed disabled. The original
remote was restored afterward. No laptop Bluetooth pairing was used for these
306025 checks. The native sanitizer suite covers reversible profile restoration;
combined-mode restoration was not exercised against the TV.

During the initial upgrade, the old saved app reconnected A2DP before it was
replaced. Installing the larger app while audio was streaming hit the unchanged
50 ms startup guard. A small temporary app enabled keyboard-only mode first,
then installation succeeded. The saved updated app started successfully on the
subsequent restart. The guard and heap limits have not been relaxed.

The [reconnection fix](../firmware/tv-reconnect-evidence/verification.json) was
installed over BLE on 306025 without changing firmware. Previously, losing a
Ditoo-initiated connection left native HID inactive: the TV could authenticate
with its saved bond but its incoming HID channel was rejected. The app now
resumes listening after link loss. The native runtime regression fails with the
old bundle and passes with the fix, including explicit OFF and subsequent Listen.
A live forced disconnect returned to listening, and the TV's Connect action
reused the existing pairing. Installation saved the corrected startup app with
native readback verification; a power cycle was not repeated in this test.
The owner confirmed the physical Play/Pause key pauses and resumes SmartTube
after installation and reconnection.
The TV did not automatically reconnect within 30 seconds of the forced
disconnect; listening allows incoming connections but does not initiate them.

The [expanded controls evidence](../firmware/tv-controls-evidence/verification.json)
records the previous revision's seven report usages, menu and reconnect checks,
and BLE installation. That revision asked for eight button assignments. The
[current fixed-controls evidence](../firmware/tv-fixed-controls-evidence/verification.json)
covers removal of that setup, the stock ADC map, migration to peer-only settings,
and live callback dispatch over BLE. The owner confirmed that the updated remote
worked. Individual-key behavior was not separately reported, and the Linux input
harness was not run against the laptop.

The profile follows the [Bluetooth HID 1.1.1 specification](https://www.bluetooth.com/specifications/specs/hid-1-1-1/).
The related [vendor SDK](https://github.com/leadercxn/bp1048_sdk_v0.1.12/tree/8105bd864b04995d81c9f9ae77cb158259f39015/MVsB1_Base_SDK/middleware/bluetooth)
contains HID setup code omitted from the Ditoo link. This implementation supplies
that profile in C, using function and structure layouts checked against the
pinned stock binary; it does not link SDK binary objects.

The earlier Classic standalone test additionally drives the app's key callbacks through
a temporary message wrapper, uses the fixed physical key IDs and local menu to
pair a fresh Linux host, checks real input press/release events, and disconnects
and reconnects. This replaces shared app settings and forgets only the laptop
bond. It requires an active scoped pairing agent and permission to grab the
Ditoo event node. Physical ADC routing is tested by the native sanitizer suite.
That harness includes Volume Up/Down and Left/Right as well as the
original Play/Pause, Mute and Space; older committed Linux evidence covers the
original three actions. The TV can be tested directly using BLE control without
pairing the laptop as an input host.

```sh
# Historical Classic app only (the script refuses the current BLE app):
sudo python3 scripts/check-standalone-remote.py --device DEVICE_MAC
# Reboot the Ditoo with the original app installed, then observe without uploading:
python3 scripts/check-standalone-remote.py --device DEVICE_MAC --after-reboot
```

See [306022 device evidence](../firmware/standalone-evidence/verification.json).
