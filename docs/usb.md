# USB capabilities and upgrade entry point

Live descriptors for the connected Ditoo Pro (`8888:171e`, product string
`Ditoo usb audio`) show USB full speed (12 Mbps), one configuration and one
vendor-specific HID interface. There are no non-control endpoints, so this
personality communicates through endpoint-zero control transfers.

Its report descriptor declares vendor usage page `0xff00`, usage `0x55aa`,
256-byte input/output reports and eight-byte feature reports. It does not expose
a USB audio interface, CDC serial port, mass-storage interface or ordinary
keyboard in this mode. A product string containing “audio” is not evidence that
its current configuration can stream USB audio. See the retained
[live descriptors](firmware-analysis/usb-8888-171e.txt).

Divoom advertises [USB-C audio playback](https://divoom.com/products/divoom-pro).
The exact binary contains USB speaker/microphone/audio-mode code. The related SDK
also has selectable USB audio, consumer media-key HID, storage and combined
configurations. Those SDK alternatives are not proof that every one is reachable
on this model. Neither a general computer-keyboard interface nor USB forwarding
of every physical key has been demonstrated.

## Firmware updates

This is more than an upgrade-related string: the exact stock 306007 binary has
the feature-report handler in executable code:

- At `0x7a3b8`, a host-to-device feature request clears an upgrade flag. A payload
  starting with `0x55` arms it when four SDK/header version bytes differ; a
  payload starting with `0xaa` arms it unconditionally.
- At `0x7a34c`, a device-to-host feature request checks that flag. If armed it
  replies with `0x55` and calls `0x2e4d0` with resource selector `0x40`.
- The related SDK identifies that call as
  `start_up_grate(AppResourceUsbDevice)`.
- USB input/output reports route through the vendor communication channel; the
  SDK calls this the online audio-tuning HID channel.

See the [bounded exact-binary disassembly](firmware-analysis/306007-usb-upgrade.nds32.S)
and the matching [SDK handler](https://github.com/leadercxn/bp1048_sdk_v0.1.12/blob/8105bd864b04995d81c9f9ae77cb158259f39015/MVsB1_Base_SDK/driver/driver_api/src/otg/device/otg_device_standard_request.c).

The controller currently implements Bluetooth firmware transfer, not USB
firmware transfer. USB entry/transfer/recovery has not been tested, and no
upgrade-triggering feature report was sent during this investigation. Even a
GET_FEATURE request can enter the updater if an earlier request armed it.
USB descriptor reads above do not invoke that path. The next USB task is to
trace the bootloader-side transfer framing and the PC download utility before
attempting a USB round trip.
