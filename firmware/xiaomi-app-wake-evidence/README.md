# Xiaomi service-UUID wake from Lua

One controlled trial woke the physical MiTV-MOEU0 / Android 14 TV using
`examples/lua/tv-wake.lua` on Ditoo firmware **306043**. The laptop controlled
Ditoo over Classic RFCOMM while Ditoo used BLE for the TV. No Ditoo firmware
was flashed for this change. Packet construction lives in `lua/wake.lua`;
addresses come from saved remote settings or a `wake ADDRESS ...` message.

## Matching firmware

The TV build was
`Xiaomi/ladybird/ladybird:14/UKN1.240806.017/V816.0.25.8.19.03.1.ULBMIXM:user/release-keys`.
The matching `vendor_dlkm/lib/modules/btmtk_usb_unify.ko` has SHA-256
`d87f5119441d605d745752f38e36b2411ea101071328da30d6c2c75941f40d8d`.
The retained [filter record](verification.json) includes command bytes and
section-relative disassembly addresses. These are decoded driver commands,
not a capture of the sleeping TV controller's internal state.

The RC filter at index `0x1c` requires an allowlisted public sender, the `MI RC`
name, and manufacturer high byte `01`. The live `rclist` contained the stock
remote, not Ditoo. This explains why replaying the RC payload under Ditoo's
own identity did not satisfy that filter.

The APK filter at index `0x1b` matches a full service UUID with feature mask
`0x0004`; it does not require a sender-address match. The UUID's advertising
bytes are `00 01 02 01 05 03 ff 00 01`, then the TV's six Bluetooth address
bytes in reverse order, then `00`. For the test target `0C:CD:B4:D0:0C:26`,
the complete packet was:

```text
02 01 06 11 07 00 01 02 01 05 03 ff 00 01 26 0c d0 b4 cd 0c 00
```

This is the UUID `000ccdb4-d00c-2601-00ff-030501020100`. The same encoder works
with other target addresses. Pairing is unnecessary for this filter's packet
match, but this hardware trial used a previously paired Ditoo. Wake on an
unpaired transmitter and acceptance by other TV models have not been tested.

## Physical trial

An initial trial was discarded: the owner pressed the stock remote's Power
button, so its `bt-gpio` result could not establish Ditoo wake.

For the second trial, the owner authorized one sleep/wake cycle and was asked
to leave the remote untouched. The laptop sent `power` to the ordinary Lua
remote over RFCOMM, then temporarily started the wake app. After a 30-second
sequence delay it sent `wake`; the app waited twelve seconds and requested a
three-second nonconnectable burst at a 100 ms interval.

- The capture received **11 HCI advertising reports** with the exact packet
  from Ditoo's random BLE identity. They span capture times 59.520954 to
  61.627955 seconds; MGMT copies are not counted as additional packets.
- The capture also received 697 stock-remote HCI reports. Every captured
  stock-remote payload had manufacturer field `03 ff 00 00`, not the
  `03 ff 00 01` required by its wake filter. A laptop capture is not an
  exhaustive record of all over-the-air traffic.
- The TV became unreachable during standby, then returned with suspend success
  count **3 → 4**, wake reason **`bt-gpio`**, and Android wakefulness **Awake**.
  The owner confirmed **“it's on”**.
- At the post-wake read, TV RTC time was `1792849387` and the pending fallback
  alarm was `1792849415`: the TV was awake **28 seconds before** that deadline.
  This comparison uses the same TV clock and does not depend on laptop clock
  alignment. The temporary alarm was subsequently cleared and read back empty.
- Android's recorded wall-clock sleep and wake events were 18:41:48 and
  18:42:36. Its uptime-based last-sleep/last-wake values exclude deep suspend
  and therefore do not measure wall-clock time asleep.

Together these establish one successful controlled wake on this TV. They do
not establish reliability across repeated cycles or other firmware/models.
The wake app's `WAKE SENT` result itself only means local transmission completed.

## Restoration and coexistence

The ordinary remote was restored with `lua start examples/lua/tv-keyboard.lua`.
Over laptop RFCOMM it returned `MEDIA: GoogleTV2511` and an outbox status of
`2 0C:CD:B4:D0:0C:26 public`, confirming an active TV HID connection alongside
serial control. Saved startup source, settings and bonds were preserved.
The temporary TV probe helper used earlier in the investigation was removed.
No further sleep test was performed.

Wake advertising remains a separate temporary app. Attempts to add it directly
to the ordinary remote exceeded the 48 KiB Lua arena in the patched 32-bit VM;
those changes were reverted. The normal Sun binding remains HID Power.
Use the [documented commands](../../docs/lua-advertising.md) to run the wake app
and restore the ordinary remote afterward. No production native code changes
are included in this implementation.

## Host validation

`python3 scripts/test-lua-app-runtime.py` passed the full native suite. New tests
run the bundled wake app in the real patched Lua VM and advertising engine,
using the existing controller/HID doubles: known packet vectors, malformed
targets without partial transmission, duplicate and multiple targets, no
transmission on startup, cancellation, busy-link preservation, advertiser
restoration and unchanged saved settings. These are host tests, separate from
the physical trial above. `bash scripts/check-lua.sh` passed formatting and lint.

Raw local captures and transcripts are retained in ignored
`firmware/runs/xiaomi-app-wake/`; their hashes are in `verification.json`.
They are not distributed because they include unrelated nearby radio traffic.
