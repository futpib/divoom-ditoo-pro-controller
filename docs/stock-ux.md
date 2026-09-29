# Stock Ditoo Pro UX patterns

The stock Ditoo Pro uses **modes, small icons, and context-dependent buttons**.
These are the interactions documented in the official manual, rather than a
complete hardware-tested inventory of firmware 306007.

| # | UX pattern | Stock behavior |
| --- | --- | --- |
| 1 | Consistent menu navigation | **M** opens/exits menus; **←/→** browse; the **lever** selects or starts. |
| 2 | Direct shortcuts outside menus | **+/−** adjust volume, arrows change tracks, lever toggles playback, lighting key cycles display functions. |
| 3 | Short press versus hold | Holding **+/−** adjusts brightness; holding **M** toggles keyboard lighting; holding the lever starts voice recording. |
| 4 | Controls change with context | In games or coloring, the four keys become directional controls. Alarm setup assigns **+** and **−** to hours and minutes. |
| 5 | Nested feature selection | Browse Music, Coloring, Tools, Alarm or Games, then select a source, tool or game within that category. |
| 6 | Icon-based feedback | Recording and message states use pictograms; pressing a button also reveals battery status. |
| 7 | Holds can perform special actions | Holding the lever inside alarm settings deletes the alarm. Double-pressing the source button disconnects Bluetooth. |

Source: [official Ditoo Pro manual, pages 1–3](https://cdn.shopify.com/s/files/1/0082/4105/3814/files/DitooPro-English-User-Manual.pdf?v=1784095580),
available from [Divoom's manual library](https://divoom.com/pages/product-manual).

## Implications for Lua apps

The recommended conventions to preserve are **M = back/menu, arrows = browse,
lever = confirm/action**. This is design guidance, not a requirement imposed by
the Lua runtime or a description of every existing example's bindings.

Secondary actions use holds and context. Preserve familiar physical controls
and document them without adding persistent button guides to the device screen.

## Physical key IDs

Lua receives the stock ADC ID before the native UI translates it. Firmware
306007's mapper at `0x75074` uses five 16-bit entries per ADC ID at `0x1812fc`
(one per event). Its short-release column gives:

| # | Control | Lua ADC ID | Native UI code |
| --- | --- | --- | --- |
| 1 | M | 0 | `0x61` |
| 2 | + | 1 | `0x16` |
| 3 | ← | 2 | `0x07` |
| 4 | → | 3 | `0x09` |
| 5 | Lever | 4 | `0x15` |
| 6 | Lighting | 7 | `0x67` |
| 7 | − | 9 | `0x0c` |
| 8 | Source | 10 | `0x6b` |

The main key callback at `0x4792c` routes `0x61` to menu/keyboard-light handling
at `0x47c66`, and `0x67` to display-mode cycling at `0x47a74`. The generic SDK
action table at `0x15ba0c` independently identifies volume, previous/next,
playback and source switching. ADC slots 5, 6 and 8 have zero native UI codes;
apps should not invent extra physical controls from those slots. These are
binary-derived mappings, not a new physical test of every key. Inspect locally:

```sh
python3 scripts/fw-inspect.py table 0x1812fc --count 55 --type u16
python3 scripts/fw-inspect.py disasm 0x4792c 0x47cf6
```

Use fixed printed controls rather than asking owners to assign them. Keep
button instructions in documentation; the device screen can show the selected
item, action feedback and connection state without persistent keybinding hints.
