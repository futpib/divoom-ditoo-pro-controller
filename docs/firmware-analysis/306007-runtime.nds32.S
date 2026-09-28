# Runtime integration evidence from code.bin, MVA offset = code address + 0x60f.


firmware/decoded/306007/code.bin:     file format binary


Disassembly of section .data:

0007516c <.data+0x7516c>:
   7516c:	3a 6f a8 bc 	smw.adm $r6, [$sp], $r10, #0x2    ! {$r6~$r10, $lp}
   75170:	84 e0       	movi55 $r7, #0
   75172:	92 00       	nop16
   75174:	49 00 19 a2 	jal	0x784b8
   75178:	80 c0       	mov55 $r6, $r0
   7517a:	50 13 87 d0 	addi	$r1, $r7, #0x7d0
   7517e:	40 10 80 07 	slts	$r1, $r1, $r0
   75182:	c1 55       	beqz38 $r1, 0x7522c
   75184:	3c 7d df 1e 	lwi.gp	$r7, [ + #-33672]
   75188:	84 00       	movi55 $r0, #0
   7518a:	49 00 19 83 	jal	0x78490
   7518e:	a8 39       	swi333 $r0, [$r7 + #4]
   75190:	80 e6       	mov55 $r7, $r6
   75192:	d5 4d       	j8 0x7522c
   75194:	9c 31       	addi333 $r0, $r6, #1
   75196:	94 04       	slli333 $r0, $r0, #4
   75198:	3c 1d df 1e 	lwi.gp	$r1, [ + #-33672]
   7519c:	38 00 80 00 	lb	$r0, [$r1 + ($r0 << #0)]
   751a0:	4e 02 00 36 	beqz	$r0, 0x7520c
   751a4:	49 00 19 80 	jal	0x784a4
   751a8:	3c 2d df 1e 	lwi.gp	$r2, [ + #-33672]
   751ac:	9c 71       	addi333 $r1, $r6, #1
   751ae:	94 4c       	slli333 $r1, $r1, #4
   751b0:	88 22       	add45 $r1, $r2
   751b2:	a0 49       	lwi333 $r1, [$r1 + #4]
   751b4:	40 00 04 06 	slt	$r0, $r0, $r1
   751b8:	c8 2a       	bnez38 $r0, 0x7520c
   751ba:	94 34       	slli333 $r0, $r6, #4
   751bc:	88 40       	add45 $r2, $r0
   751be:	a0 16       	lwi333 $r0, [$r2 + #0x18]
   751c0:	49 00 19 3e 	jal	0x7843c
   751c4:	3c 1d df 1e 	lwi.gp	$r1, [ + #-33672]
   751c8:	9c 31       	addi333 $r0, $r6, #1
   751ca:	94 04       	slli333 $r0, $r0, #4
   751cc:	38 00 80 00 	lb	$r0, [$r1 + ($r0 << #0)]
   751d0:	c0 08       	beqz38 $r0, 0x751e0
   751d2:	94 34       	slli333 $r0, $r6, #4
   751d4:	88 20       	add45 $r1, $r0
   751d6:	a0 4f       	lwi333 $r1, [$r1 + #0x1c]
   751d8:	c1 04       	beqz38 $r1, 0x751e0
   751da:	84 00       	movi55 $r0, #0
   751dc:	4b e0 04 01 	jral	$lp, $r1
   751e0:	49 00 19 62 	jal	0x784a4
   751e4:	81 40       	mov55 $r10, $r0
   751e6:	40 83 10 08 	slli	$r8, $r6, #4
   751ea:	3c 9d df 1e 	lwi.gp	$r9, [ + #-33672]
   751ee:	89 28       	add45 $r9, $r8
   751f0:	02 04 80 09 	lhi	$r0, [$r9 + #0x12]
   751f4:	49 00 19 82 	jal	0x784f8
   751f8:	88 0a       	add45 $r0, $r10
   751fa:	14 04 80 05 	swi	$r0, [$r9 + #0x14]
   751fe:	3c 0d df 1e 	lwi.gp	$r0, [ + #-33672]
   75202:	89 00       	add45 $r8, $r0
   75204:	04 04 00 06 	lwi	$r0, [$r8 + #0x18]
   75208:	49 00 19 26 	jal	0x78454
   7520c:	8c c1       	addi45 $r6, #1
   7520e:	5a 68 0a c3 	bnec	$r6, #0xa, 0x75194
   75212:	92 00       	nop16
   75214:	3c 0d df 1e 	lwi.gp	$r0, [ + #-33672]
   75218:	04 00 00 03 	lwi	$r0, [$r0 + #0xc]
   7521c:	49 00 19 1c 	jal	0x78454
   75220:	49 00 07 c0 	jal	0x761a0
   75224:	84 0a       	movi55 $r0, #0xa
   75226:	49 00 19 6b 	jal	0x784fc
   7522a:	d5 a5       	j8 0x75174
   7522c:	3c 0d df 1e 	lwi.gp	$r0, [ + #-33672]
   75230:	04 00 00 03 	lwi	$r0, [$r0 + #0xc]
   75234:	49 00 19 04 	jal	0x7843c
   75238:	3c 0d df 1e 	lwi.gp	$r0, [ + #-33672]
   7523c:	a6 00       	lbi333 $r0, [$r0 + #0]
   7523e:	84 c0       	movi55 $r6, #0
   75240:	c8 aa       	bnez38 $r0, 0x75194
   75242:	d5 e9       	j8 0x75214


firmware/decoded/306007/code.bin:     file format binary


Disassembly of section .data:

00075354 <.data+0x75354>:
   75354:	3a 6f b0 bc 	smw.adm $r6, [$sp], $r12, #0x2    ! {$r6~$r12, $lp}
   75358:	3c 4d df 1e 	lwi.gp	$r4, [ + #-33672]
   7535c:	c4 42       	beqz38 $r4, 0x753e0
   7535e:	a0 67       	lwi333 $r1, [$r4 + #0x1c]
   75360:	c1 07       	beqz38 $r1, 0x7536e
   75362:	50 12 00 2c 	addi	$r1, $r4, #0x2c
   75366:	84 c1       	movi55 $r6, #1
   75368:	b4 61       	lwi450 $r3, [$r1]
   7536a:	cb 33       	bnez38 $r3, 0x753d0
   7536c:	d5 02       	j8 0x75370
   7536e:	84 c0       	movi55 $r6, #0
   75370:	81 42       	mov55 $r10, $r2
   75372:	81 20       	mov55 $r9, $r0
   75374:	40 83 10 08 	slli	$r8, $r6, #4
   75378:	88 88       	add45 $r4, $r8
   7537a:	a0 26       	lwi333 $r0, [$r4 + #0x18]
   7537c:	49 00 18 60 	jal	0x7843c
   75380:	9d f1       	addi333 $r7, $r6, #1
   75382:	97 f8       	zeb33 $r7, $r7
   75384:	3c 0d df 1e 	lwi.gp	$r0, [ + #-33672]
   75388:	88 08       	add45 $r0, $r8
   7538a:	10 70 00 11 	sbi	$r7, [$r0 + #0x11]
   7538e:	84 4a       	movi55 $r2, #0xa
   75390:	42 25 08 24 	mul	$r2, $r10, $r2
   75394:	12 20 00 09 	shi	$r2, [$r0 + #0x12]
   75398:	49 00 18 86 	jal	0x784a4
   7539c:	81 60       	mov55 $r11, $r0
   7539e:	3c ad df 1e 	lwi.gp	$r10, [ + #-33672]
   753a2:	89 48       	add45 $r10, $r8
   753a4:	02 05 00 09 	lhi	$r0, [$r10 + #0x12]
   753a8:	49 00 18 a8 	jal	0x784f8
   753ac:	88 0b       	add45 $r0, $r11
   753ae:	14 05 00 05 	swi	$r0, [$r10 + #0x14]
   753b2:	3c 0d df 1e 	lwi.gp	$r0, [ + #-33672]
   753b6:	89 00       	add45 $r8, $r0
   753b8:	14 94 00 07 	swi	$r9, [$r8 + #0x1c]
   753bc:	8c c1       	addi45 $r6, #1
   753be:	95 b4       	slli333 $r6, $r6, #4
   753c0:	84 20       	movi55 $r1, #0
   753c2:	38 10 18 08 	sb	$r1, [$r0 + ($r6 << #0)]
   753c6:	04 04 00 06 	lwi	$r0, [$r8 + #0x18]
   753ca:	49 00 18 45 	jal	0x78454
   753ce:	d5 0b       	j8 0x753e4
   753d0:	8c c1       	addi45 $r6, #1
   753d2:	8c 30       	addi45 $r1, #0x10
   753d4:	5a 68 0a ca 	bnec	$r6, #0xa, 0x75368
   753d8:	44 70 00 ff 	movi	$r7, #0xff
   753dc:	d5 04       	j8 0x753e4
   753de:	92 00       	nop16
   753e0:	44 70 00 ff 	movi	$r7, #0xff
   753e4:	80 07       	mov55 $r0, $r7
   753e6:	3a 6f b0 84 	lmw.bim $r6, [$sp], $r12, #0x2    ! {$r6~$r12, $lp}
   753ea:	dd 9e       	ret5 $lp


firmware/decoded/306007/code.bin:     file format binary


Disassembly of section .data:

00075498 <.data+0x75498>:
   75498:	3a 6f 9c bc 	smw.adm $r6, [$sp], $r7, #0x2    ! {$r6~$r7, $lp}
   7549c:	ef f4       	addi10.sp #-12
   7549e:	3c 0d df 1e 	lwi.gp	$r0, [ + #-33672]
   754a2:	c0 09       	beqz38 $r0, 0x754b4
   754a4:	46 00 01 81 	sethi	$r0, #0x181
   754a8:	58 00 03 74 	ori	$r0, $r0, #0x374
   754ac:	49 07 04 72 	jal	0x155d90
   754b0:	48 00 00 44 	j	0x75538
   754b4:	44 00 00 b0 	movi	$r0, #0xb0
   754b8:	46 10 01 81 	sethi	$r1, #0x181
   754bc:	58 10 83 d4 	ori	$r1, $r1, #0x3d4
   754c0:	44 20 00 f5 	movi	$r2, #0xf5
   754c4:	49 00 17 6c 	jal	0x7839c
   754c8:	3c 0f df 1e 	swi.gp	$r0, [ + #-33672]
   754cc:	c8 08       	bnez38 $r0, 0x754dc
   754ce:	46 00 01 81 	sethi	$r0, #0x181
   754d2:	58 00 03 98 	ori	$r0, $r0, #0x398
   754d6:	49 07 04 5d 	jal	0x155d90
   754da:	d5 2f       	j8 0x75538
   754dc:	44 10 00 00 	movi	$r1, #0
   754e0:	44 20 00 b0 	movi	$r2, #0xb0
   754e4:	49 07 03 ac 	jal	0x155c3c
   754e8:	3c 6d df 1e 	lwi.gp	$r6, [ + #-33672]
   754ec:	49 00 17 9e 	jal	0x78428
   754f0:	a8 33       	swi333 $r0, [$r6 + #0xc]
   754f2:	84 c0       	movi55 $r6, #0
   754f4:	3c 7d df 1e 	lwi.gp	$r7, [ + #-33672]
   754f8:	49 00 17 98 	jal	0x78428
   754fc:	94 74       	slli333 $r1, $r6, #4
   754fe:	88 27       	add45 $r1, $r7
   75500:	a8 0e       	swi333 $r0, [$r1 + #0x18]
   75502:	8c c1       	addi45 $r6, #1
   75504:	5a 68 0a f8 	bnec	$r6, #0xa, 0x754f4
   75508:	84 21       	movi55 $r1, #1
   7550a:	3c 0d df 1e 	lwi.gp	$r0, [ + #-33672]
   7550e:	ae 40       	sbi333 $r1, [$r0 + #0]
   75510:	49 00 05 b4 	jal	0x76078
   75514:	3c 6d df 1e 	lwi.gp	$r6, [ + #-33672]
   75518:	84 01       	movi55 $r0, #1
   7551a:	b6 1f       	swi450 $r0, [$sp]
   7551c:	44 07 51 6c 	movi	$r0, #0x7516c
   75520:	46 10 01 81 	sethi	$r1, #0x181
   75524:	58 10 83 b8 	ori	$r1, $r1, #0x3b8
   75528:	84 40       	movi55 $r2, #0
   7552a:	44 30 02 00 	movi	$r3, #0x200
   7552e:	84 82       	movi55 $r4, #2
   75530:	84 a1       	movi55 $r5, #1
   75532:	49 00 17 9d 	jal	0x7846c
   75536:	a8 32       	swi333 $r0, [$r6 + #8]
   75538:	ec 0c       	addi10.sp #0xc
   7553a:	3a 6f 9c 84 	lmw.bim $r6, [$sp], $r7, #0x2    ! {$r6~$r7, $lp}
   7553e:	dd 9e       	ret5 $lp


firmware/decoded/306007/code.bin:     file format binary


Disassembly of section .data:

0004cb14 <.data+0x4cb14>:
   4cb14:	3b ff fc bc 	smw.adm $sp, [$sp], $sp, #0x2    ! {$lp}
   4cb18:	ef fc       	addi10.sp #-4
   4cb1a:	80 41       	mov55 $r2, $r1
   4cb1c:	84 20       	movi55 $r1, #0
   4cb1e:	49 01 44 1b 	jal	0x75354
   4cb22:	ec 04       	addi10.sp #4
   4cb24:	3b ff fc 84 	lmw.bim $sp, [$sp], $sp, #0x2    ! {$lp}
   4cb28:	dd 9e       	ret5 $lp
   4cb2a:	92 00       	nop16
   4cb2c:	3a 6f a0 bc 	smw.adm $r6, [$sp], $r8, #0x2    ! {$r6~$r8, $lp}
   4cb30:	3c 2d de d9 	lwi.gp	$r2, [ + #-33948]
   4cb34:	c2 3b       	beqz38 $r2, 0x4cbaa
   4cb36:	a1 16       	lwi333 $r4, [$r2 + #0x18]
   4cb38:	c4 3b       	beqz38 $r4, 0x4cbae
   4cb3a:	a0 d7       	lwi333 $r3, [$r2 + #0x1c]
   4cb3c:	c3 3b       	beqz38 $r3, 0x4cbb2
   4cb3e:	04 31 00 08 	lwi	$r3, [$r2 + #0x20]
   4cb42:	c3 3a       	beqz38 $r3, 0x4cbb6
   4cb44:	00 31 00 0c 	lbi	$r3, [$r2 + #0xc]
   4cb48:	5a 38 ff 04 	bnec	$r3, #0xff, 0x4cb50
   4cb4c:	a0 d2       	lwi333 $r3, [$r2 + #8]
   4cb4e:	c3 0a       	beqz38 $r3, 0x4cb62
   4cb50:	00 51 00 14 	lbi	$r5, [$r2 + #0x14]
   4cb54:	84 60       	movi55 $r3, #0
   4cb56:	5a 58 ff 33 	bnec	$r5, #0xff, 0x4cbbc
   4cb5a:	a0 94       	lwi333 $r2, [$r2 + #0x10]
   4cb5c:	84 c1       	movi55 $r6, #1
   4cb5e:	c2 03       	beqz38 $r2, 0x4cb64
   4cb60:	d5 2e       	j8 0x4cbbc
   4cb62:	84 c0       	movi55 $r6, #0
   4cb64:	dd 24       	jral5 $r4
   4cb66:	81 00       	mov55 $r8, $r0
   4cb68:	c0 29       	beqz38 $r0, 0x4cbba
   4cb6a:	95 b3       	slli333 $r6, $r6, #3
   4cb6c:	3c 7d de d9 	lwi.gp	$r7, [ + #-33948]
   4cb70:	88 e6       	add45 $r7, $r6
   4cb72:	a8 3a       	swi333 $r0, [$r7 + #8]
   4cb74:	44 04 ca c0 	movi	$r0, #0x4cac0
   4cb78:	44 10 00 05 	movi	$r1, #5
   4cb7c:	49 ff ff cc 	jal	0x4cb14
   4cb80:	10 03 80 0c 	sbi	$r0, [$r7 + #0xc]
