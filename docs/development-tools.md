# Firmware and Lua development tools

Run these commands from the repository root. Inspection, decoding, sequence
validation, and manifest checks are offline. Generated captures belong in
ignored `firmware/runs/`; the tools themselves live in tracked source files.

## Inspect firmware

`scripts/fw-inspect.py` replaces the scratch `target/lua-app/inspect.py`.
It defaults to decoded stock `firmware/decoded/306007/code.bin`. Use the existing
`firmware-decode firmware/306007.MVA --output firmware/decoded/306007` command if
that directory does not exist. Existing adjacent `code.nds32.S` is used when
available; otherwise `nds32le-elf-objdump` produces disassembly in memory.

```sh
python3 scripts/fw-inspect.py disasm 0x1139b0 0x1139c0
python3 scripts/fw-inspect.py xref 'divoom_screen_queue_send_rgb'
python3 scripts/fw-inspect.py xref --address 0x20006878
python3 scripts/fw-inspect.py callers 0x138bf6
python3 scripts/fw-inspect.py find --hex 'fc 01 f0 81 f1 01 84 46 fa 02'
python3 scripts/fw-inspect.py table 0x181284 --type u8 --count 15
python3 scripts/fw-inspect.py read 0x47924 4
python3 scripts/fw-inspect.py gp -- -27264
python3 scripts/fw-inspect.py --json disasm 0x1139b0 0x1139c0
```

Global options `--code`, `--asm`, `--base`, and `--gp` select other inputs and
load addresses. Addresses refer to decoded code, not MVA file offsets. `range`
is an alias for `disasm`; endpoints are exclusive. `table --strings` resolves
in-image pointers to at most 256 bytes. Out-of-image reads fail.

Cross-references recognize straight-line immediate construction and GP-relative
accesses. Register overwrites and branches end tracking. Callers are direct
`jal` sites (`--jumps` also includes direct jumps). These are candidate
references, not a complete control-flow analysis; missing matches do not prove
a feature is absent. An explicit assembly file must correspond to the code image
and address base supplied.

## Watch, sequence, and decode Lua

```sh
divoom-ditoo-pro-controller --transport usb lua watch --seconds 30
divoom-ditoo-pro-controller --transport usb lua watch --seconds 10 --interval-ms 200 --all
divoom-ditoo-pro-controller lua sequence examples/lua-sequence.json --dry-run
divoom-ditoo-pro-controller --transport usb lua sequence examples/lua-sequence.json
divoom-ditoo-pro-controller lua decode firmware/runs/example/replies.jsonl
printf '%s\n' '444c55410102000200000000000000003432' | divoom-ditoo-pro-controller lua decode -
```

Watch and sequence keep one transport connection open and always attempt to
disconnect on completion, error, deadline, or Ctrl-C. Watch defaults to 30 seconds
and emits state/result/generation/key changes; `--all` also emits every sample
with memory, frame and instruction counters. A runtime error is printed and
causes a nonzero exit. Observing a running app does not stop or replace it.
Watch/sequence time budgets start after connection setup. Delays longer than
five seconds perform status keepalives so USB's idle timeout does not expire.

A sequence is a JSON array. `examples/lua-sequence.json` is a read-only example.
This example intentionally replaces the app and then stops it:

```json
[
  {"op":"start","file":"lua/clock.lua"},
  {"op":"wait","state":"active","timeout_ms":5000},
  {"op":"pause"},
  {"op":"wait","state":"paused","interval_ms":100},
  {"op":"resume"},
  {"op":"sleep","ms":1000},
  {"op":"stop"},
  {"op":"wait","state":"done","memory_bytes":0}
]
```

Source paths resolve relative to the sequence file. Other steps are `run` with
`file`, `eval` with `source`, `send` with `message`, `status`, and `receive`.
`receive` acknowledges the outgoing message, like the existing CLI command.
Wait conditions `state`, `result`, `result_contains`, and `memory_bytes` are
combined with AND; at least one is required. Wait defaults to 5000 ms with a
200 ms poll. Each wait/sleep is bounded to 60 seconds; the whole sequence has
`--timeout` (default 120 seconds, maximum 3600). All steps and source files are
validated before connecting. Every emitted result includes its one-based step.

A failed step prevents subsequent steps. Timeouts/interruption disconnect the
host; they do not undo earlier actions or automatically stop the resident app.
An interrupted write may already have executed, so no mutation is retried.

Offline decode accepts raw-run JSONL, decoded status/watch/sequence JSONL, and
one hex payload per line. It skips unrelated JSON records, reports source line
numbers, and fails on malformed Lua payloads or an input with no Lua replies.
Wire decoding shares `src/lua.rs`, including its ABI-dependent header size.

## Synchronize firmware registrations

```sh
python3 scripts/firmware-manifest.py firmware/306019-lua.MVA --check
# After rebuilding the selected image and its adjacent JSON report:
python3 scripts/firmware-manifest.py firmware/306019-lua.MVA --update
# A future version needs an explicit constant name:
python3 scripts/firmware-manifest.py firmware/306021-lua.MVA --register MY_FEATURE --check
```

The default is `--check`: print a unified source diff and exit 1 for drift,
0 for consistency, or 2 for invalid inputs/source anchors. `--update` applies
the diff. `--root PATH` supports temporary checkouts. `--builder PATH` also
checks that the selected builder's output filenames, report version, and version
instruction agree. Older retained images need not match the current builder.

The image and adjacent build report must already agree on version, SHA-256,
size, and wire checksum. The tool checks the pinned stock prefix/driver, package
and internal CRCs, length header, and embedded native version before proposing
changes. It supports Lua images 306012 onward; it does not bless arbitrary MVA
files or change firmware bytes or the build report.

Existing registrations synchronize hash, size, checksum assertions, and USB
application length/block expectations. A new named registration adds the Rust
image and Lua allowlists, restore boundary, USB test case, and device-check/storage
diagnostic version lists. It preserves old registrations. Source layouts it
does not recognize fail explicitly. Review the diff, run the normal build/tests,
and verify the device separately; registration is not hardware verification.

## Check the tools

```sh
python3 -m unittest discover -s scripts -p 'test_f*.py'
cargo test --locked --no-default-features lua_tools
```
