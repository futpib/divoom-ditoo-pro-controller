# Pinned stock 306007 command and USB bridge hook boundaries.

# 0x7a270..0x7a2ac
   7a270:	3b ff fc bc 	smw.adm $sp, [$sp], $sp, #0x2    ! {$lp}
   7a274:	ef fc       	addi10.sp #-4
   7a276:	3e 08 4a 74 	addi.gp	$r0, #0x4a74
   7a27a:	44 10 01 00 	movi	$r1, #0x100
   7a27e:	49 fd 87 e1 	jal	0x2b240
   7a282:	ec 04       	addi10.sp #4
   7a284:	3b ff fc 84 	lmw.bim $sp, [$sp], $sp, #0x2    ! {$lp}
   7a288:	dd 9e       	ret5 $lp
   7a28a:	92 00       	nop16
   7a28c:	3b ff fc bc 	smw.adm $sp, [$sp], $sp, #0x2    ! {$lp}
   7a290:	ef fc       	addi10.sp #-4
   7a292:	3e 08 4b 74 	addi.gp	$r0, #0x4b74
   7a296:	44 10 01 00 	movi	$r1, #0x100
   7a29a:	84 46       	movi55 $r2, #6
   7a29c:	49 00 8a 1d 	jal	0x8b6d6
   7a2a0:	ec 04       	addi10.sp #4
   7a2a2:	3b ff fc 84 	lmw.bim $sp, [$sp], $sp, #0x2    ! {$lp}
   7a2a6:	dd 9e       	ret5 $lp
   7a2a8:	3b ff fc bc 	smw.adm $sp, [$sp], $sp, #0x2    ! {$lp}

# 0x38f00..0x38f30
   38f00:	3a 6f b0 bc 	smw.adm $r6, [$sp], $r12, #0x2    ! {$r6~$r12, $lp}
   38f04:	3c 6d de bd 	lwi.gp	$r6, [ + #-34060]
   38f08:	c6 0c       	beqz38 $r6, 0x38f20
   38f0a:	81 20       	mov55 $r9, $r0
   38f0c:	81 61       	mov55 $r11, $r1
   38f0e:	81 42       	mov55 $r10, $r2
   38f10:	81 03       	mov55 $r8, $r3
   38f12:	80 e4       	mov55 $r7, $r4
   38f14:	81 85       	mov55 $r12, $r5
   38f16:	a6 37       	lbi333 $r0, [$r6 + #7]
   38f18:	c0 04       	beqz38 $r0, 0x38f20
   38f1a:	a6 32       	lbi333 $r0, [$r6 + #2]
   38f1c:	e6 02       	slti45 $r0, #2
   38f1e:	e9 09       	bnezs8 0x38f30
   38f20:	46 00 01 5e 	sethi	$r0, #0x15e
   38f24:	58 00 06 00 	ori	$r0, $r0, #0x600
   38f28:	49 08 e7 34 	jal	0x155d90
   38f2c:	84 00       	movi55 $r0, #0
   38f2e:	d5 73       	j8 0x39014

# 0x398c8..0x39998
   398c8:	3a 6f a4 bc 	smw.adm $r6, [$sp], $r9, #0x2    ! {$r6~$r9, $lp}
   398cc:	ef 24       	addi10.sp #-220
   398ce:	80 c1       	mov55 $r6, $r1
   398d0:	b0 32       	addri36.sp $r0, #0xc8
   398d2:	84 20       	movi55 $r1, #0
   398d4:	44 20 00 0c 	movi	$r2, #0xc
   398d8:	49 08 e1 b2 	jal	0x155c3c
   398dc:	3c 0d de bd 	lwi.gp	$r0, [ + #-34060]
   398e0:	4e 02 0d 15 	beqz	$r0, 0x3b30a
   398e4:	49 00 6f 12 	jal	0x47708
   398e8:	3c 8d de bd 	lwi.gp	$r8, [ + #-34060]
   398ec:	84 0f       	movi55 $r0, #0xf
   398ee:	10 0f 80 cc 	sbi	$r0, [$sp + #0xcc]
   398f2:	00 04 00 07 	lbi	$r0, [$r8 + #7]
   398f6:	c8 11       	bnez38 $r0, 0x39918
   398f8:	49 01 ef 06 	jal	0x77704
   398fc:	10 0f 80 d7 	sbi	$r0, [$sp + #0xd7]
   39900:	84 00       	movi55 $r0, #0
   39902:	fa 23       	movpi45 $r1, #0x13
   39904:	50 2f 80 d7 	addi	$r2, $sp, #0xd7
   39908:	84 61       	movi55 $r3, #1
   3990a:	49 ff fb 89 	jal	0x3901c
   3990e:	84 21       	movi55 $r1, #1
   39910:	3c 0d de bd 	lwi.gp	$r0, [ + #-34060]
   39914:	10 10 00 07 	sbi	$r1, [$r0 + #7]
   39918:	00 04 00 02 	lbi	$r0, [$r8 + #2]
   3991c:	5a 08 01 06 	bnec	$r0, #1, 0x39928
   39920:	10 04 00 00 	sbi	$r0, [$r8 + #0]
   39924:	48 00 00 34 	j	0x3998c
   39928:	c8 32       	bnez38 $r0, 0x3998c
   3992a:	a0 32       	lwi333 $r0, [$r6 + #8]
   3992c:	a6 40       	lbi333 $r1, [$r0 + #0]
   3992e:	c1 2a       	beqz38 $r1, 0x39982
   39930:	8c 01       	addi45 $r0, #1
   39932:	49 01 84 9b 	jal	0x6a268
   39936:	80 e0       	mov55 $r7, $r0
   39938:	a0 b2       	lwi333 $r2, [$r6 + #8]
   3993a:	04 04 00 59 	lwi	$r0, [$r8 + #0x164]
   3993e:	44 10 00 33 	movi	$r1, #0x33
   39942:	8c 41       	addi45 $r2, #1
   39944:	44 30 00 04 	movi	$r3, #4
   39948:	49 ff fb 6a 	jal	0x3901c
   3994c:	04 54 00 55 	lwi	$r5, [$r8 + #0x154]
   39950:	df 10       	bnes38 $r7, 0x39970
   39952:	a0 32       	lwi333 $r0, [$r6 + #8]
   39954:	a6 05       	lbi333 $r0, [$r0 + #5]
   39956:	b6 1f       	swi450 $r0, [$sp]
   39958:	a4 31       	lhi333 $r0, [$r6 + #2]
   3995a:	f0 81       	swi37.sp $r0, [ + #4]
   3995c:	46 00 01 5e 	sethi	$r0, #0x15e
   39960:	58 00 07 74 	ori	$r0, $r0, #0x774
   39964:	49 08 e2 11 	jal	0x155d86
   39968:	a0 32       	lwi333 $r0, [$r6 + #8]
   3996a:	84 3f       	movi55 $r1, #-1
   3996c:	ae 45       	sbi333 $r1, [$r0 + #5]
   3996e:	d5 03       	j8 0x39974
   39970:	14 74 00 55 	swi	$r7, [$r8 + #0x154]
   39974:	a0 32       	lwi333 $r0, [$r6 + #8]
   39976:	8c 05       	addi45 $r0, #5
   39978:	a8 32       	swi333 $r0, [$r6 + #8]
   3997a:	a4 31       	lhi333 $r0, [$r6 + #2]
   3997c:	8e 05       	subi45 $r0, #5
   3997e:	ac 31       	shi333 $r0, [$r6 + #2]
   39980:	d5 06       	j8 0x3998c
   39982:	8c 01       	addi45 $r0, #1
   39984:	a8 32       	swi333 $r0, [$r6 + #8]
   39986:	a4 31       	lhi333 $r0, [$r6 + #2]
   39988:	8e 01       	subi45 $r0, #1
   3998a:	ac 31       	shi333 $r0, [$r6 + #2]
   3998c:	a1 f2       	lwi333 $r7, [$r6 + #8]
   3998e:	a6 38       	lbi333 $r0, [$r7 + #0]
   39990:	5a 08 6c 04 	bnec	$r0, #0x6c, 0x39998
   39994:	48 00 07 b0 	j	0x3a8f4

# 0x3b30a..0x3b312
   3b30a:	ec dc       	addi10.sp #0xdc
   3b30c:	3a 6f a4 84 	lmw.bim $r6, [$sp], $r9, #0x2    ! {$r6~$r9, $lp}
   3b310:	dd 9e       	ret5 $lp

# 0x3879c..0x387e8
   3879c:	04 00 80 08 	lwi	$r0, [$r1 + #0x20]
   387a0:	c8 20       	bnez38 $r0, 0x387e0
   387a2:	a1 8b       	lwi333 $r6, [$r1 + #0xc]
   387a4:	50 63 27 10 	addi	$r6, $r6, #0x2710
   387a8:	49 01 f6 b2 	jal	0x7750c
   387ac:	40 03 00 06 	slt	$r0, $r6, $r0
   387b0:	c0 18       	beqz38 $r0, 0x387e0
   387b2:	49 00 04 4b 	jal	0x39048
   387b6:	c0 15       	beqz38 $r0, 0x387e0
   387b8:	49 00 a7 4a 	jal	0x4d64c
   387bc:	5a 00 08 12 	beqc	$r0, #8, 0x387e0
   387c0:	5a 00 0b 10 	beqc	$r0, #0xb, 0x387e0
   387c4:	3c 6d de b9 	lwi.gp	$r6, [ + #-34076]
   387c8:	49 01 f6 a2 	jal	0x7750c
   387cc:	a8 33       	swi333 $r0, [$r6 + #0xc]
   387ce:	84 00       	movi55 $r0, #0
   387d0:	49 00 86 d6 	jal	0x4957c
   387d4:	d5 06       	j8 0x387e0
   387d6:	a0 0f       	lwi333 $r0, [$r1 + #0x1c]
   387d8:	c0 e5       	beqz38 $r0, 0x387a2
   387da:	d5 b9       	j8 0x3874c
   387dc:	a0 0f       	lwi333 $r0, [$r1 + #0x1c]
   387de:	c8 b7       	bnez38 $r0, 0x3874c
   387e0:	51 ff 80 10 	addi	$sp, $sp, #0x10
   387e4:	3a 6f 98 84 	lmw.bim $r6, [$sp], $r6, #0x2    ! {$r6, $lp}

# 0x7e4d4..0x7e536
   7e4d4:	2e 00 52 28 	lbi.gp	$r0, [ + #0x5228]
   7e4d8:	c8 2e       	bnez38 $r0, 0x7e534
   7e4da:	3b ff fc bc 	smw.adm $sp, [$sp], $sp, #0x2    ! {$lp}
   7e4de:	ef fc       	addi10.sp #-4
   7e4e0:	49 ff ea f0 	jal	0x7bac0
   7e4e4:	5c 00 00 02 	slti	$r0, $r0, #2
   7e4e8:	4e 03 00 23 	bnez	$r0, 0x7e52e
   7e4ec:	49 ff f2 34 	jal	0x7c954
   7e4f0:	5c 00 00 02 	slti	$r0, $r0, #2
   7e4f4:	c8 1d       	bnez38 $r0, 0x7e52e
   7e4f6:	84 04       	movi55 $r0, #4
   7e4f8:	84 20       	movi55 $r1, #0
   7e4fa:	3c 2d e2 4f 	lwi.gp	$r2, [ + #-30404]
   7e4fe:	dd 22       	jral5 $r2
   7e500:	3e 08 51 04 	addi.gp	$r0, #0x5104
   7e504:	84 21       	movi55 $r1, #1
   7e506:	10 10 01 24 	sbi	$r1, [$r0 + #0x124]
   7e50a:	84 20       	movi55 $r1, #0
   7e50c:	3c 1f dd 5f 	swi.gp	$r1, [ + #-35460]
   7e510:	a6 00       	lbi333 $r0, [$r0 + #0]
   7e512:	c0 0e       	beqz38 $r0, 0x7e52e
   7e514:	3e 08 51 04 	addi.gp	$r0, #0x5104
   7e518:	ae 40       	sbi333 $r1, [$r0 + #0]
   7e51a:	00 00 01 25 	lbi	$r0, [$r0 + #0x125]
   7e51e:	c8 08       	bnez38 $r0, 0x7e52e
   7e520:	49 ff f3 b2 	jal	0x7cc84
   7e524:	80 20       	mov55 $r1, $r0
   7e526:	44 00 01 f9 	movi	$r0, #0x1f9
   7e52a:	49 ff f8 23 	jal	0x7d570
   7e52e:	ec 04       	addi10.sp #4
   7e530:	3b ff fc 84 	lmw.bim $sp, [$sp], $sp, #0x2    ! {$lp}
   7e534:	dd 9e       	ret5 $lp
