# Lua linting and formatting

Run these commands from the repository root:

```sh
scripts/check-lua.sh        # Check formatting and lint; does not edit files.
scripts/check-lua.sh --fix  # Format files, then lint them.
```

The script checks maintained apps in `examples/lua/`, shared modules in `lua/`
when present, and `.luacheckrc`. Historical firmware evidence, vendor sources,
generated bundles, and runtime test fixtures are outside its scope. It never
connects to the device. CI runs the same check.

## Install the tools

CI pins [StyLua](https://github.com/JohnnyMorganz/StyLua) to **2.5.2** and
[Luacheck](https://github.com/lunarmodules/luacheck) to **1.2.0**. On Arch Linux:

```sh
sudo pacman -S --needed stylua luacheck
```

For matching versions on other systems, install through Cargo and LuaRocks
(the latter needs a Lua interpreter and development headers):

```sh
cargo install stylua --version 2.5.2 --locked --features lua54
luarocks --lua-version=5.4 install --local luacheck 1.2.0-1
```

Ensure both executables are on `PATH`. Editor integrations for either tool use
the repository's `.stylua.toml` and `.luacheckrc`.

## What the checks enforce

StyLua uses Lua 5.4 syntax, two-space indentation, single quotes where practical,
and a 100-column target. Both check and fix modes use `--verify` to reparse the
formatted result and check syntax-tree equivalence.

Luacheck catches accidental globals, unused variables, unreachable code, and
misspelled device API names such as `display.tex`. Its configuration lists the
firmware's native tables and fields explicitly and treats them as read-only
for app code. It rejects unavailable libraries (`io`, `os`, `package`, `debug`,
`utf8`), dynamic loading functions, and removed string functions such as
`string.format`. `print` is allowed because firmware redirects it to app logging.
`require` is allowed for host-side module bundling; Luacheck does not validate
module paths or whether a particular `require` call can be bundled.

These are static checks. They do not check API argument types, prove timing or
memory bounds, or replace runtime and hardware tests. Device watchdogs remain
responsible for stopping runaway scripts.

When changing the native Lua API in `native/lua-app/runtime.c` or
`native/lua-app/peripherals.c`, update `.luacheckrc` alongside it. Keep APIs
explicit rather than permitting arbitrary globals or disabling warnings. The
configuration has tests that exercise both valid scripts and common mistakes:

```sh
python3 -m unittest discover -s scripts -p 'test_lua_tools.py'
```
