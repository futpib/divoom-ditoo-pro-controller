# Lua-defined HID (306039)

The script chooses BLE keyboard/Consumer capabilities and presentation. Firmware
builds and validates the standard report map, handles pairing/security and owns
every key-release deadline. A new action within these usage pages needs only a
Lua change, followed by re-pairing if the host cached a different report map.

The saved remote and Power standby/wake were [verified on the TV](../firmware/lua-hid-evidence/README.md).

```lua
local ticket = keyboard.configure({
  name = 'My Remote',
  appearance = 384, -- Generic Remote Control
  wake = true,
  keys = { 40, 44, 79, 80 }, -- Enter, Space, Right, Left
  consumer = { 205, 48, 547, 548 }, -- Play/Pause, Power, Home, Back
})
```

Call this from a resident app after selecting `keyboard.mode('ble-remote')` and
before pairing/listening. Await `device.result(ticket)` before the next native
operation, as with the other queued APIs. Applying an identical configuration
works while connected; a different one requires `keyboard.disconnect()` and
waiting until disconnected. Configuration never silently disconnects a host.
Invalid arguments raise a Lua error; busy/low-memory submission returns
`nil, reason`; an accepted ticket can still finish with `false, reason`.

All five fields are required. `name` is 1–29 bytes without ASCII control
characters. `appearance` is 0–65535 and `wake` is boolean. `keys` contains 1–16
distinct Keyboard-page usages in 4–231; `consumer` contains 1–16 distinct
Consumer-page usages in 1–65535. These are capabilities, not physical bindings.
Modifier usages 224–231 must be explicitly advertised if used. Two input
reports retain stable IDs and GATT handles; each has one or two payload bytes.
Mouse, joystick and vendor-defined report formats are outside this API.

```lua
keyboard.tap(44)          -- Space, default 80 ms
keyboard.tap(79, 0, 500) -- Right held 500 ms; modifiers are the second argument
keyboard.consumer(548)   -- Consumer Back, not a native action-name whitelist
keyboard.consumer(48, 100) -- Power held 100 ms
```

Explicit holds are 10–2000 ms. The selected BLE usage must be advertised.
Classic HID retains its existing fixed descriptor; numeric Consumer usages
1–1023 and the same hold-duration arguments work there. `keyboard.configure`
is BLE-only. Legacy `keyboard.media(action)` stays available for existing apps.
The new path uses numeric usages and Lua aliases:

```lua
local hid = require('../../lua/hid') -- from examples/lua/
hid.remote()                       -- queue the shared TV profile
hid.media('power')                  -- after configuration and connection finish
hid.media('home', 100)
local repeat_id = hid.repeat_media('volume_up', 350, 100)
timer.cancel(repeat_id)
```

These lines illustrate separate operations, not an initialization sequence to
execute all at once. The working asynchronous flow is in
[`tv-keyboard.lua`](../examples/lua/tv-keyboard.lua). The repeat helper waits one
interval before its first press, requires interval ≥ hold + 100 ms and uses the
existing bounded timer pool. Failed/busy repetitions do not accumulate a queue.
Cancel it on the physical release event; a stopped/failed app loses its timers.
The current TV app still sends one action per short press.

`lua/hid.lua` owns friendly names and the non-alphabetic TV preset, including
Power, Home, Back and Menu. Apps can edit/replace that data without rebuilding
firmware. The bundler removes unused helper functions; an installed app is
standalone and redeclares its configuration after a boot or profile reset.
Profile state stays in RAM while the BLE profile is enabled. Stopping Lua
preserves it and an existing connection; disabling/resetting BLE frees it.

Changing descriptor content, order or lengths can require forgetting and
re-pairing on the host. Merely changing physical bindings or hold durations
does not change the descriptor. The remote checks an existing profile first
when restarting and keeps a matching connection.

Native safety remains independent of Lua: argument/size checks, encryption and
peer gates, maximum hold time, stale-command rejection, key release after app
failure, release retry/disconnect deadlines, pairing timeouts and memory limits.
`wake=true` permits configured input during HID suspend and advertises remote
wake; the host ultimately decides whether it can wake from its sleep state.
