#include "snd_drv.h"

u32 Snd_str_prepare(u16 blk_no, u16 req_no, char* name, s8 no)
{
    SND_RIT* rit;
    SND_SHD* shd;
    u32 ret;

    rit = Snd_get_rit_adrs(blk_no, req_no);
    shd = Snd_get_shd_adrs(blk_no, req_no);
    ret = Snd_str_init(shd, rit, shd->aram, name, no);
    return ret;
}

u32 Snd_str_init(SND_SHD* shd, SND_RIT* rit, u32 aram, char* name, s8 no)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
    SND_STR_WORK* str;
    s8 idx;

    if (no >= 0) {
        idx = no;
    } else {
        idx = rit->str_no;
    }
    str = &Snd_str_work[idx];
    if (str->status != 0) {
        return 0;
    }
    DVDOpen(name, &str->dvd);
    ctrl->req_id++;
    if (ctrl->req_id == 0) {
        ctrl->req_id++;
    }
    str->snd_id = ctrl->req_id;
    if (rit->flag & 0x1) {
        str->type = 2;
    } else {
        str->type = 1;
    }
    str->upd = 0;
    str->shd = shd;
    str->rit = rit;
    str->buff = Snd_str_buff[idx];
    str->dvd_status = 0;
    str->flag = shd->flag;
    str->req = 0;
    str->err = 0;
    str->cancel = 0;
    str->loop_top = 0;
    str->shortflag = 0;
    str->pan = rit->pan;
    str->span = str_init_get_span(rit);
    str->vol = str_init_get_vol(rit);
    str->x2B = 0;
    str->auxA = rit->auxA;
    str->auxB = rit->auxB;
    str->rate = (f32) shd->rate;
    str->req_vol = 0;
    str->calc_vol = 0;
    str->vol2 = 0;
    str->fade_time = 0;
    str->fade_vol = 0;
    str->fade_step = 0;
    str->fade_target = 0;
    str->err_step = 0;
    str->err_target = 0;
    str->aram = aram;
    str->read_ofs = 0;
    if (str->flag & 0x1) {
        str->blk_half = 0x4000;
        str->read_size = str->blk_half * 2;
        str->read_end = shd->len / 2 * 2;
    } else {
        str->blk_half = 0x8000;
        str->read_size = str->blk_half;
        str->read_end = shd->len / 2;
    }
    if (str->read_end % str->read_size != 0) {
        str->read_end = str->read_end / str->read_size + 1;
        str->read_end = str->read_end * str->read_size;
    }
    str->blk_size = str->blk_half * 2;
    str->play_nbl = 0;
    str->play_pos = 0;
    str->x84 = str->blk_size;
    str->loop_start = shd->loop_start;
    str->loop_end = shd->loop_end;
    str->play_blk = -1;
    str->prev_blk = -1;
    str->blk_cnt = 0;
    str->dvd_busy = 0;
    str->read_done = 0;
    str->read_cnt = 1;
    str->read_blk = 0;
    str->dma_blk = -1;
    str->buff_blks = 1;
    str->dma_busy = 0;
    str->dma_cnt = 0;
    str->dma_aram_blk = 0;
    str->dma_last_blk = -1;
    str->aram_blks = 8;
    if (str->read_end <= str->read_size * str->aram_blks) {
        str->shortflag |= 0x1;
        if (str->flag & 0x4) {
            str->shortflag |= 0x2;
        } else {
            str->shortflag |= 0x4;
        }
    }
    if (Snd_str_ax_voice_init(str, rit->voice_start, rit->voice_num) == 1) {
        return 0;
    }
    str->state = 0;
    str->prev_state = 0;
    str->status = 1;
    return str->snd_id;
}

s8 str_init_get_span(SND_RIT* rit)
{
    if (rit->span >= 0) {
        return rit->span;
    } else {
        return 0x7F;
    }
}

s8 str_init_get_vol(SND_RIT* rit)
{
    if (Snd_get_sound_mode() == 2 && rit->svol >= 0) {
        return rit->svol;
    }
    return rit->vol;
}

int Snd_str_req(u32 snd_id, u32 cmd, u32 time, u32 vol)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = str_req_sub(snd_id, cmd, time, vol);
    OSRestoreInterrupts(old);
    return 0;
}

int str_req_sub(u32 snd_id, u32 cmd, u32 time, u32 vol)
{
    SND_STR_WORK* str;

    str = Snd_search_str_work_snd_id(snd_id);
    if (str == NULL) {
        return 1;
    }
    str->req |= cmd;
    if (cmd & 0x6) {
        str->fade_time = time;
        str->fade_vol = vol;
    }
    if (cmd & 0x10) {
        str->req_vol = time;
    }
    return 0;
}

void Snd_str_reset_vol_type(u8 type)
{
    str_type_sub(type, 0, 0);
}

void Snd_str_reset_pan_type(u8 type)
{
    str_type_sub(type, 1, 0);
}

void Snd_str_fade_out_type(u8 type, s16 time)
{
    str_type_sub(type, 2, time);
}

void str_type_sub(u8 type, u32 mode, s16 time)
{
    SND_STR_WORK* str;
    int old;
    int i;

    old = OSDisableInterrupts();
    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        if (str->status == 0) {
            continue;
        }
        if (!(str->type & type)) {
            continue;
        }
        switch (mode) {
        case 1:
            str->upd |= 0x2;
            break;
        case 0:
            str->upd |= 0x1;
            break;
        case 2:
            if (time == 0) {
                str->req |= 0x8;
            } else {
                str->req |= 0x4;
                str->fade_time = time;
                str->fade_vol = 0;
            }
            break;
        }
    }
    OSRestoreInterrupts(old);
}

int Snd_str_get_status(u32 snd_id)
{
    SND_STR_WORK* str;

    str = Snd_search_str_work_snd_id(snd_id);
    if (str == NULL) {
        return -1;
    }
    return (s16) str->status;
}

int Snd_str_end_check(u32 snd_id)
{
    SND_STR_WORK* str;

    str = Snd_search_str_work_snd_id(snd_id);
    if (str == NULL) {
        return 0;
    } else {
        return 1;
    }
}

int Snd_str_pronounce_ck_type(u8 type)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = str_pro_ck_str_work(type);
    OSRestoreInterrupts(old);
    return ret;
}

int str_pro_ck_str_work(u8 type)
{
    SND_STR_WORK* str;
    int i;

    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        if (str->status != 0 && (type & str->type)) {
            return 4;
        }
    }
    return 0;
}

void Snd_str_aram_adrs_set(int no, u32 adr)
{
    SND_STR_WORK* str;

    str = &Snd_str_work[no];
    str->aram_L = adr;
    str->aram_R = str->aram_L + 0x20000;
    str->aram_L_nbl = str->aram_L * 2;
    str->aram_R_nbl = str->aram_R * 2;
}
