ENTRY(runtime_command)
stock_dispatch_end = 0x3b30a;
stock_version = 0x4b824;
stock_set_version = 0x4b858;
stock_reply = 0x3901c;
stock_alloc = 0x76d88;
stock_free = 0x76da8;
stock_free_heap = 0x83ea0;
stock_heap_init = 0x854c4;
stock_task = 0x7846c;
stock_delay = 0x784fc;
stock_volume = 0x770a0;
stock_event = 0x4a60c;
stock_ticks = 0x784a4;
stock_calendar = 0x775b0;
stock_screen_original = 0x75dd0;
runtime_screen = 0x75dcc;
stock_adc_scan = 0x2bfb0;
stock_screen_command = 0x75d60;
stock_led_original = 0x75810;
stock_led_write = 0x76aa0;
stock_page_read = 0x7854c;
stock_native_pump = 0x4a6dc;
stock_indicator_resume = 0x2d6ec;
stock_noise_enable = 0x72320;
stock_noise_display = 0x54b68;
stock_config_read = 0x6ab10;
stock_config_write = 0x6b97c;
stock_alarm_reschedule = 0x3575c;
stock_wake_reschedule = 0x47e70;
stock_seconds = 0x77588;
stock_alarm_cancel = 0x35cf8;
stock_alarm_snooze = 0x35de0;
stock_alarm_preview = 0x3648c;
stock_set_source = 0x77718;
stock_get_source = 0x77704;
stock_sd_present = 0x770e0;
stock_play = 0x77064;
stock_track_direction = 0x771a4;
stock_track = 0x771d4;
stock_seek = 0x77148;
stock_sd_queue = 0x1e9d8;
stock_repeat = 0x76e7c;
stock_audio_status = 0x771fc;
stock_memo_start = 0x440a4;
stock_memo_stop = 0x4436c;
stock_sound_stop = 0x77b3c;
stock_sound_playing = 0x77b50;
stock_fs_free = 0x6bfc0;
stock_fs_delete = 0x6db70;
stock_battery_level = 0x2000d2f8 - 37104;
stock_usb_power = 0x2000d2f8 - 34529;
stock_charge_pin = 0x2000d2f8 - 34530;
stock_alarm_state = 0x2000d2f8 - 34131;
stock_alarm_slot = 0x2000d2f8 - 34125;
stock_noise_context = 0x2000d2f8 - 33684;
stock_memo_context = 0x2000d2f8 - 34040;
_SDA_BASE_ = 0x2000d2f8;
SECTIONS {
    .hook 0x3ab9c : { KEEP(*(.hook)) }
    .init_hook 0x2ec58 : { KEEP(*(.init_hook)) }
    .screen_hook 0x75dcc : { KEEP(*(.screen_hook)) }
    .key_hook 0x2d490 : { KEEP(*(.key_hook)) }
    .led_hook 0x7580c : { KEEP(*(.led_hook)) }
    .native_hook 0x47838 : { KEEP(*(.native_hook)) }
    .indicator_hook 0x2d6e8 : { KEEP(*(.indicator_hook)) }
    .noise_hook 0x72192 : { KEEP(*(.noise_hook)) }
    .wake_select_hook 0x47eea : { KEEP(*(.wake_select_hook)) }
    .data 0x2004a000 : AT(0x1ee000) {
        __data_start = .;
        *libm.a:*(.rodata*) *(.data*) *(.sdata*)
        __data_end = .;
    }
    __data_load = LOADADDR(.data);
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*) *(.sbss*) *(COMMON)
        __bss_end = .;
    }
    .text 0x1ca000 : { *(.text*) *(.rodata*) }
    /DISCARD/ : { *(.comment) *(.note*) }
}
ASSERT(SIZEOF(.hook) <= 28, "command hook exceeds original handler")
ASSERT(SIZEOF(.init_hook) == 4, "init hook must replace exactly one call")
ASSERT(ADDR(.text) + SIZEOF(.text) <= 0x1ee000, "Lua overlaps its data load image")
ASSERT(__bss_end <= 0x2004c000, "Lua globals exceed reserved 8 KiB")
ASSERT(LOADADDR(.data) + SIZEOF(.data) < 0x1f0000, "Lua crosses stock user-data boundary")

ASSERT(SIZEOF(.screen_hook) == 4, "screen hook size")
ASSERT(SIZEOF(.key_hook) == 4, "key hook size")
ASSERT(SIZEOF(.led_hook) == 4, "LED hook size")

ASSERT(SIZEOF(.native_hook) == 4, "native hook size")
ASSERT(SIZEOF(.indicator_hook) == 4, "indicator hook size")
ASSERT(SIZEOF(.noise_hook) == 4, "noise hook size")

ASSERT(SIZEOF(.wake_select_hook) == 4, "wake selector hook size")
