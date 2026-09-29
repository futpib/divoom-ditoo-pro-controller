ENTRY(probe)
INCLUDE native/patches/stock-306007.ld
SECTIONS {
    INCLUDE target/lua/patch-layout.ld
    .text 0x1ca000 : { *(.text*) *(.rodata*) }
    .data : { *(.data*) *(.sdata*) }
    .bss : { *(.bss*) *(.sbss*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) }
}
ASSERT(SIZEOF(.data) == 0 && SIZEOF(.bss) == 0, "probe must have no writable globals")
ASSERT(. < 0x1d0000, "probe crosses SDK storage boundary")
