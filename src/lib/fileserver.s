# SN Systems ProDG libsn: host file-server protocol dispatch and the PC* syscall entry stubs.
# Hand-written assembly in the original library; reproduced from the split object's disassembly.
.include "macros.inc"

# 0x80064AE0..0x80064E30 | size: 0x350
.text
.balign 4

# .text:0x0 | 0x80064AE0 | size: 0x114
.fn snFileserver, global
	cmpwi r3, 0x7
	bge .L_80064C78
	slwi r7, r3, 2
	lis r6, lbl_80253C00@h
	ori r6, r6, lbl_80253C00@l
	lwz r3, 0x18(r31)
	lwz r4, 0x20(r31)
	lwz r5, 0x28(r31)
	lwzx r6, r7, r6
	lis r7, lbl_80253C00+0x80@h
	ori r7, r7, lbl_80253C00+0x80@l
	stb r6, 0x8(r7)
	li r8, 0x10
	li r6, 0x2
	sthbrx r8, r6, r7
	stw r3, 0xc(r7)
	stw r4, 0x10(r7)
	stw r5, 0x14(r7)
	mr r3, r7
	li r4, 0x18
	lwz r0, 0x6a4(r31)
	mtlr r0
	blrl
	li r4, 0x0
	lis r3, lbl_80253C00+0x98@h
	ori r3, r3, lbl_80253C00+0x98@l
	stw r4, 0x0(r3)
.L_80064B4C:
	li r3, 0x0
	stw r3, 0x6b4(r31)
.L_80064B54:
	lwz r0, 0x69c(r31)
	mtlr r0
	blrl
	cmpwi r3, 0x0
	beq .L_80064B54
	stw r3, 0x6b4(r31)
	cmplwi r3, 0x8008
	ble .L_80064B7C
	lis r3, 0x0
	ori r3, r3, 0x8008
.L_80064B7C:
	mr r4, r3
	stw r4, 0x6b8(r31)
	addi r3, r31, 0x700
	lwz r0, 0x6a0(r31)
	mtlr r0
	blrl
	cmpwi r3, 0x0
	beq .L_80064BC0
	lis r3, lbl_80253C00+0x9C@h
	ori r3, r3, lbl_80253C00+0x9C@l
.L_80064BA4:
	lwz r4, 0x6d0(r31)
	cmpwi r4, 0x0
	bne .L_80064BB8
	bl OSReport
	b .L_80064B4C
.L_80064BB8:
	bl proviewtty_800637BC
	b .L_80064B4C
.L_80064BC0:
	lbz r3, 0x700(r31)
	cmplwi r3, 0x14
	blt .L_80064BD8
	lis r3, lbl_80253C00+0xB8@h
	ori r3, r3, lbl_80253C00+0xB8@l
	b .L_80064BA4
.L_80064BD8:
	slwi r3, r3, 2
	lis r4, lbl_80253C00+0x1C@h
	ori r4, r4, lbl_80253C00+0x1C@l
	lwzx r5, r3, r4
	mtlr r5
	blrl
	b .L_80064B4C
.endfn snFileserver

# .text:0x114 | 0x80064BF4 | size: 0x20
.fn cmdFS_ACK, global
	li r4, 0x0
	sth r4, 0x702(r31)
	addi r3, r31, 0x700
	li r4, 0x8
	lwz r0, 0x6a4(r31)
	mtlr r0
	blrl
	lwz r3, 0x708(r31)
.endfn cmdFS_ACK

# .text:0x134 | 0x80064C14 | size: 0x34
.fn FS_Continue, global
	stw r3, 0x18(r31)
	lwz r3, 0x370(r31)
	addi r3, r3, 0x4
	stw r3, 0x370(r31)
	lis r3, lbl_80253C00+0x98@h
	ori r3, r3, lbl_80253C00+0x98@l
	lwz r3, 0x0(r3)
	cmpwi r3, 0x0
	beq cmdGo
	lwz r3, 0x378(r31)
	ori r3, r3, 0x400
	stw r3, 0x378(r31)
	b cmdGo
.endfn FS_Continue

# .text:0x168 | 0x80064C48 | size: 0x14
.fn cmdFS_STOP, global
	li r4, 0x1
	lis r3, lbl_80253C00+0x98@h
	ori r3, r3, lbl_80253C00+0x98@l
	stw r4, 0x0(r3)
	b .L_80064B4C
.endfn cmdFS_STOP

# .text:0x17C | 0x80064C5C | size: 0x84
.fn cmdFS_HostDisconnect, global
	li r4, 0x1
	lis r3, lbl_80253C00+0x98@h
	ori r3, r3, lbl_80253C00+0x98@l
	stw r4, 0x0(r3)
	lis r3, 0xffff
	ori r3, r3, 0xffff
	b FS_Continue
.L_80064C78:
	mr r30, r3
	lwz r3, 0x18(r31)
	lwz r4, 0x20(r31)
	lwz r5, 0x28(r31)
	lwz r6, 0x30(r31)
	li r7, -0x1
	li r8, 0x0
	cmpwi r30, 0x7
	beq .L_80064CC8
	cmpwi r30, 0xa
	beq .L_80064CB8
	cmpwi r30, 0xb
	beq .L_80064CC0
	li r8, 0x1
	cmpwi r30, 0x8
	bne .L_80064CC0
.L_80064CB8:
	bl PCreadAsyncInit
	b .L_80064CCC
.L_80064CC0:
	bl PCwriteAsyncInit
	b .L_80064CCC
.L_80064CC8:
	bl CompleteAsync
.L_80064CCC:
	stw r3, 0x18(r31)
	lwz r3, 0x370(r31)
	addi r3, r3, 0x4
	stw r3, 0x370(r31)
	b cmdGo
.endfn cmdFS_HostDisconnect

# .text:0x200 | 0x80064CE0 | size: 0xF0
.fn snInitFileserver, global
	li r3, 0x10
	lis r4, PCinit@h
	ori r4, r4, PCinit@l
	stw r3, 0x0(r4)
	li r3, 0x11
	lis r4, PCcreat@h
	ori r4, r4, PCcreat@l
	stw r3, 0x0(r4)
	li r3, 0x12
	lis r4, PCopen@h
	ori r4, r4, PCopen@l
	stw r3, 0x0(r4)
	li r3, 0x13
	lis r4, PCclose@h
	ori r4, r4, PCclose@l
	stw r3, 0x0(r4)
	li r3, 0x14
	lis r4, PCread@h
	ori r4, r4, PCread@l
	stw r3, 0x0(r4)
	li r3, 0x15
	lis r4, PCwrite@h
	ori r4, r4, PCwrite@l
	stw r3, 0x0(r4)
	li r3, 0x16
	lis r4, PClseek@h
	ori r4, r4, PClseek@l
	stw r3, 0x0(r4)
	li r3, 0x17
	lis r4, PCsync@h
	ori r4, r4, PCsync@l
	stw r3, 0x0(r4)
	li r3, 0x18
	lis r4, PCreadAsync@h
	ori r4, r4, PCreadAsync@l
	stw r3, 0x0(r4)
	li r3, 0x19
	lis r4, PCwriteAsync@h
	ori r4, r4, PCwriteAsync@l
	stw r3, 0x0(r4)
	li r3, 0x1a
	lis r4, PCreadAsync2@h
	ori r4, r4, PCreadAsync2@l
	stw r3, 0x0(r4)
	li r3, 0x1b
	lis r4, PCwriteAsync2@h
	ori r4, r4, PCwriteAsync2@l
	stw r3, 0x0(r4)
	blr
.L_80064DA4:
	mflr r0
	stw r0, 0x4(r1)
	stwu r1, -0x8(r1)
	lis r3, 0x8025
	ori r3, r3, 0x3cd1
	bl OSReport
	li r3, -0x1
	addi r1, r1, 0x8
	lwz r0, 0x4(r1)
	mtlr r0
	blr
.endfn snInitFileserver

# .text:0x2F0 | 0x80064DD0 | size: 0x8
.fn PCinit, global
	b .L_80064DA4
	blr
.endfn PCinit

# .text:0x2F8 | 0x80064DD8 | size: 0x8
.fn PCcreat, global
	b .L_80064DA4
	blr
.endfn PCcreat

# .text:0x300 | 0x80064DE0 | size: 0x8
.fn PCopen, global
	b .L_80064DA4
	blr
.endfn PCopen

# .text:0x308 | 0x80064DE8 | size: 0x8
.fn PCclose, global
	b .L_80064DA4
	blr
.endfn PCclose

# .text:0x310 | 0x80064DF0 | size: 0x8
.fn PCread, global
	b .L_80064DA4
	blr
.endfn PCread

# .text:0x318 | 0x80064DF8 | size: 0x8
.fn PCwrite, global
	b .L_80064DA4
	blr
.endfn PCwrite

# .text:0x320 | 0x80064E00 | size: 0x8
.fn PClseek, global
	b .L_80064DA4
	blr
.endfn PClseek

# .text:0x328 | 0x80064E08 | size: 0x8
.fn PCsync, global
	b .L_80064DA4
	blr
.endfn PCsync

# .text:0x330 | 0x80064E10 | size: 0x8
.fn PCreadAsync, global
	b .L_80064DA4
	blr
.endfn PCreadAsync

# .text:0x338 | 0x80064E18 | size: 0x8
.fn PCwriteAsync, global
	b .L_80064DA4
	blr
.endfn PCwriteAsync

# .text:0x340 | 0x80064E20 | size: 0x8
.fn PCreadAsync2, global
	b .L_80064DA4
	blr
.endfn PCreadAsync2

# .text:0x348 | 0x80064E28 | size: 0x8
.fn PCwriteAsync2, global
	b .L_80064DA4
	blr
.endfn PCwriteAsync2

# 0x80253C00..0x80253D20 | size: 0x120
.data
.balign 8

# .data:0x0 | 0x80253C00 | size: 0x120
.obj lbl_80253C00, global
	.4byte 0x00000001
	.4byte 0x00000002
	.4byte 0x00000003
	.4byte 0x00000004
	.4byte 0x00000005
	.4byte 0x00000006
	.4byte 0x00000007
	.4byte cmdNop
	.4byte cmdNop
	.4byte cmdNop
	.4byte cmdRecvMem
	.4byte cmdSendMem
	.4byte cmdNop
	.4byte cmdFS_STOP
	.4byte cmdNop
	.4byte cmdNop
	.4byte cmdNop
	.4byte cmdFS_ACK
	.4byte cmdNop
	.4byte cmdNop
	.4byte cmdNop
	.4byte cmdNop
	.4byte cmdReset
	.4byte cmdFS_HostDisconnect
	.4byte cmdNop
	.4byte cmdNop
	.4byte cmdNop
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x09000000
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x2A2A2A20
	.4byte 0x46532043
	.4byte 0x4D442052
	.4byte 0x45414420
	.4byte 0x4552524F
	.4byte 0x52202A2A
	.4byte 0x2A0A0000
	.4byte 0x2A2A2A20
	.4byte 0x46532042
	.4byte 0x41442043
	.4byte 0x4F4D4D41
	.4byte 0x4E44202A
	.4byte 0x2A2A0A00
	.4byte 0x0046696C
	.4byte 0x65207365
	.4byte 0x72766572
	.4byte 0x2066756E
	.4byte 0x6374696F
	.4byte 0x6E206E6F
	.4byte 0x74206176
	.4byte 0x61696C61
	.4byte 0x626C6520
	.4byte 0x696E206E
	.4byte 0x6F6E2064
	.4byte 0x65627567
	.4byte 0x206D6F64
	.4byte 0x650A0000
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x00000000
.endobj lbl_80253C00
