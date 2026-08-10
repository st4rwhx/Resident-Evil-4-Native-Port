#include "snd_drv.h"

void Snd_midi_sequencer(void)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
    SND_SEQ_WORK* seq;
    u32 i;
    u32 t0;
    u32 t1;

    if (ctrl->reset_flag & 0x1) {
        ctrl->reset_flag |= 0x40;
    }
    t0 = ctrl->seq_tick / 1000;
    ctrl->seq_tick += 4995;
    t1 = ctrl->seq_tick / 1000;
    ctrl->seq_msec = t1 - t0;
    if (ctrl->seq_tick == 999000) {
        ctrl->seq_tick = 0;
    }
    for (i = 0; i < SND_SEQ_MAX; i++) {
        seq = &Snd_seq_work[i];
        if (seq->status & 0x10) {
            seq_player(ctrl, seq);
        }
    }
}

void seq_player(SND_CTRL_WORK* ctrl, SND_SEQ_WORK* seq)
{
    u32 i;

    if (seq_reset_check(seq) != 0) {
        return;
    }
    seq_tpr_check(seq);
    seq_req_check(seq);
    if (seq_fade_check(seq) != 0) {
        return;
    }
    for (i = 0; i < ctrl->seq_msec; i++) {
        seq_one_msec(ctrl, seq);
    }
    seq_play_update(seq);
}

int seq_reset_check(SND_SEQ_WORK* seq)
{
    if ((Snd_ctrl_work.reset_flag & 0x40) == 0) {
        return 0;
    }
    seq_play_end(seq);
    return 1;
}

void seq_tpr_check(SND_SEQ_WORK* seq)
{
    int i;
    u16 mask;
    u16 ch;
    u8 status;
    u8 cc;
    u8 val;

    if (seq->tpr_num == 0) {
        return;
    }
    for (i = 0; i < seq->tpr_num; i++) {
        mask = seq->tpr_mask[i];
        if (seq->tpr_kind[i] == 8) {
            cc = 7;
        } else {
            cc = 10;
        }
        val = seq->tpr_val[i];
        for (ch = 0; ch < 16; ch++) {
            if (mask & 0x1) {
                if (cc == 7) {
                    seq->ch_vol[ch] = val;
                } else {
                    seq->ch_pan[ch] = val;
                }
                status = (u8) ch | 0xB0;
                SYNMidiInput(&seq->synth, &status);
            }
            mask >>= 1;
        }
    }
    seq->tpr_num = 0;
}

void seq_req_check(SND_SEQ_WORK* seq)
{
    if (seq->req == 0) {
        return;
    }
    if (seq->req & 0x4) {
        seq->req &= ~0x4;
        seq_req_vol_set(seq);
        return;
    }
    if (seq->req & 0x1) {
        seq->req &= ~0x1;
        seq_req_fade_set(seq, seq->fade_time, seq->fade_vol);
        return;
    }
    if (seq->req & 0x2) {
        seq->req &= ~0x2;
        seq_req_fade_set(seq, 50, 0);
    }
}

void seq_req_vol_set(SND_SEQ_WORK* seq)
{
    seq->vol2 = seq->vol << 8;
    seq->flag |= 0x1;
}

void seq_req_fade_set(SND_SEQ_WORK* seq, s16 time, s16 vol)
{
    s16 diff;

    seq->fade_target = vol << 8;
    diff = seq->fade_target - seq->vol2;
    seq->fade_step = diff / time;
    if (seq->fade_step == 0) {
        if (diff > 0) {
            seq->fade_step = 1;
        } else {
            seq->fade_step = -1;
        }
    }
    seq->status |= 0x100;
}

int seq_fade_check(SND_SEQ_WORK* seq)
{
    if (seq->status & 0x100) {
        if (seq->vol2 == seq->fade_target) {
            seq->status &= ~0x100;
            if (seq->vol2 == 0) {
                seq_play_end(seq);
                return 1;
            }
        } else {
            seq_fade_new_vol_set(seq, seq->fade_step, seq->fade_target);
        }
    }
    return 0;
}

void seq_fade_new_vol_set(SND_SEQ_WORK* seq, s16 step, s16 target)
{
    int v;

    v = seq->vol2 + step;
    if (step > 0) {
        if (v > target) {
            v = target;
        }
    } else {
        if (v < target) {
            v = target;
        }
    }
    seq->vol2 = v;
    seq->flag |= 0x1;
}

void seq_one_msec(SND_CTRL_WORK* ctrl, SND_SEQ_WORK* seq)
{
    while (1) {
        if (seq->delta == 0) {
            seq_one_msec_main(ctrl, seq);
            if ((seq->status & 0x10) == 0) {
                return;
            }
            seq->delta = Snd_seq_get_delta(seq) * seq->tempo;
        } else {
            seq->delta -= seq->x3180;
            if (seq->delta > 0) {
                break;
            }
            seq->delta = 0;
        }
    }
}

void seq_one_msec_main(SND_CTRL_WORK* ctrl, SND_SEQ_WORK* seq)
{
    u8* p;

    p = seq->seq_pos;
    ctrl->midi_msg[0] = *p++;
    ctrl->midi_msg[1] = *p++;
    ctrl->midi_msg[2] = *p++;
    ctrl->midi_type = ctrl->midi_msg[0] & 0xF0;
    ctrl->midi_ch = ctrl->midi_msg[0] & 0x0F;
    Snd_seq_midi_message(ctrl, seq);
}

void seq_play_end(SND_SEQ_WORK* seq)
{
    SND_VOICE_WORK* voice;
    int i;

    if (seq->vol2 != 0) {
        seq->vol2 = 0;
        seq->flag |= 0x1;
    }
    for (i = 0; i < SND_VOICE_MAX; i++) {
        voice = &Snd_voice_work[i];
        if (voice->status == 0) {
            continue;
        }
        if (voice->type != 2) {
            continue;
        }
        if ((s16) voice->seq_no != seq->no) {
            continue;
        }
        Snd_send_midi(&seq->synth, voice->seq_ch | (s8) 0x90, voice->seq_note, 0);
        voice->status = 0;
    }
    seq->status &= ~0x10;
    seq->flag = 0;
}

void seq_play_update(SND_SEQ_WORK* seq)
{
    if (seq->flag & 0x1) {
        Snd_seq_work_calc_ax_vol(seq);
        SYNSetMasterVolume(&seq->synth, seq->ax_vol);
    }
    seq->flag = 0;
}
