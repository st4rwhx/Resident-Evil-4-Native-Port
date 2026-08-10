/* ngcld -r script for the REL modules (tools/link_rel.py), after SN's preplf.ld: every output
 * section starts at 0, .ctors/.dtors get their null terminator from the script (the split objects
 * carry none), ngcld appends its BSS_TAG object itself (pointer at the end of .data, zero-size
 * __sn__bss__tag__ at the end of .bss) and COMMON symbols stay unallocated for tools/make_rel.py.
 * GXWGFifo is the same absolute symbol as in the DOL script (include/gx.h); ngcld -r keeps the
 * relocations against it, tools/link_rel.py resolves them after the link (the original RELs carry the
 * final lis/stfs -0x8000 words and no relocation there). */
GXWGFifo = 0xCC008000;
SECTIONS
{
  .text   0 : { *(.text) *(.text.*) *(.gnu.linkonce.t.*) }
  .ctors  0 : { _ctors = .; *(.ctors) *(.ctor) LONG(0); }
  .dtors  0 : { _dtors = .; *(.dtors) *(.dtor) LONG(0); }
  .rodata 0 : { *(.rodata) *(.rodata.*) *(.gnu.linkonce.r.*) }
  .data   0 : { *(.data) *(.data.*) *(.gnu.linkonce.d.*) }
  .bss    0 : { *(.bss) *(.bss.*) *(.sbss) }
}
