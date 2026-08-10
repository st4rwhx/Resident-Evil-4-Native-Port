#include "snd_drv.h"

int Snd_se_reset_check(SND_CTRL_WORK* ctrl)
{
    SND_AXV_WORK* axv;
    int i;

    if (ctrl->reset_flag & 0x10) {
        return 1;
    }
    if (ctrl->reset_flag & 0x1) {
        Snd_req_work_clear();
        for (i = 0; i < SND_AXV_MAX; i++) {
            axv = &Snd_axv_work[i];
            if (axv->status != 0) {
                Snd_axv_work_note_off(axv, 1);
            }
        }
        ctrl->reset_flag |= 0x10;
        ctrl->se_state = 0;
        ctrl->se_ctrl = 0;
        return 1;
    }
    return 0;
}

void Snd_req_work_clear(void)
{
    SND_REQ_WORK* req;
    u32 i;
    u32 j;
    u32 k;
    u8* p;

    for (i = 0; i < SND_REQ_BANK_MAX; i++) {
        for (j = 0; j < SND_REQ_MAX; j++) {
            req = &Snd_req_work[i][j];
            p = (u8*) req;
            for (k = 0; k < sizeof(SND_REQ_WORK); k++) {
                *p++ = 0;
            }
            req->no = j;
        }
    }
}

void Snd_req_work_copy_para(SND_CTRL_WORK* ctrl, SND_REQ_WORK* req)
{
    SND_SIT* sit;

    sit = Snd_iss_blk[req->blk_no].sit;
    sit += req->req_no;
    req->x1A = -1;
    req->x1B = -1;
    req->x1C = -1;
    req->x1D = -1;
    req->x1E = -1;
    req->x1F = -1;
    req->x20 = -1;
    req->x21 = -1;
    req->x22 = 0;
    req->x24 = 0;
    req->x26 = 0;
    req->flag = ctrl->flag_58;
    if (req->flag & 0x1) {
        req->x1A = ctrl->x48;
    }
    if (req->flag & 0x2) {
        req->x1B = ctrl->x49;
    }
    if (req->flag & 0x4) {
        req->x1C = ctrl->x4A;
    }
    if (req->flag & 0x8) {
        req->x1D = ctrl->x4B;
    }
    if (req->flag & 0x10) {
        req->x1E = ctrl->x4C;
    }
    if (req->flag & 0x20) {
        req->x1F = ctrl->x4D;
    }
    if (req->flag & 0x40) {
        req->x20 = ctrl->x4E;
    }
    if (req->flag & 0x80) {
        req->x21 = ctrl->x4F;
    }
    if (req->flag & 0x200) {
        req->x22 = ctrl->x52;
    }
    if (req->flag & 0x400) {
        req->x24 = ctrl->x54;
    }
    if (req->flag & 0x800) {
        req->x26 = ctrl->x56;
    }
    if (req->type != 4) {
        if (ctrl->multi_req == 1) {
            req->pitch = ctrl->rnd_pitch;
        } else {
            req->pitch = Snd_get_rnd_pitch(sit);
        }
    }
}

SND_REQ_WORK* Snd_open_req_work(void)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
    SND_REQ_WORK* req;
    int i;

    if (ctrl->reset_flag & 0x10) {
        return NULL;
    }
    for (i = 0; i < SND_REQ_MAX; i++) {
        req = &Snd_req_work[ctrl->req_bank][i];
        if (req->status == 0) {
            return req;
        }
    }
    return NULL;
}

SND_REQ_WORK* Snd_search_req_work_snd_id(u32 snd_id, u8 type)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
    SND_REQ_WORK* req;
    int i;
    int j;

    for (i = 0; i < SND_REQ_BANK_MAX; i++) {
        for (j = 0; j < SND_REQ_MAX; j++) {
            req = &Snd_req_work[i][j];
            if (req->status == 0) {
                continue;
            }
            if (req->type & 0x4) {
                continue;
            }
            if (!(req->type & type)) {
                continue;
            }
            if (req->snd_id != snd_id) {
                continue;
            }
            return req;
        }
    }
    return NULL;
}
