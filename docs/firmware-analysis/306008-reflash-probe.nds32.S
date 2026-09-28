# Patched 306008 instructions; code offsets equal MVA offsets minus 0x60f.


firmware/decoded/306008-code.bin:     file format binary


Disassembly of section .data:

00047924 <.data+0x47924>:
   47924:	44 04 ab 58 	movi	$r0, #0x4ab58
   47928:	dd 9e       	ret5 $lp
   4792a:	92 00       	nop16


firmware/decoded/306008-code.bin:     file format binary


Disassembly of section .data:

0004b548 <.data+0x4b548>:
   4b548:	49 ff e1 ee 	jal	0x47924
   4b54c:	40 00 1c 06 	slt	$r0, $r0, $r7
   4b550:	d5 16       	j8 0x4b57c
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
