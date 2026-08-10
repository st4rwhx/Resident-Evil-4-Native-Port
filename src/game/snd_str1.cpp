#include "snd_drv.h"

typedef void (*SND_STR_PLAYER)(SND_STR_WORK*);

void Snd_stream_player(void)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
    SND_STR_WORK* str;
    u32 i;
    static SND_STR_PLAYER str_player_tbl[] = {
        str_player_idle,   str_player_ready, str_player_normal, str_player_noread, str_player_close,
        str_player_idle,   str_player_error, str_abort_init,    str_abort_wait,
    };

    if (ctrl->dvd_err != -1) {
        ctrl->dvd_err = 0;
    }
    if (ctrl->reset_flag & 0x1) {
        ctrl->reset_flag |= 0x20;
    }
    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        if (str->status != 0) {
            if (str->status & 0x3000) {
                if (str->status & 0x4000) {
                    str->state = 7;
                    str->status &= ~0x4000;
                }
                str_player_tbl[str->state](str);
            } else {
                Snd_str_get_dvd_status(str);
                str_req_check(str);
                str_player_tbl[str->state](str);
                Snd_str_player_update(str);
            }
        }
    }
}

void str_player_idle(SND_STR_WORK* str)
{
    if (str->state != 0) {
        return;
    }
    str_reset_check(str);
}

void str_player_ready(SND_STR_WORK* str)
{
    if (str_reset_check(str) != 0) {
        return;
    }
    if (str->status & 0x8000) {
        if (str->cancel == 1) {
            str_play_cancel(str);
            return;
        }
        str_recovery_check(str);
    }
    if (str->req & 0x8) {
        str_play_cancel(str);
        return;
    }
    Snd_str_dvd_read_sub(str);
    Snd_str_aram_dma_sub(str);
}

void str_player_normal(SND_STR_WORK* str)
{
    if (str_reset_check(str) != 0) {
        return;
    }
    if (str_fade_check(str) != 0) {
        return;
    }
    Snd_str_get_now_play_nbl(str);
    if (str->status & 0x8000) {
        if (str->cancel == 1) {
            str_play_cancel(str);
        } else {
            Snd_str_err_check(str);
        }
        return;
    }
    Snd_str_dvd_read_sub(str);
    Snd_str_aram_dma_sub(str);
}

void str_player_noread(SND_STR_WORK* str)
{
    if (str_reset_check(str) != 0) {
        return;
    }
    if (str_fade_check(str) != 0) {
        return;
    }
    if (str->voiceL->pb.state == 0) {
        str_play_end(str);
        return;
    }
    Snd_str_get_now_play_nbl(str);
}

void str_player_close(SND_STR_WORK* str)
{
    if (str->dvd_busy != 0) {
        return;
    }
    if (str->dma_busy != 0) {
        return;
    }
    DVDClose(&str->dvd);
    Snd_str_ax_voice_free(str);
    str->state = 5;
    str->upd = 0;
}

void str_player_error(SND_STR_WORK* str)
{
    if (str_reset_check(str) != 0) {
        return;
    }
    str_recovery_check(str);
    if (!(str->status & 0x8000)) {
        if (str->cancel == 2) {
            str_play_cancel(str);
            return;
        }
        str->state = str->prev_state;
        Snd_str_ax_voice_recovery(str);
        Snd_str_get_now_play_nbl(str);
    }
    Snd_str_dvd_read_sub(str);
    Snd_str_aram_dma_sub(str);
}

void str_abort_init(SND_STR_WORK* str)
{
    SND_VOICE_WORK* vw;

    str->state++;
    if (str->dvd_busy != 0) {
        DVDCancelAsync(&str->dvd.cb, NULL);
    }
    if (!(str->status & 0x1000)) {
        if (str->status & 0x20) {
            MIXReleaseChannel(str->voiceL);
        }
        AXFreeVoice(str->voiceL);
        str->voiceL = NULL;
        vw = str->vwL;
        vw->status = 0;
    }
    if (str->flag & 0x2) {
        return;
    }
    if (!(str->status & 0x2000)) {
        if (str->status & 0x20) {
            MIXReleaseChannel(str->voiceR);
        }
        AXFreeVoice(str->voiceR);
        str->voiceR = NULL;
        vw = str->vwR;
        vw->status = 0;
    }
}

void str_abort_wait(SND_STR_WORK* str)
{
    if (str->dvd_busy != 0) {
        return;
    }
    if (str->dma_busy != 0) {
        return;
    }
    DVDClose(&str->dvd);
    str->status = 0;
    str->snd_id = 0;
    str->state = 0;
    str->prev_state = 0;
    str->upd = 0;
    str->vwL = NULL;
    str->vwR = NULL;
    str->voiceL = NULL;
    str->voiceR = NULL;
}

void str_req_check(SND_STR_WORK* str)
{
    if (Snd_ctrl_work.reset_flag & 0x20) {
        return;
    }
    if (str->status & 0x8000) {
        return;
    }
    if (str->req == 0) {
        return;
    }
    if (str->req & 0x1) {
        str_req_to_ready(str);
        return;
    }
    if ((str->status & 0x2) == 0) {
        return;
    }
    if (str->req & 0x2) {
        str_req_to_play(str);
        return;
    }
    if ((str->status & 0x10) == 0) {
        return;
    }
    if (str->req & 0x10) {
        str->req &= ~0x10;
        str_req_vol_set(str);
        return;
    }
    if (str->req & 0x4) {
        str->req &= ~0x4;
        str_req_nml_fade_set(str, str->fade_time, str->fade_vol);
        return;
    }
    if (str->req & 0x8) {
        str->req &= ~0x8;
        str_req_nml_fade_set(str, 50, 0);
    }
}

void str_req_to_ready(SND_STR_WORK* str)
{
    str->req &= ~0x1;
    str->status |= 0x4;
    str->state = 1;
}

void str_req_to_play(SND_STR_WORK* str)
{
    str->req &= ~0x2;
    str->play_blk = 0;
    Snd_str_ax_voice_play(str);
    if (str->shortflag & 0x1) {
        str->state = 3;
    } else {
        str->state = 2;
    }
    str->status |= 0x30;
}

void str_req_vol_set(SND_STR_WORK* str)
{
    str->vol2 = str->req_vol << 8;
    str->upd |= 0x1;
}

void str_req_nml_fade_set(SND_STR_WORK* str, s16 time, s16 vol)
{
    s16 diff;

    if (str->status & 0x200) {
        str->status &= ~0x200;
    }
    str->fade_target = vol << 8;
    diff = str->fade_target - str->vol2;
    str->fade_step = diff / time;
    if (str->fade_step == 0) {
        if (diff > 0) {
            str->fade_step = 1;
        } else {
            str->fade_step = -1;
        }
    }
    str->status |= 0x100;
}

int str_fade_check(SND_STR_WORK* str)
{
    if (str->status & 0x200) {
        if (str->vol2 == str->err_target) {
            str->status &= ~0x200;
            if (str->vol2 == 0) {
                str_play_end(str);
                return 1;
            }
        } else {
            str_fade_new_vol_set(str, str->err_step, str->err_target);
        }
        return 0;
    }
    if (str->status & 0x100) {
        if (str->vol2 == str->fade_target) {
            str->status &= ~0x100;
            if (str->vol2 == 0) {
                str_play_end(str);
                return 1;
            }
        } else {
            str_fade_new_vol_set(str, str->fade_step, str->fade_target);
        }
    }
    return 0;
}

void str_fade_new_vol_set(SND_STR_WORK* str, s16 step, s16 target)
{
    int v;

    v = str->vol2 + step;
    if (step > 0) {
        if (v > target) {
            v = target;
        }
    } else {
        if (v < target) {
            v = target;
        }
    }
    str->vol2 = v;
    str->upd |= 0x1;
}

int str_reset_check(SND_STR_WORK* str)
{
    if (Snd_ctrl_work.reset_flag & 0x20) {
        str_play_cancel(str);
        return 1;
    } else {
        return 0;
    }
}

void str_play_cancel(SND_STR_WORK* str)
{
    if (str->dvd_busy != 0) {
        DVDCancelAsync(&str->dvd.cb, NULL);
    }
    str_play_end(str);
}

void str_play_end(SND_STR_WORK* str)
{
    Snd_str_ax_voice_stop(str);
    str->status &= ~0x10;
    str->state = 4;
}

void str_recovery_check(SND_STR_WORK* str)
{
    if (str->err != 0) {
        return;
    }
    if (str->dvd_busy != 0) {
        return;
    }
    if (str->read_cnt != 0) {
        return;
    }
    str->status &= 0x7FFF;
}
