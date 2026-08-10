#include "snd_drv.h"

s32 Snd_dls_vol_tbl[128] = {
    -904, -842, -721, -651, -601, -562, -530, -503,
    -480, -460, -442, -425, -410, -396, -383, -371,
    -360, -349, -339, -330, -321, -313, -305, -297,
    -289, -282, -276, -269, -263, -257, -251, -245,
    -239, -234, -229, -224, -219, -214, -210, -205,
    -201, -196, -192, -188, -184, -180, -176, -173,
    -169, -165, -162, -158, -155, -152, -149, -145,
    -142, -139, -136, -133, -130, -127, -125, -122,
    -119, -116, -114, -111, -109, -106, -103, -101,
    -99, -96, -94, -91, -89, -87, -85, -82,
    -80, -78, -76, -74, -72, -70, -68, -66,
    -64, -62, -60, -58, -56, -54, -52, -50,
    -49, -47, -45, -43, -42, -40, -38, -36,
    -35, -33, -31, -30, -28, -27, -25, -23,
    -22, -20, -19, -17, -16, -14, -13, -11,
    -10, -8, -7, -6, -4, -3, -1, 0,
};

SND_LPF Snd_lpf_tbl[24] = {
    {0x6A09, 0x15F6, "16000"},
    {0x6871, 0x178E, "12800"},
    {0x6463, 0x1B9C, "10240"},
    {0x5DB3, 0x224C, " 8000"},
    {0x5618, 0x29E7, " 6400"},
    {0x4D7A, 0x3285, " 5120"},
    {0x4367, 0x3C98, " 4000"},
    {0x3A5A, 0x45A5, " 3200"},
    {0x31C5, 0x4E3A, " 2560"},
    {0x2924, 0x56DB, " 2000"},
    {0x2244, 0x5DBB, " 1600"},
    {0x1C50, 0x63AF, " 1280"},
    {0x16C0, 0x693F, " 1000"},
    {0x1292, 0x6D6D, "  800"},
    {0x0F18, 0x70E7, "  640"},
    {0x0BF5, 0x740A, "  500"},
    {0x09A9, 0x7656, "  400"},
    {0x07CA, 0x7835, "  320"},
    {0x0646, 0x79B9, "  256"},
    {0x04ED, 0x7B12, "  200"},
    {0x03F5, 0x7C0A, "  160"},
    {0x032D, 0x7CD2, "  128"},
    {0x027D, 0x7D82, "  100"},
    {0x01FE, 0x7E01, "   80"},
};

int Snd_pronounce_ck(void)
{
    int ret;

    ret = Snd_se_pronounce_ck_all();
    ret |= Snd_seq_pronounce_ck_type(3);
    ret |= Snd_str_pronounce_ck_type(3);
    return ret;
}

void Snd_soft_reset_req(void)
{
    Snd_ctrl_work.reset_flag = 1;
}

int Snd_soft_reset_ck(void)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
    int ret;

    if ((ctrl->reset_flag & 0x70) != 0x70) {
        return 0x8000;
    }
    ret = Snd_pronounce_ck();
    if (ret == 0) {
        ctrl->reset_flag = 0;
    }
    return ret;
}

void Snd_reset_pan_all(void)
{
    Snd_ctrl_work.se_ctrl |= 0x80;
    Snd_str_reset_pan_type(3);
}

void Snd_reset_vol_all(void)
{
    Snd_ctrl_work.se_ctrl |= 0x100;
    Snd_str_reset_vol_type(3);
    Snd_seq_reset_vol_type(3);
}

void Snd_set_system_vol(s16 type, u16 vol)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
    u16 v;

    v = vol << 8;
    if (type & 0x1) {
        ctrl->sys_vol[0] = v;
    }
    if (type & 0x2) {
        ctrl->sys_vol[1] = v;
    }
    if (type & 0x4) {
        ctrl->sys_vol[2] = v;
    }
    if (type & 0x8) {
        ctrl->sys_vol[3] = v;
    }
    if (type & 0x10) {
        ctrl->sys_vol[4] = v;
    }
    if (type & 0x20) {
        ctrl->sys_vol[5] = v;
    }
}

s16 Snd_get_system_vol(s16 type)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
    s16 v = 0;

    if (type == 1) {
        v = ctrl->sys_vol[0];
    }
    if (type == 2) {
        v = ctrl->sys_vol[1];
    }
    if (type == 4) {
        v = ctrl->sys_vol[2];
    }
    if (type == 8) {
        v = ctrl->sys_vol[3];
    }
    if (type == 16) {
        v = ctrl->sys_vol[4];
    }
    if (type == 32) {
        v = ctrl->sys_vol[5];
    }
    return v >> 8;
}

s32 Snd_vol_syn_to_ax(s32 vol)
{
    return Snd_dls_vol_tbl[vol];
}

s32 Snd_vol_ax_to_syn(s32 vol)
{
    int i;

    if (vol <= -904) {
        return 0;
    }
    for (i = 1; i < 128; i++) {
        if (vol <= Snd_dls_vol_tbl[i]) {
            return i;
        }
    }
    OSReport("SND Volume Error !!\n");
    return -1;
}

u8 Snd_rnd(void)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
    SND_RND r;
    SND_RND r2;

    r.w = ctrl->rnd;
    r2.w = r.w * 3;
    r.b[1] += r2.b[0];
    r.b[0] = r2.b[0];
    if (ctrl->rnd == r.w) {
        ctrl->rnd++;
    } else {
        ctrl->rnd = r.w;
    }
    return r.b[1];
}

s16 Snd_get_rnd_pitch(SND_SIT* sit)
{
    s16 lo;
    s16 hi;
    s16 range;
    s16 r;
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;

    lo = sit->pitch_lo;
    hi = sit->pitch_hi;
    if (lo == hi) {
        ctrl->rnd_pitch = lo;
        return lo;
    }
    range = hi - lo;
    if (range <= 16) {
        r = Snd_rnd() % (range + 1);
    } else {
        r = Snd_rnd() % 17;
        r *= range / 16;
    }
    r += lo;
    ctrl->rnd_pitch = r;
    return r;
}

void Snd_test_work_clear(void)
{
    SND_TEST_WORK* test;
    u32 i;
    u8* p;

    p = (u8*) &Snd_test_work;
    for (i = 0; i < sizeof(SND_TEST_WORK); i++) {
        *p++ = 0;
    }
    test = &Snd_test_work;
    test->x1 = 0;
    test->x3 = 0;
    test->xC = 1;
    test->xE = 7;
    test->x18 = 0xE;
    test->x1A = 2;
    strcpy(test->path0, "/");
    strcpy(test->path1, "/");
    for (i = 0; i < 14; i++) {
        test->x51C[i] = 0;
    }
    for (i = 0; i < 2; i++) {
        test->x554[i] = 0;
    }
    test->aram_base = Snd_ctrl_work.aram_base;
}
