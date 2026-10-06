.flag verbatim
/* Stock instruction edits. Site addresses and size limits come from the manifest. */
.section .patch_firmware_version,"ax"
    movi $r0, FIRMWARE_VERSION

.section .patch_allow_reflash,"ax"
    j8 stock_update_accept

#if defined(PROFILE_APP) || defined(PROFILE_RUNTIME)
.section .patch_heap_limit,"ax"
#ifdef PROFILE_APP
    sethi $r0, 0x2004b
#else
    sethi $r0, 0x2004a
#endif
#endif

#ifdef PROFILE_APP
.section .patch_alarm_cleanup,"ax"
    movi $r3, 9 * 16
.section .patch_wake_cleanup,"ax"
    movi $r3, 9 * 16
.section .patch_wake_slot,"ax"
    nop16
    nop16
#endif
