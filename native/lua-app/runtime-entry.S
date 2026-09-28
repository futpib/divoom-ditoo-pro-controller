.section .hook,"ax"
.global hook
hook:
    lwi $r0, [$r8 + 0x164]
    move $r1, $r7
    lhi $r2, [$r6 + 2]
    jal runtime_command
    j stock_dispatch_end
.section .init_hook,"ax"
.global init_hook
init_hook:
    jal runtime_init

/* Stock FreeRTOS guarantees only four-byte task-stack alignment. */
.section .text.runtime_worker_entry,"ax"
.global runtime_worker_entry
runtime_worker_entry:
    bitci $sp, $sp, 7
    j runtime_worker

.section .screen_hook,"ax"
    j screen_filter
.section .key_hook,"ax"
    jal key_filter
.section .led_hook,"ax"
    j led_filter

.section .text.screen_filter,"ax"
screen_filter:
    smw.adm $r6, [$sp], $r6, 2
    move $r6, $r0
    jal runtime_screen_allowed
    beqz $r0, screen_blocked
    move $r0, $r6
    lmw.bim $r6, [$sp], $r6, 2
    smw.adm $r6, [$sp], $r6, 2
    j stock_screen_original
screen_blocked:
    movi $r0, 1
    lmw.bim $r6, [$sp], $r6, 2
    ret

.section .text.key_filter,"ax"
key_filter:
    smw.adm $r6, [$sp], $r6, 2
    jal stock_adc_scan
    jal runtime_adc_result
    lmw.bim $r6, [$sp], $r6, 2
    ret

.section .text.runtime_led_context,"ax"
.global runtime_led_context
runtime_led_context:
    /* Stock lwi.gp r0, [gp - 33664]; preserve the verified instruction bytes. */
    .byte 0x3c, 0x0d, 0xdf, 0x20
    ret

.section .text.led_filter,"ax"
led_filter:
    smw.adm $r6, [$sp], $r6, 2
    jal runtime_led_override
    beqz $r0, led_stock
    lmw.bim $r6, [$sp], $r6, 2
    ret
led_stock:
    lmw.bim $r6, [$sp], $r6, 2
    smw.adm $sp, [$sp], $sp, 2
    j stock_led_original
