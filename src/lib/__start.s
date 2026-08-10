# SN Systems ProDG crt0 `.init`: entry point (stack/SDA setup, .bss clear, OSInit, main). `__start`
# itself is defined by config/G4BE08/ldscript.ld at 0x80003100, so the code carries a local label only.
# Hand-written assembly in the original library; reproduced from the split object's disassembly.
.include "macros.inc"

# 0x80003100..0x800034A0 | size: 0x3A0
.section .init, "ax"
.balign 4

# .init:0x0 | 0x80003100 | size: 0x3A0
.fn __start_entry, local
	lis r1, _stack_addr@h
	ori r1, r1, _stack_addr@l
	lis r2, _SDA2_BASE_@h
	ori r2, r2, _SDA2_BASE_@l
	lis r13, _SDA_BASE_@h
	ori r13, r13, _SDA_BASE_@l
	bl .L_80003300
	bl .L_80003334
	bl .L_80003468
	li r0, -0x1
	stwu r1, -0x8(r1)
	stw r0, 0x4(r1)
	stw r0, 0x0(r1)
	lis r3, 0x8027
	ori r3, r3, 0x4a0
	li r4, 0x0
	lis r5, 0x8031
	ori r5, r5, 0x3ee0
	subf r5, r3, r5
	bl memset
	lis r3, 0x8031
	ori r3, r3, 0x5460
	li r4, 0x0
	lis r5, 0x8031
	ori r5, r5, 0x5cb0
	subf r5, r3, r5
	bl memset
	li r4, 0x0
	lis r5, 0x8000
	stw r4, 0x44(r5)
	li r29, 0x0
	lis r6, 0x8000
	ori r6, r6, 0xf4
	lwz r6, 0x0(r6)
	cmpwi r6, 0x0
	beq .L_800031A4
	lwz r3, 0xc(r6)
	cmplwi r3, 0x2
	blt .L_800031A4
	bl SNDebugInit
	li r29, 0x1
.L_800031A4:
	bl .L_80003254
	mr r30, r3
	mr r31, r4
	bl DBInit
	bl OSInit
	lis r4, 0x8000
	lhz r3, 0x30e6(r4)
	andi. r4, r3, 0x8000
	beq .L_800031D4
	andi. r3, r3, 0x7fff
	cmplwi r3, 0x1
	bne .L_800031D8
.L_800031D4:
	bl .L_800032C0
.L_800031D8:
	bl .L_80003464
	mr r3, r30
	mr r4, r31
	bl .L_80003464
	bl __init_user
	cmpwi r29, 0x0
	beq .L_80003244
	lis r3, 0x8025
	ori r3, r3, 0x3a40
	lis r4, 0x8025
	ori r4, r4, 0x3a64
	lwz r4, 0x0(r4)
	bl OSReport
	lis r3, 0x8025
	ori r3, r3, 0x3a20
	bl OSReport
	mfmsr r5
	ori r4, r5, 0x8000
	xori r4, r4, 0x8000
	mtmsr r4
	sync
	ori r5, r5, 0x200
	mtsrr1 r5
	lis r4, 0x8000
	ori r4, r4, 0x3244
	mtsrr0 r4
	rfi
.L_80003244:
	mr r3, r30
	mr r4, r31
	bl main
	b exit
.L_80003254:
	lis r6, 0x8000
	ori r6, r6, 0xf4
	lwz r5, 0x0(r6)
	cmpwi r5, 0x0
	bne .L_80003274
.L_80003268:
	li r3, 0x0
	li r4, 0x0
	blr
.L_80003274:
	lwz r6, 0x8(r5)
	cmpwi r6, 0x0
	beq .L_80003268
	add r6, r5, r6
	lwz r3, 0x0(r6)
	cmpwi r3, 0x0
	beq .L_80003268
	addi r4, r6, 0x4
	mtctr r3
.L_80003298:
	addi r6, r6, 0x4
	lwz r7, 0x0(r6)
	add r7, r7, r5
	stw r7, 0x0(r6)
	bdnz .L_80003298
	lis r5, 0x8000
	ori r5, r5, 0x34
	clrrwi r7, r4, 5
	stw r7, 0x0(r5)
	blr
.L_800032C0:
	mflr r0
	stw r0, 0x4(r1)
	stwu r1, -0x8(r1)
	lis r3, 0x8000
	lhz r0, 0x30e4(r3)
	andi. r0, r0, 0xeef
	cmpwi r0, 0xeef
	bne .L_800032F0
	li r3, 0x0
	li r4, 0x0
	li r5, 0x0
	bl OSResetSystem
.L_800032F0:
	lwz r0, 0xc(r1)
	addi r1, r1, 0x8
	mtlr r0
	blr
.L_80003300:
	li r3, 0x0
	mtspr GQR0, r3
	mtspr GQR1, r3
	mtspr GQR2, r3
	mtspr GQR3, r3
	mtspr GQR4, r3
	mtspr GQR5, r3
	mtspr GQR6, r3
	mtspr GQR7, r3
	blr
	.4byte 0x00000000 /* invalid */
	.4byte 0x00000000 /* invalid */
	.4byte 0x00000000 /* invalid */
.L_80003334:
	mfmsr r3
	ori r3, r3, 0x2000
	mtmsr r3
	mfspr r3, HID2
	extrwi. r3, r3, 1, 2
	beq .L_800033D4
	lis r3, 0x8000
	ori r3, r3, 0x332c
	psq_l f0, 0x0(r3), 0, qr0
	ps_mr f1, f0
	ps_mr f2, f0
	ps_mr f3, f0
	ps_mr f4, f0
	ps_mr f5, f0
	ps_mr f6, f0
	ps_mr f7, f0
	ps_mr f8, f0
	ps_mr f9, f0
	ps_mr f10, f0
	ps_mr f11, f0
	ps_mr f12, f0
	ps_mr f13, f0
	ps_mr f14, f0
	ps_mr f15, f0
	ps_mr f16, f0
	ps_mr f17, f0
	ps_mr f18, f0
	ps_mr f19, f0
	ps_mr f20, f0
	ps_mr f21, f0
	ps_mr f22, f0
	ps_mr f23, f0
	ps_mr f24, f0
	ps_mr f25, f0
	ps_mr f26, f0
	ps_mr f27, f0
	ps_mr f28, f0
	ps_mr f29, f0
	ps_mr f30, f0
	ps_mr f31, f0
.L_800033D4:
	lis r3, 0x8000
	ori r3, r3, 0x3328
	lfd f0, 0x0(r3)
	fmr f1, f0
	fmr f2, f0
	fmr f3, f0
	fmr f4, f0
	fmr f5, f0
	fmr f6, f0
	fmr f7, f0
	fmr f8, f0
	fmr f9, f0
	fmr f10, f0
	fmr f11, f0
	fmr f12, f0
	fmr f13, f0
	fmr f14, f0
	fmr f15, f0
	fmr f16, f0
	fmr f17, f0
	fmr f18, f0
	fmr f19, f0
	fmr f20, f0
	fmr f21, f0
	fmr f22, f0
	fmr f23, f0
	fmr f24, f0
	fmr f25, f0
	fmr f26, f0
	fmr f27, f0
	fmr f28, f0
	fmr f29, f0
	fmr f30, f0
	fmr f31, f0
	mtfsf 255, f0
	blr
.L_80003464:
	blr
.L_80003468:
	mfmsr r0
	ori r0, r0, 0x2000
	mtmsr r0
	mflr r31
	bl 0x6fd54	# -> 0x800731C0 (OSDefaultExceptionHandler+0x4c)
	bl 0x6f318	# -> 0x80072788 (PPCDisableSpeculation+0x24)
	bl 0x70c90	# -> 0x80074104 (DMAErrorHandler+0x154)
	mtlr r31
	blr
	.4byte 0x00000000 /* invalid */
	.4byte 0x00000000 /* invalid */
	.4byte 0x00000000 /* invalid */
	.4byte 0x00000000 /* invalid */
	.4byte 0x00000000 /* invalid */
.endfn __start_entry
