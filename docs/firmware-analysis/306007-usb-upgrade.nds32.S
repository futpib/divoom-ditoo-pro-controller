
firmware/decoded/306007/code.bin:     file format binary


Disassembly of section .data:

0007a270 <.data+0x7a270>:
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
   7a2ac:	51 ff ff fc 	addi	$sp, $sp, #-4
   7a2b0:	2e 27 75 68 	lbi.gp	$r2, [ + #-35480]
   7a2b4:	5a 28 22 0a 	bnec	$r2, #0x22, 0x7a2c8
   7a2b8:	2e 07 75 69 	lbi.gp	$r0, [ + #-35479]
   7a2bc:	5a 08 01 06 	bnec	$r0, #1, 0x7a2c8
   7a2c0:	49 fd 29 b4 	jal	0x1f628
   7a2c4:	48 00 00 9e 	j	0x7a400
   7a2c8:	2e 07 75 6c 	lbi.gp	$r0, [ + #-35476]
   7a2cc:	3c 1d dd 5e 	lwi.gp	$r1, [ + #-35464]
   7a2d0:	a7 48       	lbi333 $r5, [$r1 + #0]
   7a2d2:	d8 07       	bnes38 $r0, 0x7a2e0
   7a2d4:	84 00       	movi55 $r0, #0
   7a2d6:	84 21       	movi55 $r1, #1
   7a2d8:	49 ff fe 16 	jal	0x79f04
   7a2dc:	48 00 00 92 	j	0x7a400
   7a2e0:	a7 49       	lbi333 $r5, [$r1 + #1]
   7a2e2:	d8 05       	bnes38 $r0, 0x7a2ec
   7a2e4:	49 fd 29 a2 	jal	0x1f628
   7a2e8:	48 00 00 8c 	j	0x7a400
   7a2ec:	a7 4a       	lbi333 $r5, [$r1 + #2]
   7a2ee:	4c 50 00 89 	beq	$r5, $r0, 0x7a400
   7a2f2:	a7 4b       	lbi333 $r5, [$r1 + #3]
   7a2f4:	4c 50 00 86 	beq	$r5, $r0, 0x7a400
   7a2f8:	a7 4c       	lbi333 $r5, [$r1 + #4]
   7a2fa:	d8 16       	bnes38 $r0, 0x7a326
   7a2fc:	2e 07 75 69 	lbi.gp	$r0, [ + #-35479]
   7a300:	5a 00 01 04 	beqc	$r0, #1, 0x7a308
   7a304:	48 00 00 7e 	j	0x7a400
   7a308:	2e 07 75 6b 	lbi.gp	$r0, [ + #-35477]
   7a30c:	5a 00 01 04 	beqc	$r0, #1, 0x7a314
   7a310:	48 00 00 78 	j	0x7a400
   7a314:	3e 0f 75 68 	addi.gp	$r0, #-35480
   7a318:	84 20       	movi55 $r1, #0
   7a31a:	ae 40       	sbi333 $r1, [$r0 + #0]
   7a31c:	84 21       	movi55 $r1, #1
   7a31e:	84 43       	movi55 $r2, #3
   7a320:	49 00 89 db 	jal	0x8b6d6
   7a324:	d5 6e       	j8 0x7a400
   7a326:	a7 4d       	lbi333 $r5, [$r1 + #5]
   7a328:	d8 6a       	bnes38 $r0, 0x7a3fc
   7a32a:	2e 07 75 6b 	lbi.gp	$r0, [ + #-35477]
   7a32e:	5a 08 02 07 	bnec	$r0, #2, 0x7a33c
   7a332:	5a 28 21 67 	bnec	$r2, #0x21, 0x7a400
   7a336:	49 ff ff 9d 	jal	0x7a270
   7a33a:	d5 63       	j8 0x7a400
   7a33c:	5a 08 01 08 	bnec	$r0, #1, 0x7a34c
   7a340:	5a 28 a1 60 	bnec	$r2, #0xa1, 0x7a400
   7a344:	49 ff ff a4 	jal	0x7a28c
   7a348:	48 00 00 5c 	j	0x7a400
   7a34c:	5a 08 03 5a 	bnec	$r0, #3, 0x7a400
   7a350:	5a 28 a1 34 	bnec	$r2, #0xa1, 0x7a3b8
   7a354:	46 00 01 82 	sethi	$r0, #0x182
   7a358:	58 00 00 a0 	ori	$r0, $r0, #0xa0
   7a35c:	49 06 dd 1a 	jal	0x155d90
   7a360:	3c 0d e2 44 	lwi.gp	$r0, [ + #-30448]
   7a364:	4e 02 00 1c 	beqz	$r0, 0x7a39c
   7a368:	46 00 01 82 	sethi	$r0, #0x182
   7a36c:	58 00 00 b4 	ori	$r0, $r0, #0xb4
   7a370:	49 06 dd 10 	jal	0x155d90
   7a374:	3e 0f 75 68 	addi.gp	$r0, #-35480
   7a378:	44 10 00 55 	movi	$r1, #0x55
   7a37c:	ae 40       	sbi333 $r1, [$r0 + #0]
   7a37e:	a6 47       	lbi333 $r1, [$r0 + #7]
   7a380:	40 10 a0 08 	slli	$r1, $r1, #8
   7a384:	a6 86       	lbi333 $r2, [$r0 + #6]
   7a386:	88 22       	add45 $r1, $r2
   7a388:	44 20 00 01 	movi	$r2, #1
   7a38c:	49 00 89 a5 	jal	0x8b6d6
   7a390:	44 00 00 40 	movi	$r0, #0x40
   7a394:	49 fd a0 9e 	jal	0x2e4d0
   7a398:	48 00 00 34 	j	0x7a400
   7a39c:	3e 0f 75 68 	addi.gp	$r0, #-35480
   7a3a0:	84 20       	movi55 $r1, #0
   7a3a2:	ae 40       	sbi333 $r1, [$r0 + #0]
   7a3a4:	a6 47       	lbi333 $r1, [$r0 + #7]
   7a3a6:	40 10 a0 08 	slli	$r1, $r1, #8
   7a3aa:	a6 86       	lbi333 $r2, [$r0 + #6]
   7a3ac:	88 22       	add45 $r1, $r2
   7a3ae:	84 41       	movi55 $r2, #1
   7a3b0:	49 00 89 93 	jal	0x8b6d6
   7a3b4:	48 00 00 26 	j	0x7a400
   7a3b8:	5a 28 21 24 	bnec	$r2, #0x21, 0x7a400
   7a3bc:	44 00 00 00 	movi	$r0, #0
   7a3c0:	3c 0f e2 44 	swi.gp	$r0, [ + #-30448]
   7a3c4:	46 00 01 82 	sethi	$r0, #0x182
   7a3c8:	58 00 00 c8 	ori	$r0, $r0, #0xc8
   7a3cc:	49 06 dc e2 	jal	0x155d90
   7a3d0:	2e 00 4a 74 	lbi.gp	$r0, [ + #0x4a74]
   7a3d4:	5a 08 55 0e 	bnec	$r0, #0x55, 0x7a3f0
   7a3d8:	44 01 00 b8 	movi	$r0, #0x100b8
   7a3dc:	3e 18 4a 75 	addi.gp	$r1, #0x4a75
   7a3e0:	84 44       	movi55 $r2, #4
   7a3e2:	49 06 db a3 	jal	0x155b28
   7a3e6:	c0 0d       	beqz38 $r0, 0x7a400
   7a3e8:	84 01       	movi55 $r0, #1
   7a3ea:	3c 0f e2 44 	swi.gp	$r0, [ + #-30448]
   7a3ee:	d5 09       	j8 0x7a400
   7a3f0:	5a 08 aa 08 	bnec	$r0, #0xaa, 0x7a400
   7a3f4:	84 01       	movi55 $r0, #1
   7a3f6:	3c 0f e2 44 	swi.gp	$r0, [ + #-30448]
   7a3fa:	d5 03       	j8 0x7a400
   7a3fc:	49 fd 29 16 	jal	0x1f628
   7a400:	ec 04       	addi10.sp #4
   7a402:	3b ff fc 84 	lmw.bim $sp, [$sp], $sp, #0x2    ! {$lp}
   7a406:	dd 9e       	ret5 $lp
