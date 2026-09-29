# Readable firmware patches

The four retained builders apply named assembly sections to the exact stock
306007 image. Replacement instructions are assembled from source; Python checks
and applies the resulting sections. This refactor preserves each existing MVA
and its adjacent JSON report byte for byte.

## Where to read a change

| # | File | Purpose |
| --- | --- | --- |
| 1 | [`native/patches/manifest.toml`](../native/patches/manifest.toml) | Patch names, purpose, named site, expected stock bytes, overwrite budgets, build profiles and versions. |
| 2 | [`native/patches/stock-edits.S`](../native/patches/stock-edits.S) | Small stock instruction changes: version, reflash gate, heap reservation and wake fixes. |
| 3 | [`native/patches/stock-306007.ld`](../native/patches/stock-306007.ld) | Stock functions, globals, replacement sites and branch destinations. Addresses refer to decoded code, not MVA offsets. |
| 4 | [`native/lua-app/runtime-entry.S`](../native/lua-app/runtime-entry.S) | Assembly hooks and preserved stock instructions; added functionality remains in adjacent C files. |
| 5 | [`scripts/firmware_patches.py`](../scripts/firmware_patches.py) | Generic section placement, validation, application and review reports. |

For example, the version-gate edit is now:

```asm
.section .patch_allow_reflash,"ax"
    j8 stock_update_accept
```

Its manifest entry states why it exists, binds it to `stock_patch_version_gate`,
requires the original `c816` bytes and allows exactly two bytes of space. The
linker resolves `stock_update_accept`; no replacement opcode is handwritten.
`.flag verbatim` keeps the explicit instruction widths in `stock-edits.S`:
without it, the assembler can expand a symbolic jump into a long sequence.

The expected hex remains intentional: it guards the original vendor bytes.
Every byte in the reserved overwrite region is checked, including any tail
left unchanged by a shorter replacement. The applicator rejects wrong stock
hashes, empty/oversized replacements, out-of-range sites, overlapping reserved
regions, misplaced sections and unlisted hook/patch sections. It validates all
edits before modifying a copy of the input. Native ABI guards are in the same
manifest. Linker assertions continue to bound injected code and global RAM.

## Build and inspect

Use Python 3.12+ and the toolchain below. Normal builder commands still work:

```sh
python3 scripts/build-reflash-probe.py
python3 scripts/build-lua-probe.py
python3 scripts/build-lua-runtime.py
python3 scripts/build-lua-app-runtime.py
```

Each emits `patch-report.md` and `patch-report.json` next to its intermediate
ELF: `target/reflash/`, `target/lua/`, `target/lua/runtime/` and
`target/lua-app/runtime/`, respectively. Reports include:

- Stock/output SHA-256 and build-profile version.
- Each edit's purpose, named location, bytes used and overwrite budget.
- Before/after disassembly with known destination symbols annotated.
- Injected section sizes/addresses, global RAM use and remaining code/RAM space.

Generated reports remain under ignored `target/`. CI uploads them for review.
The separate adjacent firmware JSON reports retain their existing format.
The original version-gate probe now also requires the assembler/linker.

## Reproduce in the pinned container

```sh
docker build -t divoom-firmware-ci ci/firmware
mkdir -p target/firmware-review
docker run --rm --network none --user "$(id -u):$(id -g)" \
  -v "$PWD:/src:ro" -v "$PWD/target/firmware-review:/reports" \
  divoom-firmware-ci python3 scripts/check-firmware-repro.py --output /reports
```

The container pins an Arch base image by digest, uses the frozen 2026-09-28
[Arch archive](https://archive.archlinux.org/repos/2026/09/28/), and verifies
package archive SHA-256 values for GCC 15.2.0-1, binutils 2.45.1-1 and newlib
4.5.0.20241231-1. Host-side build dependencies come from that frozen repository.
The cross-compiler is the [Arch NDS32 package](https://archlinux.org/packages/extra/x86_64/nds32le-elf-gcc/).
Image construction needs network access; firmware builds run without it.

`check-firmware-repro.py` copies inputs into two differently named temporary
build roots. It runs all four builders and requires exact agreement with each
tracked MVA and JSON report, and agreement between both generated review reports.
It never rewrites the caller's firmware files. Output logs and verification
results go to `target/firmware-review/` by default. The same command can run
locally with the pinned toolchain installed.

CI runs this check and the patch/manifest unit tests on pushes and pull requests.
This covers the four current builder profiles; older resident-app images still
require their historical source revisions. Reproducing an image proves its bytes,
not new hardware behavior. This workflow performs no flashing.

## Add or change a patch

1. Add a meaningful site/continuation name to `stock-306007.ld` if needed.
2. Write replacement instructions in a named assembly section. Put larger new
   behavior in C and call it through a bounded assembly hook.
3. Add a manifest entry with purpose, full original overwrite-region bytes,
   section, named site and budget; select it in the appropriate profile.
4. Build and inspect the generated report. Profile versions are the source of
   truth for version instructions, output filenames and build reports.
5. For a behavioral change, choose a new profile version and deliberately
   register its generated image with `firmware-manifest.py`. Do the applicable
   hardware checks separately before describing the new behavior as verified.

`firmware-manifest.py --builder ...` understands the builder's `PATCH_PROFILE`
and checks its version against the manifest and the image's decoded version.
Changing a manifest hash merely to silence an unexpected reproducibility failure
would defeat the check; inspect the changed instructions and toolchain first.
