# SN Systems ProDG libsn: debugger stub (SNDebugInit, tuner protocol, ISI/DSI exception entries).
# Hand-written assembly in the original library; reproduced from the split object's disassembly.
.include "macros.inc"

# 0x80062A60..0x80064AE0 | size: 0x2080
# The library object's .text is 32-byte aligned (the DOL pads sscrn's .text up to 0x80062A60).
.text
.balign 32

# .text:0x0 | 0x80062A60 | size: 0x88
.fn fn_80062A60, global
	li r6, 0x1
	stw r6, 0x6ec(r31)
	andi. r6, r3, 0x3
	bne .L_80062AAC
	andi. r6, r4, 0x3
	bne .L_80062AAC
	andi. r6, r5, 0x3
	bne .L_80062AAC
	subi r3, r3, 0x4
	subi r4, r4, 0x4
	add r5, r5, r4
.L_80062A8C:
	lwzu r6, 0x4(r4)
	stwu r6, 0x4(r3)
	cmplw r4, r5
	blt .L_80062A8C
	li r6, 0x0
	stw r6, 0x6ec(r31)
	li r3, 0x1
	blr
.L_80062AAC:
	subi r3, r3, 0x1
	subi r4, r4, 0x1
	add r5, r5, r4
.L_80062AB8:
	lbzu r6, 0x1(r4)
	stbu r6, 0x1(r3)
	cmplw r4, r5
	blt .L_80062AB8
	li r6, 0x0
	stw r6, 0x6ec(r31)
	li r3, 0x1
	blr
	li r6, 0x0
	stw r6, 0x6ec(r31)
	li r3, 0x0
	blr
.endfn fn_80062A60

# .text:0x88 | 0x80062AE8 | size: 0xF0
.fn DBcallback, global
	mflr r0
	stw r0, 0x4(r1)
	stwu r1, -0x10(r1)
	stw r31, 0x8(r1)
	stw r4, 0xc(r1)
	lis r6, lbl_80253A70@h
	ori r6, r6, lbl_80253A70@l
	lwz r5, 0x0(r6)
	cmpwi r5, 0x0
	bne .L_80062BC4
	lis r3, g_nRWasyncPhase@h
	ori r3, r3, g_nRWasyncPhase@l
	lwz r3, 0x0(r3)
	cmpwi r3, 0x0
	beq .L_80062B2C
	bl PCrwAsyncNextPh
	b .L_80062BC4
.L_80062B2C:
	lis r31, lbl_80278340+0x1020@h
	ori r31, r31, lbl_80278340+0x1020@l
	lwz r3, 0x6cc(r31)
	cmpwi r3, 0x2
	beq .L_80062B5C
	cmpwi r3, 0x3
	beq .L_80062B5C
	lwz r0, 0x69c(r31)
	mtlr r0
	blrl
	cmpwi r3, 0x0
	beq .L_80062BC4
.L_80062B5C:
	lwz r4, 0xc(r1)
	lwz r5, 0x19c(r4)
	ori r5, r5, 0x400
	stw r5, 0x19c(r4)
	li r5, 0x1
	stw r5, 0x6bc(r31)
	lwz r10, 0x198(r4)
	lis r7, OSDisableInterrupts@h
	ori r7, r7, OSDisableInterrupts@l
	cmplw r10, r7
	blt .L_80062BA0
	lis r8, OSDisableInterrupts+0xC@h
	ori r8, r8, OSDisableInterrupts+0xC@l
	cmplw r10, r8
	bgt .L_80062BA0
	stw r7, 0x198(r4)
	mr r10, r7
.L_80062BA0:
	lwz r7, 0x0(r10)
	rlwinm r9, r7, 0, 11, 5
	lis r8, 0x7c00
	ori r8, r8, 0xa6
	cmpw r9, r8
	bne .L_80062BC4
	rlwinm r7, r7, 14, 24, 28
	stw r7, 0x6c4(r31)
	stw r10, 0x6c0(r31)
.L_80062BC4:
	lwz r31, 0x8(r1)
	lwz r0, 0x14(r1)
	addi r1, r1, 0x10
	mtlr r0
	blr
.endfn DBcallback

# .text:0x178 | 0x80062BD8 | size: 0x64
.fn EnableMetroTRKInterrupts, global
	mflr r0
	stw r0, 0x4(r1)
	stwu r1, -0x10(r1)
	stw r31, 0x8(r1)
	lis r31, lbl_80278340+0x1020@h
	ori r31, r31, lbl_80278340+0x1020@l
	li r5, 0x0
	stw r5, 0x6bc(r31)
	lwz r0, 0x698(r31)
	mtlr r0
	blrl
	mr r4, r3
	lis r3, bConnected+0xCC@h
	ori r3, r3, bConnected+0xCC@l
	bl OSReport
	lwz r3, 0x6cc(r31)
	cmpwi r3, 0x4
	bne .L_80062C28
	bl SNInitEXI2TCHandler
	bl SNDVDEmuInitDSIHandler
.L_80062C28:
	lwz r31, 0x8(r1)
	addi r1, r1, 0x10
	lwz r0, 0x4(r1)
	mtlr r0
	blr
.endfn EnableMetroTRKInterrupts

# .text:0x1DC | 0x80062C3C | size: 0x264
.fn SNDebugInit, global
	mflr r0
	stw r0, 0x4(r1)
	stwu r1, -0x10(r1)
	stw r31, 0x8(r1)
	lis r31, lbl_80278340+0x1020@h
	ori r31, r31, lbl_80278340+0x1020@l
	stw r3, 0x6cc(r31)
	li r5, 0x0
	stw r5, 0x6d0(r31)
	lis r4, bConnected+0x4@h
	ori r4, r4, bConnected+0x4@l
	cmpwi r3, 0x2
	beq .L_80062D00
	lis r4, bConnected+0x20@h
	ori r4, r4, bConnected+0x20@l
	cmpwi r3, 0x3
	beq .L_80062D00
	lis r4, bConnected+0x3C@h
	ori r4, r4, bConnected+0x3C@l
	cmpwi r3, 0x4
	bne .L_80062CEC
	li r5, 0x1
	stw r5, 0x6d0(r31)
	lis r3, WriteUARTN@h
	ori r3, r3, WriteUARTN@l
	li r5, 0x3
	stw r5, 0x0(r3)
	lis r5, 0x4e80
	ori r5, r5, 0x20
	stw r5, 0x4(r3)
	lis r3, InitializeUART@h
	ori r3, r3, InitializeUART@l
	lis r5, 0x3860
	stw r5, 0x0(r3)
	lis r5, 0x4e80
	ori r5, r5, 0x20
	stw r5, 0x4(r3)
	lis r3, 0x8000
	ori r3, r3, 0xf4
	lwz r3, 0x0(r3)
	subi r3, r3, 0x4
	lwz r3, 0x0(r3)
	bl SNDVDEmuInit
	b .L_80062D00
.L_80062CEC:
	li r3, 0x1
	addi r1, r1, 0x10
	lwz r0, 0x4(r1)
	mtlr r0
	blr
.L_80062D00:
	addi r3, r31, 0x694
	li r5, 0x1c
	bl fn_80062A60
	li r5, 0x0
	stw r5, 0x6bc(r31)
	addi r3, r31, 0x6b0
	stw r5, 0x0(r3)
	lis r4, DBcallback@h
	ori r4, r4, DBcallback@l
	lwz r0, 0x694(r31)
	mtlr r0
	blrl
	lwz r0, 0x6a8(r31)
	mtlr r0
	blrl
	lis r3, 0x4
	ori r3, r3, 0x1040
	li r4, 0x1440
	lis r5, SN_DSI@h
	ori r5, r5, SN_DSI@l
	lwz r5, 0x0(r5)
	cmpwi r5, 0x0
	beq .L_80062D78
	lis r5, g_hDVD@h
	ori r5, r5, g_hDVD@l
	lwz r5, 0x0(r5)
	cmpwi r5, 0x0
	bne .L_80062D78
	ori r3, r3, 0x4
	ori r4, r4, 0x4
.L_80062D78:
	lis r5, SN_ISI@h
	ori r5, r5, SN_ISI@l
	lwz r5, 0x0(r5)
	cmpwi r5, 0x0
	beq .L_80062D94
	ori r3, r3, 0x8
	ori r4, r4, 0x8
.L_80062D94:
	lis r5, SN_ALIGNMENT@h
	ori r5, r5, SN_ALIGNMENT@l
	lwz r5, 0x0(r5)
	cmpwi r5, 0x0
	beq .L_80062DB0
	ori r3, r3, 0x20
	ori r4, r4, 0x20
.L_80062DB0:
	bl .L_800641D0
	li r3, 0x0
	stw r3, 0x688(r31)
	stw r3, 0x68c(r31)
	stw r3, 0x690(r31)
	lis r5, PPCHalt@h
	ori r5, r5, PPCHalt@l
	lis r3, bConnected+0x58@h
	ori r3, r3, bConnected+0x58@l
.L_80062DD4:
	lwz r4, 0x0(r3)
	cmpwi r4, -0x1
	beq .L_80062DF8
	lwz r6, 0x0(r5)
	cmplw r6, r4
	bne .L_80062E08
	addi r3, r3, 0x4
	addi r5, r5, 0x4
	b .L_80062DD4
.L_80062DF8:
	lis r3, PPCHalt@h
	ori r3, r3, PPCHalt@l
	li r31, 0x1
	stw r31, 0xc(r3)
.L_80062E08:
	bl snInitFileserver
	lis r3, WriteUARTN@h
	ori r3, r3, WriteUARTN@l
	li r4, 0x100
	bl DCFlushRange
	lis r3, InitializeUART@h
	ori r3, r3, InitializeUART@l
	li r4, 0x100
	bl DCFlushRange
	lis r3, PCinit@h
	ori r3, r3, PCinit@l
	li r4, 0x100
	bl DCFlushRange
	lis r3, PPCHalt@h
	ori r3, r3, PPCHalt@l
	li r4, 0x100
	bl DCFlushRange
	lis r3, WriteUARTN@h
	ori r3, r3, WriteUARTN@l
	li r4, 0x100
	bl ICInvalidateRange
	lis r3, InitializeUART@h
	ori r3, r3, InitializeUART@l
	li r4, 0x100
	bl ICInvalidateRange
	lis r3, PCinit@h
	ori r3, r3, PCinit@l
	li r4, 0x100
	bl ICInvalidateRange
	lis r3, PPCHalt@h
	ori r3, r3, PPCHalt@l
	li r4, 0x100
	bl ICInvalidateRange
	lwz r31, 0x8(r1)
	addi r1, r1, 0x10
	lwz r0, 0x4(r1)
	mtlr r0
	blr
.endfn SNDebugInit

# .text:0x440 | 0x80062EA0 | size: 0x128
.fn SNDebugBoot, global
	lis r3, lbl_80278340+0x1020@h
	ori r3, r3, lbl_80278340+0x1020@l
	li r4, 0x1
	stw r4, 0x6d4(r3)
	blr
.L_80062EB4:
	li r3, 0x0
	stw r3, 0x6b4(r31)
.L_80062EBC:
	lwz r0, 0x69c(r31)
	mtlr r0
	blrl
	cmpwi r3, 0x0
	beq .L_80062EBC
	stw r3, 0x6b4(r31)
	cmplwi r3, 0x8008
	ble .L_80062EE4
	lis r3, 0x0
	ori r3, r3, 0x8008
.L_80062EE4:
	mr r4, r3
	stw r4, 0x6b8(r31)
	addi r3, r31, 0x700
	lwz r0, 0x6a0(r31)
	mtlr r0
	blrl
	cmpwi r3, 0x0
	beq .L_80062F28
	lis r3, 0x8025
	ori r3, r3, 0x3b6c
	lwz r4, 0x6d0(r31)
	cmpwi r4, 0x0
	bne .L_80062F20
	bl OSReport
	b .L_80062F90
.L_80062F20:
	bl proviewtty_800637BC
	b .L_80062F90
.L_80062F28:
	lwz r3, 0x6b4(r31)
	lwz r4, 0x6b8(r31)
	subf r3, r4, r3
	stw r3, 0x6b4(r31)
	lbz r3, 0x700(r31)
	cmplwi r3, 0x14
	blt .L_80062F68
	lis r3, 0x8025
	ori r3, r3, 0x3b85
	lwz r4, 0x6d0(r31)
	cmpwi r4, 0x0
	bne .L_80062F60
	bl OSReport
	b .L_80062F90
.L_80062F60:
	bl proviewtty_800637BC
	b .L_80062F90
.L_80062F68:
	lis r4, 0x8025
	ori r4, r4, 0x3a8c
	li r5, 0x1
	stw r5, 0x0(r4)
	slwi r3, r3, 2
	lis r4, 0x8025
	ori r4, r4, 0x3b00
	lwzx r5, r3, r4
	mtlr r5
	blrl
.L_80062F90:
	lwz r5, 0x6bc(r31)
	cmpwi r5, 0x0
	beq .L_80062EB4
	b cmdGo
.L_80062FA0:
	lis r3, 0x8025
	ori r3, r3, 0x3be2
	lwz r4, 0x6d0(r31)
	cmpwi r4, 0x0
	bne .L_80062FBC
	bl OSReport
	b .L_80062FC0
.L_80062FBC:
	bl proviewtty_800637BC
.L_80062FC0:
	mtlr r30
	blr
.endfn SNDebugBoot

# .text:0x568 | 0x80062FC8 | size: 0xD4
.fn cmdNop, global
	blr
.L_80062FCC:
	mflr r30
	addi r3, r31, 0x700
	li r4, 0x8
	lwz r0, 0x6a4(r31)
	mtlr r0
	blrl
	lis r3, 0x8025
	ori r3, r3, 0x3b50
	li r4, 0x8
	lwz r0, 0x6a4(r31)
	mtlr r0
	blrl
	li r5, 0x0
	stw r5, 0x6bc(r31)
	mtlr r30
	blr
.L_8006300C:
	mflr r30
	li r6, 0x704
	lwbrx r3, r6, r31
	addi r4, r31, 0x0
	add r3, r3, r4
	addi r4, r31, 0x708
	li r6, 0x702
	lhbrx r5, r6, r31
	bl fn_80062A60
	addi r3, r31, 0x700
	li r4, 0x8
	lwz r0, 0x6a4(r31)
	mtlr r0
	blrl
	mtlr r30
	blr
.L_8006304C:
	mflr r30
	li r6, 0x704
	lwbrx r4, r6, r31
	addi r5, r31, 0x0
	add r4, r4, r5
	addi r3, r31, 0x708
	li r6, 0x702
	lhbrx r5, r6, r31
	mr r29, r5
	bl fn_80062A60
	addi r3, r31, 0x700
	mr r4, r29
	addi r4, r4, 0x8
	lwz r0, 0x6a4(r31)
	mtlr r0
	blrl
	cmpwi r3, 0x0
	bne .L_80062FA0
	mtlr r30
	blr
.endfn cmdNop

# .text:0x63C | 0x8006309C | size: 0x9C
.fn cmdRecvMem, global
	mflr r30
	li r6, 0x704
	lwbrx r3, r6, r31
	addi r4, r31, 0x708
	li r6, 0x702
	lhbrx r5, r6, r31
	lwz r7, 0x390(r31)
	andis. r7, r7, 0x1000
	bne .L_800630F0
	lis r7, 0xe000
	cmplw r4, r7
	blt .L_800630F0
	ori r7, r7, 0x3fff
	cmplw r4, r7
	bgt .L_800630F0
	li r6, 0x702
	li r4, 0x0
	sthbrx r4, r6, r31
	addi r3, r31, 0x700
	li r4, 0x8
	b .L_80063120
.L_800630F0:
	mr r28, r3
	mr r29, r5
	bl fn_80062A60
	cmplwi r3, 0x1
	beq .L_80063104
.L_80063104:
	mr r3, r28
	mr r4, r29
	bl DCFlushRange
	mr r3, r28
	mr r4, r29
	bl ICInvalidateRange
	addi r3, r31, 0x700
.L_80063120:
	li r4, 0x8
	lwz r0, 0x6a4(r31)
	mtlr r0
	blrl
	mtlr r30
	blr
.endfn cmdRecvMem

# .text:0x6D8 | 0x80063138 | size: 0x1DC
.fn cmdSendMem, global
	mflr r30
	li r6, 0x702
	lis r3, 0x0
	ori r3, r3, 0x8000
	lhbrx r5, r6, r31
	cmplw r5, r3
	ble .L_8006315C
	mr r5, r3
	stwbrx r5, r6, r31
.L_8006315C:
	mr r29, r5
	addi r3, r31, 0x708
	li r6, 0x704
	lwbrx r4, r6, r31
	lwz r7, 0x390(r31)
	andis. r7, r7, 0x1000
	bne .L_800631D4
	lis r7, 0xe000
	cmplw r4, r7
	blt .L_800631A8
	ori r7, r7, 0x3fff
	cmplw r4, r7
	bgt .L_800631A8
	li r6, 0x702
	li r4, 0x0
	sthbrx r4, r6, r31
	addi r3, r31, 0x700
	li r4, 0x8
	b .L_8006320C
.L_800631A8:
	bl fn_80062A60
	cmplwi r3, 0x1
	beq .L_80063200
	addi r6, r31, 0x708
	mr r3, r29
	li r5, 0xaa
.L_800631C0:
	subi r3, r3, 0x1
	stbx r5, r3, r6
	cmpwi r3, 0x0
	bgt .L_800631C0
	b .L_80063200
.L_800631D4:
	mr r28, r4
	bl fn_80062A60
	lis r7, 0xe000
	cmplw r4, r7
	blt .L_800631F4
	ori r7, r7, 0x3fff
	cmplw r4, r7
	ble .L_80063200
.L_800631F4:
	mr r3, r28
	mr r4, r29
	bl DCFlushRange
.L_80063200:
	addi r3, r31, 0x700
	mr r4, r29
	addi r4, r4, 0x8
.L_8006320C:
	lwz r0, 0x6a4(r31)
	mtlr r0
	blrl
	cmpwi r3, 0x0
	bne .L_80062FA0
	mtlr r30
	blr
.L_80063228:
	mflr r30
	lwz r3, 0x6bc(r31)
	stw r3, 0x708(r31)
	lwz r5, 0x6d4(r31)
	li r6, 0x704
	stwbrx r5, r6, r31
	li r4, 0x4
	li r6, 0x702
	sthbrx r4, r6, r31
	addi r3, r31, 0x700
	addi r4, r4, 0x8
	lwz r0, 0x6a4(r31)
	mtlr r0
	blrl
	cmpwi r3, 0x0
	bne .L_80062FA0
	mtlr r30
	blr
.L_80063270:
	mflr r30
	li r4, 0x0
	addi r5, r31, 0x70c
	lis r6, 0x8000
	lwz r6, 0x30c8(r6)
	cmpwi r6, 0x0
	beq .L_800632D4
	li r7, 0x0
	li r8, 0x4
	li r9, 0x8
	li r10, 0xc
.L_8006329C:
	addi r4, r4, 0x1
	stwbrx r6, r7, r5
	lwz r3, 0x0(r6)
	stwbrx r3, r8, r5
	lwz r3, 0x14(r6)
	stwbrx r3, r9, r5
	lwz r3, 0x18(r6)
	stwbrx r3, r10, r5
	addi r5, r5, 0x10
	cmpwi r4, 0x40
	beq .L_800632D4
	lwz r6, 0x4(r6)
	cmpwi r6, 0x0
	bne .L_8006329C
.L_800632D4:
	addi r3, r31, 0x708
	li r6, 0x0
	stwbrx r4, r6, r3
	li r6, 0x702
	subf r4, r3, r5
	stwbrx r4, r6, r31
	addi r4, r4, 0x8
	addi r3, r31, 0x700
	lwz r0, 0x6a4(r31)
	mtlr r0
	blrl
	cmpwi r3, 0x0
	bne .L_80062FA0
	mtlr r30
	blr
.L_80063310:
	mflr r30
.endfn cmdSendMem

# .text:0x8B4 | 0x80063314 | size: 0x124
.fn cmdThreadList, global
	li r4, 0x0
	addi r5, r31, 0x70c
	lis r6, 0x8000
	lwz r6, 0xdc(r6)
	cmpwi r6, 0x0
	beq .L_800633FC
	li r7, 0x0
	li r8, 0x4
	li r9, 0x8
	li r10, 0xc
	li r11, 0x10
	li r12, 0x14
	li r14, 0x18
	li r15, 0x1c
	li r16, 0x20
.L_80063350:
	addi r4, r4, 0x1
	stwbrx r6, r7, r5
	lwz r3, 0x2c8(r6)
	stwbrx r3, r8, r5
	lwz r3, 0x2cc(r6)
	stwbrx r3, r9, r5
	lwz r3, 0x2d0(r6)
	stwbrx r3, r10, r5
	lwz r3, 0x2d4(r6)
	stwbrx r3, r11, r5
	lwz r3, 0x2d8(r6)
	stwbrx r3, r12, r5
	lwz r3, 0x2f0(r6)
	stwbrx r3, r14, r5
	li r18, 0x0
	lwz r3, 0x2e8(r6)
	addi r17, r5, 0x24
.L_80063394:
	cmpwi r3, 0x0
	beq .L_800633AC
	stwbrx r3, r18, r17
	addi r18, r18, 0x4
	lwz r3, 0x2e0(r3)
	b .L_80063394
.L_800633AC:
	add r17, r17, r18
	srwi r18, r18, 2
	stwbrx r18, r15, r5
	li r18, 0x0
	lwz r3, 0x2f4(r6)
.L_800633C0:
	cmpwi r3, 0x0
	beq .L_800633D8
	stwbrx r3, r18, r17
	addi r18, r18, 0x4
	lwz r3, 0x10(r3)
	b .L_800633C0
.L_800633D8:
	add r17, r17, r18
	srwi r18, r18, 2
	stwbrx r18, r16, r5
	mr r5, r17
	cmpwi r4, 0x40
	beq .L_800633FC
	lwz r6, 0x2fc(r6)
	cmpwi r6, 0x0
	bne .L_80063350
.L_800633FC:
	addi r3, r31, 0x708
	li r6, 0x0
	stwbrx r4, r6, r3
	li r6, 0x702
	subf r4, r3, r5
	sthbrx r4, r6, r31
	addi r4, r4, 0x8
	addi r3, r31, 0x700
	lwz r0, 0x6a4(r31)
	mtlr r0
	blrl
	cmpwi r3, 0x0
	bne .L_80062FA0
	mtlr r30
	blr
.endfn cmdThreadList

# .text:0x9D8 | 0x80063438 | size: 0x50
.fn cmdReset, global
	lis r5, 0xcc00
	li r3, 0x4cc8
	li r4, 0x3
	stw r4, 0x3024(r5)
	stw r3, 0x3024(r5)
	nop
	b cmdReset
.L_80063454:
	lis r4, bConnected@h
	ori r4, r4, bConnected@l
	li r5, 0x0
	stw r5, 0x0(r4)
	blr
.L_80063468:
	mflr r30
	lbz r3, 0x707(r31)
	lis r4, 0x0
	ori r4, r4, 0x704
	lhbrx r4, r4, r31
	lwz r5, 0x708(r31)
	cmplwi r3, 0xff
	beq tunerprotocol
.endfn cmdReset

# .text:0xA28 | 0x80063488 | size: 0x180
.fn tunerprotocol, global
	mtlr r30
	blr
.L_80063490:
	lis r3, 0xcc00
	ori r3, r3, 0x6000
	lwz r4, 0x0(r3)
	andi. r4, r4, 0x14
	li r4, 0x0
	blr
.L_800634A8:
	lwz r4, 0x0(r3)
	andi. r4, r4, 0x14
	beq .L_800634A8
	stw r4, 0x0(r3)
	blr
.L_800634BC:
	mflr r30
	bl .L_80063490
	bne .L_8006351C
	lis r3, 0x8028
	ori r3, r3, 0x1b80
	li r4, 0x20
	bl DCInvalidateRange
	lis r3, 0xcc00
	ori r3, r3, 0x6000
	li r4, 0x14
	stw r4, 0x0(r3)
	lis r4, 0x1200
	stw r4, 0x8(r3)
	li r4, 0x0
	stw r4, 0xc(r3)
	stw r4, 0x10(r3)
	lis r4, 0x8028
	ori r4, r4, 0x1b80
	stw r4, 0x14(r3)
	li r4, 0x20
	stw r4, 0x18(r3)
	li r4, 0x3
	stw r4, 0x1c(r3)
	bl .L_800634A8
.L_8006351C:
	addi r3, r31, 0x708
	stw r4, 0x0(r3)
	lis r4, 0x8000
	ori r4, r4, 0x24
	lwzu r5, 0x4(r4)
	stwu r5, 0x4(r3)
	lwzu r5, 0x4(r4)
	stwu r5, 0x4(r3)
	addi r3, r3, 0x4
	lis r4, 0x8028
	ori r4, r4, 0x1b80
	li r5, 0x20
	bl fn_80062A60
	li r4, 0x2c
	li r5, 0x702
	sthbrx r4, r5, r31
	addi r3, r31, 0x700
	addi r4, r4, 0x8
	lwz r0, 0x6a4(r31)
	mtlr r0
	blrl
	cmpwi r3, 0x0
	bne .L_80062FA0
	mtlr r30
	blr
.L_80063580:
	mflr r30
	lbz r3, 0x701(r31)
	bl 0x31f4	# -> 0x8006677C (SNDVDEmuControl+0x0)
	sth r3, 0x708(r31)
	clrrwi. r3, r3, 31
	beq .L_800635A8
	mfspr r4, DABR
	stw r4, 0x450(r31)
	li r3, 0x0
	mtspr DABR, r3
.L_800635A8:
	li r4, 0x2
	li r5, 0x702
	sthbrx r4, r5, r31
	addi r3, r31, 0x700
	addi r4, r4, 0x8
	lwz r0, 0x6a4(r31)
	mtlr r0
	blrl
	cmpwi r3, 0x0
	bne .L_80062FA0
	mtlr r30
	blr
.L_800635D8:
	li r6, 0x704
	lwbrx r3, r6, r31
	stw r3, 0x370(r31)
	mfmsr r3
	rlwinm r3, r3, 0, 17, 15
	mtmsr r3
	lis r5, 0xcc00
	li r4, 0x0
	stw r4, 0x3004(r5)
	li r4, 0x0
	lis r3, 0xcc00
	sth r4, 0x2002(r3)
.endfn tunerprotocol

# .text:0xBA8 | 0x80063608 | size: 0x1B4
.fn cmdGo, global
	lwz r4, 0x6cc(r31)
	cmpwi r4, 0x3
	bne .L_80063620
	lis r5, 0xcc00
	li r4, 0x1000
	stw r4, 0x3000(r5)
.L_80063620:
	li r5, 0x0
	stw r5, 0x6bc(r31)
	bl .L_80063820
	bl .L_80063864
	lwz r5, 0x360(r31)
	mtcrf 255, r5
	lwz r5, 0x358(r31)
	mtlr r5
	lwz r5, 0x6c8(r31)
	lis r4, 0xcc00
	sth r5, 0x4010(r4)
	lis r5, lbl_80253A70@h
	ori r5, r5, lbl_80253A70@l
	li r4, 0x0
	stw r4, 0x0(r5)
	lwz r30, 0xf0(r31)
	lwz r29, 0xe8(r31)
	lwz r28, 0xe0(r31)
	lwz r27, 0xd8(r31)
	lwz r26, 0xd0(r31)
	lwz r25, 0xc8(r31)
	lwz r24, 0xc0(r31)
	lwz r23, 0xb8(r31)
	lwz r22, 0xb0(r31)
	lwz r21, 0xa8(r31)
	lwz r20, 0xa0(r31)
	lwz r19, 0x98(r31)
	lwz r18, 0x90(r31)
	lwz r17, 0x88(r31)
	lwz r16, 0x80(r31)
	lwz r15, 0x78(r31)
	lwz r14, 0x70(r31)
	lwz r13, 0x68(r31)
	lwz r12, 0x60(r31)
	lwz r11, 0x58(r31)
	lwz r10, 0x50(r31)
	lwz r9, 0x48(r31)
	lwz r8, 0x40(r31)
	lwz r7, 0x38(r31)
	lwz r6, 0x30(r31)
	lwz r5, 0x28(r31)
	lwz r4, 0x20(r31)
	lwz r2, 0x10(r31)
	lwz r3, 0x18(r31)
	lwz r1, 0x8(r31)
	lwz r0, 0x0(r31)
	lwz r31, 0xf8(r31)
	rfi
.L_800636E0:
	lwz r5, 0x20(r31)
	cmpwi r5, 0x0
	beq .L_80063778
	lis r3, SN_BUFFERED_TTY@h
	ori r3, r3, SN_BUFFERED_TTY@l
	lwz r3, 0x0(r3)
	cmpwi r3, 0x0
	beq .L_80063750
	cmpwi r5, 0x1
	bne .L_80063750
	lwz r3, 0x18(r31)
	lbz r3, 0x0(r3)
	cmpwi r3, 0xa
	beq .L_80063750
	cmpwi r3, 0xd
	beq .L_80063750
	lbz r5, 0x6d8(r31)
	add r4, r5, r31
	stb r3, 0x6d9(r4)
	addi r5, r5, 0x1
	stb r5, 0x6d8(r31)
	cmpwi r5, 0x10
	bne .L_80063778
	addi r4, r31, 0x6d9
	bl .L_800637E0
	li r5, 0x0
	stb r5, 0x6d8(r31)
	b .L_80063778
.L_80063750:
	lbz r5, 0x6d8(r31)
	cmpwi r5, 0x0
	beq .L_8006376C
	addi r4, r31, 0x6d9
	bl .L_800637E0
	li r5, 0x0
	stb r5, 0x6d8(r31)
.L_8006376C:
	lwz r4, 0x18(r31)
	lwz r5, 0x20(r31)
	bl .L_800637E0
.L_80063778:
	lwz r3, 0x370(r31)
	addi r3, r3, 0x4
	stw r3, 0x370(r31)
	li r3, 0x0
	stw r3, 0x18(r31)
	b cmdGo
.L_80063790:
	lbz r5, 0x6d8(r31)
	cmpwi r5, 0x0
	beq .L_800637AC
	addi r4, r31, 0x6d9
	bl .L_800637E0
	li r5, 0x0
	stb r5, 0x6d8(r31)
.L_800637AC:
	lwz r3, 0x370(r31)
	addi r3, r3, 0x4
	stw r3, 0x370(r31)
	b cmdGo
.endfn cmdGo

# .text:0xD5C | 0x800637BC | size: 0x9D4
.fn proviewtty_800637BC, global
	mr r5, r3
	lbz r0, 0x0(r5)
	cmpwi r0, 0x0
	beq .L_800637D8
.L_800637CC:
	lbzu r0, 0x1(r5)
	cmpwi r0, 0x0
	bne .L_800637CC
.L_800637D8:
	subf r5, r3, r5
	mr r4, r3
.L_800637E0:
	addi r3, r31, 0x708
	mr r28, r5
	mflr r29
	bl fn_80062A60
	mr r4, r28
	addi r3, r31, 0x700
	li r6, 0xd
	stb r6, 0x0(r3)
	li r6, 0x2
	sthbrx r4, r6, r3
	addi r4, r4, 0x8
	lwz r0, 0x6a4(r31)
	mtlr r0
	blrl
	mtlr r29
	blr
.L_80063820:
	lwz r5, 0x390(r31)
	mtspr HID2, r5
	lwz r5, 0x388(r31)
	mtspr HID1, r5
	lwz r5, 0x380(r31)
	mtspr HID0, r5
	lwz r5, 0x450(r31)
	mtspr DABR, r5
	lwz r5, 0x350(r31)
	mtctr r5
	lwz r5, 0x348(r31)
	mtxer r5
	lwz r5, 0x370(r31)
	mtsrr0 r5
	lwz r5, 0x378(r31)
	mtsrr1 r5
	blr
.L_80063864:
	mfmsr r5
	ori r5, r5, 0x900
	xori r5, r5, 0x900
	ori r5, r5, 0x2000
	mtmsr r5
	isync
	lfd f0, 0x200(r31)
	mtfsf 255, f0
	mfspr r5, HID2
	isync
	extrwi. r5, r5, 1, 2
	beq .L_80063960
	li r5, 0x0
	mtspr GQR0, r5
	isync
	psq_l f0, 0x248(r31), 0, qr0
	psq_l f1, 0x250(r31), 0, qr0
	psq_l f2, 0x258(r31), 0, qr0
	psq_l f3, 0x260(r31), 0, qr0
	psq_l f4, 0x268(r31), 0, qr0
	psq_l f5, 0x270(r31), 0, qr0
	psq_l f6, 0x278(r31), 0, qr0
	psq_l f7, 0x280(r31), 0, qr0
	psq_l f8, 0x288(r31), 0, qr0
	psq_l f9, 0x290(r31), 0, qr0
	psq_l f10, 0x298(r31), 0, qr0
	psq_l f11, 0x2a0(r31), 0, qr0
	psq_l f12, 0x2a8(r31), 0, qr0
	psq_l f13, 0x2b0(r31), 0, qr0
	psq_l f14, 0x2b8(r31), 0, qr0
	psq_l f15, 0x2c0(r31), 0, qr0
	psq_l f16, 0x2c8(r31), 0, qr0
	psq_l f17, 0x2d0(r31), 0, qr0
	psq_l f18, 0x2d8(r31), 0, qr0
	psq_l f19, 0x2e0(r31), 0, qr0
	psq_l f20, 0x2e8(r31), 0, qr0
	psq_l f21, 0x2f0(r31), 0, qr0
	psq_l f22, 0x2f8(r31), 0, qr0
	psq_l f23, 0x300(r31), 0, qr0
	psq_l f24, 0x308(r31), 0, qr0
	psq_l f25, 0x310(r31), 0, qr0
	psq_l f26, 0x318(r31), 0, qr0
	psq_l f27, 0x320(r31), 0, qr0
	psq_l f28, 0x328(r31), 0, qr0
	psq_l f29, 0x330(r31), 0, qr0
	psq_l f30, 0x338(r31), 0, qr0
	psq_l f31, 0x340(r31), 0, qr0
	lwz r5, 0x208(r31)
	mtspr GQR0, r5
	lwz r5, 0x210(r31)
	mtspr GQR1, r5
	lwz r5, 0x218(r31)
	mtspr GQR2, r5
	lwz r5, 0x220(r31)
	mtspr GQR3, r5
	lwz r5, 0x228(r31)
	mtspr GQR4, r5
	lwz r5, 0x230(r31)
	mtspr GQR5, r5
	lwz r5, 0x238(r31)
	mtspr GQR6, r5
	lwz r5, 0x240(r31)
	mtspr GQR7, r5
.L_80063960:
	lfd f0, 0x100(r31)
	lfd f1, 0x108(r31)
	lfd f2, 0x110(r31)
	lfd f3, 0x118(r31)
	lfd f4, 0x120(r31)
	lfd f5, 0x128(r31)
	lfd f6, 0x130(r31)
	lfd f7, 0x138(r31)
	lfd f8, 0x140(r31)
	lfd f9, 0x148(r31)
	lfd f10, 0x150(r31)
	lfd f11, 0x158(r31)
	lfd f12, 0x160(r31)
	lfd f13, 0x168(r31)
	lfd f14, 0x170(r31)
	lfd f15, 0x178(r31)
	lfd f16, 0x180(r31)
	lfd f17, 0x188(r31)
	lfd f18, 0x190(r31)
	lfd f19, 0x198(r31)
	lfd f20, 0x1a0(r31)
	lfd f21, 0x1a8(r31)
	lfd f22, 0x1b0(r31)
	lfd f23, 0x1b8(r31)
	lfd f24, 0x1c0(r31)
	lfd f25, 0x1c8(r31)
	lfd f26, 0x1d0(r31)
	lfd f27, 0x1d8(r31)
	lfd f28, 0x1e0(r31)
	lfd f29, 0x1e8(r31)
	lfd f30, 0x1f0(r31)
	lfd f31, 0x1f8(r31)
	lis r5, 0x8000
	lwz r3, 0xd4(r5)
	lwz r30, 0xd8(r5)
	cmpw r3, r30
	beq .L_80063A20
	mflr r29
	bl .L_800645E0
	cmpwi r30, 0x0
	beq .L_80063A1C
	mr r3, r30
	lhz r30, 0x1a2(r3)
	ori r4, r30, 0x1
	sth r4, 0x1a2(r3)
	bl .L_800644B8
	sth r30, 0x1a2(r3)
.L_80063A1C:
	mtlr r29
.L_80063A20:
	blr
.L_80063A24:
	mfmsr r5
	ori r5, r5, 0x900
	xori r5, r5, 0x900
	ori r5, r5, 0x2000
	mtmsr r5
	isync
	lis r5, 0x8000
	lwz r30, 0xd4(r5)
	lwz r3, 0xd8(r5)
	cmpw r3, r30
	beq .L_80063A7C
	mflr r29
	cmpwi r3, 0x0
	beq .L_80063A60
	bl .L_800645E0
.L_80063A60:
	mr r3, r30
	lhz r30, 0x1a2(r3)
	ori r4, r30, 0x1
	sth r4, 0x1a2(r3)
	bl .L_800644B8
	sth r30, 0x1a2(r3)
	mtlr r29
.L_80063A7C:
	stfd f0, 0x100(r31)
	stfd f1, 0x108(r31)
	stfd f2, 0x110(r31)
	stfd f3, 0x118(r31)
	stfd f4, 0x120(r31)
	stfd f5, 0x128(r31)
	stfd f6, 0x130(r31)
	stfd f7, 0x138(r31)
	stfd f8, 0x140(r31)
	stfd f9, 0x148(r31)
	stfd f10, 0x150(r31)
	stfd f11, 0x158(r31)
	stfd f12, 0x160(r31)
	stfd f13, 0x168(r31)
	stfd f14, 0x170(r31)
	stfd f15, 0x178(r31)
	stfd f16, 0x180(r31)
	stfd f17, 0x188(r31)
	stfd f18, 0x190(r31)
	stfd f19, 0x198(r31)
	stfd f20, 0x1a0(r31)
	stfd f21, 0x1a8(r31)
	stfd f22, 0x1b0(r31)
	stfd f23, 0x1b8(r31)
	stfd f24, 0x1c0(r31)
	stfd f25, 0x1c8(r31)
	stfd f26, 0x1d0(r31)
	stfd f27, 0x1d8(r31)
	stfd f28, 0x1e0(r31)
	stfd f29, 0x1e8(r31)
	stfd f30, 0x1f0(r31)
	stfd f31, 0x1f8(r31)
	mfspr r5, HID2
	isync
	extrwi. r5, r5, 1, 2
	beq .L_80063BD8
	mfspr r5, GQR0
	stw r5, 0x208(r31)
	mfspr r5, GQR1
	stw r5, 0x210(r31)
	mfspr r5, GQR2
	stw r5, 0x218(r31)
	mfspr r5, GQR3
	stw r5, 0x220(r31)
	mfspr r5, GQR4
	stw r5, 0x228(r31)
	mfspr r5, GQR5
	stw r5, 0x230(r31)
	mfspr r5, GQR6
	stw r5, 0x238(r31)
	mfspr r5, GQR7
	stw r5, 0x240(r31)
	li r5, 0x0
	mtspr GQR0, r5
	isync
	psq_st f0, 0x248(r31), 0, qr0
	psq_st f1, 0x250(r31), 0, qr0
	psq_st f2, 0x258(r31), 0, qr0
	psq_st f3, 0x260(r31), 0, qr0
	psq_st f4, 0x268(r31), 0, qr0
	psq_st f5, 0x270(r31), 0, qr0
	psq_st f6, 0x278(r31), 0, qr0
	psq_st f7, 0x280(r31), 0, qr0
	psq_st f8, 0x288(r31), 0, qr0
	psq_st f9, 0x290(r31), 0, qr0
	psq_st f10, 0x298(r31), 0, qr0
	psq_st f11, 0x2a0(r31), 0, qr0
	psq_st f12, 0x2a8(r31), 0, qr0
	psq_st f13, 0x2b0(r31), 0, qr0
	psq_st f14, 0x2b8(r31), 0, qr0
	psq_st f15, 0x2c0(r31), 0, qr0
	psq_st f16, 0x2c8(r31), 0, qr0
	psq_st f17, 0x2d0(r31), 0, qr0
	psq_st f18, 0x2d8(r31), 0, qr0
	psq_st f19, 0x2e0(r31), 0, qr0
	psq_st f20, 0x2e8(r31), 0, qr0
	psq_st f21, 0x2f0(r31), 0, qr0
	psq_st f22, 0x2f8(r31), 0, qr0
	psq_st f23, 0x300(r31), 0, qr0
	psq_st f24, 0x308(r31), 0, qr0
	psq_st f25, 0x310(r31), 0, qr0
	psq_st f26, 0x318(r31), 0, qr0
	psq_st f27, 0x320(r31), 0, qr0
	psq_st f28, 0x328(r31), 0, qr0
	psq_st f29, 0x330(r31), 0, qr0
	psq_st f30, 0x338(r31), 0, qr0
	psq_st f31, 0x340(r31), 0, qr0
.L_80063BD8:
	mffs f0
	stfd f0, 0x200(r31)
	blr
.L_80063BE4:
	mfxer r5
	stw r5, 0x348(r31)
	mfctr r5
	stw r5, 0x350(r31)
	mfspr r5, HID0
	stw r5, 0x380(r31)
	mfspr r5, HID1
	stw r5, 0x388(r31)
	mfibatu r5, 0
	stw r5, 0x398(r31)
	mfibatl r5, 0
	stw r5, 0x3a0(r31)
	mfibatu r5, 1
	stw r5, 0x3a8(r31)
	mfibatl r5, 1
	stw r5, 0x3b0(r31)
	mfibatu r5, 2
	stw r5, 0x3b8(r31)
	mfibatl r5, 2
	stw r5, 0x3c0(r31)
	mfibatu r5, 3
	stw r5, 0x3c8(r31)
	mfibatl r5, 3
	stw r5, 0x3d0(r31)
	mfdbatu r5, 0
	stw r5, 0x3d8(r31)
	mfdbatl r5, 0
	stw r5, 0x3e0(r31)
	mfdbatu r5, 1
	stw r5, 0x3e8(r31)
	mfdbatl r5, 1
	stw r5, 0x3f0(r31)
	mfdbatu r5, 2
	stw r5, 0x3f8(r31)
	mfdbatl r5, 2
	stw r5, 0x400(r31)
	mfdbatu r5, 3
	stw r5, 0x408(r31)
	mfdbatl r5, 3
	stw r5, 0x410(r31)
	mfsr r5, 0
	stw r5, 0x500(r31)
	mfsr r5, 1
	stw r5, 0x508(r31)
	mfsr r5, 2
	stw r5, 0x510(r31)
	mfsr r5, 3
	stw r5, 0x518(r31)
	mfsr r5, 4
	stw r5, 0x520(r31)
	mfsr r5, 5
	stw r5, 0x528(r31)
	mfsr r5, 6
	stw r5, 0x530(r31)
	mfsr r5, 7
	stw r5, 0x538(r31)
	mfsr r5, 8
	stw r5, 0x540(r31)
	mfsr r5, 9
	stw r5, 0x548(r31)
	mfsr r5, 10
	stw r5, 0x550(r31)
	mfsr r5, 11
	stw r5, 0x558(r31)
	mfsr r5, 12
	stw r5, 0x560(r31)
	mfsr r5, 13
	stw r5, 0x568(r31)
	mfsr r5, 14
	stw r5, 0x570(r31)
	mfsr r5, 15
	stw r5, 0x578(r31)
	mfsprg r5, 0
	stw r5, 0x418(r31)
	mfsprg r5, 1
	stw r5, 0x420(r31)
	mfsprg r5, 2
	stw r5, 0x428(r31)
	mfsprg r5, 3
	stw r5, 0x430(r31)
	mfdar r5
	stw r5, 0x438(r31)
	mfdsisr r5
	stw r5, 0x440(r31)
	mfear r5
	stw r5, 0x448(r31)
	mfspr r5, DABR
	stw r5, 0x450(r31)
	mfspr r5, 284
	stw r5, 0x458(r31)
	mfspr r5, 285
	stw r5, 0x460(r31)
	mfspr r5, L2CR
	stw r5, 0x468(r31)
	mfdec r5
	stw r5, 0x470(r31)
	mfspr r5, IABR
	stw r5, 0x478(r31)
	mfspr r5, PMC1
	stw r5, 0x480(r31)
	mfspr r5, PMC2
	stw r5, 0x488(r31)
	mfspr r5, PMC3
	stw r5, 0x490(r31)
	mfspr r5, PMC4
	stw r5, 0x498(r31)
	mfspr r5, SIA
	stw r5, 0x4a0(r31)
	mfspr r5, MMCR0
	stw r5, 0x4a8(r31)
	mfspr r5, MMCR1
	stw r5, 0x4b0(r31)
	mfspr r5, THRM1
	stw r5, 0x4b8(r31)
	mfspr r5, THRM2
	stw r5, 0x4c0(r31)
	mfspr r5, THRM3
	stw r5, 0x4c8(r31)
	mfspr r5, ICTC
	stw r5, 0x4d0(r31)
	mfsdr1 r5
	stw r5, 0x4d8(r31)
	mfspr r5, PVR
	stw r5, 0x4e0(r31)
	mfspr r5, DMA_L
	stw r5, 0x4f0(r31)
	mfspr r5, DMA_U
	stw r5, 0x4e8(r31)
	mfspr r5, WPAR
	stw r5, 0x4f8(r31)
	mfspr r5, HID2
	stw r5, 0x390(r31)
	blr
	stw r31, 0xdf8(r0)
	mfsrr0 r31
	stw r31, 0xdf4(r0)
	mfsrr1 r31
	stw r31, 0xdf0(r0)
	mfmsr r31
	ori r31, r31, 0x30
	mtsrr1 r31
	lis r31, 0x8006
	ori r31, r31, 0x3efc
	mtsrr0 r31
	mflr r31
	stw r31, 0xdec(r0)
	bl .L_80063E30
.L_80063E30:
	mflr r31
	stw r31, 0xde8(r0)
	rfi
	.4byte 0x00000000 /* invalid */
	.4byte 0x00000000 /* invalid */
	.4byte 0x00000000 /* invalid */
	.4byte 0x00000000 /* invalid */
	stw r31, 0xdf8(r0)
	mfsrr0 r31
	stw r31, 0xdf4(r0)
	mfcr r31
	mtsprg 0, r31
	mfxer r31
	mtsprg 1, r31
	mfsrr1 r31
	stw r31, 0xdf0(r0)
	extrwi r31, r31, 6, 10
	cmpwi r31, 0x10
	bne .L_80063EAC
	lis r31, 0x8025
	ori r31, r31, 0x3a88
	clrlwi r31, r31, 4
	lwz r31, 0x0(r31)
	cmpwi r31, 0x0
	bne .L_80063EAC
	mfsprg r31, 0
	mtcrf 255, r31
	mfsprg r31, 1
	mtxer r31
	lwz r31, 0xdf8(r0)
	b 0x21dbc0	# -> 0x80281A58 (lbl_80278340+0x9718)
.L_80063EAC:
	mfsprg r31, 0
	mtcrf 255, r31
	mfsprg r31, 1
	mtxer r31
	mfmsr r31
	ori r31, r31, 0x30
	mtsrr1 r31
	lis r31, 0x8006
	ori r31, r31, 0x3efc
	mtsrr0 r31
	mflr r31
	stw r31, 0xdec(r0)
	bl .L_80063EE0
.L_80063EE0:
	mflr r31
	stw r31, 0xde8(r0)
	rfi
	.4byte 0x00000000 /* invalid */
	.4byte 0x00000000 /* invalid */
	.4byte 0x00000000 /* invalid */
	.4byte 0x00000000 /* invalid */
	lis r31, 0x8027
	ori r31, r31, 0x9360
	stw r0, 0x0(r31)
	stw r1, 0x8(r31)
	stw r2, 0x10(r31)
	stw r3, 0x18(r31)
	stw r4, 0x20(r31)
	stw r5, 0x28(r31)
	stw r6, 0x30(r31)
	stw r7, 0x38(r31)
	stw r8, 0x40(r31)
	stw r9, 0x48(r31)
	stw r10, 0x50(r31)
	stw r11, 0x58(r31)
	stw r12, 0x60(r31)
	stw r13, 0x68(r31)
	stw r14, 0x70(r31)
	stw r15, 0x78(r31)
	stw r16, 0x80(r31)
	stw r17, 0x88(r31)
	stw r18, 0x90(r31)
	stw r19, 0x98(r31)
	stw r20, 0xa0(r31)
	stw r21, 0xa8(r31)
	stw r22, 0xb0(r31)
	stw r23, 0xb8(r31)
	stw r24, 0xc0(r31)
	stw r25, 0xc8(r31)
	stw r26, 0xd0(r31)
	stw r27, 0xd8(r31)
	stw r28, 0xe0(r31)
	stw r29, 0xe8(r31)
	stw r30, 0xf0(r31)
	lis r1, 0x8027
	ori r1, r1, 0x9340
	mfcr r5
	stw r5, 0x360(r31)
	mfmsr r5
	stw r5, 0x368(r31)
	bl .L_80063BE4
	bl .L_80063A24
	mfmsr r9
	xori r0, r9, 0x10
	mtmsr r0
	sync
	lwz r4, 0xdf8(r0)
	lwz r5, 0xdec(r0)
	lwz r6, 0xdf4(r0)
	lwz r7, 0xdf0(r0)
	lwz r8, 0xde8(r0)
	srwi r8, r8, 8
	mtmsr r9
	sync
.L_80063FD0:
	lis r2, 0x8032
	ori r2, r2, 0xbee0
	lis r13, 0x8031
	ori r13, r13, 0xbee0
	stw r4, 0xf8(r31)
	stw r5, 0x358(r31)
	stw r6, 0x370(r31)
	ori r7, r7, 0x600
	xori r7, r7, 0x600
	stw r7, 0x378(r31)
	stw r8, 0x580(r31)
	lis r4, 0xcc00
	lhz r5, 0x4010(r4)
	stw r5, 0x6c8(r31)
	li r5, 0xff
	sth r5, 0x4010(r4)
	li r4, 0x0
	mtspr DABR, r4
	cmpwi r8, 0xd
	bne .L_80064070
	lwz r4, 0x6f0(r31)
	cmpwi r4, 0x0
	beq .L_80064040
	li r4, 0x0
	stw r4, 0x6f0(r31)
	lwz r4, 0x6f4(r31)
	stw r4, 0x450(r31)
	b cmdGo
.L_80064040:
	lwz r4, 0x6c0(r31)
	cmpwi r4, 0x0
	beq .L_80064198
	lwz r4, 0x6c4(r31)
	add r5, r31, r4
	lwz r7, 0x0(r5)
	ori r7, r7, 0x600
	xori r7, r7, 0x600
	stw r7, 0x0(r5)
	li r4, 0x0
	stw r4, 0x6c0(r31)
	b .L_80064198
.L_80064070:
	bl 0x1f00	# -> 0x80065F50 (PCrwSyncFSACK+0xb8)
	lwz r8, 0x580(r31)
	cmpwi r8, 0x7
	bne .L_80064140
	lwz r4, 0x370(r31)
	lwz r3, 0x0(r4)
	cmpwi r3, 0x5f
	bgt checkexternal
	cmpwi r3, 0x50
	bge checkexternal
	cmpwi r3, 0x1
	bne .L_800640D4
	li r5, 0x1e
	stw r5, 0x580(r31)
	addi r4, r4, 0x4
	stw r4, 0x370(r31)
	lis r3, 0x8025
	ori r3, r3, 0x3ba5
	lwz r4, 0x6d0(r31)
	cmpwi r4, 0x0
	bne .L_800640CC
	bl 0x109b0	# -> 0x80074A54 (__OSContextInit+0x28)
	b .L_800640D0
.L_800640CC:
	bl proviewtty_800637BC
.L_800640D0:
	b checkexternal
.L_800640D4:
	cmpwi r3, 0x4
	beq .L_80063790
	cmpwi r3, 0x3
	beq .L_800636E0
	cmpwi r3, 0x2
	bne .L_80064114
	addi r4, r4, 0x4
	stw r4, 0x370(r31)
	lwz r0, 0x69c(r31)
	mtlr r0
	blrl
	cmpwi r3, 0x0
	beq cmdGo
	li r4, 0x1
	stw r4, 0x6bc(r31)
	b .L_80064198
.L_80064114:
	cmpwi r3, 0x10
	blt checkexternal
	cmpwi r3, 0x16
	ble .L_80064138
	lwz r4, 0x6cc(r31)
	cmpwi r4, 0x4
	bne checkexternal
	cmpwi r3, 0x1b
	bgt checkexternal
.L_80064138:
	andi. r3, r3, 0xf
	b 0x9a4	# -> 0x80064AC0 (snIsSNTDEV+0x618)
.L_80064140:
	cmpwi r8, 0x3
	bne checkexternal
	lwz r5, 0x440(r31)
	extrwi. r5, r5, 1, 9
	beq checkexternal
	bl .L_80064744
	cmpwi r3, 0x0
	beq .L_80064188
	lwz r3, 0x378(r31)
	ori r3, r3, 0x400
	stw r3, 0x378(r31)
	lwz r3, 0x450(r31)
	stw r3, 0x6f4(r31)
	li r3, 0x0
	stw r3, 0x450(r31)
	li r3, 0x1
	stw r3, 0x6f0(r31)
	b cmdGo
.L_80064188:
	li r5, 0x1f
	stw r5, 0x580(r31)
.endfn proviewtty_800637BC

# .text:0x1730 | 0x80064190 | size: 0x1D4
.fn checkexternal, global
	li r4, 0x0
	stw r4, 0x6bc(r31)
.L_80064198:
	lis r5, lbl_80253A70@h
	ori r5, r5, lbl_80253A70@l
	li r4, 0x1
	stw r4, 0x0(r5)
	lwz r4, 0x6bc(r31)
	cmpwi r4, 0x0
	bne .L_80062EB4
	lis r3, bConnected+0xC4@h
	ori r3, r3, bConnected+0xC4@l
	li r4, 0x8
	lwz r0, 0x6a4(r31)
	mtlr r0
	blrl
	b .L_80062EB4
.L_800641D0:
	mflr r0
	stw r0, 0x4(r1)
	stwu r1, -0x8(r1)
	lis r5, 0x8000
	stw r4, 0x44(r5)
	li r0, 0x1
	stw r0, 0x40(r5)
	rlwinm. r4, r3, 0, 29, 29
	beq .L_80064224
	lis r6, DSIentry@h
	ori r6, r6, DSIentry@l
	lis r4, 0x8000
	ori r4, r4, 0x300
	lis r5, 0x6000
	stw r5, 0x0(r4)
	addi r5, r4, 0x4
	subf r6, r5, r6
	lis r5, 0x4800
	or r6, r6, r5
	stw r6, 0x4(r4)
	rlwinm r3, r3, 0, 30, 28
.L_80064224:
	lis r5, 0x8000
	ori r5, r5, 0x100
	li r6, 0x1
.L_80064230:
	and. r4, r3, r6
	beq .L_80064258
	lis r7, proviewtty_800637BC+0x638@h
	ori r7, r7, proviewtty_800637BC+0x638@l
	subi r8, r5, 0x4
	addi r9, r7, 0x54
.L_80064248:
	lwzu r4, 0x4(r7)
	stwu r4, 0x4(r8)
	cmplw r7, r9
	blt .L_80064248
.L_80064258:
	addi r5, r5, 0x100
	slwi. r6, r6, 1
	bne .L_80064230
	lis r7, proviewtty_800637BC+0x68C@h
	ori r7, r7, proviewtty_800637BC+0x68C@l
	lis r8, 0x8000
	ori r8, r8, 0x6fc
	addi r9, r7, 0xb0
.L_80064278:
	lwzu r4, 0x4(r7)
	stwu r4, 0x4(r8)
	cmplw r7, r9
	blt .L_80064278
	lis r7, 0x8000
	ori r7, r7, 0x4fc
	lis r8, lbl_80278340+0x9724@h
	ori r8, r8, lbl_80278340+0x9724@l
	addi r9, r7, 0x104
.L_8006429C:
	lwzu r4, 0x4(r7)
	stwu r4, 0x4(r8)
	cmplw r7, r9
	blt .L_8006429C
	lis r3, 0x3860
	ori r3, r3, 0x4
	lis r4, lbl_80278340+0x9724@h
	ori r4, r4, lbl_80278340+0x9724@l
	addi r5, r4, 0x104
.L_800642C0:
	lwzu r6, 0x4(r4)
	cmplw r3, r6
	beq .L_800642F8
	cmplw r4, r5
	blt .L_800642C0
	lis r3, bConnected+0x130@h
	ori r3, r3, bConnected+0x130@l
	lwz r4, 0x6d0(r31)
	cmpwi r4, 0x0
	bne .L_800642F0
	bl OSReport
	b PPCHalt
.L_800642F0:
	bl proviewtty_800637BC
	b PPCHalt
.L_800642F8:
	subi r4, r4, 0x4
	lis r6, 0x3860
	ori r6, r6, 0x6
	stwu r6, 0x4(r4)
	lis r3, 0x8000
	ori r3, r3, 0x700
	lwz r4, 0x5c(r3)
	lis r5, proviewtty_800637BC+0x690@h
	ori r5, r5, proviewtty_800637BC+0x690@l
	subf r5, r3, r5
	add r4, r4, r5
	stw r4, 0x5c(r3)
	lis r3, 0x8000
	ori r3, r3, 0x100
	li r4, 0x2000
	bl DCFlushRange
	lis r3, 0x8000
	ori r3, r3, 0x100
	li r4, 0x2000
	bl ICInvalidateRange
	li r3, 0x0
	li r4, 0x2000
	bl ICInvalidateRange
	addi r1, r1, 0x8
	lwz r0, 0x4(r1)
	mtlr r0
	blr
.endfn checkexternal

# .text:0x1904 | 0x80064364 | size: 0x18
.fn ISIentry, global
	mtsprg 1, r31
	li r31, 0x4
	mtsprg 2, r31
	lis r31, lbl_80278340+0x1020@h
	ori r31, r31, lbl_80278340+0x1020@l
	b .L_800643F0
.endfn ISIentry

# .text:0x191C | 0x8006437C | size: 0x12C
.fn DSIentry, global
	mtsprg 1, r31
	mfsrr0 r31
	mtsprg 0, r31
	lis r31, DSIentry+0x1C@h
	ori r31, r31, DSIentry+0x1C@l
	mtsrr0 r31
	rfi
	mfcr r31
	mtsprg 3, r31
	lis r31, lbl_80278340+0x1020@h
	ori r31, r31, lbl_80278340+0x1020@l
	lwz r31, 0x6ec(r31)
	cmplwi r31, 0x0
	beq .L_800643D0
	mfsprg r31, 3
	mtcrf 255, r31
	lis r31, fn_80062A60+0x78@h
	ori r31, r31, fn_80062A60+0x78@l
	mtsrr0 r31
	mfsprg r31, 1
	rfi
.L_800643D0:
	mfsprg r31, 3
	mtcrf 255, r31
	mfsprg r31, 0
	mtsrr0 r31
	li r31, 0x3
	mtsprg 2, r31
	lis r31, lbl_80278340+0x1020@h
	ori r31, r31, lbl_80278340+0x1020@l
.L_800643F0:
	stw r0, 0x0(r31)
	stw r1, 0x8(r31)
	stw r2, 0x10(r31)
	stw r3, 0x18(r31)
	stw r4, 0x20(r31)
	stw r5, 0x28(r31)
	stw r6, 0x30(r31)
	stw r7, 0x38(r31)
	stw r8, 0x40(r31)
	stw r9, 0x48(r31)
	stw r10, 0x50(r31)
	stw r11, 0x58(r31)
	stw r12, 0x60(r31)
	stw r13, 0x68(r31)
	stw r14, 0x70(r31)
	stw r15, 0x78(r31)
	stw r16, 0x80(r31)
	stw r17, 0x88(r31)
	stw r18, 0x90(r31)
	stw r19, 0x98(r31)
	stw r20, 0xa0(r31)
	stw r21, 0xa8(r31)
	stw r22, 0xb0(r31)
	stw r23, 0xb8(r31)
	stw r24, 0xc0(r31)
	stw r25, 0xc8(r31)
	stw r26, 0xd0(r31)
	stw r27, 0xd8(r31)
	stw r28, 0xe0(r31)
	stw r29, 0xe8(r31)
	stw r30, 0xf0(r31)
	lis r1, lbl_80278340+0x1000@h
	ori r1, r1, lbl_80278340+0x1000@l
	mflr r28
	mfcr r5
	stw r5, 0x360(r31)
	mfmsr r5
	stw r5, 0x368(r31)
	bl .L_80063BE4
	bl .L_80063A24
	mfsprg r4, 1
	mr r5, r28
	mfsrr0 r6
	mfsrr1 r7
	mfsprg r8, 2
	b .L_80063FD0
.endfn DSIentry

# .text:0x1A48 | 0x800644A8 | size: 0x638
.fn snIsSNTDEV, global
	lis r3, lbl_80278340+0x1020@h
	ori r3, r3, lbl_80278340+0x1020@l
	lwz r3, 0x6d0(r3)
	blr
.L_800644B8:
	mr r4, r3
	lhz r5, 0x1a2(r4)
	clrlwi. r5, r5, 31
	beq .L_800645DC
	lfd f0, 0x190(r4)
	mtfsf 255, f0
	mfspr r5, HID2
	extrwi. r5, r5, 1, 2
	beq .L_8006455C
	psq_l f0, 0x1c8(r4), 0, qr0
	psq_l f1, 0x1d0(r4), 0, qr0
	psq_l f2, 0x1d8(r4), 0, qr0
	psq_l f3, 0x1e0(r4), 0, qr0
	psq_l f4, 0x1e8(r4), 0, qr0
	psq_l f5, 0x1f0(r4), 0, qr0
	psq_l f6, 0x1f8(r4), 0, qr0
	psq_l f7, 0x200(r4), 0, qr0
	psq_l f8, 0x208(r4), 0, qr0
	psq_l f9, 0x210(r4), 0, qr0
	psq_l f10, 0x218(r4), 0, qr0
	psq_l f11, 0x220(r4), 0, qr0
	psq_l f12, 0x228(r4), 0, qr0
	psq_l f13, 0x230(r4), 0, qr0
	psq_l f14, 0x238(r4), 0, qr0
	psq_l f15, 0x240(r4), 0, qr0
	psq_l f16, 0x248(r4), 0, qr0
	psq_l f17, 0x250(r4), 0, qr0
	psq_l f18, 0x258(r4), 0, qr0
	psq_l f19, 0x260(r4), 0, qr0
	psq_l f20, 0x268(r4), 0, qr0
	psq_l f21, 0x270(r4), 0, qr0
	psq_l f22, 0x278(r4), 0, qr0
	psq_l f23, 0x280(r4), 0, qr0
	psq_l f24, 0x288(r4), 0, qr0
	psq_l f25, 0x290(r4), 0, qr0
	psq_l f26, 0x298(r4), 0, qr0
	psq_l f27, 0x2a0(r4), 0, qr0
	psq_l f28, 0x2a8(r4), 0, qr0
	psq_l f29, 0x2b0(r4), 0, qr0
	psq_l f30, 0x2b8(r4), 0, qr0
	psq_l f31, 0x2c0(r4), 0, qr0
.L_8006455C:
	lfd f0, 0x90(r4)
	lfd f1, 0x98(r4)
	lfd f2, 0xa0(r4)
	lfd f3, 0xa8(r4)
	lfd f4, 0xb0(r4)
	lfd f5, 0xb8(r4)
	lfd f6, 0xc0(r4)
	lfd f7, 0xc8(r4)
	lfd f8, 0xd0(r4)
	lfd f9, 0xd8(r4)
	lfd f10, 0xe0(r4)
	lfd f11, 0xe8(r4)
	lfd f12, 0xf0(r4)
	lfd f13, 0xf8(r4)
	lfd f14, 0x100(r4)
	lfd f15, 0x108(r4)
	lfd f16, 0x110(r4)
	lfd f17, 0x118(r4)
	lfd f18, 0x120(r4)
	lfd f19, 0x128(r4)
	lfd f20, 0x130(r4)
	lfd f21, 0x138(r4)
	lfd f22, 0x140(r4)
	lfd f23, 0x148(r4)
	lfd f24, 0x150(r4)
	lfd f25, 0x158(r4)
	lfd f26, 0x160(r4)
	lfd f27, 0x168(r4)
	lfd f28, 0x170(r4)
	lfd f29, 0x178(r4)
	lfd f30, 0x180(r4)
	lfd f31, 0x188(r4)
.L_800645DC:
	blr
.L_800645E0:
	mr r5, r3
	lhz r3, 0x1a2(r5)
	ori r3, r3, 0x1
	sth r3, 0x1a2(r5)
	stfd f0, 0x90(r5)
	stfd f1, 0x98(r5)
	stfd f2, 0xa0(r5)
	stfd f3, 0xa8(r5)
	stfd f4, 0xb0(r5)
	stfd f5, 0xb8(r5)
	stfd f6, 0xc0(r5)
	stfd f7, 0xc8(r5)
	stfd f8, 0xd0(r5)
	stfd f9, 0xd8(r5)
	stfd f10, 0xe0(r5)
	stfd f11, 0xe8(r5)
	stfd f12, 0xf0(r5)
	stfd f13, 0xf8(r5)
	stfd f14, 0x100(r5)
	stfd f15, 0x108(r5)
	stfd f16, 0x110(r5)
	stfd f17, 0x118(r5)
	stfd f18, 0x120(r5)
	stfd f19, 0x128(r5)
	stfd f20, 0x130(r5)
	stfd f21, 0x138(r5)
	stfd f22, 0x140(r5)
	stfd f23, 0x148(r5)
	stfd f24, 0x150(r5)
	stfd f25, 0x158(r5)
	stfd f26, 0x160(r5)
	stfd f27, 0x168(r5)
	stfd f28, 0x170(r5)
	stfd f29, 0x178(r5)
	stfd f30, 0x180(r5)
	stfd f31, 0x188(r5)
	mffs f0
	stfd f0, 0x190(r5)
	lfd f0, 0x90(r5)
	mfspr r3, HID2
	extrwi. r3, r3, 1, 2
	beq .L_80064708
	psq_st f0, 0x1c8(r5), 0, qr0
	psq_st f1, 0x1d0(r5), 0, qr0
	psq_st f2, 0x1d8(r5), 0, qr0
	psq_st f3, 0x1e0(r5), 0, qr0
	psq_st f4, 0x1e8(r5), 0, qr0
	psq_st f5, 0x1f0(r5), 0, qr0
	psq_st f6, 0x1f8(r5), 0, qr0
	psq_st f7, 0x200(r5), 0, qr0
	psq_st f8, 0x208(r5), 0, qr0
	psq_st f9, 0x210(r5), 0, qr0
	psq_st f10, 0x218(r5), 0, qr0
	psq_st f11, 0x220(r5), 0, qr0
	psq_st f12, 0x228(r5), 0, qr0
	psq_st f13, 0x230(r5), 0, qr0
	psq_st f14, 0x238(r5), 0, qr0
	psq_st f15, 0x240(r5), 0, qr0
	psq_st f16, 0x248(r5), 0, qr0
	psq_st f17, 0x250(r5), 0, qr0
	psq_st f18, 0x258(r5), 0, qr0
	psq_st f19, 0x260(r5), 0, qr0
	psq_st f20, 0x268(r5), 0, qr0
	psq_st f21, 0x270(r5), 0, qr0
	psq_st f22, 0x278(r5), 0, qr0
	psq_st f23, 0x280(r5), 0, qr0
	psq_st f24, 0x288(r5), 0, qr0
	psq_st f25, 0x290(r5), 0, qr0
	psq_st f26, 0x298(r5), 0, qr0
	psq_st f27, 0x2a0(r5), 0, qr0
	psq_st f28, 0x2a8(r5), 0, qr0
	psq_st f29, 0x2b0(r5), 0, qr0
	psq_st f30, 0x2b8(r5), 0, qr0
	psq_st f31, 0x2c0(r5), 0, qr0
.L_80064708:
	blr
.L_8006470C:
	li r12, 0xff
	li r0, 0xf0
	cmpwi r10, 0x0
	beq .L_80064734
	li r12, 0xf0
	li r0, 0xc0
	andi. r10, r10, 0x1
	bne .L_80064734
	li r12, 0xc0
	li r0, 0x80
.L_80064734:
	extrwi. r10, r4, 1, 16
	beq .L_80064740
	mr r12, r0
.L_80064740:
	blr
.L_80064744:
	mflr r30
	lbz r3, 0x4e4(r31)
	cmpwi r3, 0x0
	beq .L_80064AD0
	xori r3, r3, 0xff
	lwz r4, 0x370(r31)
	lwz r4, 0x0(r4)
	srwi r5, r4, 26
	extrwi r6, r4, 10, 21
	lwz r11, 0x438(r31)
	lwz r8, 0x450(r31)
	clrrwi r8, r8, 3
	subf r7, r8, r11
	extrwi. r9, r4, 5, 11
	bne .L_80064788
	li r9, 0x0
	b .L_80064790
.L_80064788:
	slwi r9, r9, 3
	lwzx r9, r9, r31
.L_80064790:
	extrwi r10, r4, 5, 16
	slwi r10, r10, 3
	lwzx r10, r10, r31
	cmpwi r5, 0x4
	beq .L_800648C4
	cmpwi r5, 0x1f
	beq .L_8006490C
	extsh r8, r4
	add r8, r8, r9
	cmpwi r5, 0x22
	beq .L_80064A94
	cmpwi r5, 0x23
	beq .L_80064A94
	cmpwi r5, 0x26
	beq .L_80064A94
	cmpwi r5, 0x27
	beq .L_80064A94
	cmpwi r5, 0x2a
	beq .L_80064A9C
	cmpwi r5, 0x2b
	beq .L_80064A9C
	cmpwi r5, 0x28
	beq .L_80064A9C
	cmpwi r5, 0x29
	beq .L_80064A9C
	cmpwi r5, 0x2c
	beq .L_80064A9C
	cmpwi r5, 0x2d
	beq .L_80064A9C
	cmpwi r5, 0x20
	beq .L_80064AA4
	cmpwi r5, 0x21
	beq .L_80064AA4
	cmpwi r5, 0x24
	beq .L_80064AA4
	cmpwi r5, 0x25
	beq .L_80064AA4
	cmpwi r5, 0x30
	beq .L_80064AA4
	cmpwi r5, 0x31
	beq .L_80064AA4
	cmpwi r5, 0x34
	beq .L_80064AA4
	cmpwi r5, 0x35
	beq .L_80064AA4
	cmpwi r5, 0x32
	beq .L_80064AAC
	cmpwi r5, 0x33
	beq .L_80064AAC
	cmpwi r5, 0x36
	beq .L_80064AAC
	cmpwi r5, 0x37
	beq .L_80064AAC
	cmpwi r5, 0x2e
	beq .L_80064A68
	cmpwi r5, 0x2f
	beq .L_80064A68
	slwi r8, r4, 20
	srawi r8, r8, 20
	add r8, r8, r9
	extrwi r9, r4, 3, 17
	slwi r9, r9, 3
	addi r9, r9, 0x208
	lwzx r9, r9, r31
	extrwi r10, r9, 3, 13
	bl .L_8006470C
	cmpwi r5, 0x38
	beq .L_80064AB0
	cmpwi r5, 0x39
	beq .L_80064AB0
	clrlwi r11, r9, 29
	bl .L_8006470C
	cmpwi r5, 0x3c
	beq .L_80064AB0
	cmpwi r5, 0x3d
	beq .L_80064AB0
	b .L_80064AD0
.L_800648C4:
	add r8, r9, r10
	extrwi r9, r4, 3, 17
	slwi r9, r9, 3
	addi r9, r9, 0x208
	lwzx r9, r9, r31
	extrwi r10, r9, 3, 13
	bl .L_8006470C
	cmpwi r6, 0x6
	beq .L_80064AB0
	cmpwi r6, 0x26
	beq .L_80064AB0
	clrlwi r11, r9, 29
	bl .L_8006470C
	cmpwi r6, 0x7
	beq .L_80064AB0
	cmpwi r6, 0x27
	beq .L_80064AB0
	b .L_80064AD0
.L_8006490C:
	add r8, r9, r10
	cmpwi r6, 0x77
	beq .L_80064A94
	cmpwi r6, 0x57
	beq .L_80064A94
	cmpwi r6, 0xf7
	beq .L_80064A94
	cmpwi r6, 0xd7
	beq .L_80064A94
	cmpwi r6, 0x177
	beq .L_80064A9C
	cmpwi r6, 0x157
	beq .L_80064A9C
	cmpwi r6, 0x137
	beq .L_80064A9C
	cmpwi r6, 0x117
	beq .L_80064A9C
	cmpwi r6, 0x1b7
	beq .L_80064A9C
	cmpwi r6, 0x197
	beq .L_80064A9C
	cmpwi r6, 0x316
	beq .L_80064A9C
	cmpwi r6, 0x396
	beq .L_80064A9C
	cmpwi r6, 0x37
	beq .L_80064AA4
	cmpwi r6, 0x17
	beq .L_80064AA4
	cmpwi r6, 0xb7
	beq .L_80064AA4
	cmpwi r6, 0x97
	beq .L_80064AA4
	cmpwi r6, 0x216
	beq .L_80064AA4
	cmpwi r6, 0x296
	beq .L_80064AA4
	cmpwi r6, 0x237
	beq .L_80064AA4
	cmpwi r6, 0x217
	beq .L_80064AA4
	cmpwi r6, 0x2b7
	beq .L_80064AA4
	cmpwi r6, 0x297
	beq .L_80064AA4
	cmpwi r6, 0x3d7
	beq .L_80064AA4
	cmpwi r6, 0x14
	beq .L_80064AA4
	cmpwi r6, 0x96
	beq .L_80064AA4
	cmpwi r6, 0x277
	beq .L_80064AAC
	cmpwi r6, 0x257
	beq .L_80064AAC
	cmpwi r6, 0x2f7
	beq .L_80064AAC
	cmpwi r6, 0x2d7
	beq .L_80064AAC
	cmpwi r6, 0x215
	beq .L_80064A30
	cmpwi r6, 0x295
	beq .L_80064A30
	mr r8, r9
	cmpwi r6, 0x255
	beq .L_80064A20
	cmpwi r6, 0x2d5
	beq .L_80064A20
	b .L_80064AD0
.L_80064A20:
	extrwi. r4, r4, 5, 16
	bne .L_80064A38
	li r4, 0x20
	b .L_80064A38
.L_80064A30:
	lwz r4, 0x348(r31)
	clrlwi r5, r4, 25
.L_80064A38:
	subf. r5, r8, r11
	beq .L_80064A48
	subf r4, r5, r4
	mr r8, r11
.L_80064A48:
	li r12, 0xff
	cmpwi r4, 0x8
	bge .L_80064AB0
	li r5, 0x8
	subf r4, r4, r5
	slw r12, r12, r4
	andi. r12, r12, 0xff
	b .L_80064AB0
.L_80064A68:
	subf. r5, r8, r11
	beq .L_80064AAC
	mr r8, r11
	extrwi r6, r4, 5, 6
	li r4, 0x20
	subf r4, r6, r4
	slwi r4, r4, 2
	subf r4, r5, r4
	cmpwi r4, 0x4
	beq .L_80064AA4
	b .L_80064AAC
.L_80064A94:
	li r12, 0x80
	b .L_80064AB0
.L_80064A9C:
	li r12, 0xc0
	b .L_80064AB0
.L_80064AA4:
	li r12, 0xf0
	b .L_80064AB0
.L_80064AAC:
	li r12, 0xff
.L_80064AB0:
	subf r8, r8, r11
	slw r4, r12, r8
	srw r4, r4, r7
	and. r3, r3, r4
	bne .L_80064AD0
	li r3, 0x1
	mtlr r30
	blr
.L_80064AD0:
	li r3, 0x0
	mtlr r30
	blr
	.4byte 0x00000000 /* invalid */
.endfn snIsSNTDEV

# 0x80253A70..0x80253C00 | size: 0x190
.data
.balign 8

# .data:0x0 | 0x80253A70 | size: 0x8
.obj lbl_80253A70, global
	.4byte 0x00000001
	.4byte 0x00000000
.endobj lbl_80253A70

# .data:0x8 | 0x80253A78 | size: 0x4
.obj SN_ISI, global
	.4byte 0x00000001
.endobj SN_ISI

# .data:0xC | 0x80253A7C | size: 0x4
.obj SN_DSI, global
	.4byte 0x00000001
.endobj SN_DSI

# .data:0x10 | 0x80253A80 | size: 0x4
.obj SN_ALIGNMENT, global
	.4byte 0x00000001
.endobj SN_ALIGNMENT

# .data:0x14 | 0x80253A84 | size: 0x4
.obj SN_BUFFERED_TTY, global
	.4byte 0x00000001
.endobj SN_BUFFERED_TTY

# .data:0x18 | 0x80253A88 | size: 0x4
.obj SN_FPE, global
	.4byte 0x00000000
.endobj SN_FPE

# .data:0x1C | 0x80253A8C | size: 0x174
.obj bConnected, global
	.4byte 0x00000000
	.4byte EXI2_Init
	.4byte EXI2_EnableInterrupts
	.4byte EXI2_Poll
	.4byte EXI2_ReadN
	.4byte EXI2_WriteN
	.4byte EXI2_Reserve
	.4byte EXI2_Unreserve
	.4byte DBInitComm
	.4byte DBInitInterrupts
	.4byte DBQueryData
	.4byte DBRead
	.4byte DBWrite
	.4byte DBOpen
	.4byte DBClose
	.4byte SNInitComm
	.4byte SNInitInterrupts
	.4byte SNQueryData
	.4byte SNRead
	.4byte SNWrite
	.4byte SNOpen
	.4byte SNClose
	.4byte 0x7C0004AC
	.4byte 0x60000000
	.4byte 0x38600000
	.4byte 0x60000000
	.4byte 0x4BFFFFF4
	.4byte 0xFFFFFFFF
	.4byte 0x00000000
	.4byte cmdNop
	.rel cmdNop, .L_8006300C
	.rel cmdNop, .L_8006304C
	.4byte cmdRecvMem
	.4byte cmdSendMem
	.4byte cmdGo
	.rel cmdNop, .L_80062FCC
	.4byte cmdNop
	.rel cmdSendMem, .L_80063228
	.4byte cmdNop
	.4byte cmdNop
	.rel cmdSendMem, .L_80063270
	.rel cmdSendMem, .L_80063310
	.4byte cmdNop
	.rel tunerprotocol, .L_800635D8
	.4byte cmdReset
	.rel cmdReset, .L_80063454
	.rel cmdReset, .L_80063468
	.rel tunerprotocol, .L_800634BC
	.rel tunerprotocol, .L_80063580
	.4byte 0x07000000
	.4byte 0x00000000
	.4byte 0x0A496E74
	.4byte 0x206D6F64
	.4byte 0x6520656E
	.4byte 0x61626C65
	.4byte 0x640A0000
	.4byte 0x2A2A2A20
	.4byte 0x434D4420
	.4byte 0x52454144
	.4byte 0x20455252
	.4byte 0x4F52202A
	.4byte 0x2A2A0A00
	.4byte 0x002A2A2A
	.4byte 0x20424144
	.4byte 0x20434F4D
	.4byte 0x4D414E44
	.4byte 0x202A2A2A
	.4byte 0x0A000057
	.4byte 0x4149542E
	.4byte 0x2E2E0A00
	.4byte 0x00736E50
	.4byte 0x61757365
	.4byte 0x2829203A
	.4byte 0x2053746F
	.4byte 0x70706564
	.4byte 0x2E0A0000
	.4byte 0x46617461
	.4byte 0x6C206572
	.4byte 0x726F723A
	.4byte 0x2043616E
	.4byte 0x27742070
	.4byte 0x61746368
	.4byte 0x20657863
	.4byte 0x20766563
	.4byte 0x746F720A
	.4byte 0x0000436F
	.4byte 0x6D6D7320
	.4byte 0x4572726F
	.4byte 0x720A0000
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x00000000
.endobj bConnected

# 0x80278340..0x80281BA0 | size: 0x9860
.section .bss, "wa", @nobits
.balign 8

# .bss:0x0 | 0x80278340 | size: 0x9728
.obj lbl_80278340, global
	.skip 0x9728
.endobj lbl_80278340

# .bss:0x9728 | 0x80281A68 | size: 0x138
.obj NOA_ProgramExc, global
	.skip 0x138
.endobj NOA_ProgramExc
