# Resident Lua apps

Firmware **306039** runs Lua 5.4.9 on the Ditoo Pro itself. Upload a clock or game
over Bluetooth, disconnect, and it keeps running. Changing scripts does not flash
firmware. 306014 adds independent RGB control of 12 keyboard LED positions;
306013 remains supported for the original resident app API. The earlier one-shot [306012 runtime](lua-306012.md) remains reproducible
and supported by the CLI.

Firmware **306016** adds [native battery, alarms, power schedules, speaker and
microphone APIs](lua-peripherals.md).

Firmware **306026** exposes a [stock asset catalogue](lua-assets.md): native sound
modes, flash-backed 16×16 font glyphs and 598 stock image frames, with bounded
direct drawing into the Lua framebuffer.

Firmware **306015** introduced a host-only, read-only
[filesystem metadata diagnostic](lua-storage.md). Firmware **306022** adds one
saved startup app and a shared 128-byte settings value in the native configuration
journal. `lua install FILE` saves and starts an app; `lua uninstall` removes its
autostart record and stops it, preserving settings. `lua start` remains temporary
and does not replace the saved app. See [persistence and recovery](lua-storage.md).

```sh
# Install the runtime once; see docs/usb.md for USB permissions.
divoom-ditoo-pro-controller --transport usb firmware-update firmware/306039-lua.MVA

# Save the standalone remote once; it runs after subsequent power-ons.
divoom-ditoo-pro-controller --transport usb lua install examples/lua/tv-keyboard.lua

# Temporary apps upload source into RAM.
divoom-ditoo-pro-controller --transport ble lua start examples/lua/clock.lua
divoom-ditoo-pro-controller --transport ble lua start examples/lua/snake.lua
divoom-ditoo-pro-controller --transport ble lua send right
divoom-ditoo-pro-controller --transport ble lua status
divoom-ditoo-pro-controller --transport ble lua pause
divoom-ditoo-pro-controller --transport ble lua resume
divoom-ditoo-pro-controller --transport ble lua receive
divoom-ditoo-pro-controller --transport ble lua stop
```

Add `--device B1:21:81:DD:B8:9B` if discovery is ambiguous. `lua run FILE` and
`lua eval SOURCE` execute one-shot programs and return a scalar. `lua cancel`
is an alias for stopping. The runtime has one app slot and one Lua VM;
start/run/eval stop the previous app before uploading. Coroutines and timers
belong to that same app and share its limits. Native Bluetooth connections can
remain connected after the app stops. The CLI checks the installed firmware
before sending any extension command.

File-based uploads automatically bundle local `require('...')` imports.
Reusable scrolling text, progress bars, and screen helpers live in
[plain Lua modules](lua-modules.md); firmware does not need to contain them.
`lua bundle FILE [-o OUTPUT]` previews the compacted, tree-shaken text offline.

The clock cycles colors on each key-down. Use `examples/lua/key-monitor.lua`
to see physical key IDs and event numbers. **Hold a keyboard key for five seconds
to stop the app and open the native menu.** The native worker handles this escape, including while paused;
Lua cannot disable it. The separate native power-key scanner remains untouched.
Hold any keyboard key during power-on to skip the saved app for that boot.
Autostart waits three seconds for that escape and tries only once; a failing app
returns control to stock firmware without a restart loop.

Firmware 306028 adds the [standalone menu](device-menu.md): a saved-app launcher,
persistent system settings, USB audio mode and voice memo controls.

## App contract

```lua
local x = 0
return {
  init = function()
    brightness(20)
    timer.every(1000, function() app.log(time.calendar().sec) end)
  end,
  update = function(dt_ms)
    x = (x + dt_ms / 100) % 16
    display.clear(0)
    display.pixel(math.floor(x), 8, 0x20a0ff)
    display.present()
  end,
  key = function(id, event)
    if event == 1 then comms.send('pressed ' .. id) end
  end,
  message = function(text) app.log(text) end,
}
```

Callbacks are optional. Globals, closures and tables persist between callbacks.
The native worker owns the interpreter; key/protocol hooks enqueue bounded data
and never execute Lua. By default an app owns the display and keyboard input.
Pause, stop or an error releases that ownership. `app.claim(false)` deliberately
passes display/input back to stock firmware while the app continues running.
The next stock redraw restores the native screen; immediate redraw is not forced.

| # | API | Behavior |
| --- | --- | --- |
| 1 | `display.clear([rgb])`, `pixel(x,y,rgb)`, `get(x,y)` | 16×16 RGB888 framebuffer; color is `0xRRGGBB`. |
| 2 | `display.line(x1,y1,x2,y2,rgb)`, `rect(x,y,w,h,rgb,filled)` | Clipped, bounded drawing. |
| 3 | `display.blit(x,y,w,h,bytes)`, `frame()` | Packed RGB888 sprites/frame export; blit dimensions 1–16. |
| 4 | `display.text(x,y,text,rgb)`, `present()` | Compact 3×5 letters/digits, up to 64 text bytes; present queues the frame for the next app tick. |
| 5 | `time.millis()`, `time.calendar()` | Milliseconds modulo 2³¹; RTC table with year/month/day/hour/min/sec/wday. |
| 6 | `timer.after(ms,fn)`, `every(ms,fn)`, `cancel(id)` | 12 timers, 10 ms–1 day delay, callbacks serviced at app ticks. |
| 7 | `keys.held(id)` and `key(id,event)` | Physical ADC IDs 0–10; events 1=down, 2=up, 3=long-down, 4=held-repeat, 5=long-release. Hardware simultaneous-key support is not established. |
| 8 | `device.brightness(0..100)`, `device.volume([0..15])` | Native controls; globals `brightness` and `volume` also exist. No volume argument reads the current value. |
| 9 | `app.claim(bool)`, `app.stop()`, `app.menu()`, `app.log(value)` | Ownership, stop/menu requests and last-result logging. `print(value)` logs its first argument. |
| 10 | `comms.send(string)` and `message(string)` | 128-byte inbox/outbox; send returns false while the outgoing slot is occupied. Host `lua receive` reads/acknowledges that slot. |
| 11 | `lights.count`, `fill(rgb)`, `pixel(index,rgb)`, `frame([rgb888])` | 306014: 12 LED positions indexed 0–11; 36-byte packed RGB buffer, independent of display pixels. Physical key/LED correspondence is not assumed. |
| 12 | `lights.present()`, `enabled([bool])`, `claim(bool)` | 306014: publish lights at the next tick; disable to publish black; release ownership to restore native lighting. |
| 13 | `device.stats()` | Free stock heap, Lua used/peak memory, frames, callbacks and dropped keys. 306016 also reports `lua_reserved`, the native heap currently backing Lua pages. |
| 14 | `storage.get()`, `storage.set(string)` | 306022: shared persistent settings, at most 128 bytes. Reads return a string or nil; writes return a job ticket for `device.result`. |

`examples/lua/keyboard-lights.lua` chases the LED positions; any keyboard press
changes color. LED intensities are raw 8-bit values, without the stock gamma
curve. `lights.present()` claims lighting independently of display/input;
`lights.claim(false)` releases it. Pause/error/stop also releases lighting.
The normal native LED task performs all hardware I/O; Lua publishes a complete
buffer and requests refresh. Resume restores the app's previous LED output.
A one-shot program can present a display frame; keyboard effects require a
resident app so ownership has a defined lifetime.

Selected base, math, string, table and coroutine libraries are available. There
is no `io`, `os`, `package`, `debug`, `load`, `loadfile`, `dofile`,
`collectgarbage`, or binary chunk loading. String `format`, `dump`, `pack`,
`unpack` and `packsize` are removed. Numbers are 32-bit integers/floats.

## Execution limits and recovery

- Source: 1–16,384 bytes on 306030; earlier resident firmware accepts 8,192.
  Text only, uploaded in at most 512-byte chunks. The host checks the installed
  firmware limit before stopping the current app.
  306023 frees consumed source blocks during compilation; it does not retain
  the source after compilation. See [memory accounting](lua-memory.md).
- Lua memory: a 48 KiB ceiling in 306021 (40 KiB previously), acquired in
  nonmoving KiB pages. The quota includes allocator headers, alignment and
  temporary allocations during realloc. Startup requires a 40 KiB budget plus
  24 KiB of native headroom. Every new arena allocation also preserves at least
  24 KiB for native tasks, so available native heap can lower the Lua ceiling.
  306023 reclaims shrunk allocation tails, grows into adjacent free space,
  pre-sizes native API tables and starts incremental GC earlier.
  Empty pages return to native audio; stop/error frees all pages without running
  Lua finalizers. Native tasks can still allocate independently afterward.
- Each setup or complete app tick has a 100,000-unit work budget. The guard runs
  for every VM instruction, allocator call and string-pattern backtracking step.
  A 50 ms elapsed-time check runs every 128 guard calls. Native configuration
  calls are separately limited to 32 per tick.
- A private native abort boundary is outside Lua protected calls. `pcall`,
  coroutines, `__gc`, `__close`, comparison callbacks and string callbacks cannot
  swallow a budget violation.
- App ticks run at most every 40 ms (25 fps). At most four queued key callbacks
  and four timer callbacks run per tick. The key queue holds 16 events and
  reports overflow. Pausing drops queued keys but preserves held-key recovery.
- Stack: 512 Lua values, C recursion limit 20, separate 16 KiB native task stack.
  The worker yields every 10 ms. Stop/reload remains available over Bluetooth.
- A stopped app consumes a held recovery key through its release, so that release
  cannot accidentally trigger a native long-press action.

Scripts are isolated by bounded native APIs and their allocator, **not an MPU or
process boundary**. Native firmware bugs remain possible. The elapsed-time guard
is not a preemptive interrupt over arbitrary C code. A saved app starts once at
boot unless a keyboard key is held; USB stop/uninstall remains available.

## Coverage and remaining work

This is an app platform foundation, not complete parity with every built-in
feature. The implementation and physical tests cover a custom clock, framebuffer
ownership, keyboard input, timers, persistent-in-RAM state, messaging and recovery.
Snake has a provisional numeric key map plus explicit `up/down/left/right/reset`
messages; its physical direction labels still need mapping on this device.

| # | Capability | Current state |
| --- | --- | --- |
| 1 | Drawing, animations and custom games | Implemented primitives; clock and Snake examples. 306026 adds native font and stock image access; see [assets](lua-assets.md). |
| 2 | Controls and lifecycle | Physical input, held state, ownership, pause/resume/stop/reload implemented. |
| 3 | Clocks, timers, stopwatch, scoreboards | RTC, timers and drawing implemented. 306016 binds native alarms and power schedules, with native alarm priority. |
| 4 | Keyboard lighting | 12 independently controlled RGB LED positions, custom effects and ownership restoration implemented in 306014. |
| 5 | Audio | 306016 binds native playback, sound previews, voice memos and noise readings; see the peripheral API and its hardware evidence. |
| 6 | Installed apps, assets and settings | Saved app/settings and boot escape in 306022; local modules are bundled by the host; read-only stock assets in 306026. General asset files remain unimplemented. |
| 7 | Communications | BLE and USB upload, bidirectional app messages and status implemented. USB requires 306019; see [USB control](usb-control.md). |
| 8 | Stock modes/settings | Brightness/volume implemented; other mode APIs and precedence still need integration. |

Internet services and notifications still require an external phone/PC provider.
Native entry points are traced against the exact stock 306007 image; names in
Android or the related SDK alone do not prove that a binding works on this model.
The occupied user-data region at `0x1f0000` is not treated as available app storage.

## Reproduce and evidence

```sh
python3 scripts/build-lua-app-runtime.py
python3 scripts/test-lua-app-runtime.py
cargo test --locked --no-default-features
cargo build --locked --release --no-default-features
# Requires an already-installed 306039; stops the current app, performs no flash writes.
python3 scripts/check-lua-app-device.py B1:21:81:DD:B8:9B \
  --output firmware/runs/lua-app-check
```

The host sanitizer test requires a 32-bit C toolchain and 32-bit ASan/UBSan
libraries (on this Arch host, `lib32-glibc` and `lib32-gcc-libs`). It uses the
device pointer size and accounts for the native heap consumed by allocations.

Build prerequisites and the earlier one-shot checks are in
[306012 documentation](lua-306012.md#reproduce). The builder pins stock firmware,
Lua sources and original hook bytes, checks code/RAM boundaries and bootloader
CRC preservation, and rejects accidental semihosting system calls. The updater
separately pins the resulting image's SHA and size. The build report deliberately
states offline status; [306013 hardware evidence](../firmware/lua-app-evidence/verification.json),
[306014 hardware evidence](../firmware/lua-io-evidence/verification.json) and
[306016 hardware evidence](../firmware/lua-peripherals-evidence/verification.json)
are separate. Raw runs stay in ignored `firmware/runs/`.

To reproduce the previous 306013 image exactly, build the source at commit
`9a9d89c`; use `59b5e28` for 306014 and `842115b` for 306015.
Use `ae62a69` for 306016, `95d7f6e` for 306017 and `ac82bb9` for 306018.
Use `606eb91` to reproduce 306019, `b703bc0` for 306020 and `c79d982` for 306021.
Use `a586981` to reproduce 306022.
The current builder produces 306039 with [Lua-defined HID profiles](lua-hid.md), the [native device menu](device-menu.md),
[persistent keyboard connections](lua-keyboard.md#idle-links-and-reconnecting),
[disconnect-request tracing](bluetooth-trace.md),
[stock assets](lua-assets.md), [lower Lua memory use](lua-memory.md),
saved apps, settings,
[Bluetooth HID keyboard support](lua-keyboard.md) and full USB control, retaining the queued AVRCP mute toggle and
[native Bluetooth media connection APIs](lua-bluetooth.md). Previous images
remain pinned.

The image reserves 4 KiB of native globals below `0x2004c000`; text starts at
`0x1ca000`. From 306027, initialized data loads immediately after the aligned
text end. The builder checks that the whole MVA, including its header and CRC,
fits the stock Bluetooth staging capacity of `0x1f0000` bytes. Earlier images
placed data at `0x1ef800`, which made the larger packages exceed that limit.
Hooks wrap heap initialization, command 0x37, screen output and the ADC scanner's
return at `0x2d490`. 306014 also wraps the LED flush at `0x7580c`;
when ownership is released it executes the original native path. Intercepting after native key-action mapping loses key-down
records whose stock action is zero; the ADC hook precedes that filtering.

The command prefix is `7f DLUA`, followed by operation: 0=status, 1=one-shot,
2=stop, 3=begin upload (u16 length, u8 mode: 0 one-shot, 1 resident, 2 install), 4=chunk (u16 offset, bytes),
5=commit upload, 6=pause, 7=resume, 8=incoming message, 9=read/ack outgoing, 10=filesystem diagnostic, 11=uninstall, 12=config diagnostic,
13=Bluetooth trace, 14=[menu diagnostics](device-menu.md#build-and-diagnostics).
ABI 2 replies start with `DLUA`, ABI, state, request error, result length, then
little-endian u32 peak memory, work units, current memory, frames, callbacks,
dropped keys, held bits and generation, followed by result bytes. States are
0=idle, 1=running, 2=done, 3=error, 4=active, 5=paused, 6=uploading, 7=saving.
