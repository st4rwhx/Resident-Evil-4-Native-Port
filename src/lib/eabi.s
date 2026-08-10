# SN Systems ProDG libsn: EABI floating-point register save/restore helpers (_savefpr_14/_restfpr_14).
# Hand-written assembly in the original library; reproduced from the split object's disassembly.
.include "macros.inc"

# 0x8021C8B4..0x8021C960 | size: 0xAC
.text
.balign 4

# .text:0x0 | 0x8021C8B4 | size: 0x4C
.fn _savefpr_14, global
	stfd f14, -0x90(r11)
	stfd f15, -0x88(r11)
	stfd f16, -0x80(r11)
	stfd f17, -0x78(r11)
	stfd f18, -0x70(r11)
	stfd f19, -0x68(r11)
	stfd f20, -0x60(r11)
	stfd f21, -0x58(r11)
	stfd f22, -0x50(r11)
	stfd f23, -0x48(r11)
	stfd f24, -0x40(r11)
	stfd f25, -0x38(r11)
	stfd f26, -0x30(r11)
	stfd f27, -0x28(r11)
	stfd f28, -0x20(r11)
	stfd f29, -0x18(r11)
	stfd f30, -0x10(r11)
	stfd f31, -0x8(r11)
	blr
.endfn _savefpr_14

# .text:0x4C | 0x8021C900 | size: 0x4C
.fn _restfpr_14, global
	lfd f14, -0x90(r11)
	lfd f15, -0x88(r11)
	lfd f16, -0x80(r11)
	lfd f17, -0x78(r11)
	lfd f18, -0x70(r11)
	lfd f19, -0x68(r11)
	lfd f20, -0x60(r11)
	lfd f21, -0x58(r11)
	lfd f22, -0x50(r11)
	lfd f23, -0x48(r11)
	lfd f24, -0x40(r11)
	lfd f25, -0x38(r11)
	lfd f26, -0x30(r11)
	lfd f27, -0x28(r11)
	lfd f28, -0x20(r11)
	lfd f29, -0x18(r11)
	lfd f30, -0x10(r11)
	lfd f31, -0x8(r11)
	blr
.endfn _restfpr_14

# .text:0x98 | 0x8021C94C | size: 0x14
.fn gap_01_8021C94C_text, global
.hidden gap_01_8021C94C_text
	.4byte 0x00000000 /* invalid */
	.4byte 0x00000000 /* invalid */
	.4byte 0x00000000 /* invalid */
	.4byte 0x00000000 /* invalid */
	.4byte 0x00000000 /* invalid */
.endfn gap_01_8021C94C_text
