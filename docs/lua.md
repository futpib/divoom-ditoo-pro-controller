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
