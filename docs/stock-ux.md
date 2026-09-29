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

The stock design's main weakness is discoverability: secondary actions require
memorizing holds and their context. Lua apps can retain the familiar navigation
while making those secondary actions visible in their on-screen instructions.
