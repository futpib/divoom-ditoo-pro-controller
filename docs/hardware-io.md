# Ditoo Pro hardware I/O

This inventory distinguishes model documentation from tested custom-firmware
access. The official [Ditoo Pro manual, pages 1–5](https://cdn.shopify.com/s/files/1/0082/4105/3814/files/DitooPro-English-User-Manual.pdf?v=1784095580)
is linked from Divoom's [manual library](https://divoom.com/pages/product-manual).

| # | Interface | Evidence and custom runtime status |
| --- | --- | --- |
| 1 | Six keyboard keys, lever, audio-source button, power button, reset pinhole | Listed in the manual. Physical Lua key events work; the complete physical-to-ID mapping remains unfinished. The power-button scan is left native. Reset is a hardware control, not a Lua event. |
| 2 | 16×16 RGB screen | Custom Lua drawing and resident clock physically confirmed. |
| 3 | Keyboard RGB lighting | 12 independently controlled RGB positions physically confirmed with a chase and key-triggered color changes. |
| 4 | Speaker output | Manual specifies 15 W. 306016 exposes volume, native source/playback controls, alarm previews and memo playback. No arbitrary PCM/synthesis API. |
| 5 | Microphone input | Manual documents on-device voice-memo recording and noise detection. 306016 binds native noise readings and bounded voice-memo capture. USB microphone operation remains unverified. |
| 6 | Bluetooth | Audio and control connections documented; BLE control, firmware transfer and resident Lua upload/messages tested. |
| 7 | USB-C | Charging, observed audio/HID interfaces and tested application flashing. See [USB details](usb.md); USB Lua upload is not implemented. |
| 8 | TF/microSD slot | Manual documents offline music playback. Lua filesystem access is not implemented. |
| 9 | Battery/charging status and indicator | Manual documents battery indication and charge-completion behavior. 306016 exposes the native 0–7 level, power/charging/full status and an override for the five indicator LEDs. |

See the [native peripheral API](lua-peripherals.md) for exact limits and completion semantics.

The internal RTC and flash are also relevant resources for apps. RTC calendar
reads work from Lua; internal filesystem metadata has a verified read-only
[diagnostic](lua-storage.md). Persistent Lua files and safe autostart remain
unimplemented.

The official FAQ says the Pro does not include a microphone while comparing it
with the karaoke-oriented Ditoo-Mic. That wording does not override the Pro
manual's explicit on-device recording instructions; it should not be used as
proof that the Pro lacks an internal sound input.

Divoom's current [product page](https://divoom.com/products/divoom-pro) advertises
FM radio, but a working tuner has not been verified on this unit. No working
motion, ambient-light or temperature sensor, exposed GPIO/UART, Wi-Fi, or other
expansion interface has been established. Shared firmware/SDK symbols alone
are insufficient evidence for extra physical hardware.
