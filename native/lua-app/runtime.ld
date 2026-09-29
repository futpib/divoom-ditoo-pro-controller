ENTRY(runtime_command)
stock_dispatch_end = 0x3b30a;
stock_dispatch_command = 0x3998c;
stock_response_resume = 0x38f04;
stock_control_connected = 0x39048;
stock_usb_receive = 0x2b240;
stock_usb_send = 0x8b6d6;
stock_usb_setup = 0x2000d2f8 - 35480;
stock_command_context = 0x2000d2f8 - 34060;
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
stock_config_find = 0x6aa88;
stock_config_context = 0x2000d2f8 - 33700;
stock_partition = 0x73b8c;
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
stock_bt_context = 0x2000d2f8 - 27264;
stock_bt_manager = 0x2000d2f8 + 0x5104;
stock_ble_connected = 0x2000d2f8 - 30420;
stock_avrcp_state = 0x7c954;
stock_a2dp_state = 0x7bac0;
stock_avrcp_connect = 0x1139b0;
stock_avrcp_disconnect = 0x1139c0;
stock_avrcp_play = 0x1139ce;
stock_avrcp_pause = 0x1139e8;
stock_bt_peek = 0x138bf6;
stock_bt_enqueue = 0x138b74;
stock_avrcp_panel = 0x11c52e;
stock_bt_core = 0x2000d2f8 - 35356;
stock_l2_register = 0x12288e;
stock_l2_connect = 0x122e3a;
stock_l2_accept = 0x122d44;
stock_l2_disconnect = 0x122ab6;
stock_l2_send = 0x1229b2;
stock_sdp_add = 0x12eafa;
stock_cmgr_register = 0x11dd34;
stock_cmgr_connect = 0x11ddac;
stock_cmgr_remove = 0x11df44;
stock_bt_class = 0x128914;
stock_bt_record_count = 0x7cc84;
stock_bt_save_records = 0x7d570;
stock_sec_register = 0x12854c;
stock_bt_access = 0x128032;
stock_bt_find_device = 0x1276c8;
stock_l2_security = 0x1233c8;
stock_bt_stack_resume = 0x7ec24;
stock_bt_hci_resume = 0x121c72;
_SDA_BASE_ = 0x2000d2f8;
SECTIONS {
    .hook 0x3ab9c : { KEEP(*(.hook)) }
    .init_hook 0x2ec58 : { KEEP(*(.init_hook)) }
    .screen_hook 0x75dcc : { KEEP(*(.screen_hook)) }
    .key_hook 0x2d490 : { KEEP(*(.key_hook)) }
    .led_hook 0x7580c : { KEEP(*(.led_hook)) }
    .native_hook 0x47838 : { KEEP(*(.native_hook)) }
    .usb_receive_hook 0x7a27e : { KEEP(*(.usb_receive_hook)) }
    .usb_send_hook 0x7a29c : { KEEP(*(.usb_send_hook)) }
    .usb_response_hook 0x38f00 : { KEEP(*(.usb_response_hook)) }
    .usb_connected_hook 0x387b2 : { KEEP(*(.usb_connected_hook)) }
    .bt_command_hook 0x138c76 : { KEEP(*(.bt_command_hook)) }
    .bt_stack_hook 0x7ec20 : { KEEP(*(.bt_stack_hook)) }
    .bt_hci_hook 0x121c6c : { KEEP(*(.bt_hci_hook)) }
    .indicator_hook 0x2d6e8 : { KEEP(*(.indicator_hook)) }
    .noise_hook 0x72192 : { KEEP(*(.noise_hook)) }
    .wake_select_hook 0x47eea : { KEEP(*(.wake_select_hook)) }
    .data 0x2004a000 : AT(0x1ef800) {
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
ASSERT(ADDR(.text) + SIZEOF(.text) <= 0x1ef800, "Lua overlaps its data load image")
ASSERT(__bss_end <= 0x2004c000, "Lua globals exceed reserved 8 KiB")
ASSERT(LOADADDR(.data) + SIZEOF(.data) < 0x1f0000, "Lua crosses stock user-data boundary")

ASSERT(SIZEOF(.screen_hook) == 4, "screen hook size")
ASSERT(SIZEOF(.key_hook) == 4, "key hook size")
ASSERT(SIZEOF(.led_hook) == 4, "LED hook size")

ASSERT(SIZEOF(.native_hook) == 4, "native hook size")
ASSERT(SIZEOF(.usb_receive_hook) == 4, "USB receive hook size")
ASSERT(SIZEOF(.usb_send_hook) == 4, "USB send hook size")
ASSERT(SIZEOF(.usb_response_hook) == 4, "USB response hook size")
ASSERT(SIZEOF(.usb_connected_hook) == 4, "USB notification hook size")
ASSERT(SIZEOF(.bt_command_hook) == 4, "Bluetooth command hook size")
ASSERT(SIZEOF(.bt_stack_hook) == 4, "Bluetooth stack hook size")
ASSERT(SIZEOF(.bt_hci_hook) == 6, "Bluetooth HCI hook size")
ASSERT(SIZEOF(.indicator_hook) == 4, "indicator hook size")
ASSERT(SIZEOF(.noise_hook) == 4, "noise hook size")

ASSERT(SIZEOF(.wake_select_hook) == 4, "wake selector hook size")
