ENTRY(runtime_command)
INCLUDE native/patches/stock-306007.ld
SECTIONS {
    INCLUDE target/lua-app/runtime/patch-layout.ld
    .text 0x1ca000 : { *(.text*) *(EXCLUDE_FILE (*libm.a:libm_a-ef_sqrt.o) .rodata*) }
    .data 0x2004b000 : AT(ALIGN(ADDR(.text) + SIZEOF(.text), 8)) {
        __data_start = .;
        /* sqrtf alone uses GP-relative constants; all other math tables stay in flash. */
        *libm.a:libm_a-ef_sqrt.o(.rodata*) *(.data*) *(.sdata*)
        __data_end = .;
    }
    __data_load = LOADADDR(.data);
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*) *(.sbss*) *(COMMON)
        __bss_end = .;
    }
    /DISCARD/ : { *(.comment) *(.note*) }
}
ASSERT(ADDR(.text) + SIZEOF(.text) <= LOADADDR(.data), "Lua overlaps its data load image")
ASSERT(__bss_end <= 0x2004c000, "Lua globals exceed reserved 4 KiB")
ASSERT(LOADADDR(.data) + SIZEOF(.data) + 0x613 <= 0x1f0000, "MVA exceeds stock Bluetooth staging capacity")
