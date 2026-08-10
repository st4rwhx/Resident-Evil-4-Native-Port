# SN Systems ProDG libsn (tealeaf): __cvt_fp2unsigned, CodeWarrior-named 64-bit runtime aliases
# branching to libgcc, and the MWCC-ABI __va_arg. Hand-written (cpp-preprocessed) assembly in the
# original library (libsn.a tealeaf.o carries a `tea151.tmp` FILE symbol like proview/ppcdown, the C
# objects carry `<name>.c`); reproduced from the split object's disassembly.
.include "macros.inc"


# 0x80065854..0x800659AC | size: 0x158
.text
.balign 4

# .text:0x0 | 0x80065854 | size: 0x48
.fn __cvt_fp2unsigned, global
	stwu r1, -0x10(r1)
	lis r9, lbl_80225C60@ha
	addi r9, r9, lbl_80225C60@l
	lfd f0, 0x0(r9)
	fcmpu cr0, f1, f0
	cror un, eq, gt
	bso .L_80065880
	fctiwz f0, f1
	stfd f0, 0x8(r1)
	lwz r3, 0xc(r1)
	b .L_80065894
.L_80065880:
	fsub f0, f1, f0
	fctiwz f13, f0
	stfd f13, 0x8(r1)
	lwz r3, 0xc(r1)
	xoris r3, r3, 0x8000
.L_80065894:
	addi r1, r1, 0x10
	blr
.endfn __cvt_fp2unsigned

# .text:0x48 | 0x8006589C | size: 0x4
.fn __shr2u, global
	b __lshrdi3
.endfn __shr2u

# .text:0x4C | 0x800658A0 | size: 0x4
.fn __div2i, global
	b __divdi3
.endfn __div2i

# .text:0x50 | 0x800658A4 | size: 0x4
.fn __shl2i, global
	b __ashldi3
.endfn __shl2i

# .text:0x54 | 0x800658A8 | size: 0x4
.fn __mod2i, global
	b __moddi3
.endfn __mod2i

# .text:0x58 | 0x800658AC | size: 0x4
.fn __shr2i, global
	b __ashrdi3
.endfn __shr2i

# .text:0x5C | 0x800658B0 | size: 0x4
.fn __div2u, global
	b __udivdi3
.endfn __div2u

# .text:0x60 | 0x800658B4 | size: 0x4
.fn __mod2u, global
	b __umoddi3
.endfn __mod2u

# .text:0x64 | 0x800658B8 | size: 0xF4
.fn __va_arg, global
	clrlwi r0, r4, 24
	lbz r6, 0x0(r3)
	cmplwi r0, 0x4
	addi r7, r3, 0x0
	extsb r6, r6
	li r5, 0x8
	li r8, 0x4
	li r9, 0x1
	li r10, 0x0
	li r11, 0x0
	li r12, 0x4
	bne .L_80065904
	lwz r4, 0x4(r3)
	addi r0, r4, 0xf
	clrrwi r4, r0, 4
	addi r0, r4, 0x10
	stw r0, 0x4(r3)
	mr r3, r4
	blr
.L_80065904:
	cmplwi r0, 0x3
	bne .L_80065924
	lbz r6, 0x1(r3)
	addi r7, r3, 0x1
	li r8, 0x8
	extsb r6, r6
	li r11, 0x20
	li r12, 0x8
.L_80065924:
	clrlwi r0, r4, 24
	cmplwi r0, 0x2
	bne .L_80065948
	clrlwi. r0, r6, 31
	li r8, 0x8
	li r5, 0x7
	beq .L_80065944
	li r10, 0x1
.L_80065944:
	li r9, 0x2
.L_80065948:
	cmpw r6, r5
	bge .L_80065970
	add r6, r6, r10
	lwz r5, 0x8(r3)
	mullw r3, r6, r12
	add r0, r6, r9
	add r6, r11, r3
	stb r0, 0x0(r7)
	add r6, r5, r6
	b .L_80065998
.L_80065970:
	li r0, 0x8
	stb r0, 0x0(r7)
	subi r0, r8, 0x1
	nor r6, r0, r0
	lwz r0, 0x4(r3)
	add r5, r8, r0
	subi r0, r5, 0x1
	and r6, r6, r0
	add r0, r6, r8
	stw r0, 0x4(r3)
.L_80065998:
	clrlwi. r0, r4, 24
	bne .L_800659A4
	lwz r6, 0x0(r6)
.L_800659A4:
	mr r3, r6
	blr
.endfn __va_arg

# 0x80225C60..0x80225C68 | size: 0x8
.rodata
.balign 8

# .rodata:0x0 | 0x80225C60 | size: 0x8
.obj lbl_80225C60, local
	.double 2147483648
.endobj lbl_80225C60
