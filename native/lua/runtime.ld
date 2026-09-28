ENTRY(runtime_command)
stock_dispatch_end = 0x3b30a;
stock_version = 0x4b824;
stock_set_version = 0x4b858;
stock_reply = 0x3901c;
stock_alloc = 0x76d88;
stock_free = 0x76da8;
stock_heap_init = 0x854c4;
stock_task = 0x7846c;
stock_delay = 0x784fc;
stock_volume = 0x770a0;
stock_event = 0x4a60c;
_SDA_BASE_ = 0x2000d2f8;
SECTIONS {
    .hook 0x3ab9c : { KEEP(*(.hook)) }
    .init_hook 0x2ec58 : { KEEP(*(.init_hook)) }
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
