#include "types.h"
#include "snd_test.h"
#include "eprintf.h"

extern "C" {
char* strcat(char* dst, const char* src);
}

// Sound test of the t_movie REL (snd_test.cpp; the file name is not in the binary). Plays ISS / stream
// requests, edits their SIT/RIT parameters and the AUX effects, shows the driver state and dumps ARAM.
// Every function works on the driver's Snd_test_work (snd_test.h).

// One editable SIT/RIT parameter.
struct TestPara {
    void* ptr;   // 0x00  the field inside Snd_test_work.sit / .rit
    s8 type;     // 0x04  0 s8, 1 s16, 3..7 flag bits, 8 s8 with max 3 (stream number)
    s8 kind;     // 0x05  move_type_tbl index
    s16 min;     // 0x06
    s16 max;     // 0x08
    s16 step;    // 0x0A
    s16 fast;    // 0x0C  step with the R trigger held
    s16 pad_E;
};

// One editable AUX effect parameter (float / integer versions share the layout).
struct EfxParaF {
    f32 min;
    f32 max;
    f32 step;
    f32 fast;
    f32* ptr[2];  // 0x10  AUX A / AUX B
};

struct EfxParaU {
    u32 min;
    u32 max;
    u32 step;
    u32 fast;
    u32* ptr[2];
};

static TestPara test_para_sit[] = {
    {&Snd_test_work.sit.prog, 1, 4, 0, 0, 0, 0},
    {&Snd_test_work.sit.prog, 1, 4, 0, 0, 0, 0},
    {&Snd_test_work.sit.curve_no, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.sit.vol, 0, 0, -1, 0x7F, 1, 10},
    {&Snd_test_work.sit.svol, 0, 0, -1, 0x7F, 1, 10},
    {&Snd_test_work.sit.x7, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.sit.x8, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.sit.pitch_lo, 1, 0, -2400, 2400, 1, 100},
    {&Snd_test_work.sit.pitch_hi, 1, 0, -2400, 2400, 1, 100},
    {&Snd_test_work.sit.voice_start, 0, 0, 0, 0x3F, 1, 10},
    {&Snd_test_work.sit.voice_num, 0, 0, 0, 0x3F, 1, 10},
    {&Snd_test_work.sit.prio, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.sit.flag, 4, 3, 0, 0, 0, 0},
    {&Snd_test_work.sit.pan, 0, 0, -2, 0x7F, 1, 10},
    {&Snd_test_work.sit.span, 0, 0, -2, 0x7F, 1, 10},
    {&Snd_test_work.sit.srd_type, 0, 0, 0, 3, 1, 1},
    {&Snd_test_work.sit.flag, 3, 3, 0, 0, 0, 0},
    {&Snd_test_work.sit.rnd_no, 0, 0, -128, 0x7F, 1, 10},
    {&Snd_test_work.sit.se_flag, 0, 0, -128, 0x7F, 1, 10},
    {&Snd_test_work.sit.wall_vol, 0, 0, -128, 0x7F, 1, 10},
    {&Snd_test_work.sit.inner_vol, 0, 0, -128, 0x7F, 1, 10},
    {&Snd_test_work.sit.xF, 0, 0, -128, 0x7F, 1, 10},
};

static TestPara test_para_midi[] = {
    {&Snd_test_work.sit.prog, 1, 4, 0, 0, 0, 0},
    {&Snd_test_work.sit.curve_no, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.sit.vol, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.sit.voice_start, 0, 0, 0, 0x3F, 1, 10},
    {&Snd_test_work.sit.voice_num, 0, 0, 0, 0x3F, 1, 10},
    {&Snd_test_work.sit.flag, 5, 3, 0, 0, 0, 0},
    {&Snd_test_work.sit.flag, 6, 3, 0, 0, 0, 0},
};

static TestPara test_para_rit[] = {
    {&Snd_test_work.rit.shd_no, 1, 4, 0, 0, 0, 0},
    {&Snd_test_work.rit.pad_6[0], 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.rit.vol, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.rit.pad_B[0], 0, 4, 0, 0, 0, 0},
    {&Snd_test_work.rit.flag, 7, 3, 0, 0, 0, 0},
    {&Snd_test_work.rit.voice_start, 0, 0, 0, 0x3F, 1, 10},
    {&Snd_test_work.rit.str_no, 8, 0, 0, 0, 1, 1},
    {&Snd_test_work.rit.auxA, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.rit.auxB, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.rit.pad_6[1], 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.rit.pan, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.rit.span, 0, 0, -1, 0x7F, 1, 10},
    {&Snd_test_work.rit.svol, 0, 0, -1, 0x7F, 1, 10},
};

static void move_type_num(SndTestWork* w, TestPara* para);
static void move_type_note(SndTestWork* w, TestPara* para);
static void move_type_ext(SndTestWork* w, TestPara* para);
static void move_type_flag(SndTestWork* w, TestPara* para);
static void move_type_nop(SndTestWork* w, TestPara* para);

// parameters per SIT type (0 dummy, 1 normal, 2 ADSR, 3 MIDI)
static s8 test_para_max[4] = {0, 0x16, 0x16, 7};
static void (*move_type_tbl[5])(SndTestWork*, TestPara*) = {move_type_num, move_type_note, move_type_ext,
                                                            move_type_flag, move_type_nop};

static EfxParaF efx_para_rev_hi[6] = {
    {0.0f, 0.1f, 0.05f, 0.05f, {&Snd_efx_work[0].fx.hi.preDelay, &Snd_efx_work[1].fx.hi.preDelay}},
    {0.0f, 10.0f, 0.125f, 1.0f, {&Snd_efx_work[0].fx.hi.time, &Snd_efx_work[1].fx.hi.time}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].fx.hi.coloration, &Snd_efx_work[1].fx.hi.coloration}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].fx.hi.damping, &Snd_efx_work[1].fx.hi.damping}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].fx.hi.crosstalk, &Snd_efx_work[1].fx.hi.crosstalk}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].fx.hi.mix, &Snd_efx_work[1].fx.hi.mix}},
};
static EfxParaF efx_para_rev_std[5] = {
    {0.0f, 0.1f, 0.05f, 0.05f, {&Snd_efx_work[0].fx.std.preDelay, &Snd_efx_work[1].fx.std.preDelay}},
    {0.0f, 10.0f, 0.125f, 1.0f, {&Snd_efx_work[0].fx.std.time, &Snd_efx_work[1].fx.std.time}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].fx.std.coloration, &Snd_efx_work[1].fx.std.coloration}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].fx.std.damping, &Snd_efx_work[1].fx.std.damping}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].fx.std.mix, &Snd_efx_work[1].fx.std.mix}},
};
static EfxParaU efx_para_chorus[3] = {
    {5, 15, 1, 5, {&Snd_efx_work[0].fx.chorus.baseDelay, &Snd_efx_work[1].fx.chorus.baseDelay}},
    {0, 5, 1, 1, {&Snd_efx_work[0].fx.chorus.variation, &Snd_efx_work[1].fx.chorus.variation}},
    {500, 10000, 1, 1000, {&Snd_efx_work[0].fx.chorus.period, &Snd_efx_work[1].fx.chorus.period}},
};
static EfxParaU efx_para_delay[9] = {
    {10, 5000, 1, 100, {&Snd_efx_work[0].fx.delay.delay[0], &Snd_efx_work[1].fx.delay.delay[0]}},
    {10, 5000, 1, 100, {&Snd_efx_work[0].fx.delay.delay[1], &Snd_efx_work[1].fx.delay.delay[1]}},
    {10, 5000, 1, 100, {&Snd_efx_work[0].fx.delay.delay[2], &Snd_efx_work[1].fx.delay.delay[2]}},
    {0, 100, 1, 10, {&Snd_efx_work[0].fx.delay.feedback[0], &Snd_efx_work[1].fx.delay.feedback[0]}},
    {0, 100, 1, 10, {&Snd_efx_work[0].fx.delay.feedback[1], &Snd_efx_work[1].fx.delay.feedback[1]}},
    {0, 100, 1, 10, {&Snd_efx_work[0].fx.delay.feedback[2], &Snd_efx_work[1].fx.delay.feedback[2]}},
    {0, 100, 1, 10, {&Snd_efx_work[0].fx.delay.output[0], &Snd_efx_work[1].fx.delay.output[0]}},
    {0, 100, 1, 10, {&Snd_efx_work[0].fx.delay.output[1], &Snd_efx_work[1].fx.delay.output[1]}},
    {0, 100, 1, 10, {&Snd_efx_work[0].fx.delay.output[2], &Snd_efx_work[1].fx.delay.output[2]}},
};
static EfxParaF efx_para_rev_dpl2[5] = {
    {0.0f, 0.1f, 0.05f, 0.05f, {&Snd_efx_work[0].fx.dpl2.preDelay, &Snd_efx_work[1].fx.dpl2.preDelay}},
    {0.0f, 10.0f, 0.125f, 1.0f, {&Snd_efx_work[0].fx.dpl2.time, &Snd_efx_work[1].fx.dpl2.time}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].fx.dpl2.coloration, &Snd_efx_work[1].fx.dpl2.coloration}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].fx.dpl2.damping, &Snd_efx_work[1].fx.dpl2.damping}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].fx.dpl2.mix, &Snd_efx_work[1].fx.dpl2.mix}},
};

static int test_move_epara_f32(SndTestWork* w);
static int test_move_epara_u32(SndTestWork* w);

static int (*test_move_epara_tbl[6])(SndTestWork*) = {NULL, test_move_epara_f32, test_move_epara_f32,
                                                      test_move_epara_u32, test_move_epara_u32, test_move_epara_f32};
// parameters per effect type
static s8 test_aux_para_num[6] = {0, 6, 5, 3, 9, 5};
static s16* vol_ptr_tbl[6] = {&Snd_ctrl_work.sys_vol[0], &Snd_ctrl_work.sys_vol[1], &Snd_ctrl_work.sys_vol[2],
                              &Snd_ctrl_work.sys_vol[3], &Snd_ctrl_work.sys_vol[4], &Snd_ctrl_work.sys_vol[5]};

int Snd_test_mode();
void test_mode_menu(SndTestWork* w);
void test_mode_move(SndTestWork* w);
int test_sit_init(SndTestWork* w);
int test_rit_init(SndTestWork* w);
int test_sit_or_rit(SndTestWork* w);
void test_tbl_now_check(SndTestWork* w, int dir, int max);
TestPara* test_get_tpara_adrs(SndTestWork* w);
int test_tbl_para_select(SndTestWork* w, int max);
s16 test_para_ck_s16(SndTestWork* w, TestPara* para, s16 val);
int test_play_or_stop(SndTestWork* w);
int test_req_no_select(SndTestWork* w, s16 max);
void test_blk_no_select(SndTestWork* w);
int test_blk_enable_ck(SndTestWork* w, int dir);
void Snd_test_efx_init(SndTestWork* w);
int Snd_test_efx_main(SndTestWork* w);
int test_efx_type_select(SndTestWork* w, SND_EFX_WORK* efx);
int test_efx_on_or_off(SndTestWork* w, SND_EFX_WORK* efx);
int test_aux_para_select(SndTestWork* w);
void test_tbl_aux_ck(SndTestWork* w);
int Snd_test_volume(SndTestWork* w);
void Snd_test_disp_move(SndTestWork* w);
static void snd_test_disp_sit();
void disp_sit_type(SndTestWork* w, SND_SIT* sit);

void disp_sit_normal(SND_ISS_BLK* blk, SND_SIT* sit, int x, int y);
void disp_sit_midi(SND_ISS_BLK* blk, SND_SIT* sit, int x, int y);
void disp_seq_volume(SND_SEQ_WORK* seq);
static void snd_test_disp_rit();
// Snd_test_get_str_name with the caller's second argument (see test_play_or_stop).
const char* Snd_test_get_str_name2(int type, u16 no) asm("Snd_test_get_str_name__Fi");
int str_get_player_id();
void disp_cursor(SndTestWork* w, int x, int y);
void disp_str_status(SndTestWork* w, int x, int y);
void disp_sequencer();
void disp_se_wt_data(SndTestWork* w, SND_ISS_BLK* blk, SND_SIT* sit);
void get_wt_ptr(SndTestWork* w, SND_ISS_BLK* blk, SND_SIT* sit);
void disp_adsr_para(SndTestWork* w);
void get_axv_ptr(SndTestWork* w);
void Snd_test_disp_menu(SndTestWork* w);
void Snd_test_disp_basic(SndTestWork* w);
void Snd_test_disp_voice(SndTestWork* w);
void Snd_test_disp_req_para(SndTestWork* w);
void Snd_test_disp_aux(SndTestWork* w);
void Snd_test_disp_efx();
static void test_disp_efx_rev_hi(SndTestWork* w, SND_EFX_WORK* efx, int x, int y);
static void test_disp_efx_rev_std(SndTestWork* w, SND_EFX_WORK* efx, int x, int y);
static void test_disp_efx_chorus(SndTestWork* w, SND_EFX_WORK* efx, int x, int y);
static void test_disp_efx_delay(SndTestWork* w, SND_EFX_WORK* efx, int x, int y);
static void test_disp_efx_rev_dpl2(SndTestWork* w, SND_EFX_WORK* efx, int x, int y);
void Snd_test_disp_vol();
void Snd_test_load_init(SndTestWork* w);
void Snd_test_mode_load(SndTestWork* w);
int blk_no_check(SndTestWork* w);
void load_select(SndTestWork* w);
void load_sit_data(SndTestWork* w, char* name);
void load_rit_data(SndTestWork* w, char* name);
void directory_open(SndTestWork* w);
void dir_entry_read(SndTestWork* w, int dirs);
void directory_disp(SndTestWork* w);
void cursor_disp(SndTestWork* w);
void blk_file_disp(SndTestWork* w);
void dir_name_up(SndTestWork* w);
void change_to_cap(char* s);
char* get_file_ext(char* s);
int get_dir_level(char* s);
void Snd_test_dump_init(SndTestWork* w);
void Snd_test_aram_dump(SndTestWork* w);
void aram_blk_no_select(SndTestWork* w);
void blk_enable_ck(SndTestWork* w, int dir);
void aram_dump_dma(SndTestWork* w);
static void cb_dma_end(u32 task);
void aram_dump_disp(SndTestWork* w);

int Snd_test_mode()
{
    SndTestWork* w = &Snd_test_work;

    if (w->trg & 0x1010) {
        return 1;
    }
    if (w->menu != 0) {
        test_mode_menu(w);
    } else if (w->trg & 0x800) {
        w->menu = 1;
    } else {
        switch (w->mode) {
        case 5:
            Snd_test_aram_dump(w);
            break;
        case 6:
        case 7:
            Snd_test_mode_load(w);
            break;
        default:
            test_mode_move(w);
            break;
        }
    }
    Snd_test_disp_basic(w);
    w->frame++;
    return 0;
}

void test_mode_menu(SndTestWork* w)
{
    if (w->trg & 0x900) {
        w->menu = 0;
        switch (w->mode) {
        case 0:
            w->tbl = 0;
            break;
        case 1:
            w->tbl = 1;
            break;
        case 2:
            w->aux = 0;
            Snd_test_efx_init(w);
            break;
        case 3:
            w->aux = 1;
            Snd_test_efx_init(w);
            break;
        case 5:
            Snd_test_dump_init(w);
            break;
        case 6:
            w->loadTbl = 0;
            Snd_test_load_init(w);
            return;
        case 7:
            w->loadTbl = 1;
            Snd_test_load_init(w);
            return;
        }
        if (test_blk_enable_ck(w, 1) == 1) {
            w->menu = 1;
        }
    } else {
        if (w->rep & 8) {
            w->mode--;
        }
        if (w->rep & 4) {
            w->mode++;
        }
        if (w->mode & 0x80) {
            w->mode = 5;
        }
        if (w->mode > 5) {
            w->mode = 0;
        }
        Snd_test_disp_menu(w);
    }
}

void test_mode_move(SndTestWork* w)
{
    int ret;

    if (w->tbl == 0) {
        ret = test_sit_init(w);
    } else {
        ret = test_rit_init(w);
    }
    w->cur = w->cursor[w->tbl];
    w->efxCur = w->efxType[w->aux];
    w->auxCur = w->auxCursor[w->aux];
    if (ret == 0) {
        switch (w->mode) {
        case 0:
        case 1:
            ret = test_sit_or_rit(w);
            break;
        case 2:
        case 3:
            ret = Snd_test_efx_main(w);
            break;
        case 4:
            ret = Snd_test_volume(w);
            break;
        }
        if (ret == 0) {
            ret = test_play_or_stop(w);
        }
    }
    Snd_test_disp_move(w);
    w->reqNo[w->tbl] = w->reqCur;
    if (w->tbl == 0) {
        *w->pSit = w->sit;
    } else {
        *w->pRit = w->rit;
    }
    w->cursor[w->tbl] = w->cur;
    w->efxType[w->aux] = w->efxCur;
    w->auxCursor[w->aux] = w->auxCur;
    if (ret == 0) {
        test_blk_no_select(w);
    }
}

int test_sit_init(SndTestWork* w)
{
    SND_ISS_BLK* blk = &Snd_iss_blk[w->blkNo[0]];
    int ret;

    ret = test_req_no_select(w, blk->num);
    w->pSit = &blk->sit[w->reqCur];
    w->sit = *w->pSit;
    if (w->pSit->flag & 0x8000) {
        w->type = 0;
    }
    if (w->pSit->flag & 0x1) {
        w->type = 1;
    }
    if (w->pSit->flag & 0x2) {
        w->type = 2;
    }
    if (w->pSit->flag & 0x4) {
        w->type = 3;
    }
    return ret;
}

int test_rit_init(SndTestWork* w)
{
    SND_STR_BLK* blk = &Snd_str_blk[w->blkNo[1]];
    int ret;

    ret = test_req_no_select(w, blk->num);
    w->pRit = &blk->rit[w->reqCur];
    w->rit = *w->pRit;
    return ret;
}

int test_sit_or_rit(SndTestWork* w)
{
    s8 max;
    TestPara* para;

    if (w->tbl == 0) {
        s8 type = w->type;

        if (type == 0) {
            return 1;
        }
        if (w->trg & 0x00F00000) {
            if (type == 3) {
                w->seqDisp ^= 1;
            } else {
                w->wtDisp ^= 1;
            }
            return 1;
        }
        max = test_para_max[type];
    } else {
        if (w->rit.flag & 0x8000) {
            return 1;
        }
        max = 0xD;
    }
    test_tbl_now_check(w, 1, max);
    if (test_tbl_para_select(w, max) != 0) {
        return 1;
    }
    if (!(w->rep & 0xF)) {
        return 0;
    }
    if (w->tbl == 1) {
        Snd_str_fade_out_type(3, 0);
    }
    para = test_get_tpara_adrs(w);
    move_type_tbl[para->kind](w, para);
    return 1;
}

// skips the parameters the table marks as unselectable (kind 4) in direction `dir`
void test_tbl_now_check(SndTestWork* w, int dir, int max)
{
    for (;;) {
        if (w->cur & 0x80) {
            w->cur = max - 1;
        }
        if (w->cur >= max) {
            w->cur = 0;
        }
        if (test_get_tpara_adrs(w)->kind != 4) {
            break;
        }
        {
            // the byte in an int local (zero-extended read): a direct `w->cur += dir` is a paradoxical
            // SUBREG of the QI load, which cse's fold_rtx always puts first (`add cur,dir`)
            int c = (u8) w->cur;
            w->cur = dir + c;
        }
    }
}

TestPara* test_get_tpara_adrs(SndTestWork* w)
{
    // integer form: `add r3, idx, tbl` operand order; one result variable: the index temp is not
    // tied to r3 by local-alloc (the result pseudo is global), so it lands in r0
    TestPara* p;

    if (w->tbl == 0) {
        if (w->type == 3) {
            p = (TestPara*) (w->cur * sizeof(TestPara) + (u32) test_para_midi);
        } else {
            p = (TestPara*) (w->cur * sizeof(TestPara) + (u32) test_para_sit);
        }
    } else {
        p = (TestPara*) (w->cur * sizeof(TestPara) + (u32) test_para_rit);
    }
    return p;
}

int test_tbl_para_select(SndTestWork* w, int max)
{
    s8 old;

    if (w->on & 0x400) {
        return 0;
    }
    old = w->cur;
    if (w->rep & 8) {
        w->cur--;
        test_tbl_now_check(w, -1, max);
    }
    if (w->rep & 4) {
        w->cur++;
        test_tbl_now_check(w, 1, max);
    }
    if (w->cur != old) {
        return 1;
    }
    return 0;
}

static void move_type_num(SndTestWork* w, TestPara* para)
{
    if (para->type == 0) {
        *(s8*) para->ptr = test_para_ck_s16(w, para, *(s8*) para->ptr);
    } else if (para->type == 1) {
        *(s16*) para->ptr = test_para_ck_s16(w, para, *(s16*) para->ptr);
    } else if (para->type == 8) {
        para->max = 3;
        *(s8*) para->ptr = test_para_ck_s16(w, para, *(s8*) para->ptr);
    }
}

// bank (high byte) / program (low byte) of the SIT note, selected by the cursor column
static void move_type_note(SndTestWork* w, TestPara* para)
{
    s16 v = *(s16*) para->ptr;
    s16 val;

    if (w->cur != 1) {
        val = v >> 8;
        v &= 0xFF;
    } else {
        val = v & 0xFF;
        v &= ~0xFF;
    }
    val = test_para_ck_s16(w, para, val);
    if (w->cur != 1) {
        v = (val << 8) | v;
    } else {
        v |= val & 0xFF;
    }
    *(s16*) para->ptr = v;
}

static void move_type_ext(SndTestWork* w, TestPara* para)
{
}

static void move_type_flag(SndTestWork* w, TestPara* para)
{
    u16* p = (u16*) para->ptr;
    s8 type = para->type;
    u16 bit = (type == 3) ? 0x2000 : 0;
    u16 v = *p;

    if (type == 4) {
        bit = 0x4000;
    }
    if (type == 5) {
        bit = 0x4000;
    }
    if (type == 6) {
        bit = 0x100;
    }
    if (type == 7) {
        bit = 1;
    }
    if (w->rep & 3) {
        v ^= bit;
    }
    *p = v;
}

static void move_type_nop(SndTestWork* w, TestPara* para)
{
}

// steps `val` by the pad (left/right; with X held: min/max/zero) and wraps it into the range
s16 test_para_ck_s16(SndTestWork* w, TestPara* para, s16 val)
{
    s16 step;

    if (w->on & 0x20) {
        step = para->fast;
    } else {
        step = para->step;
    }
    if (w->on & 0x400) {
        if (w->on & 1) {
            val = para->min;
        }
        if (w->on & 2) {
            val = para->max;
        }
        if (w->on & 8) {
            val = 0;
        }
        if (w->on & 4) {
            val = 0;
        }
    } else {
        if (w->rep & 1) {
            val -= step;
        }
        if (w->rep & 2) {
            val += step;
        }
    }
    if (val < para->min) {
        val = para->max;
    }
    if (val > para->max) {
        val = para->min;
    }
    return val;
}

int test_play_or_stop(SndTestWork* w)
{
    if (w->trg & 0x200) {
        if (w->tbl == 0) {
            Snd_se_fade_out_all(0);
            Snd_seq_fade_out_type(3, 0);
        } else {
            Snd_str_fade_out_type(3, 0);
        }
        return 1;
    }
    if (!(w->trg & 0x100)) {
        return 0;
    }
    {
        s8 tbl = w->tbl;
        u16 blk = w->blkNo[tbl];
        u32 id;

        if (tbl == 0) {
            SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
            SND_SIT* sit = Snd_get_sit_adrs(blk, w->reqCur);

            if (sit->srd_type == 1) {
                ctrl->x50 = 1;
            } else {
                ctrl->x50 = 0;
            }
            ctrl->flag_58 = 0x100;
            id = Snd_iss_req_para(blk, w->reqCur, NULL);
            if (id) {
                w->sndId = id;
            }
        } else {
            // The original passes w->reqCur as a second argument to Snd_test_get_str_name (`lhz r4` before
            // the call; the callee ignores it) and re-reads it for Snd_str_prepare: a two-argument view of
            // the same symbol.
            char* name = (char*) Snd_test_get_str_name2(blk, w->reqCur);
            id = Snd_str_prepare(blk, w->reqCur, name, -1);
            if (id) {
                w->sndId = id;
                Snd_str_req(id, 3, 0, 0);
            }
        }
    }
    return 1;
}

int test_req_no_select(SndTestWork* w, s16 max)
{
    int step = 1;
    s8 tbl = w->tbl;
    s16 old;

    w->reqCur = w->reqNo[tbl];
    w->reqMax[tbl] = max;
    old = w->reqCur;
    if (w->on & 0x20) {
        step = 10;
    }
    if (w->on & 0x400) {
        w->reqCur = 0;
    } else {
        if (w->rep & 0x10000) {
            w->reqCur -= step;
        }
        if (w->rep & 0x20000) {
            {
                // same as test_tbl_now_check: the halfword in an int local, promoted variable first
                int c = (u16) w->reqCur;
                w->reqCur = step + c;
            }
        }
    }
    if (w->reqCur < 0) {
        w->reqCur = max - 1;
    }
    if (w->reqCur >= max) {
        w->reqCur = 0;
    }
    if (w->reqCur == old) {
        return 0;
    }
    return 1;
}

void test_blk_no_select(SndTestWork* w)
{
    if (w->trg & 0x80000) {
        test_blk_enable_ck(w, -1);
    }
    if (w->trg & 0x40000) {
        test_blk_enable_ck(w, 1);
    }
}

// steps to the next loaded block; 0 when one was found
int test_blk_enable_ck(SndTestWork* w, int dir)
{
    s16 i;
    s16 max = w->blkMax[w->tbl];
    s16 no = w->blkNo[w->tbl];

    for (i = 0; i < max; i++) {
        no += dir;
        if (no < 0) {
            no = max - 1;
        }
        if (no >= max) {
            no = 0;
        }
        // one test per table arm (jump2 cross-jumps the identical cmpwi/beq tails: the two
        // block-local num qtys take r9 next to the r0 index temp instead of one global r0)
        if (w->tbl == 0) {
            if (Snd_iss_blk[no].num == 0) {
                continue;
            }
        } else {
            if (Snd_str_blk[no].num == 0) {
                continue;
            }
        }
        w->blkNo[w->tbl] = no;
        w->reqNo[w->tbl] = 0;
        return 0;
    }
    return 1;
}

void Snd_test_efx_init(SndTestWork* w)
{
    SND_EFX_WORK* efx = &Snd_efx_work[w->aux];

    if (efx->status & 1) {
        w->efxState[w->aux] = 1;
    } else {
        w->efxState[w->aux] = -1;
    }
    if (efx->type == 6) {
        w->efxType[w->aux] = 0;
    } else {
        w->efxType[w->aux] = efx->type;
    }
}

int Snd_test_efx_main(SndTestWork* w)
{
    SND_EFX_WORK* efx = &Snd_efx_work[w->aux];

    if (test_efx_type_select(w, efx) != 0) {
        return 1;
    }
    if (w->efxCur == 0) {
        return 0;
    }
    test_tbl_aux_ck(w);
    if (test_efx_on_or_off(w, efx) != 0) {
        return 1;
    }
    if (test_aux_para_select(w) != 0) {
        return 1;
    }
    if (test_move_epara_tbl[w->efxCur](w) != 0) {
        return 1;
    }
    return 0;
}

int test_efx_type_select(SndTestWork* w, SND_EFX_WORK* efx)
{
    if (!(w->trg & 0x40)) {
        return 0;
    }
    w->efxCur++;
    if (w->efxCur == 6) {
        w->efxCur = 0;
    }
    if (Snd_get_sound_mode() != 2 && w->efxCur == 5) {
        w->efxCur = 0;
    }
    w->efxState[w->aux] = -1;
    Snd_efx_req(w->aux, 6);
    w->auxCur = 0;
    return 1;
}

int test_efx_on_or_off(SndTestWork* w, SND_EFX_WORK* efx)
{
    if (w->trg & 0x100) {
        if (w->efxState[w->aux] <= 0) {
            if (Snd_efx_req(w->aux, w->efxCur) == 0) {
                w->efxState[w->aux] = 1;
            }
            return 1;
        }
    }
    if (w->trg & 0x10) {
        w->efxState[w->aux] = -1;
        Snd_efx_req(w->aux, 6);
        return 1;
    }
    return 0;
}

int test_aux_para_select(SndTestWork* w)
{
    s8 max;
    s8 old;

    if (w->on & 0x400) {
        return 0;
    }
    max = test_aux_para_num[w->efxCur];
    old = w->auxCur;
    if (w->rep & 8) {
        w->auxCur--;
    }
    if (w->rep & 4) {
        w->auxCur++;
    }
    if (w->auxCur & 0x80) {
        w->auxCur = max - 1;
    }
    if (w->auxCur >= max) {
        w->auxCur = 0;
    }
    if (w->auxCur != old) {
        return 1;
    }
    return 0;
}

static int test_move_epara_f32(SndTestWork* w)
{
    EfxParaF* p = NULL;
    f32* v;
    f32 step;
    f32 val;
    f32 old;

    switch (w->efxCur) {
    case 1:
        p = &efx_para_rev_hi[w->auxCur];
        break;
    case 2:
        p = &efx_para_rev_std[w->auxCur];
        break;
    case 5:
        p = &efx_para_rev_dpl2[w->auxCur];
        break;
    }
    if (w->aux == 0) {
        v = p->ptr[0];
    } else {
        v = p->ptr[1];
    }
    if (w->on & 0x20) {
        step = p->fast;
    } else {
        step = p->step;
    }
    val = *v;
    old = val;
    if (w->on & 0x400) {
        if (w->on & 1) {
            val = p->min;
        }
        if (w->on & 2) {
            val = p->max;
        }
        if (w->on & 8) {
            val = 0.0f;
        }
        if (w->on & 4) {
            val = 0.0f;
        }
    } else {
        if (w->rep & 1) {
            val -= step;
        }
        if (w->rep & 2) {
            val += step;
        }
    }
    if (val < p->min) {
        val = p->max;
    }
    if (val > p->max) {
        val = p->min;
    }
    if (w->auxCur == 1 && val == 0.0f) {
        val = 0.01f;
    }
    if (val == old) {
        return 0;
    }
    w->efxState[w->aux] = 0;
    *v = val;
    return 1;
}

static int test_move_epara_u32(SndTestWork* w)
{
    EfxParaU* p;
    u32* v;
    u32 step;
    u32 val;
    u32 old;

    if (w->efxCur == 3) {
        p = &efx_para_chorus[w->auxCur];
    } else {
        p = &efx_para_delay[w->auxCur];
    }
    if (w->aux == 0) {
        v = p->ptr[0];
    } else {
        v = p->ptr[1];
    }
    if (w->on & 0x20) {
        step = p->fast;
    } else {
        step = p->step;
    }
    val = *v;
    old = val;
    if (w->on & 0x400) {
        if (w->on & 1) {
            val = p->min;
        }
        if (w->on & 2) {
            val = p->max;
        }
        if (w->on & 8) {
            val = 0;
        }
        if (w->on & 4) {
            val = 0;
        }
    } else {
        if (w->rep & 1) {
            val -= step;
        }
        if (w->rep & 2) {
            val += step;
        }
    }
    if ((s32) val < (s32) p->min) {
        val = p->max;
    }
    if ((s32) val > (s32) p->max) {
        val = p->min;
    }
    if (val == old) {
        return 0;
    }
    w->efxState[w->aux] = 0;
    *v = val;
    return 1;
}

// AUX send level of the request being edited (sub stick)
void test_tbl_aux_ck(SndTestWork* w)
{
    s8* p;
    s8 v;
    int step;

    if (w->tbl == 0) {
        p = &w->sit.x8;
        if (w->aux == 0) {
            p = &w->sit.x7;
        }
    } else {
        p = &w->rit.auxB;
        if (w->aux == 0) {
            p = &w->rit.auxA;
        }
    }
    v = *p;
    step = 1;
    if (w->on & 0x20) {
        step = 10;
    }
    if (w->rep & 0x100000) {
        v -= step;
    }
    if (w->rep & 0x200000) {
        v += step;
    }
    if (w->rep & 0x800000) {
        v = 0;
    }
    if (w->rep & 0x400000) {
        v = 0x7F;
    }
    v &= 0x7F;
    *p = v;
}

int Snd_test_volume(SndTestWork* w)
{
    s8 old = w->volCursor;
    s16* p;
    int step;
    s8 cur;
    s8 v;

    if (!(w->on & 0x400)) {
        if (w->rep & 8) {
            w->volCursor--;
        }
        if (w->rep & 4) {
            w->volCursor++;
        }
        if (w->volCursor & 0x80) {
            w->volCursor = 5;
        }
        if (w->volCursor > 5) {
            w->volCursor = 0;
        }
    }
    if (w->volCursor != old) {
        return 1;
    }
    p = vol_ptr_tbl[w->volCursor];
    cur = *p >> 8;
    v = cur;
    step = 1;
    if (w->on & 0x20) {
        step = 10;
    }
    if (w->on & 0x400) {
        if (w->on & 1) {
            v = 0;
        }
        if (w->on & 2) {
            v = 0x7F;
        }
        if (w->on & 8) {
            v = 0;
        }
        if (w->on & 4) {
            v = 0;
        }
    } else {
        if (w->rep & 1) {
            v -= step;
        }
        if (w->rep & 2) {
            v += step;
        }
    }
    v &= 0x7F;
    if (v == cur) {
        return 0;
    }
    *p = v << 8;
    Snd_reset_vol_all();
    return 1;
}

static void (*disp_tbl[5])(SndTestWork*) = {(void (*)(SndTestWork*)) snd_test_disp_sit,
                                            (void (*)(SndTestWork*)) snd_test_disp_rit,
                                            (void (*)(SndTestWork*)) Snd_test_disp_efx,
                                            (void (*)(SndTestWork*)) Snd_test_disp_efx,
                                            (void (*)(SndTestWork*)) Snd_test_disp_vol};

void Snd_test_disp_move(SndTestWork* w)
{
    disp_tbl[w->mode](w);
}

static void snd_test_disp_sit()
{
    SndTestWork* w = &Snd_test_work;
    SND_ISS_BLK* blk = &Snd_iss_blk[w->blkNo[w->tbl]];
    SND_SIT* sit = blk->sit;

    sit += w->reqCur;
    switch (w->type) {
    case 0:
        disp_sit_type(w, sit);
        break;
    case 1:
    case 2:
        disp_sit_type(w, sit);
        if (w->wtDisp == 0) {
            disp_sit_normal(blk, sit, 0xB8, 0x54);
            disp_cursor(w, 0x18, 0x54);
        } else {
            disp_se_wt_data(w, blk, sit);
        }
        break;
    case 3:
        if (w->seqDisp == 0) {
            disp_sit_type(w, sit);
            disp_sit_midi(blk, sit, 0xB8, 0x54);
            disp_cursor(w, 0x18, 0x54);
        } else {
            disp_sequencer();
        }
        break;
    }
}

static char* sit_type_name[4] = {"TYPE      : DUMMY", "TYPE      :   NML", "TYPE      :  ADSR", "TYPE      :  MIDI"};

void disp_sit_type(SndTestWork* w, SND_SIT* sit)
{
    eprintf(0x18, 0x54, 0, 1, sit_type_name[w->type]);
    eprintf(0x18, 0x70, 0, 1, "NOTE      : %04XH", sit->prog);
    eprintf(0x18, 0x7E, 0, 1, "FLAG      : %04XH", sit->flag);
}

static char* srd_type_name[4] = {"   OFF", "DIRECT", "UPDATE", "  ONCE"};
static char* on_off_name[2] = {"OFF", " ON"};

void disp_sit_normal(SND_ISS_BLK* blk, SND_SIT* sit, int x, int y)
{
    u8* wt = blk->dls;
    SND_WT_HDR* hdr = (SND_WT_HDR*) wt;
    WTINST* inst = (WTINST*) (wt + hdr->inst_ofs);
    WTREGION* rgn = (WTREGION*) (wt + hdr->rgn_ofs);
    WTART* art = (WTART*) (wt + hdr->art_ofs);
    s32 dlsVol;
    s32 synVol;
    s32 axVol;

    // the three table pointers are advanced in place (the base pseudo is the final pointer:
    // `add r26,r26,r0`), like rit in snd_test_disp_rit
    inst += sit->prog >> 8;
    rgn += inst->keyRegion[sit->prog & 0xFF];
    art += rgn->articulationIndex;
    eprintf(x, y, 0, 1, "PATCH     : %5d", sit->prog >> 8);
    eprintf(x, y + 0xE, 0, 1, "NOTE      : %5d", sit->prog & 0xFF);
    eprintf(x, y + 0x1C, 0, 1, "VTBL_NO   : %5d", sit->curve_no);
    eprintf(x, y + 0x2A, 0, 1, "VOL       : %5d", sit->vol);
    eprintf(x, y + 0x38, 0, 1, "SVOL      : %5d", sit->svol);
    eprintf(x, y + 0x46, 0, 1, "AUX_A     : %5d", sit->x7);
    eprintf(x, y + 0x54, 0, 1, "AUX_B     : %5d", sit->x8);
    eprintf(x, y + 0x62, 0, 1, "PITCH_L   : %5d", (s16) sit->pitch_lo);
    eprintf(x, y + 0x70, 0, 1, "PITCH_H   : %5d", (s16) sit->pitch_hi);
    eprintf(x, y + 0x7E, 0, 1, "CH_NO     : %5d", sit->voice_start);
    eprintf(x, y + 0x8C, 0, 1, "MONOPOLY  : %5d", sit->voice_num);
    eprintf(x, y + 0x9A, 0, 1, "PRIO_NO   : %5d", sit->prio);
    if (sit->flag & 0x4000) {
        eprintf(x, y + 0xA8, 0, 1, "PRIO_MODE :   BCK");
    } else {
        eprintf(x, y + 0xA8, 0, 1, "PRIO_MODE :   FWD");
    }
    eprintf(x, y + 0xB6, 0, 1, "PAN       : %5d", sit->pan);
    eprintf(x, y + 0xC4, 0, 1, "SPAN      : %5d", sit->span);
    x += 0xA8;
    eprintf(x, y, 0, 1, "SRD_TYPE : %s", srd_type_name[sit->srd_type]);
    if (sit->flag & 0x2000) {
        eprintf(x, y + 0xE, 0, 1, "LINK     :     ON");
    } else {
        eprintf(x, y + 0xE, 0, 1, "LINK     :    OFF");
    }
    eprintf(x, y + 0x1C, 0, 1, "RTBL_NO  :  %5d", sit->rnd_no);
    eprintf(x, y + 0x2A, 0, 1, "CTRL     :  %5d", (s8) sit->se_flag);
    eprintf(x, y + 0x38, 0, 1, "FREE3    :  %5d", (s8) sit->wall_vol);
    eprintf(x, y + 0x46, 0, 1, "FREE4    :  %5d", (s8) sit->inner_vol);
    eprintf(x, y + 0x54, 0, 1, "FREE5    :  %5d", (s8) sit->xF);
    eprintf(x, y + 0x70, 0, 1, "NO FADEOUT :  %s", on_off_name[(sit->se_flag & 1) ? 1 : 0]);
    eprintf(x, y + 0x7E, 0, 1, "NO PAUSE   :  %s", on_off_name[(sit->se_flag & 2) ? 1 : 0]);
    eprintf(x, y + 0x8C, 0, 1, "NO VDOWN   :  %s", on_off_name[(sit->se_flag & 4) ? 1 : 0]);
    eprintf(x, y + 0x9A, 0, 1, "EVENT OK   :  %s", on_off_name[(sit->se_flag & 8) ? 1 : 0]);
    eprintf(x, y + 0xA8, 0, 1, "WALL CHECK :  %s", on_off_name[(sit->se_flag & 0x10) ? 1 : 0]);
    eprintf(x, y + 0xB6, 0, 1, "AT CHECK   :  %s", on_off_name[(sit->se_flag & 0x20) ? 1 : 0]);
    // the driver's iss_ax_set_vol shape: `/ 0x10000` is the branchy signed division (BRANCH_COST 0),
    // and the two-arm if/else is what keeps the FREE4/FREE5 row y out of cse1's path so that gcse PRE
    // copies them for the VOL(DLS)/VOL(SYN) rows (`mr r21,r29` / `mr r22,r30`); the then-arm's
    // `mr r31,r3` is cross-jumped with the call arm's
    dlsVol = rgn->attn / 0x10000;
    if (sit->vol >= 0) {
        synVol = sit->vol;
    } else {
        synVol = Snd_vol_ax_to_syn(dlsVol);
    }
    axVol = Snd_vol_syn_to_ax(synVol);
    eprintf(0x18, y + 0x46, 0, 1, "VOL(DLS)  : %5d", dlsVol);
    eprintf(0x18, y + 0x54, 0, 1, "VOL(SYN)  : %5d", synVol);
    eprintf(0x18, y + 0x62, 0, 1, "VOL(AX)   : %5d", axVol);
    eprintf(0x18, y + 0x7E, 0, 1, "PAN(DLS)  : %5d", art->pan);
}

void disp_sit_midi(SND_ISS_BLK* blk, SND_SIT* sit, int x, int y)
{
    SND_SEQ_WORK* seq;

    eprintf(x, y, 0, 1, "MIDI_NO   : %5d", sit->prog >> 8);
    eprintf(x, y + 0xE, 0, 1, "VOL_FLAG  : %5d", sit->curve_no);
    eprintf(x, y + 0x1C, 0, 1, "VOL       : %5d", sit->vol);
    eprintf(x, y + 0x2A, 0, 1, "CH_NO     : %5d", sit->voice_start);
    eprintf(x, y + 0x38, 0, 1, "MONOPOLY  : %5d", sit->voice_num);
    if (sit->flag & 0x4000) {
        eprintf(x, y + 0x46, 0, 1, "PRIO_MODE :   BCK");
    } else {
        eprintf(x, y + 0x46, 0, 1, "PRIO_MODE :   FWD");
    }
    if (sit->flag & 0x100) {
        eprintf(x, y + 0x54, 0, 1, "MIDI_TYPE :   BGM");
    } else {
        eprintf(x, y + 0x54, 0, 1, "MIDI_TYPE :    SE");
    }
    eprintf(x, y + 0x70, 0, 1, "VOL 0     : %5d", sit->rnd_no);
    eprintf(x, y + 0x7E, 0, 1, "VOL 1     : %5d", (s8) sit->se_flag);
    eprintf(x, y + 0x8C, 0, 1, "VOL 2     : %5d", (s8) sit->wall_vol);
    seq = Snd_search_seq_work_snd_id(Snd_test_work.sndId);
    if (seq == NULL) {
        seq = &Snd_seq_work[0];
    }
    eprintf(0x18, y + 0x54, 0, 1, "BE_FLAG : %04X", seq->status);
    eprintf(0x18, y + 0x62, 0, 1, "WORK_ID : %04X", seq->no);
    eprintf(0x18, y + 0x7E, 0, 1, "TOP SEQ : %08X", seq->seq_top);
    eprintf(0x18, y + 0x8C, 0, 1, "NOW SEQ : %08X", seq->seq_pos);
    eprintf(0x18, y + 0x9A, 0, 1, "LOP SEQ : %08X", seq->seq_loop);
    eprintf(0x18, y + 0xA8, 0, 1, "TEMPO   : %8d", seq->tempo);
    eprintf(0x18, y + 0xB6, 0, 1, "TPM     : %8d", seq->x3180);
    eprintf(0x18, y + 0xC4, 0, 1, "D TIME  : %8d", seq->delta);
    disp_seq_volume(seq);
}

void disp_seq_volume(SND_SEQ_WORK* seq)
{
    eprintf(0xD0, 0x16C, 0, 1, "OUT : %04XH", seq->calc_vol);
    eprintf(0xD0, 0x17A, 0, 1, "NOW : %04XH", seq->vol2);
    eprintf(0xD0, 0x196, 0, 1, "FDE : %04XH", seq->fade_target);
    eprintf(0xD0, 0x1A4, 0, 1, "SPD : %0d", seq->fade_step);
}

static void snd_test_disp_rit()
{
    SndTestWork* w = &Snd_test_work;
    SND_STR_BLK* blk = &Snd_str_blk[w->blkNo[w->tbl]];
    SND_RIT* rit = blk->rit;
    SND_SHD* shd;
    SND_STR_WORK* str;
    // The MONOPOLY row's y is `li r4,84; addi r4,r4,84` in the original: a single-set constant pseudo
    // (REG_EQUIV 84) whose init local-alloc's update_equiv_regs moves in front of its only use, because
    // substituting 84 into `y0 + 0x54` fails: validate_replace_rtx's "constant last" swap of
    // `(plus 84 84)` is a no-op (rtx_equal_p), so the PLUS is never folded (both constants must be equal).
    int y0 = 0x54;

    // rit is advanced in place: cse cannot rewrite the first rit-> load's address as
    // base+offset (the base register was overwritten), so combine forms the lhzux update load
    rit += w->reqCur;
    {
        // one `blk->shd` load shared by the table index and the base (two loads let cse/local-alloc
        // tie the block-0 temporaries differently: r8/r9/r10/r11 rotated by one)
        u8* base = blk->shd;
        shd = (SND_SHD*) (base + ((u32*) base)[rit->shd_no]);
    }

    disp_cursor(w, 0x18, 0x54);
    str = Snd_search_str_work_snd_id(w->sndId);
    if (str == NULL) {
        str = &Snd_str_work[str_get_player_id()];
    }
    eprintf(0x18, 0x54, 0, 1, "BE_FLAG : %04XH %02XH", str->status, Snd_ctrl_work.dvd_err);
    eprintf(0x18, 0x62, 0, 1, "RNO     : %02X %02X", str->state, str->prev_state);
    eprintf(0x18, 0x70, 0, 1, "DVD E/S : %02X %02X %2d", str->err, str->dvd_status, str->read_done);
    eprintf(0x18, 0x7E, 0, 1, "DVD NIE : %2d %2d %2d", str->read_cnt, str->read_blk, str->dma_blk);
    eprintf(0x18, 0x8C, 0, 1, "DMA NIE : %2d %2d %2d", str->dma_cnt, str->dma_aram_blk, str->dma_last_blk);
    eprintf(0x18, 0x9A, 0, 1, "PLY IDX : %2d %2d %03X", str->play_blk, str->prev_blk, str->blk_cnt);
    eprintf(0x18, 0xB6, 0, 1, "TOP NBL : %08XH", str->loop_start);
    eprintf(0x18, 0xC4, 0, 1, "END NBL : %08XH", str->loop_end);
    eprintf(0x18, 0xE0, 0, 1, "ARM NBL : %08XH", str->play_nbl);
    eprintf(0x18, 0xEE, 0, 1, "PLY NBL : %08XH", str->play_pos);
    eprintf(0x18, 0xFC, 0, 1, "NXT NBL : %08XH", str->x84);
    eprintf(0x18, 0x118, 0, 1, "ST SIZE : %08XH", str->read_end);
    eprintf(0x18, 0x126, 0, 1, "ST POS  : %08XH", str->read_ofs);
    eprintf(0xD0, 0x54, 0, 1, "STR_NO    : %5d", rit->shd_no);
    eprintf(0xD0, 0x62, 0, 1, "VOL_FLAG  : %5d", (s8) rit->pad_6[0]);
    eprintf(0xD0, 0x70, 0, 1, "VOL       : %5d", rit->vol);
    eprintf(0xD0, 0x7E, 0, 1, "STEREO    : %5d", (s8) rit->pad_B[0]);
    if (rit->flag & 1) {
        eprintf(0xD0, 0x8C, 0, 1, "STR_TYPE  :    SE");
    } else {
        eprintf(0xD0, 0x8C, 0, 1, "STR_TYPE  :   BGM");
    }
    eprintf(0xD0, 0x9A, 0, 1, "CH_NO     : %5d", rit->voice_start);
    eprintf(0xD0, y0 + 0x54, 0, 1, "MONOPOLY  : %5d", rit->voice_num);
    eprintf(0xD0, 0xB6, 0, 1, "PLAYER_ID : %5d", rit->str_no);
    eprintf(0xD0, 0xC4, 0, 1, "AUX_A     : %5d", rit->auxA);
    eprintf(0xD0, 0xD2, 0, 1, "AUX_B     : %5d", rit->auxB);
    eprintf(0xD0, 0xE0, 0, 1, "PAN_FLAG  : %5d", (s8) rit->pad_6[1]);
    eprintf(0xD0, 0xEE, 0, 1, "PAN       : %5d", rit->pan);
    eprintf(0xD0, 0xFC, 0, 1, "SPAN      : %5d", rit->span);
    eprintf(0xD0, 0x10A, 0, 1, "SVOL      : %5d", rit->svol);
    eprintf(0xD0, 0x118, 0, 1, "SMP RATE  : %d", shd->rate);
    if (shd->flag & 4) {
        eprintf(0xD0, 0x126, 0, 1, "STR LOOP  :    ON");
    } else {
        eprintf(0xD0, 0x126, 0, 1, "STR LOOP  :   OFF");
    }
    disp_str_status(w, 0xD0, 0x54);
    eprintf(0xD0, 0x16C, 0, 1, "OUT : %04XH", str->calc_vol);
    eprintf(0xD0, 0x17A, 0, 1, "NOW : %04XH", str->vol2);
    eprintf(0xD0, 0x196, 0, 1, "FDE : %04XH", str->fade_target);
    eprintf(0xD0, 0x1A4, 0, 1, "SPD : %0d", str->fade_step);
}

// first stream player in use (0 when none)
int str_get_player_id()
{
    int i;

    for (i = 0; i < 4; i++) {
        if (Snd_str_work[i].status != 0) {
            return i;
        }
    }
    return 0;
}

void disp_cursor(SndTestWork* w, int x, int y)
{
    u16 flag;
    int cur;
    int col;

    if (w->tbl == 1) {  // sic: the original tests the SIT copy's flag for the RIT table
        flag = w->sit.flag;
    } else {
        flag = w->rit.flag;
    }
    if (flag & 0x8000) {
        return;
    }
    cur = w->cur;
    if (cur > 0xE) {
        cur -= 0xF;
        col = 0x27;
    } else {
        col = 0x12;
    }
    if (w->tbl == 1) {
        col += 3;
    }
    x += col * 8;
// COMPILER-DIFF: candidate #10 - the original build keeps separate .rodata copies of 1-char string
// literals per function group (3x ">", 2x "<", 2x "D" in this unit) while ours merges them; the
// explicit "\0" padding forces distinct constants with identical bytes.
    y += cur * 14;
    eprintf(x, y, 4, 1, ">\0");
    eprintf(x + 0xA0, y, 4, 1, "<\0");
}

static char* str_state_name[7] = {"WAIT   ", "READY  ", "PLAY   ", "NO READ", "CLOSE  ", "IDLE   ", "ERROR  "};
static u8 str_state_col[8] = {0, 0, 4, 0, 7, 7, 2, 0};

void disp_str_status(SndTestWork* w, int x, int y)
{
    int i;

    x += 0xA8;
    for (i = 0; i < 4; i++) {
        SND_STR_WORK* str = &Snd_str_work[i];

        if (str->status == 0) {
            eprintf(x, y, 7, 1, "%02d : NO MOVE\n", i);
        } else {
            eprintf(x, y, str_state_col[str->state], 1, "%02d : %s\n", i, str_state_name[str->state]);
        }
        y += 0xE;
    }
}

// sequencer voices playing MIDI channel `ch`. `work` is a reference to the caller's `const` pointer
// local: taking the address of a folded const scalar (`&voices`, DECL_RTL never made) forces its
// initializer into the constant pool (expr.c ADDR_EXPR -> force_const_mem), which is the `.4byte
// Snd_voice_work` word at the end of disp_sequencer's pool that the loop preheader reads with
// `lis; addi; lwz 0()`.
static inline int seq_note_count(int ch, SND_VOICE_WORK* const& work)
{
    SND_VOICE_WORK* vw = work;
    int notes = 0;
    int i;

    for (i = 0; i < 64; i++) {
        if (vw->status != 0 && vw->type == 2 && vw->seq_ch == ch) {
            notes++;
        }
        vw++;
    }
    return notes;
}

void disp_sequencer()
{
    SND_SEQ_WORK* seq;
    int ch;
    int y0;

    seq = Snd_search_seq_work_snd_id(Snd_test_work.sndId);
    if (seq == NULL) {
        seq = &Snd_seq_work[0];
    }
    eprintf(0x18, 0x54, 0, 1, "TRACK    :");
    eprintf(0x18, 0x62, 0, 1, "NOTE ON  :");
    eprintf(0x18, 0x70, 0, 1, "PRIO     :");
    eprintf(0x18, 0x7E, 0, 1, "PROG NO  :");
    eprintf(0x18, 0x8C, 0, 1, "CH_VOL   :");
    eprintf(0x18, 0x9A, 0, 1, "EXPRESS  :");
    eprintf(0x18, 0xA8, 0, 1, "PAN      :");
    eprintf(0x18, 0xB6, 0, 1, "PITCH M  :");
    eprintf(0x18, 0xC4, 0, 1, "PITCH L  :");
    eprintf(0x18, 0xD2, 0, 1, "P SEN M  :");
    eprintf(0x18, 0xE0, 0, 1, "P SEN L  :");
    eprintf(0x18, 0xEE, 0, 1, "MODULATE :");
    eprintf(0x18, 0xFC, 0, 1, "HOLD     :");
    eprintf(0x18, 0x10A, 0, 1, "AUX A    :");
    eprintf(0x18, 0x118, 0, 1, "AUX B    :");
    // `y0`: the PAN row's y is `li r9,84; addi r27,r9,84` at the top of the join block in the
    // original: a single-set constant pseudo (REG_EQUIV 84, rematerialised at its use) set
    // before the loop, added after the D diamond's join label (cse cannot fold across it). Its
    // `li` is also the first free filler of the preheader (LUID before `ch = 0`), which is what
    // puts the two spilled row pointers (ch_flag/ch_prio) one header eprintf later.
    y0 = 0x54;
    for (ch = 0; ch < 16; ch++) {
        int x = 0x70 + ch * 0x18;
        SND_VOICE_WORK* const voices = Snd_voice_work;
        int y;

        // `ch + 1`: the target passes r10 = ch+1 to the label eprintf2 and keeps that value
        // (`mr r26,r10`) as the loop's next ch
        eprintf2(6, 13, x, 0x54, 0, 1, "%03d", ch + 1);
        if (seq->ch_flag[ch] & 1) {
            eprintf2(6, 13, x, 0x54, 4, 1, "D");
        }
        y = y0 + 0x54;
        eprintf2(6, 13, x, 0x62, 0, 1, "%3d", seq_note_count(ch, voices));
        eprintf2(6, 13, x, 0x70, 0, 1, "%3d", seq->ch_prio[ch]);
        eprintf2(6, 13, x, 0x7E, 0, 1, "%3d", seq->ch_prog[ch] + 1);
        eprintf2(6, 13, x, 0x8C, 0, 1, "%3d", (s8) seq->ch_vol[ch]);
        eprintf2(6, 13, x, 0x9A, 0, 1, "%3d", (s8) seq->ch_exp[ch]);
        eprintf2(6, 13, x, y, 0, 1, "%3d", seq->ch_pan[ch]);
        eprintf2(6, 13, x, 0xB6, 0, 1, "%3d", seq->ch_pitch_lo[ch]);
        eprintf2(6, 13, x, 0xC4, 0, 1, "%3d", seq->ch_pitch_hi[ch]);
        eprintf2(6, 13, x, 0xD2, 0, 1, "%3d", seq->ch_data_msb[ch]);
        eprintf2(6, 13, x, 0xE0, 0, 1, "%3d", seq->ch_data_lsb[ch]);
        eprintf2(6, 13, x, 0xEE, 0, 1, "%3d", (s8) seq->ch_mod[ch]);
        eprintf2(6, 13, x, 0xFC, 0, 1, "%3d", (s8) seq->ch_hold[ch]);
        eprintf2(6, 13, x, 0x10A, 0, 1, "%3d", (s8) seq->ch_reverb[ch]);
        eprintf2(6, 13, x, 0x118, 0, 1, "%3d", (s8) seq->ch_chorus[ch]);
    }
    disp_seq_volume(seq);
}

void disp_se_wt_data(SndTestWork* w, SND_ISS_BLK* blk, SND_SIT* sit)
{
    u32 ofs;
    u32 len;

    get_wt_ptr(w, blk, sit);
    eprintf(0x18, 0xA8, 0, 1, "- WTREGION - ");
    eprintf(0x18, 0xB6, 0, 1, "ART    IDX : %d", w->rgn->articulationIndex);
    eprintf(0x18, 0xC4, 0, 1, "SAMPLE IDX : %d", w->rgn->sampleIndex);
    eprintf(0x18, 0xD2, 0, 1, "unityNote  : %d", w->rgn->unityNote);
    eprintf(0x18, 0xE0, 0, 1, "keyGroup   : %d", w->rgn->keyGroup);
    eprintf(0x18, 0xEE, 0, 1, "fineTune   : %d", w->rgn->fineTune);
    eprintf(0x18, 0xFC, 0, 1, "attn       : %08XH", w->rgn->attn);
    eprintf(0x18, 0x10A, 0, 1, "             %d", (s16) (w->rgn->attn >> 16));
    eprintf(0x18, 0x118, 0, 1, "loopStart  : %08XH", w->rgn->loopStart);
    eprintf(0x18, 0x126, 0, 1, "loopLength : %08XH", w->rgn->loopLength);
    eprintf(0xB8, 0x54, 0, 1, "- WTART - ");
    eprintf(0xB8, 0x62, 0, 1, "Attack     : %08XH", w->art->eg1Attack);
    eprintf(0xB8, 0x70, 0, 1, "Decay      : %08XH", w->art->eg1Decay);
    eprintf(0xB8, 0x7E, 0, 1, "Sustain    : %08XH", w->art->eg1Sustain);
    eprintf(0xB8, 0x8C, 0, 1, "Release    : %08XH", w->art->eg1Release);
    eprintf(0xB8, 0x9A, 0, 1, "Vel2Attack : %08XH", w->art->eg1Vel2Attack);
    eprintf(0xB8, 0xA8, 0, 1, "Key2Decay  : %08XH", w->art->eg1Key2Decay);
    eprintf(0xB8, 0xB6, 0, 1, "pan        : %08XH", w->art->pan);
    ofs = w->sample->offset / 2;
    len = (w->sample->length / 2 / 7) * 16 + w->sample->length % 14;
    len /= 2;
    eprintf(0xD8, 0xEE, 0, 1, "- WTSAMPLE - ");
    eprintf(0xD8, 0xFC, 0, 1, "sampleRate : %d Hz", w->sample->sampleRate);
    eprintf(0xD8, 0x10A, 0, 1, "offset     : %08XH (%08XH)", ofs, w->sample->offset);
    eprintf(0xD8, 0x118, 0, 1, "length     : %08XH (%08XH)", len, w->sample->length);
    eprintf(0xD8, 0x126, 0, 1, "ADPCM IDX  : %d", w->sample->adpcmIndex);
    disp_adsr_para(w);
}

void get_wt_ptr(SndTestWork* w, SND_ISS_BLK* blk, SND_SIT* sit)
{
    SND_WT_HDR* hdr = (SND_WT_HDR*) blk->dls;

    w->wt = blk->dls;
    w->inst = (WTINST*) (blk->dls + hdr->inst_ofs);
    w->inst = (WTINST*) ((u8*) w->inst + (sit->prog & 0xFF00));
    w->rgn = (WTREGION*) (blk->dls + hdr->rgn_ofs);
    w->rgn += w->inst->keyRegion[sit->prog & 0xFF];
    w->art = (WTART*) (blk->dls + hdr->art_ofs);
    w->art += w->rgn->articulationIndex;
    w->sample = (WTSAMPLE*) (blk->dls + hdr->sample_ofs);
    w->sample += w->rgn->sampleIndex;
    w->adpcm = (WTADPCM*) (blk->dls + hdr->adpcm_ofs);
    w->adpcm += w->sample->adpcmIndex;
}

void disp_adsr_para(SndTestWork* w)
{
    SND_AXV_WORK* axv;

    get_axv_ptr(w);
    axv = w->axv;
    if (axv == NULL) {
        axv = &Snd_axv_work[0];
    }
    eprintf(0x178, 0x54, 0, 1, "- ADSR -");
    eprintf(0x178, 0x62, 0, 1, "ADSR CTR : %4d", (s16) axv->env_cnt);
    eprintf(0x178, 0x70, 0, 1, "ADSR ATK : %4d", axv->attack_steps);
    eprintf(0x178, 0x7E, 0, 1, "ADSR REL : %4d", axv->rel_time);
    eprintf(0x178, 0x8C, 0, 1, "ADSR VOL : %04XH", axv->env_vol);
    eprintf(0x178, 0x9A, 0, 1, "ADSR END : %04XH", axv->env_target);
    eprintf(0x178, 0xA8, 0, 1, "ADSR SPD : %04XH", axv->env_step);
    eprintf(0x178, 0xB6, 0, 1, "NOW  VOL : %04XH", axv->now_vol);
    eprintf(0x178, 0xC4, 0, 1, "OUT  VOL : %04XH", axv->calc_vol);
}

void get_axv_ptr(SndTestWork* w)
{
    SND_VOICE_WORK* vw;

    if (w->sndId == 0) {
        return;
    }
    vw = Snd_search_voice_work_snd_id(w->sndId);
    if (vw == NULL) {
        return;
    }
    if (vw->axv == NULL) {
        return;
    }
    w->axv = vw->axv;
}

void Snd_test_disp_menu(SndTestWork* w)
{
    int y;

    eprintf(0xA8, 0x54, 0, 1, "  SOUND TEST MODE MENU");
    eprintf(0xA8, 0x70, 0, 1, "  SIT   - I.S.S. PLAY");
    eprintf(0xA8, 0x7E, 0, 1, "  RIT   - STREAM PLAY");
    eprintf(0xA8, 0x8C, 0, 1, "  AUX A - EFFECT SET");
    eprintf(0xA8, 0x9A, 0, 1, "  AUX B - EFFECT SET");
    eprintf(0xA8, 0xA8, 0, 1, "  VOL   - SYSTEM VOLUME");
    eprintf(0xA8, 0xB6, 0, 1, "  DUMP  - A-RAM  DUMP");
    eprintf(0xA0, 0xFC, 0, 1, "SOUND PROGRAM %s", "Version 1.910");
    eprintf(0xA0, 0x118, 0, 1, "GAMECUBE SDK  %s", "20Apr2004Patch1");
    y = w->mode * 14 + 0x70;
    eprintf(0xA8, y, 4, 1, ">");
    eprintf(0x168, y, 4, 1, "<");
}

void Snd_test_disp_basic(SndTestWork* w)
{
    // The original passes a stale r3 to the 2nd/3rd call (no `mr r3,r31` reload): the argument
    // was a hard-register variable, which GCC 2.95 does not restore after a call.
    register SndTestWork* p asm("r3") = w;

    if (w->dispFlag & 2) {
        Snd_test_disp_voice(p);
    }
    if (w->dispFlag & 1) {
        Snd_test_disp_req_para(p);
    }
    if (w->dispFlag & 4) {
        Snd_test_disp_aux(p);
    }
}

static char* sound_mode_name[3] = {"MONO  ", "STEREO", "DPL2  "};

// 8x8 map of the voice slots
void Snd_test_disp_voice(SndTestWork* w)
{
    int i;
    int j;

    eprintf(0x1C0, 0x134, 0, 1, sound_mode_name[Snd_ctrl_work.sound_mode]);
    for (i = 0; i < 8; i++) {
        // x is a per-row statement; n and y are inner-body expressions (y a reduced giv, n
        // computed in the loop header) and the voice is indexed by n: the address giv's
        // increment lands right after the load (auto_inc_opt) and its init is emitted by
        // loop.c after gcse's insertions, so i dies there and not at the PRE'd i+1
        int x = 0x138 + i * 0x18;

        for (j = 0; j < 8; j++) {
            int n = i * 8 + j;
            int y = 0x142 + j * 0xE;
            if (Snd_voice_work[n].status != 0) {
                eprintf(x, y, 4, 1, "%02d", n);
            } else {
                eprintf(x, y, 7, 1, "%02d", n);
            }
        }
    }
}

static char* tbl_name[2] = {"I.S.S.(SIT)", "STREAM(RIT)"};

// the parameter is ignored: the original reads the global work and the control work through locals
void Snd_test_disp_req_para(SndTestWork* unused)
{
    SndTestWork* w = &Snd_test_work;
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;

    eprintf(0x18, 0x142, 0, 1, "TBL    : %s", tbl_name[w->tbl]);
    eprintf(0x18, 0x150, 0, 1, "BLK_NO : %4d / %4d", w->blkNo[w->tbl], w->blkMax[w->tbl] - 1);
    eprintf(0x18, 0x15E, 0, 1, "REQ_NO : %4d / %4d", w->reqCur, w->reqMax[w->tbl] - 1);
    eprintf(0x18, 0x17A, 0, 1, "MASTER : %4d / %4d", ctrl->sys_vol[0] >> 8, ctrl->sys_vol[1] >> 8);
    eprintf(0x18, 0x188, 0, 1, "ISS    : %4d / %4d", ctrl->sys_vol[2] >> 8, ctrl->sys_vol[3] >> 8);
    eprintf(0x18, 0x196, 0, 1, "STR    : %4d / %4d", ctrl->sys_vol[4] >> 8, ctrl->sys_vol[5] >> 8);
    eprintf(0x18, 0x1A4, 0, 1, "VOICE  : %4d / %4d", ctrl->voice_num, ctrl->voice_peak);
}

static u8 efx_type_col[8] = {7, 4, 4, 4, 4, 4, 0, 2};
static char* efx_type_name[8] = {"EFX OFF  ", "REV HI   ", "REV STD. ", "CHORUS   ", "DELAY    ", "REV DPL2 ", "EFX STOP ", "EFX ERROR"};

void Snd_test_disp_aux(SndTestWork* w)
{
    s16 type;

    type = Snd_efx_get_type(0);
    eprintf(0xD0, 0x142, efx_type_col[type], 1, "%d %s", type, efx_type_name[type]);
    type = Snd_efx_get_type(1);
    eprintf(0xD0, 0x150, efx_type_col[type], 1, "%d %s", type, efx_type_name[type]);
}

static char* aux_name[2] = {"AUX A", "AUX B"};
static char* efx_name[6] = {"NO EFFECT  ", "REVERB HI  ", "REVERB STD.", "CHORUS     ", "DELAY      ", "REVERB DPL2"};
static void (*test_disp_efx_tbl[7])(SndTestWork*, SND_EFX_WORK*, int, int) = {
    NULL, test_disp_efx_rev_hi, test_disp_efx_rev_std, test_disp_efx_chorus, test_disp_efx_delay, test_disp_efx_rev_dpl2, NULL};

void Snd_test_disp_efx()
{
    SndTestWork* w = &Snd_test_work;
    SND_EFX_WORK* efx = &Snd_efx_work[w->aux];
    int col;
    int y;

    eprintf(0x18, 0x54, 0, 1, aux_name[w->aux]);
    col = 0;
    if (w->efxState[w->aux] > 0) {
        col = 4;
    }
    eprintf(0x18, 0x70, col, 1, efx_name[w->efxCur]);
    if (w->efxCur == 0) {
        return;
    }
    switch (w->efxState[w->aux]) {
    case 1:
        eprintf(0x18, 0x8C, 4, 1, "EXECUTED.");
        eprintf(0x18, 0x10A, 0, 1, "USE MEMORY is %7d (0x%06XH) Bytes.", efx->err, efx->err);
        break;
    case 0:
        if (w->frame & 0x10) {
            eprintf(0x18, 0x8C, 0, 1, "CHANGING.");
        }
        break;
    case -1:
        eprintf(0x18, 0x8C, 7, 1, "STOPPED.");
        break;
    }
    y = w->auxCur * 14 + 0x54;
    eprintf(0x80, y, 4, 1, ">");
    y = w->auxCur * 14 + 0x54;
    eprintf(0x1D8, y, 4, 1, "<");
    test_disp_efx_tbl[w->efxCur](w, efx, 0x90, 0x54);
    if (w->tbl == 0) {
        eprintf(0x90, 0xE0, 0, 1, "I.S.S.(SIT) AUX A : %3d", w->sit.x7);
        eprintf(0x90, 0xEE, 0, 1, "I.S.S.(SIT) AUX B : %3d", w->sit.x8);
    } else {
        eprintf(0x90, 0xE0, 0, 1, "Stream(RIT) AUX A : %3d", w->rit.auxA);
        eprintf(0x90, 0xEE, 0, 1, "Stream(RIT) AUX B : %3d", w->rit.auxB);
    }
}

static void test_disp_efx_rev_hi(SndTestWork* w, SND_EFX_WORK* efx, int x, int y)
{
    eprintf(x, y, 0, 1, "PREDELAY    ( 0.000 -  0.100) : %2.4fF", efx->fx.hi.preDelay);
    eprintf(x, y + 0xE, 0, 1, "TIME        ( 0.010 - 10.000) : %2.4fF", efx->fx.hi.time);
    eprintf(x, y + 0x1C, 0, 1, "COLORATION  ( 0.000 -  1.000) : %2.4fF", efx->fx.hi.coloration);
    eprintf(x, y + 0x2A, 0, 1, "DAMPING     ( 0.000 -  1.000) : %2.4fF", efx->fx.hi.damping);
    eprintf(x, y + 0x38, 0, 1, "CROSSTALK   ( 0.000 -  1.000) : %2.4fF", efx->fx.hi.crosstalk);
    eprintf(x, y + 0x46, 0, 1, "MIX         ( 0.000 -  1.000) : %2.4fF", efx->fx.hi.mix);
}

static void test_disp_efx_rev_std(SndTestWork* w, SND_EFX_WORK* efx, int x, int y)
{
    eprintf(x, y, 0, 1, "PREDELAY    ( 0.000 -  0.100) : %2.4fF", efx->fx.std.preDelay);
    eprintf(x, y + 0xE, 0, 1, "TIME        ( 0.010 - 10.000) : %2.4fF", efx->fx.std.time);
    eprintf(x, y + 0x1C, 0, 1, "COLORATION  ( 0.000 -  1.000) : %2.4fF", efx->fx.std.coloration);
    eprintf(x, y + 0x2A, 0, 1, "DAMPING     ( 0.000 -  1.000) : %2.4fF", efx->fx.std.damping);
    eprintf(x, y + 0x38, 0, 1, "MIX         ( 0.000 -  1.000) : %2.4fF", efx->fx.std.mix);
}

static void test_disp_efx_chorus(SndTestWork* w, SND_EFX_WORK* efx, int x, int y)
{
    eprintf(x, y, 0, 1, "BASEDELAY   (     5 -     15) : %8d", efx->fx.chorus.baseDelay);
    eprintf(x, y + 0xE, 0, 1, "VARIATION   (     0 -      5) : %8d", efx->fx.chorus.variation);
    eprintf(x, y + 0x1C, 0, 1, "PERIOD      (   500 -  10000) : %8d", efx->fx.chorus.period);
}

static void test_disp_efx_delay(SndTestWork* w, SND_EFX_WORK* efx, int x, int y)
{
    eprintf(x, y, 0, 1, "DELAY[0]    (    10 -   5000) : %8d", efx->fx.delay.delay[0]);
    eprintf(x, y + 0xE, 0, 1, "DELAY[1]    (    10 -   5000) : %8d", efx->fx.delay.delay[1]);
    eprintf(x, y + 0x1C, 0, 1, "DELAY[2]    (    10 -   5000) : %8d", efx->fx.delay.delay[2]);
    eprintf(x, y + 0x2A, 0, 1, "FEEDBACK[0] (     0 -    100) : %8d", efx->fx.delay.feedback[0]);
    eprintf(x, y + 0x38, 0, 1, "FEEDBACK[1] (     0 -    100) : %8d", efx->fx.delay.feedback[1]);
    eprintf(x, y + 0x46, 0, 1, "FEEDBACK[2] (     0 -    100) : %8d", efx->fx.delay.feedback[2]);
    eprintf(x, y + 0x54, 0, 1, "OUTPUT[0]   (     0 -    100) : %8d", efx->fx.delay.output[0]);
    eprintf(x, y + 0x62, 0, 1, "OUTPUT[1]   (     0 -    100) : %8d", efx->fx.delay.output[1]);
    eprintf(x, y + 0x70, 0, 1, "OUTPUT[2]   (     0 -    100) : %8d", efx->fx.delay.output[2]);
}

static void test_disp_efx_rev_dpl2(SndTestWork* w, SND_EFX_WORK* efx, int x, int y)
{
    eprintf(x, y, 0, 1, "PREDELAY    ( 0.000 -  0.100) : %2.4fF", efx->fx.dpl2.preDelay);
    eprintf(x, y + 0xE, 0, 1, "TIME        ( 0.010 - 10.000) : %2.4fF", efx->fx.dpl2.time);
    eprintf(x, y + 0x1C, 0, 1, "COLORATION  ( 0.000 -  1.000) : %2.4fF", efx->fx.dpl2.coloration);
    eprintf(x, y + 0x2A, 0, 1, "DAMPING     ( 0.000 -  1.000) : %2.4fF", efx->fx.dpl2.damping);
    eprintf(x, y + 0x38, 0, 1, "MIX         ( 0.000 -  1.000) : %2.4fF", efx->fx.dpl2.mix);
}

void Snd_test_disp_vol()
{
    SndTestWork* w = &Snd_test_work;
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
    int y;

    y = w->volCursor * 14 + 0x54;
    eprintf(0xA8, y, 4, 1, ">");
    y = w->volCursor * 14 + 0x54;
    eprintf(0x160, y, 4, 1, "<");
    eprintf(0xB8, 0x54, 0, 1, "MASTER VOL BGM : %3d", ctrl->sys_vol[0] >> 8);
    eprintf(0xB8, 0x62, 0, 1, "MASTER VOL SE  : %3d", ctrl->sys_vol[1] >> 8);
    eprintf(0xB8, 0x70, 0, 1, "I.S.S. VOL BGM : %3d", ctrl->sys_vol[2] >> 8);
    eprintf(0xB8, 0x7E, 0, 1, "I.S.S. VOL SE  : %3d", ctrl->sys_vol[3] >> 8);
    eprintf(0xB8, 0x8C, 0, 1, "STREAM VOL BGM : %3d", ctrl->sys_vol[4] >> 8);
    eprintf(0xB8, 0x9A, 0, 1, "STREAM VOL SE  : %3d", ctrl->sys_vol[5] >> 8);
}

void Snd_test_load_init(SndTestWork* w)
{
    directory_open(w);
}

void Snd_test_mode_load(SndTestWork* w)
{
    int moved;

    moved = blk_no_check(w);
    if (moved == 0) {
        if (w->trg & 0x100) {
            load_select(w);
            moved = 1;
        }
        if (moved == 0) {
            int step = 1;

            if (w->on & 0x20) {
                step = 10;
            }
            if (w->rep & 8) {
                w->dirCur -= step;
            }
            if (w->rep & 4) {
                w->dirCur += step;
            }
            if (w->dirCur < 0) {
                w->dirCur = w->dirNum - 1;
            }
            if (w->dirCur >= w->dirNum) {
                w->dirCur = 0;
            }
            w->dirTop = w->dirCur / 10 * 10;
        }
    }
    directory_disp(w);
    cursor_disp(w);
    blk_file_disp(w);
}

// block number of the load target (sub stick); 1 when it changed
int blk_no_check(SndTestWork* w)
{
    s16 max;
    s16 no;
    s16 old;

    if (w->loadTbl == 0) {
        max = 4;
    } else {
        max = w->blkMax[w->loadTbl];
    }
    no = w->blkNo[w->loadTbl];
    old = no;
    if (w->rep & 0x80000) {
        no--;
    }
    if (w->rep & 0x40000) {
        no++;
    }
    if (no < 0) {
        no = max - 1;
    }
    if (no >= max) {
        no = 0;
    }
    if (no != old) {
        w->blkNo[w->loadTbl] = no;
        return 1;
    }
    return 0;
}

void load_select(SndTestWork* w)
{
    s8 tbl = w->loadTbl;
    int cur;

    if (get_dir_level(w->path[tbl]) != 0 && w->dirCur == 0) {
        dir_name_up(w);
        directory_open(w);
        return;
    }
    cur = w->dirCur;
    if (w->dirIsDir[cur] != 0) {
        char* path = w->path[tbl];

        strcat(path, w->dirName[cur]);
        strcat(path, "/");
        change_to_cap(path);
        directory_open(w);
        return;
    }
    if (w->loadTbl == 0) {
        load_sit_data(w, w->dirName[cur]);
    } else {
        load_rit_data(w, w->dirName[cur]);
    }
}

void load_sit_data(SndTestWork* w, char* name)
{
}

void load_rit_data(SndTestWork* w, char* name)
{
}

static char dir_up_name[4] = "..";

void directory_open(SndTestWork* w)
{
    w->dirNum = 0;
    w->dirTop = 0;
    w->dirCur = 0;
    if (get_dir_level(w->path[w->loadTbl]) != 0) {
        w->dirName[0] = dir_up_name;
        w->dirIsDir[0] = 1;
        w->dirNum++;
    }
    dir_entry_read(w, 1);
    dir_entry_read(w, 0);
}

static char* blk_ext_name[3] = {"BLK", "BKR", NULL};

// appends the directories (dirs = 1) or the block files of the current path to the list
void dir_entry_read(SndTestWork* w, int dirs)
{
    char name[0x100];

    if (DVDOpenDir(w->path[w->loadTbl], &w->dir) == 0) {
        return;
    }
    for (;;) {
        // the original returns WITHOUT DVDCloseDir when the list is full (the dirNum test
        // jumps to the epilogue); the loop is rotated around the DVDReadDir break
        if (w->dirNum > 0x7F) {
            return;
        }
        if (DVDReadDir(&w->dir, &w->ent) == 0) {
            break;
        }
        if (dirs) {
            if (w->ent.isDir == 0) {
                continue;
            }
        } else {
            if (w->ent.isDir != 0) {
                continue;
            }
            strcpy(name, w->ent.name);
            change_to_cap(name);
            if (strcmp(get_file_ext(name), blk_ext_name[w->loadTbl]) != 0) {
                continue;
            }
        }
        w->dirName[w->dirNum] = w->ent.name;
        w->dirIsDir[w->dirNum] = w->ent.isDir;
        w->dirNum++;
    }
    DVDCloseDir(&w->dir);
}

void directory_disp(SndTestWork* w)
{
    char name[0x100];
    int i;
    int end;
    int y = 0x62;

    eprintf(0x18, 0x54, 0, 1, "DIR : %s", w->path[w->loadTbl]);
    eprintf(0x18, 0x62, 0, 1, "NO. : %3d", w->dirCur);
    eprintf(0x18, 0x70, 0, 1, "MAX : %3d", w->dirNum - 1);
    end = w->dirTop + 10;
    if (end > w->dirNum) {
        end = w->dirNum;
    }
    for (i = w->dirTop; i < end; i++) {
        strcpy(name, w->dirName[i]);
        change_to_cap(name);
        if (w->dirIsDir[i] != 0) {
            eprintf(0xA8, y, 4, 1, "D\0");
        } else {
            eprintf(0xA8, y, 0, 1, "F");
        }
        eprintf(0xB8, y, 0, 1, "%s", name);
        y += 0xE;
    }
}

void cursor_disp(SndTestWork* w)
{
    eprintf(0x98, (w->dirCur - w->dirTop + 7) * 14, 4, 1, ">\0\0");
}

void blk_file_disp(SndTestWork* w)
{
    char name[0x100];
    s16 blk = w->blkNo[w->loadTbl];
    char* file;

    if (w->loadTbl == 0) {
        eprintf(0x18, 0x10A, 0, 1, "SIT BLK[%02d] :", blk);
        file = (char*) w->sitData[blk];
    } else {
        eprintf(0x18, 0x10A, 0, 1, "RIT BLK[%02d] :", blk);
        file = (char*) w->ritData[blk];
    }
    if (file == NULL) {
        eprintf(0x88, 0x10A, 7, 1, "%s", "NO DATA");
    } else {
        strcpy(name, file);
        change_to_cap(name);
        eprintf(0x88, 0x10A, 0, 1, "%s", name);
    }
    // dead code: the original's .rodata keeps an "sbb" literal between "NO DATA" and aram_dump_disp's
    // strings; the code that used it is gone
    if (0) {
        eprintf(0x88, 0x10A, 0, 1, "sbb");
    }
}

// removes the last directory of the current path
void dir_name_up(SndTestWork* w)
{
    char* path = w->path[w->loadTbl];
    int last = 0;
    int i;

    for (i = 0; i <= 0xFF; i++) {
        if (path[i] == '/') {
            if (*(path + i + 1) == 0) {
                break;
            }
            last = i;
        }
    }
    *(path + last + 1) = 0;
}

void change_to_cap(char* s)
{
    char c = *s;

    while (c != 0) {
        if (c >= 'a' && c <= 'z') {
            *s = c - 0x20;
        }
        c = *++s;
    }
}

char* get_file_ext(char* s)
{
    char c;

    do {
        c = *s++;
    } while (c != '.');
    return s;
}

int get_dir_level(char* s)
{
    s8 level = 0;
    char c;

    while ((c = *s++) != 0) {
        if (c == '/') {
            level++;
        }
    }
    return level;
}

void Snd_test_dump_init(SndTestWork* w)
{
    aram_dump_dma(w);
    aram_dump_disp(w);
}

void Snd_test_aram_dump(SndTestWork* w)
{
    u32 old = w->aramAdrs;

    aram_blk_no_select(w);
    if (w->on & 0x20) {
        if (w->rep & 1) {
            w->aramAdrs -= 0x100000;
        }
        if (w->rep & 2) {
            w->aramAdrs += 0x100000;
        }
        if (w->rep & 8) {
            w->aramAdrs -= 0x10000;
        }
        if (w->rep & 4) {
            w->aramAdrs += 0x10000;
        }
    } else {
        if (w->rep & 1) {
            w->aramAdrs -= 0x1000;
        }
        if (w->rep & 2) {
            w->aramAdrs += 0x1000;
        }
        if (w->rep & 8) {
            w->aramAdrs -= 0x100;
        }
        if (w->rep & 4) {
            w->aramAdrs += 0x100;
        }
    }
    if (w->trg & 0x400) {
        w->aramAdrs = Snd_ctrl_work.aram_base;
    }
    if ((s32) w->aramAdrs < 0) {
        w->aramAdrs = 0x00FFFF00;
    }
    if ((s32) w->aramAdrs > 0x00FFFFFF) {
        w->aramAdrs = 0;
    }
    if (old != w->aramAdrs) {
        aram_dump_dma(w);
    }
    aram_dump_disp(w);
}

void aram_blk_no_select(SndTestWork* w)
{
    if (w->tbl == 1) {
        return;
    }
    if (w->trg & 0x80000) {
        blk_enable_ck(w, -1);
    }
    if (w->trg & 0x40000) {
        blk_enable_ck(w, 1);
    }
}

void blk_enable_ck(SndTestWork* w, int dir)
{
    int i;
    s16 max = w->blkMax[w->tbl];
    int no = w->blkNo[w->tbl];

    for (i = 0; i < max; i++) {
        no += dir;
        if (no < 0) {
            no = max - 1;
        }
        if (no >= max) {
            no = 0;
        }
        if (Snd_iss_blk[no].num != 0) {
            w->blkNo[w->tbl] = no;
            w->reqNo[w->tbl] = 0;
            break;
        }
    }
    w->aramAdrs = Snd_iss_blk[w->blkNo[w->tbl]].aram & ~0xFF;
}

void aram_dump_dma(SndTestWork* w)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;

    ctrl->dma_busy = 1;
    ARQPostRequest(&ctrl->arq, 0, 1, 0, w->aramAdrs, (u32) Snd_test_work.dump, 0x100, cb_dma_end);
    do {
    } while (ctrl->dma_busy != 0);
    DCInvalidateRange(w->dump, 0x100);
}

static void cb_dma_end(u32 task)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;

    ctrl->dma_busy = 0;
}

void aram_dump_disp(SndTestWork* w)
{
    u32 i;
    u32 j;
    int n;
    int x;

    x = 0x68;
    for (i = 0; i < 16; i++) {
        eprintf(x, 0x46, 4, 1, "%02X", i);
        x += 0x18;
    }
    n = 0;
    for (i = 0; i < 16; i++) {
        int y = 0x54 + i * 14;

        eprintf(0x20, y, 4, 1, "%08X", w->aramAdrs + i * 16);
        x = 0x68;
        for (j = 0; j < 16; j++) {
            eprintf(x, y, 0, 1, "%02X", w->dump[n]);
            x += 0x18;
            n++;
        }
    }
    if (w->tbl != 1) {
        eprintf(0xD0, 0x16C, 0, 1, "PCM ADDRESS");
        eprintf(0xE0, 0x188, 0, 1, "%08XH", Snd_iss_blk[w->blkNo[w->tbl]].aram);
    }
}
