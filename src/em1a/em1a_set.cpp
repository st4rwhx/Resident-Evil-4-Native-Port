// em1a: the per-enemy object of a Ganado module (the real file name is not in the binary; the
// module is em10.cpp + this file). _prolog registers the enemy's Init/Set functions with the DOL
// (EmInitFunc) and the shared em10.cpp (Em10SetFunc); Em1aInit constructs the shared cEm10 class in
// the manager's work, Em1aSet / Em1aWeaponSet fill the Ganado work's motion table from the enemy
// archive (cEm::subArc) by model type.

#include "types.h"
#include "atari.h"
#include "global.h"
#include "cManager.h"
#include "em10.h"

extern "C" void OSReport(const char* fmt, ...);
extern "C" void Em10SetSeTbl(cEm10* em, int type);

extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

void Em1aInit(cEm* em);
void Em1aSet(cEm10* em);
void Em1aWeaponSet(cEm10* em);

#define ARC(no) PL_ARC_PTR(em->subArc, no)

extern "C" void _prolog()
{
    OSReport("em10 prolog Ok\n");
    EmInitFunc = Em1aInit;
    Em10SetFunc = Em1aSet;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em1aInit(cEm* em)
{
    new (em) cEm10();
}

void Em1aSet(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->type) {
    case 7:
    default:
        em->type = 7;
        w->mot[0] = ARC(0x1FD);
        w->mot[1] = ARC(0x1FC);
        w->mot[2] = ARC(0x1FE);
        w->mot[3] = ARC(0x1FF);
        w->mot[4] = ARC(0x200);
        w->mot[5] = ARC(0x201);
        w->mot[6] = ARC(0x204);
        w->mot[7] = ARC(0x205);
        w->mot[8] = ARC(0x206);
        w->mot[9] = ARC(0x207);
        w->mot[10] = ARC(0x207);
        w->mot[10] = ARC(0x207);
        w->mot[11] = ARC(0x208);
        w->mot[12] = ARC(0x209);
        w->mot[13] = ARC(0x20A);
        w->mot[14] = ARC(0x20B);
        w->mot[15] = ARC(0x20C);
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
        w->mot[26] = ARC(0x202);
        w->mot[27] = ARC(0x203);
        w->mot[28] = ARC(0x20D);
        w->mot[29] = ARC(0x20E);
        w->mot[30] = ARC(0x20F);
        w->mot[31] = ARC(0x210);
        w->mot[32] = ARC(0x211);
        w->mot[33] = ARC(0x212);
        w->mot[34] = ARC(0x213);
        w->mot[35] = ARC(0x214);
        w->mot[36] = ARC(0x215);
        w->mot[37] = ARC(0x216);
        w->mot[38] = ARC(0x217);
        w->mot[39] = ARC(0x218);
        w->mot[40] = ARC(0x219);
        if (em->emsetNo & 1) {
            Em10SetSeTbl(em, 0);
        } else {
            Em10SetSeTbl(em, 2);
        }
        break;
    case 8:
        em->type = 8;
        w->mot[0] = ARC(0x21A);
        w->mot[1] = ARC(0x1FC);
        w->mot[2] = ARC(0x21B);
        w->mot[3] = ARC(0x1FF);
        w->mot[4] = ARC(0x200);
        w->mot[5] = ARC(0x21C);
        w->mot[6] = ARC(0x204);
        w->mot[7] = ARC(0x205);
        w->mot[8] = ARC(0x206);
        w->mot[9] = ARC(0x207);
        w->mot[10] = ARC(0x207);
        w->mot[10] = ARC(0x207);
        w->mot[11] = ARC(0x208);
        w->mot[12] = ARC(0x209);
        w->mot[13] = ARC(0x20A);
        w->mot[14] = ARC(0x20B);
        w->mot[15] = ARC(0x20C);
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
        w->mot[26] = ARC(0x202);
        w->mot[27] = ARC(0x203);
        w->mot[28] = ARC(0x20D);
        w->mot[29] = ARC(0x20E);
        w->mot[30] = ARC(0x20F);
        w->mot[31] = ARC(0x210);
        w->mot[32] = ARC(0x211);
        w->mot[33] = ARC(0x212);
        w->mot[34] = ARC(0x213);
        w->mot[35] = ARC(0x214);
        w->mot[36] = ARC(0x215);
        w->mot[37] = ARC(0x216);
        w->mot[38] = ARC(0x217);
        w->mot[39] = ARC(0x218);
        w->mot[40] = ARC(0x219);
        Em10SetSeTbl(em, 3);
        break;
    case 9:
        w->mot[0] = ARC(0x21E);
        w->mot[1] = ARC(0x21D);
        w->mot[2] = ARC(0x21F);
        w->mot[3] = ARC(0x1FF);
        w->mot[4] = ARC(0x200);
        w->mot[5] = ARC(0x201);
        w->mot[6] = ARC(0x204);
        w->mot[7] = ARC(0x205);
        w->mot[8] = ARC(0x206);
        w->mot[9] = ARC(0x207);
        w->mot[10] = ARC(0x207);
        w->mot[10] = ARC(0x207);
        w->mot[11] = ARC(0x208);
        w->mot[12] = ARC(0x209);
        w->mot[13] = ARC(0x20A);
        w->mot[14] = ARC(0x20B);
        w->mot[15] = ARC(0x20C);
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
        w->mot[26] = ARC(0x202);
        w->mot[27] = ARC(0x203);
        w->mot[28] = ARC(0x20D);
        w->mot[29] = ARC(0x20E);
        w->mot[30] = ARC(0x20F);
        w->mot[31] = ARC(0x210);
        w->mot[32] = ARC(0x211);
        w->mot[33] = ARC(0x212);
        w->mot[34] = ARC(0x213);
        w->mot[35] = ARC(0x214);
        w->mot[36] = ARC(0x215);
        w->mot[37] = ARC(0x216);
        w->mot[38] = ARC(0x217);
        w->mot[39] = ARC(0x218);
        w->mot[40] = ARC(0x219);
        if (em->emsetNo & 1) {
            Em10SetSeTbl(em, 0);
        } else {
            Em10SetSeTbl(em, 2);
        }
        break;
    }
    w->x6C5 = 1;
    Em1aWeaponSet(em);
}

void Em1aWeaponSet(cEm10* em)
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
    w->mot[63] = 0;
    w->mot[64] = 0;
    w->mot[65] = ARC(0x276);
    w->mot[66] = ARC(0x277);
    w->mot[67] = 0;
    w->mot[68] = 0;
    w->mot[69] = ARC(0x26E);
    w->mot[70] = ARC(0x26F);
    w->mot[71] = ARC(0x270);
    w->mot[72] = ARC(0x271);
    w->mot[73] = ARC(0x272);
    w->mot[74] = ARC(0x273);
    w->mot[75] = ARC(0x274);
    w->mot[76] = ARC(0x275);
    w->mot[77] = 0;
    w->mot[78] = 0;
}
