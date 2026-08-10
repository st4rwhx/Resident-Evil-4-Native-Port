.include "macros.inc"
/* yz2 (LZ + adaptive frequency) decoder, hand-written asm (game/yz2asm). */

.text
.balign 4

.fn yz2Decode_Decode, global
	stwu r1, -0x80(r1)
	mflr r0
	stmw r2, 0x8(r1)
	stw r0, 0x84(r1)
	lwz r30, 0x0(r3)
	mr r20, r4
	add r22, r4, r5
	lwz r23, 0x0(r6)
	mr r21, r20
	lis r31, 0x80
	addi r29, r3, 0x30
	addi r27, r3, 0xc
	lis r4, .float_one_data@ha
	lfs f5, .float_one_data@l(r4)
	lis r4, .float_zero_data@ha
	lfs f4, .float_zero_data@l(r4)
	fmr f2, f4
	fmr f3, f4
	lwz r17, 0x0(r29)
	lwz r18, 0x14(r29)
	lwz r19, 0x4(r29)
	lwz r16, 0x8(r29)
	lwz r15, 0xc(r29)
	lwz r6, 0x0(r27)
	lwz r12, 0x14(r27)
	lwz r14, 0x4(r27)
	lwz r5, 0x8(r27)
	lwz r3, 0xc(r27)
.endfn yz2Decode_Decode

.fn yz2Decode_loop, global
	cmplw r20, r22
	bge yz2Decode_end
	bl FrequencyDecode_Decode768
	cmplwi r24, 0x3ff
	ble yz2Decode_L01
	stb r24, 0x0(r20)
	li r24, 0x1
	addi r20, r20, 0x1
	b yz2Decode_dic_set
.endfn yz2Decode_loop

.fn yz2Decode_L01, global
	lbz r9, 0x0(r21)
	cmplwi r24, 0x1ff
	slwi r0, r9, 10
	add r0, r0, r9
	slwi r0, r0, 2
	add r26, r30, r0
	lhz r9, 0x2(r26)
	add r9, r24, r9
	clrlwi r25, r9, 23
	bgt yz2Decode_L02
	bl FrequencyDecode_Decode
	cmplwi r24, 0x2
	bgt .L_801D3F28
	cmpwi r24, 0x2
	bne .L_801D3EE0
	bl FrequencyDecode_Decode
	slwi r28, r24, 8
	b .L_801D3F20
.L_801D3EE0:
	cmpwi r24, 0x1
	bne .L_801D3F00
	bl FrequencyDecode_Decode
	slwi r28, r24, 16
	bl FrequencyDecode_Decode
	slwi r0, r24, 8
	or r28, r28, r0
	b .L_801D3F20
.L_801D3F00:
	bl FrequencyDecode_Decode
	slwi r28, r24, 24
	bl FrequencyDecode_Decode
	slwi r0, r24, 16
	or r28, r28, r0
	bl FrequencyDecode_Decode
	slwi r0, r24, 8
	or r28, r28, r0
.L_801D3F20:
	bl FrequencyDecode_Decode
	or r24, r28, r24
.L_801D3F28:
	slwi r0, r25, 2
	addi r9, r26, 0x4
	lwzx r9, r9, r0
	subi r24, r24, 0x1
	b yz2Decode_L03
.endfn yz2Decode_L01

.fn yz2Decode_L02, global
	slwi r10, r25, 2
	addi r9, r26, 0x804
	addi r11, r26, 0x4
	lwzx r24, r9, r10
	lwzx r9, r11, r10
.endfn yz2Decode_L02

.fn yz2Decode_L03, local
	li r8, 0x0
	mtctr r24
.endfn yz2Decode_L03

.fn yz2Decode_str_trans_loop, global
	lbzx r0, r9, r8
	stbx r0, r20, r8
	addi r8, r8, 0x1
	bdnz yz2Decode_str_trans_loop
	add r20, r20, r24
.endfn yz2Decode_str_trans_loop

.fn yz2Decode_dic_set, global
	subi r9, r20, 0x1
	cmplw r21, r9
	bge yz2Decode_loop
	lbz r0, 0x0(r21)
	addi r4, r21, 0x1
	mr r21, r9
	slwi r9, r0, 10
	add r9, r9, r0
	slwi r9, r9, 2
	lwzx r10, r30, r9
	add r11, r30, r9
	addi r8, r11, 0x4
	slwi r0, r10, 2
	addi r11, r11, 0x804
	stwx r4, r8, r0
	addi r10, r10, 0x1
	stwx r24, r11, r0
	clrlwi r10, r10, 23
	stwx r10, r30, r9
	b yz2Decode_loop
.endfn yz2Decode_dic_set

.fn yz2Decode_end, global
	lwz r0, 0x84(r1)
	mtlr r0
	lmw r2, 0x8(r1)
	addi r1, r1, 0x80
	blr
.endfn yz2Decode_end

.fn FrequencyDecode_Decode, global
	lwz r4, 0x0(r17)
	lwz r8, 0x4(r17)
	cmplw r4, r31
	bgt .L_801D4058
	cmplwi r4, 0x8000
	ble .L_801D4000
	slwi r10, r8, 8
	slwi r4, r4, 8
	lbz r0, 0x0(r23)
	addi r23, r23, 0x1
	or r8, r10, r0
	b .L_801D4058
.L_801D4000:
	cmplwi r4, 0x80
	ble .L_801D402C
	slwi r8, r8, 16
	slwi r4, r4, 16
	lbz r0, 0x0(r23)
	slwi r0, r0, 8
	lbz r7, 0x1(r23)
	addi r23, r23, 0x2
	or r8, r8, r0
	or r8, r8, r7
	b .L_801D4058
.L_801D402C:
	slwi r8, r8, 24
	slwi r4, r4, 24
	lbz r9, 0x0(r23)
	slwi r9, r9, 16
	lbz r0, 0x1(r23)
	or r8, r8, r9
	slwi r0, r0, 8
	lbz r9, 0x2(r23)
	addi r23, r23, 0x3
	or r8, r8, r0
	or r8, r8, r9
.L_801D4058:
	srwi r4, r4, 14
	stw r8, 0x4(r17)
	stw r4, 0x0(r17)
	divwu r24, r8, r4
	lwz r4, 0x20(r29)
	fcmpu cr0, f2, f4
	bne .L_801D40D0
	lwz r8, 0x18(r29)
	li r7, 0x0
	cmpw r7, r8
	bge .L_801D40CC
	li r0, 0x0
.L_801D4088:
	slwi r9, r7, 2
	add r11, r18, r9
	lhzx r10, r18, r9
	mr r9, r0
	lhz r0, 0x2(r11)
	add r0, r0, r10
	cmpw r9, r0
	bge .L_801D40C0
	slwi r11, r9, 2
	subf r9, r9, r0
	mtctr r9
.L_801D40B4:
	stwx r7, r11, r4
	addi r11, r11, 0x4
	bdnz .L_801D40B4
.L_801D40C0:
	addi r7, r7, 0x1
	cmpw r7, r8
	blt .L_801D4088
.L_801D40CC:
	fmr f2, f5
.L_801D40D0:
	slwi r24, r24, 2
	lwzx r24, r24, r4
	slwi r7, r24, 2
	add r8, r24, r24
	add r10, r7, r18
	lwz r11, 0x0(r17)
	lhz r0, 0x2(r10)
	lwz r9, 0x4(r17)
	mullw r0, r11, r0
	subf r9, r0, r9
	stw r9, 0x4(r17)
	lhzx r0, r7, r18
	mullw r11, r11, r0
	srwi r11, r11, 1
	stw r11, 0x0(r17)
	lhzx r9, r8, r19
	addi r9, r9, 0x1
	sthx r9, r8, r19
	addi r16, r16, 0x1
	cmpwi r15, 0xe
	bgt .L_801D4190
	lwz r0, 0x10(r29)
	cmpw r16, r0
	bne .L_801D41EC
	subfic r0, r15, 0xf
	lwz r4, 0x18(r29)
	li r10, 0x0
	fmr f2, f4
	li r9, 0x1
	slw r11, r9, r0
	cmpwi r4, 0x0
	ble .L_801D417C
	mr r8, r18
	mr r7, r19
	mtctr r4
.L_801D415C:
	lhz r0, 0x0(r7)
	addi r7, r7, 0x2
	mullw r0, r11, r0
	sth r10, 0x2(r8)
	add r10, r10, r0
	sth r0, 0x0(r8)
	addi r8, r8, 0x4
	bdnz .L_801D415C
.L_801D417C:
	li r0, 0x1
	addi r15, r15, 0x1
	slw r0, r0, r15
	stw r0, 0x10(r29)
	b .L_801D41EC
.L_801D4190:
	cmplwi r16, 0x7fff
	ble .L_801D41EC
	lwz r4, 0x18(r29)
	fmr f2, f4
	li r16, 0x0
	li r8, 0x0
	cmpw r16, r4
	bge .L_801D41EC
	mr r7, r18
	mr r10, r19
	mtctr r4
.L_801D41BC:
	lhz r0, 0x0(r10)
	sth r0, 0x0(r7)
	cmplwi r0, 0x1
	sth r8, 0x2(r7)
	add r8, r8, r0
	ble .L_801D41DC
	srwi r0, r0, 1
	sth r0, 0x0(r10)
.L_801D41DC:
	addi r10, r10, 0x2
	add r16, r16, r0
	addi r7, r7, 0x4
	bdnz .L_801D41BC
.L_801D41EC:
	blr
.endfn FrequencyDecode_Decode

.fn FrequencyDecode_Decode768, global
	lwz r4, 0x0(r6)
	lwz r8, 0x4(r6)
	cmplw r4, r31
	bgt .L_801D4278
	cmplwi r4, 0x8000
	ble .L_801D4220
	slwi r10, r8, 8
	slwi r4, r4, 8
	lbz r0, 0x0(r23)
	addi r23, r23, 0x1
	or r8, r10, r0
	b .L_801D4278
.L_801D4220:
	cmplwi r4, 0x80
	ble .L_801D424C
	slwi r8, r8, 16
	slwi r4, r4, 16
	lbz r0, 0x0(r23)
	slwi r0, r0, 8
	lbz r7, 0x1(r23)
	addi r23, r23, 0x2
	or r8, r8, r0
	or r8, r8, r7
	b .L_801D4278
.L_801D424C:
	slwi r8, r8, 24
	slwi r4, r4, 24
	lbz r9, 0x0(r23)
	slwi r9, r9, 16
	lbz r0, 0x1(r23)
	or r8, r8, r9
	slwi r0, r0, 8
	lbz r9, 0x2(r23)
	addi r23, r23, 0x3
	or r8, r8, r0
	or r8, r8, r9
.L_801D4278:
	srwi r4, r4, 14
	stw r8, 0x4(r6)
	stw r4, 0x0(r6)
	divwu r24, r8, r4
	lwz r4, 0x20(r27)
	fcmpu cr0, f3, f4
	bne .L_801D42F0
	lwz r8, 0x18(r27)
	li r7, 0x0
	cmpw r7, r8
	bge .L_801D42EC
	li r0, 0x0
.L_801D42A8:
	slwi r9, r7, 2
	add r11, r12, r9
	lhzx r10, r12, r9
	mr r9, r0
	lhz r0, 0x2(r11)
	add r0, r0, r10
	cmpw r9, r0
	bge .L_801D42E0
	slwi r11, r9, 2
	subf r9, r9, r0
	mtctr r9
.L_801D42D4:
	stwx r7, r11, r4
	addi r11, r11, 0x4
	bdnz .L_801D42D4
.L_801D42E0:
	addi r7, r7, 0x1
	cmpw r7, r8
	blt .L_801D42A8
.L_801D42EC:
	fmr f3, f5
.L_801D42F0:
	slwi r24, r24, 2
	lwzx r24, r24, r4
	slwi r7, r24, 2
	add r8, r24, r24
	add r10, r7, r12
	lwz r11, 0x0(r6)
	lhz r0, 0x2(r10)
	lwz r9, 0x4(r6)
	mullw r0, r11, r0
	subf r9, r0, r9
	stw r9, 0x4(r6)
	lhzx r0, r7, r12
	mullw r11, r11, r0
	srwi r11, r11, 1
	stw r11, 0x0(r6)
	lhzx r9, r8, r14
	addi r9, r9, 0x1
	sthx r9, r8, r14
	addi r5, r5, 0x1
	cmpwi r3, 0xe
	bgt .L_801D43B0
	lwz r0, 0x10(r27)
	cmpw r5, r0
	bne .L_801D440C
	subfic r0, r3, 0xf
	lwz r4, 0x18(r27)
	li r10, 0x0
	fmr f3, f4
	li r9, 0x1
	slw r11, r9, r0
	cmpwi r4, 0x0
	ble .L_801D439C
	mr r8, r12
	mr r7, r14
	mtctr r4
.L_801D437C:
	lhz r0, 0x0(r7)
	addi r7, r7, 0x2
	mullw r0, r11, r0
	sth r10, 0x2(r8)
	add r10, r10, r0
	sth r0, 0x0(r8)
	addi r8, r8, 0x4
	bdnz .L_801D437C
.L_801D439C:
	li r0, 0x1
	addi r3, r3, 0x1
	slw r0, r0, r3
	stw r0, 0x10(r27)
	b .L_801D440C
.L_801D43B0:
	cmplwi r5, 0x7fff
	ble .L_801D440C
	lwz r4, 0x18(r27)
	fmr f3, f4
	li r5, 0x0
	li r8, 0x0
	cmpw r5, r4
	bge .L_801D440C
	mr r7, r12
	mr r10, r14
	mtctr r4
.L_801D43DC:
	lhz r0, 0x0(r10)
	sth r0, 0x0(r7)
	cmplwi r0, 0x1
	sth r8, 0x2(r7)
	add r8, r8, r0
	ble .L_801D43FC
	srwi r0, r0, 1
	sth r0, 0x0(r10)
.L_801D43FC:
	addi r10, r10, 0x2
	add r5, r5, r0
	addi r7, r7, 0x4
	bdnz .L_801D43DC
.L_801D440C:
	blr
.endfn FrequencyDecode_Decode768

.rodata
.balign 8

.obj .float_one_data, local
	.float 1
.endobj .float_one_data

.obj .float_zero_data, global
	.float 0
.endobj .float_zero_data
