// em1d: the per-enemy object of a Ganado module (the real file name is not in the binary; the
// module is em10.cpp + this file). _prolog registers the enemy's Init/Set functions with the DOL
// (EmInitFunc) and the shared em10.cpp (Em10SetFunc); Em1dInit constructs the shared cEm10 class in
// the manager's work, Em1dSet / Em1dWeaponSet fill the Ganado work's motion table from the enemy
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

void Em1dInit(cEm* em);
void Em1dSet(cEm10* em);
void Em1dWeaponSet(cEm10* em);

#define ARC(no) PL_ARC_PTR(em->subArc, no)

extern "C" void _prolog()
{
    OSReport("em10 prolog Ok\n");
    EmInitFunc = Em1dInit;
    Em10SetFunc = Em1dSet;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em1dInit(cEm* em)
{
    new (em) cEm10();
}

void Em1dSet(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->type) {
    case 14:
    case 18:
    default:
        if (em->type == 18) {
            w->mot[0] = ARC(0x248);
            w->mot[1] = ARC(0x247);
        } else {
            w->mot[0] = ARC(0x230);
            w->mot[1] = ARC(0x22F);
            em->type = 14;
        }
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
    case 19:
        if (em->type == 15) {
            w->mot[0] = ARC(0x230);
            w->mot[1] = ARC(0x22F);
        } else {
            w->mot[0] = ARC(0x248);
            w->mot[1] = ARC(0x247);
        }
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
    case 16:
    case 20:
        if (em->type == 16) {
            w->mot[0] = ARC(0x230);
            w->mot[1] = ARC(0x22F);
        } else {
            w->mot[0] = ARC(0x248);
            w->mot[1] = ARC(0x247);
        }
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
        Em10SetSeTbl(em, 3);
        break;
    case 17:
        w->mot[0] = ARC(0x230);
        w->mot[1] = ARC(0x22F);
        // types 17 and 21 share the rest of the arm through this label: the target's
        // case 21 block starts at mot[2] (its zero-store schedule is the other arms'),
        // which two full copies of the body do not give (the cross-jump happens after sched1)
        goto common17;
    case 21:
        w->mot[0] = ARC(0x248);
        w->mot[1] = ARC(0x247);
    common17:
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
    case 2:
        w->mot[0] = ARC(0x22C);
        w->mot[1] = ARC(0x22B);
        w->mot[2] = 0;
        w->mot[3] = 0;
        w->mot[4] = 0;
        w->mot[5] = 0;
        w->mot[6] = 0;
        w->mot[7] = 0;
        w->mot[8] = 0;
        w->mot[9] = 0;
        w->mot[10] = 0;
        w->mot[11] = 0;
        w->mot[12] = 0;
        w->mot[13] = 0;
        w->mot[14] = 0;
        w->mot[15] = 0;
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
    }
    w->x6C5 = 2;
    Em1dWeaponSet(em);
    EspDataLoad((u32) ARC(0x1A4), 0xcc, 0);
    EspDataLoad((u32) ARC(0x278), 0xcd, 0);
}

void Em1dWeaponSet(cEm10* em)
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
