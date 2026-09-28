# Numeric IDs, provenance and storage

`ids` lists categories; `ids CATEGORY` prints decimal IDs and names.
`ids --json` returns all categories with applicability, source and uncertainty;
`ids games --json` returns one category. These commands never connect to a device.
The compiled catalogue is [device-ids.json](../data/device-ids.json).

## What lives on the device?

The Android Bluetooth game path selects an existing game by ID and sends virtual
button presses/releases. It does not upload game code. This is evidence for
built-in firmware games, not proof of how their code/assets are laid out in flash.
No supported path for installing new executable games has been found. A firmware
update could change built-in games; generic file-transfer opcodes alone do not
establish an installable-game format or loader.

Images and animations are transferable content. Persistent slots and custom
resources require additional transfer workflows beyond displaying an image.
SD track IDs identify the contents of the inserted card and are not a fixed
firmware enum. Clock-face IDs may depend on firmware/resource versions; the
current face can be queried but there is no complete decoded face catalogue.
Tools, key codes, weather conditions and mode selectors are protocol values,
not separately installable applications.

## Sources

Inspected Divoom Android 3.8.40 (640), APK SHA-256
`d7ae490205cf71cc37f74948bd1ca7f1b2a446070565294b3ae835c0a51fde81`.
Sources below are under `com/divoom/Divoom/` in the APK. Previously missing
anonymous callback bodies were recovered by targeted JADX 1.5.6 decompilation
in its default mode rather than relying only on the earlier simple-mode output.

- `utils/DeviceFunction/DeviceFunction.java` selects GameTypeV5 for Ditoo Pro.
- `view/fragment/game/horizontal/GameHorizontalMainFragment.java`: `E2` provides
  the 15-entry V5 menu; `lazyLoad` associates labels; its item-click callback
  calls `GameModel.b(position + 1)`.
- `view/fragment/game/model/GameModel.java`: `b` calls
  `CmdManager.q2(true, (byte) id)` on Bluetooth. `CmdManager.q2` builds opcode A0
  with `[enabled, gameID]`. This verifies menu-to-wire mapping, not game execution.
- Game labels come from the default APK resources `game_two_txt_top` through
  `game_sixteen_txt_top`; the resource suffix is not the wire ID.
- `GameHorizontalMainFragment.onTouch` sends left=1, right=2, up=3, down=4,
  switch=5 through `F2`, then `CmdManager.u1/v1` (opcodes 17/21). Start/go calls
  `GameModel.d` and `CmdManager.w1` (88), not an invented sixth key code.
- `view/fragment/tool/StopwatchFragment.java` sends 0 to stop, 1 to start,
  and 2 in the reset-confirmation callback. `ToolServer.j` forwards these to
  `CmdManager.t3` unchanged on Bluetooth.
- `view/fragment/tool/model/ToolServer.java`: `h` calls
  `CmdManager.v3(enabled ? 1 : 2)` on Bluetooth. Its Wi-Fi branch sends 1/0;
  those are not the Bluetooth control values.
- `view/fragment/tool/model/ToolModel.java` and `CmdManager` define the tool
  selectors. `view/fragment/weather/model/WeatherUtils.java::returnType` maps
  OpenWeather icon codes to firmware condition IDs 1–18.
- Alarm slot IDs 0–9 are visible in the saved firmware-306007 response in
  [queries.jsonl](protocol-evidence/queries.jsonl). They are observed data from
  this unit, not an app-wide guarantee.
- Light sub-mode labels and repeat-mask meanings retain the existing CLI
  definitions; they are labeled accordingly rather than claimed as new
  hardware discoveries.

Reproduce targeted callback inspection with:

```sh
jadx -r --single-class com.divoom.Divoom.view.fragment.game.horizontal.GameHorizontalMainFragment \
  --single-class-output /tmp/GameHorizontalMainFragment.java divoom.apk
```

The catalogue also includes explicit unresolved entries for clock faces, SD
tracks/play modes, track directions, special/music effects, startup channels,
sleep modes and alarm modes/triggers. An empty mapping means unknown or dynamic,
not that the feature has no supported values. No games were launched or effects
changed to validate these names; source-defined mappings are labeled unverified
on hardware.
