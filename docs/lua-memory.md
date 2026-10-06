# Lua memory on the Ditoo Pro

Firmware 306030 accepts up to 16,384 source bytes with a 49,152-byte Lua arena.
It retains the allocator and streaming-parser improvements introduced in
306023. UI helpers remain ordinary Lua modules bundled on the host.

## Lua API and compiler metadata (306039)

Native API tables are created on first ordinary global access and cached for
that VM. For example, a clock does not allocate the Bluetooth or microphone API
tables. `rawget(_G, 'keyboard')` and `pairs(_G)` do not discover an API before its
first access; use `keyboard` normally. The global metatable is protected.

After compilation, the runtime frees local-variable debug records and upvalue
names, then collects their otherwise unused strings. Bytecode, closure captures
and source line information remain intact, including line numbers in errors.
The TV app separates rendering from its update callback to reduce temporary
compiler allocations. Host tests cover nested captured values and error lines.

The 48 KiB Lua cap, 24 KiB native reserve, source size and execution budgets are
unchanged. Configured HID profiles use a small native allocation while enabled;
pending configuration copies are freed on completion, cancellation, timeout or
Bluetooth reset. Active profiles are freed on profile disable/reset, while an
app stop preserves the existing connection and its report format.

## Allocate only while used (306036)

The app profile now reserves 4,096 bytes of globals instead of 8,192. The current
link map uses 2,724 bytes of that reservation. The other 4 KiB returns to the
native allocator; the 48 KiB Lua limit and 24 KiB native safety reserve stay the same.

| # | Resource | Allocate | Release |
| --- | --- | --- | --- |
| 1 | 16 KiB Lua worker stack and native task record | When execution starts, after upload/save | After completion, stop or error; a retired worker is deleted by another task and the stock idle task reclaims its stack/record. Paused apps still own their VM and worker. |
| 2 | Lua arena directory, timers, key queue, input message and recovery context | At VM launch | On every completion, stop, OOM or other abort. Arena pages retain their existing incremental reclamation. |
| 3 | 768-byte framebuffer | First drawing/readback operation, or while an extension menu draws | App teardown or leaving the extension menu. A script that never draws needs no framebuffer. |
| 4 | Classic HID context and writable SDP attributes | First keyboard/profile request | After leaving remote-only mode and disconnecting; wait for closed channels/returned packets, unregister the manager/security/SDP/PSMs, then free. Stack recreation also drops the stale context. |
| 5 | BLE HID context | Enabling the BLE remote profile | Disabling it, after restoring its advertising identity; also on stack reset. A listening or connected remote still uses this context. |
| 6 | Bluetooth trace ring, 768 bytes | First diagnostic read | Explicit reader release or 15 seconds without a read. Event/IRQ hooks never allocate. |
| 7 | USB dispatcher context and 8 KiB reply ring | Opening a USB control session | Session close/replacement or the existing 15-second idle timeout. Input frames are sized to each command and freed after dispatch. |
| 8 | Storage diagnostic scratch, 432 bytes | Each diagnostic request | Before that request returns, including failure. |
| 9 | Uploaded source | Upload starts | Consumed by compilation, canceled/replaced, or abandoned for 30 seconds. Uploading and saving no longer require a worker stack. |

Math tables now live in flash except for eight bytes (`sqrtf`'s `one` and `tiny`)
that the toolchain addresses relative to the RAM global pointer. Moving the other
1,068 bytes does not require loading a math library when a script starts.

Small routing/status records, settings, USB interrupt mailboxes, LED output buffers
and C-library bookkeeping remain permanent. Interrupt handlers still have storage
before any app/session exists; native LED I/O may retain its published buffer.
A Bluetooth connection/listener or selected remote-only policy remains in use even
when Lua stops. This change does not disconnect a TV just to reclaim its profile.

The native bindings added here are `vTaskDelete` at `0x84c20`, connection-manager
unregister at `0x11dd76`, and security-record unregister at `0x1285ac`, checked
against stock 306007 disassembly. Unregister failures retain the context for retry;
no callback-owned object is freed while a channel/packet still references it.

Validation: the 32-bit ASan/UBSan suite exercises repeated worker/profile lifetimes,
failed allocation, partial unregister, deferred identity restoration, abandoned
uploads, trace expiry/release, storage scratch, native menus and the TV app with
16 simulated bonds. The TV fixture includes the 4 KiB returned by the smaller
static reservation when comparing native heap budgets.

306036 was flashed over BLE and its running version confirmed. All 43
guard/recovery checks and 60 demand-memory checks passed. Twelve repeated
worker cycles and cleanup after drawing, an abandoned upload, twenty storage
diagnostic reads, trace release and trace expiry returned to the same native
heap baseline. The fixed one-shot probe observed 71,368 native free bytes with
16,384 Lua bytes reserved; these readings include the probe's running worker.
They do not measure the idle worker-stack saving directly.

Fresh instances of the same resident app had identical Lua reservations;
the first draw consumed 788 native bytes including allocator overhead. Trace
activation consumed 800 bytes. Both allocations were fully reclaimed. Comparing
repeated telemetry calls inside one VM is insufficient: a new Lua arena page
also adds native allocation overhead not counted in `lua_reserved`.
See the [device memory checks](../firmware/demand-memory-evidence/device-memory.json)
and [guard results](../firmware/demand-memory-evidence/device-guards.json).
USB buffer lifetimes and Classic HID unregister still need physical checks;
their host sanitizer coverage is unchanged. Reproduce with:

```sh
python3 scripts/build-lua-app-runtime.py
python3 scripts/test-lua-app-runtime.py
python3 scripts/firmware-manifest.py firmware/306036-lua.MVA --check
python3 scripts/check-firmware-repro.py
python3 scripts/check-lua-app-device.py CONTROL_ADDRESS --firmware 306036 \
  --output firmware/runs/306036-guards
python3 scripts/check-demand-memory-device.py --device CONTROL_ADDRESS \
  --output firmware/runs/306036-memory
```

The two device scripts replace the running app in RAM without saving it. Restore
the installed app through the native Saved app launcher after testing. For a TV
that holds the BLE link, the [maintenance fixture](../firmware/ble-pairing-evidence/README.md)
can temporarily restore the public control identity without deleting its bond.

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
