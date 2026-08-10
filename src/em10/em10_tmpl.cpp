// Second object of every Ganado module (em10..em20), between em10.cpp and the per-enemy
// <em>_set.cpp; its real name is not in the binary. It compiles to no code and no data of its own:
// only the header strings of this include set (cFlag.set() / atari.h / light.h / dmg.h / ctrl.h,
// .rodata 0x1D40..0x1E88) and the five cManager<cLight> template strings, plus the 0x3B8-byte
// nameless linkonce block (log / countActiveWork / create(int) / create() / create(int, u32)) that
// every later object including light.h carries (.text 0x43518..0x438D0, its `bl`s resolve to
// em10.cpp's named copies). Two objects are the only way to get the duplicated header strings:
// GCC 2.95 merges identical string constants within one translation unit.
#include "types.h"
#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
