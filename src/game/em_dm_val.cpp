// game/em_dm_val.cpp: weapon damage value per enemy type and weapon level.

#include "atari.h"
#include "em.h"
#include "global.h"

int Dmg_tbl_em10[0x2E] = {
    0, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 9999, 0, 150,
    100, 150, 9999, 2500, 200, 500, 0, 0, 100, 0, 150, 150, 150, 150, 150, 0,
    0, 150, 0, 0, 400, 2000, 150, 150, 150, 2500, 0, 150, 150, 2500,
};

int Dmg_tbl_em31[0x2E] = {
    0, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 2000, 0, 15,
    15, 15, 500, 125, 15, 100, 0, 0, 0, 0, 15, 15, 15, 15, 15, 0,
    0, 15, 0, 0, 30, 60, 15, 15, 15, 250, 0, 15, 15, 125,
};

static int Dmg_tbl_em36[0x2E] = {
    0, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 9999, 0, 30,
    30, 30, 9999, 500, 30, 100, 0, 0, 0, 0, 30, 30, 30, 30, 30, 0,
    0, 30, 0, 0, 120, 240, 30, 30, 30, 500, 0, 30, 30, 500,
};

int Dmg_tbl_em39[0x2E] = {
    0, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 2000, 0, 15,
    400, 15, 500, 250, 15, 50, 0, 0, 0, 0, 600, 15, 15, 15, 15, 0,
    0, 15, 0, 0, 30, 60, 15, 15, 15, 250, 0, 15, 15, 250,
};

static int Dmg_tbl_em3c[0x2E] = {
    0, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 9999, 0, 15,
    15, 15, 2500, 250, 15, 50, 0, 0, 0, 0, 15, 15, 15, 15, 15, 0,
    0, 15, 0, 0, 30, 60, 15, 15, 15, 250, 0, 15, 15, 250,
};

static int Dmg_tbl_em2b[0x2E] = {
    0, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 9999, 0, 30,
    30, 30, 2500, 600, 20, 100, 0, 600, 0, 0, 30, 30, 30, 30, 30, 0,
    0, 30, 0, 0, 40, 80, 30, 30, 30, 600, 0, 30, 30, 600,
};

int Dmg_tbl_em2c[0x2E] = {
    0, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 9999, 0, 25,
    25, 25, 2500, 750, 200, 25, 0, 0, 25, 0, 25, 25, 25, 25, 25, 0,
    0, 25, 0, 0, 400, 800, 25, 25, 25, 750, 0, 25, 25, 750,
};

static int Dmg_tbl_em2d[0x2E] = {
    0, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 9999, 0, 50,
    50, 50, 9999, 750, 100, 50, 0, 0, 25, 0, 50, 50, 50, 50, 50, 0,
    0, 50, 0, 0, 200, 400, 50, 50, 50, 750, 0, 50, 50, 750,
};

static int Dmg_tbl_sml[0x2E] = {
    9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999,
    9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999,
    9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999, 9999,
};

// damage rate per weapon (row) and weapon level (0..6)
f32 WeaponLevelTbl[0x2E][7] = {
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 0.9f, 1.1f, 1.3f, 1.5f, 1.7f, 1.9f, 1.9f },
    { 1.0f, 1.2f, 1.4f, 1.6f, 1.8f, 2.0f, 2.0f },
    { 1.4f, 1.7f, 2.0f, 2.4f, 2.8f, 3.5f, 5.0f },
    { 1.6f, 1.8f, 2.0f, 2.3f, 2.7f, 3.0f, 3.4f },
    { 13.0f, 15.0f, 17.0f, 20.0f, 24.0f, 28.0f, 50.0f },
    { 25.0f, 30.0f, 35.0f, 35.0f, 35.0f, 35.0f, 35.0f },
    { 4.0f, 4.5f, 5.0f, 6.0f, 7.0f, 8.0f, 8.0f },
    { 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 12.0f, 12.0f },
    { 4.0f, 5.0f, 6.0f, 8.0f, 10.0f, 12.0f, 18.0f },
    { 7.0f, 8.0f, 9.0f, 11.0f, 13.0f, 15.0f, 15.0f },
    { 0.4f, 0.5f, 0.6f, 0.8f, 1.0f, 1.2f, 1.8f },
    { 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.2f, 1.4f, 1.6f, 1.8f, 2.0f, 2.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f },
    { 5.0f, 10.0f, 15.0f, 20.0f, 25.0f, 50.0f, 50.0f },
    { 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f },
    { 1.0f, 1.2f, 1.4f, 1.6f, 2.0f, 4.0f, 4.0f },
    { 7.0f, 8.0f, 9.0f, 12.0f, 16.0f, 32.0f, 32.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 5.0f, 5.5f, 6.0f, 6.5f, 7.0f, 8.0f, 10.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 4.0f, 5.0f, 6.0f, 8.0f, 10.0f, 12.0f, 18.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
};

// shotgun rates when flag == 0: rows for weapon 7, 8, 0x21
f32 WeaponLevelTblShotGun[3][7] = {
    { 1.4f, 1.6f, 1.8f, 2.2f, 2.5f, 3.0f, 6.0f },
    { 2.0f, 2.4f, 2.7f, 3.0f, 3.5f, 4.0f, 4.0f },
    { 2.5f, 2.8f, 3.0f, 3.3f, 3.5f, 4.0f, 8.0f },
};

int GetWepDmVal(cEm* em, u32 wep, int flag)
{
    int val;
    u32 lv;
    f32 rate;

    if (wep > 0x2D) {
        wep = 2;
    }
    switch (em->id) {
    case 0x10:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x11:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x12:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x13:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x14:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x15:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x16:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x17:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x18:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x19:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x1A:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x1B:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x1C:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x1D:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x1E:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x1F:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x20:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x22:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x25:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x3A:
        val = Dmg_tbl_em10[wep];
        break;
    case 0x23:
        val = Dmg_tbl_sml[wep];
        break;
    case 0x24:
        val = Dmg_tbl_sml[wep];
        break;
    case 0x27:
        val = Dmg_tbl_sml[wep];
        break;
    case 0x28:
        val = Dmg_tbl_sml[wep];
        break;
    case 0x29:
        val = Dmg_tbl_sml[wep];
        break;
    case 0x2A:
        val = Dmg_tbl_sml[wep];
        break;
    case 0x2B:
        val = Dmg_tbl_em2b[wep];
        break;
    case 0x2C:
        val = Dmg_tbl_em2c[wep];
        break;
    case 0x2D:
        val = Dmg_tbl_em2d[wep];
        break;
    case 0x2E:
        val = Dmg_tbl_sml[wep];
        break;
    case 0x31:
        val = Dmg_tbl_em31[wep];
        break;
    case 0x32:
        val = Dmg_tbl_em3c[wep];
        break;
    case 0x35:
        val = Dmg_tbl_em3c[wep];
        break;
    case 0x36:
        val = Dmg_tbl_em36[wep];
        break;
    case 0x38:
        val = Dmg_tbl_em3c[wep];
        break;
    case 0x39:
        val = Dmg_tbl_em39[wep];
        break;
    case 0x3C:
        val = Dmg_tbl_em3c[wep];
        break;
    default:
        val = Dmg_tbl_em10[wep];
        break;
    }
    lv = pG->wep_lv;
    if (lv > 7) {
        lv = 7;
    }
    rate = WeaponLevelTbl[wep][lv];
    if (flag == 0) {
        switch (wep) {
        case 7:
            rate = WeaponLevelTblShotGun[0][lv];
            break;
        case 8:
            rate = WeaponLevelTblShotGun[1][lv];
            break;
        case 0x21:
            rate = WeaponLevelTblShotGun[2][lv];
            break;
        }
    }
    if (pG->x4FB8 == 4 && wep == 0x14) {
        val *= 10;
    }
    if (pG->x4FB8 == 5 && wep == 0x14) {
        val *= 10;
    }
    if (pG->x4FB8 == 3 && wep == 0x14) {
        val *= 5;
    }
    if (pG->x4FB8 == 2 && wep == 0x14) {
        val *= 2;
    }
    return (int) ((f32) val * rate);
}
