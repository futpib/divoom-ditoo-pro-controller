# On-device Lua runtime

The experimental 306012 image adds Lua 5.4.9 to the SHA-pinned 306007 firmware.
Programs travel over Bluetooth and execute on the Ditoo's NDS32 processor. A new
program does not rebuild or flash firmware. Hardware verification is recorded
separately below; an offline build alone is not device proof.

## Commands

```sh
# One runtime installation over USB (see docs/usb.md for permissions).
divoom-ditoo-pro-controller --transport usb \
  firmware-update firmware/306012-lua.MVA

# Subsequent program changes are RAM uploads, not firmware updates.
divoom-ditoo-pro-controller --device B1:21:81:DD:B8:9B --transport ble \
  lua eval 'local sum=0; for i=1,100 do sum=sum+i end; return sum'
divoom-ditoo-pro-controller --device B1:21:81:DD:B8:9B --transport ble lua run example.lua
divoom-ditoo-pro-controller --device B1:21:81:DD:B8:9B --transport ble lua status
divoom-ditoo-pro-controller --device B1:21:81:DD:B8:9B --transport ble lua cancel

# Restore the archived stock firmware.
divoom-ditoo-pro-controller --device B1:21:81:DD:B8:9B --transport ble \
  firmware-update firmware/306007.MVA --restore-stock
```

The CLI checks for installed version 306012 before sending the extension, since
stock firmware assigns other meanings to nonzero selectors of command 0x37.
It prints a JSON result and returns a failure exit status for a Lua error.

## First runtime API and limits

Language features include local variables, functions, tables, arithmetic, strings,
conditionals and loops. The initial global functions are `type(value)`,
`tostring(value)`, `brightness(0..100)` and `volume([0..15])`. Calling `volume()`
reads the current volume. The last returned scalar is included in the result.
A returned table/function is reported as `non-scalar result`.

This is intentionally a small device API. Standard libraries such as `io`, `os`,
`package`, `debug`, `string`, `math` and `table` are not opened. Lua table syntax
still works. There are no physical-key callbacks, persistent scripts, automatic
startup programs, timers or drawing API yet. Each run creates a fresh Lua state;
programs and results do not survive power loss. Brightness/volume changes use the
stock device functions and can outlive the Lua state.

- Source: 1–2,048 bytes, text only; Lua binary chunks are rejected.
- Lua requested allocation sizes: 32 KiB; the peak is returned with the result.
  Stock allocator headers, alignment padding, temporary reallocations, and the
  separate C-library/worker allocations consume additional memory.
- Execution: 20,000 VM instructions; cancellation checked every 100 instructions.
  A count hook yields out to the native worker, so an infinite loop cannot keep
  the Bluetooth task occupied. This is an instruction bound, not a real-time
  guarantee for every native C operation.
- Numbers: 32-bit integers and 32-bit floating point.
- Lua value stack: 512 entries; nested C/parser calls limited to 20.
- Worker stack: 4,096 words (16 KiB), allocated lazily through stock FreeRTOS.
- Results: at most 191 bytes, plus status, allocation peak and instruction count.

The interpreter is isolated by its allocator and API, not by a hardware process
boundary. It shares firmware privileges. Recovery from a nonbooting image remains
unproven; the prior version-gate round trip proves recovery from a working custom
image only.

## Target: replace the built-in applications

The intended endpoint is an on-device application runtime capable of replacing
all device-side built-in features: clocks, games, animations, timers, alarms,
scoreboards, lighting, audio tools and custom applications. The current 306012
runtime is a proof of bounded execution and two device bindings; it does not
meet that endpoint. PC/phone editors and cloud services are outside this runtime.

Lua should own application behavior while native firmware supplies FreeRTOS,
Bluetooth/USB, key scanning, display refresh, audio processing and storage.
The interpreter supports host-defined C bindings and resumable execution; see
the [Lua embedding interface](https://www.lua.org/manual/5.4/manual.html#4).
The work is in device integration, application lifecycle and resource management.
A wrapper that merely launches a built-in game would not establish that a new
game can be implemented in Lua.

The following is a target capability map, **not an implemented API contract**:

| # | Capability | Current Lua | Required for the target |
| --- | --- | --- | --- |
| 1 | Drawing and animations | Brightness only | Own a 16x16 framebuffer; pixels, clear, lines, rectangles, text/fonts, sprites, palette/frame operations and present. |
| 2 | Physical controls | None | Key down/up, held state, combinations and repeat/hold events; consume or pass through events deliberately. |
| 3 | Time and scheduling | None | Monotonic time, RTC/calendar reads, periodic callbacks, timers, alarms and appropriate power/wake events. |
| 4 | App lifetime | Fresh state for each short run | Keep state between events; start, suspend, resume, stop, replace and switch apps; isolate script errors and reclaim resources. |
| 5 | Keyboard lighting | None | Set/read available lighting controls and run custom effects at the granularity the hardware exposes; per-key RGB is not established. |
| 6 | Audio | Volume read/write | Playback/track controls, samples or tones, recording controls and microphone/level/spectrum events for games, mixers, noise meters and visualizers. |
| 7 | Persistence and assets | RAM-only source | Install/list/delete apps; load assets; save settings/high scores; select a boot app and start without a host. |
| 8 | Communications | Upload source, poll result, cancel via Bluetooth | App messages, remote input and data feeds; report errors/state and reload programs without firmware replacement. USB script upload is not currently implemented. |
| 9 | Lua libraries | Only type/tostring plus two device functions | Selected base, math, string, table, coroutine and text helpers; package-scoped modules and logging. |
| 10 | Stock settings and modes | Volume and brightness | Bind confirmed device controls and queries, and define precedence between apps, alarms, notifications, stock modes and recovery controls. |

### Execution and ownership

The current 20,000-instruction limit applies to the entire program. Hitting it
produces an error and closes the Lua state. A long-running app instead needs
bounded work per callback or resumable time slice, with a persistent state and
event queue. The scheduler must yield to Bluetooth/audio tasks, bound event
backlog and avoid blocking native calls. Increasing the lifetime instruction
limit alone does not implement an application scheduler.

A custom app must be able to claim display/input so stock clock/game tasks do
not redraw its screen or also act on its keys. Application stop/error must
release that ownership. An intentional recovery key action must remain usable
independently of Lua, including for a broken boot app. Timers and native callbacks
must be unregistered before unloading a state. RTC wake and background alarms
need native support when the Lua task is not running.

Useful acceptance cases, in dependency order:

1. Upload a custom clock and a game such as Snake. Each uses Lua drawing and
   timing; the game receives real physical key events. Switch between them
   without flashing, retain state between ticks, and continue with Bluetooth
   disconnected. Verify that stock redraw/key actions do not interfere.
2. Install both apps and their assets into verified storage. Select a boot app,
   power-cycle, recover from a deliberately broken boot script, and update an
   app without replacing firmware. Preserve stock settings and existing files.
3. Cover the remaining built-in categories using the same primitives: timers,
   stopwatch, scoreboard, alarm/reminder UI, animations, keyboard effects,
   audio controls, mixer/sound tools, recording and reactive visualizers.
   Establish each native binding on this model rather than assuming that an
   Android protocol symbol or a related SDK example proves it works.
4. Run sustained clock/game/audio workloads; measure frame timing, input latency,
   free heap, allocation failures and cancellation/reload behavior. Keep the
   communication and recovery paths usable throughout.

### Hardware and investigation limits

A packed RGB888 16x16 framebuffer is 768 bytes (1,536 for two buffers), so the
screen itself is inexpensive. Lua table representations cost more; native packed
buffers and drawing/audio helpers should do bulk work. Game performance and a
sustainable frame rate have not yet been measured.

The 2 KiB source, 32 KiB Lua allocation and lifetime instruction limits are
choices in this runtime, not language limits. Larger chunked program uploads,
assets loaded on demand and a measured memory budget are needed. The previous
104,384-byte free-heap measurement predates the full runtime and is not a promise
of spare app RAM during Bluetooth audio. Worker stacks, native allocations and
allocator overhead also consume heap.

The existing native image is tight too. Its built ELF contains 119,784 bytes of
runtime text/read-only data and 720 bytes of initialized data. The current linker
layout has 27,672 bytes before the data-load address and 7,472 bytes after that
data before `0x1f0000`: about 34.3 KiB combined, in separate regions. Opening
libraries and adding native APIs must be budgeted; expansion may require code
size work or a revised layout. The stock user-data region is occupied until
proven otherwise and must not be treated as free app storage. TF/SD storage is
an avenue to investigate, not an implemented persistence backend.

The exact Ditoo framebuffer/present, key dispatch, RTC, audio and filesystem
entry points still need tracing, ABI/threading checks and hardware validation.
The related SDK is useful for orientation but is not the Divoom application
source. Clocks and small games are plausible targets; complete built-in parity
is a goal, not a verified property of this firmware.

Weather, notifications and internet radio still need their external data/source
provider. A custom local interface can use data delivered by a phone or PC;
adding Lua does not itself add internet connectivity or missing hardware.

## Reproduce

The native build uses `nds32le-elf-gcc` 15.2.0, binutils 2.45.1, and newlib
4.5.0.20241231. On Arch these are packages `nds32le-elf-gcc`,
`nds32le-elf-binutils`, and `nds32le-elf-newlib`. The Lua source archive is vendored
with its upstream SHA-256. The scripts require Python 3.12+ and no source downloads.

```sh
python scripts/build-lua-probe.py
python scripts/build-lua-runtime.py
python scripts/test-lua-runtime.py
cargo test --locked --no-default-features
cargo build --locked --release --no-default-features

# On a device already running 306012; this performs no firmware writes.
python scripts/check-lua-device.py B1:21:81:DD:B8:9B --output firmware/runs/lua-check
```

The builder validates the exact stock image and original instruction bytes,
checks section boundaries, and recalculates the application change marker,
full-code CRC and package CRC. The updater independently pins the resulting SHA
and size; changing runtime source requires rebuilding and deliberately updating
that allowlist. The bootloader executable and bootloader CRC remain unchanged.

For faster BLE firmware transfers, the optional root-only helper
`scripts/ble-connection-interval.py DEVICE` requests a 7.5 ms connection interval
on an already-connected target. It resolves the HCI handle by MAC and requires a
matching successful controller event; it does not reset Bluetooth or connect to
other devices. Example during an active transfer:

```sh
sudo python scripts/ble-connection-interval.py B1:21:81:DD:B8:9B
```

The observed BlueZ disconnect stall can occur after a valid reply. The device
checker has an optional `--kernel-disconnect` flag for targeted `sudo -n btmgmt`
cleanup between checks, as in the earlier firmware round-trip runner.

## Memory and integration evidence

The live 306009 diagnostic extension reported 104,384 free heap bytes and a
90,576-byte largest free block, with five free-list entries. The response also
confirmed stock `$gp = 0x2000d2f8`. This is a snapshot of this device in its tested
state, not a guarantee under every audio/game mode. The retained
[probe reply](../firmware/lua-runtime-evidence/heap-probe.jsonl) includes its request.
To repeat the diagnostic, install the pinned `306009-lua-probe.MVA` and query
`raw send 0x37 --data 7f444c554100000000 --query` before installing the runtime.
The diagnostic is a separate image and does not run Lua.

Stock allocator initialization at `0x854c4` defines one region from
`$gp + 0x9300` through `0x2004c000`. Runtime 306012 lowers its upper boundary to
`0x2004a000`, reserving 8 KiB for the runtime and its C-library globals. The stock task entry guarantees only four-byte stack alignment; an assembly
trampoline aligns the Lua worker stack to eight bytes before entering C. Lua
allocations are also explicitly aligned to eight bytes. A hook at
`0x2ec58` calls the original heap initializer, copies runtime initialized data,
and clears runtime BSS. No script starts at boot.

The stock image's header declares user data at `0x1f0000` (offset `0xb4`, mirrored
at application offset `0x100b4`). The related SDK calls this header field user
data; its example's separate `0x1d0000` FlashFS constant is not this image's
boundary. Lua text starts at `0x1ca000`, after the stock image ends at `0x1c9f70`.
Initialized data loads from `0x1ee000`. Linker assertions prevent either section
crossing `0x1f0000` and prevent RAM globals crossing the original heap end.

Command 0x37's handler at `0x3ab9c` routes the `7f 44 4c 55 41` (`7f DLUA`)
prefix to the runtime. Selector zero keeps the ordinary version query; other
legacy selectors retain their stock handler. Runtime operations after the magic
are 0=status, 1=run (u16 little-endian source length, then source), 2=cancel.
Responses begin `DLUA`, ABI byte 1, state byte, request-error byte, result length
byte, u32 peak allocated bytes, u32 instruction count, then result bytes.
States are 0=idle, 1=running, 2=done, 3=script error.

## Hardware verification

On 2026-09-28 the device accepted 306012, rebooted, and reported 306012 through
its ordinary version query. The transfer took 287.14 seconds at an average
7,055 bytes/second. See the [flash events](../firmware/lua-runtime-evidence/flash.jsonl).

All [12 live script checks](../firmware/lua-runtime-evidence/script-checks.json)
passed: strings, integers, fractions, numeric string conversion, a second program,
instruction exhaustion, syntax errors, allocation exhaustion, recursion overflow,
a full 2,048-byte source upload, and successful execution after those errors.
No firmware writes occurred between programs. The memory-exhaustion test returned
`not enough memory` at a 32,751-byte Lua allocation peak. The loop test returned
`instruction budget exceeded` at 20,000 instructions.

The [control checks](../firmware/lua-runtime-evidence/control-checks.json) uploaded
`examples/lua/sum.lua` and returned 5050, then used Lua to set brightness 7 and
volume 4. Independent stock protocol reads confirmed both values. Lua restored
brightness 0 and volume 3, also confirmed by stock reads. Each record retains the
CLI arguments and replies for reproduction.

The initial 306011 execution exposed a numeric-formatting error caused by the
stock task's four-byte stack alignment. The
[pre-fix results](../firmware/lua-runtime-evidence/pre-fix-numeric-checks.json)
are retained; the eight-byte task-entry trampoline and integer-specific formatting
in 306012 passed the integer and fractional tests. The earlier 306010/306011
images are not supported installation targets.

Host validation passed 41 Rust tests, production-target Clippy with warnings
denied, and the native ASan/UBSan execution, quota, and allocation-failure checks.
The native probe and runtime were also
[rebuilt byte-for-byte from an isolated source copy](../firmware/lua-runtime-evidence/reproduce.log).
Build JSON reports describe offline generation; these separate device records
supply hardware verification.

These checks do not establish physical-key integration, long-duration operation
under simultaneous audio/game workloads, cancellation of a running program over
Bluetooth, or a 306012-to-stock restoration round trip. The earlier
306008-to-stock round trip is the retained restoration evidence.
