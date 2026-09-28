# GNU binutils 2.40 NDS32 linear disassembly of code.bin.
# Addresses are code-image offsets; original MVA offset = address + 0x60f.
# Inline data can decode as instructions; see firmware-format.md.
    2ab2:	84 80       	movi55 $r4, #0
    2ab4:	d5 0f       	j8 0x2ad2
    2ab6:	40 51 20 09 	srli	$r5, $r2, #8
    2aba:	a6 c0       	lbi333 $r3, [$r0 + #0]
    2abc:	fe ed       	xor33 $r3, $r5
    2abe:	44 50 84 e0 	movi	$r5, #0x84e0
    2ac2:	38 32 8d 01 	lh	$r3, [$r5 + ($r3 << #1)]
    2ac6:	40 21 20 08 	slli	$r2, $r2, #8
    2aca:	96 91       	zeh33 $r2, $r2
    2acc:	fe 9d       	xor33 $r2, $r3
    2ace:	8c 81       	addi45 $r4, #1
    2ad0:	8c 01       	addi45 $r0, #1
    2ad2:	e2 81       	slt45 $r4, $r1
    2ad4:	e9 f1       	bnezs8 0x2ab6
    2ad6:	80 02       	mov55 $r0, $r2
    2ad8:	dd 9e       	ret5 $lp
    2b48:	3a 6f a4 bc 	smw.adm $r6, [$sp], $r9, #0x2    ! {$r6~$r9, $lp}
    2b4c:	ef e4       	addi10.sp #-28
    2b4e:	3e 68 10 dc 	addi.gp	$r6, #0x10dc
    2b52:	3c 0d b3 f2 	lwi.gp	$r0, [ + #-77880]
    2b56:	8c 04       	addi45 $r0, #4
    2b58:	80 26       	mov55 $r1, $r6
    2b5a:	84 41       	movi55 $r2, #1
    2b5c:	49 00 0b 12 	jal	0x4180
    2b60:	a7 f0       	lbi333 $r7, [$r6 + #0]
    2b62:	3c 0d b3 f2 	lwi.gp	$r0, [ + #-77880]
    2b66:	8c 05       	addi45 $r0, #5
    2b68:	80 26       	mov55 $r1, $r6
    2b6a:	84 49       	movi55 $r2, #9
    2b6c:	49 00 0b 0a 	jal	0x4180
    2b70:	49 00 23 ef 	jal	0x734e
    2b74:	84 c5       	movi55 $r6, #5
    2b76:	2e 10 10 dc 	lbi.gp	$r1, [ + #0x10dc]
    2b7a:	9e 09       	subi333 $r0, $r1, #1
    2b7c:	e6 05       	slti45 $r0, #5
    2b7e:	4e f2 01 2d 	beqz	$r15, 0x2dd8
    2b82:	44 f0 2b 8c 	movi	$r15, #0x2b8c
    2b86:	38 07 82 02 	lw	$r0, [$r15 + ($r0 << #2)]
    2b8a:	dd 00       	jr5 $r0
    2b8c:	a0 2b       	lwi333 $r0, [$r5 + #0xc]
    2b8e:	00 00 e2 2b 	lbi	$r0, [$r1 + #-7637]
    2b92:	00 00 bc 2b 	lbi	$r0, [$r1 + #0x3c2b]
    2b96:	00 00 56 2d 	lbi	$r0, [$r0 + #-10707]
    2b9a:	00 00 56 2d 	lbi	$r0, [$r0 + #-10707]
    2b9e:	00 00 b6 df 	lbi	$r0, [$r1 + #0x36df]
    2ba2:	44 00 87 84 	movi	$r0, #0x8784
    2ba6:	49 00 25 1f 	jal	0x75e4
    2baa:	3e 08 10 dd 	addi.gp	$r0, #0x10dd
    2bae:	3a 00 00 00 	lmw.bi $r0, [$r0], $r0, #0x0    ! {$r0}
    2bb2:	88 c0       	add45 $r6, $r0
    2bb4:	8c c5       	addi45 $r6, #5
    2bb6:	8e e1       	subi45 $r7, #1
    2bb8:	48 00 01 17 	j	0x2de6
    2bbc:	b6 df       	swi450 $r6, [$sp]
    2bbe:	44 00 87 98 	movi	$r0, #0x8798
    2bc2:	49 00 25 11 	jal	0x75e4
    2bc6:	3e 08 10 dd 	addi.gp	$r0, #0x10dd
    2bca:	3a 80 20 00 	lmw.bi $r8, [$r0], $r8, #0x0    ! {$r8}
    2bce:	b7 1f       	swi450 $r8, [$sp]
    2bd0:	44 00 87 ac 	movi	$r0, #0x87ac
    2bd4:	49 00 25 08 	jal	0x75e4
    2bd8:	88 c8       	add45 $r6, $r8
    2bda:	8c c5       	addi45 $r6, #5
    2bdc:	8e e1       	subi45 $r7, #1
    2bde:	48 00 01 04 	j	0x2de6
    2be2:	b6 df       	swi450 $r6, [$sp]
    2be4:	44 00 87 bc 	movi	$r0, #0x87bc
    2be8:	49 00 24 fe 	jal	0x75e4
    2bec:	3e 08 10 dd 	addi.gp	$r0, #0x10dd
    2bf0:	3a 80 20 00 	lmw.bi $r8, [$r0], $r8, #0x0    ! {$r8}
    2bf4:	b7 1f       	swi450 $r8, [$sp]
    2bf6:	44 00 87 d0 	movi	$r0, #0x87d0
    2bfa:	49 00 24 f5 	jal	0x75e4
    2bfe:	50 44 7f fc 	addi	$r4, $r8, #-4
    2c02:	44 11 00 00 	movi	$r1, #0x10000
    2c06:	40 10 90 06 	slt	$r1, $r1, $r4
    2c0a:	c9 1d       	bnez38 $r1, 0x2c44
    2c0c:	3e 39 30 f4 	addi.gp	$r3, #0x130f4
    2c10:	00 21 80 08 	lbi	$r2, [$r3 + #8]
    2c14:	00 11 80 09 	lbi	$r1, [$r3 + #9]
    2c18:	40 10 a0 08 	slli	$r1, $r1, #8
    2c1c:	fe 8f       	or33 $r2, $r1
    2c1e:	00 11 80 0a 	lbi	$r1, [$r3 + #0xa]
    2c22:	40 10 c0 08 	slli	$r1, $r1, #0x10
    2c26:	fe 57       	or33 $r1, $r2
    2c28:	00 01 80 0b 	lbi	$r0, [$r3 + #0xb]
    2c2c:	40 00 60 08 	slli	$r0, $r0, #0x18
    2c30:	fe 0f       	or33 $r0, $r1
    2c32:	e2 04       	slt45 $r0, $r4
    2c34:	e8 08       	beqzs8 0x2c44
    2c36:	44 00 87 e0 	movi	$r0, #0x87e0
    2c3a:	49 00 24 da 	jal	0x75ee
    2c3e:	84 0f       	movi55 $r0, #0xf
    2c40:	48 00 00 df 	j	0x2dfe
    2c44:	3c 0d b3 f2 	lwi.gp	$r0, [ + #-77880]
    2c48:	88 06       	add45 $r0, $r6
    2c4a:	3e 98 10 dc 	addi.gp	$r9, #0x10dc
    2c4e:	8c 09       	addi45 $r0, #9
    2c50:	80 29       	mov55 $r1, $r9
    2c52:	44 20 04 00 	movi	$r2, #0x400
    2c56:	49 00 0a 95 	jal	0x4180
    2c5a:	50 94 80 e0 	addi	$r9, $r9, #0xe0
    2c5e:	3a 04 80 00 	lmw.bi $r0, [$r9], $r0, #0x0    ! {$r0}
    2c62:	5a 0f ff 09 	bnec	$r0, #-1, 0x2c74
    2c66:	44 00 87 f4 	movi	$r0, #0x87f4
    2c6a:	49 00 24 c2 	jal	0x75ee
    2c6e:	84 0d       	movi55 $r0, #0xd
    2c70:	48 00 00 c7 	j	0x2dfe
    2c74:	3e 08 11 9c 	addi.gp	$r0, #0x119c
    2c78:	3a 50 14 00 	lmw.bi $r5, [$r0], $r5, #0x0    ! {$r5}
    2c7c:	46 0b 0b eb 	sethi	$r0, #0xb0beb
    2c80:	50 00 0d c9 	addi	$r0, $r0, #0xdc9
    2c84:	d0 08       	beqs38 $r0, 0x2c94
    2c86:	44 00 88 08 	movi	$r0, #0x8808
    2c8a:	49 00 24 b2 	jal	0x75ee
    2c8e:	84 0c       	movi55 $r0, #0xc
    2c90:	48 00 00 b7 	j	0x2dfe
    2c94:	2e 00 11 db 	lbi.gp	$r0, [ + #0x11db]
    2c98:	2e 50 10 d4 	lbi.gp	$r5, [ + #0x10d4]
    2c9c:	d0 08       	beqs38 $r0, 0x2cac
    2c9e:	44 00 88 1c 	movi	$r0, #0x881c
    2ca2:	49 00 24 a6 	jal	0x75ee
    2ca6:	84 0e       	movi55 $r0, #0xe
    2ca8:	48 00 00 ab 	j	0x2dfe
    2cac:	3e 08 11 98 	addi.gp	$r0, #0x1198
    2cb0:	3a 10 04 00 	lmw.bi $r1, [$r0], $r1, #0x0    ! {$r1}
    2cb4:	3c 5c 04 32 	lwi.gp	$r5, [ + #0x10c8]
    2cb8:	d1 0e       	beqs38 $r1, 0x2cd4
    2cba:	b6 3f       	swi450 $r1, [$sp]
    2cbc:	f5 81       	swi37.sp $r5, [ + #4]
    2cbe:	44 00 88 48 	movi	$r0, #0x8848
    2cc2:	49 00 24 91 	jal	0x75e4
    2cc6:	44 00 00 55 	movi	$r0, #0x55
    2cca:	3e 06 cf c0 	sbi.gp	$r0, [ + #-77888]
    2cce:	84 00       	movi55 $r0, #0
    2cd0:	48 00 00 97 	j	0x2dfe
    2cd4:	3c 0d b3 f2 	lwi.gp	$r0, [ + #-77880]
    2cd8:	88 06       	add45 $r0, $r6
    2cda:	44 11 00 09 	movi	$r1, #0x10009
    2cde:	88 01       	add45 $r0, $r1
    2ce0:	3e 18 10 dc 	addi.gp	$r1, #0x10dc
    2ce4:	44 20 04 00 	movi	$r2, #0x400
    2ce8:	49 00 0a 4c 	jal	0x4180
    2cec:	2e 00 10 d5 	lbi.gp	$r0, [ + #0x10d5]
    2cf0:	54 00 00 f0 	andi	$r0, $r0, #0xf0
    2cf4:	84 60       	movi55 $r3, #0
    2cf6:	84 44       	movi55 $r2, #4
    2cf8:	5a 00 f0 17 	beqc	$r0, #0xf0, 0x2d26
    2cfc:	44 01 00 cc 	movi	$r0, #0x100cc
    2d00:	b0 43       	addri36.sp $r1, #0xc
    2d02:	49 ff f1 26 	jal	0xf4e
    2d06:	b0 03       	addri36.sp $r0, #0xc
    2d08:	3e 18 11 a8 	addi.gp	$r1, #0x11a8
    2d0c:	84 44       	movi55 $r2, #4
    2d0e:	49 00 24 21 	jal	0x7550
    2d12:	c0 1e       	beqz38 $r0, 0x2d4e
    2d14:	44 00 88 58 	movi	$r0, #0x8858
    2d18:	49 00 24 6b 	jal	0x75ee
    2d1c:	44 00 00 55 	movi	$r0, #0x55
    2d20:	3e 06 cf c1 	sbi.gp	$r0, [ + #-77887]
    2d24:	d5 15       	j8 0x2d4e
    2d26:	44 01 00 b8 	movi	$r0, #0x100b8
    2d2a:	b0 43       	addri36.sp $r1, #0xc
    2d2c:	49 ff f1 11 	jal	0xf4e
    2d30:	b0 03       	addri36.sp $r0, #0xc
    2d32:	3e 18 11 94 	addi.gp	$r1, #0x1194
    2d36:	84 44       	movi55 $r2, #4
    2d38:	49 00 24 0c 	jal	0x7550
    2d3c:	c0 09       	beqz38 $r0, 0x2d4e
    2d3e:	44 00 88 7c 	movi	$r0, #0x887c
    2d42:	49 00 24 56 	jal	0x75ee
    2d46:	44 00 00 55 	movi	$r0, #0x55
    2d4a:	3e 06 cf c1 	sbi.gp	$r0, [ + #-77887]
    2d4e:	88 c8       	add45 $r6, $r8
    2d50:	8c c5       	addi45 $r6, #5
    2d52:	8e e1       	subi45 $r7, #1
    2d54:	d5 49       	j8 0x2de6
    2d56:	b6 3f       	swi450 $r1, [$sp]
    2d58:	f6 81       	swi37.sp $r6, [ + #4]
    2d5a:	44 00 88 a0 	movi	$r0, #0x88a0
    2d5e:	49 00 24 43 	jal	0x75e4
    2d62:	3e 08 10 dc 	addi.gp	$r0, #0x10dc
    2d66:	9c 41       	addi333 $r1, $r0, #1
    2d68:	3a 50 94 00 	lmw.bi $r5, [$r1], $r5, #0x0    ! {$r5}
    2d6c:	8c 05       	addi45 $r0, #5
    2d6e:	3a 40 10 00 	lmw.bi $r4, [$r0], $r4, #0x0    ! {$r4}
    2d72:	41 02 14 00 	add	$r16, $r4, $r5
    2d76:	3e 39 30 f4 	addi.gp	$r3, #0x130f4
    2d7a:	00 21 80 08 	lbi	$r2, [$r3 + #8]
    2d7e:	00 11 80 09 	lbi	$r1, [$r3 + #9]
    2d82:	40 10 a0 08 	slli	$r1, $r1, #8
    2d86:	fe 8f       	or33 $r2, $r1
    2d88:	00 11 80 0a 	lbi	$r1, [$r3 + #0xa]
    2d8c:	40 10 c0 08 	slli	$r1, $r1, #0x10
    2d90:	fe 57       	or33 $r1, $r2
    2d92:	00 01 80 0b 	lbi	$r0, [$r3 + #0xb]
    2d96:	40 00 60 08 	slli	$r0, $r0, #0x18
    2d9a:	fe 0f       	or33 $r0, $r1
    2d9c:	40 00 40 06 	slt	$r0, $r0, $r16
    2da0:	c0 07       	beqz38 $r0, 0x2dae
    2da2:	44 00 88 b4 	movi	$r0, #0x88b4
    2da6:	49 00 24 24 	jal	0x75ee
    2daa:	fa 00       	movpi45 $r0, #0x10
    2dac:	d5 29       	j8 0x2dfe
    2dae:	54 42 0f ff 	andi	$r4, $r4, #0xfff
    2db2:	c4 07       	beqz38 $r4, 0x2dc0
    2db4:	44 00 88 f8 	movi	$r0, #0x88f8
    2db8:	49 00 24 1b 	jal	0x75ee
    2dbc:	fa 02       	movpi45 $r0, #0x12
    2dbe:	d5 20       	j8 0x2dfe
    2dc0:	88 c5       	add45 $r6, $r5
    2dc2:	44 00 00 55 	movi	$r0, #0x55
    2dc6:	3e 06 cf c2 	sbi.gp	$r0, [ + #-77886]
    2dca:	44 00 89 0c 	movi	$r0, #0x890c
    2dce:	49 00 24 10 	jal	0x75ee
    2dd2:	8c c5       	addi45 $r6, #5
    2dd4:	8e e1       	subi45 $r7, #1
    2dd6:	d5 08       	j8 0x2de6
    2dd8:	3e 08 10 dd 	addi.gp	$r0, #0x10dd
    2ddc:	3a 00 00 00 	lmw.bi $r0, [$r0], $r0, #0x0    ! {$r0}
    2de0:	88 c0       	add45 $r6, $r0
    2de2:	8c c5       	addi45 $r6, #5
    2de4:	8e e1       	subi45 $r7, #1
    2de6:	3c 0d b3 f2 	lwi.gp	$r0, [ + #-77880]
    2dea:	88 06       	add45 $r0, $r6
    2dec:	3e 18 10 dc 	addi.gp	$r1, #0x10dc
    2df0:	44 20 20 00 	movi	$r2, #0x2000
    2df4:	49 00 09 c6 	jal	0x4180
    2df8:	4e 73 fe bf 	bnez	$r7, 0x2b76
    2dfc:	84 00       	movi55 $r0, #0
    2dfe:	ec 1c       	addi10.sp #0x1c
    2e00:	3a 6f a4 84 	lmw.bim $r6, [$sp], $r9, #0x2    ! {$r6~$r9, $lp}
    2e04:	dd 9e       	ret5 $lp
    2e06:	92 00       	nop16
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
    4528:	3a 6f a0 bc 	smw.adm $r6, [$sp], $r8, #0x2    ! {$r6~$r8, $lp}
    452c:	ef e8       	addi10.sp #-24
    452e:	80 c0       	mov55 $r6, $r0
    4530:	85 00       	movi55 $r8, #0
    4532:	14 8f 80 04 	swi	$r8, [$sp + #0x10]
    4536:	14 8f 80 03 	swi	$r8, [$sp + #0xc]
    453a:	14 8f 80 02 	swi	$r8, [$sp + #8]
    453e:	44 00 91 38 	movi	$r0, #0x9138
    4542:	49 00 18 56 	jal	0x75ee
    4546:	44 00 99 3c 	movi	$r0, #0x993c
    454a:	3e 7e cf d4 	addi.gp	$r7, #-77868
    454e:	3b 00 48 04 	lmw.bim $r16, [$r0], $r18, #0x0    ! {$r16~$r18}
    4552:	80 27       	mov55 $r1, $r7
    4554:	3b 00 c8 24 	smw.bim $r16, [$r1], $r18, #0x0    ! {$r16~$r18}
    4558:	a6 00       	lbi333 $r0, [$r0 + #0]
    455a:	ae 08       	sbi333 $r0, [$r1 + #0]
    455c:	44 00 91 54 	movi	$r0, #0x9154
    4560:	49 00 18 47 	jal	0x75ee
    4564:	84 0d       	movi55 $r0, #0xd
    4566:	f0 83       	swi37.sp $r0, [ + #0xc]
    4568:	f0 03       	lwi37.sp $r0, [ + #0xc]
    456a:	84 23       	movi55 $r1, #3
    456c:	38 13 80 08 	sb	$r1, [$r7 + ($r0 << #0)]
    4570:	f0 03       	lwi37.sp $r0, [ + #0xc]
    4572:	8c 01       	addi45 $r0, #1
    4574:	f0 83       	swi37.sp $r0, [ + #0xc]
    4576:	44 00 05 f4 	movi	$r0, #0x5f4
    457a:	f0 85       	swi37.sp $r0, [ + #0x14]
    457c:	f0 03       	lwi37.sp $r0, [ + #0xc]
    457e:	88 07       	add45 $r0, $r7
    4580:	b0 45       	addri36.sp $r1, #0x14
    4582:	3a 10 84 00 	lmw.bi $r1, [$r1], $r1, #0x0    ! {$r1}
    4586:	3a 10 04 20 	smw.bi $r1, [$r0], $r1, #0x0    ! {$r1}
    458a:	f0 03       	lwi37.sp $r0, [ + #0xc]
    458c:	8c 04       	addi45 $r0, #4
    458e:	f0 83       	swi37.sp $r0, [ + #0xc]
    4590:	14 8f 80 04 	swi	$r8, [$sp + #0x10]
    4594:	f0 03       	lwi37.sp $r0, [ + #0xc]
    4596:	88 07       	add45 $r0, $r7
    4598:	b0 44       	addri36.sp $r1, #0x10
    459a:	3a 10 84 00 	lmw.bi $r1, [$r1], $r1, #0x0    ! {$r1}
    459e:	3a 10 04 20 	smw.bi $r1, [$r0], $r1, #0x0    ! {$r1}
    45a2:	f0 03       	lwi37.sp $r0, [ + #0xc]
    45a4:	8c 04       	addi45 $r0, #4
    45a6:	f0 83       	swi37.sp $r0, [ + #0xc]
    45a8:	f0 03       	lwi37.sp $r0, [ + #0xc]
    45aa:	88 07       	add45 $r0, $r7
    45ac:	44 10 93 4c 	movi	$r1, #0x934c
    45b0:	44 20 05 f0 	movi	$r2, #0x5f0
    45b4:	49 00 17 ea 	jal	0x7588
    45b8:	f0 03       	lwi37.sp $r0, [ + #0xc]
    45ba:	50 00 05 f0 	addi	$r0, $r0, #0x5f0
    45be:	f0 83       	swi37.sp $r0, [ + #0xc]
    45c0:	44 00 91 6c 	movi	$r0, #0x916c
    45c4:	49 00 18 15 	jal	0x75ee
    45c8:	f0 03       	lwi37.sp $r0, [ + #0xc]
    45ca:	84 22       	movi55 $r1, #2
    45cc:	38 13 80 08 	sb	$r1, [$r7 + ($r0 << #0)]
    45d0:	f0 03       	lwi37.sp $r0, [ + #0xc]
    45d2:	8c 01       	addi45 $r0, #1
    45d4:	f0 83       	swi37.sp $r0, [ + #0xc]
    45d6:	44 00 91 7c 	movi	$r0, #0x917c
    45da:	49 00 18 0a 	jal	0x75ee
    45de:	00 03 00 e2 	lbi	$r0, [$r6 + #0xe2]
    45e2:	00 13 00 e1 	lbi	$r1, [$r6 + #0xe1]
    45e6:	00 23 00 e0 	lbi	$r2, [$r6 + #0xe0]
    45ea:	88 01       	add45 $r0, $r1
    45ec:	88 02       	add45 $r0, $r2
    45ee:	96 00       	zeb33 $r0, $r0
    45f0:	00 53 00 e3 	lbi	$r5, [$r6 + #0xe3]
    45f4:	d0 07       	beqs38 $r0, 0x4602
    45f6:	44 00 91 94 	movi	$r0, #0x9194
    45fa:	49 00 17 fa 	jal	0x75ee
    45fe:	49 00 11 4d 	jal	0x6898
    4602:	00 03 00 e2 	lbi	$r0, [$r6 + #0xe2]
    4606:	00 13 00 e1 	lbi	$r1, [$r6 + #0xe1]
    460a:	40 00 20 08 	slli	$r0, $r0, #8
    460e:	88 01       	add45 $r0, $r1
    4610:	40 00 20 08 	slli	$r0, $r0, #8
    4614:	00 13 00 e0 	lbi	$r1, [$r6 + #0xe0]
    4618:	88 01       	add45 $r0, $r1
    461a:	f0 85       	swi37.sp $r0, [ + #0x14]
    461c:	f0 05       	lwi37.sp $r0, [ + #0x14]
    461e:	b6 1f       	swi450 $r0, [$sp]
    4620:	44 00 91 ac 	movi	$r0, #0x91ac
    4624:	49 00 17 e0 	jal	0x75e4
    4628:	f0 05       	lwi37.sp $r0, [ + #0x14]
    462a:	8c 04       	addi45 $r0, #4
    462c:	f0 84       	swi37.sp $r0, [ + #0x10]
    462e:	f0 03       	lwi37.sp $r0, [ + #0xc]
    4630:	3e 7e cf d4 	addi.gp	$r7, #-77868
    4634:	88 07       	add45 $r0, $r7
    4636:	b0 44       	addri36.sp $r1, #0x10
    4638:	3a 10 84 00 	lmw.bi $r1, [$r1], $r1, #0x0    ! {$r1}
    463c:	3a 10 04 20 	smw.bi $r1, [$r0], $r1, #0x0    ! {$r1}
    4640:	f0 03       	lwi37.sp $r0, [ + #0xc]
    4642:	8c 04       	addi45 $r0, #4
    4644:	f0 83       	swi37.sp $r0, [ + #0xc]
    4646:	44 00 91 c4 	movi	$r0, #0x91c4
    464a:	49 00 17 d2 	jal	0x75ee
    464e:	85 00       	movi55 $r8, #0
    4650:	14 8f 80 04 	swi	$r8, [$sp + #0x10]
    4654:	f0 03       	lwi37.sp $r0, [ + #0xc]
    4656:	88 07       	add45 $r0, $r7
    4658:	b0 44       	addri36.sp $r1, #0x10
    465a:	3a 10 84 00 	lmw.bi $r1, [$r1], $r1, #0x0    ! {$r1}
    465e:	3a 10 04 20 	smw.bi $r1, [$r0], $r1, #0x0    ! {$r1}
    4662:	f0 03       	lwi37.sp $r0, [ + #0xc]
    4664:	8c 04       	addi45 $r0, #4
    4666:	f0 83       	swi37.sp $r0, [ + #0xc]
    4668:	44 00 91 e4 	movi	$r0, #0x91e4
    466c:	49 00 17 c1 	jal	0x75ee
    4670:	f0 03       	lwi37.sp $r0, [ + #0xc]
    4672:	f2 05       	lwi37.sp $r2, [ + #0x14]
    4674:	88 07       	add45 $r0, $r7
    4676:	80 26       	mov55 $r1, $r6
    4678:	49 00 17 88 	jal	0x7588
    467c:	44 00 92 08 	movi	$r0, #0x9208
    4680:	49 00 17 b7 	jal	0x75ee
    4684:	14 8f 80 02 	swi	$r8, [$sp + #8]
    4688:	f2 02       	lwi37.sp $r2, [ + #8]
    468a:	80 06       	mov55 $r0, $r6
    468c:	44 10 00 a4 	movi	$r1, #0xa4
    4690:	96 91       	zeh33 $r2, $r2
    4692:	49 ff f2 10 	jal	0x2ab2
    4696:	f0 82       	swi37.sp $r0, [ + #8]
    4698:	f2 02       	lwi37.sp $r2, [ + #8]
    469a:	50 03 00 a8 	addi	$r0, $r6, #0xa8
    469e:	fa 24       	movpi45 $r1, #0x14
    46a0:	96 91       	zeh33 $r2, $r2
    46a2:	49 ff f2 08 	jal	0x2ab2
    46a6:	f0 82       	swi37.sp $r0, [ + #8]
    46a8:	f2 02       	lwi37.sp $r2, [ + #8]
    46aa:	50 03 00 c0 	addi	$r0, $r6, #0xc0
    46ae:	84 2c       	movi55 $r1, #0xc
    46b0:	96 91       	zeh33 $r2, $r2
    46b2:	49 ff f2 00 	jal	0x2ab2
    46b6:	f0 82       	swi37.sp $r0, [ + #8]
    46b8:	f2 02       	lwi37.sp $r2, [ + #8]
    46ba:	50 03 00 d4 	addi	$r0, $r6, #0xd4
    46be:	fa 20       	movpi45 $r1, #0x10
    46c0:	96 91       	zeh33 $r2, $r2
    46c2:	49 ff f1 f8 	jal	0x2ab2
    46c6:	f0 82       	swi37.sp $r0, [ + #8]
    46c8:	f1 05       	lwi37.sp $r1, [ + #0x14]
    46ca:	f2 02       	lwi37.sp $r2, [ + #8]
    46cc:	50 03 00 ec 	addi	$r0, $r6, #0xec
    46d0:	50 10 ff 14 	addi	$r1, $r1, #-236
    46d4:	96 91       	zeh33 $r2, $r2
    46d6:	49 ff f1 ee 	jal	0x2ab2
    46da:	f0 82       	swi37.sp $r0, [ + #8]
    46dc:	f0 02       	lwi37.sp $r0, [ + #8]
    46de:	b6 1f       	swi450 $r0, [$sp]
    46e0:	44 00 92 28 	movi	$r0, #0x9228
    46e4:	49 00 17 80 	jal	0x75e4
    46e8:	f0 03       	lwi37.sp $r0, [ + #0xc]
    46ea:	50 00 00 a4 	addi	$r0, $r0, #0xa4
    46ee:	50 63 00 e0 	addi	$r6, $r6, #0xe0
    46f2:	88 07       	add45 $r0, $r7
    46f4:	3a 13 04 00 	lmw.bi $r1, [$r6], $r1, #0x0    ! {$r1}
    46f8:	3a 10 04 20 	smw.bi $r1, [$r0], $r1, #0x0    ! {$r1}
    46fc:	f0 03       	lwi37.sp $r0, [ + #0xc]
    46fe:	50 00 00 d0 	addi	$r0, $r0, #0xd0
    4702:	88 07       	add45 $r0, $r7
    4704:	3a 13 04 00 	lmw.bi $r1, [$r6], $r1, #0x0    ! {$r1}
    4708:	3a 10 04 20 	smw.bi $r1, [$r0], $r1, #0x0    ! {$r1}
    470c:	f0 03       	lwi37.sp $r0, [ + #0xc]
    470e:	50 00 00 bc 	addi	$r0, $r0, #0xbc
    4712:	88 07       	add45 $r0, $r7
    4714:	b0 42       	addri36.sp $r1, #8
    4716:	3a 10 84 00 	lmw.bi $r1, [$r1], $r1, #0x0    ! {$r1}
    471a:	3a 10 04 20 	smw.bi $r1, [$r0], $r1, #0x0    ! {$r1}
    471e:	f0 03       	lwi37.sp $r0, [ + #0xc]
    4720:	50 00 00 cc 	addi	$r0, $r0, #0xcc
    4724:	88 07       	add45 $r0, $r7
    4726:	3a 10 04 20 	smw.bi $r1, [$r0], $r1, #0x0    ! {$r1}
    472a:	f0 02       	lwi37.sp $r0, [ + #8]
    472c:	b6 1f       	swi450 $r0, [$sp]
    472e:	44 00 92 3c 	movi	$r0, #0x923c
    4732:	49 00 17 59 	jal	0x75e4
    4736:	44 00 92 48 	movi	$r0, #0x9248
    473a:	49 00 17 5a 	jal	0x75ee
    473e:	f1 03       	lwi37.sp $r1, [ + #0xc]
