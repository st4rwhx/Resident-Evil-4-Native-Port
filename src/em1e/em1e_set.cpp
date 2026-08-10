// em1e: the per-enemy object of a Ganado module (the real file name is not in the binary; the
// module is em10.cpp + this file). _prolog registers the enemy's Init/Set functions with the DOL
// (EmInitFunc) and the shared em10.cpp (Em10SetFunc); Em1eInit constructs the shared cEm10 class in
// the manager's work, Em1eSet / Em1eWeaponSet fill the Ganado work's motion table from the enemy
// archive (cEm::subArc) by model type.

#include "types.h"
#include "atari.h"
#include "global.h"
#include "cManager.h"
#include "em10.h"
#include "light.h"
#include "esp.h"

extern "C" void OSReport(const char* fmt, ...);
extern "C" void Em10SetSeTbl(cEm10* em, int type);

extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

void Em1eInit(cEm* em);
void Em1eSet(cEm10* em);
void Em1eWeaponSet(cEm10* em);

#define ARC(no) PL_ARC_PTR(em->subArc, no)

extern "C" void _prolog()
{
    OSReport("em10 prolog Ok\n");
    EmInitFunc = Em1eInit;
    Em10SetFunc = Em1eSet;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em1eInit(cEm* em)
{
    new (em) cEm10();
}

void Em1eSet(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->type) {
    case 14:
    default:
        w->mot[0] = ARC(0x230);
        w->mot[1] = ARC(0x22F);
        em->type = 14;
        w->mot[2] = ARC(0x231);
        w->mot[3] = ARC(0x233);
        w->mot[4] = ARC(0x233);
        w->mot[5] = ARC(0x232);
        w->mot[6] = ARC(0x234);
        w->mot[7] = ARC(0x235);
        w->mot[8] = ARC(0x236);
        w->mot[9] = ARC(0x237);
        w->mot[10] = ARC(0x237);
        w->mot[11] = ARC(0x238);
        w->mot[12] = ARC(0x239);
        w->mot[13] = ARC(0x23A);
        w->mot[14] = ARC(0x23B);
        w->mot[15] = ARC(0x23C);
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
    case 15:
        w->mot[0] = ARC(0x230);
        w->mot[1] = ARC(0x22F);
        w->mot[2] = ARC(0x249);
        w->mot[3] = ARC(0x233);
        w->mot[4] = ARC(0x233);
        w->mot[5] = ARC(0x24A);
        w->mot[6] = ARC(0x234);
        w->mot[7] = ARC(0x235);
        w->mot[8] = ARC(0x236);
        w->mot[9] = ARC(0x237);
        w->mot[10] = ARC(0x237);
        w->mot[11] = ARC(0x238);
        w->mot[12] = ARC(0x239);
        w->mot[13] = ARC(0x23A);
        w->mot[14] = ARC(0x23B);
        w->mot[15] = ARC(0x23C);
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
    case 23:
        w->mot[0] = ARC(0x248);
        w->mot[1] = ARC(0x247);
        w->mot[2] = ARC(0x24D);
        w->mot[3] = ARC(0x233);
        w->mot[4] = ARC(0x233);
        w->mot[5] = ARC(0x24E);
        w->mot[6] = ARC(0x234);
        w->mot[7] = ARC(0x235);
        w->mot[8] = ARC(0x236);
        w->mot[9] = ARC(0x237);
        w->mot[10] = ARC(0x237);
        w->mot[11] = ARC(0x238);
        w->mot[12] = ARC(0x239);
        w->mot[13] = ARC(0x23A);
        w->mot[14] = ARC(0x23B);
        w->mot[15] = ARC(0x23C);
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
    case 25:
        w->mot[0] = ARC(0x248);
        w->mot[1] = ARC(0x247);
        w->mot[2] = ARC(0x24B);
        w->mot[3] = ARC(0x233);
        w->mot[4] = ARC(0x233);
        w->mot[5] = ARC(0x24C);
        w->mot[6] = ARC(0x234);
        w->mot[7] = ARC(0x235);
        w->mot[8] = ARC(0x236);
        w->mot[9] = ARC(0x237);
        w->mot[10] = ARC(0x237);
        w->mot[11] = ARC(0x238);
        w->mot[12] = ARC(0x239);
        w->mot[13] = ARC(0x23A);
        w->mot[14] = ARC(0x23B);
        w->mot[15] = ARC(0x23C);
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
    case 6:
        w->mot[0] = ARC(0x1E8);
        w->mot[1] = ARC(0x1E7);
        w->mot[2] = ARC(0x1E9);
        w->mot[3] = ARC(0x1E9);
        w->mot[4] = ARC(0x1E9);
        w->mot[16] = ARC(0x1F0);
        w->mot[5] = ARC(0x1EA);
        w->mot[17] = ARC(0x1F1);
        w->mot[6] = ARC(0x1F4);
        w->mot[7] = ARC(0x1F5);
        w->mot[8] = ARC(0x1F6);
        w->mot[9] = ARC(0x1F7);
        w->mot[10] = ARC(0x1F7);
        w->mot[11] = ARC(0x1F8);
        w->mot[12] = ARC(0x1F9);
        w->mot[13] = ARC(0x1FA);
        w->mot[14] = ARC(0x1FB);
        w->mot[15] = ARC(0x1FB);
        w->mot[18] = ARC(0x1ED);
        w->mot[19] = ARC(0x1EE);
        w->mot[20] = ARC(0x1EF);
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
    }
    if (em->type == 6) {
        w->x6C5 = 0;
    } else {
        w->x6C5 = 2;
    }
    Em1eWeaponSet(em);
    EspDataLoad((u32) ARC(0x278), 0xcd, 0);
}

void Em1eWeaponSet(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    w->mot[41] = 0;
    w->mot[42] = 0;
    w->mot[43] = 0;
    w->mot[44] = 0;
    w->mot[45] = 0;
    w->mot[46] = 0;
    w->mot[47] = 0;
    w->mot[48] = 0;
    w->mot[49] = 0;
    w->mot[50] = 0;
    w->mot[51] = 0;
    w->mot[52] = 0;
    w->mot[53] = 0;
    w->mot[54] = 0;
    w->mot[55] = 0;
    w->mot[56] = 0;
    w->mot[57] = 0;
    w->mot[58] = 0;
    w->mot[59] = 0;
    w->mot[60] = 0;
    w->mot[61] = 0;
    w->mot[62] = 0;
    w->mot[63] = ARC(0x26C);
    w->mot[64] = ARC(0x26D);
    w->mot[65] = ARC(0x276);
    w->mot[66] = ARC(0x277);
    w->mot[67] = 0;
    w->mot[68] = 0;
    w->mot[69] = ARC(0x26A);
    w->mot[70] = ARC(0x26B);
    w->mot[71] = ARC(0x270);
    w->mot[72] = ARC(0x271);
    w->mot[73] = ARC(0x272);
    w->mot[74] = ARC(0x273);
    w->mot[75] = ARC(0x274);
    w->mot[76] = ARC(0x275);
    w->mot[77] = 0;
    w->mot[78] = 0;
}
