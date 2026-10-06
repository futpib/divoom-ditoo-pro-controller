# Lua memory on the Ditoo Pro

Firmware 306030 accepts up to 16,384 source bytes with a 49,152-byte Lua arena.
It retains the allocator and streaming-parser improvements introduced in
306023. UI helpers remain ordinary Lua modules bundled on the host.

## 16 KiB source limit (306030)

The upload and saved-app ceiling is now 16,384 bytes. The 48 KiB Lua arena,
24 KiB native reserve, instruction/time guards and recovery controls are unchanged.
A larger source is not a promise that its compiled code or runtime data fits.
Source upload now checks native headroom as well as allocation success.

Installation copies source blocks into the saved record and releases each block
immediately. Native writing then needs the record, one writer copy, and one
page-rounded old record, rather than retaining the uploaded source too. A changed
16 KiB save requires about 74 KiB of native heap before the uploaded source;
low-memory operations fail before writing. Boot needs about 57 KiB available to
read the two generations and construct source blocks.

The stock journal collector can temporarily buffer live flash pages. Before it is
needed, the installer checks its allocation bound and invokes it separately from
the native writer. Sector-boundary padding is included in the space check. The
old app records are retired; this upgrade requires reinstalling the startup app.
See [saved storage](lua-storage.md) for the exact format and recovery limits.

On hardware, three distinct 16 KiB sources saved and reloaded through the native
Saved app menu. Journal compaction reduced used pages from 963 to 209, followed
by another successful reload. All 43 device guard/recovery checks passed.
The current remote bundle is 9,940 bytes. The native sanitizer test with sixteen
simulated bonds peaks at 48,064 Lua bytes, under the unchanged 49,152-byte ceiling.
These are source/storage and bounded-runtime checks, not a guarantee that any
16 KiB program fits the Lua heap. See the [306030 evidence](../firmware/lua-16k-evidence/verification.json).

From 306033, compilation and API setup have a separate 250 ms loading budget.
Top-level Lua execution and the `init` callback each receive a fresh 50 ms
budget, as later callbacks do. All phases retain the 100,000-work-unit limit;
memory limits and native recovery remain unchanged. This prevents cold-start
compilation from exhausting the saved app's first execution budget.

## What changed in 306023

- Shrinking a Lua allocation releases its unused tail. Previously the compiler
  could shrink an array while the allocator retained the original large block.
- Growing an allocation can consume adjacent free blocks without first reserving
  another block and copying the old data.
- Native API tables reserve their known function count before registration.
- Incremental garbage collection starts earlier: pause 120%, step multiplier
  200%, step size 1,024 bytes. It remains inside the existing execution guard.
- Source is buffered in blocks of at most 512 bytes. Lua's reader callback frees
  each consumed block when the parser requests the next one. Completion or error
  frees all remaining blocks and their index.

Uploads still buffer the full script before execution. Boot still reads the saved
record before constructing source blocks. This is not direct execution from flash
or a constant-memory upload. Compilation produces Lua VM instructions, function
prototypes, strings and debug metadata in RAM; that compiled representation stays
until the app stops. The original source is not retained after compilation.

The source blocks and their 132-byte index (68 bytes before 306030) are native
allocations outside the Lua arena. Native allocation headers also consume space. They overlap less with the
growing compiler heap because consumed blocks are released progressively. Native
Bluetooth, audio, the USB session buffer, the 16 KiB Lua task stack and the 8 KiB
reserved globals also live outside the arena. `device.stats().free_heap` reports
the shared native heap; `lua_used` reports occupied arena blocks and
`lua_reserved` reports the arena pages backing them. These are not interchangeable.

## Measurements

The same 7,828-byte TV bundle was tested on 306022 and 306023 over USB. Each
firmware completed five starts, followed by 60 samples of an 8,025-byte copy
with an added memory-reporting message handler. Values below are bytes.

| # | Measurement | 306022 | 306023 |
| --- | --- | ---: | ---: |
| 1 | Unmodified app, 500 ms after start; all five starts identical | 45,984 | 41,280 |
| 2 | Instrumented app, median occupied Lua blocks | 45,484 | 43,536 |
| 3 | Instrumented app, median reserved Lua pages | 48,128 | 45,056 |
| 4 | Instrumented app, maximum reported Lua peak | 48,112 | 47,496 |
| 5 | Instrumented app, maximum reserved Lua pages | 49,152 | 49,152 |
| 6 | Native sanitizer workload, occupied blocks after 1,000 ticks | 47,448 | 42,744 |
| 7 | Native sanitizer workload, reserved pages after 1,000 ticks | 49,152 | 44,032 |

Startup saves 4,704 bytes (10.2%). Sustained median reservation saves 3 KiB,
but peak reservation can still reach the existing cap. This does not establish
that a larger source or Lua quota would be safe. Native free heap varied with
Bluetooth/runtime state across the reboot, so it is not a controlled savings
measurement. The native harness uses fixed simulated native heap and ticks;
its results are not device RAM measurements.

The old saved app autostarted after flashing, all 43 hardware guard/recovery
checks passed, and the module/UI assertion sequence passed on-device. See the
[verification report](../firmware/lua-memory-evidence/verification.json) for
image identity, bounds, preservation checks and limitations. Raw captures and
private configuration backups remain in ignored `firmware/runs/` directories.

## Reproduce

```sh
python3 scripts/build-lua-app-runtime.py
python3 scripts/test-lua-app-runtime.py
cargo test --locked --no-default-features
cargo build --release --locked --no-default-features
divoom-ditoo-pro-controller --transport usb firmware-update firmware/306030-lua.MVA
python3 scripts/check-lua-app-device.py --transport usb --firmware 306030 \
  --output firmware/runs/memory-guards
python3 scripts/check-lua-memory.py --output firmware/runs/memory-profile
```

These commands exercise the current app and firmware. Reproducing the historical
306022/306023 comparison requires the matching older revision and its 7,828-byte
remote bundle; the current remote exceeds those versions' source limit.

The memory profiler temporarily starts the bundled TV app five times, then runs
an instrumented copy for 60 samples, checks its menu and restores the unmodified
running app. It does not write saved apps or Bluetooth bonds; the current app may
migrate its own settings format. It replaces
any other running app with the TV app. Compare firmware versions with identical
source, settings, sampling parameters and USB connection state. Bluetooth/native
activity and garbage-collection timing can still affect individual samples.

The native sanitizer harness checks 5,000 randomized allocator operations, data
preservation, quota/accounting, source-allocation failure rollback, reader-block
release, tokens spanning blocks, syntax-error cleanup, full-size persistence,
the TV app's native key dispatch, and guard recovery. Hardware guard tests also
cover infinite loops through protected calls and coroutines, allocation exhaustion,
callback loops, a full 8 KiB upload and recovery through an independent command.

The 50 ms / 100,000-work-unit guard, 24 KiB native allocation reserve, held-key
boot escape and one-shot autostart behavior are unchanged. The runtime still has
no MPU/process boundary; these limits do not make arbitrary native bugs impossible.
