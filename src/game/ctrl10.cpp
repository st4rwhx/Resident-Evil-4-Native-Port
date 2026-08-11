#include "types.h"
#include "cManager.h"
#include "ctrl.h"

// The original .rodata is 8-aligned (0x18 bytes) although only the ctrl.h string survived the link.
#ifndef RE4_PORT   // portage : placement en sections, sans objet hors du build matching
#ifndef RE4_PORT   // portage : placement en sections, sans objet hors du build matching
#ifndef RE4_PORT   // portage : placement en sections, sans objet hors du build matching
#ifndef RE4_PORT   // portage : placement en sections, sans objet hors du build matching
asm(".section .rodata; .balign 8");
#endif
#endif
#endif
#endif
