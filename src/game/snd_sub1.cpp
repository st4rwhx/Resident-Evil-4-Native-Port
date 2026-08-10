#include "snd_drv.h"

void Snd_voice_work_clear(void)
{
    SND_VOICE_WORK* voice;
    u32 i;
    u32 j;
    u8* p;

    for (i = 0; i < SND_VOICE_MAX; i++) {
        voice = &Snd_voice_work[i];
        p = (u8*) voice;
        for (j = 0; j < sizeof(SND_VOICE_WORK); j++) {
            *p++ = 0;
        }
        voice->no = i;
        voice->out_mode = 0;
        voice->type = 0;
        voice->x8 = 0;
        voice->seq_no = -1;
        voice->seq_ch = -1;
        voice->seq_note = -1;
    }
}

void Snd_stop_voice_work(SND_VOICE_WORK* voice)
{
    SND_SEQ_WORK* seq;

    if (voice->type == 2) {
        seq = &Snd_seq_work[voice->seq_no];
        Snd_send_midi(&seq->synth, voice->seq_ch | (s8) 0x90, voice->seq_note, 0);
        voice->status = 0;
        voice->axv = NULL;
    } else {
        Snd_axv_work_note_off(voice->axv, voice->rel_time);
    }
}

SND_VOICE_WORK* Snd_open_voice_work_str(SND_STR_WORK* str, s8 no)
{
    SND_VOICE_WORK* voice;

    voice = &Snd_voice_work[no];
    if (voice->status != 0) {
        return NULL;
    }
    voice->snd_id = str->snd_id;
    if (str->type & 0x2) {
        voice->out_mode = 2;
    } else {
        voice->out_mode = 1;
    }
    voice->type = 3;
    voice->x8 = 0;
    voice->count = 0;
    voice->axv = NULL;
    voice->blk_no = -1;
    voice->req_no = -1;
    voice->prio = 0x7F;
    voice->status = 1;
    return voice;
}

SND_VOICE_WORK* Snd_open_voice_work_seq(SND_SEQ_WORK* seq, s8 prio)
{
    SND_VOICE_WORK* voice;

    voice = Snd_voice_work_open_ck(seq->sit, prio);
    if (voice == NULL) {
        return NULL;
    }
    voice->snd_id = seq->snd_id;
    if (seq->type == 2) {
        voice->out_mode = 2;
    } else {
        voice->out_mode = 1;
    }
    voice->type = 2;
    voice->x8 = 0;
    voice->count = 0;
    voice->axv = NULL;
    voice->blk_no = -1;
    voice->req_no = -1;
    voice->status = 1;
    return voice;
}

SND_VOICE_WORK* Snd_voice_work_open_ck(SND_SIT* info, s8 prio)
{
    SND_VOICE_WORK* voice;
    SND_VOICE_WORK* found;
    int i;
    u32 count;
    s8 min_prio;

    for (i = 0; i <= info->voice_num; i++) {
        voice = &Snd_voice_work[info->voice_start + i];
        if (voice->status == 0) {
            return voice;
        }
    }
    min_prio = 0x7F;
    for (i = 0; i <= info->voice_num; i++) {
        voice = &Snd_voice_work[info->voice_start + i];
        if (min_prio >= voice->prio) {
            min_prio = voice->prio;
        }
    }
    if (min_prio > prio) {
        return NULL;
    }
    if (min_prio == prio && !(info->flag & 0x4000)) {
        return NULL;
    }
    found = NULL;
    count = 0;
    for (i = 0; i <= info->voice_num; i++) {
        voice = &Snd_voice_work[info->voice_start + i];
        if (min_prio != voice->prio) {
            continue;
        }
        if (count <= voice->count) {
            found = voice;
            count = voice->count;
        }
    }
    if (found == NULL) {
        return NULL;
    }
    if (info->flag & 0x4) {
        if (found->type != 2) {
            return NULL;
        }
    } else {
        if (found->type != 1) {
            return NULL;
        }
    }
    if (found->status & 0x2) {
        return NULL;
    }
    Snd_stop_voice_work(found);
    return found;
}

SND_VOICE_WORK* Snd_search_voice_work_snd_id(u32 snd_id)
{
    SND_VOICE_WORK* voice;
    int i;

    for (i = 0; i < SND_VOICE_MAX; i++) {
        voice = &Snd_voice_work[i];
        if (voice->status == 0) {
            continue;
        }
        if (voice->type != 1) {
            continue;
        }
        if (voice->snd_id == snd_id) {
            return voice;
        }
    }
    return NULL;
}

SND_VOICE_WORK* Snd_search_voice_work_seq(SND_SEQ_WORK* seq, u8 ch, u8 note)
{
    SND_VOICE_WORK* voice;
    int i;

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
        if (voice->seq_ch != ch) {
            continue;
        }
        if (voice->seq_note != note) {
            continue;
        }
        return voice;
    }
    return NULL;
}
