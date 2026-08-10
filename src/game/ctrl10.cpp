#include "types.h"
#include "cManager.h"
#include "ctrl.h"

// The original .rodata is 8-aligned (0x18 bytes) although only the ctrl.h string survived the link.
asm(".section .rodata; .balign 8");
