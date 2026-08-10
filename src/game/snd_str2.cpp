#include "snd_drv.h"

void Snd_str_dvd_read_sub(SND_STR_WORK* str)
{
    u32 blks;
    u8* dst;

    if (str->read_cnt == 0) {
        return;
    }
    if (str->dvd_busy != 0) {
        return;
    }
    if (str->buff_blks == 1 && str->dma_busy != 0) {
        return;
    }
    str->read_cnt--;
    dst = str->buff;
    dst += str->read_blk * str->read_size;
    DVDReadAsyncPrio(&str->dvd, dst, str->read_size, str->aram + str->read_ofs, cb_dvd_read_end, 0);
    str->dvd_busy = 1;
    str->read_ofs += str->read_size;
    if (str->read_ofs < str->read_end) {
        return;
    }
    if (!(str->flag & 0x4)) {
        str->read_done = 1;
    } else {
        blks = (str->loop_start >> 1) / str->blk_half;
        str->read_ofs = blks * str->read_size;
    }
}

void Snd_str_aram_dma_sub(SND_STR_WORK* str)
{
    u8* src;
    u32 ofs;

    if (str->dma_cnt == 0) {
        return;
    }
    if (str->dma_busy != 0) {
        return;
    }
    if (str->buff_blks == 1 && str->dvd_busy != 0) {
        return;
    }
    str->dma_cnt--;
    src = str->buff;
    src += str->dma_blk * str->read_size;
    ofs = str->dma_aram_blk * str->blk_half;
    if (str->flag & 0x1) {
        ARQPostRequest(&str->arqL, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH, (u32) src, str->aram_L + ofs,
                       str->blk_half, NULL);
        ARQPostRequest(&str->arqR, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH, (u32) (src + 0x4000),
                       str->aram_R + ofs, str->blk_half, cb_aram_dma_end);
    } else {
        ARQPostRequest(&str->arqL, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH, (u32) src, str->aram_L + ofs,
                       str->blk_half, cb_aram_dma_end);
    }
    str->dma_busy = 1;
    if (str->dma_aram_blk != 0) {
        return;
    }
    str->pred_L = src[0];
    if (str->flag & 0x2) {
        return;
    }
    str->pred_R = src[0x4000];
}

void cb_dvd_read_end(s32 result, DVDFileInfo* info)
{
    SND_STR_WORK* str;
    int i;

    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        if (&str->dvd == info) {
            break;
        }
    }
    str->dvd_busy = 0;
    switch (result) {
    case -1:
    case -2:
    case -3:
        return;
    default:
        str->dma_blk = str->read_blk;
        str->read_blk++;
        if (str->read_blk == str->buff_blks) {
            str->read_blk = 0;
        }
        str->dma_cnt++;
    }
}

void cb_aram_dma_end(u32 task)
{
    ARQRequest* req;
    SND_STR_WORK* str;
    int i;
    s8 last;

    req = (ARQRequest*) task;
    str = NULL;
    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        if (str->flag & 0x1) {
            if (&str->arqR == req) {
                break;
            }
        } else {
            if (&str->arqL == req) {
                break;
            }
        }
    }
    str->dma_busy = 0;
    str->dma_last_blk = str->dma_aram_blk;
    str->dma_aram_blk++;
    if (str->dma_aram_blk == str->aram_blks) {
        str->dma_aram_blk = 0;
    }
    if (str->status & 0x2) {
        return;
    }
    if (str->shortflag & 0x1) {
        last = str->read_end / str->read_size - 1;
    } else {
        last = str->aram_blks - 1;
    }
    if (str->dma_last_blk == last) {
        str->status &= ~0x4;
        str->status |= 0x2;
    } else {
        str->read_cnt++;
    }
}

void Snd_str_get_now_play_nbl(SND_STR_WORK* str)
{
    u32 cur;

    if (str->play_blk == -1) {
        return;
    }
    cur = *(u32*) &str->voiceL->pb.addr.currentAddressHi;
    str->play_nbl = cur - str->aram_L_nbl;
    str->prev_blk = str->play_blk;
    str->play_blk = str->play_nbl / str->blk_size;
    if (str->shortflag & 0x1) {
        str->play_pos = str->play_nbl;
        return;
    }
    if (str->play_blk != str->prev_blk) {
        str_ax_voice_to_next_block(str);
    }
    str->play_pos = str->blk_cnt * str->blk_size;
    str->play_pos = str->play_pos + str->play_nbl % str->blk_size;
}

void str_ax_voice_to_next_block(SND_STR_WORK* str)
{
    u32 ofs;

    if (str->loop_top != 0) {
        str->loop_top = 0;
        str->blk_cnt = str->loop_start / str->blk_size;
        str->x84 = str->blk_cnt * str->blk_size;
    } else {
        str->blk_cnt++;
    }
    str->x84 += str->blk_size;
    if (str->x84 >= str->loop_end) {
        ofs = str->play_blk * str->blk_size;
        ofs += str->loop_end % str->blk_size;
        str->end_L = str->aram_L_nbl + ofs;
        str->end_R = str->aram_R_nbl + ofs;
        if (str->flag & 0x4) {
            str->read_cnt++;
            str_ax_voice_loop_to_top(str);
        } else {
            str_ax_voice_loop_to_end(str);
        }
        return;
    }
    if (str->play_blk == str->aram_blks - 1) {
        str_ax_voice_last_to_top(str);
    }
    if (str->read_done == 0) {
        str->read_cnt++;
    }
}

void str_ax_voice_loop_to_top(SND_STR_WORK* str)
{
    SND_SHD* shd;
    AXPBADPCMLOOP loop;
    u32 ofs;
    s16 blk;

    shd = str->shd;
    str->loop_top = 1;
    blk = str->play_blk + 1;
    if (blk == str->aram_blks) {
        blk = 0;
    }
    ofs = blk * str->blk_size;
    ofs += str->loop_start % str->blk_size;
    str->loop_L = str->aram_L_nbl + ofs;
    str->loop_R = str->aram_R_nbl + ofs;
    loop.loop_pred_scale = shd->loop_pred_scale[0];
    loop.loop_yn1 = shd->loop_yn1[0];
    loop.loop_yn2 = shd->loop_yn2[0];
    AXSetVoiceAdpcmLoop(str->voiceL, &loop);
    AXSetVoiceType(str->voiceL, 0);
    AXSetVoiceLoopAddr(str->voiceL, str->loop_L);
    AXSetVoiceEndAddr(str->voiceL, str->end_L);
    if (str->flag & 0x2) {
        return;
    }
    loop.loop_pred_scale = shd->loop_pred_scale[1];
    loop.loop_yn1 = shd->loop_yn1[1];
    loop.loop_yn2 = shd->loop_yn2[1];
    AXSetVoiceAdpcmLoop(str->voiceR, &loop);
    AXSetVoiceType(str->voiceR, 0);
    AXSetVoiceLoopAddr(str->voiceR, str->loop_R);
    AXSetVoiceEndAddr(str->voiceR, str->end_R);
}

void str_ax_voice_loop_to_end(SND_STR_WORK* str)
{
    u32 zero;

    zero = Snd_ctrl_work.aram_base * 2 + 2;
    str->state = 3;
    AXSetVoiceLoop(str->voiceL, 0);
    AXSetVoiceLoopAddr(str->voiceL, zero);
    AXSetVoiceEndAddr(str->voiceL, str->end_L);
    if (str->flag & 0x2) {
        return;
    }
    AXSetVoiceLoop(str->voiceR, 0);
    AXSetVoiceLoopAddr(str->voiceR, zero);
    AXSetVoiceEndAddr(str->voiceR, str->end_R);
}

void str_ax_voice_last_to_top(SND_STR_WORK* str)
{
    AXPBADPCMLOOP loop;
    u32 len;

    str->loop_L = str->aram_L_nbl + 2;
    str->loop_R = str->aram_R_nbl + 2;
    if (str->flag & 0x1) {
        len = 0x3FFFF;
    } else {
        len = 0x7FFFF;
    }
    str->end_L = str->aram_L_nbl + len;
    str->end_R = str->aram_R_nbl + len;
    loop.loop_pred_scale = str->pred_L;
    loop.loop_yn1 = 0;
    loop.loop_yn2 = 0;
    AXSetVoiceAdpcmLoop(str->voiceL, &loop);
    AXSetVoiceType(str->voiceL, 1);
    AXSetVoiceLoopAddr(str->voiceL, str->loop_L);
    AXSetVoiceEndAddr(str->voiceL, str->end_L);
    if (str->flag & 0x2) {
        return;
    }
    loop.loop_pred_scale = str->pred_R;
    loop.loop_yn1 = 0;
    loop.loop_yn2 = 0;
    AXSetVoiceAdpcmLoop(str->voiceR, &loop);
    AXSetVoiceType(str->voiceR, 1);
    AXSetVoiceLoopAddr(str->voiceR, str->loop_R);
    AXSetVoiceEndAddr(str->voiceR, str->end_R);
}
