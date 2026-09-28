# Android app feature gaps

Audit date: 2026-09-28. Compared Divoom Android **3.8.40 (640)** with the
Rust CLI at **ff5196c**, including the new BLE option and automatic fallback.
APK SHA-256: `d7ae490205cf71cc37f74948bd1ca7f1b2a446070565294b3ae835c0a51fde81`.

This is a source-backed inventory of the inspected APK, not a claim that every
server-gated, regional, account-specific or hidden runtime feature has been
exercised. JADX reported 65 decompilation errors; many anonymous callbacks are
not reconstructed. File/class names are supporting evidence, not proof that a
feature works on this physical unit. No phone UI was exercised in this audit.
The inventory therefore separates the explicit Ditoo menu, deeper controls,
phone/cloud features, and hardware-gated features.

The app explicitly routes both DitooPro and DitooPro5 through DitooProConsole,
TiViUiArch, SD/music/microphone/alarm support, and GameTypeV5 with multi-key
controls (`utils/DeviceFunction/DeviceFunction.java:900-1000`). Its discover
menu is enumerated in `view/fragment/discover/model/DiscoverModel.java:746-771`.
These are app capability declarations, not live tests of each function.

Already present in Rust: BLE/RFCOMM connection selection, discovery and paired
device listing, static images, animations, GIF/Divoom conversion, image debug,
static/scrolling text, local video rendering, brightness set, clock-ID get/set,
date/time set, device language set, volume get/set, play/pause, keyboard-light
next/previous/toggle, light/hot/special/music modes and a raw **box-mode payload**.
That last option is not an arbitrary command API and does not close the gaps.
Features that overlap are marked Partial and their missing portion is stated.

**Alarm defect:** `src/main.rs` logs the requested boolean but both branches
call `send_alarm(mac)` without it. `src/lib.rs::send_alarm` always serializes
slot 0, `enable: false`, 13:37, repeat 0, mode 0, trigger 0, volume 100. This
is an audit finding, not a fix made in this documentation commit.

Source IDs below refer to APK paths listed at the end. The machine-readable
[android-surface.json](android-surface.json) includes every Java file under the
app's fragment tree, extracted UI string references, CmdManager references,
and HTTP constant inventory. UI helper files are not counted as features.

## Ditoo Pro discover menu: all 20 tiles

| # | App feature | Rust gap | Applicability | Evidence |
| --- | --- | --- | --- | --- |
| 1 | Pixel-art designer | Partial: file upload exists; no interactive drawing canvas, live editing, layers, or drawing tools. | Ditoo profile | S001 |
| 2 | Animation editor | Partial: animation upload/conversion exists; no interactive frame/timing editor. | Ditoo profile | S002 |
| 3 | LED text/sign editor | Partial: static and scrolling text exist; no app-style text/effect composition UI. | Ditoo profile | S003, S004, S005, S006, S007, S008, S009 |
| 4 | Pixel coloring game | Missing: coloring templates, play/progress, and coloring-game creation. | Ditoo profile | S010, S011, S012, S013, S014, S015 |
| 5 | Music center | Partial: volume and play/pause exist; no local library, TF-card browser, radio directory, or playlist UI. | Ditoo profile | S016 |
| 6 | Music mixer | Missing: musical-instrument and sample-pad interface, multi-track composition, recording and playback. | Ditoo profile | S017, S018 |
| 7 | Voice recorder | Missing: record, preview and send recorded voice through the app/device flow. | Ditoo profile | S019 |
| 8 | Chat | Missing: app messaging and artwork/voice sharing. Hidden in kids mode. | Ditoo profile | S020 |
| 9 | Alarm scheduler | Broken/partial: the CLI alarm boolean is ignored; both values send disabled slot 0 at 13:37. No alarm editor, repeat days, sound or readback. | Ditoo profile | S021 |
| 10 | Sleep aid | Missing: sleep timer, light/sound choices and end-state settings. | Ditoo profile | S022, S023, S024 |
| 11 | Time planner | Missing: scheduled reminders with artwork and repeat rules. | Ditoo profile | S025, S026, S027, S028, S029, S030, S031 |
| 12 | Games | Missing: game selection and virtual game controls. The Ditoo profile selects 15 entries listed below. | Ditoo profile | S032 |
| 13 | Stopwatch | Missing: start/pause/reset and status display. | Ditoo profile | S033 |
| 14 | Celebrations / memorial dates | Missing: date events and associated display content. | Ditoo profile | S034, S035 |
| 15 | Power on/off schedules | Missing: schedule editing and repeat rules; not a claim that an unpowered unit can wake over Bluetooth. | Ditoo profile | S036 |
| 16 | Countdown | Missing: timer configuration, control and readback. | Ditoo profile | S037 |
| 17 | Noise meter | Missing: noise-level tool and its display/control flow. | Ditoo profile | S038 |
| 18 | Scoreboard | Missing: score editing and display control. | Ditoo profile | S039 |
| 19 | Notification forwarding | Missing: Android notification-listener bridge, per-app selection and device notification settings. | Ditoo profile | S040 |
| 20 | Weather | Missing: location/city selection, weather retrieval and device weather synchronization. Selecting a display submode does not fetch weather. | Ditoo profile | S041, S042 |

## Ditoo settings and deeper controls

| # | App feature | Rust gap | Applicability | Evidence |
| --- | --- | --- | --- | --- |
| 21 | Firmware version / device information | Missing in the Rust CLI; available in the separate investigation helper. | Shared protocol; firmware version verified on this device | S043 |
| 22 | Firmware availability and image download | Missing as a CLI command. The downloaded image and service responses are now in firmware/. | Shared update infrastructure; family 306 lookup verified | S044 |
| 23 | Firmware installation and progress | Missing: update negotiation, transfer, progress and completion handling. | App update infrastructure; flashing not tested on this device | S045 |
| 24 | Current mode/brightness/state readback | Missing: CLI has setters and volume/clock getters, but no full status command. | Device state response verified separately | S046 |
| 25 | Clock preview and catalogue | Partial: clock ID get/set exists; app offers graphical face selection. | Ditoo clock profile | S047 |
| 26 | 12/24-hour clock setting | Missing. set-datetime only sets date and time. | Visible for TiViUiArch used by Ditoo | S048 |
| 27 | Custom display slots and content management | Missing: slot selection, saved/custom content lists and content editing/deletion workflow. Single file upload is not slot management. | Shared pixel-device UI; exact slot limits not tested | S049 |
| 28 | Custom animation loop interval and preferred clock interval | Missing: configuration controls in LightConfigFragment. | Shared display UI; per-firmware applicability not tested | S050 |
| 29 | Custom visualizer/equalizer drawing | Partial: music visualizer preset selection exists; no custom visualizer editor. | Shared pixel-device UI; exact applicability not tested | S051 |
| 30 | Hot/trending content synchronization | Partial: mode hot selects the channel; no server catalogue download or built-in hot-content refresh. | Shared Ditoo content path | S045, S052 |
| 31 | Power-on image / startup artwork | Missing: dedicated startup-artwork selection and transfer. | Shared settings; device visibility conditional | S053 |
| 32 | Power-on/off sound and saved startup volume | Missing: startup sound toggle/volume preferences. | Ditoo profile enables startup-volume feature | S048 |
| 33 | Automatic Bluetooth audio reconnection preference | Missing: app preference controls this separately from controller transport retries. | Shown for supported music devices in NewMode | S048 |
| 34 | Notification sound preferences | Missing. | Shown for supported music devices in NewMode | S048 |
| 35 | Automatic shutdown / idle timeout | Missing. | Shared settings; exact device visibility conditional | S048 |
| 36 | TF-card mode, track list, selection and playback position | Missing: TF-card music browser and status. Basic play/pause is not a card library. | Ditoo profile enables SD support | S054, S055 |
| 37 | Music next/previous and repeat/shuffle modes | Missing typed CLI commands and player controls. | App music center | S056, S057 |
| 38 | Local audio-library playback | Missing: file/library player and playlists. Rust video is a separate mpv/video-display feature. | Phone-side audio source | S058 |
| 39 | Internet radio browse/search/favorites/history | Missing: ShoutCast station directory and playback. | Phone-side streaming; not a Ditoo Wi-Fi radio | S059, S060 |
| 40 | Alarm slots and repeat weekdays | Missing: list/read, select slot, time, day mask and enable/disable editing. | Ditoo alarm profile | S021 |
| 41 | Alarm trigger, sound, volume and custom artwork | Missing: configurable alarm content rather than hard-coded bytes. | Options vary by device/trigger | S021 |
| 42 | Game enter/exit and virtual key events | Missing: game commands and gamepad interface. | Ditoo GameTypeV5 / multi-key profile | S061, S032 |
| 43 | Persistent per-device app settings and connection UI | Partial: scan/list and explicit address exist; no saved device-management UI or per-device app database. | Shared app feature | S062 |

## Individual games selected for Ditoo GameTypeV5

| # | App feature | Rust gap | Applicability | Evidence |
| --- | --- | --- | --- | --- |
| 44 | Pixel Slot | Missing: launch/select and app virtual controls. | Ditoo game menu; not played during this audit | S032 |
| 45 | Pixel Dice | Missing: launch/select and app virtual controls. | Ditoo game menu; not played during this audit | S032 |
| 46 | Magic 8 Box | Missing: launch/select and app virtual controls. | Ditoo game menu; not played during this audit | S032 |
| 47 | Astro Battle | Missing: launch/select and app virtual controls. | Ditoo game menu; not played during this audit | S032 |
| 48 | Flappy Wings | Missing: launch/select and app virtual controls. | Ditoo game menu; not played during this audit | S032 |
| 49 | Pixel Snake | Missing: launch/select and app virtual controls. | Ditoo game menu; not played during this audit | S032 |
| 50 | Pixel Race | Missing: launch/select and app virtual controls. | Ditoo game menu; not played during this audit | S032 |
| 51 | Block Eliminator | Missing: launch/select and app virtual controls. | Ditoo game menu; not played during this audit | S032 |
| 52 | PixelFrog | Missing: launch/select and app virtual controls. | Ditoo game menu; not played during this audit | S032 |
| 53 | Gomoku | Missing: launch/select and app virtual controls. | Ditoo game menu; not played during this audit | S032 |
| 54 | Pixel Match | Missing: launch/select and app virtual controls. | Ditoo game menu; not played during this audit | S032 |
| 55 | Pixel Crush | Missing: launch/select and app virtual controls. | Ditoo game menu; not played during this audit | S032 |
| 56 | Pixel Fighter | Missing: launch/select and app virtual controls. | Ditoo game menu; not played during this audit | S032 |
| 57 | Pixel Bee | Missing: launch/select and app virtual controls. | Ditoo game menu; not played during this audit | S032 |
| 58 | Pixel Battle | Missing: launch/select and app virtual controls. | Ditoo game menu; not played during this audit | S032 |

## Shared Android creation, community and account features

| # | App feature | Rust gap | Applicability | Evidence |
| --- | --- | --- | --- | --- |
| 59 | Drawing tools and shapes | Missing: interactive pixel painting/selection/shapes. | Shared phone/cloud feature; not device firmware functionality | S063, S064 |
| 60 | Layers, opacity, color/HSL and gradients | Missing: editor layer controls, alpha/color adjustment and gradient tools. | Shared phone/cloud feature; not device firmware functionality | S065, S066, S067, S068 |
| 61 | Move, rotate, zoom and dithering | Missing interactive editing tools. | Shared phone/cloud feature; not device firmware functionality | S069, S070, S071, S072 |
| 62 | Drawing history / undo workflow | Missing editor history. | Shared phone/cloud feature; not device firmware functionality | S073 |
| 63 | Frame ordering, frame editing and timing | Missing interactive animation editing. | Shared phone/cloud feature; not device firmware functionality | S074, S075 |
| 64 | Live drawing synchronization | Missing: streaming edits while drawing in the app. | Shared phone/cloud feature; not device firmware functionality | S076 |
| 65 | Local and bundled artwork library | Missing library browsing, organization and previews. File-based image/animation commands already exist. | Shared phone/cloud feature; not device firmware functionality | S077, S078 |
| 66 | Cloud artwork discovery, filtering and browsing | Missing Divoom gallery client. | Shared phone/cloud feature; not device firmware functionality | S079, S080 |
| 67 | Publish/upload artwork and manage personal cloud works | Missing cloud publishing and personal-work workflows. | Shared phone/cloud feature; not device firmware functionality | S081, S082, S083, S084 |
| 68 | Artwork comments and moderation/reporting | Missing social/comment and report UI. | Shared phone/cloud feature; not device firmware functionality | S085, S086 |
| 69 | Creator profiles and following/follower relationships | Missing. | Shared phone/cloud feature; not device firmware functionality | S087, S088 |
| 70 | Community collections/playlists | Missing cloud playlist selection and management. | Shared phone/cloud feature; not device firmware functionality | S089, S090, S091, S092 |
| 71 | Community levels, rewards and medals | Missing. | Shared phone/cloud feature; not device firmware functionality | S093, S094 |
| 72 | Artwork ownership/verification workflows | Missing creator/work verification and plagiarism-report screens. | Shared phone/cloud feature; not device firmware functionality | S095, S096 |
| 73 | Private/group messages and inbox | Missing messenger, conversation groups and message inbox. | Shared phone/cloud feature; not device firmware functionality | S097, S098, S099, S100, S101, S102 |
| 74 | Registration, login and password recovery | Missing Divoom account client. | Shared phone/cloud feature; not device firmware functionality | S103, S104, S105, S106, S107, S108, S109, S110, S111, S112, S113, S114, S115 |
| 75 | Account password changes, unlinking and deletion | Missing account-management flows. | Shared phone/cloud feature; not device firmware functionality | S116, S117, S118 |
| 76 | Personal data / artwork export | Missing app export workflows. | Shared phone/cloud feature; not device firmware functionality | S119, S120 |
| 77 | Child/kids mode and onboarding | Missing app-level restricted mode and onboarding flow. | Shared phone/cloud feature; not device firmware functionality | S121, S122 |
| 78 | App language and app preferences | Missing app-level settings. Rust language controls the device, not the phone app. | Shared phone/cloud feature; not device firmware functionality | S123 |
| 79 | App update checks, announcements, help/legal/about | Missing app-shell functions. | Shared phone/cloud feature; not device firmware functionality | S124, S125, S126 |
| 80 | Store, shopping and merchandise integrations | Missing app shopping interfaces. | Shared phone/cloud feature; not device firmware functionality | S127, S128, S129, S130, S131, S132, S133, S134, S135, S136, S137, S138, S139, S140 |
| 81 | AI pixel-art generation | Missing AI-art service/client. | App feature; not selected in the Ditoo Pro discover menu | S141 |
| 82 | QR-code generation/display UI | Missing QR editor/display workflow. | App feature; applicability varies | S142 |
| 83 | Multi-device / multiscreen composition | Missing multi-device configuration and layout workflows. | Device compatibility conditional | S143 |
| 84 | Color palettes and color-picker UI | Missing reusable palette selection and phone color-picker interfaces. | Shared phone/cloud UI | S144, S145, S146 |
| 85 | Custom discover dashboard and feature shortcuts | Missing app home/discover customization. | Shared phone/cloud UI | S147, S148 |
| 86 | Giveaways / lucky draws, rules, history and delivery details | Missing app promotional reward workflows. | Shared phone/cloud UI | S149, S150, S151, S152, S153 |
| 87 | App rating prompts and after-sales/help interface | Missing app-shell rating/support screens. | Shared phone/cloud UI | S154, S155 |
| 88 | Child-mode settings | Missing dedicated restricted-mode configuration UI. | Shared phone/cloud UI | S156 |
| 89 | Notification/system inbox and conversations | Missing message notification, system message and conversation screens. | Shared phone/cloud UI | S157, S158, S159, S160, S161 |

## Other-model or conditionally available app features: not established Ditoo gaps

| # | App feature | Rust gap | Applicability | Evidence |
| --- | --- | --- | --- | --- |
| 90 | Wi-Fi provisioning, binding, sharing and remote control | Missing in Rust; the connected Ditoo has not been shown to support Wi-Fi. | Other-model / capability-gated; not a verified Ditoo feature | S162, S163, S164, S165, S166 |
| 91 | Wi-Fi clock/app store, widgets, templates and clock editor | Missing broader display ecosystem. Numeric Ditoo clock selection already exists. | Other-model / capability-gated; not a verified Ditoo feature | S167, S168, S169, S170, S171, S172, S173, S174, S175, S176, S177, S178, S179, S180, S181, S182 |
| 92 | Scheduled cloud playlists and clock playback plans | Missing Wi-Fi/cloud scheduling features. | Other-model / capability-gated; not a verified Ditoo feature | S090, S091, S092 |
| 93 | Photo/video frame galleries, albums and photo editing | Missing photo-frame app workflow, separate from Rust local video playback. | Other-model / capability-gated; not a verified Ditoo feature | S183, S184, S185, S186, S187, S188, S189, S190, S191, S192, S193, S194, S195, S196, S197, S198, S199, S200 |
| 94 | AI photo generation and Vision Lab | Missing model-specific image services/editors. | Other-model / capability-gated; not a verified Ditoo feature | S201, S202, S203, S204, S205 |
| 95 | Google Calendar integration | Missing. | Other-model / capability-gated; not a verified Ditoo feature | S206 |
| 96 | Outlook Calendar integration | Missing. | Other-model / capability-gated; not a verified Ditoo feature | S207 |
| 97 | Pomodoro timer | Missing; its tile is not selected in DitooProConsole. | Other-model / capability-gated; not a verified Ditoo feature | S208, S209, S210, S211, S212, S213, S214, S215 |
| 98 | Dedicated white-noise library | Missing dedicated white-noise feature; distinct from the Ditoo sleep tile. | Other-model / capability-gated; not a verified Ditoo feature | S216 |
| 99 | Advanced sleep-aid interface | Missing dedicated aidSleep UI, separate from Ditoo sleep settings. | Other-model / capability-gated; not a verified Ditoo feature | S217, S218, S219 |
| 100 | FM tuner | Missing FM controls; DitooProConsole includes music/radio streaming, not HomeFmRadio. | Other-model / capability-gated; not a verified Ditoo feature | S220, S221 |
| 101 | Lyrics, karaoke and microphone controls | Missing dedicated lyric/sound-control workflows; hardware-specific. | Other-model / capability-gated; not a verified Ditoo feature | S222, S223 |
| 102 | Danmaku / live scrolling comments | Missing dedicated connected-comment display workflow. | Other-model / capability-gated; not a verified Ditoo feature | S224, S225, S226, S227 |
| 103 | ESport device features | Missing dedicated ESport UI. | Other-model / capability-gated; not a verified Ditoo feature | S228, S229 |
| 104 | White balance, color temperature and pixel style | Missing display-calibration/configuration features for supported screens. | Other-model / capability-gated; not a verified Ditoo feature | S230, S231, S232 |
| 105 | Display rotation and mirroring | Missing typed settings; shared app hides/shows by device capability. | Other-model / capability-gated; not a verified Ditoo feature | S048 |
| 106 | Temperature unit selection | Missing; shared app gates it by capability. Ditoo support not inferred from a generic temperature display mode. | Other-model / capability-gated; not a verified Ditoo feature | S048 |
| 107 | Car mode, energy-saving and eye-protection modes | Missing; these settings are capability-gated and are not all enabled for Ditoo. | Other-model / capability-gated; not a verified Ditoo feature | S048 |
| 108 | Song-title display preference | Missing; shared settings explicitly hide it when f11235c is true, as in Ditoo Pro. | Other-model / capability-gated; not a verified Ditoo feature | S048 |
| 109 | Factory reset | Missing; app reset control is capability-gated. No reset was sent. | Other-model / capability-gated; not a verified Ditoo feature | S048 |
| 110 | Device name/password, screen lock and time zone | Missing model-specific management settings. | Other-model / capability-gated; not a verified Ditoo feature | S162, S233, S234 |
| 111 | Developer/test screens and pixel diagnostics | Missing internal app test tooling; presence in APK does not imply normal user access. | Other-model / capability-gated; not a verified Ditoo feature | S235, S236 |
| 112 | Wi-Fi alarms, memorial dates and voice messages | Missing model-specific cloud-connected variants of these features. | Other-model / capability-gated; not a verified Ditoo feature | S237, S238, S239, S240, S241, S242, S243 |
| 113 | Wi-Fi channel fonts, layouts, notifications and display regions | Missing detailed channel/widget configuration. | Other-model / capability-gated; not a verified Ditoo feature | S244, S245, S246, S247, S248, S249, S250, S251, S252, S253, S254, S255, S256, S257, S258, S259, S260, S261, S262, S263, S264, S265, S266, S267, S268, S269, S270, S271, S272, S273, S274, S275, S276, S277 |
| 114 | Other Bluetooth screen picture management | Missing bluePixelPicture UI for applicable models. | Other-model / capability-gated; not a verified Ditoo feature | S278 |
| 115 | Planet-series lighting designer | Missing Planet-specific design interface. | Other-model / capability-gated; not a verified Ditoo feature | S279 |
| 116 | Accessory / parts configuration | Missing supported accessory settings editor. | Other-model / capability-gated; not a verified Ditoo feature | S280 |
| 117 | Delayed shutdown | Missing separate delayed-shutdown discover tool. | Other-model / capability-gated; not a verified Ditoo feature | S281 |
| 118 | Other-model game interfaces, including mini car UI | Missing games not selected in the inspected Ditoo GameTypeV5 menu. | Other-model / capability-gated; not a verified Ditoo feature | S282, S283 |

## Keyboard remapping is not an established app feature

The app has on-screen game controls and stock keyboard-light settings. This
audit found no generic UI for binding arbitrary physical Ditoo keys to host
commands or installing arbitrary user firmware. Those should not be advertised
as app features the Rust tool merely needs to copy. USB key capture remains a
separate host-side opportunity.

## Coverage and evidence

The detailed tables contain **118 entries**, including overlaps, individual games and conditional features; this is not 118 independent missing Ditoo capabilities.

The inventory covers **1178 Java files** across **73 fragment families**, **510 HTTP string constants**, and all **20** Ditoo discover tiles.

APK source paths below are relative to `com/divoom/Divoom/`. The inspected local extraction is `/home/claude/code/divoom-lab/artifacts/app-simple/sources/`. Hashes in the JSON identify the exact files.

| # | Source ID | APK source path |
| --- | --- | --- |
| 1 | S001 | `view/fragment/designNew/DesignFragment.java` |
| 2 | S002 | `view/fragment/animation/AnimationEditFragment.java` |
| 3 | S003 | `view/fragment/ledMatrix/LedBaseFragment.java` |
| 4 | S004 | `view/fragment/ledMatrix/LedColorFragment.java` |
| 5 | S005 | `view/fragment/ledMatrix/LedEffectFragment.java` |
| 6 | S006 | `view/fragment/ledMatrix/LedGeneratorFragment.java` |
| 7 | S007 | `view/fragment/ledMatrix/LedMainFragment.java` |
| 8 | S008 | `view/fragment/ledMatrix/LedMixerFragment.java` |
| 9 | S009 | `view/fragment/ledMatrix/LedTextFragment.java` |
| 10 | S010 | `view/fragment/fillGame/FillGameFinishFragment.java` |
| 11 | S011 | `view/fragment/fillGame/FillGameGuideFragment.java` |
| 12 | S012 | `view/fragment/fillGame/FillGameStartFragment.java` |
| 13 | S013 | `view/fragment/fillGame/FillGameTaskDataFragment.java` |
| 14 | S014 | `view/fragment/fillGame/FillGameTaskFragment.java` |
| 15 | S015 | `view/fragment/fillGameDesign/FillGameDesignFragment.java` |
| 16 | S016 | `view/fragment/music/MusicMainFragment.java` |
| 17 | S017 | `view/fragment/mix/MixerMainFragment.java` |
| 18 | S018 | `view/fragment/mix/MixerRecordFragment.java` |
| 19 | S019 | `view/fragment/voice/VoiceFragment.java` |
| 20 | S020 | `view/fragment/chat/ChatEnterFragment.java` |
| 21 | S021 | `view/fragment/alarm/model/AlarmViewModel.java` |
| 22 | S022 | `view/fragment/sleep/normal/SleepMainFragment.java` |
| 23 | S023 | `view/fragment/sleep/pixoo/SleepPixooMainFragment.java` |
| 24 | S024 | `view/fragment/sleep/wifi/WifiSleepMainFragment.java` |
| 25 | S025 | `view/fragment/planner/model/BluePlannerModel.java` |
| 26 | S026 | `view/fragment/planner/PlannerAddFragment.java` |
| 27 | S027 | `view/fragment/planner/PlannerEditFragment.java` |
| 28 | S028 | `view/fragment/planner/PlannerMainFragment.java` |
| 29 | S029 | `view/fragment/planner/wifi/WIfiPlannerMainFragment.java` |
| 30 | S030 | `view/fragment/planner/wifi/WifiPlannerAddFragment.java` |
| 31 | S031 | `view/fragment/planner/wifi/WifiPlannerEditFragment.java` |
| 32 | S032 | `view/fragment/game/horizontal/GameHorizontalMainFragment.java` |
| 33 | S033 | `view/fragment/tool/StopwatchFragment.java` |
| 34 | S034 | `view/fragment/memorialday/MemorialDayFragment.java` |
| 35 | S035 | `view/fragment/memorialday/MemorialNewFragment.java` |
| 36 | S036 | `view/fragment/power/PowerEditFragment.java` |
| 37 | S037 | `view/fragment/tool/CountDownFragment.java` |
| 38 | S038 | `view/fragment/tool/NoiseFragment.java` |
| 39 | S039 | `view/fragment/tool/ScoreboardFragment.java` |
| 40 | S040 | `view/fragment/notification/NotificationsMainFragment.java` |
| 41 | S041 | `view/fragment/weather/WeatherFragment.java` |
| 42 | S042 | `view/fragment/weather/WeatherSettingFragment.java` |
| 43 | S043 | `bluetooth/u.java` |
| 44 | S044 | `http/request/update/GetUpdateFileRequest.java` |
| 45 | S045 | `bluetooth/update/HotUpdateHandle.java` |
| 46 | S046 | `bluetooth/l.java` |
| 47 | S047 | `view/fragment/light/common/LightKaraokeClockFragment.java` |
| 48 | S048 | `view/fragment/more/lightSetting/LightSettingsFragment.java` |
| 49 | S049 | `view/fragment/light/common/LightCustomFragment.java` |
| 50 | S050 | `view/fragment/light/common/LightConfigFragment.java` |
| 51 | S051 | `view/fragment/light/common/LightPixEqualizerMakeFragment.java` |
| 52 | S052 | `view/fragment/light/common/LightHotFragment.java` |
| 53 | S053 | `view/fragment/more/lightSetting/LightSettingServer.java` |
| 54 | S054 | `view/fragment/music/sd/SdMainFragment.java` |
| 55 | S055 | `view/fragment/music/sd/SdInfo.java` |
| 56 | S056 | `view/fragment/music/model/MusicControl.java` |
| 57 | S057 | `view/fragment/music/player/PlayMode.java` |
| 58 | S058 | `view/fragment/music/local/MusicLocalFragment.java` |
| 59 | S059 | `view/fragment/music/radio/ShoutCastMainFragment.java` |
| 60 | S060 | `view/fragment/music/radio/ShoutCastSearchFragment.java` |
| 61 | S061 | `view/fragment/game/model/GameModel.java` |
| 62 | S062 | `view/fragment/more/device/MoreBluetoothFragment.java` |
| 63 | S063 | `view/fragment/designNew/plugView/CanvasView.java` |
| 64 | S064 | `view/fragment/designNew/plugView/SelectView.java` |
| 65 | S065 | `view/fragment/designNew/view/DesignLayerButtonView.java` |
| 66 | S066 | `view/fragment/designNew/view/DesignAlphaView.java` |
| 67 | S067 | `view/fragment/designNew/view/DesignHSLView.java` |
| 68 | S068 | `view/fragment/designNew/view/DesignGradientView.java` |
| 69 | S069 | `view/fragment/designNew/view/DesignMoveView.java` |
| 70 | S070 | `view/fragment/designNew/view/DesignTurnView.java` |
| 71 | S071 | `view/fragment/designNew/view/DesignZoomLayout.java` |
| 72 | S072 | `view/fragment/designNew/view/DesignDitheringView.java` |
| 73 | S073 | `view/fragment/designNew/model/DesignHistoryModel.java` |
| 74 | S074 | `view/fragment/designNew/view/DesignFrameView.java` |
| 75 | S075 | `view/fragment/designNew/SpeedDialog.java` |
| 76 | S076 | `view/fragment/designNew/model/DesignSyncSendModel.java` |
| 77 | S077 | `view/fragment/gallery/local/LocalImageFragment.java` |
| 78 | S078 | `view/fragment/gallery/system/SystemAniFragment.java` |
| 79 | S079 | `view/fragment/cloudV2/CloudFilterDialogFragment.java` |
| 80 | S080 | `view/fragment/gallery/cloud/OldCloudCategoryFragment.java` |
| 81 | S081 | `view/fragment/publish/PublishCloseFragment.java` |
| 82 | S082 | `view/fragment/publish/PublishFragment.java` |
| 83 | S083 | `view/fragment/publish/PublishRuleFragment.java` |
| 84 | S084 | `view/fragment/more/personal/PersonalCollectFragment.java` |
| 85 | S085 | `view/fragment/cloudV2/verify/CloudVerifyCommentFragment.java` |
| 86 | S086 | `view/fragment/cloudV2/userDetails/CloudUserReportFragment.java` |
| 87 | S087 | `view/fragment/cloudV2/userDetails/CloudUserDetailsFragment.java` |
| 88 | S088 | `view/fragment/cloudV2/relation/CloudRelationshipFragment.java` |
| 89 | S089 | `view/fragment/cloudV2/userDetails/CloudUserDetailsPlayListFragment.java` |
| 90 | S090 | `view/fragment/playList/CloudPlayListFragment.java` |
| 91 | S091 | `view/fragment/playList/PlayListAddImageFragment.java` |
| 92 | S092 | `view/fragment/playList/PlayListInfoFragment.java` |
| 93 | S093 | `view/fragment/cloudV2/grade/CloudLevelFragment.java` |
| 94 | S094 | `view/fragment/cloudV2/medal/CloudMedalFragment.java` |
| 95 | S095 | `view/fragment/cloudV2/verify/CloudVerifyWorksFragment.java` |
| 96 | S096 | `view/fragment/cloudV2/verify/CloudVerifyStealFragment.java` |
| 97 | S097 | `view/fragment/messageGroup/MessageGroupConFragment.java` |
| 98 | S098 | `view/fragment/messageGroup/MessageGroupListFragment.java` |
| 99 | S099 | `view/fragment/messageGroup/MessageGroupSetFragment.java` |
| 100 | S100 | `view/fragment/messageTop/MessageCommentFragment.java` |
| 101 | S101 | `view/fragment/messageTop/MessageFansFragment.java` |
| 102 | S102 | `view/fragment/messageTop/MessageLikeFragment.java` |
| 103 | S103 | `view/fragment/Register/PhoneRegister/BindEmailFragment.java` |
| 104 | S104 | `view/fragment/Register/PhoneRegister/BindPhoneFragment.java` |
| 105 | S105 | `view/fragment/Register/PhoneRegister/RegisterChooseFragment.java` |
| 106 | S106 | `view/fragment/Register/PhoneRegister/RegisterEmailFragment.java` |
| 107 | S107 | `view/fragment/Register/PhoneRegister/RegisterPhoneFragment.java` |
| 108 | S108 | `view/fragment/Login/BirthdayDialogFragment.java` |
| 109 | S109 | `view/fragment/Login/LoginExplainFragment.java` |
| 110 | S110 | `view/fragment/Login/LoginFragment.java` |
| 111 | S111 | `view/fragment/Login/LoginRuleFragment.java` |
| 112 | S112 | `view/fragment/Login/TwitterWebFragment.java` |
| 113 | S113 | `view/fragment/Login/UserAgreementFragment.java` |
| 114 | S114 | `view/fragment/Forget/ForgetFragment.java` |
| 115 | S115 | `view/fragment/Forget/ForgetNewPassFragment.java` |
| 116 | S116 | `view/fragment/more/Account/PasswordFragment.java` |
| 117 | S117 | `view/fragment/more/Account/AccountUnbindingFragment.java` |
| 118 | S118 | `view/fragment/more/Account/DeleteUserFragment.java` |
| 119 | S119 | `view/fragment/more/personal/PersonalExportFragment.java` |
| 120 | S120 | `view/fragment/more/personal/PersonalExportEmailFragment.java` |
| 121 | S121 | `view/fragment/eventChain/loginChain/KidsDialogChain.java` |
| 122 | S122 | `view/fragment/eventChain/loginChain/ShowGuideChain.java` |
| 123 | S123 | `view/fragment/more/language/LanguageFragment.java` |
| 124 | S124 | `view/fragment/eventChain/loginChain/AppUpdateChain.java` |
| 125 | S125 | `view/fragment/eventChain/loginChain/AnnounceChain.java` |
| 126 | S126 | `view/fragment/more/about/AboutFragment.java` |
| 127 | S127 | `view/fragment/shop/ShopFragment.java` |
| 128 | S128 | `view/fragment/shopify/ShopifyAccountFragment.java` |
| 129 | S129 | `view/fragment/shopify/ShopifyAddressEditFragment.java` |
| 130 | S130 | `view/fragment/shopify/ShopifyAddressesFragment.java` |
| 131 | S131 | `view/fragment/shopify/ShopifyCartFragment.java` |
| 132 | S132 | `view/fragment/shopify/ShopifyCollectionsFragment.java` |
| 133 | S133 | `view/fragment/shopify/ShopifyOrdersFragment.java` |
| 134 | S134 | `view/fragment/shopify/ShopifyProductDetailFragment.java` |
| 135 | S135 | `view/fragment/shopify/ShopifySearchFragment.java` |
| 136 | S136 | `view/fragment/shopify/ShopifyStoreFragment.java` |
| 137 | S137 | `view/fragment/mall/MallAddressFragment.java` |
| 138 | S138 | `view/fragment/mall/MallBuyFragment.java` |
| 139 | S139 | `view/fragment/mall/MallDialogFragment.java` |
| 140 | S140 | `view/fragment/mall/MallListFragment.java` |
| 141 | S141 | `view/fragment/aiPixel/AiPixelFragment.java` |
| 142 | S142 | `view/fragment/qrcode/QRCodeFragment.java` |
| 143 | S143 | `view/fragment/multiscreen/MultiScreenFragment.java` |
| 144 | S144 | `view/fragment/colorPicker/ColorPalettesFragment.java` |
| 145 | S145 | `view/fragment/colorPicker/ColorPickerHSVFragment.java` |
| 146 | S146 | `view/fragment/miniColorPicker/ColorPicketFragment.java` |
| 147 | S147 | `view/fragment/discover/DiscoverAddFragment.java` |
| 148 | S148 | `view/fragment/home/MoreFragment.java` |
| 149 | S149 | `view/fragment/luck/LuckAddressFragment.java` |
| 150 | S150 | `view/fragment/luck/LuckDialogFragment.java` |
| 151 | S151 | `view/fragment/luck/LuckFragment.java` |
| 152 | S152 | `view/fragment/luck/LuckRecordFragment.java` |
| 153 | S153 | `view/fragment/luck/LuckRuleFragment.java` |
| 154 | S154 | `view/fragment/rating/RatingDialogFragment.java` |
| 155 | S155 | `view/fragment/discover/DiscoverAftermarketFragment.java` |
| 156 | S156 | `view/fragment/parent/KidsFragment.java` |
| 157 | S157 | `view/fragment/message/MessageConversationFragment.java` |
| 158 | S158 | `view/fragment/message/MessageListFragment.java` |
| 159 | S159 | `view/fragment/message/MessageMainFragment.java` |
| 160 | S160 | `view/fragment/message/MessageNotifyFragment.java` |
| 161 | S161 | `view/fragment/message/MessageSystemFragment.java` |
| 162 | S162 | `view/fragment/more/device/WifiDeviceSetFragment.java` |
| 163 | S163 | `view/fragment/wifi/Wifi24GFragment.java` |
| 164 | S164 | `view/fragment/wifi/WifiConfigAstroBindFragment.java` |
| 165 | S165 | `view/fragment/wifi/WifiConnectSuccessFragment.java` |
| 166 | S166 | `view/fragment/wifi/WifiGiftFragment.java` |
| 167 | S167 | `view/fragment/myClock/MyClockAddFragment.java` |
| 168 | S168 | `view/fragment/myClock/MyClockMainFragment.java` |
| 169 | S169 | `view/fragment/myClock/MyClockPlayPlanFragment.java` |
| 170 | S170 | `view/fragment/myClock/MyClockStoreClassifyFragment.java` |
| 171 | S171 | `view/fragment/myClock/MyClockStoreDiscoverFragment.java` |
| 172 | S172 | `view/fragment/myClock/MyClockStoreEditorMcpFragment.java` |
| 173 | S173 | `view/fragment/myClock/MyClockStoreFragment.java` |
| 174 | S174 | `view/fragment/myClock/MyClockStoreNewestFragment.java` |
| 175 | S175 | `view/fragment/myClock/MyClockStoreRecommendFragment.java` |
| 176 | S176 | `view/fragment/myClock/MyClockStoreTabFragment.java` |
| 177 | S177 | `view/fragment/myClock/MyClockStoreWorkshopFragment.java` |
| 178 | S178 | `view/fragment/myClock/MyClockTabMainFragment.java` |
| 179 | S179 | `view/fragment/myClock/playplan/MyClockPlayPlanCycleListFragment.java` |
| 180 | S180 | `view/fragment/myClock/playplan/MyClockPlayPlanDateListFragment.java` |
| 181 | S181 | `view/fragment/myClock/playplan/MyClockPlayPlanEditFragment.java` |
| 182 | S182 | `view/fragment/clockEdit/ClockEditFragment.java` |
| 183 | S183 | `view/fragment/photoWifi/CloudPhotoListFragment.java` |
| 184 | S184 | `view/fragment/photoWifi/WIfiPhotoListFragment.java` |
| 185 | S185 | `view/fragment/photoWifi/WIfiPhotoSelectLogoFragment.java` |
| 186 | S186 | `view/fragment/photoWifi/WifiAstroTooRGBFragment.java` |
| 187 | S187 | `view/fragment/photoWifi/WifiPhotoAlbumAddFragment.java` |
| 188 | S188 | `view/fragment/photoWifi/WifiPhotoAlbumFragment.java` |
| 189 | S189 | `view/fragment/photoWifi/WifiPhotoAlbumRGBFragment.java` |
| 190 | S190 | `view/fragment/photoWifi/WifiPhotoImageAdjFragment.java` |
| 191 | S191 | `view/fragment/photoWifi/WifiPhotoImageRectAdjFragment.java` |
| 192 | S192 | `view/fragment/photoWifi/WifiPhotoImageTitleFragment.java` |
| 193 | S193 | `view/fragment/photoWifi/WifiPhotoPartFragment.java` |
| 194 | S194 | `view/fragment/photoWifi/WifiPhotoSettingFragment.java` |
| 195 | S195 | `view/fragment/photoWifi/WifiPhotoSlideThemeFragment.java` |
| 196 | S196 | `view/fragment/photoWifi/WifiPhotoVideoAdjFragment.java` |
| 197 | S197 | `view/fragment/photoWifi/WifiPhotoVideoTitleFragment.java` |
| 198 | S198 | `view/fragment/bluePhoto/BluePhotoFragment.java` |
| 199 | S199 | `view/fragment/bluePhoto/BluePhotoImageFragment.java` |
| 200 | S200 | `view/fragment/bluePhoto/BluePhotoVideoFragment.java` |
| 201 | S201 | `view/fragment/aiPhoto/AiModelListFragment.java` |
| 202 | S202 | `view/fragment/aiPhoto/AiPhotoFragment.java` |
| 203 | S203 | `view/fragment/visionLab/WifiPhotoVisionLabFragment.java` |
| 204 | S204 | `view/fragment/visionLab/WifiPhotoVisionLabStoreClassifyFragment.java` |
| 205 | S205 | `view/fragment/visionLab/WifiPhotoVisionLabStoreFragment.java` |
| 206 | S206 | `view/fragment/googleCalendars/GoogleCalendarsFragment.java` |
| 207 | S207 | `view/fragment/outlookCalendars/OutlookCalendarsFragment.java` |
| 208 | S208 | `view/fragment/tomato/TomatoAddFragment.java` |
| 209 | S209 | `view/fragment/tomato/TomatoAlarmSoundFragment.java` |
| 210 | S210 | `view/fragment/tomato/TomatoFocusEditFragment.java` |
| 211 | S211 | `view/fragment/tomato/TomatoFocusMainFragment.java` |
| 212 | S212 | `view/fragment/tomato/TomatoHistoryFragment.java` |
| 213 | S213 | `view/fragment/tomato/TomatoMainFragment.java` |
| 214 | S214 | `view/fragment/tomato/TomatoRepeatTypeFragment.java` |
| 215 | S215 | `view/fragment/tomato/TomatoSoundFragment.java` |
| 216 | S216 | `view/fragment/whiteNoise/WhiteNoiseMainFragment.java` |
| 217 | S217 | `view/fragment/aidSleep/AidSleepListFragment.java` |
| 218 | S218 | `view/fragment/aidSleep/AidSleepMainFragment.java` |
| 219 | S219 | `view/fragment/aidSleep/AidSleepManageMainFragment.java` |
| 220 | S220 | `view/fragment/fm/FmChannelFragment.java` |
| 221 | S221 | `view/fragment/fm/FmFragment.java` |
| 222 | S222 | `view/fragment/lyric/LyricMainFragment.java` |
| 223 | S223 | `view/fragment/soundControl/SoundControlFragment.java` |
| 224 | S224 | `view/fragment/danmaku/BlueDanmakuMainFragment.java` |
| 225 | S225 | `view/fragment/danmaku/DanmakuConfigFragment.java` |
| 226 | S226 | `view/fragment/danmaku/DanmakuFaceFragment.java` |
| 227 | S227 | `view/fragment/danmaku/WifiDanmakuMainFragment.java` |
| 228 | S228 | `view/fragment/eSport/ESportConfigFragment.java` |
| 229 | S229 | `view/fragment/eSport/ESportEqualizerFragment.java` |
| 230 | S230 | `view/fragment/more/lightsettingWifi/WifiWhiteBalanceFragment.java` |
| 231 | S231 | `view/fragment/more/lightsettingWifi/WifiSysColorTempFragment.java` |
| 232 | S232 | `view/fragment/more/lightsettingWifi/WifiSysPixelStyleFragment.java` |
| 233 | S233 | `view/fragment/more/lightsettingWifi/WifiSysLockScreenFragment.java` |
| 234 | S234 | `view/fragment/more/lightsettingWifi/WifiSysTimeZoneSearchFragment.java` |
| 235 | S235 | `view/fragment/more/test/AboutTestFragment.java` |
| 236 | S236 | `view/fragment/more/test/PixelTestFragment.java` |
| 237 | S237 | `view/fragment/alarmWifi/WifiAlarmEditFragment.java` |
| 238 | S238 | `view/fragment/alarmWifi/WifiAlarmMainFragment.java` |
| 239 | S239 | `view/fragment/alarmWifi/WifiAlarmSoundFragment.java` |
| 240 | S240 | `view/fragment/memorialWifi/WifiMemorialAddFragment.java` |
| 241 | S241 | `view/fragment/memorialWifi/WifiMemorialFragment.java` |
| 242 | S242 | `view/fragment/voiceWifi/VoiceMessageWifiFragment.java` |
| 243 | S243 | `view/fragment/voiceWifi/VoiceMessageWifiSettingFragment.java` |
| 244 | S244 | `view/fragment/channelWifi/BlueCustom64Fragment.java` |
| 245 | S245 | `view/fragment/channelWifi/BlueEq64Fragment.java` |
| 246 | S246 | `view/fragment/channelWifi/BlueHotFragment.java` |
| 247 | S247 | `view/fragment/channelWifi/WifiChannelAlbumListFragment.java` |
| 248 | S248 | `view/fragment/channelWifi/WifiChannelClockCommentFragment.java` |
| 249 | S249 | `view/fragment/channelWifi/WifiChannelClockDeviceAreaFragment.java` |
| 250 | S250 | `view/fragment/channelWifi/WifiChannelClockFontFragment.java` |
| 251 | S251 | `view/fragment/channelWifi/WifiChannelClockFormatFragment.java` |
| 252 | S252 | `view/fragment/channelWifi/WifiChannelClockNotifyFragment.java` |
| 253 | S253 | `view/fragment/channelWifi/WifiChannelClockSettingFragment.java` |
| 254 | S254 | `view/fragment/channelWifi/WifiChannelClockStyleFragment.java` |
| 255 | S255 | `view/fragment/channelWifi/WifiChannelClockTemperatureFragment.java` |
| 256 | S256 | `view/fragment/channelWifi/WifiChannelClockTimeZoneFragment.java` |
| 257 | S257 | `view/fragment/channelWifi/WifiChannelCustomFragment.java` |
| 258 | S258 | `view/fragment/channelWifi/WifiChannelDateElementStyleFragment.java` |
| 259 | S259 | `view/fragment/channelWifi/WifiChannelEqFragment.java` |
| 260 | S260 | `view/fragment/channelWifi/WifiChannelEqMakeFragment.java` |
| 261 | S261 | `view/fragment/channelWifi/WifiChannelFiveLcdAllFragment.java` |
| 262 | S262 | `view/fragment/channelWifi/WifiChannelFiveLcdMainFragment.java` |
| 263 | S263 | `view/fragment/channelWifi/WifiChannelFiveLcdMyClockFragment.java` |
| 264 | S264 | `view/fragment/channelWifi/WifiChannelFiveLcdRGBFragment.java` |
| 265 | S265 | `view/fragment/channelWifi/WifiChannelFiveLcdSingleFragment.java` |
| 266 | S266 | `view/fragment/channelWifi/WifiChannelFiveLcdStoreClassifyFragment.java` |
| 267 | S267 | `view/fragment/channelWifi/WifiChannelFiveLcdStoreFragment.java` |
| 268 | S268 | `view/fragment/channelWifi/WifiChannelFiveLcdTabMainFragment.java` |
| 269 | S269 | `view/fragment/channelWifi/WifiChannelItemSearchFragment.java` |
| 270 | S270 | `view/fragment/channelWifi/WifiChannelMainFragment.java` |
| 271 | S271 | `view/fragment/channelWifi/WifiChannelRoundElementStyleFragment.java` |
| 272 | S272 | `view/fragment/channelWifi/WifiChannelSearchFragment.java` |
| 273 | S273 | `view/fragment/channelWifi/WifiChannelSearchUserFragment.java` |
| 274 | S274 | `view/fragment/channelWifi/WifiChannelSelectSubClockFragment.java` |
| 275 | S275 | `view/fragment/channelWifi/WifiChannelSubscribeFragment.java` |
| 276 | S276 | `view/fragment/channelWifi/WifiClockClassifyFragment.java` |
| 277 | S277 | `view/fragment/channelWifi/WifiLcdConfigFragment.java` |
| 278 | S278 | `view/fragment/bluePixelPicture/BluePixelPictureFragment.java` |
| 279 | S279 | `view/fragment/planetDesign/PlanetDesignFragment.java` |
| 280 | S280 | `view/fragment/more/parts/PartsSettingsFragment.java` |
| 281 | S281 | `view/fragment/discover/DelayShutdownFragment.java` |
| 282 | S282 | `view/fragment/game/mini/GameCarFragment.java` |
| 283 | S283 | `view/fragment/game/vertical/GameVerticalMainFragment.java` |

All fragment families represented in the machine inventory:

`Forget`, `Login`, `Register`, `aiPhoto`, `aiPixel`, `aidSleep`, `alarm`, `alarmWifi`, `animation`, `bluePhoto`, `bluePixelPicture`, `channelWifi`, `chat`, `clockEdit`, `cloudV2`, `colorPicker`, `control`, `danmaku`, `designLocalDialog`, `designNew`, `dialog`, `discover`, `eSport`, `eventChain`, `fillGame`, `fillGameDesign`, `fm`, `gallery`, `game`, `googleCalendars`, `home`, `ledMatrix`, `light`, `luck`, `lyric`, `mainDevice`, `mall`, `medal`, `memorialWifi`, `memorialday`, `message`, `messageGroup`, `messageTop`, `miniColorPicker`, `mix`, `more`, `multiscreen`, `music`, `myClock`, `notification`, `outlookCalendars`, `parent`, `photoWifi`, `planetDesign`, `planner`, `playList`, `power`, `publish`, `qrcode`, `rating`, `shop`, `shopify`, `sleep`, `soundControl`, `tomato`, `tool`, `video`, `visionLab`, `voice`, `voiceWifi`, `weather`, `whiteNoise`, `wifi`.
