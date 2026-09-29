# Shared Lua modules

Modules live in the repository, not in firmware. `lua start`, `lua run`, and
`lua install` bundle local imports automatically before contacting the device.
Sequence `start`/`run` steps use the same bundler. The device receives one text
chunk and needs no filesystem, package loader, or additional firmware update.
An installed bundle remains standalone after disconnecting the host.

For an app under `examples/lua/`:

```lua
local ui = require('../../lua/ui')
local view = ui.screen()
return {
  init = function() view.set('HI', 'THIS TEXT SCROLLS') end,
  update = function() view.draw() end,
}
```

See [ui-demo.lua](../examples/lua/ui-demo.lua) and the standalone
[TV keyboard](../examples/lua/tv-keyboard.lua). To inspect exactly what will be
uploaded, without connecting to a device:

```sh
divoom-ditoo-pro-controller lua bundle examples/lua/ui-demo.lua -o /tmp/demo.lua
wc -c /tmp/demo.lua
```

Omit `-o` to write the generated source to stdout. `lua eval` accepts a text
chunk directly and does not resolve imports.

## Import and size rules

- Use `require('literal.name')` with parentheses and one quoted literal.
  Dotted names resolve to `literal/name.lua`; paths such as `../ui` or `ui.lua`
  are also supported. Every import is relative to the file containing it.
- Canonical paths deduplicate imports, including different paths to one file.
  Modules share their returned value. General modules initialize on first use,
  once per app VM; `false` is cached and a `nil` return becomes `true`.
- Cycles, absolute paths, escaped import names, dynamic imports, and aliases or
  redefinitions of bare `require` are rejected. Bundled sources reserve `__dm`
  and `__dr`. Collection allows at most 32 files and 256 KiB of source, including
  dependencies; even unused imports must resolve successfully.
- Bundles discard comments and unnecessary whitespace, preserving literal
  contents. A file without imports stays unchanged if it fits; otherwise the
  host tries the same compaction. Syntax errors in compacted source refer to
  generated lines, so use `lua bundle` to inspect them.
- The **resulting text must fit 8,192 bytes**. This is the current native upload
  and saved-app record limit, not a Lua language or transport limit.
  Bundling cannot increase it. The separate Lua memory ceiling is 48 KiB;
  compiled functions, tables, and runtime allocations still consume that memory.
  Firmware 306023 consumes source in 512-byte blocks during compilation and
  frees each consumed block. Source does not remain resident; see
  [memory accounting and measurements](lua-memory.md).

## Tree shaking

The bundler recognizes modules consisting only of an empty local table, named
function declarations on that table, and a final return:

```lua
local m = {}
function m.used() return m.helper() end
function m.helper() return 42 end
function m.unused() return require('another.module').run() end
return m
```

For `local m = require('module'); m.used()`, only `used` and `helper` remain.
Unused exports and dependencies referenced only by those exports disappear.
Unused local imports of these modules disappear entirely. Multiple imports
combine their required exports and keep one module table.

Passing the module table around, returning it, or indexing it dynamically keeps
all exports. Modules with initialization statements keep their initialization
and lazy loading behavior. There is no unsafe "pure" annotation to override
that check. When every retained dependency has the simple form above, tables
are created directly at startup and the runtime loader is omitted. This is
conservative elimination, not arbitrary Lua dead-code analysis or variable
renaming.

## UI helpers

[lua/ui.lua](../lua/ui.lua) uses the native 3×5 font on the 16×16 display.
Coordinates and colors use the native drawing API; text should use supported
single-byte glyphs. Helpers do not clear or present the framebuffer unless
stated below.

| # | Function | Behavior |
| --- | --- | --- |
| 1 | `ui.elapsed(since[, now])` | Elapsed milliseconds across the native 31-bit clock wrap. Defaults to `time.millis()`. |
| 2 | `ui.center(y, text, color)` | Centers up to four characters; truncates longer text. |
| 3 | `ui.scroll(y, text, since, color[, speed])` | Centers short text; scrolls longer text with a bounded five-character drawing window. Speed defaults to 130 ms per pixel. |
| 4 | `ui.bar(y, value, total[, color[, width]])` | One-row progress bar, clamped to 0–100%; positive progress draws at least one pixel. Default width 15; maximum 16. Nonpositive totals draw nothing. |
| 5 | `ui.indicator(on[, x[, y]])` | Green/amber status pixel, default `(15, 6)`. |
| 6 | `ui.screen()` | Creates an independent title, scrolling hint, and redraw state. Returns the two functions below. |
| 7 | `view.set(title, hint[, color])` | Updates labels; changed labels reset scrolling and emit an app log. Default title color `0x20a0ff`. |
| 8 | `view.draw([connected[, value, total]])` | At most every 100 ms: clears, draws title at row 0 and hint at row 10, and presents. Optional connection pixel and progress bar at row 7. |

Use dot calls (`view.set`, `view.draw`), not colon calls. Views own independent
state but draw to the same framebuffer. Call primitive helpers directly when
an app needs its own layout or redraw schedule.

## Verification

`cargo test --locked` covers import resolution, compaction, conservative export
selection, and bundle limits. `python3 scripts/test-lua-app-runtime.py` also runs
the generated bundles inside the actual 32-bit runtime with ASan/UBSan, exercises
the TV app's native key dispatch, and checks framebuffer output and redraw timing.

The same module and UI assertions can run on the device. This replaces the
running app temporarily and leaves saved settings and startup source untouched:

```sh
divoom-ditoo-pro-controller lua sequence tests/lua/sequence.json --dry-run
divoom-ditoo-pro-controller --transport usb lua sequence tests/lua/sequence.json
divoom-ditoo-pro-controller --transport usb lua start examples/lua/tv-keyboard.lua
```

[Hardware verification](../firmware/lua-modules-evidence/verification.json)
records module/UI assertions on 306022, the running demo, and installation of
the bundled TV app with its existing settings preserved. Firmware was unchanged.
