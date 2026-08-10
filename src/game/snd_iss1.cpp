#include "snd_drv.h"

int Snd_se_set_paras(u32 snd_id)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = se_set_paras_sub(snd_id);
    OSRestoreInterrupts(old);
    return ret;
}

int se_set_paras_sub(u32 snd_id)
{
    SND_REQ_WORK* req;

    req = Snd_open_req_work();
    if (req == NULL) {
        return 1;
    }
    req->status = 1;
    req->type = 4;
    req->cmd = 1;
    req->snd_id = snd_id;
    Snd_req_work_copy_para(&Snd_ctrl_work, req);
    return 0;
}

int Snd_se_end_check(u32 snd_id)
{
    SND_REQ_WORK* req;
    SND_VOICE_WORK* voice;
    int old;

    old = OSDisableInterrupts();
    req = Snd_search_req_work_snd_id(snd_id, 2);
    OSRestoreInterrupts(old);
    if (req != NULL) {
        return -1;
    }
    old = OSDisableInterrupts();
    voice = Snd_search_voice_work_snd_id(snd_id);
    OSRestoreInterrupts(old);
    if (voice == NULL) {
        return 0;
    }
    return 1;
}

int Snd_se_stop_one(u32 snd_id)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = se_cmd_req_work(0, snd_id, 0);
    OSRestoreInterrupts(old);
    return ret;
}

int se_cmd_req_work(u16 cmd, u32 snd_id, u16 para)
{
    SND_REQ_WORK* req;

    req = Snd_open_req_work();
    if (req == NULL) {
        return 1;
    }
    req->status = 1;
    req->type = 4;
    req->cmd = cmd;
    req->snd_id = snd_id;
    req->para = para;
    return 0;
}

int Snd_se_fade_out_all(s16 time)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = se_ctrl_sub(0x200, time);
    OSRestoreInterrupts(old);
    return ret;
}

int Snd_se_fade_out_all2(s16 time)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = se_ctrl_sub(0x400, time);
    OSRestoreInterrupts(old);
    return ret;
}

int se_ctrl_sub(u16 cmd, s16 para)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;

    if (ctrl->reset_flag & 0x10) {
        return 1;
    }
    switch (cmd) {
    case 0x200:
        ctrl->se_ctrl |= 0x200;
        ctrl->se_fade_time = para;
        break;
    case 0x400:
        ctrl->se_ctrl |= 0x400;
        ctrl->se_fade_time = para;
        break;
    case 0x1:
        ctrl->se_ctrl |= 0x1;
        ctrl->se_state |= 0x1;
        break;
    case 0x2:
        ctrl->se_ctrl |= 0x2;
        ctrl->se_pause_type = para;
        break;
    case 0x4:
        ctrl->se_ctrl |= 0x4;
        ctrl->se_pause_type = para;
        break;
    case 0x8:
        ctrl->se_ctrl |= 0x8;
        break;
    case 0x10:
        ctrl->se_ctrl |= 0x10;
        ctrl->se_pause_type = para;
        break;
    case 0x20:
        ctrl->se_ctrl |= 0x20;
        ctrl->se_state |= 0x2;
        ctrl->se_vdown_vol = para;
        break;
    case 0x40:
        ctrl->se_ctrl |= 0x40;
        break;
    }
    return 0;
}

int Snd_se_pronounce_ck_all(void)
{
    SND_CTRL_WORK* ctrl;
    int old;
    int ret;

    old = OSDisableInterrupts();
    ctrl = &Snd_ctrl_work;
    ret = 0;
    ret |= se_pro_ck_req_work(ctrl->req_bank);
    ret |= se_pro_ck_req_work(ctrl->req_bank_sub);
    OSRestoreInterrupts(old);
    old = OSDisableInterrupts();
    ret |= se_pro_ck_axv_work();
    OSRestoreInterrupts(old);
    return ret;
}

int se_pro_ck_req_work(int bank)
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
        if (!(req->type & 0x2)) {
            continue;
        }
        if (req->sit->flag & 0x3) {
            return 0x10;
        }
    }
    return 0;
}

int se_pro_ck_axv_work(void)
{
    SND_AXV_WORK* axv;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (axv->status != 0) {
            return 1;
        }
    }
    return 0;
}

int Snd_se_pause_on2(s16 type)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = se_ctrl_sub(0x2, type);
    OSRestoreInterrupts(old);
    return ret;
}

int Snd_se_pause_on3(void)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = se_ctrl_sub(0x4, 0);
    OSRestoreInterrupts(old);
    return ret;
}

int Snd_se_pause_off2(s16 type)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = se_ctrl_sub(0x10, type);
    OSRestoreInterrupts(old);
    return ret;
}
