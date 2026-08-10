#include "snd_drv.h"

typedef void (*SND_REQ_CMD)(SND_AXV_WORK*, SND_REQ_WORK*, u16);

void Snd_iss_manager(void)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
    int ret;

    Snd_axv_work_close_check();
    ret = Snd_se_reset_check(ctrl);
    if (ret == 0) {
        se_ctrl_execute(ctrl);
        iss_req_execute(ctrl);
    }
    Snd_axv_work_control();
}

void se_ctrl_execute(SND_CTRL_WORK* ctrl)
{
    if (ctrl->se_ctrl & 0x200) {
        se_ctrl_fade_out(ctrl, 1);
    }
    if (ctrl->se_ctrl & 0x400) {
        se_ctrl_fade_out(ctrl, 2);
    }
    if (ctrl->se_ctrl & 0x1) {
        se_ctrl_pause_on(ctrl);
    }
    if (ctrl->se_ctrl & 0x2) {
        se_ctrl_pause_on2(ctrl);
    }
    if (ctrl->se_ctrl & 0x4) {
        se_ctrl_pause_on(ctrl);
    }
    if (ctrl->se_ctrl & 0x8) {
        se_ctrl_pause_off(ctrl);
    }
    if (ctrl->se_ctrl & 0x10) {
        se_ctrl_pause_off2(ctrl);
    }
    if (ctrl->se_ctrl & 0x20) {
        se_ctrl_vdown_on(ctrl);
    }
    if (ctrl->se_ctrl & 0x40) {
        se_ctrl_vdown_off(ctrl);
    }
    if (ctrl->se_ctrl & 0x80) {
        se_ctrl_reset_pan_or_vol(ctrl, 3);
    }
    if (ctrl->se_ctrl & 0x100) {
        se_ctrl_reset_pan_or_vol(ctrl, 4);
    }
    ctrl->se_ctrl = 0;
}

void se_ctrl_fade_out(SND_CTRL_WORK* ctrl, int mode)
{
    SND_AXV_WORK* axv;
    SND_VOICE_WORK* vw;
    int i;
    s32 time;

    for (i = 0; i < SND_VOICE_MAX; i++) {
        vw = &Snd_voice_work[i];
        if (vw->status == 0) {
            continue;
        }
        if (vw->type != 1) {
            continue;
        }
        axv = vw->axv;
        if (axv == NULL) {
            continue;
        }
        if (mode == 1 && (axv->flag & 0x4)) {
            continue;
        }
        if (ctrl->se_fade_time == 0) {
            time = vw->rel_time;
        } else {
            time = ctrl->se_fade_time;
        }
        Snd_axv_work_note_off(axv, time);
    }
}

void se_ctrl_pause_on(SND_CTRL_WORK* ctrl)
{
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        seCtrlPauseOn_sub(&Snd_axv_work[i], ctrl);
    }
}

void se_ctrl_pause_on2(SND_CTRL_WORK* ctrl)
{
    SND_AXV_WORK* axv;
    SND_VOICE_WORK* vw;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (ctrl->se_pause_type != -1) {
            if (axv->status != 0) {
                vw = axv->vw;
                if (vw != NULL && vw->blk_no == ctrl->se_pause_type) {
                    seCtrlPauseOn_sub(axv, ctrl);
                }
            }
        } else {
            seCtrlPauseOn_sub(axv, ctrl);
        }
    }
}

void seCtrlPauseOn_sub(SND_AXV_WORK* axv, SND_CTRL_WORK* ctrl)
{
    SND_VOICE_WORK* vw;
    u32 cur;
    u32 end;

    if (axv->status == 0) {
        return;
    }
    if (axv->status & 0x4) {
        return;
    }
    if (!(ctrl->se_ctrl & 0x4) && (axv->flag & 0x1)) {
        return;
    }
    if (axv->voice->pb.state == 0) {
        return;
    }
    if (axv->voice->pb.addr.loopFlag == 0) {
        cur = *(u32*) &axv->voice->pb.addr.currentAddressHi;
        end = *(u32*) &axv->voice->pb.addr.endAddressHi;
        if (end - cur <= 800) {
            return;
        }
    }
    axv->status |= 0x8;
    axv->upd |= 0x100;
    vw = axv->vw;
    if (vw != NULL) {
        vw->status |= 0x2;
    }
}

void se_ctrl_pause_off(SND_CTRL_WORK* ctrl)
{
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        seCtrlPauseOff_sub(&Snd_axv_work[i]);
    }
    ctrl->se_state &= ~0x1;
}

void se_ctrl_pause_off2(SND_CTRL_WORK* ctrl)
{
    SND_AXV_WORK* axv;
    SND_VOICE_WORK* vw;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (ctrl->se_pause_type != -1) {
            if (axv->status != 0) {
                vw = axv->vw;
                if (vw != NULL && vw->blk_no == ctrl->se_pause_type) {
                    seCtrlPauseOff_sub(axv);
                }
            }
        } else {
            seCtrlPauseOff_sub(axv);
        }
    }
}

void seCtrlPauseOff_sub(SND_AXV_WORK* axv)
{
    SND_VOICE_WORK* vw;

    if (axv->status == 0) {
        return;
    }
    if (axv->status & 0x8) {
        axv->status &= ~0x8;
        axv->upd |= 0x200;
        vw = axv->vw;
        if (vw != NULL) {
            vw->status &= ~0x2;
        }
    }
}

void se_ctrl_vdown_on(SND_CTRL_WORK* ctrl)
{
    SND_AXV_WORK* axv;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (axv->status == 0) {
            continue;
        }
        if (axv->status & 0x4) {
            continue;
        }
        if (axv->flag & 0x2) {
            continue;
        }
        axv->status |= 0x10;
        axv->upd |= 0x1;
        Snd_axv_work_calc_vdown_vol(axv);
    }
}

void se_ctrl_vdown_off(SND_CTRL_WORK* ctrl)
{
    SND_AXV_WORK* axv;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (axv->status == 0) {
            continue;
        }
        if ((axv->status & 0x10) == 0) {
            continue;
        }
        axv->status &= ~0x10;
        axv->upd |= 0x1;
    }
    ctrl->se_state &= ~0x2;
}

void se_ctrl_reset_pan_or_vol(SND_CTRL_WORK* ctrl, int mode)
{
    SND_AXV_WORK* axv;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (axv->status == 0) {
            continue;
        }
        if (axv->status & 0x4) {
            continue;
        }
        if (mode == 3) {
            axv->upd |= 0x2;
        } else {
            axv->upd |= 0x1;
        }
    }
}

void iss_req_execute(SND_CTRL_WORK* ctrl)
{
    SND_REQ_WORK* req;
    int i;

    for (i = 0; i < SND_REQ_MAX; i++) {
        req = &Snd_req_work[ctrl->req_bank_sub][i];
        if (req->status == 0) {
            continue;
        }
        if (req->type & 0x4) {
            iss_req_command(req);
        } else {
            Snd_req_iss_new_play(req);
        }
        req->status = 0;
    }
    ctrl->req_bank ^= 1;
    ctrl->req_bank_sub ^= 1;
}

void iss_req_command(SND_REQ_WORK* req)
{
    if (req->cmd == 0) {
        req_cmd_se_stop(req);
    } else {
        req_cmd_se_para(req);
    }
}

void req_cmd_se_stop(SND_REQ_WORK* req)
{
    SND_VOICE_WORK* vw;
    int i;

    for (i = 0; i < SND_VOICE_MAX; i++) {
        vw = &Snd_voice_work[i];
        if (vw->status == 0) {
            continue;
        }
        if (vw->snd_id != req->snd_id) {
            continue;
        }
        if (vw->type != 1) {
            continue;
        }
        Snd_stop_voice_work(vw);
    }
}

void req_cmd_se_para(SND_REQ_WORK* req)
{
    SND_VOICE_WORK* vw;
    SND_AXV_WORK* axv;
    int i;
    int j;
    u16 bit;
    static SND_REQ_CMD req_cmd_se_tbl[] = {
        NULL,           req_cmd_se_pan, req_cmd_se_pan, req_cmd_se_vol,   req_cmd_se_vol,   req_cmd_se_aux, req_cmd_se_aux,
        req_cmd_se_lpf, NULL,           req_cmd_se_pitch, req_cmd_se_pitch, NULL,           NULL,
    };

    for (i = 0; i < SND_VOICE_MAX; i++) {
        vw = &Snd_voice_work[i];
        if (vw->status == 0) {
            continue;
        }
        if (vw->snd_id != req->snd_id) {
            continue;
        }
        if (vw->type != 1) {
            continue;
        }
        axv = vw->axv;
        if (axv == NULL) {
            continue;
        }
        bit = 2;
        for (j = 1; j <= 10; j++) {
            if ((req->flag & bit) && req_cmd_se_tbl[j] != NULL) {
                req_cmd_se_tbl[j](axv, req, bit);
            }
            bit <<= 1;
        }
    }
}

void req_cmd_se_pan(SND_AXV_WORK* axv, SND_REQ_WORK* req, u16 bit)
{
    if (bit == 0x2) {
        axv->pan = req->x1B;
    } else {
        axv->span = req->x1C;
    }
    axv->upd |= 0x2;
}

void req_cmd_se_vol(SND_AXV_WORK* axv, SND_REQ_WORK* req, u16 bit)
{
    if (bit == 0x8) {
        axv->vol = req->x1D << 8;
        axv->vdown_src_vol = axv->vol;
    } else {
        axv->svol = req->x1E << 8;
        axv->vdown_src_svol = axv->svol;
    }
    axv->upd |= 0x1;
}

void req_cmd_se_aux(SND_AXV_WORK* axv, SND_REQ_WORK* req, u16 bit)
{
    if (bit == 0x20) {
        axv->auxA = req->x1F;
        axv->upd |= 0x4;
    } else {
        axv->auxB = req->x20;
        axv->upd |= 0x8;
    }
}

void req_cmd_se_lpf(SND_AXV_WORK* axv, SND_REQ_WORK* req, u16 bit)
{
    if (axv->lpf_on == 0) {
        if (req->x21 == -1) {
            return;
        }
        axv->lpf_on = 1;
        axv->lpf_no = req->x21;
        axv->upd |= 0x10;
    } else {
        if (req->x21 == -1) {
            axv->lpf_on = 0;
            axv->lpf_no = -1;
            axv->upd |= 0x10;
        } else {
            axv->lpf_no = req->x21;
            axv->upd |= 0x20;
        }
    }
}

void req_cmd_se_pitch(SND_AXV_WORK* axv, SND_REQ_WORK* req, u16 bit)
{
    if (bit == 0x200) {
        axv->pitch_base += req->x22;
    } else {
        axv->pitch_ofs = req->x24;
    }
    axv->pitch = axv->pitch_base + axv->pitch_ofs;
    if (axv->pitch > 2400) {
        OSReport("OUT PITCH HIGH over : %d\n", axv->pitch);
        axv->pitch = 2400;
    }
    if (axv->pitch < -2400) {
        OSReport("OUT PITCH LOW  over : %d\n", axv->pitch);
        axv->pitch = -2400;
    }
    axv->upd |= 0x40;
}
