#include "snd_drv.h"

void Snd_axv_work_clear(void)
{
    SND_AXV_WORK* axv;
    u32 i;
    u32 j;
    u8* p;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        p = (u8*) axv;
        for (j = 0; j < sizeof(SND_AXV_WORK); j++) {
            *p++ = 0;
        }
        axv->no = i;
    }
}

SND_AXV_WORK* Snd_open_axv_work(void)
{
    SND_AXV_WORK* axv;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (axv->status == 0) {
            return axv;
        }
    }
    OSReport("Snd_axv_work is full !!\n");
    return NULL;
}

void Snd_axv_work_close_check(void)
{
    SND_AXV_WORK* axv;
    SND_VOICE_WORK* vw;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (axv->status != 0) {
            axv_close_ck_main(axv);
        }
    }
    for (i = 0; i < SND_VOICE_MAX; i++) {
        vw = &Snd_voice_work[i];
        if (vw->status == 0) {
            continue;
        }
        if (vw->count != -1) {
            vw->count++;
        }
    }
}

void axv_close_ck_main(SND_AXV_WORK* axv)
{
    SND_VOICE_WORK* vw;

    if (axv->status & 0x8) {
        return;
    }
    if (axv->voice->pb.state != 0) {
        return;
    }
    MIXReleaseChannel(axv->voice);
    AXFreeVoice(axv->voice);
    vw = axv->vw;
    if (vw != NULL) {
        vw->status = 0;
        vw->axv = NULL;
    }
    axv->status = 0;
    axv->vw = NULL;
    axv->voice = NULL;
}

void Snd_axv_work_note_off(SND_AXV_WORK* axv, s32 time)
{
    SND_VOICE_WORK* vw;
    u16 mask;
    s16 diff;

    if (axv == NULL) {
        return;
    }
    axv->rel_time = time;
    axv->env_target = 0;
    diff = axv->env_target - axv->env_vol;
    axv->env_step = diff / axv->rel_time;
    if (axv->env_step == 0) {
        if (diff > 0) {
            axv->env_step = 1;
        } else {
            axv->env_step = -1;
        }
    }
    axv->env_cnt = 0;
    AXSetVoicePriority(axv->voice, 1);
    vw = axv->vw;
    if (vw != NULL) {
        vw->status = 0;
        vw->axv = NULL;
    }
    mask = 0xA;
    axv->status &= ~mask;
    axv->status |= 0x4;
    axv->vw = NULL;
}

int Snd_axv_work_get_out_mode(SND_AXV_WORK* axv)
{
    if (Snd_get_sound_mode() == 2) {
        if (axv->srd_type == 0) {
            return 0;
        } else {
            return 1;
        }
    } else {
        return 0;
    }
}

void Snd_axv_work_choice_now_vol(SND_AXV_WORK* axv)
{
    if (Snd_axv_work_get_out_mode(axv) == 1) {
        if (axv->status & 0x10) {
            axv->now_vol = axv->vdown_svol;
        } else {
            axv->now_vol = axv->svol;
        }
    } else {
        if (axv->status & 0x10) {
            axv->now_vol = axv->vdown_vol;
        } else {
            axv->now_vol = axv->vol;
        }
    }
}

void Snd_axv_work_calc_vdown_vol(SND_AXV_WORK* axv)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;

    axv->vdown_vol = axv->vdown_src_vol / 127 * ctrl->se_vdown_vol;
    axv->vdown_svol = axv->vdown_src_svol / 127 * ctrl->se_vdown_vol;
}

void Snd_axv_work_calc_ax_vol(SND_AXV_WORK* axv)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;

    axv->calc_vol = ctrl->sys_vol[1] / 127 * (ctrl->sys_vol[3] >> 8);
    axv->calc_vol = axv->calc_vol / 127 * (axv->now_vol >> 8);
    axv->calc_vol = axv->calc_vol / 127 * (axv->env_vol >> 8);
    axv->ax_vol = Snd_vol_syn_to_ax((s16) (axv->calc_vol >> 8));
}

void Snd_axv_work_choice_out_span(SND_AXV_WORK* axv)
{
    if (Snd_axv_work_get_out_mode(axv) == 1) {
        axv->out_span = axv->span;
    } else {
        axv->out_span = 0x7F;
    }
}

void Snd_axv_work_control(void)
{
    SND_AXV_WORK* axv;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (axv->status == 0) {
            continue;
        }
        axv_work_adsr(axv);
        axv_work_update(axv);
    }
}

void axv_work_adsr(SND_AXV_WORK* axv)
{
    AXVPB* voice;
    int v;

    if ((axv->status & 0x6) == 0) {
        return;
    }
    voice = axv->voice;
    if (voice->pb.state == 0) {
        return;
    }
    if (axv->env_vol == axv->env_target) {
        if (axv->status & 0x2) {
            axv->status &= ~0x2;
        } else {
            AXSetVoiceState(voice, 0);
        }
        return;
    }
    v = axv->env_vol + axv->env_step;
    if (axv->env_step > 0) {
        if (v > axv->env_target) {
            v = axv->env_target;
        }
    } else {
        if (v < axv->env_target) {
            v = axv->env_target;
        }
    }
    axv->env_vol = v;
    axv->env_cnt++;
    axv->upd |= 0x1;
}

void axv_work_update(SND_AXV_WORK* axv)
{
    if (axv->upd & 0x100) {
        axv->ax_vol = Snd_vol_syn_to_ax(0);
        MIXSetInput(axv->voice, axv->ax_vol);
        AXSetVoiceState(axv->voice, 0);
    }
    if (axv->upd & 0x200) {
        axv->upd |= 0x1;
        AXSetVoiceState(axv->voice, 1);
    }
    axv_work_update_vol_pan(axv);
    axv_work_update_aux(axv);
    axv_work_update_lpf(axv);
    axv_work_update_pitch(axv);
    axv->upd = 0;
}

void axv_work_update_vol_pan(SND_AXV_WORK* axv)
{
    if (axv->upd & 0x1) {
        Snd_axv_work_choice_now_vol(axv);
        Snd_axv_work_calc_ax_vol(axv);
        if (axv->voice->pb.state != 0) {
            MIXSetInput(axv->voice, axv->ax_vol);
        }
    }
    if (axv->upd & 0x2) {
        Snd_axv_work_choice_out_span(axv);
        MIXSetPan(axv->voice, axv->pan);
        MIXSetSPan(axv->voice, axv->out_span);
    }
}

void axv_work_update_aux(SND_AXV_WORK* axv)
{
    if (axv->upd & 0x4) {
        axv->ax_auxA = Snd_vol_syn_to_ax(axv->auxA);
        MIXSetAuxA(axv->voice, axv->ax_auxA);
    }
    if (axv->upd & 0x8) {
        axv->ax_auxB = Snd_vol_syn_to_ax(axv->auxB);
        MIXSetAuxB(axv->voice, axv->ax_auxB);
    }
}

void axv_work_update_lpf(SND_AXV_WORK* axv)
{
    AXPBLPF lpf;
    u16 a0;
    u16 b0;

    if (axv->upd & 0x10) {
        if (axv->lpf_on != 0) {
            lpf.on = 1;
            lpf.yn1 = 0;
            lpf.a0 = Snd_lpf_tbl[axv->lpf_no].a0;
            lpf.b0 = Snd_lpf_tbl[axv->lpf_no].b0;
        } else {
            lpf.on = 0;
            lpf.yn1 = 0;
            lpf.a0 = 0;
            lpf.b0 = 0;
        }
        AXSetVoiceLpf(axv->voice, &lpf);
    }
    if (axv->upd & 0x20) {
        a0 = Snd_lpf_tbl[axv->lpf_no].a0;
        b0 = Snd_lpf_tbl[axv->lpf_no].b0;
        AXSetVoiceLpfCoefs(axv->voice, a0, b0);
    }
}

void axv_work_update_pitch(SND_AXV_WORK* axv)
{
    f64 ratio;
    u32 r;

    if ((axv->upd & 0x40) == 0) {
        return;
    }
    ratio = pow(2.0, (f64) axv->pitch / 1200.0);
    ratio = axv->rate * (f32) ratio / 32000.0f;
    r = (u32) (ratio * 65536.0);
    if (r > 0x40000) {
        OSReport("SND Sample Rate Over.\n");
    }
    AXSetVoiceSrcRatio(axv->voice, ratio);
}
