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
    lwi.gp $r0, [+ -33664]
    ret

.section .text.runtime_fs_context,"ax"
.global runtime_fs_context
runtime_fs_context:
    /* Stock lwi.gp r0, [gp - 33692]. */
    lwi.gp $r0, [+ -33692]
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

.section .native_hook,"ax"
    jal native_service
.section .bt_command_hook,"ax"
    jal runtime_bt_peek
.section .bt_stack_hook,"ax"
    j bt_stack_trace
.section .bt_hci_hook,"ax"
    j bt_hci_trace
    nop16
.section .text.bt_stack_trace,"ax"
bt_stack_trace:
    smw.adm $r6, [$sp], $r7, 2
    move $r6, $r0
    move $r7, $r1
    jal runtime_bt_trace_stack
    move $r0, $r6
    move $r1, $r7
    lmw.bim $r6, [$sp], $r7, 2
    subi45 $r0, 1
    slti45 $r0, 16
    j stock_bt_stack_resume
.section .text.bt_hci_trace,"ax"
bt_hci_trace:
    smw.adm $r6, [$sp], $r6, 2
    move $r6, $r0
    jal runtime_bt_trace_hci
    move $r0, $r6
    lmw.bim $r6, [$sp], $r6, 2
    /* Reproduce the exact push25 and gp-relative fp setup at HciProcessEvent. */
    push25 $r14, 24
    addi.gp $fp, -35356
    j stock_bt_hci_resume
.section .indicator_hook,"ax"
    j indicator_filter
.section .noise_hook,"ax"
    jal runtime_noise_sample

.section .text.native_service,"ax"
native_service:
    smw.adm $r6, [$sp], $r6, 2
    jal stock_native_pump
    jal runtime_native_service
    jal runtime_usb_service
    lmw.bim $r6, [$sp], $r6, 2
    ret

.section .usb_receive_hook,"ax"
    jal runtime_usb_receive
.section .usb_send_hook,"ax"
    jal runtime_usb_send
.section .usb_response_hook,"ax"
    j runtime_usb_response
.section .usb_connected_hook,"ax"
    jal runtime_usb_connected

.section .text.stock_response_original,"ax"
.global stock_response_original
stock_response_original:
    smw.adm $r6, [$sp], $r12, 2
    j stock_response_resume

.section .text.runtime_usb_dispatch,"ax"
.global runtime_usb_dispatch
runtime_usb_dispatch:
    /* Same frame as 0x398c8; bypass only Bluetooth envelope handling. */
    smw.adm $r6, [$sp], $r9, 2
    addi $sp, $sp, -220
    move $r6, $r0
    move $r8, $r1
    addi $r0, $sp, 200
    movi $r1, 0
    movi $r2, 12
    jal memset
    movi $r0, 15
    sbi $r0, [$sp + 204]
    j stock_dispatch_command

.section .text.runtime_irq_save,"ax"
.global runtime_irq_save
runtime_irq_save:
    mfsr $r0, $PSW
    setgie.d
    dsb
    ret
.section .text.runtime_irq_restore,"ax"
.global runtime_irq_restore
runtime_irq_restore:
    andi $r0, $r0, 1
    beqz $r0, irq_restore_done
    setgie.e
    dsb
irq_restore_done:
    ret

.section .menu_root_init_hook,"ax"
    la $r0, runtime_menu_root
.section .menu_root_back_hook,"ax"
    la $r0, runtime_menu_root
.section .menu_select_hook,"ax"
    j runtime_menu_select
.section .text.stock_menu_dispatch,"ax"
.global stock_menu_dispatch
stock_menu_dispatch:
    smw.adm $r6, [$sp], $r6, 2
    j stock_menu_dispatch_resume
.section .usb_mode_hook,"ax"
    j runtime_usb_select
.section .text.stock_usb_mode_original,"ax"
.global stock_usb_mode_original
stock_usb_mode_original:
    addi.gp $r3, -36924
    j stock_usb_mode_resume
.section .source_select_hook,"ax"
    j runtime_source_select
.section .text.stock_source_original,"ax"
.global stock_source_original
stock_source_original:
    smw.adm $sp, [$sp], $sp, 2
    j stock_source_resume

.section .text.indicator_filter,"ax"
indicator_filter:
    smw.adm $r6, [$sp], $r6, 2
    jal runtime_indicator_filter
    j stock_indicator_resume

.section .text.stock_indicator_write,"ax"
.global stock_indicator_write
stock_indicator_write:
    smw.adm $r6, [$sp], $r6, 2
    j stock_indicator_resume

.section .wake_select_hook,"ax"
    jal wake_selected
.section .text.wake_selected,"ax"
wake_selected:
    /* r0 is the new earliest time; r6/r7 are the entry and its slot. */
    swi.gp $r0, [+ -37052]
    lbi $r1, [$r6 + 5]
    movi $r0, -1
    cmovz $r0, $r7, $r1
    sbi.gp $r0, [+ -34020]
    ret

.section .bt_app_disconnect_hook,"ax"
    j bt_app_disconnect_trace
.section .text.bt_app_disconnect_trace,"ax"
bt_app_disconnect_trace:
    smw.adm $r0, [$sp], $r3, 2
    move $r0, $lp
    jal runtime_bt_trace_app_disconnect
    bnez $r0, bt_app_disconnect_skip
    lmw.bim $r0, [$sp], $r3, 2
    smw.adm $sp, [$sp], $sp, 2
    j stock_bt_app_disconnect_resume
bt_app_disconnect_skip:
    lmw.bim $r0, [$sp], $r3, 2
    ret

.section .bt_force_disconnect_hook,"ax"
    j bt_force_disconnect_trace
.section .text.bt_force_disconnect_trace,"ax"
bt_force_disconnect_trace:
    smw.adm $r0, [$sp], $r3, 2
    move $r0, $r1
    move $r1, $r2
    move $r2, $r3
    move $r3, $lp
    jal runtime_bt_trace_force_disconnect
    lmw.bim $r0, [$sp], $r3, 2
    push25 $r10, 0
    slti45 $r2, 0x16
    j stock_bt_force_disconnect_resume

.section .bt_link_disconnect_hook,"ax"
    j bt_link_disconnect_trace
    nop16
.section .text.bt_link_disconnect_trace,"ax"
bt_link_disconnect_trace:
    smw.adm $r0, [$sp], $r3, 2
    move $r1, $lp
    jal runtime_bt_trace_link_disconnect
    lmw.bim $r0, [$sp], $r3, 2
    push25 $r6, 0
    addi.gp $fp, -35356
    j stock_bt_link_disconnect_resume
