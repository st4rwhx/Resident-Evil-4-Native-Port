# SN Systems ProDG libsn: EXI channel 2 host link (SNInitComm, SNRead/SNWrite, SNDVD* DMA helpers).
# Hand-written assembly in the original library; reproduced from the split object's disassembly.
.include "macros.inc"

# 0x80064E30..0x80065854 | size: 0xA24
.text
.balign 4

# .text:0x0 | 0x80064E30 | size: 0x154
.fn SNInitComm, global
	mflr r0
	stw r0, 0x4(r1)
	stwu r1, -0x10(r1)
	stw r31, 0x8(r1)
	stw r30, 0xc(r1)
	addi r31, r4, 0x0
	addi r30, r3, 0x0
	bl OSDisableInterrupts
	lis r0, lbl_80253D20+0x8@h
	ori r0, r0, lbl_80253D20+0x8@l
	lis r4, lbl_80253D20+0x4@h
	ori r4, r4, lbl_80253D20+0x4@l
	stw r0, 0x0(r4)
	stw r4, 0x0(r30)
	addi r30, r3, 0x0
	lis r3, lbl_80253D20@h
	ori r3, r3, lbl_80253D20@l
	stw r31, 0x0(r3)
	lis r3, 0x1
	ori r3, r3, 0x8000
	bl __OSMaskInterrupts
	li r0, 0x0
	lis r3, 0xcc00
	stw r0, 0x6828(r3)
.L_80064E90:
	bl SNSelect
.L_80064E94:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_80064E94
.L_80064EA0:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_80064EA0
	bl SNWiggleSelect
	li r3, 0x0
	bl SNWrite8
	li r3, 0x0
	bl SNWrite8
	li r3, 0x4
	bl SNWrite8
	li r3, 0x80
	bl SNWrite8
	li r3, 0x0
	bl SNWrite8
	bl SNWiggleSelect
.L_80064EE0:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_80064EE0
.L_80064EEC:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_80064EEC
	bl SNWiggleSelect
	bl SNRead32
	mr r5, r3
	bl SNRead32
	mr r6, r3
	li r3, 0xf
	mtctr r3
.L_80064F18:
	bdnz .L_80064F18
	bl SNDeselect
	lis r4, 0x534e
	ori r4, r4, 0x444c
	subf r5, r4, r5
	cmpwi r5, 0x0
	bne .L_80064E90
	lis r3, lbl_80281BA0@h
	ori r3, r3, lbl_80281BA0@l
	stw r6, 0x0(r3)
	li r5, 0x1
	slw r5, r5, r6
	stw r5, 0x4(r3)
	li r5, -0x1
	subfic r4, r6, 0x20
	srw r5, r5, r4
	clrrwi r4, r5, 2
	stw r5, 0x8(r3)
	stw r4, 0xc(r3)
	mr r3, r30
	bl OSRestoreInterrupts
	lwz r31, 0x8(r1)
	lwz r30, 0xc(r1)
	lwz r0, 0x14(r1)
	addi r1, r1, 0x10
	mtlr r0
	blr
.endfn SNInitComm

# .text:0x154 | 0x80064F84 | size: 0x48
.fn SNInitInterrupts, global
	mflr r0
	stw r0, 0x4(r1)
	stwu r1, -0x8(r1)
	lis r3, 0x1
	ori r3, r3, 0x8000
	bl __OSMaskInterrupts
	li r3, 0x40
	bl __OSMaskInterrupts
	lis r4, SNHandler@h
	ori r4, r4, SNHandler@l
	li r3, 0x19
	bl __OSSetInterruptHandler
	li r3, 0x40
	bl __OSUnmaskInterrupts
	lwz r0, 0xc(r1)
	addi r1, r1, 0x8
	mtlr r0
	blr
.endfn SNInitInterrupts

# .text:0x19C | 0x80064FCC | size: 0x88
.fn SNQueryData, global
	mflr r0
	stw r0, 0x4(r1)
	stwu r1, -0x8(r1)
	bl SNSelect
.L_80064FDC:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_80064FDC
.L_80064FE8:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_80064FE8
	bl SNWiggleSelect
	li r3, 0x1
	bl SNWrite8
	bl SNWiggleSelect
.L_80065008:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_80065008
.L_80065014:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_80065014
	bl SNWiggleSelect
	bl SNRead32
	mr r5, r3
	li r3, 0xf
	mtctr r3
.L_80065038:
	bdnz .L_80065038
	bl SNDeselect
	mr r3, r5
	lwz r0, 0xc(r1)
	addi r1, r1, 0x8
	mtlr r0
	blr
.endfn SNQueryData

# .text:0x224 | 0x80065054 | size: 0x1BC
.fn SNRead, global
	mflr r0
	stw r0, 0x4(r1)
	stwu r1, -0x18(r1)
	stw r31, 0x8(r1)
	stw r30, 0xc(r1)
	stw r29, 0x10(r1)
	stw r28, 0x14(r1)
	mr r31, r4
	mr r30, r3
	bl DCInvalidateRange
	bl SNSelect
.L_80065080:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_80065080
.L_8006508C:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_8006508C
	bl SNWiggleSelect
	li r3, 0x2
	bl SNWrite8
	lis r3, lbl_80281BA0@h
	ori r3, r3, lbl_80281BA0@l
	lwz r4, 0x0(r3)
	srw r28, r31, r4
	cmpwi r28, 0x0
	bne .L_800650D0
	lwz r4, 0x8(r3)
	and. r28, r31, r4
	beq .L_800651DC
	b .L_8006513C
.L_800650D0:
	bl SNWiggleSelect
.L_800650D4:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_800650D4
.L_800650E0:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_800650E0
	bl SNWiggleSelect
	lis r5, lbl_80281BA0@h
	ori r5, r5, lbl_80281BA0@l
	lwz r5, 0x4(r5)
	lis r29, 0xcc00
	stw r30, 0x682c(r29)
	stw r5, 0x6830(r29)
	li r3, 0x3
	stw r3, 0x6834(r29)
	bl SNSync
	add r30, r30, r5
	subi r28, r28, 0x1
	cmpwi r28, 0x0
	bne .L_800650D0
	lis r5, lbl_80281BA0@h
	ori r5, r5, lbl_80281BA0@l
	lwz r5, 0x8(r5)
	and. r28, r31, r5
	beq .L_800651DC
.L_8006513C:
	bl SNWiggleSelect
.L_80065140:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_80065140
.L_8006514C:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_8006514C
	bl SNWiggleSelect
	lis r5, lbl_80281BA0@h
	ori r5, r5, lbl_80281BA0@l
	lwz r5, 0xc(r5)
	and r29, r31, r5
	andi. r29, r29, 0xffe0
	beq .L_80065198
	lis r28, 0xcc00
	stw r30, 0x682c(r28)
	stw r29, 0x6830(r28)
	li r3, 0x3
	stw r3, 0x6834(r28)
	add r30, r30, r29
	andi. r29, r29, 0x1f
	bl SNSync
.L_80065198:
	andi. r28, r31, 0x1c
	beq .L_800651BC
	li r29, 0x0
.L_800651A4:
	bl SNRead32
	stwx r3, r30, r29
	addi r29, r29, 0x4
	cmpw r29, r28
	blt .L_800651A4
	add r30, r30, r29
.L_800651BC:
	andi. r28, r31, 0x3
	beq .L_800651DC
	li r29, 0x0
.L_800651C8:
	bl SNRead8
	stbx r3, r30, r29
	addi r29, r29, 0x1
	cmpw r29, r28
	blt .L_800651C8
.L_800651DC:
	li r3, 0xf
	mtctr r3
.L_800651E4:
	bdnz .L_800651E4
	bl SNDeselect
	li r3, 0x0
	lwz r31, 0x8(r1)
	lwz r30, 0xc(r1)
	lwz r29, 0x10(r1)
	lwz r28, 0x14(r1)
	lwz r0, 0x1c(r1)
	addi r1, r1, 0x18
	mtlr r0
	blr
.endfn SNRead

# .text:0x3E0 | 0x80065210 | size: 0x198
.fn SNWrite, global
	mflr r0
	stw r0, 0x4(r1)
	stwu r1, -0x18(r1)
	stw r31, 0x8(r1)
	stw r30, 0xc(r1)
	stw r29, 0x10(r1)
	stw r28, 0x14(r1)
	mr r31, r4
	mr r30, r3
	bl DCFlushRange
	bl SNSelect
.L_8006523C:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_8006523C
.L_80065248:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_80065248
	bl SNWiggleSelect
	li r3, 0x3
	bl SNWrite8
	srwi r3, r31, 8
	bl SNWrite8
	mr r3, r31
	bl SNWrite8
	srwi r28, r31, 9
	cmpwi r28, 0x0
	bne .L_8006528C
	andi. r28, r31, 0x1ff
	beq .L_80065374
	b .L_800652E4
.L_8006528C:
	bl SNWiggleSelect
.L_80065290:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_80065290
.L_8006529C:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_8006529C
	bl SNWiggleSelect
	lis r29, 0xcc00
	stw r30, 0x682c(r29)
	li r5, 0x200
	stw r5, 0x6830(r29)
	li r3, 0x7
	stw r3, 0x6834(r29)
	bl SNSync
	add r30, r30, r5
	subi r28, r28, 0x1
	cmpwi r28, 0x0
	bne .L_8006528C
	andi. r28, r31, 0x1ff
	beq .L_80065374
.L_800652E4:
	bl SNWiggleSelect
.L_800652E8:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_800652E8
.L_800652F4:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_800652F4
	bl SNWiggleSelect
	andi. r29, r31, 0x1e0
	beq .L_80065330
	lis r28, 0xcc00
	stw r30, 0x682c(r28)
	stw r29, 0x6830(r28)
	li r3, 0x7
	stw r3, 0x6834(r28)
	add r30, r30, r29
	andi. r29, r29, 0x1f
	bl SNSync
.L_80065330:
	andi. r28, r31, 0x1c
	beq .L_80065354
	li r29, 0x0
.L_8006533C:
	lwzx r3, r30, r29
	bl SNWrite32
	addi r29, r29, 0x4
	cmpw r29, r28
	blt .L_8006533C
	add r30, r30, r29
.L_80065354:
	andi. r28, r31, 0x3
	beq .L_80065374
	li r29, 0x0
.L_80065360:
	lbzx r3, r30, r29
	bl SNWrite8
	addi r29, r29, 0x1
	cmpw r29, r28
	blt .L_80065360
.L_80065374:
	li r3, 0xf
	mtctr r3
.L_8006537C:
	bdnz .L_8006537C
	bl SNDeselect
	li r3, 0x0
	lwz r31, 0x8(r1)
	lwz r30, 0xc(r1)
	lwz r29, 0x10(r1)
	lwz r28, 0x14(r1)
	lwz r0, 0x1c(r1)
	addi r1, r1, 0x18
	mtlr r0
	blr
.endfn SNWrite

# .text:0x578 | 0x800653A8 | size: 0x4
.fn SNOpen, global
	blr
.endfn SNOpen

# .text:0x57C | 0x800653AC | size: 0x4
.fn SNClose, global
	blr
.endfn SNClose

# .text:0x580 | 0x800653B0 | size: 0x54
.fn SNHandler, global
	mflr r0
	stw r0, 0x4(r1)
	stwu r1, -0x8(r1)
	li r0, 0x1000
	lis r5, 0xcc00
	stw r0, 0x3000(r5)
	lis r5, lbl_80253D20@h
	ori r5, r5, lbl_80253D20@l
	lwz r5, 0x0(r5)
	cmpwi r5, 0x0
	beq .L_800653F4
	mtlr r5
	lis r5, lbl_80253D20+0x8@h
	ori r5, r5, lbl_80253D20+0x8@l
	li r0, 0x1
	stb r0, 0x0(r5)
	blrl
.L_800653F4:
	lwz r0, 0xc(r1)
	addi r1, r1, 0x8
	mtlr r0
	blr
.endfn SNHandler

# .text:0x5D4 | 0x80065404 | size: 0x18
.fn SNSelect, local
	lis r4, 0xcc00
	lwz r3, 0x6828(r4)
	andi. r3, r3, 0x5
	ori r3, r3, 0xd0
	stw r3, 0x6828(r4)
	blr
.endfn SNSelect

# .text:0x5EC | 0x8006541C | size: 0x14
.fn SNDeselect, global
	lis r4, 0xcc00
	lwz r3, 0x6828(r4)
	andi. r3, r3, 0x5
	stw r3, 0x6828(r4)
	blr
.endfn SNDeselect

# .text:0x600 | 0x80065430 | size: 0x28
.fn SNWiggleSelect, global
	lis r4, 0xcc00
	lwz r5, 0x6828(r4)
	andi. r5, r5, 0x5
	stw r5, 0x6828(r4)
	li r3, 0x6
	mtctr r3
.L_80065448:
	bdnz .L_80065448
	ori r5, r5, 0xd0
	stw r5, 0x6828(r4)
	blr
.endfn SNWiggleSelect

# .text:0x628 | 0x80065458 | size: 0x14
.fn SNSync, global
	lis r4, 0xcc00
.L_8006545C:
	lwz r3, 0x6834(r4)
	clrlwi. r3, r3, 31
	bne .L_8006545C
	blr
.endfn SNSync

# .text:0x63C | 0x8006546C | size: 0x40
.fn SNRead8, global
	mflr r4
	stw r4, 0x4(r1)
	stwu r1, -0x8(r1)
	lis r4, 0xcc00
	li r3, 0x0
	stw r3, 0x6838(r4)
	li r3, 0x1
	stw r3, 0x6834(r4)
	bl SNSync
	lis r4, 0xcc00
	lwz r3, 0x6838(r4)
	srwi r3, r3, 24
	lwz r4, 0xc(r1)
	addi r1, r1, 0x8
	mtlr r4
	blr
.endfn SNRead8

# .text:0x67C | 0x800654AC | size: 0x34
.fn SNWrite8, global
	mflr r4
	stw r4, 0x4(r1)
	stwu r1, -0x8(r1)
	slwi r3, r3, 24
	lis r4, 0xcc00
	stw r3, 0x6838(r4)
	li r3, 0x5
	stw r3, 0x6834(r4)
	bl SNSync
	lwz r4, 0xc(r1)
	addi r1, r1, 0x8
	mtlr r4
	blr
.endfn SNWrite8

# .text:0x6B0 | 0x800654E0 | size: 0x3C
.fn SNRead32, global
	mflr r4
	stw r4, 0x4(r1)
	stwu r1, -0x8(r1)
	lis r4, 0xcc00
	li r3, 0x0
	stw r3, 0x6838(r4)
	li r3, 0x31
	stw r3, 0x6834(r4)
	bl SNSync
	lis r4, 0xcc00
	lwz r3, 0x6838(r4)
	lwz r4, 0xc(r1)
	addi r1, r1, 0x8
	mtlr r4
	blr
.endfn SNRead32

# .text:0x6EC | 0x8006551C | size: 0x30
.fn SNWrite32, global
	mflr r4
	stw r4, 0x4(r1)
	stwu r1, -0x8(r1)
	lis r4, 0xcc00
	stw r3, 0x6838(r4)
	li r3, 0x35
	stw r3, 0x6834(r4)
	bl SNSync
	addi r1, r1, 0x8
	lwz r4, 0x4(r1)
	mtlr r4
	blr
.endfn SNWrite32

# .text:0x71C | 0x8006554C | size: 0x50
.fn SNDVDRead_init, global
	mflr r0
	stwu r1, -0x18(r1)
	stw r0, 0x1c(r1)
	stmw r28, 0x8(r1)
	bl SNSelect
.L_80065560:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_80065560
.L_8006556C:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_8006556C
	bl SNWiggleSelect
	li r3, 0x2
	bl SNWrite8
	lwz r0, 0x1c(r1)
	mtlr r0
	lmw r28, 0x8(r1)
	addi r1, r1, 0x18
	blr
.endfn SNDVDRead_init

# .text:0x76C | 0x8006559C | size: 0x70
.fn SNDVDReadAsync_next, global
	mflr r0
	stwu r1, -0x18(r1)
	stw r0, 0x1c(r1)
	stmw r28, 0x8(r1)
	mr r31, r4
	mr r30, r3
	bl SNWiggleSelect
.L_800655B8:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_800655B8
.L_800655C4:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_800655C4
	bl SNWiggleSelect
	lis r29, 0xcc00
	stw r30, 0x682c(r29)
	stw r31, 0x6830(r29)
	li r3, 0x3
	stw r3, 0x6834(r29)
	lwz r3, 0x6828(r29)
	ori r3, r3, 0xc
	stw r3, 0x6828(r29)
	lwz r0, 0x1c(r1)
	mtlr r0
	lmw r28, 0x8(r1)
	addi r1, r1, 0x18
	blr
.endfn SNDVDReadAsync_next

# .text:0x7DC | 0x8006560C | size: 0x68
.fn SNDVDReadSync_next, global
	mflr r0
	stwu r1, -0x18(r1)
	stw r0, 0x1c(r1)
	stmw r28, 0x8(r1)
	mr r31, r4
	mr r30, r3
	bl SNWiggleSelect
.L_80065628:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_80065628
.L_80065634:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_80065634
	bl SNWiggleSelect
	lis r29, 0xcc00
	stw r30, 0x682c(r29)
	stw r31, 0x6830(r29)
	li r3, 0x3
	stw r3, 0x6834(r29)
	bl SNSync
	lwz r0, 0x1c(r1)
	mtlr r0
	lmw r28, 0x8(r1)
	addi r1, r1, 0x18
	blr
.endfn SNDVDReadSync_next

# .text:0x844 | 0x80065674 | size: 0x64
.fn SNDVDWrite_init, global
	mflr r0
	stwu r1, -0x18(r1)
	stw r0, 0x1c(r1)
	stmw r28, 0x8(r1)
	mr r31, r3
	bl SNSelect
.L_8006568C:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_8006568C
.L_80065698:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_80065698
	bl SNWiggleSelect
	li r3, 0x3
	bl SNWrite8
	srwi r3, r31, 8
	bl SNWrite8
	mr r3, r31
	bl SNWrite8
	lwz r0, 0x1c(r1)
	mtlr r0
	lmw r28, 0x8(r1)
	addi r1, r1, 0x18
	blr
.endfn SNDVDWrite_init

# .text:0x8A8 | 0x800656D8 | size: 0x70
.fn SNDVDWriteAsync_next, local
	mflr r0
	stwu r1, -0x18(r1)
	stw r0, 0x1c(r1)
	stmw r28, 0x8(r1)
	mr r31, r4
	mr r30, r3
	bl SNWiggleSelect
.L_800656F4:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_800656F4
.L_80065700:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_80065700
	bl SNWiggleSelect
	lis r29, 0xcc00
	stw r30, 0x682c(r29)
	stw r31, 0x6830(r29)
	li r3, 0x7
	stw r3, 0x6834(r29)
	lwz r3, 0x6828(r29)
	ori r3, r3, 0xc
	stw r3, 0x6828(r29)
	lwz r0, 0x1c(r1)
	mtlr r0
	lmw r28, 0x8(r1)
	addi r1, r1, 0x18
	blr
.endfn SNDVDWriteAsync_next

# .text:0x918 | 0x80065748 | size: 0x68
.fn SNDVDWriteSync_next, global
	mflr r0
	stwu r1, -0x18(r1)
	stw r0, 0x1c(r1)
	stmw r28, 0x8(r1)
	mr r31, r4
	mr r30, r3
	bl SNWiggleSelect
.L_80065764:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_80065764
.L_80065770:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_80065770
	bl SNWiggleSelect
	lis r29, 0xcc00
	stw r30, 0x682c(r29)
	stw r31, 0x6830(r29)
	li r3, 0x7
	stw r3, 0x6834(r29)
	bl SNSync
	lwz r0, 0x1c(r1)
	mtlr r0
	lmw r28, 0x8(r1)
	addi r1, r1, 0x18
	blr
.endfn SNDVDWriteSync_next

# .text:0x980 | 0x800657B0 | size: 0xA4
.fn SNDVDWriteNoDMA_next, global
	mflr r0
	stwu r1, -0x18(r1)
	stw r0, 0x1c(r1)
	stmw r28, 0x8(r1)
	mr r31, r4
	mr r30, r3
	bl SNWiggleSelect
.L_800657CC:
	bl SNRead8
	cmpwi r3, 0x0
	beq .L_800657CC
.L_800657D8:
	bl SNWiggleSelect
	bl SNRead8
	cmpwi r3, 0xff
	beq .L_800657D8
	bl SNWiggleSelect
	andi. r28, r31, 0x1c
	beq .L_80065810
	li r29, 0x0
.L_800657F8:
	lwzx r3, r30, r29
	bl SNWrite32
	addi r29, r29, 0x4
	cmpw r29, r28
	blt .L_800657F8
	add r30, r30, r29
.L_80065810:
	andi. r28, r31, 0x3
	beq .L_80065830
	li r29, 0x0
.L_8006581C:
	lbzx r3, r30, r29
	bl SNWrite8
	addi r29, r29, 0x1
	cmpw r29, r28
	blt .L_8006581C
.L_80065830:
	li r3, 0xf
	mtctr r3
.L_80065838:
	bdnz .L_80065838
	bl SNDeselect
	lwz r0, 0x1c(r1)
	mtlr r0
	lmw r28, 0x8(r1)
	addi r1, r1, 0x18
	blr
.endfn SNDVDWriteNoDMA_next

# 0x80253D20..0x80253D30 | size: 0x10
.data
.balign 8

# .data:0x0 | 0x80253D20 | size: 0x10
.obj lbl_80253D20, global
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x00000000
.endobj lbl_80253D20

# 0x80281BA0..0x80281BC0 | size: 0x20
.section .bss, "wa", @nobits
.balign 8

# .bss:0x0 | 0x80281BA0 | size: 0x20
.obj lbl_80281BA0, global
	.skip 0x20
.endobj lbl_80281BA0
