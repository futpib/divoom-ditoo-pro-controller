# Stock sounds, fonts and images

Firmware **306026** adds an `assets` catalogue and native image/glyph drawing.
The resources stay in their existing flash locations. Scripts do not bundle
copies of them, and reading or drawing them does not switch stock display modes.

Browse the [image gallery](stock-assets.html) locally to choose an image ID.
It contains 598 independently decodable 16×16 frames found in pinned stock
306007: status/menu icons, game graphics, notification pictures and individual
animation frames. Duplicate frames are retained. This is not a complete inventory
of procedural graphics, frames requiring a previous palette, or uploaded files.
IDs are stable within this catalogue; they are not vendor protocol IDs.

| # | API | Result |
| --- | --- | --- |
| 1 | `assets.count('image'\|'sound'\|'font')` | 598 images, 14 sound modes, one flash font. |
| 2 | `assets.info('image', id)` | IDs 1–598; `id`, `width`, `height`, `colors`, `delay_ms`. The delay is stored metadata; drawing does not start animation. |
| 3 | `assets.image(id)` | One 768-byte RGB888 string, compatible with `display.blit`. |
| 4 | `display.image(x, y, id [, transparent_rgb])` | Draw directly into the app framebuffer, clipping at the screen. Optional exact RGB color key leaves matching pixels unchanged. |
| 5 | `assets.info('font', 1)` | `id`, `width=16`, `height=16`, `glyph_bytes=32`. |
| 6 | `assets.glyph(codepoint)` | One 32-byte monochrome glyph: 16 columns, each a little-endian u16 with bit 0 at the top. Space returns a blank glyph. |
| 7 | `display.glyph(x, y, codepoint, rgb)` | Draw the glyph's set pixels; background pixels stay unchanged. |
| 8 | `assets.info('sound', mode)` | Modes **0–13**, matching `audio.preview`; returns `id` and the stock `native_id`. |
| 9 | `audio.preview(mode, volume)` | Existing queued native playback, volume 0–100. Wait for `device.result(ticket)`; use `audio.stop()` to stop. Resident app required. |

Drawing calls return `true` or `nil, reason`. Resource reads return data or
`nil, reason`; invalid types, IDs, colors and coordinates raise Lua errors.
Coordinates range from −16 to 16. Call `display.present()` after drawing.
You may use the drawing/read APIs from resident or one-shot programs. Sound
requests retain the existing native main-task queue, priority and cleanup rules.

```lua
display.clear(0)
assert(display.image(0, 0, 1))
display.present()

-- Draw a native Cyrillic glyph in cyan over the current image.
assert(display.glyph(0, 0, 0x0416, 0x40c0ff))
display.present()

-- Keep a copy only when the script needs to edit or reuse its pixel data.
local rgb = assert(assets.image(1))
display.blit(0, 0, 16, 16, rgb)
```

Sound modes map to native IDs `41, 41, 34, 35, 33, 36, 32, 37, 38, 39, 41,
40, 43, 42`. There are aliases, so 14 modes does not mean 14 different clips.
Sound names and dependencies on uploaded resources are not fully decoded; the
catalogue does not invent names or assert that every mode is audible. Mode 6 is
the previously owner-confirmed audible sound. Previews have a native 60-second
limit; `audio.stop()` also pauses native music. See [audio behavior and evidence](lua-peripherals.md#speaker-and-microphone).

The native character map supports printable ASCII plus these BMP ranges:
`00A1–02AF`, `0370–052F`, `0600–06FF`, `0750–077F`, `0E00–0E7F`,
`1100–11FF`, `1E00–1FFF`, `2E80–2FDF`, `2FF0–2FFF`, `3000–30FF`,
`3130–318F`, `31C0–31EF`, `31F0–331F`, `4E00–9FA5`, `AC00–D7FF`,
`FF00–FFEF`. Surrogates and unmapped characters return `nil, 'glyph unavailable'`.
The range map identifies allocated slots; a slot may still contain a blank glyph.
This API accepts numeric codepoints, not UTF-8 strings or text shaping.

## Memory and recovery

The image offset index is read-only firmware data: 1,272 bytes from firmware
306027, using 19 group bases and 598 relative 16-bit offsets (2,392 bytes in
306026). The native regression checks all 598 decoded images against the
independent stock decoder. Compressed image
pixels are read from their existing memory-mapped stock locations. Font reads
use one aligned 256-byte stack buffer from partition 3 (`0x1f3000`, size
`0x119000`); only the selected 32-byte glyph is copied out. Images use a 768-byte
stack buffer. There is no permanent native RAM allocation or native heap
allocation for these APIs. Lua API tables/strings still consume Lua memory, and
retained `assets.image()`/`assets.glyph()` results count against its 48 KiB quota.
Direct drawing avoids allocating the pixel data in Lua.

Each read/draw consumes the existing 32-native-calls-per-callback budget.
Decoders have fixed loop bounds and participate in instruction/time checks.
The app never selects a flash address, file handle or native pointer. Font reads
validate the pinned partition layout and use the stock SPI mutex. Missing glyphs,
layout mismatches and driver errors return without drawing. Image decoding
finishes before any framebuffer mutation. Existing stop, USB control, held-key
escape and error cleanup remain available.

## Try it and reproduce

```sh
python3 scripts/build-lua-app-runtime.py
python3 scripts/firmware-manifest.py firmware/306026-lua.MVA --check
cargo build --locked --release --no-default-features
target/release/divoom-ditoo-pro-controller --transport usb firmware-update firmware/306026-lua.MVA
target/release/divoom-ditoo-pro-controller --transport usb lua start examples/lua/stock-assets.lua
target/release/divoom-ditoo-pro-controller --transport usb lua send 'image 1'
target/release/divoom-ditoo-pro-controller --transport usb lua send 'glyph 0416'
# Optional: a quiet sound request, stopped after about 300 ms of reported playback.
target/release/divoom-ditoo-pro-controller --transport usb lua send 'sound 6'
```

Any key advances the browser to the next image. It starts silently and requires
an explicit message to play sound. Starting it temporarily replaces the running
Lua app but does not change the saved startup app or settings.

`scripts/stock-assets.py` verifies the stock MVA SHA, scans for complete independent
palette frames, checks lengths and every palette index, and generates the compact
C index and HTML gallery. `--check` verifies generated files without writing;
the firmware builder checks the index automatically. Native font mapping is
traced from `0x51c10`/`0x51c84`, column orientation from `0x6936c`/`0x695ca`,
and sound mapping from `0x3648c` with its table at `0x15dd80`. All addresses here
are decoded stock-code addresses, not MVA offsets.

Host sanitizer tests compare every image against an independent Python decode,
exercise glyph addressing and read failures, clipping/transparency, invalid IDs,
and runaway-call recovery. They also run the existing standalone TV app and
Bluetooth/USB tests. On-device rendering and font contents require the hardware
check; a successful offline build does not establish those results.
The [recorded verification](../firmware/lua-assets-evidence/verification.json)
currently covers offline tests and reproducible builds. The device was unavailable
over USB, so 306026 has not been flashed or tested on hardware.

The repeatable hardware check reads all 598 images, checks their combined pixel
hash and direct drawing, reads/draws Latin/Cyrillic/Chinese glyphs, stops the app,
checks reclaimed Lua memory, and executes a fresh program. It plays no sound.
The supplied restore app is started again even after a check failure:

```sh
python3 scripts/check-lua-assets.py --output firmware/runs/assets-check \
  --restore-app examples/lua/tv-keyboard.lua
```

Add `--dry-run` with a fresh output directory to prepare and validate the sequence
without USB. Neither form changes the saved startup app or settings.
