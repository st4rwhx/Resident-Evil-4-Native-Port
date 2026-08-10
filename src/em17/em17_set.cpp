// em17: the per-enemy object of a Ganado module (the real file name is not in the binary; the
// module is em10.cpp + this file). _prolog registers the enemy's Init/Set functions with the DOL
// (EmInitFunc) and the shared em10.cpp (Em10SetFunc); Em17Init constructs the shared cEm10 class in
// the manager's work, Em17Set / Em17WeaponSet fill the Ganado work's motion table from the enemy
// archive (cEm::subArc) by model type.

#include "types.h"
#include "atari.h"
#include "global.h"
#include "cManager.h"
#include "em10.h"

extern "C" void OSReport(const char* fmt, ...);
extern "C" void Em10SetSeTbl(cEm10* em, int type);

extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

void Em17Init(cEm* em);
void Em17Set(cEm10* em);
void Em17WeaponSet(cEm10* em);

#define ARC(no) PL_ARC_PTR(em->subArc, no)

extern "C" void _prolog()
{
    OSReport("em10 prolog Ok\n");
    EmInitFunc = Em17Init;
    Em10SetFunc = Em17Set;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em17Init(cEm* em)
{
    new (em) cEm10();
}

void Em17Set(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->type) {
    case 0:
    default:
        em->type = 0;
        w->mot[0] = ARC(0x1BD);
        w->mot[1] = ARC(0x1BC);
        w->mot[2] = ARC(0x1BE);
        w->mot[3] = ARC(0x1BF);
        w->mot[4] = ARC(0x1BF);
        w->mot[5] = ARC(0x1BD);
        w->mot[6] = ARC(0x1C0);
        w->mot[7] = ARC(0x1C1);
        w->mot[8] = ARC(0x1C2);
        w->mot[9] = ARC(0x1C3);
        w->mot[10] = ARC(0x1C4);
        w->mot[11] = ARC(0x1C5);
        w->mot[12] = ARC(0x1C6);
        w->mot[13] = ARC(0x1C7);
        w->mot[14] = ARC(0x1C8);
        w->mot[15] = ARC(0x1C8);
        w->mot[16] = 0;
        w->mot[17] = 0;
        w->mot[18] = 0;
        w->mot[19] = 0;
        w->mot[20] = 0;
        w->mot[21] = 0;
        w->mot[22] = 0;
        w->mot[23] = 0;
        w->mot[24] = 0;
        w->mot[25] = 0;
        w->mot[26] = 0;
        w->mot[27] = 0;
        w->mot[28] = 0;
        w->mot[29] = 0;
        w->mot[30] = 0;
        w->mot[31] = 0;
        w->mot[32] = 0;
        w->mot[33] = 0;
        w->mot[34] = 0;
        w->mot[35] = 0;
        w->mot[36] = 0;
        w->mot[37] = 0;
        w->mot[38] = 0;
        w->mot[39] = 0;
        w->mot[40] = 0;
        Em10SetSeTbl(em, 0);
        break;
    case 1:
        w->mot[0] = ARC(0x1D9);
        w->mot[1] = ARC(0x1D8);
        w->mot[2] = ARC(0x1DA);
        w->mot[5] = ARC(0x1D9);
        w->mot[3] = ARC(0x1DB);
        w->mot[4] = ARC(0x1DB);
        w->mot[6] = ARC(0x1C0);
        w->mot[7] = ARC(0x1C1);
        w->mot[8] = ARC(0x1C2);
        w->mot[9] = ARC(0x1C3);
        w->mot[10] = ARC(0x1C4);
        w->mot[11] = ARC(0x1C5);
        w->mot[12] = ARC(0x1C6);
        w->mot[13] = ARC(0x1C7);
        w->mot[14] = ARC(0x1C8);
        w->mot[15] = ARC(0x1C8);
        w->mot[16] = 0;
        w->mot[17] = 0;
        w->mot[18] = 0;
        w->mot[19] = 0;
        w->mot[20] = 0;
        w->mot[21] = 0;
        w->mot[22] = 0;
        w->mot[23] = 0;
        w->mot[24] = 0;
        w->mot[25] = 0;
        w->mot[26] = 0;
        w->mot[27] = 0;
        w->mot[28] = 0;
        w->mot[29] = 0;
        w->mot[30] = 0;
        w->mot[31] = 0;
        w->mot[32] = 0;
        w->mot[33] = 0;
        w->mot[34] = 0;
        w->mot[35] = 0;
        w->mot[36] = 0;
        w->mot[37] = 0;
        w->mot[38] = 0;
        w->mot[39] = 0;
        w->mot[40] = 0;
        Em10SetSeTbl(em, 1);
        break;
    case 3:
        w->mot[0] = ARC(0x1DD);
        w->mot[1] = ARC(0x1DC);
        w->mot[2] = ARC(0x1DE);
        w->mot[3] = ARC(0x1DF);
        w->mot[4] = ARC(0x1DF);
        w->mot[5] = ARC(0x1DD);
        w->mot[6] = ARC(0x1C0);
        w->mot[7] = ARC(0x1C1);
        w->mot[8] = ARC(0x1C2);
        w->mot[9] = ARC(0x1C3);
        w->mot[10] = ARC(0x1C4);
        w->mot[11] = ARC(0x1C5);
        w->mot[12] = ARC(0x1C6);
        w->mot[13] = ARC(0x1C7);
        w->mot[14] = ARC(0x1C8);
        w->mot[15] = ARC(0x1C8);
        w->mot[16] = 0;
        w->mot[17] = 0;
        w->mot[18] = 0;
        w->mot[19] = 0;
        w->mot[20] = 0;
        w->mot[21] = 0;
        w->mot[22] = 0;
        w->mot[23] = 0;
        w->mot[24] = 0;
        w->mot[25] = 0;
        w->mot[26] = 0;
        w->mot[27] = 0;
        w->mot[28] = 0;
        w->mot[29] = 0;
        w->mot[30] = 0;
        w->mot[31] = 0;
        w->mot[32] = 0;
        w->mot[33] = 0;
        w->mot[34] = 0;
        w->mot[35] = 0;
        w->mot[36] = 0;
        w->mot[37] = 0;
        w->mot[38] = 0;
        w->mot[39] = 0;
        w->mot[40] = 0;
        Em10SetSeTbl(em, 2);
        break;
    case 12:
        em->type = 12;
        w->mot[0] = ARC(0x1CE);
        w->mot[1] = ARC(0x1CD);
        w->mot[2] = ARC(0x1CF);
        w->mot[3] = ARC(0x1D0);
        w->mot[4] = ARC(0x1D0);
        w->mot[5] = ARC(0x1CE);
        w->mot[6] = ARC(0x1C0);
        w->mot[7] = ARC(0x1C1);
        w->mot[8] = ARC(0x1C2);
        w->mot[9] = ARC(0x1C3);
        w->mot[10] = ARC(0x1C4);
        w->mot[11] = ARC(0x1C5);
        w->mot[12] = ARC(0x1C6);
        w->mot[13] = ARC(0x1C7);
        w->mot[14] = ARC(0x1C8);
        w->mot[15] = ARC(0x1C8);
        w->mot[16] = 0;
        w->mot[17] = 0;
        w->mot[18] = 0;
        w->mot[19] = 0;
        w->mot[20] = 0;
        w->mot[21] = 0;
        w->mot[22] = 0;
        w->mot[23] = ARC(0x1D3);
        w->mot[24] = ARC(0x1D4);
        w->mot[25] = ARC(0x1D5);
        w->mot[26] = 0;
        w->mot[27] = 0;
        w->mot[28] = 0;
        w->mot[29] = 0;
        w->mot[30] = 0;
        w->mot[31] = 0;
        w->mot[32] = 0;
        w->mot[33] = 0;
        w->mot[34] = 0;
        w->mot[35] = 0;
        w->mot[36] = 0;
        w->mot[37] = 0;
        w->mot[38] = 0;
        w->mot[39] = 0;
        w->mot[40] = 0;
        Em10SetSeTbl(em, 3);
        break;
    }
    w->x6C5 = 0;
    Em17WeaponSet(em);
}

void Em17WeaponSet(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    w->mot[41] = ARC(0x254);
    w->mot[42] = ARC(0x255);
    w->mot[43] = 0;
    w->mot[44] = 0;
    w->mot[45] = 0;
    w->mot[46] = 0;
    w->mot[47] = 0;
    w->mot[48] = 0;
    w->mot[49] = 0;
    w->mot[50] = 0;
    w->mot[51] = 0;
    w->mot[52] = ARC(0x25F);
    w->mot[53] = ARC(0x260);
    w->mot[54] = ARC(0x261);
    w->mot[55] = 0;
    w->mot[56] = 0;
    w->mot[57] = 0;
    w->mot[58] = ARC(0x265);
    w->mot[59] = ARC(0x266);
    w->mot[60] = ARC(0x267);
    w->mot[61] = ARC(0x268);
    w->mot[62] = ARC(0x269);
    w->mot[63] = ARC(0x26A);
    w->mot[64] = ARC(0x26B);
    w->mot[65] = ARC(0x26C);
    w->mot[66] = ARC(0x26D);
    w->mot[67] = 0;
    w->mot[68] = 0;
    w->mot[69] = 0;
    w->mot[70] = 0;
    w->mot[71] = ARC(0x270);
    w->mot[72] = ARC(0x271);
    w->mot[73] = 0;
    w->mot[74] = 0;
    w->mot[75] = 0;
    w->mot[76] = 0;
    w->mot[77] = ARC(0x1D6);
    w->mot[78] = ARC(0x1D7);
}
