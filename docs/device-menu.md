# Standalone device menu

Firmware **306028** adds a saved-app launcher and persistent settings to the
stock menu. Press **M**, browse with **←/→**, and select with the **lever**.
**M** goes back. The added screens show the selected item and action feedback,
with scrolling labels where needed; they contain no keybinding prompts.

The original categories and their handlers remain in place:

```text
Music
  Bluetooth
  SD card
  USB audio
Coloring
Tools
  Scoreboard / stopwatch / noise meter / countdown (stock order)
  Voice memo
    Record / Stop recording
    Play / Stop playback
    Delete → No / Yes
Alarm
Games
Saved app
Settings
  Autostart → On / Off
  Remove app → No / Yes
  Bluetooth → Remote only / Speaker and remote / App control
  Saved devices → address → Forget device → No / Yes
  Key lights → App and stock / Stock lights / Off
  Battery indicator → Auto / Off
  USB mode → Audio and control / Charge and control
```

USB audio was already in the stock Music menu. Its existing handler is retained;
when USB audio is disabled, selecting it opens USB mode settings instead.

## Entering and leaving Lua

**Saved app** reloads the installed source from flash and starts a fresh VM. It
also works when autostart is off. An empty slot shows **NO SAVED APP**.
`app.menu()` stops the calling Lua app and opens the native main menu after its
worker has released its resources. The TV app exposes this through **M → Exit**.
From the TV remote, press M, move three items right to EXIT, and press the lever.
In the native root, Saved app is one item before Settings; select it to return.
The existing five-second keyboard-key hold now stops Lua and opens that menu;
the held key's release is consumed so it cannot accidentally select an item.
The native power key and alarm priority are preserved.

Autostart defaults to on; holding a keyboard key during boot still skips it for
that boot. Turning autostart off keeps the installed source. Removing the app
requires an explicit **Yes** and retains app settings, Bluetooth bonds and
system preferences. An app error still releases ownership without a restart
loop. The native launcher does not depend on a functioning Lua script.

## Settings and peripherals

| # | Setting | Behavior |
| --- | --- | --- |
| 1 | Bluetooth | Remote only hides the native audio services and retains HID. Speaker and remote restores them. App control (the default) lets apps choose this policy. A saved choice overrides `keyboard.mode()`; `keyboard.status().mode_locked` tells an app to respect it. |
| 2 | Saved devices | Shows stored Bluetooth addresses without exposing link keys. Forgetting defaults to No and targets the captured address, not a shifting list index. A selected active keyboard is disconnected first. Active audio profiles must be disconnected before forgetting. |
| 3 | Key lights | App and stock retains existing behavior. Stock lights enables native lighting and ignores Lua LED ownership. Off suppresses both. |
| 4 | Battery indicator | Auto retains native/Lua indicator behavior; Off overrides it. Battery sensing and charging are unchanged. |
| 5 | USB mode | Charge and control selects the existing `8888:171e` vendor-HID-only descriptor set. There are no USB audio interfaces; normal control and bootloader flashing remain available. Audio and control enables USB audio and selects it when a cable is attached. |

USB mode changes cause re-enumeration; reconnect a host controller after the
device reappears. With audio enabled, selecting a different music source can
still use the stock control-only identity. The native USB storage identity is
left intact. Early boot exposes control-only descriptors until saved settings
have been read, so charge-only mode cannot briefly advertise a speaker.

Voice memo uses the native recorder and playback implementation. Recording is
bounded to 60 seconds, stops on leaving the memo menu, and needs the existing
native heap/storage headroom. Recording can replace the existing memo; Delete
requires Yes. The existing native write and recording limits still apply.

Preferences use model `0xd7`, slots **4/5**, separately from saved-app slots 0/1
and app-settings slots 2/3. They use the same CRC, alternating-bank recovery,
foreign-record refusal, write-rate limit and readback checks. Browsing and
cancelling never write preferences. Flashing this firmware preserves the native
configuration and filesystem partitions.

## Build and diagnostics

```sh
python3 scripts/build-lua-app-runtime.py
python3 scripts/test-lua-app-runtime.py
python3 scripts/firmware-manifest.py firmware/306028-lua.MVA --check
cargo build --release --locked --no-default-features
target/release/divoom-ditoo-pro-controller --transport usb firmware-update firmware/306028-lua.MVA
python3 scripts/device-menu.py open
python3 scripts/device-menu.py key left
python3 scripts/device-menu.py key select
python3 scripts/device-menu.py status
python3 scripts/device-menu.py screenshot target/menu.ppm
```

`status` reads state; `open` stops Lua and opens the menu; `key` navigates and can
confirm actions. `screenshot` reads the added menu's RGB framebuffer, not a
camera image. The native root menu retains its original timeout. Opcode 14 of
the `0x37 / 7f DLUA` extension accepts exactly three bytes: action (0 read,
1 open, 2 key), key (ADC IDs 0/2/3/4/7), and framebuffer chunk (0..5). Its
160-byte `DMNU`, ABI 1 reply contains state/preferences in the first 32 bytes
and 128 RGB bytes. No pointers or arbitrary native calls are accepted.

The host updater also handles the existing stock boot-entry quirk in which the
old USB identity remains until a bus reset; see [USB recovery](usb.md). This
does not change the bootloader or relax image verification.

The build replaces the double-precision dependency of Lua's number parser with
a fixed-stack binary32 parser. Decimal and hexadecimal conversion retain
round-to-nearest-even behavior, including subnormals, overflow and long exact
midpoint tails. ASan/UBSan differential tests cover the boundary cases and
randomized inputs. The app source, memory, instruction and callback time limits
remain unchanged. The image remains below the original staging/flash boundary.
The launch headroom check includes source payload that is progressively freed
during parsing. Each arena allocation still enforces the full native heap reserve.
The USB bridge now allocates its command frame only while receiving/dispatching
a command, returning up to 4,100 bytes between commands. The full packet limit
and reply-ring capacity are unchanged. Together these changes allow the saved
TV app to start while a USB control session is open.

See the [306028 verification report](../firmware/menu-evidence/verification.json)
for the exact image, flash readback, menu checks, USB descriptors, 43 device
safety/recovery checks, five consecutive app starts and memory measurements.
Physical button wiring and audible memo capture/playback were not retested;
destructive menu actions were cancelled on hardware and covered in native tests.
