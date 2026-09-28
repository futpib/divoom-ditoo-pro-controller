# Stock 306007 code.bin, MVA SHA256:
# fc16341c005b11d0ac476917dc2fd98c9bc7481b92a183e64bfe209801566544
# Bounded NDS32 disassembly: USB report reception, flash readback and updater.
# See docs/usb.md for the traced command/metadata fields and addresses.

    2910:	3a 6f 9c bc 	smw.adm $r6, [$sp], $r7, #0x2    ! {$r6~$r7, $lp}
    2914:	ef f4       	addi10.sp #-12
    2916:	80 e0       	mov55 $r7, $r0
    2918:	49 00 1e 73 	jal	0x65fe
    291c:	80 c0       	mov55 $r6, $r0
    291e:	3e 0e cf ac 	addi.gp	$r0, #-77908
    2922:	84 20       	movi55 $r1, #0
    2924:	84 48       	movi55 $r2, #8
    2926:	49 00 26 47 	jal	0x75b4
    292a:	97 96       	bmski33 $r6, #2
    292c:	97 b0       	zeb33 $r6, $r6
    292e:	c6 04       	beqz38 $r6, 0x2936
    2930:	84 00       	movi55 $r0, #0
    2932:	49 00 1e 02 	jal	0x6536
    2936:	3e 0e cf ac 	addi.gp	$r0, #-77908
    293a:	84 28       	movi55 $r1, #8
    293c:	b0 81       	addri36.sp $r2, #4
    293e:	49 00 1e ce 	jal	0x66da
    2942:	c8 68       	bnez38 $r0, 0x2a12
    2944:	2e 26 cf ac 	lbi.gp	$r2, [ + #-77908]
    2948:	ca 0a       	bnez38 $r2, 0x295c
    294a:	2e 06 cf ad 	lbi.gp	$r0, [ + #-77907]
    294e:	c8 07       	bnez38 $r0, 0x295c
    2950:	2e 06 cf ae 	lbi.gp	$r0, [ + #-77906]
    2954:	c8 04       	bnez38 $r0, 0x295c
    2956:	2e 06 cf af 	lbi.gp	$r0, [ + #-77905]
    295a:	c0 5e       	beqz38 $r0, 0x2a16
    295c:	96 52       	seb33 $r1, $r2
    295e:	4e 15 00 4d 	bltz	$r1, 0x29f8
    2962:	2e 16 cf b2 	lbi.gp	$r1, [ + #-77902]
    2966:	c9 04       	bnez38 $r1, 0x296e
    2968:	2e 06 cf b3 	lbi.gp	$r0, [ + #-77901]
    296c:	c0 46       	beqz38 $r0, 0x29f8
    296e:	2e 66 cf b3 	lbi.gp	$r6, [ + #-77901]
    2972:	40 63 20 08 	slli	$r6, $r6, #8
    2976:	88 c1       	add45 $r6, $r1
    2978:	5a 28 21 2d 	bnec	$r2, #0x21, 0x29d2
    297c:	2e 06 cf ad 	lbi.gp	$r0, [ + #-77907]
    2980:	5a 08 09 29 	bnec	$r0, #9, 0x29d2
    2984:	2e 06 cf ae 	lbi.gp	$r0, [ + #-77906]
    2988:	c8 25       	bnez38 $r0, 0x29d2
    298a:	2e 06 cf af 	lbi.gp	$r0, [ + #-77905]
    298e:	5a 08 02 22 	bnec	$r0, #2, 0x29d2
    2992:	80 07       	mov55 $r0, $r7
    2994:	44 10 00 40 	movi	$r1, #0x40
    2998:	b0 81       	addri36.sp $r2, #4
    299a:	84 6a       	movi55 $r3, #0xa
    299c:	49 00 1e 63 	jal	0x6662
    29a0:	50 03 80 40 	addi	$r0, $r7, #0x40
    29a4:	44 10 00 40 	movi	$r1, #0x40
    29a8:	b0 81       	addri36.sp $r2, #4
    29aa:	84 6a       	movi55 $r3, #0xa
    29ac:	49 00 1e 5b 	jal	0x6662
    2f06:	3a 6f a8 bc 	smw.adm $r6, [$sp], $r10, #0x2    ! {$r6~$r10, $lp}
    2f0a:	81 20       	mov55 $r9, $r0
    2f0c:	81 01       	mov55 $r8, $r1
    2f0e:	80 c2       	mov55 $r6, $r2
    2f10:	84 6a       	movi55 $r3, #0xa
    2f12:	49 ff f0 ff 	jal	0x1110
    2f16:	50 74 10 00 	addi	$r7, $r8, #0x1000
    2f1a:	80 09       	mov55 $r0, $r9
    2f1c:	80 27       	mov55 $r1, $r7
    2f1e:	80 46       	mov55 $r2, $r6
    2f20:	84 6a       	movi55 $r3, #0xa
    2f22:	49 ff f0 16 	jal	0xf4e
    2f26:	80 08       	mov55 $r0, $r8
    2f28:	80 27       	mov55 $r1, $r7
    2f2a:	80 46       	mov55 $r2, $r6
    2f2c:	49 00 23 12 	jal	0x7550
    2f30:	c0 03       	beqz38 $r0, 0x2f36
    2f32:	84 00       	movi55 $r0, #0
    2f34:	d5 02       	j8 0x2f38
    2f36:	84 01       	movi55 $r0, #1
    2f38:	3a 6f a8 84 	lmw.bim $r6, [$sp], $r10, #0x2    ! {$r6~$r10, $lp}
    2f3c:	dd 9e       	ret5 $lp
    2f3e:	00 00 3a 6f 	lbi	$r0, [$r0 + #0x3a6f]
    2f42:	b4 bc       	lwi450 $r5, [$fp]
    2f44:	ef ec       	addi10.sp #-20
    2f46:	3c 0d b3 f2 	lwi.gp	$r0, [ + #-77880]
    2f4a:	3e 18 10 dc 	addi.gp	$r1, #0x10dc
    2f4e:	84 49       	movi55 $r2, #9
    2f50:	49 00 09 18 	jal	0x4180
    2f54:	2e 00 10 dc 	lbi.gp	$r0, [ + #0x10dc]
    2f58:	5a 08 4d 0e 	bnec	$r0, #0x4d, 0x2f74
    2f5c:	2e 10 10 dd 	lbi.gp	$r1, [ + #0x10dd]
    2f60:	5a 18 56 0a 	bnec	$r1, #0x56, 0x2f74
    2f64:	2e 10 10 de 	lbi.gp	$r1, [ + #0x10de]
    2f68:	5a 18 b1 06 	bnec	$r1, #0xb1, 0x2f74
    2f6c:	2e 10 10 df 	lbi.gp	$r1, [ + #0x10df]
    2f70:	5a 10 58 22 	beqc	$r1, #0x58, 0x2fb4
    2f74:	b6 1f       	swi450 $r0, [$sp]
    2f76:	44 00 89 20 	movi	$r0, #0x8920
    2f7a:	49 00 23 35 	jal	0x75e4
    2f7e:	3e 68 10 dc 	addi.gp	$r6, #0x10dc
    2f82:	a6 30       	lbi333 $r0, [$r6 + #0]
    2f84:	b6 1f       	swi450 $r0, [$sp]
    2f86:	44 00 89 28 	movi	$r0, #0x8928
    2f8a:	49 00 23 2d 	jal	0x75e4
    2f8e:	a6 30       	lbi333 $r0, [$r6 + #0]
    2f90:	b6 1f       	swi450 $r0, [$sp]
    2f92:	44 00 89 30 	movi	$r0, #0x8930
    2f96:	49 00 23 27 	jal	0x75e4
    2f9a:	a6 30       	lbi333 $r0, [$r6 + #0]
    2f9c:	b6 1f       	swi450 $r0, [$sp]
    2f9e:	44 00 89 38 	movi	$r0, #0x8938
    2fa2:	49 00 23 21 	jal	0x75e4
    2fa6:	44 00 89 40 	movi	$r0, #0x8940
    2faa:	49 00 23 22 	jal	0x75ee
    2fae:	84 0b       	movi55 $r0, #0xb
    2fb0:	48 00 01 e6 	j	0x337c
    2fb4:	2e 80 10 e0 	lbi.gp	$r8, [ + #0x10e0]
    2fb8:	3e 0f ff f0 	addi.gp	$r0, #-16
    2fbc:	49 ff fd c6 	jal	0x2b48
    2fc0:	80 c0       	mov55 $r6, $r0
    2fc2:	4e 03 01 db 	bnez	$r0, 0x3378
    2fc6:	2e 06 cf c0 	lbi.gp	$r0, [ + #-77888]
    2fca:	5a 00 55 11 	beqc	$r0, #0x55, 0x2fec
    2fce:	2e 06 cf c1 	lbi.gp	$r0, [ + #-77887]
    2fd2:	5a 00 55 0d 	beqc	$r0, #0x55, 0x2fec
    2fd6:	2e 06 cf c2 	lbi.gp	$r0, [ + #-77886]
    2fda:	5a 00 55 09 	beqc	$r0, #0x55, 0x2fec
    2fde:	44 00 89 58 	movi	$r0, #0x8958
    2fe2:	49 00 23 06 	jal	0x75ee
    2fe6:	84 02       	movi55 $r0, #2
    2fe8:	48 00 01 ca 	j	0x337c
    2fec:	3c 0d b3 f2 	lwi.gp	$r0, [ + #-77880]
    2ff0:	8c 05       	addi45 $r0, #5
    2ff2:	3e 18 10 dc 	addi.gp	$r1, #0x10dc
    2ff6:	84 49       	movi55 $r2, #9
    2ff8:	49 00 08 c4 	jal	0x4180
    2ffc:	84 e5       	movi55 $r7, #5
    2ffe:	2e 10 10 dc 	lbi.gp	$r1, [ + #0x10dc]
    3002:	9e 09       	subi333 $r0, $r1, #1
    3004:	e6 05       	slti45 $r0, #5
    3006:	4e f2 01 a4 	beqz	$r15, 0x334e
    300a:	44 f0 30 14 	movi	$r15, #0x3014
    300e:	38 07 82 02 	lw	$r0, [$r15 + ($r0 << #2)]
    3012:	dd 00       	jr5 $r0
    3014:	28 30 00 00 	lbsi.bi	$r3, [$r0], #0
    3018:	9e 30       	subi333 $r0, $r6, #0
    301a:	00 00 7a 30 	lbi	$r0, [$r0 + #-1488]
    301e:	00 00 4c 32 	lbi	$r0, [$r0 + #-13262]
    3384:	3b ff fc bc 	smw.adm $sp, [$sp], $sp, #0x2    ! {$lp}
    3388:	ef fc       	addi10.sp #-4
    338a:	49 ff fa c3 	jal	0x2910
    338e:	c0 03       	beqz38 $r0, 0x3394
    3390:	84 01       	movi55 $r0, #1
    3392:	d5 02       	j8 0x3396
    3394:	84 00       	movi55 $r0, #0
    3396:	ec 04       	addi10.sp #4
    3398:	3b ff fc 84 	lmw.bim $sp, [$sp], $sp, #0x2    ! {$lp}
    339c:	dd 9e       	ret5 $lp
    339e:	3a 6f a0 bc 	smw.adm $r6, [$sp], $r8, #0x2    ! {$r6~$r8, $lp}
    33a2:	ef f0       	addi10.sp #-16
    33a4:	81 00       	mov55 $r8, $r0
    33a6:	3c 6d fb f8 	lwi.gp	$r6, [ + #-4128]
    33aa:	b0 01       	addri36.sp $r0, #4
    33ac:	84 28       	movi55 $r1, #8
    33ae:	b0 83       	addri36.sp $r2, #0xc
    33b0:	49 00 19 95 	jal	0x66da
    33b4:	c8 18       	bnez38 $r0, 0x33e4
    33b6:	00 1f 80 04 	lbi	$r1, [$sp + #4]
    33ba:	5a 18 a1 15 	bnec	$r1, #0xa1, 0x33e4
    33be:	00 1f 80 05 	lbi	$r1, [$sp + #5]
    33c2:	5a 18 01 11 	bnec	$r1, #1, 0x33e4
    33c6:	00 1f 80 07 	lbi	$r1, [$sp + #7]
    33ca:	5a 18 01 0d 	bnec	$r1, #1, 0x33e4
    33ce:	00 7f 80 0b 	lbi	$r7, [$sp + #0xb]
    33d2:	5a 78 01 09 	bnec	$r7, #1, 0x33e4
    33d6:	80 08       	mov55 $r0, $r8
    33d8:	44 10 01 00 	movi	$r1, #0x100
    33dc:	84 41       	movi55 $r2, #1
    33de:	49 00 19 15 	jal	0x6608
    33e2:	d5 08       	j8 0x33f2
    33e4:	3c 1d fb f8 	lwi.gp	$r1, [ + #-4128]
    33e8:	8a 26       	sub45 $r1, $r6
    33ea:	5c f0 80 65 	slti	$r15, $r1, #0x65
    33ee:	e9 de       	bnezs8 0x33aa
    33f0:	84 e0       	movi55 $r7, #0
    33f2:	80 07       	mov55 $r0, $r7
    33f4:	ec 10       	addi10.sp #0x10
    33f6:	3a 6f a0 84 	lmw.bim $r6, [$sp], $r8, #0x2    ! {$r6~$r8, $lp}
    33fa:	dd 9e       	ret5 $lp
    33fc:	3a 6f a0 bc 	smw.adm $r6, [$sp], $r8, #0x2    ! {$r6~$r8, $lp}
    3400:	80 e0       	mov55 $r7, $r0
    3402:	3c 8d fb f8 	lwi.gp	$r8, [ + #-4128]
    3406:	84 c0       	movi55 $r6, #0
    3408:	d5 0f       	j8 0x3426
    340a:	40 03 20 08 	slli	$r0, $r6, #8
    340e:	88 07       	add45 $r0, $r7
    3410:	49 ff ff ba 	jal	0x3384
    3414:	c8 08       	bnez38 $r0, 0x3424
    3416:	3c 1d fb f8 	lwi.gp	$r1, [ + #-4128]
    341a:	8a 28       	sub45 $r1, $r8
    341c:	5c f0 83 e9 	slti	$r15, $r1, #0x3e9
    3420:	e9 f5       	bnezs8 0x340a
    3422:	d5 05       	j8 0x342c
    3424:	8c c1       	addi45 $r6, #1
    3426:	e4 d0       	sltsi45 $r6, #0x10
    3428:	e9 f1       	bnezs8 0x340a
    342a:	84 01       	movi55 $r0, #1
    342c:	3a 6f a0 84 	lmw.bim $r6, [$sp], $r8, #0x2    ! {$r6~$r8, $lp}
    3430:	dd 9e       	ret5 $lp
    3432:	3a 6f a4 bc 	smw.adm $r6, [$sp], $r9, #0x2    ! {$r6~$r9, $lp}
    3436:	ef b4       	addi10.sp #-76
    3438:	84 00       	movi55 $r0, #0
    343a:	f0 91       	swi37.sp $r0, [ + #0x44]
    343c:	f0 90       	swi37.sp $r0, [ + #0x40]
    343e:	f0 8f       	swi37.sp $r0, [ + #0x3c]
    3440:	f0 8e       	swi37.sp $r0, [ + #0x38]
    3442:	f0 8d       	swi37.sp $r0, [ + #0x34]
    3444:	f0 8c       	swi37.sp $r0, [ + #0x30]
    3446:	f0 8b       	swi37.sp $r0, [ + #0x2c]
    3448:	f0 8a       	swi37.sp $r0, [ + #0x28]
    344a:	f0 89       	swi37.sp $r0, [ + #0x24]
    344c:	46 04 00 00 	sethi	$r0, #0x40000
    3450:	02 00 01 86 	lhi	$r0, [$r0 + #0x30c]
    3454:	3c 0f b3 ed 	swi.gp	$r0, [ + #-77900]
    3458:	3e 08 10 b8 	addi.gp	$r0, #0x10b8
    345c:	44 10 00 c8 	movi	$r1, #0xc8
    3460:	49 ff f8 92 	jal	0x2584
    3464:	84 e0       	movi55 $r7, #0
    3466:	49 00 1f 74 	jal	0x734e
    346a:	3e 08 10 b8 	addi.gp	$r0, #0x10b8
    346e:	49 ff f8 9b 	jal	0x25a4
    3472:	c0 1e       	beqz38 $r0, 0x34ae
    3474:	3e 08 10 b8 	addi.gp	$r0, #0x10b8
    3478:	44 10 00 c8 	movi	$r1, #0xc8
    347c:	49 ff f8 84 	jal	0x2584
    3480:	3c 1d b3 f3 	lwi.gp	$r1, [ + #-77876]
    3484:	96 4c       	xlsb33 $r1, $r1
    3486:	46 04 00 00 	sethi	$r0, #0x40000
    348a:	c1 06       	beqz38 $r1, 0x3496
    348c:	02 00 01 86 	lhi	$r0, [$r0 + #0x30c]
    3490:	3c 0f b3 ed 	swi.gp	$r0, [ + #-77900]
    3494:	d5 08       	j8 0x34a4
    3496:	02 50 01 86 	lhi	$r5, [$r0 + #0x30c]
    349a:	3c 0d b3 ed 	lwi.gp	$r0, [ + #-77900]
    349e:	d8 03       	bnes38 $r0, 0x34a4
    34a0:	49 00 19 fc 	jal	0x6898
    34a4:	3c 1d b3 f3 	lwi.gp	$r1, [ + #-77876]
    34a8:	8c 21       	addi45 $r1, #1
    34aa:	3c 1f b3 f3 	swi.gp	$r1, [ + #-77876]
    34ae:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    34b2:	49 ff ff 69 	jal	0x3384
    34b6:	c0 d8       	beqz38 $r0, 0x3466
    34b8:	3e 68 10 dc 	addi.gp	$r6, #0x10dc
    34bc:	a6 f0       	lbi333 $r3, [$r6 + #0]
    34be:	a6 b1       	lbi333 $r2, [$r6 + #1]
    34c0:	a6 72       	lbi333 $r1, [$r6 + #2]
    34c2:	a6 33       	lbi333 $r0, [$r6 + #3]
    34c4:	b6 7f       	swi450 $r3, [$sp]
    34c6:	f2 81       	swi37.sp $r2, [ + #4]
    34c8:	f1 82       	swi37.sp $r1, [ + #8]
    34ca:	f0 83       	swi37.sp $r0, [ + #0xc]
    34cc:	44 00 8a dc 	movi	$r0, #0x8adc
    34d0:	49 00 20 8a 	jal	0x75e4
    34d4:	80 06       	mov55 $r0, $r6
    34d6:	3c 1d b3 c9 	lwi.gp	$r1, [ + #-78044]
    34da:	84 44       	movi55 $r2, #4
    34dc:	49 00 20 3a 	jal	0x7550
    34e0:	4e 03 01 ba 	bnez	$r0, 0x3854
    34e4:	3e 08 10 fc 	addi.gp	$r0, #0x10fc
    34e8:	3a 00 00 00 	lmw.bi $r0, [$r0], $r0, #0x0    ! {$r0}
    34ec:	b0 51       	addri36.sp $r1, #0x44
    34ee:	3a 00 80 20 	smw.bi $r0, [$r1], $r0, #0x0    ! {$r0}
    34f2:	3e 08 11 00 	addi.gp	$r0, #0x1100
    34f6:	3a 00 00 00 	lmw.bi $r0, [$r0], $r0, #0x0    ! {$r0}
    34fa:	b0 50       	addri36.sp $r1, #0x40
    34fc:	3a 00 80 20 	smw.bi $r0, [$r1], $r0, #0x0    ! {$r0}
    3500:	3e 08 10 e4 	addi.gp	$r0, #0x10e4
    3504:	3a 00 00 00 	lmw.bi $r0, [$r0], $r0, #0x0    ! {$r0}
    3508:	b0 4f       	addri36.sp $r1, #0x3c
    350a:	3a 00 80 20 	smw.bi $r0, [$r1], $r0, #0x0    ! {$r0}
    350e:	3e 08 11 0c 	addi.gp	$r0, #0x110c
    3512:	3a 00 00 00 	lmw.bi $r0, [$r0], $r0, #0x0    ! {$r0}
    3516:	b0 4e       	addri36.sp $r1, #0x38
    3518:	3a 00 80 20 	smw.bi $r0, [$r1], $r0, #0x0    ! {$r0}
    351c:	3e 08 10 f4 	addi.gp	$r0, #0x10f4
    3520:	3a 00 00 00 	lmw.bi $r0, [$r0], $r0, #0x0    ! {$r0}
    3524:	b0 49       	addri36.sp $r1, #0x24
    3526:	3a 00 80 20 	smw.bi $r0, [$r1], $r0, #0x0    ! {$r0}
    352a:	3e 08 10 e8 	addi.gp	$r0, #0x10e8
    352e:	3a 00 00 00 	lmw.bi $r0, [$r0], $r0, #0x0    ! {$r0}
    3532:	b0 4d       	addri36.sp $r1, #0x34
    3534:	3a 00 80 20 	smw.bi $r0, [$r1], $r0, #0x0    ! {$r0}
    3538:	3e 08 10 ec 	addi.gp	$r0, #0x10ec
    353c:	3a 00 00 00 	lmw.bi $r0, [$r0], $r0, #0x0    ! {$r0}
    3540:	b0 4c       	addri36.sp $r1, #0x30
    3542:	3a 00 80 20 	smw.bi $r0, [$r1], $r0, #0x0    ! {$r0}
    3546:	3e 08 11 04 	addi.gp	$r0, #0x1104
    354a:	3a 00 00 00 	lmw.bi $r0, [$r0], $r0, #0x0    ! {$r0}
    354e:	b0 4b       	addri36.sp $r1, #0x2c
    3550:	3a 00 80 20 	smw.bi $r0, [$r1], $r0, #0x0    ! {$r0}
    3554:	3e 08 11 08 	addi.gp	$r0, #0x1108
    3558:	3a 00 00 00 	lmw.bi $r0, [$r0], $r0, #0x0    ! {$r0}
    355c:	b0 4a       	addri36.sp $r1, #0x28
    355e:	3a 00 80 20 	smw.bi $r0, [$r1], $r0, #0x0    ! {$r0}
    3562:	44 00 8a f4 	movi	$r0, #0x8af4
    3566:	49 00 20 3f 	jal	0x75e4
    356a:	f0 11       	lwi37.sp $r0, [ + #0x44]
    356c:	b6 1f       	swi450 $r0, [$sp]
    356e:	44 00 8b 00 	movi	$r0, #0x8b00
    3572:	49 00 20 39 	jal	0x75e4
    3576:	f0 10       	lwi37.sp $r0, [ + #0x40]
    3578:	b6 1f       	swi450 $r0, [$sp]
    357a:	44 00 8b 10 	movi	$r0, #0x8b10
    357e:	49 00 20 33 	jal	0x75e4
    3582:	f0 0f       	lwi37.sp $r0, [ + #0x3c]
    3584:	b6 1f       	swi450 $r0, [$sp]
    3586:	44 00 8b 20 	movi	$r0, #0x8b20
    358a:	49 00 20 2d 	jal	0x75e4
    358e:	f0 0e       	lwi37.sp $r0, [ + #0x38]
    3590:	b6 1f       	swi450 $r0, [$sp]
    3592:	44 00 8b 30 	movi	$r0, #0x8b30
    3596:	49 00 20 27 	jal	0x75e4
    359a:	f0 0d       	lwi37.sp $r0, [ + #0x34]
    359c:	b6 1f       	swi450 $r0, [$sp]
    359e:	44 00 8b 40 	movi	$r0, #0x8b40
    35a2:	49 00 20 21 	jal	0x75e4
    35a6:	f0 0b       	lwi37.sp $r0, [ + #0x2c]
    35a8:	b6 1f       	swi450 $r0, [$sp]
    35aa:	44 00 8b 54 	movi	$r0, #0x8b54
    35ae:	49 00 20 1b 	jal	0x75e4
    35b2:	f0 0c       	lwi37.sp $r0, [ + #0x30]
    35b4:	b6 1f       	swi450 $r0, [$sp]
    35b6:	44 00 8b 68 	movi	$r0, #0x8b68
    35ba:	49 00 20 15 	jal	0x75e4
    35be:	f0 0a       	lwi37.sp $r0, [ + #0x28]
    35c0:	b6 1f       	swi450 $r0, [$sp]
    35c2:	44 00 8b 7c 	movi	$r0, #0x8b7c
    35c6:	49 00 20 0f 	jal	0x75e4
    35ca:	f0 09       	lwi37.sp $r0, [ + #0x24]
    35cc:	b6 1f       	swi450 $r0, [$sp]
    35ce:	44 00 8c 0c 	movi	$r0, #0x8c0c
    35d2:	49 00 20 09 	jal	0x75e4
    35d6:	84 3f       	movi55 $r1, #-1
    35d8:	10 13 00 08 	sbi	$r1, [$r6 + #8]
    35dc:	10 13 00 09 	sbi	$r1, [$r6 + #9]
    35e0:	10 13 00 0a 	sbi	$r1, [$r6 + #0xa]
    35e4:	10 13 00 0b 	sbi	$r1, [$r6 + #0xb]
    35e8:	10 13 00 0c 	sbi	$r1, [$r6 + #0xc]
    35ec:	00 03 00 34 	lbi	$r0, [$r6 + #0x34]
    35f0:	5a 00 02 04 	beqc	$r0, #2, 0x35f8
    35f4:	48 00 00 77 	j	0x36e2
    35f8:	2e 50 10 d4 	lbi.gp	$r5, [ + #0x10d4]
    35fc:	2e 00 10 f0 	lbi.gp	$r0, [ + #0x10f0]
    3600:	d0 0d       	beqs38 $r0, 0x361a
    3602:	44 00 8b 94 	movi	$r0, #0x8b94
    3606:	49 00 1f f4 	jal	0x75ee
    360a:	44 10 00 33 	movi	$r1, #0x33
    360e:	10 13 00 08 	sbi	$r1, [$r6 + #8]
    3612:	10 13 00 0b 	sbi	$r1, [$r6 + #0xb]
    3616:	84 ee       	movi55 $r7, #0xe
    3618:	d5 65       	j8 0x36e2
    361a:	3c 5c 04 32 	lwi.gp	$r5, [ + #0x10c8]
    361e:	f1 10       	lwi37.sp $r1, [ + #0x40]
    3620:	d1 1d       	beqs38 $r1, 0x365a
    3622:	b6 bf       	swi450 $r5, [$sp]
    3624:	44 00 8b b0 	movi	$r0, #0x8bb0
    3628:	49 00 1f de 	jal	0x75e4
    362c:	f1 11       	lwi37.sp $r1, [ + #0x44]
    362e:	40 00 a0 08 	slli	$r0, $r1, #8
    3632:	c0 0a       	beqz38 $r0, 0x3646
    3634:	44 01 00 00 	movi	$r0, #0x10000
    3638:	e2 01       	slt45 $r0, $r1
    363a:	e9 06       	bnezs8 0x3646
    363c:	44 00 00 55 	movi	$r0, #0x55
    3640:	3e 00 10 e7 	sbi.gp	$r0, [ + #0x10e7]
    3644:	d5 4f       	j8 0x36e2
    3646:	44 00 8b c8 	movi	$r0, #0x8bc8
    364a:	49 00 1f d2 	jal	0x75ee
    364e:	44 00 00 33 	movi	$r0, #0x33
    3652:	3e 00 10 e7 	sbi.gp	$r0, [ + #0x10e7]
    3656:	84 ed       	movi55 $r7, #0xd
    3658:	d5 45       	j8 0x36e2
    365a:	44 01 00 00 	movi	$r0, #0x10000
    365e:	f1 0f       	lwi37.sp $r1, [ + #0x3c]
    3660:	88 20       	add45 $r1, $r0
    3662:	3e 29 30 f4 	addi.gp	$r2, #0x130f4
    3666:	00 31 00 08 	lbi	$r3, [$r2 + #8]
    366a:	00 01 00 09 	lbi	$r0, [$r2 + #9]
    366e:	40 00 20 08 	slli	$r0, $r0, #8
    3672:	fe 1f       	or33 $r0, $r3
    3674:	00 31 00 0a 	lbi	$r3, [$r2 + #0xa]
    3678:	40 31 c0 08 	slli	$r3, $r3, #0x10
    367c:	fe 1f       	or33 $r0, $r3
    367e:	00 21 00 0b 	lbi	$r2, [$r2 + #0xb]
    3682:	40 21 60 08 	slli	$r2, $r2, #0x18
    3686:	fe 17       	or33 $r0, $r2
    3688:	e2 20       	slt45 $r1, $r0
    368a:	e9 0b       	bnezs8 0x36a0
    368c:	44 00 8b dc 	movi	$r0, #0x8bdc
    3690:	49 00 1f af 	jal	0x75ee
    3694:	44 00 00 33 	movi	$r0, #0x33
    3698:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    369c:	84 ef       	movi55 $r7, #0xf
    369e:	d5 22       	j8 0x36e2
    36a0:	2e 00 10 d5 	lbi.gp	$r0, [ + #0x10d5]
    36a4:	5a 00 5f 04 	beqc	$r0, #0x5f, 0x36ac
    36a8:	5a 08 55 10 	bnec	$r0, #0x55, 0x36c8
    36ac:	f1 0e       	lwi37.sp $r1, [ + #0x38]
    36ae:	3c 5c 04 34 	lwi.gp	$r5, [ + #0x10d0]
    36b2:	d1 18       	beqs38 $r1, 0x36e2
    36b4:	44 10 00 55 	movi	$r1, #0x55
    36b8:	3e 10 10 e4 	sbi.gp	$r1, [ + #0x10e4]
    36bc:	b6 1f       	swi450 $r0, [$sp]
    36be:	44 00 8b ec 	movi	$r0, #0x8bec
    36c2:	49 00 1f 91 	jal	0x75e4
    36c6:	d5 0e       	j8 0x36e2
    36c8:	3c 5c 04 33 	lwi.gp	$r5, [ + #0x10cc]
    36cc:	f1 09       	lwi37.sp $r1, [ + #0x24]
    36ce:	d1 0a       	beqs38 $r1, 0x36e2
    36d0:	b6 bf       	swi450 $r5, [$sp]
    36d2:	44 00 8c 08 	movi	$r0, #0x8c08
    36d6:	49 00 1f 87 	jal	0x75e4
    36da:	44 00 00 55 	movi	$r0, #0x55
    36de:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    36e2:	2e 00 11 11 	lbi.gp	$r0, [ + #0x1111]
    36e6:	5a 08 04 2d 	bnec	$r0, #4, 0x3740
    36ea:	f3 0c       	lwi37.sp $r3, [ + #0x30]
    36ec:	54 01 8f ff 	andi	$r0, $r3, #0xfff
    36f0:	c0 07       	beqz38 $r0, 0x36fe
    36f2:	44 00 8c 24 	movi	$r0, #0x8c24
    36f6:	49 00 1f 7c 	jal	0x75ee
    36fa:	fa e2       	movpi45 $r7, #0x12
    36fc:	d5 22       	j8 0x3740
    36fe:	f0 0d       	lwi37.sp $r0, [ + #0x34]
    3700:	88 60       	add45 $r3, $r0
    3702:	3e 29 30 f4 	addi.gp	$r2, #0x130f4
    3706:	00 01 00 08 	lbi	$r0, [$r2 + #8]
    370a:	00 11 00 09 	lbi	$r1, [$r2 + #9]
    370e:	40 10 a0 08 	slli	$r1, $r1, #8
    3712:	fe 47       	or33 $r1, $r0
    3714:	00 01 00 0a 	lbi	$r0, [$r2 + #0xa]
    3718:	40 00 40 08 	slli	$r0, $r0, #0x10
    371c:	fe 47       	or33 $r1, $r0
    371e:	00 01 00 0b 	lbi	$r0, [$r2 + #0xb]
    3722:	40 00 60 08 	slli	$r0, $r0, #0x18
    3726:	fe 0f       	or33 $r0, $r1
    3728:	e2 03       	slt45 $r0, $r3
    372a:	e8 07       	beqzs8 0x3738
    372c:	44 00 8c 40 	movi	$r0, #0x8c40
    3730:	49 00 1f 5f 	jal	0x75ee
    3734:	fa e0       	movpi45 $r7, #0x10
    3736:	d5 05       	j8 0x3740
    3738:	44 00 00 55 	movi	$r0, #0x55
    373c:	3e 00 10 e5 	sbi.gp	$r0, [ + #0x10e5]
    3740:	2e 00 11 12 	lbi.gp	$r0, [ + #0x1112]
    3744:	5a 08 05 2d 	bnec	$r0, #5, 0x379e
    3748:	f3 0a       	lwi37.sp $r3, [ + #0x28]
    374a:	54 01 8f ff 	andi	$r0, $r3, #0xfff
    374e:	c0 07       	beqz38 $r0, 0x375c
    3750:	44 00 8c 58 	movi	$r0, #0x8c58
    3754:	49 00 1f 4d 	jal	0x75ee
    3758:	fa e3       	movpi45 $r7, #0x13
    375a:	d5 22       	j8 0x379e
    375c:	f0 0b       	lwi37.sp $r0, [ + #0x2c]
    375e:	88 60       	add45 $r3, $r0
    3760:	3e 29 30 f4 	addi.gp	$r2, #0x130f4
    3764:	00 01 00 08 	lbi	$r0, [$r2 + #8]
    3768:	00 11 00 09 	lbi	$r1, [$r2 + #9]
    376c:	40 10 a0 08 	slli	$r1, $r1, #8
    3770:	fe 47       	or33 $r1, $r0
    3772:	00 01 00 0a 	lbi	$r0, [$r2 + #0xa]
    3776:	40 00 40 08 	slli	$r0, $r0, #0x10
    377a:	fe 47       	or33 $r1, $r0
    377c:	00 01 00 0b 	lbi	$r0, [$r2 + #0xb]
    3780:	40 00 60 08 	slli	$r0, $r0, #0x18
    3784:	fe 0f       	or33 $r0, $r1
    3786:	e2 03       	slt45 $r0, $r3
    3788:	e8 07       	beqzs8 0x3796
    378a:	44 00 8c 74 	movi	$r0, #0x8c74
    378e:	49 00 1f 30 	jal	0x75ee
    3792:	fa e1       	movpi45 $r7, #0x11
    3794:	d5 05       	j8 0x379e
    3796:	44 00 00 55 	movi	$r0, #0x55
    379a:	3e 00 10 e6 	sbi.gp	$r0, [ + #0x10e6]
    379e:	3e 68 10 dc 	addi.gp	$r6, #0x10dc
    37a2:	00 33 00 08 	lbi	$r3, [$r6 + #8]
    37a6:	00 23 00 09 	lbi	$r2, [$r6 + #9]
    37aa:	00 13 00 0a 	lbi	$r1, [$r6 + #0xa]
    37ae:	00 03 00 0b 	lbi	$r0, [$r6 + #0xb]
    37b2:	b6 7f       	swi450 $r3, [$sp]
    37b4:	f2 81       	swi37.sp $r2, [ + #4]
    37b6:	f1 82       	swi37.sp $r1, [ + #8]
    37b8:	f0 83       	swi37.sp $r0, [ + #0xc]
    37ba:	44 00 8c 8c 	movi	$r0, #0x8c8c
    37be:	49 00 1f 13 	jal	0x75e4
    37c2:	80 06       	mov55 $r0, $r6
    37c4:	49 ff fd ed 	jal	0x339e
    37c8:	00 03 00 08 	lbi	$r0, [$r6 + #8]
    37cc:	5a 00 33 0e 	beqc	$r0, #0x33, 0x37e8
    37d0:	2e 10 10 e5 	lbi.gp	$r1, [ + #0x10e5]
    37d4:	5a 10 33 0a 	beqc	$r1, #0x33, 0x37e8
    37d8:	2e 20 10 e6 	lbi.gp	$r2, [ + #0x10e6]
    37dc:	5a 20 33 06 	beqc	$r2, #0x33, 0x37e8
    37e0:	2e 30 10 e7 	lbi.gp	$r3, [ + #0x10e7]
    37e4:	5a 38 33 0b 	bnec	$r3, #0x33, 0x37fa
    37e8:	49 00 15 84 	jal	0x62f0
    37ec:	49 00 15 bb 	jal	0x6362
    37f0:	49 00 15 93 	jal	0x6316
    37f4:	80 07       	mov55 $r0, $r7
    37f6:	48 00 04 11 	j	0x4018
    37fa:	5a 00 55 08 	beqc	$r0, #0x55, 0x380a
    37fe:	5a 10 55 06 	beqc	$r1, #0x55, 0x380a
    3802:	5a 20 55 04 	beqc	$r2, #0x55, 0x380a
    3806:	5a 38 55 1e 	bnec	$r3, #0x55, 0x3842
    380a:	b0 04       	addri36.sp $r0, #0x10
    380c:	46 10 00 08 	sethi	$r1, #8
    3810:	02 10 87 b8 	lhi	$r1, [$r1 + #0xf70]
    3814:	12 1f 80 08 	shi	$r1, [$sp + #0x10]
    3818:	46 10 00 08 	sethi	$r1, #8
    381c:	00 10 8f 72 	lbi	$r1, [$r1 + #0xf72]
    3820:	10 1f 80 12 	sbi	$r1, [$sp + #0x12]
    3824:	b6 1f       	swi450 $r0, [$sp]
    3826:	84 03       	movi55 $r0, #3
    3828:	f0 81       	swi37.sp $r0, [ + #4]
    382a:	84 02       	movi55 $r0, #2
    382c:	49 ff ef de 	jal	0x17e8
    3830:	4e 02 03 eb 	beqz	$r0, 0x4006
    3834:	44 00 89 bc 	movi	$r0, #0x89bc
    3838:	49 00 1e db 	jal	0x75ee
    383c:	fa 09       	movpi45 $r0, #0x19
    383e:	48 00 03 ed 	j	0x4018
    3842:	49 00 15 57 	jal	0x62f0
    3846:	49 00 15 8e 	jal	0x6362
    384a:	49 00 15 66 	jal	0x6316
    384e:	84 02       	movi55 $r0, #2
    3850:	48 00 03 e4 	j	0x4018
    3854:	3c 6d b3 c2 	lwi.gp	$r6, [ + #-78072]
    3858:	80 06       	mov55 $r0, $r6
    385a:	49 00 1e f3 	jal	0x7640
    385e:	80 40       	mov55 $r2, $r0
    3860:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3864:	80 26       	mov55 $r1, $r6
    3866:	49 00 1e 75 	jal	0x7550
    386a:	c8 5e       	bnez38 $r0, 0x3926
    386c:	3e 09 30 fc 	addi.gp	$r0, #0x130fc
    3870:	3a 10 04 00 	lmw.bi $r1, [$r0], $r1, #0x0    ! {$r1}
    3874:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3878:	3a 10 04 20 	smw.bi $r1, [$r0], $r1, #0x0    ! {$r1}
    387c:	3e 39 30 f4 	addi.gp	$r3, #0x130f4
    3880:	00 01 80 08 	lbi	$r0, [$r3 + #8]
    3884:	00 01 80 09 	lbi	$r0, [$r3 + #9]
    3888:	00 61 80 0a 	lbi	$r6, [$r3 + #0xa]
    388c:	40 63 40 08 	slli	$r6, $r6, #0x10
    3890:	00 01 80 0b 	lbi	$r0, [$r3 + #0xb]
    3894:	40 00 60 08 	slli	$r0, $r0, #0x18
    3898:	ff 87       	or33 $r6, $r0
    389a:	92 d0       	srli45 $r6, #0x10
    389c:	8e c2       	subi45 $r6, #2
    389e:	97 b0       	zeb33 $r6, $r6
    38a0:	00 11 80 08 	lbi	$r1, [$r3 + #8]
    38a4:	00 01 80 09 	lbi	$r0, [$r3 + #9]
    38a8:	40 00 20 08 	slli	$r0, $r0, #8
    38ac:	40 20 80 04 	or	$r2, $r1, $r0
    38b0:	00 01 80 0a 	lbi	$r0, [$r3 + #0xa]
    38b4:	40 00 40 08 	slli	$r0, $r0, #0x10
    38b8:	40 10 08 04 	or	$r1, $r0, $r2
    38bc:	00 01 80 0b 	lbi	$r0, [$r3 + #0xb]
    38c0:	40 00 60 08 	slli	$r0, $r0, #0x18
    38c4:	fe 0f       	or33 $r0, $r1
    38c6:	81 06       	mov55 $r8, $r6
    38c8:	b6 1f       	swi450 $r0, [$sp]
    38ca:	f6 81       	swi37.sp $r6, [ + #4]
    38cc:	44 00 8c b0 	movi	$r0, #0x8cb0
    38d0:	49 00 1e 8a 	jal	0x75e4
    38d4:	84 01       	movi55 $r0, #1
    38d6:	f0 85       	swi37.sp $r0, [ + #0x14]
    38d8:	d5 1a       	j8 0x390c
    38da:	f0 05       	lwi37.sp $r0, [ + #0x14]
    38dc:	40 00 40 08 	slli	$r0, $r0, #0x10
    38e0:	44 11 00 00 	movi	$r1, #0x10000
    38e4:	49 ff fa 92 	jal	0x2e08
    38e8:	f1 05       	lwi37.sp $r1, [ + #0x14]
    38ea:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    38ee:	10 10 00 08 	sbi	$r1, [$r0 + #8]
    38f2:	10 60 00 09 	sbi	$r6, [$r0 + #9]
    38f6:	49 ff fd 54 	jal	0x339e
    38fa:	f0 05       	lwi37.sp $r0, [ + #0x14]
    38fc:	b6 1f       	swi450 $r0, [$sp]
    38fe:	44 00 8c e8 	movi	$r0, #0x8ce8
    3902:	49 00 1e 71 	jal	0x75e4
    3906:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3908:	8c 01       	addi45 $r0, #1
    390a:	f0 85       	swi37.sp $r0, [ + #0x14]
    390c:	50 14 00 01 	addi	$r1, $r8, #1
    3910:	f2 05       	lwi37.sp $r2, [ + #0x14]
    3912:	40 11 04 07 	slts	$r1, $r2, $r1
    3916:	c9 e2       	bnez38 $r1, 0x38da
    3918:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    391c:	84 48       	movi55 $r2, #8
    391e:	49 00 1e 4b 	jal	0x75b4
    3922:	48 00 03 72 	j	0x4006
    3926:	3c 6d b3 c8 	lwi.gp	$r6, [ + #-78048]
    392a:	80 06       	mov55 $r0, $r6
    392c:	49 00 1e 8a 	jal	0x7640
    3930:	80 40       	mov55 $r2, $r0
    3932:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3936:	80 26       	mov55 $r1, $r6
    3938:	49 00 1e 0c 	jal	0x7550
    393c:	4e 03 00 ba 	bnez	$r0, 0x3ab0
    3940:	44 00 8d 00 	movi	$r0, #0x8d00
    3944:	49 00 1e 55 	jal	0x75ee
    3948:	f0 11       	lwi37.sp $r0, [ + #0x44]
    394a:	50 00 06 13 	addi	$r0, $r0, #0x613
    394e:	f0 86       	swi37.sp $r0, [ + #0x18]
    3950:	f0 06       	lwi37.sp $r0, [ + #0x18]
    3952:	92 0c       	srli45 $r0, #0xc
    3954:	f0 88       	swi37.sp $r0, [ + #0x20]
    3956:	f0 06       	lwi37.sp $r0, [ + #0x18]
    3958:	54 00 0f ff 	andi	$r0, $r0, #0xfff
    395c:	f0 87       	swi37.sp $r0, [ + #0x1c]
    395e:	f0 07       	lwi37.sp $r0, [ + #0x1c]
    3960:	c0 04       	beqz38 $r0, 0x3968
    3962:	f0 08       	lwi37.sp $r0, [ + #0x20]
    3964:	8c 01       	addi45 $r0, #1
    3966:	f0 88       	swi37.sp $r0, [ + #0x20]
    3968:	f1 08       	lwi37.sp $r1, [ + #0x20]
    396a:	44 01 00 00 	movi	$r0, #0x10000
    396e:	40 10 b0 08 	slli	$r1, $r1, #0xc
    3972:	49 ff fa 4b 	jal	0x2e08
    3976:	84 01       	movi55 $r0, #1
    3978:	f0 85       	swi37.sp $r0, [ + #0x14]
    397a:	d5 3a       	j8 0x39ee
    397c:	f6 05       	lwi37.sp $r6, [ + #0x14]
    397e:	8c d0       	addi45 $r6, #0x10
    3980:	40 63 30 08 	slli	$r6, $r6, #0xc
    3984:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3988:	49 ff fd 3a 	jal	0x33fc
    398c:	c8 07       	bnez38 $r0, 0x399a
    398e:	44 00 8d 10 	movi	$r0, #0x8d10
    3992:	49 00 1e 2e 	jal	0x75ee
    3996:	49 00 17 81 	jal	0x6898
    399a:	80 06       	mov55 $r0, $r6
    399c:	3e 18 10 dc 	addi.gp	$r1, #0x10dc
    39a0:	44 20 10 00 	movi	$r2, #0x1000
    39a4:	49 ff fa b1 	jal	0x2f06
    39a8:	c8 10       	bnez38 $r0, 0x39c8
    39aa:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    39ae:	84 3f       	movi55 $r1, #-1
    39b0:	10 10 00 08 	sbi	$r1, [$r0 + #8]
    39b4:	49 ff fc f5 	jal	0x339e
    39b8:	44 00 8d 2c 	movi	$r0, #0x8d2c
    39bc:	49 00 1e 19 	jal	0x75ee
    39c0:	49 00 17 6c 	jal	0x6898
    39c4:	fa e5       	movpi45 $r7, #0x15
    39c6:	d5 05       	j8 0x39d0
    39c8:	44 00 00 55 	movi	$r0, #0x55
    39cc:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    39d0:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    39d4:	49 ff fc e5 	jal	0x339e
    39d8:	49 00 1c bb 	jal	0x734e
    39dc:	f0 05       	lwi37.sp $r0, [ + #0x14]
    39de:	b6 1f       	swi450 $r0, [$sp]
    39e0:	44 00 8d 48 	movi	$r0, #0x8d48
    39e4:	49 00 1e 00 	jal	0x75e4
    39e8:	f0 05       	lwi37.sp $r0, [ + #0x14]
    39ea:	8c 01       	addi45 $r0, #1
    39ec:	f0 85       	swi37.sp $r0, [ + #0x14]
    39ee:	f0 05       	lwi37.sp $r0, [ + #0x14]
    39f0:	f1 08       	lwi37.sp $r1, [ + #0x20]
    39f2:	e2 01       	slt45 $r0, $r1
    39f4:	e9 c4       	bnezs8 0x397c
    39f6:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    39fa:	44 10 00 55 	movi	$r1, #0x55
    39fe:	10 10 00 08 	sbi	$r1, [$r0 + #8]
    3a02:	49 ff fc fd 	jal	0x33fc
    3a06:	c8 07       	bnez38 $r0, 0x3a14
    3a08:	44 00 8d 10 	movi	$r0, #0x8d10
    3a0c:	49 00 1d f1 	jal	0x75ee
    3a10:	49 00 17 44 	jal	0x6898
    3a14:	84 0f       	movi55 $r0, #0xf
    3a16:	f0 85       	swi37.sp $r0, [ + #0x14]
    3a18:	d5 13       	j8 0x3a3e
    3a1a:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3a1c:	50 00 01 00 	addi	$r0, $r0, #0x100
    3a20:	f1 05       	lwi37.sp $r1, [ + #0x14]
    3a22:	40 10 a0 08 	slli	$r1, $r1, #8
    3a26:	40 00 20 08 	slli	$r0, $r0, #8
    3a2a:	3e 28 10 dc 	addi.gp	$r2, #0x10dc
    3a2e:	88 22       	add45 $r1, $r2
    3a30:	44 20 01 00 	movi	$r2, #0x100
    3a34:	49 ff eb 6e 	jal	0x1110
    3a38:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3a3a:	8e 01       	subi45 $r0, #1
    3a3c:	f0 85       	swi37.sp $r0, [ + #0x14]
    3a3e:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3a40:	84 6a       	movi55 $r3, #0xa
    3a42:	4e 04 ff ec 	bgez	$r0, 0x3a1a
    3a46:	44 01 00 00 	movi	$r0, #0x10000
    3a4a:	3e 18 20 dc 	addi.gp	$r1, #0x20dc
    3a4e:	44 20 10 00 	movi	$r2, #0x1000
    3a52:	49 ff ea 7e 	jal	0xf4e
    3a56:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3a5a:	3e 18 20 dc 	addi.gp	$r1, #0x20dc
    3a5e:	44 20 10 00 	movi	$r2, #0x1000
    3a62:	49 00 1d 77 	jal	0x7550
    3a66:	c0 10       	beqz38 $r0, 0x3a86
    3a68:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3a6c:	84 3f       	movi55 $r1, #-1
    3a6e:	10 10 00 08 	sbi	$r1, [$r0 + #8]
    3a72:	49 ff fc 96 	jal	0x339e
    3a76:	44 00 8d 2c 	movi	$r0, #0x8d2c
    3a7a:	49 00 1d ba 	jal	0x75ee
    3a7e:	49 00 17 0d 	jal	0x6898
    3a82:	fa e5       	movpi45 $r7, #0x15
    3a84:	d5 0d       	j8 0x3a9e
    3a86:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3a8a:	44 10 00 55 	movi	$r1, #0x55
    3a8e:	10 10 00 08 	sbi	$r1, [$r0 + #8]
    3a92:	49 ff fc 86 	jal	0x339e
    3a96:	44 00 8d 54 	movi	$r0, #0x8d54
    3a9a:	49 00 1d aa 	jal	0x75ee
    3a9e:	84 01       	movi55 $r0, #1
    3aa0:	44 11 00 00 	movi	$r1, #0x10000
    3aa4:	49 00 17 05 	jal	0x68ae
    3aa8:	49 00 17 37 	jal	0x6916
    3aac:	48 00 02 ad 	j	0x4006
    3ab0:	3c 6d b3 c7 	lwi.gp	$r6, [ + #-78052]
    3ab4:	80 06       	mov55 $r0, $r6
    3ab6:	49 00 1d c5 	jal	0x7640
    3aba:	80 40       	mov55 $r2, $r0
    3abc:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3ac0:	80 26       	mov55 $r1, $r6
    3ac2:	49 00 1d 47 	jal	0x7550
    3ac6:	4e 03 00 ab 	bnez	$r0, 0x3c1c
    3aca:	44 00 8d 74 	movi	$r0, #0x8d74
    3ace:	49 00 1d 90 	jal	0x75ee
    3ad2:	f0 0f       	lwi37.sp $r0, [ + #0x3c]
    3ad4:	40 10 30 09 	srli	$r1, $r0, #0xc
    3ad8:	f1 88       	swi37.sp $r1, [ + #0x20]
    3ada:	54 00 0f ff 	andi	$r0, $r0, #0xfff
    3ade:	f0 87       	swi37.sp $r0, [ + #0x1c]
    3ae0:	f0 07       	lwi37.sp $r0, [ + #0x1c]
    3ae2:	c0 04       	beqz38 $r0, 0x3aea
    3ae4:	f0 08       	lwi37.sp $r0, [ + #0x20]
    3ae6:	8c 01       	addi45 $r0, #1
    3ae8:	f0 88       	swi37.sp $r0, [ + #0x20]
    3aea:	f1 08       	lwi37.sp $r1, [ + #0x20]
    3aec:	44 01 00 00 	movi	$r0, #0x10000
    3af0:	40 10 b0 08 	slli	$r1, $r1, #0xc
    3af4:	49 ff f9 8a 	jal	0x2e08
    3af8:	84 01       	movi55 $r0, #1
    3afa:	f0 85       	swi37.sp $r0, [ + #0x14]
    3afc:	d5 34       	j8 0x3b64
    3afe:	f6 05       	lwi37.sp $r6, [ + #0x14]
    3b00:	8c d0       	addi45 $r6, #0x10
    3b02:	40 63 30 08 	slli	$r6, $r6, #0xc
    3b06:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3b0a:	49 ff fc 79 	jal	0x33fc
    3b0e:	c8 07       	bnez38 $r0, 0x3b1c
    3b10:	44 00 8d 84 	movi	$r0, #0x8d84
    3b14:	49 00 1d 6d 	jal	0x75ee
    3b18:	49 00 16 c0 	jal	0x6898
    3b1c:	80 06       	mov55 $r0, $r6
    3b1e:	3e 18 10 dc 	addi.gp	$r1, #0x10dc
    3b22:	44 20 10 00 	movi	$r2, #0x1000
    3b26:	49 ff f9 f0 	jal	0x2f06
    3b2a:	c8 0a       	bnez38 $r0, 0x3b3e
    3b2c:	44 00 8d a0 	movi	$r0, #0x8da0
    3b30:	49 00 1d 5f 	jal	0x75ee
    3b34:	84 1f       	movi55 $r0, #-1
    3b36:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    3b3a:	fa e6       	movpi45 $r7, #0x16
    3b3c:	d5 05       	j8 0x3b46
    3b3e:	44 00 00 55 	movi	$r0, #0x55
    3b42:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    3b46:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3b4a:	49 ff fc 2a 	jal	0x339e
    3b4e:	49 00 1c 00 	jal	0x734e
    3b52:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3b54:	b6 1f       	swi450 $r0, [$sp]
    3b56:	44 00 8d bc 	movi	$r0, #0x8dbc
    3b5a:	49 00 1d 45 	jal	0x75e4
    3b5e:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3b60:	8c 01       	addi45 $r0, #1
    3b62:	f0 85       	swi37.sp $r0, [ + #0x14]
    3b64:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3b66:	f1 08       	lwi37.sp $r1, [ + #0x20]
    3b68:	e2 01       	slt45 $r0, $r1
    3b6a:	e9 ca       	bnezs8 0x3afe
    3b6c:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3b70:	44 10 00 55 	movi	$r1, #0x55
    3b74:	10 10 00 08 	sbi	$r1, [$r0 + #8]
    3b78:	49 ff fc 42 	jal	0x33fc
    3b7c:	c8 07       	bnez38 $r0, 0x3b8a
    3b7e:	44 00 8d 84 	movi	$r0, #0x8d84
    3b82:	49 00 1d 36 	jal	0x75ee
    3b86:	49 00 16 89 	jal	0x6898
    3b8a:	84 0f       	movi55 $r0, #0xf
    3b8c:	f0 85       	swi37.sp $r0, [ + #0x14]
    3b8e:	d5 13       	j8 0x3bb4
    3b90:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3b92:	50 00 01 00 	addi	$r0, $r0, #0x100
    3b96:	f1 05       	lwi37.sp $r1, [ + #0x14]
    3b98:	40 10 a0 08 	slli	$r1, $r1, #8
    3b9c:	40 00 20 08 	slli	$r0, $r0, #8
    3ba0:	3e 28 10 dc 	addi.gp	$r2, #0x10dc
    3ba4:	88 22       	add45 $r1, $r2
    3ba6:	44 20 01 00 	movi	$r2, #0x100
    3baa:	49 ff ea b3 	jal	0x1110
    3bae:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3bb0:	8e 01       	subi45 $r0, #1
    3bb2:	f0 85       	swi37.sp $r0, [ + #0x14]
    3bb4:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3bb6:	84 6a       	movi55 $r3, #0xa
    3bb8:	4e 04 ff ec 	bgez	$r0, 0x3b90
    3bbc:	44 01 00 00 	movi	$r0, #0x10000
    3bc0:	3e 18 20 dc 	addi.gp	$r1, #0x20dc
    3bc4:	44 20 10 00 	movi	$r2, #0x1000
    3bc8:	49 ff e9 c3 	jal	0xf4e
    3bcc:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3bd0:	3e 18 20 dc 	addi.gp	$r1, #0x20dc
    3bd4:	44 20 10 00 	movi	$r2, #0x1000
    3bd8:	49 00 1c bc 	jal	0x7550
    3bdc:	c0 10       	beqz38 $r0, 0x3bfc
    3bde:	44 00 8d c8 	movi	$r0, #0x8dc8
    3be2:	49 00 1d 06 	jal	0x75ee
    3be6:	44 01 00 00 	movi	$r0, #0x10000
    3bea:	44 10 10 00 	movi	$r1, #0x1000
    3bee:	49 ff f9 0d 	jal	0x2e08
    3bf2:	84 1f       	movi55 $r0, #-1
    3bf4:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    3bf8:	fa e6       	movpi45 $r7, #0x16
    3bfa:	d5 09       	j8 0x3c0c
    3bfc:	44 00 00 55 	movi	$r0, #0x55
    3c00:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    3c04:	44 00 8d e4 	movi	$r0, #0x8de4
    3c08:	49 00 1c f3 	jal	0x75ee
    3c0c:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3c10:	49 ff fb c7 	jal	0x339e
    3c14:	4e 72 01 f9 	beqz	$r7, 0x4006
    3c18:	48 00 01 ff 	j	0x4016
    3c1c:	3c 6d b3 c6 	lwi.gp	$r6, [ + #-78056]
    3c20:	80 06       	mov55 $r0, $r6
    3c22:	49 00 1d 0f 	jal	0x7640
    3c26:	80 40       	mov55 $r2, $r0
    3c28:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3c2c:	80 26       	mov55 $r1, $r6
    3c2e:	49 00 1c 91 	jal	0x7550
    3c32:	4e 03 00 88 	bnez	$r0, 0x3d42
    3c36:	44 00 8d f8 	movi	$r0, #0x8df8
    3c3a:	49 00 1c da 	jal	0x75ee
    3c3e:	f0 0d       	lwi37.sp $r0, [ + #0x34]
    3c40:	40 10 30 09 	srli	$r1, $r0, #0xc
    3c44:	f1 88       	swi37.sp $r1, [ + #0x20]
    3c46:	54 00 0f ff 	andi	$r0, $r0, #0xfff
    3c4a:	f0 87       	swi37.sp $r0, [ + #0x1c]
    3c4c:	f0 07       	lwi37.sp $r0, [ + #0x1c]
    3c4e:	c0 04       	beqz38 $r0, 0x3c56
    3c50:	f0 08       	lwi37.sp $r0, [ + #0x20]
    3c52:	8c 01       	addi45 $r0, #1
    3c54:	f0 88       	swi37.sp $r0, [ + #0x20]
    3c56:	f1 08       	lwi37.sp $r1, [ + #0x20]
    3c58:	f0 0c       	lwi37.sp $r0, [ + #0x30]
    3c5a:	40 10 b0 08 	slli	$r1, $r1, #0xc
    3c5e:	49 ff f8 d5 	jal	0x2e08
    3c62:	84 01       	movi55 $r0, #1
    3c64:	f0 85       	swi37.sp $r0, [ + #0x14]
    3c66:	d5 38       	j8 0x3cd6
    3c68:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3c6a:	40 00 30 08 	slli	$r0, $r0, #0xc
    3c6e:	f1 0c       	lwi37.sp $r1, [ + #0x30]
    3c70:	99 81       	add333 $r6, $r0, $r1
    3c72:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3c76:	49 ff fb c3 	jal	0x33fc
    3c7a:	c8 0a       	bnez38 $r0, 0x3c8e
    3c7c:	44 00 8e 0c 	movi	$r0, #0x8e0c
    3c80:	49 00 1c b7 	jal	0x75ee
    3c84:	84 1f       	movi55 $r0, #-1
    3c86:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    3c8a:	fa e7       	movpi45 $r7, #0x17
    3c8c:	d5 29       	j8 0x3cde
    3c8e:	80 06       	mov55 $r0, $r6
    3c90:	3e 18 10 dc 	addi.gp	$r1, #0x10dc
    3c94:	44 20 10 00 	movi	$r2, #0x1000
    3c98:	49 ff f9 37 	jal	0x2f06
    3c9c:	c8 0a       	bnez38 $r0, 0x3cb0
    3c9e:	44 00 8e 28 	movi	$r0, #0x8e28
    3ca2:	49 00 1c a6 	jal	0x75ee
    3ca6:	84 1f       	movi55 $r0, #-1
    3ca8:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    3cac:	fa e7       	movpi45 $r7, #0x17
    3cae:	d5 18       	j8 0x3cde
    3cb0:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3cb4:	44 10 00 55 	movi	$r1, #0x55
    3cb8:	10 10 00 08 	sbi	$r1, [$r0 + #8]
    3cbc:	49 ff fb 71 	jal	0x339e
    3cc0:	49 00 1b 47 	jal	0x734e
    3cc4:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3cc6:	b6 1f       	swi450 $r0, [$sp]
    3cc8:	44 00 8e 44 	movi	$r0, #0x8e44
    3ccc:	49 00 1c 8c 	jal	0x75e4
    3cd0:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3cd2:	8c 01       	addi45 $r0, #1
    3cd4:	f0 85       	swi37.sp $r0, [ + #0x14]
    3cd6:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3cd8:	f1 08       	lwi37.sp $r1, [ + #0x20]
    3cda:	e2 01       	slt45 $r0, $r1
    3cdc:	e9 c6       	bnezs8 0x3c68
    3cde:	2e 00 10 e4 	lbi.gp	$r0, [ + #0x10e4]
    3ce2:	5a 08 55 2c 	bnec	$r0, #0x55, 0x3d3a
    3ce6:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3cea:	49 ff fb 89 	jal	0x33fc
    3cee:	c8 09       	bnez38 $r0, 0x3d00
    3cf0:	44 00 8e 0c 	movi	$r0, #0x8e0c
    3cf4:	49 00 1c 7d 	jal	0x75ee
    3cf8:	84 1f       	movi55 $r0, #-1
    3cfa:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    3cfe:	fa e7       	movpi45 $r7, #0x17
    3d00:	f0 0c       	lwi37.sp $r0, [ + #0x30]
    3d02:	3e 18 10 dc 	addi.gp	$r1, #0x10dc
    3d06:	44 20 10 00 	movi	$r2, #0x1000
    3d0a:	49 ff f8 fe 	jal	0x2f06
    3d0e:	c8 0a       	bnez38 $r0, 0x3d22
    3d10:	44 00 8e 28 	movi	$r0, #0x8e28
    3d14:	49 00 1c 6d 	jal	0x75ee
    3d18:	84 1f       	movi55 $r0, #-1
    3d1a:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    3d1e:	fa e7       	movpi45 $r7, #0x17
    3d20:	d5 09       	j8 0x3d32
    3d22:	44 00 00 55 	movi	$r0, #0x55
    3d26:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    3d2a:	44 00 8e 54 	movi	$r0, #0x8e54
    3d2e:	49 00 1c 60 	jal	0x75ee
    3d32:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3d36:	49 ff fb 34 	jal	0x339e
    3d3a:	4e 72 01 66 	beqz	$r7, 0x4006
    3d3e:	48 00 01 6c 	j	0x4016
    3d42:	3c 6d b3 c5 	lwi.gp	$r6, [ + #-78060]
    3d46:	80 06       	mov55 $r0, $r6
    3d48:	49 00 1c 7c 	jal	0x7640
    3d4c:	80 40       	mov55 $r2, $r0
    3d4e:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3d52:	80 26       	mov55 $r1, $r6
    3d54:	49 00 1b fe 	jal	0x7550
    3d58:	4e 03 00 88 	bnez	$r0, 0x3e68
    3d5c:	44 00 8e 6c 	movi	$r0, #0x8e6c
    3d60:	49 00 1c 47 	jal	0x75ee
    3d64:	f0 0b       	lwi37.sp $r0, [ + #0x2c]
    3d66:	40 10 30 09 	srli	$r1, $r0, #0xc
    3d6a:	f1 88       	swi37.sp $r1, [ + #0x20]
    3d6c:	54 00 0f ff 	andi	$r0, $r0, #0xfff
    3d70:	f0 87       	swi37.sp $r0, [ + #0x1c]
    3d72:	f0 07       	lwi37.sp $r0, [ + #0x1c]
    3d74:	c0 04       	beqz38 $r0, 0x3d7c
    3d76:	f0 08       	lwi37.sp $r0, [ + #0x20]
    3d78:	8c 01       	addi45 $r0, #1
    3d7a:	f0 88       	swi37.sp $r0, [ + #0x20]
    3d7c:	f1 08       	lwi37.sp $r1, [ + #0x20]
    3d7e:	f0 0a       	lwi37.sp $r0, [ + #0x28]
    3d80:	40 10 b0 08 	slli	$r1, $r1, #0xc
    3d84:	49 ff f8 42 	jal	0x2e08
    3d88:	84 01       	movi55 $r0, #1
    3d8a:	f0 85       	swi37.sp $r0, [ + #0x14]
    3d8c:	d5 38       	j8 0x3dfc
    3d8e:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3d90:	40 00 30 08 	slli	$r0, $r0, #0xc
    3d94:	f1 0a       	lwi37.sp $r1, [ + #0x28]
    3d96:	99 81       	add333 $r6, $r0, $r1
    3d98:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3d9c:	49 ff fb 30 	jal	0x33fc
    3da0:	c8 0a       	bnez38 $r0, 0x3db4
    3da2:	44 00 8e 80 	movi	$r0, #0x8e80
    3da6:	49 00 1c 24 	jal	0x75ee
    3daa:	84 1f       	movi55 $r0, #-1
    3dac:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    3db0:	fa e8       	movpi45 $r7, #0x18
    3db2:	d5 29       	j8 0x3e04
    3db4:	80 06       	mov55 $r0, $r6
    3db6:	3e 18 10 dc 	addi.gp	$r1, #0x10dc
    3dba:	44 20 10 00 	movi	$r2, #0x1000
    3dbe:	49 ff f8 a4 	jal	0x2f06
    3dc2:	c8 0a       	bnez38 $r0, 0x3dd6
    3dc4:	44 00 8e 9c 	movi	$r0, #0x8e9c
    3dc8:	49 00 1c 13 	jal	0x75ee
    3dcc:	84 1f       	movi55 $r0, #-1
    3dce:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    3dd2:	fa e8       	movpi45 $r7, #0x18
    3dd4:	d5 18       	j8 0x3e04
    3dd6:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3dda:	44 10 00 55 	movi	$r1, #0x55
    3dde:	10 10 00 08 	sbi	$r1, [$r0 + #8]
    3de2:	49 ff fa de 	jal	0x339e
    3de6:	49 00 1a b4 	jal	0x734e
    3dea:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3dec:	b6 1f       	swi450 $r0, [$sp]
    3dee:	44 00 8e b8 	movi	$r0, #0x8eb8
    3df2:	49 00 1b f9 	jal	0x75e4
    3df6:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3df8:	8c 01       	addi45 $r0, #1
    3dfa:	f0 85       	swi37.sp $r0, [ + #0x14]
    3dfc:	f0 05       	lwi37.sp $r0, [ + #0x14]
    3dfe:	f1 08       	lwi37.sp $r1, [ + #0x20]
    3e00:	e2 01       	slt45 $r0, $r1
    3e02:	e9 c6       	bnezs8 0x3d8e
    3e04:	2e 00 10 e4 	lbi.gp	$r0, [ + #0x10e4]
    3e08:	5a 08 55 2c 	bnec	$r0, #0x55, 0x3e60
    3e0c:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3e10:	49 ff fa f6 	jal	0x33fc
    3e14:	c8 09       	bnez38 $r0, 0x3e26
    3e16:	44 00 8e 80 	movi	$r0, #0x8e80
    3e1a:	49 00 1b ea 	jal	0x75ee
    3e1e:	84 1f       	movi55 $r0, #-1
    3e20:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    3e24:	fa e8       	movpi45 $r7, #0x18
    3e26:	f0 0a       	lwi37.sp $r0, [ + #0x28]
    3e28:	3e 18 10 dc 	addi.gp	$r1, #0x10dc
    3e2c:	44 20 10 00 	movi	$r2, #0x1000
    3e30:	49 ff f8 6b 	jal	0x2f06
    3e34:	c8 0a       	bnez38 $r0, 0x3e48
    3e36:	44 00 8e 9c 	movi	$r0, #0x8e9c
    3e3a:	49 00 1b da 	jal	0x75ee
    3e3e:	84 1f       	movi55 $r0, #-1
    3e40:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    3e44:	fa e8       	movpi45 $r7, #0x18
    3e46:	d5 09       	j8 0x3e58
    3e48:	44 00 00 55 	movi	$r0, #0x55
    3e4c:	3e 00 10 e4 	sbi.gp	$r0, [ + #0x10e4]
    3e50:	44 00 8e c8 	movi	$r0, #0x8ec8
    3e54:	49 00 1b cd 	jal	0x75ee
    3e58:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3e5c:	49 ff fa a1 	jal	0x339e
    3e60:	4e 72 00 d3 	beqz	$r7, 0x4006
    3e64:	48 00 00 d9 	j	0x4016
    3e68:	3c 6d b3 c4 	lwi.gp	$r6, [ + #-78064]
    3e6c:	80 06       	mov55 $r0, $r6
    3e6e:	49 00 1b e9 	jal	0x7640
    3e72:	80 40       	mov55 $r2, $r0
    3e74:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3e78:	80 26       	mov55 $r1, $r6
    3e7a:	49 00 1b 6b 	jal	0x7550
    3e7e:	4e 03 00 9d 	bnez	$r0, 0x3fb8
    3e82:	3e 69 30 f4 	addi.gp	$r6, #0x130f4
    3e86:	00 13 00 08 	lbi	$r1, [$r6 + #8]
    3e8a:	00 03 00 09 	lbi	$r0, [$r6 + #9]
    3e8e:	40 00 20 08 	slli	$r0, $r0, #8
    3e92:	fe 0f       	or33 $r0, $r1
    3e94:	00 13 00 0a 	lbi	$r1, [$r6 + #0xa]
    3e98:	40 10 c0 08 	slli	$r1, $r1, #0x10
    3e9c:	fe 0f       	or33 $r0, $r1
    3e9e:	00 13 00 0b 	lbi	$r1, [$r6 + #0xb]
    3ea2:	40 10 e0 08 	slli	$r1, $r1, #0x18
    3ea6:	fe 0f       	or33 $r0, $r1
    3ea8:	50 00 70 00 	addi	$r0, $r0, #-4096
    3eac:	3e 18 20 dc 	addi.gp	$r1, #0x20dc
    3eb0:	44 20 10 00 	movi	$r2, #0x1000
    3eb4:	84 6a       	movi55 $r3, #0xa
    3eb6:	49 ff e8 4c 	jal	0xf4e
    3eba:	00 13 00 08 	lbi	$r1, [$r6 + #8]
    3ebe:	00 03 00 09 	lbi	$r0, [$r6 + #9]
    3ec2:	40 00 20 08 	slli	$r0, $r0, #8
    3ec6:	fe 0f       	or33 $r0, $r1
    3ec8:	00 13 00 0a 	lbi	$r1, [$r6 + #0xa]
    3ecc:	40 10 c0 08 	slli	$r1, $r1, #0x10
    3ed0:	fe 0f       	or33 $r0, $r1
    3ed2:	00 13 00 0b 	lbi	$r1, [$r6 + #0xb]
    3ed6:	40 10 e0 08 	slli	$r1, $r1, #0x18
    3eda:	fe 0f       	or33 $r0, $r1
    3edc:	50 00 70 00 	addi	$r0, $r0, #-4096
    3ee0:	44 10 10 00 	movi	$r1, #0x1000
    3ee4:	49 ff f7 92 	jal	0x2e08
    3ee8:	3e 88 20 dc 	addi.gp	$r8, #0x20dc
    3eec:	3e 98 10 e4 	addi.gp	$r9, #0x10e4
    3ef0:	80 29       	mov55 $r1, $r9
    3ef2:	3b 00 e4 04 	lmw.bim $r16, [$r1], $r25, #0x0    ! {$r16~$r25}
    3ef6:	80 08       	mov55 $r0, $r8
    3ef8:	3b 00 64 24 	smw.bim $r16, [$r0], $r25, #0x0    ! {$r16~$r25}
    3efc:	3b 00 e4 04 	lmw.bim $r16, [$r1], $r25, #0x0    ! {$r16~$r25}
    3f00:	3b 00 64 24 	smw.bim $r16, [$r0], $r25, #0x0    ! {$r16~$r25}
    3f04:	3b 00 c8 00 	lmw.bi $r16, [$r1], $r18, #0x0    ! {$r16~$r18}
    3f08:	3b 00 48 20 	smw.bi $r16, [$r0], $r18, #0x0    ! {$r16~$r18}
    3f0c:	00 13 00 08 	lbi	$r1, [$r6 + #8]
    3f10:	00 03 00 09 	lbi	$r0, [$r6 + #9]
    3f14:	40 00 20 08 	slli	$r0, $r0, #8
    3f18:	fe 0f       	or33 $r0, $r1
    3f1a:	00 13 00 0a 	lbi	$r1, [$r6 + #0xa]
    3f1e:	40 10 c0 08 	slli	$r1, $r1, #0x10
    3f22:	fe 0f       	or33 $r0, $r1
    3f24:	00 13 00 0b 	lbi	$r1, [$r6 + #0xb]
    3f28:	40 10 e0 08 	slli	$r1, $r1, #0x18
    3f2c:	fe 0f       	or33 $r0, $r1
    3f2e:	50 00 70 00 	addi	$r0, $r0, #-4096
    3f32:	80 28       	mov55 $r1, $r8
    3f34:	44 20 10 00 	movi	$r2, #0x1000
    3f38:	84 6a       	movi55 $r3, #0xa
    3f3a:	49 ff e8 eb 	jal	0x1110
    3f3e:	80 08       	mov55 $r0, $r8
    3f40:	84 20       	movi55 $r1, #0
    3f42:	44 20 10 00 	movi	$r2, #0x1000
    3f46:	49 00 1b 37 	jal	0x75b4
    3f4a:	00 13 00 08 	lbi	$r1, [$r6 + #8]
    3f4e:	00 03 00 09 	lbi	$r0, [$r6 + #9]
    3f52:	40 00 20 08 	slli	$r0, $r0, #8
    3f56:	fe 0f       	or33 $r0, $r1
    3f58:	00 13 00 0a 	lbi	$r1, [$r6 + #0xa]
    3f5c:	40 10 c0 08 	slli	$r1, $r1, #0x10
    3f60:	fe 0f       	or33 $r0, $r1
    3f62:	00 13 00 0b 	lbi	$r1, [$r6 + #0xb]
    3f66:	40 10 e0 08 	slli	$r1, $r1, #0x18
    3f6a:	fe 0f       	or33 $r0, $r1
    3f6c:	50 00 70 00 	addi	$r0, $r0, #-4096
    3f70:	80 28       	mov55 $r1, $r8
    3f72:	44 20 10 00 	movi	$r2, #0x1000
    3f76:	84 6a       	movi55 $r3, #0xa
    3f78:	49 ff e7 eb 	jal	0xf4e
    3f7c:	80 09       	mov55 $r0, $r9
    3f7e:	80 28       	mov55 $r1, $r8
    3f80:	44 20 00 5c 	movi	$r2, #0x5c
    3f84:	49 00 1a e6 	jal	0x7550
    3f88:	c8 0f       	bnez38 $r0, 0x3fa6
    3f8a:	3e 68 10 dc 	addi.gp	$r6, #0x10dc
    3f8e:	44 00 00 55 	movi	$r0, #0x55
    3f92:	10 03 00 08 	sbi	$r0, [$r6 + #8]
    3f96:	44 00 8e e0 	movi	$r0, #0x8ee0
    3f9a:	49 00 1b 2a 	jal	0x75ee
    3f9e:	80 06       	mov55 $r0, $r6
    3fa0:	49 ff f9 ff 	jal	0x339e
    3fa4:	d5 31       	j8 0x4006
    3fa6:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3faa:	84 3f       	movi55 $r1, #-1
    3fac:	10 10 00 08 	sbi	$r1, [$r0 + #8]
    3fb0:	49 ff f9 f7 	jal	0x339e
    3fb4:	fa ea       	movpi45 $r7, #0x1a
    3fb6:	d5 30       	j8 0x4016
    3fb8:	3c 6d b3 c3 	lwi.gp	$r6, [ + #-78068]
    3fbc:	80 06       	mov55 $r0, $r6
    3fbe:	49 00 1b 41 	jal	0x7640
    3fc2:	80 40       	mov55 $r2, $r0
    3fc4:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    3fc8:	80 26       	mov55 $r1, $r6
    3fca:	49 00 1a c3 	jal	0x7550
    3fce:	c8 1c       	bnez38 $r0, 0x4006
    3fd0:	2e 00 10 e4 	lbi.gp	$r0, [ + #0x10e4]
    3fd4:	5a 08 6f 12 	bnec	$r0, #0x6f, 0x3ff8
    3fd8:	2e 00 10 e5 	lbi.gp	$r0, [ + #0x10e5]
    3fdc:	5a 08 6b 0e 	bnec	$r0, #0x6b, 0x3ff8
    3fe0:	44 00 8e f4 	movi	$r0, #0x8ef4
    3fe4:	49 00 1b 05 	jal	0x75ee
    3fe8:	49 00 11 84 	jal	0x62f0
    3fec:	49 00 11 bb 	jal	0x6362
    3ff0:	49 00 11 93 	jal	0x6316
    3ff4:	84 01       	movi55 $r0, #1
    3ff6:	d5 11       	j8 0x4018
    3ff8:	b6 ff       	swi450 $r7, [$sp]
    3ffa:	44 00 8f 04 	movi	$r0, #0x8f04
    3ffe:	49 00 1a f3 	jal	0x75e4
    4002:	49 00 14 4b 	jal	0x6898
    4006:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    400a:	84 20       	movi55 $r1, #0
    400c:	84 48       	movi55 $r2, #8
    400e:	49 00 1a d3 	jal	0x75b4
    4012:	48 ff fa 2a 	j	0x3466
    4016:	80 07       	mov55 $r0, $r7
    4018:	ec 4c       	addi10.sp #0x4c
    401a:	3a 6f a4 84 	lmw.bim $r6, [$sp], $r9, #0x2    ! {$r6~$r9, $lp}
