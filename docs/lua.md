# Resident Lua apps

Firmware **306014** runs Lua 5.4.9 on the Ditoo Pro itself. Upload a clock or game
over Bluetooth, disconnect, and it keeps running. Changing scripts does not flash
firmware. 306014 adds independent RGB control of 12 keyboard LED positions;
306013 remains supported for the original resident app API. The earlier one-shot [306012 runtime](lua-306012.md) remains reproducible
and supported by the CLI.

Firmware **306015** retains these app APIs and adds a host-only, read-only
[filesystem metadata diagnostic](lua-storage.md). It does not yet provide
persistent Lua files or autostart.

```sh
# Install the runtime once; see docs/usb.md for USB permissions.
divoom-ditoo-pro-controller --transport usb firmware-update firmware/306014-lua.MVA

# Subsequent changes only upload source into RAM.
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
is an alias for stopping. Start/run replace the previous app. The CLI checks
the installed firmware before sending any extension command.

The clock cycles colors on each key-down. Use `examples/lua/key-monitor.lua`
to see physical key IDs and event numbers. **Hold a keyboard key for five seconds
to stop the app.** The native worker handles this escape, including while paused;
Lua cannot disable it. The separate native power-key scanner remains untouched.

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
| 9 | `app.claim(bool)`, `app.stop()`, `app.log(value)` | Ownership, stop request and last-result logging. `print(value)` logs its first argument. |
| 10 | `comms.send(string)` and `message(string)` | 128-byte inbox/outbox; send returns false while the outgoing slot is occupied. Host `lua receive` reads/acknowledges that slot. |
| 11 | `lights.count`, `fill(rgb)`, `pixel(index,rgb)`, `frame([rgb888])` | 306014: 12 LED positions indexed 0–11; 36-byte packed RGB buffer, independent of display pixels. Physical key/LED correspondence is not assumed. |
| 12 | `lights.present()`, `enabled([bool])`, `claim(bool)` | 306014: publish lights at the next tick; disable to publish black; release ownership to restore native lighting. |
| 13 | `device.stats()` | 306014: free stock heap, Lua used/peak memory, frames, callbacks and dropped keys. |

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

- Source: 1–8,192 bytes, text only, uploaded in at most 512-byte chunks.
- Lua memory: a fixed 40 KiB arena. The quota includes allocator headers,
  alignment and temporary allocations during realloc. Stop/error frees the whole
  arena without running Lua finalizers. 306014 also requires at least 24 KiB
  of spare native heap beyond the arena before launching. Other native tasks
  can still allocate afterward; this is a startup reserve check.
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
is not a preemptive interrupt over arbitrary C code. No app automatically starts
at boot in this image: power cycling returns to the native firmware.

## Coverage and remaining work

This is an app platform foundation, not complete parity with every built-in
feature. The implementation and physical tests cover a custom clock, framebuffer
ownership, keyboard input, timers, persistent-in-RAM state, messaging and recovery.
Snake has a provisional numeric key map plus explicit `up/down/left/right/reset`
messages; its physical direction labels still need mapping on this device.

| # | Capability | Current state |
| --- | --- | --- |
| 1 | Drawing, animations and custom games | Implemented primitives; clock and Snake examples. Custom fonts/palettes can be represented in Lua/packed RGB data. |
| 2 | Controls and lifecycle | Physical input, held state, ownership, pause/resume/stop/reload implemented. |
| 3 | Clocks, timers, stopwatch, scoreboards | RTC, timers and drawing implemented. Native sleep/wake alarms and stock notification precedence remain unbound. |
| 4 | Keyboard lighting | 12 independently controlled RGB LED positions, custom effects and ownership restoration implemented in 306014. |
| 5 | Audio | Volume only. Playback, samples/tones, recording, microphone levels/spectrum need verified native bindings. |
| 6 | Installed apps, assets and settings | RAM only. Filesystem isolation, atomic writes, app installation, modules and safe boot selection remain to implement. |
| 7 | Communications | BLE upload, bidirectional app messages and status implemented. USB script upload is not implemented. |
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
# Requires an already-installed 306015; stops the current app, performs no flash writes.
python3 scripts/check-lua-app-device.py B1:21:81:DD:B8:9B \
  --output firmware/runs/lua-app-check
```

Build prerequisites and the earlier one-shot checks are in
[306012 documentation](lua-306012.md#reproduce). The builder pins stock firmware,
Lua sources and original hook bytes, checks code/RAM boundaries and bootloader
CRC preservation, and rejects accidental semihosting system calls. The updater
separately pins the resulting image's SHA and size. The build report deliberately
states offline status; [306013 hardware evidence](../firmware/lua-app-evidence/verification.json) and
[306014 hardware evidence](../firmware/lua-io-evidence/verification.json)
are separate. Raw runs stay in ignored `firmware/runs/`.

To reproduce the previous 306013 image exactly, build the source at commit
`9a9d89c`; use `59b5e28` for 306014. The current builder produces 306015.
All three images remain pinned.

The image reserves 16 KiB of native globals below `0x2004c000`; text starts at
`0x1ca000`, initialized data loads at `0x1ee000`, and neither crosses `0x1f0000`.
Hooks wrap heap initialization, command 0x37, screen output and the ADC scanner's
return at `0x2d490`. 306014 also wraps the LED flush at `0x7580c`;
when ownership is released it executes the original native path. Intercepting after native key-action mapping loses key-down
records whose stock action is zero; the ADC hook precedes that filtering.

The command prefix is `7f DLUA`, followed by operation: 0=status, 1=one-shot,
2=stop, 3=begin upload (u16 length, u8 resident), 4=chunk (u16 offset, bytes),
5=commit upload, 6=pause, 7=resume, 8=incoming message, 9=read/ack outgoing.
ABI 2 replies start with `DLUA`, ABI, state, request error, result length, then
little-endian u32 peak memory, work units, current memory, frames, callbacks,
dropped keys, held bits and generation, followed by result bytes. States are
0=idle, 1=running, 2=done, 3=error, 4=active, 5=paused, 6=uploading.
