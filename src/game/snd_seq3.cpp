#include "snd_drv.h"

void Snd_seq_work_clear(void)
{
    SND_SEQ_WORK* seq;
    u32 i;
    u32 j;
    u8* p;

    for (i = 0; i < SND_SEQ_MAX; i++) {
        seq = &Snd_seq_work[i];
        p = (u8*) seq;
        for (j = 0; j < sizeof(SND_SEQ_WORK); j++) {
            *p++ = 0;
        }
        seq->no = i;
    }
}

SND_SEQ_WORK* Snd_search_seq_work_snd_id(u32 snd_id)
{
    SND_SEQ_WORK* seq;
    int i;

    if (snd_id == 0) {
        return NULL;
    }
    for (i = 0; i < SND_SEQ_MAX; i++) {
        seq = &Snd_seq_work[i];
        if (seq->status == 0) {
            continue;
        }
        if (seq->snd_id != snd_id) {
            continue;
        }
        return seq;
    }
    return NULL;
}

void Snd_seq_work_close_check(void)
{
    SND_SEQ_WORK* seq;
    int i;

    for (i = 0; i < SND_SEQ_MAX; i++) {
        seq = &Snd_seq_work[i];
        if (seq->status == 0) {
            continue;
        }
        if (seq->status & 0x10) {
            continue;
        }
        if (SYNGetActiveNotes(&seq->synth) != 0) {
            continue;
        }
        SYNQuitSynth(&seq->synth);
        seq->status = 0;
    }
}

u32 Snd_seq_get_delta(SND_SEQ_WORK* seq)
{
    u32 d;
    u32 c;

    d = *seq->seq_pos++;
    if (d & 0x80) {
        c = *seq->seq_pos++;
        d = ((d & 0x7F) << 7) | (c & 0x7F);
        if (c & 0x80) {
            c = *seq->seq_pos++;
            d = (d << 7) | (c & 0x7F);
            if (c & 0x80) {
                c = *seq->seq_pos++;
                d = (d << 7) | (c & 0x7F);
            }
        }
    }
    return d;
}

void Snd_seq_send_midi(SND_CTRL_WORK* msg, SND_SEQ_WORK* seq)
{
    int old;

    old = OSDisableInterrupts();
    SYNMidiInput(&seq->synth, msg->midi_msg);
    OSRestoreInterrupts(old);
}

void Snd_send_midi(SYNSYNTH* synth, u8 status, u8 data1, u8 data2)
{
    u8 msg[3];
    int old;

    msg[0] = status;
    msg[1] = data1;
    msg[2] = data2;
    old = OSDisableInterrupts();
    SYNMidiInput(synth, msg);
    OSRestoreInterrupts(old);
}

void Snd_seq_work_calc_ax_vol(SND_SEQ_WORK* seq)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;

    if (seq->type == 2) {
        seq->calc_vol = ctrl->sys_vol[1] / 127 * (ctrl->sys_vol[3] >> 8);
    } else {
        seq->calc_vol = ctrl->sys_vol[0] / 127 * (ctrl->sys_vol[2] >> 8);
    }
    seq->calc_vol = seq->calc_vol / 127 * (seq->vol2 >> 8);
    seq->ax_vol = Snd_vol_syn_to_ax((s16) (seq->calc_vol >> 8));
}

void Snd_iss_new_seq_work(SND_ISS_BLK* blk, SND_SIT* sit, SND_REQ_WORK* req)
{
    SND_SEQ_WORK* seq;
    u8* tbl;
    u32 ofs;
    u32 bank;

    seq = open_seq_work();
    if (seq == NULL) {
        return;
    }
    seq->status = 0x11;
    seq->snd_id = req->snd_id;
    seq->type = req->type;
    seq_work_init_track(seq);
    seq->aram = blk->aram;
    seq->wt = blk->dls;
    seq->sit = sit;
    SYNInitSynth(&seq->synth, seq->wt, seq->aram, Snd_ctrl_work.aram_base, 30, 30, 1);
    bank = (u16) ((u16) (sit->prog >> 8) & 0xFF);
    tbl = blk->seq;
    ofs = ((u32*) tbl)[bank + 1];
    seq->seq_top = blk->seq + ofs;
    seq->seq_pos = seq->seq_top;
    seq->seq_loop = seq->seq_top;
    seq->delta = Snd_seq_get_delta(seq);
    if (req->x1D >= 0) {
        seq->vol2 = req->x1D << 8;
    } else {
        seq->vol2 = sit->vol << 8;
    }
    seq->flag |= 0x1;
    seq->status |= 0x10;
}

SND_SEQ_WORK* open_seq_work(void)
{
    SND_SEQ_WORK* seq;
    int i;

    for (i = 0; i < SND_SEQ_MAX; i++) {
        seq = &Snd_seq_work[i];
        if (seq->status != 0) {
            continue;
        }
        return seq;
    }
    OSReport("Snd_seq_work is full.\n");
    return NULL;
}

void seq_work_init_track(SND_SEQ_WORK* seq)
{
    u32 i;

    seq->flag = 0;
    seq->req = 0;
    seq->vol = 0;
    seq->calc_vol = 0;
    seq->vol2 = 0;
    seq->fade_time = 0;
    seq->fade_vol = 0;
    seq->fade_step = 0;
    seq->fade_target = 0;
    seq->tempo = 1000;
    seq->x3180 = 480;
    seq->delta = 0;
    seq->tpr_num = 0;
    for (i = 0; i < 16; i++) {
        seq->ch_flag[i] = 0;
        seq->ch_prio[i] = 0;
        seq->ch_prog[i] = -1;
        seq->ch_vol[i] = 0;
        seq->ch_exp[i] = 0;
        seq->ch_pan[i] = -1;
        seq->ch_pitch_lo[i] = 0;
        seq->ch_pitch_hi[i] = 0;
        seq->ch_data_msb[i] = -1;
        seq->ch_data_lsb[i] = -1;
        seq->ch_mod[i] = 0;
        seq->ch_hold[i] = 0;
        seq->ch_reverb[i] = 0;
        seq->ch_chorus[i] = 0;
    }
}
