#include "snd_drv.h"

int Snd_str_ax_voice_init(SND_STR_WORK* str, s8 start, s8 num)
{
    int ret;

    ret = str_secure_voice_work(str, start, num);
    if (ret == 1) {
        return 1;
    }
    str->cur_L = str->aram_L_nbl + 2;
    str->cur_R = str->aram_R_nbl + 2;
    if (str->shortflag & 0x1) {
        str_ax_adrs_set_short(str);
    } else {
        str_ax_adrs_set_long(str);
    }
    str->play_nbl = str->cur_L - str->aram_L_nbl;
    str_ax_voice_para_set(str);
    return 0;
}

void str_ax_adrs_set_long(SND_STR_WORK* str)
{
    u32 len;

    str->loop_L = str->cur_L;
    str->loop_R = str->cur_R;
    if (str->flag & 0x1) {
        len = 0x3FFFF;
    } else {
        len = 0x7FFFF;
    }
    str->end_L = str->aram_L_nbl + len;
    str->end_R = str->aram_R_nbl + len;
}

void str_ax_adrs_set_short(SND_STR_WORK* str)
{
    u32 zero;

    zero = Snd_ctrl_work.aram_base * 2 + 2;
    if (str->shortflag & 0x2) {
        str->loop_L = str->aram_L_nbl + str->loop_start;
        str->loop_R = str->aram_R_nbl + str->loop_start;
    } else {
        str->loop_L = zero;
        str->loop_R = zero;
    }
    str->end_L = str->aram_L_nbl + str->loop_end;
    str->end_R = str->aram_R_nbl + str->loop_end;
}

void str_ax_voice_para_set(SND_STR_WORK* str)
{
    SND_SHD* shd;
    AXPBADDR addr;
    AXPBSRC src;
    AXPBADPCM adpcm;
    AXPBLPF lpf;
    u32 i;
    u32 loop;
    u32 ratio;
    u32 srctype;
    f32 f;

    shd = str->shd;
    f = str->rate / 32000.0f;
    ratio = (u32) (f * 65536.0f);
    if (str->rate == 32000.0f) {
        srctype = 0;
    } else {
        srctype = 1;
    }
    src.ratioHi = ratio >> 16;
    src.ratioLo = ratio;
    src.currentAddressFrac = 0;
    src.last_samples[0] = 0;
    src.last_samples[1] = 0;
    src.last_samples[2] = 0;
    src.last_samples[3] = 0;
    lpf.on = 0;
    lpf.yn1 = 0;
    lpf.a0 = 0;
    lpf.b0 = 0;
    if (str->shortflag & 0x4) {
        loop = 0;
    } else {
        loop = 1;
    }
    addr.loopFlag = loop;
    addr.format = 0;
    addr.loopAddressHi = str->loop_L >> 16;
    addr.loopAddressLo = str->loop_L;
    addr.endAddressHi = str->end_L >> 16;
    addr.endAddressLo = str->end_L;
    addr.currentAddressHi = str->cur_L >> 16;
    addr.currentAddressLo = str->cur_L;
    for (i = 0; i < 8; i++) {
        adpcm.a[i][0] = shd->coefL[i];
        adpcm.a[i][1] = shd->coefL[i + 8];
    }
    adpcm.gain = shd->gain[0];
    adpcm.pred_scale = shd->pred_scale[0];
    adpcm.yn1 = shd->yn1[0];
    adpcm.yn2 = shd->yn2[0];
    if (loop == 1) {
        AXSetVoiceType(str->voiceL, 1);
    } else {
        AXSetVoiceType(str->voiceL, 0);
    }
    AXSetVoiceAddr(str->voiceL, &addr);
    AXSetVoiceAdpcm(str->voiceL, &adpcm);
    AXSetVoiceLpf(str->voiceL, &lpf);
    AXSetVoiceSrc(str->voiceL, &src);
    AXSetVoiceSrcType(str->voiceL, srctype);
    if (str->flag & 0x2) {
        return;
    }
    addr.loopFlag = loop;
    addr.format = 0;
    addr.loopAddressHi = str->loop_R >> 16;
    addr.loopAddressLo = str->loop_R;
    addr.endAddressHi = str->end_R >> 16;
    addr.endAddressLo = str->end_R;
    addr.currentAddressHi = str->cur_R >> 16;
    addr.currentAddressLo = str->cur_R;
    for (i = 0; i < 8; i++) {
        adpcm.a[i][0] = shd->coefR[i];
        adpcm.a[i][1] = shd->coefR[i + 8];
    }
    adpcm.gain = shd->gain[1];
    adpcm.pred_scale = shd->pred_scale[1];
    adpcm.yn1 = shd->yn1[1];
    adpcm.yn2 = shd->yn2[1];
    if (loop == 1) {
        AXSetVoiceType(str->voiceR, 1);
    } else {
        AXSetVoiceType(str->voiceR, 0);
    }
    AXSetVoiceAddr(str->voiceR, &addr);
    AXSetVoiceAdpcm(str->voiceR, &adpcm);
    AXSetVoiceLpf(str->voiceR, &lpf);
    AXSetVoiceSrc(str->voiceR, &src);
    AXSetVoiceSrcType(str->voiceR, srctype);
}

void cb_str_voice_drop(void* voice)
{
    int old;
    AXVPB* axvpb;
    SND_STR_WORK* str;
    SND_VOICE_WORK* vw;
    u32 i;

    axvpb = (AXVPB*) voice;
    vw = NULL;
    OSReport("Str Voice Drop 0x%08x\n", voice);
    old = OSDisableInterrupts() ? TRUE : FALSE;
    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        if (str->status != 0) {
            str->status |= 0x4000;
            if (str->voiceL == axvpb) {
                str->status |= 0x1000;
                MIXReleaseChannel(axvpb);
                vw = str->vwL;
                if (vw != NULL) {
                    vw->status = 0;
                    vw->axv = NULL;
                }
            } else if (str->voiceR == axvpb) {
                str->status |= 0x2000;
                MIXReleaseChannel(axvpb);
                vw = str->vwR;
                if (vw != NULL) {
                    vw->status = 0;
                    vw->axv = NULL;
                }
            }
        }
    }
    OSRestoreInterrupts(old);
}

int str_secure_voice_work(SND_STR_WORK* str, s8 start, s8 num)
{
    SND_VOICE_WORK* vw;
    int i;

    vw = NULL;
    for (i = 0; i <= num; i++) {
        vw = Snd_open_voice_work_str(str, start + (s8) i);
        if (vw != NULL) {
            break;
        }
    }
    if (vw == NULL) {
        OSReport("STR SND_VOICE L is not available\n");
        return 1;
    }
    str->vwL = vw;
    str->voiceL = AXAcquireVoice(31, cb_str_voice_drop, 0);
    if (str->voiceL == NULL) {
        OSReport("STR AX_VOICE_L is not available\n");
        return 1;
    }
    if (str->flag & 0x2) {
        return 0;
    }
    for (i = 0; i <= num; i++) {
        vw = Snd_open_voice_work_str(str, start + (s8) i);
        if (vw != NULL) {
            break;
        }
    }
    if (vw == NULL) {
        OSReport("STR SND_VOICE R is not available\n");
        return 1;
    }
    str->vwR = vw;
    str->voiceR = AXAcquireVoice(31, cb_str_voice_drop, 0);
    if (str->voiceR == NULL) {
        OSReport("STR AX_VOICE_R is not available\n");
        return 1;
    }
    return 0;
}

void Snd_str_ax_voice_play(SND_STR_WORK* str)
{
    if (str->fade_time == 0) {
        str->vol2 = str->vol << 8;
    } else {
        str->vol2 = 0;
        str->fade_target = str->fade_vol << 8;
        str->fade_step = str->fade_target / str->fade_time;
        str->status |= 0x100;
    }
    Snd_str_work_calc_ax_vol(str);
    str->play_span = str->span;
    Snd_str_work_choice_out_span(str);
    str->ax_auxA = Snd_vol_syn_to_ax(str->auxA);
    str->ax_auxB = Snd_vol_syn_to_ax(str->auxB);
    if (str->flag & 0x1) {
        MIXInitChannel(str->voiceL, 0, str->ax_vol, str->ax_auxA, str->ax_auxB, 0, str->out_span, 0);
        MIXInitChannel(str->voiceR, 0, str->ax_vol, str->ax_auxA, str->ax_auxB, 0x7F, str->out_span, 0);
        AXSetVoiceState(str->voiceL, 1);
        AXSetVoiceState(str->voiceR, 1);
    } else {
        MIXInitChannel(str->voiceL, 0, str->ax_vol, str->ax_auxA, str->ax_auxB, str->pan, str->out_span, 0);
        AXSetVoiceState(str->voiceL, 1);
    }
}

void Snd_str_ax_voice_stop(SND_STR_WORK* str)
{
    str->vol2 = 0;
    Snd_str_work_calc_ax_vol(str);
    if (str->voiceL->pb.state != 0) {
        MIXSetInput(str->voiceL, str->ax_vol);
        AXSetVoiceState(str->voiceL, 0);
    }
    if (str->flag & 0x2) {
        return;
    }
    if (str->voiceR->pb.state != 0) {
        MIXSetInput(str->voiceR, str->ax_vol);
        AXSetVoiceState(str->voiceR, 0);
    }
}

void Snd_str_ax_voice_recovery(SND_STR_WORK* str)
{
    str->status |= 0x200;
    Snd_str_work_calc_ax_vol(str);
    MIXSetInput(str->voiceL, str->ax_vol);
    AXSetVoiceState(str->voiceL, 1);
    if (str->flag & 0x2) {
        return;
    }
    MIXSetInput(str->voiceR, str->ax_vol);
    AXSetVoiceState(str->voiceR, 1);
}

void Snd_str_ax_voice_free(SND_STR_WORK* str)
{
    SND_VOICE_WORK* vw;

    if (str->status & 0x20) {
        MIXReleaseChannel(str->voiceL);
    }
    AXFreeVoice(str->voiceL);
    str->voiceL = NULL;
    vw = str->vwL;
    vw->status = 0;
    if (str->flag & 0x2) {
        return;
    }
    if (str->status & 0x20) {
        MIXReleaseChannel(str->voiceR);
    }
    AXFreeVoice(str->voiceR);
    str->voiceR = NULL;
    vw = str->vwR;
    vw->status = 0;
}
