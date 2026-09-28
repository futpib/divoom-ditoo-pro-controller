# Unmodified firmware 306007; addresses are relative to code.bin.
# MVA file offset = address + 0x60f.


firmware/decoded/306007/code.bin:     file format binary


Disassembly of section .data:

0003abb4 <.data+0x3abb4>:
   3abb4:	a6 39       	lbi333 $r0, [$r7 + #1]
   3abb6:	4e 03 03 aa 	bnez	$r0, 0x3b30a
   3abba:	50 9f 80 30 	addi	$r9, $sp, #0x30
   3abbe:	80 09       	mov55 $r0, $r9
   3abc0:	84 20       	movi55 $r1, #0
   3abc2:	fa 40       	movpi45 $r2, #0x10
   3abc4:	49 08 d8 3c 	jal	0x155c3c
   3abc8:	04 04 00 59 	lwi	$r0, [$r8 + #0x164]
   3abcc:	f0 8c       	swi37.sp $r0, [ + #0x30]
   3abce:	9c 3a       	addi333 $r0, $r7, #2
   3abd0:	49 01 7b 4c 	jal	0x6a268
   3abd4:	14 04 80 03 	swi	$r0, [$r9 + #0xc]
   3abd8:	a0 32       	lwi333 $r0, [$r6 + #8]
   3abda:	8c 06       	addi45 $r0, #6
   3abdc:	49 01 7b 46 	jal	0x6a268
   3abe0:	14 04 80 01 	swi	$r0, [$r9 + #4]
   3abe4:	a0 32       	lwi333 $r0, [$r6 + #8]
   3abe6:	8c 0a       	addi45 $r0, #0xa
   3abe8:	49 01 7b 40 	jal	0x6a268
   3abec:	14 04 80 02 	swi	$r0, [$r9 + #8]
   3abf0:	84 0b       	movi55 $r0, #0xb
   3abf2:	10 0f 80 ca 	sbi	$r0, [$sp + #0xca]
   3abf6:	84 0a       	movi55 $r0, #0xa
   3abf8:	10 0f 80 cc 	sbi	$r0, [$sp + #0xcc]
   3abfc:	14 9f 80 34 	swi	$r9, [$sp + #0xd0]
   3ac00:	50 0f 80 c8 	addi	$r0, $sp, #0xc8
   3ac04:	49 00 7d 04 	jal	0x4a60c
   3ac08:	02 0f 80 67 	lhi	$r0, [$sp + #0xce]
   3ac0c:	5a 08 01 04 	bnec	$r0, #1, 0x3ac14
   3ac10:	48 00 03 7d 	j	0x3b30a
   3ac14:	a0 72       	lwi333 $r1, [$r6 + #8]
   3ac16:	a6 49       	lbi333 $r1, [$r1 + #1]
   3ac18:	10 1f 80 24 	sbi	$r1, [$sp + #0x24]
   3ac1c:	5a 08 02 16 	bnec	$r0, #2, 0x3ac48
   3ac20:	b6 3f       	swi450 $r1, [$sp]
   3ac22:	46 00 01 5e 	sethi	$r0, #0x15e
   3ac26:	58 00 08 f4 	ori	$r0, $r0, #0x8f4
   3ac2a:	49 08 d8 ae 	jal	0x155d86
   3ac2e:	84 00       	movi55 $r0, #0
   3ac30:	10 0f 80 25 	sbi	$r0, [$sp + #0x25]
   3ac34:	04 04 00 59 	lwi	$r0, [$r8 + #0x164]
   3ac38:	44 10 00 98 	movi	$r1, #0x98
   3ac3c:	b0 89       	addri36.sp $r2, #0x24
   3ac3e:	84 62       	movi55 $r3, #2
   3ac40:	49 ff f1 ee 	jal	0x3901c
   3ac44:	48 00 03 63 	j	0x3b30a
   3ac48:	b6 3f       	swi450 $r1, [$sp]
   3ac4a:	46 00 01 5e 	sethi	$r0, #0x15e
   3ac4e:	58 00 09 44 	ori	$r0, $r0, #0x944
   3ac52:	49 08 d8 9a 	jal	0x155d86
   3ac56:	84 01       	movi55 $r0, #1
   3ac58:	10 0f 80 25 	sbi	$r0, [$sp + #0x25]
   3ac5c:	04 04 00 59 	lwi	$r0, [$r8 + #0x164]
   3ac60:	44 10 00 98 	movi	$r1, #0x98
   3ac64:	b0 89       	addri36.sp $r2, #0x24
   3ac66:	84 62       	movi55 $r3, #2
   3ac68:	49 ff f1 da 	jal	0x3901c
   3ac6c:	48 00 03 4f 	j	0x3b30a


firmware/decoded/306007/code.bin:     file format binary


Disassembly of section .data:

00038294 <.data+0x38294>:
   38294:	04 83 00 02 	lwi	$r8, [$r6 + #8]
   38298:	84 00       	movi55 $r0, #0
   3829a:	49 ff fd 19 	jal	0x37ccc
   3829e:	c0 3d       	beqz38 $r0, 0x38318
   382a0:	3c 7d de b9 	lwi.gp	$r7, [ + #-34076]
   382a4:	04 03 80 08 	lwi	$r0, [$r7 + #0x20]
   382a8:	c0 06       	beqz38 $r0, 0x382b4
   382aa:	84 02       	movi55 $r0, #2
   382ac:	12 03 00 03 	shi	$r0, [$r6 + #6]
   382b0:	48 00 01 bb 	j	0x38626
   382b4:	49 01 f9 2c 	jal	0x7750c
   382b8:	a8 3d       	swi333 $r0, [$r7 + #0x14]
   382ba:	3c 7d de b9 	lwi.gp	$r7, [ + #-34076]
   382be:	a0 3d       	lwi333 $r0, [$r7 + #0x14]
   382c0:	a8 3e       	swi333 $r0, [$r7 + #0x18]
   382c2:	b4 08       	lwi450 $r0, [$r8]
   382c4:	04 14 00 01 	lwi	$r1, [$r8 + #4]
   382c8:	04 24 00 02 	lwi	$r2, [$r8 + #8]
   382cc:	04 34 00 03 	lwi	$r3, [$r8 + #0xc]
   382d0:	49 00 98 ea 	jal	0x4b4a4
   382d4:	14 03 80 08 	swi	$r0, [$r7 + #0x20]
   382d8:	3c 0d de b9 	lwi.gp	$r0, [ + #-34076]
   382dc:	04 10 00 08 	lwi	$r1, [$r0 + #0x20]
   382e0:	c1 16       	beqz38 $r1, 0x3830c
   382e2:	84 21       	movi55 $r1, #1
   382e4:	10 1f 80 10 	sbi	$r1, [$sp + #0x10]
   382e8:	84 20       	movi55 $r1, #0
   382ea:	ae 40       	sbi333 $r1, [$r0 + #0]
   382ec:	84 01       	movi55 $r0, #1
   382ee:	ac 33       	shi333 $r0, [$r6 + #6]
   382f0:	fa 01       	movpi45 $r0, #0x11
   382f2:	10 0f 80 1e 	sbi	$r0, [$sp + #0x1e]
   382f6:	fa 0c       	movpi45 $r0, #0x1c
   382f8:	10 0f 80 20 	sbi	$r0, [$sp + #0x20]
   382fc:	b0 04       	addri36.sp $r0, #0x10
   382fe:	f0 89       	swi37.sp $r0, [ + #0x24]
   38300:	50 0f 80 1c 	addi	$r0, $sp, #0x1c
   38304:	49 00 91 84 	jal	0x4a60c
   38308:	48 00 01 8f 	j	0x38626
   3830c:	84 27       	movi55 $r1, #7
   3830e:	ae 40       	sbi333 $r1, [$r0 + #0]
   38310:	84 00       	movi55 $r0, #0
   38312:	ac 33       	shi333 $r0, [$r6 + #6]
   38314:	48 00 01 89 	j	0x38626
   38318:	46 00 01 5e 	sethi	$r0, #0x15e
   3831c:	58 00 04 98 	ori	$r0, $r0, #0x498
   38320:	b6 1f       	swi450 $r0, [$sp]
   38322:	46 00 01 5e 	sethi	$r0, #0x15e
   38326:	58 00 03 7c 	ori	$r0, $r0, #0x37c
   3832a:	49 08 ed 2e 	jal	0x155d86
   3832e:	84 00       	movi55 $r0, #0
   38330:	ac 33       	shi333 $r0, [$r6 + #6]
   38332:	48 00 01 7a 	j	0x38626
   38336:	92 00       	nop16


firmware/decoded/306007/code.bin:     file format binary


Disassembly of section .data:

00047924 <.data+0x47924>:
   47924:	44 04 ab 57 	movi	$r0, #0x4ab57
   47928:	dd 9e       	ret5 $lp
   4792a:	92 00       	nop16


firmware/decoded/306007/code.bin:     file format binary


Disassembly of section .data:

0004b4a4 <.data+0x4b4a4>:
   4b4a4:	3a 6f a8 bc 	smw.adm $r6, [$sp], $r10, #0x2    ! {$r6~$r10, $lp}
   4b4a8:	ef f0       	addi10.sp #-16
   4b4aa:	81 20       	mov55 $r9, $r0
   4b4ac:	81 01       	mov55 $r8, $r1
   4b4ae:	81 42       	mov55 $r10, $r2
   4b4b0:	80 e3       	mov55 $r7, $r3
   4b4b2:	84 00       	movi55 $r0, #0
   4b4b4:	f0 83       	swi37.sp $r0, [ + #0xc]
   4b4b6:	84 02       	movi55 $r0, #2
   4b4b8:	10 0f 80 0d 	sbi	$r0, [$sp + #0xd]
   4b4bc:	49 ff e2 34 	jal	0x47924
   4b4c0:	46 41 06 24 	sethi	$r4, #0x10624
   4b4c4:	50 42 0d d3 	addi	$r4, $r4, #0xdd3
   4b4c8:	42 13 90 b8 	smmul	$r1, $r7, $r4
   4b4cc:	40 23 fc 0a 	srai	$r2, $r7, #0x1f
   4b4d0:	fe a6       	and33 $r2, $r4
   4b4d2:	88 22       	add45 $r1, $r2
   4b4d4:	40 50 98 09 	srli	$r5, $r1, #6
   4b4d8:	42 10 10 b8 	smmul	$r1, $r0, $r4
   4b4dc:	90 1f       	srai45 $r0, #0x1f
   4b4de:	ff 06       	and33 $r4, $r0
   4b4e0:	88 24       	add45 $r1, $r4
   4b4e2:	92 26       	srli45 $r1, #6
   4b4e4:	d1 16       	beqs38 $r1, 0x4b510
   4b4e6:	49 ff e2 1f 	jal	0x47924
   4b4ea:	b6 ff       	swi450 $r7, [$sp]
   4b4ec:	f0 81       	swi37.sp $r0, [ + #4]
   4b4ee:	46 00 01 79 	sethi	$r0, #0x179
   4b4f2:	58 00 02 d8 	ori	$r0, $r0, #0x2d8
   4b4f6:	49 08 54 48 	jal	0x155d86
   4b4fa:	84 00       	movi55 $r0, #0
   4b4fc:	44 10 00 98 	movi	$r1, #0x98
   4b500:	b0 83       	addri36.sp $r2, #0xc
   4b502:	84 62       	movi55 $r3, #2
   4b504:	49 ff 6d 8c 	jal	0x3901c
   4b508:	44 00 00 00 	movi	$r0, #0
   4b50c:	48 00 01 13 	j	0x4b732
   4b510:	49 ff e2 0a 	jal	0x47924
   4b514:	46 41 06 24 	sethi	$r4, #0x10624
   4b518:	50 42 0d d3 	addi	$r4, $r4, #0xdd3
   4b51c:	42 13 90 b8 	smmul	$r1, $r7, $r4
   4b520:	90 26       	srai45 $r1, #6
   4b522:	40 23 fc 0a 	srai	$r2, $r7, #0x1f
   4b526:	8a 22       	sub45 $r1, $r2
   4b528:	b6 3f       	swi450 $r1, [$sp]
   4b52a:	42 40 10 b8 	smmul	$r4, $r0, $r4
   4b52e:	40 12 18 0a 	srai	$r1, $r4, #6
   4b532:	40 40 7c 0a 	srai	$r4, $r0, #0x1f
   4b536:	9b 0c       	sub333 $r4, $r1, $r4
   4b538:	14 4f 80 01 	swi	$r4, [$sp + #4]
   4b53c:	46 00 01 79 	sethi	$r0, #0x179
   4b540:	58 00 03 14 	ori	$r0, $r0, #0x314
   4b544:	49 08 54 21 	jal	0x155d86
   4b548:	49 ff e1 ee 	jal	0x47924
   4b54c:	40 00 1c 06 	slt	$r0, $r0, $r7
   4b550:	c8 16       	bnez38 $r0, 0x4b57c
   4b552:	49 ff e1 e9 	jal	0x47924
   4b556:	b6 ff       	swi450 $r7, [$sp]
   4b558:	f0 81       	swi37.sp $r0, [ + #4]
   4b55a:	46 00 01 79 	sethi	$r0, #0x179
   4b55e:	58 00 02 d8 	ori	$r0, $r0, #0x2d8
   4b562:	49 08 54 12 	jal	0x155d86
   4b566:	84 00       	movi55 $r0, #0
   4b568:	44 10 00 98 	movi	$r1, #0x98
   4b56c:	b0 83       	addri36.sp $r2, #0xc
   4b56e:	84 62       	movi55 $r3, #2
   4b570:	49 ff 6d 56 	jal	0x3901c
   4b574:	44 00 00 00 	movi	$r0, #0
   4b578:	48 00 00 dd 	j	0x4b732
   4b57c:	44 00 01 2c 	movi	$r0, #0x12c
   4b580:	46 10 01 79 	sethi	$r1, #0x179
   4b584:	58 10 84 88 	ori	$r1, $r1, #0x488
   4b588:	44 20 01 2b 	movi	$r2, #0x12b
   4b58c:	49 01 67 08 	jal	0x7839c
   4b590:	80 c0       	mov55 $r6, $r0
   4b592:	c8 12       	bnez38 $r0, 0x4b5b6
   4b594:	84 00       	movi55 $r0, #0
   4b596:	44 10 00 98 	movi	$r1, #0x98
   4b59a:	b0 83       	addri36.sp $r2, #0xc
   4b59c:	44 30 00 02 	movi	$r3, #2
   4b5a0:	49 ff 6d 3e 	jal	0x3901c
   4b5a4:	46 00 01 79 	sethi	$r0, #0x179
   4b5a8:	58 00 03 44 	ori	$r0, $r0, #0x344
   4b5ac:	49 08 53 f2 	jal	0x155d90
   4b5b0:	84 00       	movi55 $r0, #0
   4b5b2:	48 00 00 c0 	j	0x4b732
   4b5b6:	84 20       	movi55 $r1, #0
   4b5b8:	44 20 01 2c 	movi	$r2, #0x12c
   4b5bc:	49 08 53 40 	jal	0x155c3c
   4b5c0:	a9 f2       	swi333 $r7, [$r6 + #8]
   4b5c2:	84 02       	movi55 $r0, #2
   4b5c4:	50 13 00 0c 	addi	$r1, $r6, #0xc
   4b5c8:	49 01 42 e2 	jal	0x73b8c
   4b5cc:	a8 34       	swi333 $r0, [$r6 + #0x10]
   4b5ce:	c8 1b       	bnez38 $r0, 0x4b604
   4b5d0:	46 00 01 79 	sethi	$r0, #0x179
   4b5d4:	58 00 03 94 	ori	$r0, $r0, #0x394
   4b5d8:	b6 1f       	swi450 $r0, [$sp]
   4b5da:	44 00 01 3b 	movi	$r0, #0x13b
   4b5de:	f0 81       	swi37.sp $r0, [ + #4]
   4b5e0:	46 00 01 79 	sethi	$r0, #0x179
   4b5e4:	58 00 03 74 	ori	$r0, $r0, #0x374
   4b5e8:	49 08 53 cf 	jal	0x155d86
   4b5ec:	80 06       	mov55 $r0, $r6
   4b5ee:	46 10 01 79 	sethi	$r1, #0x179
   4b5f2:	58 10 84 88 	ori	$r1, $r1, #0x488
   4b5f6:	44 20 01 3c 	movi	$r2, #0x13c
   4b5fa:	49 01 66 e7 	jal	0x783c8
   4b5fe:	84 00       	movi55 $r0, #0
   4b600:	48 00 00 99 	j	0x4b732
   4b604:	14 83 00 05 	swi	$r8, [$r6 + #0x14]
   4b608:	14 a3 00 07 	swi	$r10, [$r6 + #0x1c]
   4b60c:	84 3f       	movi55 $r1, #-1
   4b60e:	ae 70       	sbi333 $r1, [$r6 + #0]
   4b610:	50 a4 00 ff 	addi	$r10, $r8, #0xff
   4b614:	93 48       	srli45 $r10, #8
   4b616:	54 14 00 ff 	andi	$r1, $r8, #0xff
   4b61a:	c1 05       	beqz38 $r1, 0x4b624
   4b61c:	14 a3 00 06 	swi	$r10, [$r6 + #0x18]
   4b620:	48 00 00 06 	j	0x4b62c
   4b624:	50 15 00 01 	addi	$r1, $r10, #1
   4b628:	14 13 00 06 	swi	$r1, [$r6 + #0x18]
   4b62c:	e2 08       	slt45 $r0, $r8
   4b62e:	e8 1e       	beqzs8 0x4b66a
   4b630:	b7 1f       	swi450 $r8, [$sp]
   4b632:	f0 81       	swi37.sp $r0, [ + #4]
   4b634:	46 00 01 79 	sethi	$r0, #0x179
   4b638:	58 00 03 cc 	ori	$r0, $r0, #0x3cc
   4b63c:	49 08 53 a5 	jal	0x155d86
   4b640:	84 01       	movi55 $r0, #1
   4b642:	10 0f 80 0d 	sbi	$r0, [$sp + #0xd]
   4b646:	84 00       	movi55 $r0, #0
   4b648:	44 10 00 98 	movi	$r1, #0x98
   4b64c:	b0 83       	addri36.sp $r2, #0xc
   4b64e:	84 62       	movi55 $r3, #2
   4b650:	49 ff 6c e6 	jal	0x3901c
   4b654:	80 06       	mov55 $r0, $r6
   4b656:	46 10 01 79 	sethi	$r1, #0x179
   4b65a:	58 10 84 88 	ori	$r1, $r1, #0x488
   4b65e:	44 20 01 4c 	movi	$r2, #0x14c
   4b662:	49 01 66 b3 	jal	0x783c8
   4b666:	84 00       	movi55 $r0, #0
   4b668:	d5 65       	j8 0x4b732
