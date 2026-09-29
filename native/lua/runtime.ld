ENTRY(runtime_command)
INCLUDE native/patches/stock-306007.ld
SECTIONS {
    INCLUDE target/lua/runtime/patch-layout.ld
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
ASSERT(ADDR(.text) + SIZEOF(.text) <= 0x1ee000, "Lua overlaps its data load image")
ASSERT(__bss_end <= 0x2004c000, "Lua globals exceed reserved 8 KiB")
ASSERT(LOADADDR(.data) + SIZEOF(.data) < 0x1f0000, "Lua crosses stock user-data boundary")
