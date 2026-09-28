# Custom programs without recurring firmware replacement

## Current findings

Firmware 306007 has no identified user-accessible shell, scripting interpreter,
or native application loader. This is an evidence-limited finding, not a proof
that every hidden command has been ruled out. No arbitrary code has been run on
the device. This investigation was offline and sent no device commands.

The related MVsilicon SDK includes a small UART diagnostic shell behind
`CFG_FUNC_SHELL_EN`; its example configuration leaves that define commented out.
Its commands show task/runtime/heap information and run a fixed test handler.
Even enabling this shell would not by itself provide arbitrary script execution.
Every distinctive shell command and banner checked is absent from our firmware,
including `displaytaskinfo`, `displayruntimestats`, `DisplayMemInf`,
`mv_shell_task`, `mv shell cmd list`, `SHELL`, and even `help`. This supports the
inference that this SDK shell was excluded from the Divoom build.

There are also no hits for the checked Lua, MicroPython, Python, Duktape,
JerryScript, JavaScript, ELF, or script-file markers. Stripped or custom
interpreters could escape this test. Ordinary Bluetooth commands, GIF/image
uploads, and file operations are not evidence of a programmable interpreter.

The named `divoom_microtask` subsystem is **native callback scheduling**. Its
machine code establishes:

- Worker entry `0x7516c` walks ten 16-byte slots, tests enable/deadline fields,
  locks a slot, and calls its function pointer at `0x751dc`.
- Registration entry `0x75354` stores its native callback argument at `0x753b8`.
- Initialization at `0x75498` allocates 176 bytes for this scheduler and creates
  its worker. That allocation is not available plugin memory.
- Observed registration call sites use compiled firmware addresses, directly or
  through wrapper `0x4cb14`. These include `0x50dd4`, `0x54f58`, `0x553d4`,
  `0x55ce0`, `0x5648c`, `0x67434`, `0x683dc`, `0x68848`, `0x71724`, `0x7215c`,
  and wrapper caller `0x4cac0`. No inspected registration path accepts uploaded
  bytecode or a user-selected native entrypoint.

See [marker results and addresses](firmware-analysis/306007-runtime.json) and
[disassembly](firmware-analysis/306007-runtime.nds32.S). Addresses are relative
to `code.bin`; add `0x60f` for offsets in the MVA.

## Native execution versus uploadable features

The processor already runs NDS32 native code. The SDK also has RAM-code sections
and remapping support. That makes native extensions an architectural possibility,
but does not establish an accessible stock upload-and-execute command, executable
heap memory, free RAM, a working custom-flash route, or recovery on this device.
USB PC-upgrade logic exists in the binary; it has not been shown to expose a
shell or a general RAM loader. Debug log strings do not prove a bidirectional
UART console is available on the board.

A native plugin loader would need an agreed memory map, executable placement,
relocations or fixed linking, correct NDS32 calling convention/global pointer,
cache handling, and versioned firmware API bindings. An arbitrary function
pointer call in a scheduler does not solve the loading problem.

## Proposed design for this device

The desired endpoint is a **one-time runtime installation**, followed by ordinary
program uploads and reloads. It is not implemented or hardware-verified yet.

1. Keep Bluetooth, audio, display and key scanning in the existing firmware.
2. Add a bounded interpreter and a small API: key press/release events, display
   drawing/present, timers, keyboard lights, and sending events to a connected PC.
3. Load programs into a separate RAM arena. Bluetooth uploads replace program
   data; they do not replace bootloader/application firmware.
4. Provide optional storage for programs in a verified dedicated data area or
   supported removable storage. Persistence still writes storage, but does not
   require rebuilding/reflashing firmware for each feature.
5. Schedule bounded VM work through a native callback; queue key events so slow
   scripts cannot block key scanning or Bluetooth/audio tasks. Define stack,
   memory and instruction limits and a way to disable a broken program at boot.

A compact bytecode interpreter is the initial design preference. Lua or another
larger language remains an option after measuring the device's actual free RAM,
flash space and execution budget. No runtime has been selected based solely on
SDK-wide memory constants. Native plugins can be added later if their speed is
needed; they require stronger ABI and lifecycle guarantees.

This design supports standalone custom games, keyboard mappings, animations and
logic. A PC-hosted program using stock display commands is another route that
needs no firmware replacement, but arbitrary physical-key forwarding has not
been established for this Ditoo Pro. It should not be presented as equivalent to
standalone programmable keyboard support.

## Required next proof

First establish controlled native entry and a recovery method, then demonstrate
a minimal callback without destabilizing the stock tasks. The known OTA version
gate is only the first gate: an accepted announcement does not demonstrate that
modified code will boot. Next measure memory, map key/display APIs, and prove a
second program can be uploaded and run without another firmware update. Building
a PC-only interpreter first would not resolve these device-side unknowns.

Related SDK evidence, pinned to commit `8105bd864b04995d81c9f9ae77cb158259f39015`:

- [UART shell](https://github.com/leadercxn/bp1048_sdk_v0.1.12/blob/8105bd864b04995d81c9f9ae77cb158259f39015/MVsB1_Base_SDK/middleware/rtos/freertos/src/shell.c)
- [Compile-time configuration](https://github.com/leadercxn/bp1048_sdk_v0.1.12/blob/8105bd864b04995d81c9f9ae77cb158259f39015/BT_Audio_APP/bt_audio_app_src/inc/app_config.h)
- [RAM/code layout](https://github.com/leadercxn/bp1048_sdk_v0.1.12/blob/8105bd864b04995d81c9f9ae77cb158259f39015/BT_Audio_APP/nds32-ae210p.sag)
