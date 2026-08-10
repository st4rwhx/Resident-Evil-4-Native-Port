#include "snd_drv.h"

int Snd_seq_req(u32 snd_id, u32 cmd, u32 time, u32 vol)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = seq_req_sub(snd_id, cmd, time, vol);
    OSRestoreInterrupts(old);
    return ret;
}

int seq_req_sub(u32 snd_id, u32 cmd, u32 time, u32 vol)
{
    SND_SEQ_WORK* seq;

    seq = Snd_search_seq_work_snd_id(snd_id);
    if (seq == NULL) {
        return 1;
    }
    seq->req |= cmd;
    if (cmd & 0x1) {
        seq->fade_time = time;
        seq->fade_vol = vol;
    }
    if (cmd & 0x4) {
        seq->vol = time;
    }
    return 0;
}

void Snd_seq_reset_vol_type(u8 type)
{
    seq_type_sub(type, 0, 0);
}

void Snd_seq_fade_out_type(u8 type, s16 time)
{
    seq_type_sub(type, 1, time);
}

void seq_type_sub(u8 type, int mode, s16 time)
{
    SND_SEQ_WORK* seq;
    int old;
    int i;

    old = OSDisableInterrupts();
    for (i = 0; i < SND_SEQ_MAX; i++) {
        seq = &Snd_seq_work[i];
        if (seq->status == 0) {
            continue;
        }
        if (!(seq->type & type)) {
            continue;
        }
        switch (mode) {
        case 0:
            seq->flag |= 0x1;
            break;
        case 1:
            if (time == 0) {
                seq->req |= 0x2;
            } else {
                seq->req |= 0x1;
                seq->fade_time = time;
                seq->fade_vol = 0;
            }
            break;
        }
    }
    OSRestoreInterrupts(old);
}

int Snd_seq_fade_check(u32 snd_id)
{
    SND_SEQ_WORK* seq;

    seq = Snd_search_seq_work_snd_id(snd_id);
    if (seq == NULL) {
        return -1;
    }
    if (seq->status & 0x100) {
        return 1;
    } else {
        return 0;
    }
}

int Snd_seq_end_check(u32 snd_id)
{
    SND_REQ_WORK* req;
    SND_SEQ_WORK* seq;
    int old;

    old = OSDisableInterrupts();
    req = Snd_search_req_work_snd_id(snd_id, 3);
    OSRestoreInterrupts(old);
    if (req != NULL) {
        return -1;
    }
    seq = Snd_search_seq_work_snd_id(snd_id);
    if (seq == NULL) {
        return 0;
    }
    return 1;
}

int Snd_seq_pronounce_ck_type(u8 type)
{
    SND_CTRL_WORK* ctrl;
    int old;
    int ret;

    old = OSDisableInterrupts();
    ctrl = &Snd_ctrl_work;
    ret = 0;
    ret |= seq_pro_ck_req_work(ctrl->req_bank, type);
    ret |= seq_pro_ck_req_work(ctrl->req_bank_sub, type);
    OSRestoreInterrupts(old);
    old = OSDisableInterrupts();
    ret |= seq_pro_ck_seq_work(type);
    OSRestoreInterrupts(old);
    return ret;
}

int seq_pro_ck_req_work(int bank, u8 type)
{
    SND_REQ_WORK* req;
    int i;

    for (i = 0; i < SND_REQ_MAX; i++) {
        req = &Snd_req_work[bank][i];
        if (req->status == 0) {
            continue;
        }
        if (req->type & 0x4) {
            continue;
        }
        if (!(req->type & type)) {
            continue;
        }
        if (req->sit->flag & 0x4) {
            return 0x10;
        }
    }
    return 0;
}

int seq_pro_ck_seq_work(u8 type)
{
    SND_SEQ_WORK* seq;
    int i;

    for (i = 0; i < SND_SEQ_MAX; i++) {
        seq = &Snd_seq_work[i];
        if (seq->status != 0 && (type & seq->type)) {
            return 2;
        }
    }
    return 0;
}
