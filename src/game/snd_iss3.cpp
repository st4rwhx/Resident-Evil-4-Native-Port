#include "snd_drv.h"

void Snd_req_iss_new_play(SND_REQ_WORK* req)
{
    SND_ISS_BLK* blk;
    SND_SIT* sit;

    blk = &Snd_iss_blk[req->blk_no];
    sit = blk->sit;
    sit += req->req_no;
    if (sit->flag & 0x4) {
        Snd_iss_new_seq_work(blk, sit, req);
    } else {
        iss_new_voice_work(blk, sit, req);
    }
}

void iss_new_voice_work(SND_ISS_BLK* blk, SND_SIT* sit, SND_REQ_WORK* req)
{
    SND_VOICE_WORK* vw;
    SND_AXV_WORK* axv;
    s8 prio;

    if (req->x1A >= 0) {
        prio = req->x1A;
    } else {
        prio = sit->prio;
    }
    vw = Snd_voice_work_open_ck(sit, prio);
    if (vw == NULL) {
        return;
    }
    axv = Snd_open_axv_work();
    if (axv == NULL) {
        return;
    }
    axv->voice = AXAcquireVoice(30, cb_drop_voice, 0);
    if (axv->voice == NULL) {
        return;
    }
    iss_voice_work_init(vw, axv, req, prio);
    if (sit->flag & 0x1) {
        axv->adsr_on = 0;
    } else {
        axv->adsr_on = 1;
    }
    axv->aram = blk->aram;
    axv->wt = blk->dls;
    axv->sit = sit;
    iss_ax_set_wt_ptr(axv, sit);
    iss_ax_set_adsr(axv);
    iss_ax_set_vol(axv, req, sit);
    iss_ax_set_pan(axv, req, sit);
    iss_ax_set_aux(axv, req, sit);
    iss_ax_set_pitch(axv, req, sit);
    iss_ax_set_lpf(axv, req);
    iss_ax_set_para(axv, req);
    vw->rel_time = axv->rel_time;
    MIXInitChannel(axv->voice, 0, axv->ax_vol, axv->ax_auxA, axv->ax_auxB, axv->pan, axv->out_span, 0);
    AXSetVoiceState(axv->voice, 1);
}

void iss_voice_work_init(SND_VOICE_WORK* vw, SND_AXV_WORK* axv, SND_REQ_WORK* req, s8 prio)
{
    vw->status = 1;
    vw->snd_id = req->snd_id;
    vw->x8 = req->srd_type;
    vw->out_mode = 2;
    vw->type = 1;
    vw->count = 0;
    vw->axv = axv;
    vw->blk_no = req->blk_no;
    vw->req_no = req->req_no;
    vw->prio = prio;
    axv->status = 1;
    axv->snd_id = req->snd_id;
    axv->srd_type = req->srd_type;
    axv->upd = 0;
    axv->flag = req->x26;
    axv->vw = vw;
}

void iss_ax_set_wt_ptr(SND_AXV_WORK* axv, SND_SIT* sit)
{
    u16 prog;

    prog = sit->prog;
    axv->hdr = (SND_WT_HDR*) axv->wt;
    axv->inst = (WTINST*) (axv->wt + axv->hdr->inst_ofs);
    axv->inst += (u16) (prog >> 8);
    axv->rgn = (WTREGION*) (axv->wt + axv->hdr->rgn_ofs);
    axv->rgn += axv->inst->keyRegion[prog & 0xFF];
    axv->art = (WTART*) (axv->wt + axv->hdr->art_ofs);
    axv->art += axv->rgn->articulationIndex;
    axv->sample = (WTSAMPLE*) (axv->wt + axv->hdr->sample_ofs);
    axv->sample += axv->rgn->sampleIndex;
    axv->adpcm = (WTADPCM*) (axv->wt + axv->hdr->adpcm_ofs);
    axv->adpcm += axv->sample->adpcmIndex;
}

void iss_ax_set_adsr(SND_AXV_WORK* axv)
{
    s32 attack;
    u32 steps;

    axv->attack_steps = 0;
    axv->rel_time = 1;
    axv->env_vol = 0x7F00;
    axv->env_target = 0x7F00;
    axv->env_step = 0;
    axv->env_cnt = 0;
    if (axv->adsr_on != 1) {
        return;
    }
    axv->rel_time = (s32) 0xFC400000 / axv->art->eg1Release;
    attack = axv->art->eg1Attack;
    if (attack == (s32) 0x80000000) {
        return;
    }
    steps = (u32) (pow(2.0, (f64) attack / 78643200.0) * 1000.0);
    if (steps > 4) {
        axv->attack_steps = steps / 5;
    } else {
        axv->attack_steps = 1;
    }
    axv->env_vol = 0;
    axv->env_step = axv->env_target / axv->attack_steps;
    axv->status |= 0x2;
}

void iss_ax_set_vol(SND_AXV_WORK* axv, SND_REQ_WORK* req, SND_SIT* sit)
{
    s32 vol;

    if (req->x1D >= 0) {
        axv->vol = req->x1D;
    } else if (sit->vol >= 0) {
        axv->vol = sit->vol;
    } else {
        vol = axv->rgn->attn / 0x10000;
        axv->vol = Snd_vol_ax_to_syn(vol);
    }
    if (req->x1E >= 0) {
        axv->svol = req->x1E;
    } else if (sit->svol >= 0) {
        axv->svol = sit->svol;
    } else {
        axv->svol = axv->vol;
    }
    axv->vol = axv->vol << 8;
    axv->svol = axv->svol << 8;
    axv->vdown_src_vol = axv->vol;
    axv->vdown_src_svol = axv->svol;
    if ((Snd_ctrl_work.se_state & 0x2) && !(axv->flag & 0x2)) {
        axv->status |= 0x10;
        Snd_axv_work_calc_vdown_vol(axv);
    }
    Snd_axv_work_choice_now_vol(axv);
    Snd_axv_work_calc_ax_vol(axv);
}

void iss_ax_set_pan(SND_AXV_WORK* axv, SND_REQ_WORK* req, SND_SIT* sit)
{
    if (req->x1B >= 0) {
        axv->pan = req->x1B;
    } else if (sit->pan >= 0) {
        axv->pan = sit->pan;
    } else {
        axv->pan = axv->art->pan;
    }
    if (req->x1C >= 0) {
        axv->span = req->x1C;
    } else if (sit->span >= 0) {
        axv->span = sit->span;
    } else {
        axv->span = 0x7F;
    }
    Snd_axv_work_choice_out_span(axv);
}

void iss_ax_set_aux(SND_AXV_WORK* axv, SND_REQ_WORK* req, SND_SIT* sit)
{
    if (req->x1F >= 0) {
        axv->auxA = req->x1F;
    } else if (sit->x7 >= 0) {
        axv->auxA = sit->x7;
    } else {
        axv->auxA = 0;
    }
    if (req->x20 >= 0) {
        axv->auxB = req->x20;
    } else if (sit->x8 >= 0) {
        axv->auxB = sit->x8;
    } else {
        axv->auxB = 0;
    }
    axv->ax_auxA = Snd_vol_syn_to_ax(axv->auxA);
    axv->ax_auxB = Snd_vol_syn_to_ax(axv->auxB);
}

void iss_ax_set_pitch(SND_AXV_WORK* axv, SND_REQ_WORK* req, SND_SIT* sit)
{
    WTREGION* rgn;
    int cents;

    rgn = axv->rgn;
    axv->rate = (f64) axv->sample->sampleRate;
    cents = (u16) (sit->prog & 0xFF) - rgn->unityNote;
    cents *= 100;
    cents += rgn->fineTune;
    cents += req->pitch;
    cents += req->x22;
    axv->pitch_base = cents;
    axv->pitch_ofs = req->x24;
    axv->pitch = axv->pitch_base + axv->pitch_ofs;
}

void iss_ax_set_lpf(SND_AXV_WORK* axv, SND_REQ_WORK* req)
{
    if (req->x21 == -1) {
        axv->lpf_on = 0;
    } else {
        axv->lpf_on = 1;
    }
    axv->lpf_no = req->x21;
}

void iss_ax_set_para(SND_AXV_WORK* axv, SND_REQ_WORK* req)
{
    WTREGION* rgn;
    WTSAMPLE* sample;
    WTADPCM* adpcm;
    AXPBADDR addr;
    AXPBSRC src;
    AXPBADPCM ax_adpcm;
    AXPBADPCMLOOP loop;
    AXPBLPF lpf;
    u32 cur;
    u32 loop_addr;
    u32 end_addr;
    u32 loop_end;
    u32 ratio;
    f64 ratio_f;

    rgn = axv->rgn;
    sample = axv->sample;
    adpcm = axv->adpcm;
    cur = axv->aram * 2 + sample->offset;
    cur += 2;
    if (rgn->loopLength == 0) {
        loop_addr = Snd_ctrl_work.aram_base * 2 + 2;
        end_addr = cur;
        end_addr += sample->length / 14 * 16 + sample->length % 14;
        addr.loopFlag = 0;
        addr.format = 0;
    } else {
        loop_addr = cur;
        loop_addr += rgn->loopStart / 14 * 16 + rgn->loopStart % 14;
        loop_end = rgn->loopStart + rgn->loopLength;
        end_addr = cur;
        end_addr += loop_end / 14 * 16 + loop_end % 14;
        addr.loopFlag = 1;
        addr.format = 0;
        loop.loop_pred_scale = adpcm->loop_pred_scale;
        loop.loop_yn1 = adpcm->loop_yn1;
        loop.loop_yn2 = adpcm->loop_yn2;
        AXSetVoiceAdpcmLoop(axv->voice, &loop);
    }
    addr.loopAddressHi = loop_addr >> 16;
    addr.loopAddressLo = loop_addr;
    addr.endAddressHi = end_addr >> 16;
    addr.endAddressLo = end_addr;
    addr.currentAddressHi = cur >> 16;
    addr.currentAddressLo = cur;
    ax_adpcm.a[0][0] = adpcm->a[0][0];
    ax_adpcm.a[1][0] = adpcm->a[1][0];
    ax_adpcm.a[2][0] = adpcm->a[2][0];
    ax_adpcm.a[3][0] = adpcm->a[3][0];
    ax_adpcm.a[4][0] = adpcm->a[4][0];
    ax_adpcm.a[5][0] = adpcm->a[5][0];
    ax_adpcm.a[6][0] = adpcm->a[6][0];
    ax_adpcm.a[7][0] = adpcm->a[7][0];
    ax_adpcm.a[0][1] = adpcm->a[0][1];
    ax_adpcm.a[1][1] = adpcm->a[1][1];
    ax_adpcm.a[2][1] = adpcm->a[2][1];
    ax_adpcm.a[3][1] = adpcm->a[3][1];
    ax_adpcm.a[4][1] = adpcm->a[4][1];
    ax_adpcm.a[5][1] = adpcm->a[5][1];
    ax_adpcm.a[6][1] = adpcm->a[6][1];
    ax_adpcm.a[7][1] = adpcm->a[7][1];
    ax_adpcm.gain = adpcm->gain;
    ax_adpcm.pred_scale = adpcm->pred_scale;
    ax_adpcm.yn1 = adpcm->yn1;
    ax_adpcm.yn2 = adpcm->yn2;
    ratio_f = pow(2.0, (f64) axv->pitch / 1200.0);
    ratio_f = axv->rate * (f32) ratio_f / 32000.0f;
    ratio = (u32) (ratio_f * 65536.0);
    if (ratio > 0x40000) {
        OSReport("SND Sample Rate Over.\n");
        OSReport("BLK_NO : %d / REQ_NO : %d\n", req->blk_no, req->req_no);
        ratio = 0x40000;
    }
    src.ratioHi = ratio >> 16;
    src.ratioLo = ratio;
    src.currentAddressFrac = 0;
    src.last_samples[0] = 0;
    src.last_samples[1] = 0;
    src.last_samples[2] = 0;
    src.last_samples[3] = 0;
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
    AXSetVoiceAddr(axv->voice, &addr);
    AXSetVoiceAdpcm(axv->voice, &ax_adpcm);
    AXSetVoiceLpf(axv->voice, &lpf);
    AXSetVoiceSrc(axv->voice, &src);
    AXSetVoiceSrcType(axv->voice, 1);
}

void cb_drop_voice(void* voice)
{
    AXVPB* axvpb;
    SND_AXV_WORK* axv;
    SND_VOICE_WORK* vw;
    u32 i;
    int old;

    axvpb = (AXVPB*) voice;
    OSReport("ISS Voice Drop 0x%08x\n", voice);
    old = OSDisableInterrupts();
    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (axvpb != axv->voice) {
            continue;
        }
        MIXReleaseChannel(axv->voice);
        vw = axv->vw;
        if (vw != NULL) {
            vw->status = 0;
            vw->axv = NULL;
        }
        axv->status = 0;
        axv->vw = NULL;
        axv->voice = NULL;
        break;
    }
    OSRestoreInterrupts(old);
}
