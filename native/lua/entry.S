.section .hook,"ax"
.global hook
hook:
    lwi $r0, [$r8 + 0x164]
    move $r1, $r7
    lhi $r2, [$r6 + 2]
    jal probe
    j stock_dispatch_end
