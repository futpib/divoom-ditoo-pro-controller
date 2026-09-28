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
_SDA_BASE_ = 0x2000d2f8;
SECTIONS {
    .hook 0x3ab9c : { KEEP(*(.hook)) }
    .init_hook 0x2ec58 : { KEEP(*(.init_hook)) }
    .screen_hook 0x75dcc : { KEEP(*(.screen_hook)) }
    .key_hook 0x2d490 : { KEEP(*(.key_hook)) }
    .led_hook 0x7580c : { KEEP(*(.led_hook)) }
    .data 0x20048000 : AT(0x1ee000) {
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
ASSERT(__bss_end <= 0x2004c000, "Lua globals exceed reserved 16 KiB")
ASSERT(LOADADDR(.data) + SIZEOF(.data) < 0x1f0000, "Lua crosses stock user-data boundary")

ASSERT(SIZEOF(.screen_hook) == 4, "screen hook size")
ASSERT(SIZEOF(.key_hook) == 4, "key hook size")
ASSERT(SIZEOF(.led_hook) == 4, "LED hook size")
