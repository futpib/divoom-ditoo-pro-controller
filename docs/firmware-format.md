# Decoding Ditoo Pro firmware 306007

The vendor MVA is a record container containing a small control record, an opaque
flash-driver blob, and directly readable Andes NDS32 firmware. The main code does
not need decryption or decompression. This analysis does not recover original C
source or establish that the device accepts modified firmware.

## Reproduce offline

```sh
divoom-ditoo-pro-controller firmware-decode firmware/306007.MVA
divoom-ditoo-pro-controller firmware-decode firmware/306007.MVA --output firmware/decoded/306007
```

The output directory must not already exist. Extraction writes complete record
payloads, a manifest, and ASCII strings with original file offsets. For the exact
archived 306007 image it also writes `code.bin`, `bootloader.bin`,
`application.bin`, and `flash-driver-encoded.bin`. The last filename denotes an
opaque blob: its encoding/encryption has not been decoded. Extracting the code
has no connection to the device and does not relax the updater's image allowlist.

The general container parser validates lengths, record count, magic, and package
CRC. Internal layout interpretation is deliberately limited to the exact known
306007 SHA-256; a different valid MVA gets records without assumed code splits.

## Container layout

All integer fields are little endian. The header is four magic bytes
`4d 56 b1 58` followed by a one-byte record count (`3`). Each record has a one-byte
type, a four-byte payload length, and that many payload bytes. Addressed record
payloads begin with a four-byte address included in their payload length.

| # | Record offset | Type | Payload length | Meaning in this image |
|---|---|---|---|---|
| 1 | `0x000005` | 1 | 3 | Control bytes `35 ba 69` |
| 2 | `0x00000d` | 3 | 1,524 | Address 0 + 1,520-byte flash-driver blob |
| 3 | `0x000606` | 2 | 1,875,828 | Address 0 + 1,875,824-byte code image |

The trailer at `0x1ca57f` is `b6 f9 00 00`: CRC-16/XMODEM, stored in a 32-bit
little-endian field. CRC covers every preceding byte, starts at zero, uses
polynomial `0x1021`, and has no final XOR. Recalculation gives **0xf9b6**.
There are no unaccounted trailing bytes or separate signature record.

## Code image

The code starts at MVA offset `0x60f`, flash/code address zero. Instructions use
big-endian encoding; scalar data and pointers are little endian. Its initial
vector instruction is `48 00 0e 4e`, decoded as `j 0x1c9c`. Another vector table
at code offset `0x10000` starts the application (`j 0x11d08`).

| # | MVA offset | Code address | Length | Contents |
|---|---|---|---|---|
| 1 | `0x60f` | `0` | 65,536 | Bootloader region, including zero padding |
| 2 | `0x1060f` | `0x10000` | 1,810,288 | Application, embedded libraries and data |

The used bootloader length is `0x9e80` (40,576 bytes). Code header offset `0xe0`
contains this 24-bit length followed by the low byte of the sum of its three
length bytes. Offset `0xd0` similarly contains the full length `0x1c9f70` and
check byte `0x2b`.

The bootloader's MVA construction routine at `0x4528` copies a 13-byte prefix,
appends a type-3 record of length `0x5f4`, copies its embedded driver at `0x934c`,
and appends the type-2 code record. The embedded driver is byte-for-byte identical
to the package's 1,520-byte driver. This independently confirms the extraction.

Internal CRC-16 uses the same polynomial and seed, but skips mutable header
fields. Concatenate these code-relative ranges, with exclusive ends:

```
[0, 0xa4), [0xa8, 0xbc), [0xc0, 0xcc), [0xd4, 0xe4), [0xec, length)
```

For bootloader length `0x9e80`, CRC is **0x5f08**, matching the 32-bit field at
`0xbc`. For the whole code length `0x1c9f70`, CRC is **0x97e8**, matching `0xcc`.
All three independent CRC checks pass on the archived file.

## What the code reveals

- The bootloader identifies itself as `0.5.1`, built November 9, 2021.
- The application embeds paths under
  `MVsB1_BT_Audio_ditoopro_SDK_v0.1.12+P01/MVsB1_Base_SDK`.
- Divoom-specific display, keyboard, games, file management and update logic live
  alongside Bluetooth A2DP, AVRCP, HFP, PBAP, SPP and BLE SDK components.
- Game function strings include snake, tetris, tank, flappy bird, frogger,
  galaxian, gobang, slot machine, car, plane and others. These are compiled into
  this code image, not separate game packages in the MVA.
- The bootloader parser at `0x2b48` checks code size, an internal magic word
  `0xb0bebdc9`, an encryption-related byte, and CRC/version changes. The magic
  checker at `0x2f40` explicitly compares the four container magic bytes.
- The encryption-related comparison at `0x2c94` examines incoming code byte
  `0xff` against a saved device value. This image has `0xff` there; the flag's
  complete meaning and underlying ROM enforcement remain unresolved.
- The bootloader has a no-upgrade-needed return path, but its return value must
  not be equated with the Bluetooth `0x98` status byte. A subsequent [application-handler trace](firmware-update.md#device-side-rejection-decoded)
  independently establishes that our equal-version announcement produces the
  Bluetooth refusal `00 02`.

The inspected package and bootloader paths show CRC and compatibility checks,
not public-key signature verification. This does **not** prove that every
ROM/flash-driver path accepts modified code: the opaque driver and on-chip ROM
still need analysis. No modified image was flashed.

## Disassembly and evidence

A local GNU binutils 2.40 build with target `nds32le-elf` produced the disassembly:

```sh
nds32le-elf-objdump -D -b binary -m nds32 -EL firmware/decoded/306007/code.bin > firmware/decoded/306007/code.nds32.S
```

`-EL` describes scalar data; the NDS32 disassembler handles instruction byte order.
A raw linear sweep also interprets tables and strings as instructions; restart
at known function entrypoints for focused analysis. See the retained
[parser and CRC disassembly](firmware-analysis/306007-parser.nds32.S) and
[decoded manifest](firmware-analysis/306007.json).

Source corroboration comes from the MVsilicon SDK mirror at commit
`8105bd864b04995d81c9f9ae77cb158259f39015`:

- [SDK update CRC routine](https://github.com/leadercxn/bp1048_sdk_v0.1.12/blob/8105bd864b04995d81c9f9ae77cb158259f39015/BT_Audio_APP/bt_audio_app_src/apps/bt_obex_upgrade.c)
- [NDS32 linker layout](https://github.com/leadercxn/bp1048_sdk_v0.1.12/blob/8105bd864b04995d81c9f9ae77cb158259f39015/BT_Audio_APP/nds32-ae210p.ld)
- [ROM update interface](https://github.com/leadercxn/bp1048_sdk_v0.1.12/blob/8105bd864b04995d81c9f9ae77cb158259f39015/MVsB1_Base_SDK/driver/driver/inc/rom.h)

The SDK is related source evidence; Divoom's own binary and matching CRCs establish
the exact offsets above. No SDK binaries need to be executed to use the decoder.

For shell/interpreter findings and a one-time runtime design, see
[custom program support](firmware-runtime.md).
