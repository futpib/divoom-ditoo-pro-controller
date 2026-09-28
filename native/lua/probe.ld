ENTRY(probe)
stock_dispatch_end = 0x3b30a;
stock_version = 0x4b824;
stock_set_version = 0x4b858;
stock_reply = 0x3901c;
stock_free_heap = 0x83ea0;
stock_suspend = 0x84dec;
stock_resume = 0x84f3c;
SECTIONS {
    .hook 0x3ab9c : { *(.hook) }
    .text 0x1ca000 : { *(.text*) *(.rodata*) }
    .data : { *(.data*) *(.sdata*) }
    .bss : { *(.bss*) *(.sbss*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) }
}
ASSERT(SIZEOF(.hook) <= 28, "hook overwrites next command")
ASSERT(SIZEOF(.data) == 0 && SIZEOF(.bss) == 0, "probe must have no writable globals")
ASSERT(. < 0x1d0000, "probe crosses SDK storage boundary")
