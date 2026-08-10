// game/foot_shadow_tbl.cpp: foot shadow tables of the characters that cast them.

#include "foot_shadow.h"

FootShadowDat pl_fsd_dat[6] = {
    {0x15, 2, 1, 0xFF, 300.0f},
    {0x13, 1, 1, 0xC0, 450.0f},
    {0x12, 1, 0, 0x80, 700.0f},
    {0x19, 2, 1, 0xFF, 300.0f},
    {0x17, 1, 1, 0xC0, 450.0f},
    {0x16, 1, 0, 0x80, 700.0f},
};

FootShadowDat Em10_fsd_dat[6] = {
    {0x15, 1, 1, 0xFF, 300.0f},
    {0x13, 1, 1, 0xC8, 450.0f},
    {0x12, 1, 0, 0x80, 700.0f},
    {0x19, 1, 1, 0xFF, 300.0f},
    {0x17, 1, 1, 0xC8, 450.0f},
    {0x16, 1, 0, 0x80, 700.0f},
};

FootShadowDat Em2b_fsd_dat[6] = {
    {0x15, 5, 1, 0x40, 1200.0f},
    {0x13, 3, 1, 0x80, 1800.0f},
    {0x12, 2, 0, 0x80, 2500.0f},
    {0x19, 5, 1, 0x40, 1200.0f},
    {0x17, 3, 1, 0x80, 1800.0f},
    {0x16, 2, 0, 0x80, 2500.0f},
};

FootShadowDat Em2c_fsd_dat[6] = {
    {0x0C, 1, 1, 0xFF, 1000.0f},
    {0x0B, 1, 1, 0x80, 800.0f},
    {0x0A, 1, 0, 0x80, 800.0f},
    {0x10, 1, 1, 0xFF, 1000.0f},
    {0x0F, 1, 1, 0x80, 800.0f},
    {0x0E, 1, 0, 0x80, 800.0f},
};

FootShadowDat Obm72_fsd_dat[4] = {
    {0x00, 1, 1, 0xFF, 1000.0f},
    {0x01, 1, 0, 0xFF, 1000.0f},
    {0x00, 1, 1, 0xFF, 1000.0f},
    {0x01, 1, 0, 0xFF, 1000.0f},
};

FootShadowDat Em32_fsd_dat[18] = {
    {0x1A, 1, 1, 0x80, 800.0f},
    {0x1B, 1, 1, 0xA0, 800.0f},
    {0x1C, 1, 1, 0xA0, 800.0f},
    {0x1D, 1, 1, 0xA0, 2000.0f},
    {0x1E, 1, 1, 0xA0, 2000.0f},
    {0x1F, 1, 0, 0xFF, 2500.0f},
    {0x21, 1, 1, 0x40, 1000.0f},
    {0x22, 1, 1, 0xA0, 800.0f},
    {0x23, 1, 0, 0xA0, 1000.0f},
    {0x27, 1, 1, 0x40, 1000.0f},
    {0x28, 1, 1, 0xA0, 800.0f},
    {0x29, 1, 0, 0xA0, 1000.0f},
    {0x2D, 1, 1, 0xFF, 1000.0f},
    {0x2E, 1, 1, 0xC8, 1000.0f},
    {0x2F, 1, 0, 0xFF, 1500.0f},
    {0x33, 1, 1, 0xFF, 1000.0f},
    {0x34, 1, 1, 0xC8, 1000.0f},
    {0x35, 1, 0, 0xFF, 1500.0f},
};

FootShadowTbl pl_fs_tbl = {6, pl_fsd_dat};
FootShadowTbl Em10_fs_tbl = {6, Em10_fsd_dat};
FootShadowTbl Em2b_fs_tbl = {6, Em2b_fsd_dat};
FootShadowTbl Em2c_fs_tbl = {6, Em2c_fsd_dat};
static FootShadowTbl Em32_fs_tbl = {18, Em32_fsd_dat};
FootShadowTbl Em39_fs_tbl = {6, pl_fsd_dat};
