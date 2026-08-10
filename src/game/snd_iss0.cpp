#include "snd_drv.h"

int Snd_iss_req_para(u16 blk_no, u16 req_no, u8* para)
{
    int ret;

    ret = req_iss_main(blk_no, req_no, para);
    return ret;
}

int req_iss_main(u16 blk_no, u16 req_no, u8* para)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
    SND_SIT* sit;

    ctrl->multi_req = 0;
    if (req_no >= Snd_iss_blk[blk_no].num) {
        OSReport("SND REQ_NO is not found.\n");
        OSReport("BLK_NO : %d / REQ_NO : %d\n", blk_no, req_no);
        return 0;
    }
    ctrl->req_id++;
    if (ctrl->req_id == 0) {
        ctrl->req_id++;
    }
    while (1) {
        sit = Snd_iss_blk[blk_no].sit;
        sit += req_no;
        if (sit->flag & 0x8000) {
            OSReport("SND REQ_NO is dummy data.\n");
            OSReport("BLK_NO : %d / REQ_NO : %d\n", blk_no, req_no);
            return 0;
        }
        req_set_srd_type(ctrl, sit, para);
        if (req_iss_one(ctrl, sit, blk_no, req_no) == 1) {
            return 0;
        }
        if (!(sit->flag & 0x2000)) {
            return ctrl->req_id;
        }
        ctrl->multi_req = 1;
        req_no++;
    }
}

void req_set_srd_type(SND_CTRL_WORK* ctrl, SND_SIT* sit, u8* para)
{
    if (ctrl->flag_58 & 0x100) {
        ctrl->srd_type = ctrl->x50;
    } else {
        ctrl->srd_type = sit->srd_type;
    }
    if (para != NULL) {
        *para = ctrl->srd_type;
    }
}

int req_iss_one(SND_CTRL_WORK* ctrl, SND_SIT* sit, u16 blk_no, u16 req_no)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = req_iss_one_sub(ctrl, sit, blk_no, req_no);
    OSRestoreInterrupts(old);
    return ret;
}

int req_iss_one_sub(SND_CTRL_WORK* ctrl, SND_SIT* sit, u16 blk_no, u16 req_no)
{
    SND_REQ_WORK* req;

    req = Snd_open_req_work();
    if (req == NULL) {
        return 1;
    }
    req->status = 1;
    if (sit->flag & 0x100) {
        req->type = 1;
    } else {
        req->type = 2;
    }
    req->srd_type = ctrl->srd_type;
    req->blk_no = blk_no;
    req->req_no = req_no;
    req->snd_id = ctrl->req_id;
    req->sit = sit;
    Snd_req_work_copy_para(ctrl, req);
    return 0;
}

int Snd_get_play_type(u32 snd_id)
{
    SND_REQ_WORK* req;
    SND_VOICE_WORK* voice;
    SND_SEQ_WORK* seq;
    SND_STR_WORK* str;
    int old;
    int type = 0;

    old = OSDisableInterrupts();
    req = Snd_search_req_work_snd_id(snd_id, 3);
    if (req != NULL) {
        if (req->sit->flag & 0x4) {
            type = 2;
        } else {
            type = 1;
        }
    }
    OSRestoreInterrupts(old);
    if (type != 0) {
        return type;
    }
    old = OSDisableInterrupts();
    voice = Snd_search_voice_work_snd_id(snd_id);
    OSRestoreInterrupts(old);
    if (voice != NULL) {
        return 1;
    }
    seq = Snd_search_seq_work_snd_id(snd_id);
    if (seq != NULL) {
        return 2;
    }
    str = Snd_search_str_work_snd_id(snd_id);
    if (str != NULL) {
        return 4;
    }
    return 0;
}
