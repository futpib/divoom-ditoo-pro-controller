INCLUDE native/patches/stock-306007.ld
SECTIONS {
    INCLUDE target/reflash/patch-layout.ld
    /DISCARD/ : { *(.comment) *(.note*) }
}
