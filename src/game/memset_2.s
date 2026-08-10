/* Hand-written fill helpers (game/memset_2): word-fill in 16-byte blocks when the destination is
 * word aligned, byte tail otherwise. */
.include "macros.inc"

.text
.balign 4

/* void memset_asm(void *dst, int c, u32 n) */
.fn memset_asm, global
	cmplwi r5, 0xf
	ble tail_set
	andi. r0, r3, 0x3
	bne tail_set
	clrlwi r4, r4, 24
	slwi r11, r4, 8
	or r11, r11, r4
	slwi r9, r11, 16
	or r11, r11, r9
	subi r3, r3, 0x4
	srwi r12, r5, 4
	andi. r5, r5, 0xf
	mtctr r12
block_set:
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	bdnz block_set
	addi r3, r3, 0x4
tail_set:
	cmpwi r5, 0x0
	beqlr
	subi r3, r3, 0x1
	mtctr r5
byte_set:
	stbu r4, 0x1(r3)
	bdnz byte_set
	blr
.endfn memset_asm

/* void memclr_asm(void *dst, u32 n) */
.fn memclr_asm, global
	li r11, 0x0
	cmplwi r4, 0xf
	ble tail_clr
	andi. r0, r3, 0x3
	bne tail_clr
	subi r3, r3, 0x4
	srwi r12, r4, 4
	andi. r4, r4, 0xf
	mtctr r12
block_clr:
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	bdnz block_clr
	addi r3, r3, 0x4
tail_clr:
	cmpwi r4, 0x0
	beqlr
	subi r3, r3, 0x1
	mtctr r4
byte_clr:
	stbu r11, 0x1(r3)
	bdnz byte_clr
	blr
.endfn memclr_asm

/* void mtxclr_asm(Mtx *dst, u32 count): zero `count` 3x4 float matrices */
.fn mtxclr_asm, global
	li r11, 0x0
	subi r3, r3, 0x4
	mtctr r4
mtx_clr:
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	stwu r11, 0x4(r3)
	bdnz mtx_clr
	blr
.endfn mtxclr_asm
