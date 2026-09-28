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
