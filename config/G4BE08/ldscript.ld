__start = 0x80003100;
ENTRY(__start)

SECTIONS
{
  .init   0x80003100 : { *(.init) *(.init.*) }
  .text   0x800034A0 : { *(.text) *(.text.*) }
  /* .ctor/.dtor: split objects (include the null terminators); .ctors/.dtors: compiled objects */
  .ctors  0x8021C960 : { *(.ctor) *(.ctors) *(.ctors.*) *(.dtor) *(.dtors) *(.dtors.*) }
  .rodata 0x8021CA40 : { *(.rodata) *(.rodata.*) }
  .data   0x802511E0 : { *(.data) *(.data.*) }
  .bss    0x802704A0 : { *(.bss) *(.bss.*) *(COMMON) }
  .sdata  0x80313EE0 : { *(.sdata) *(.sdata.*) }
  .sbss   0x80315460 : { *(.sbss) *(.sbss.*) }
  .sdata2 0x80315CC0 : { *(.sdata2) *(.sdata2.*) }

  _SDA_BASE_ = 0x8031BEE0;
  _SDA2_BASE_ = 0x8032BEE0;
  /* Absolute symbols referenced by the Dolphin SDK objects (values from the original OSInit/OSThread code). */
  _stack_addr = 0x8031ACE0;
  _stack_end = 0x80316CE0;
  __ArenaLo = 0x8031AD00;
  __ArenaHi = 0x81780000;
  /* GX write-gather FIFO (include/gx.h declares it as an extern object). */
  GXWGFifo = 0xCC008000;
  /* MWCC callee-saved FPR helpers: entry points inside _savefpr_14/_restfpr_14 (dct_ac DCT_AcInit saves f27..f31). */
  _savefpr_27 = _savefpr_14 + 0x34;
  _restfpr_27 = _restfpr_14 + 0x34;
}
