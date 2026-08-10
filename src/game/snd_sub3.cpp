#include "snd_drv.h"

void Snd_iss_blk_init(u32 blk_no, void* data)
{
    SND_ISS_BLK* blk;
    u32* p;

    blk = &Snd_iss_blk[blk_no];
    p = (u32*) data;
    blk->num = *p++;
    blk->dls = (u8*) data + *p++;
    blk->sit = (SND_SIT*) ((u8*) data + *p++);
    blk->seq = (u8*) data + *p++;
}

void Snd_str_blk_init(u32 blk_no, void* data)
{
    SND_STR_BLK* blk;
    u32* p;

    blk = &Snd_str_blk[blk_no];
    p = (u32*) data;
    blk->num = *p++;
    blk->shd = (u8*) data + *p++;
    blk->rit = (SND_RIT*) ((u8*) data + *p++);
}

SND_ISS_BLK* Snd_get_blk_adrs(u16 blk_no, u16 req_no)
{
    return &Snd_iss_blk[blk_no];
}

SND_SIT* Snd_get_sit_adrs(u16 blk_no, u16 req_no)
{
    SND_SIT* sit;

    sit = Snd_iss_blk[blk_no].sit;
    sit += req_no;
    return sit;
}

SND_RIT* Snd_get_rit_adrs(u16 blk_no, u16 req_no)
{
    SND_RIT* rit;

    rit = Snd_str_blk[blk_no].rit;
    rit += req_no;
    return rit;
}

SND_SHD* Snd_get_shd_adrs(u16 blk_no, u16 req_no)
{
    SND_RIT* rit;
    u8* shd;

    rit = Snd_get_rit_adrs(blk_no, req_no);
    shd = Snd_str_blk[blk_no].shd;
    shd += ((u32*) shd)[rit->shd_no];
    return (SND_SHD*) shd;
}

u16 Snd_iss_get_sit_type(u16 blk_no, u16 req_no)
{
    SND_SIT* sit;

    sit = Snd_get_sit_adrs(blk_no, req_no);
    if (sit->flag & 0x8000) {
        return 0x8000;
    }
    return sit->flag & 0x7;
}

s8 Snd_iss_get_sit_vol(u16 blk_no, u16 req_no)
{
    SND_SIT* sit;
    s8 vol;

    sit = Snd_get_sit_adrs(blk_no, req_no);
    if (sit->vol < 0) {
        vol = get_dls_vol_pan(blk_no, req_no, 0);
        return vol;
    } else {
        return sit->vol;
    }
}

s8 Snd_iss_get_sit_svol(u16 blk_no, u16 req_no)
{
    SND_SIT* sit;
    s8 vol;

    sit = Snd_get_sit_adrs(blk_no, req_no);
    if (sit->svol < 0) {
        vol = Snd_iss_get_sit_vol(blk_no, req_no);
        return vol;
    } else {
        return sit->svol;
    }
}

s8 Snd_iss_get_sit_pan(u16 blk_no, u16 req_no)
{
    SND_SIT* sit;
    s8 pan;

    sit = Snd_get_sit_adrs(blk_no, req_no);
    if (sit->pan == -1) {
        return -1;
    }
    if (sit->pan < 0) {
        pan = get_dls_vol_pan(blk_no, req_no, 1);
        return pan;
    } else {
        return sit->pan;
    }
}

s8 Snd_iss_get_sit_span(u16 blk_no, u16 req_no)
{
    SND_SIT* sit;

    sit = Snd_get_sit_adrs(blk_no, req_no);
    if (sit->span == -1) {
        return -1;
    }
    if (sit->span < 0) {
        return 0x7F;
    } else {
        return sit->span;
    }
}

s8 get_dls_vol_pan(u16 blk_no, u16 req_no, int mode)
{
    SND_ISS_BLK* blk;
    SND_SIT* sit;
    SND_WT_HDR* hdr;
    WTINST* inst;
    WTREGION* rgn;
    WTART* art;
    s32 vol;

    blk = Snd_get_blk_adrs(blk_no, req_no);
    sit = Snd_get_sit_adrs(blk_no, req_no);
    hdr = (SND_WT_HDR*) blk->dls;
    inst = (WTINST*) (blk->dls + hdr->inst_ofs);
    inst += (u16) (sit->prog >> 8);
    rgn = (WTREGION*) (blk->dls + hdr->rgn_ofs);
    rgn += inst->keyRegion[sit->prog & 0xFF];
    art = (WTART*) (blk->dls + hdr->art_ofs);
    art += rgn->articulationIndex;
    if (mode == 0) {
        vol = rgn->attn / 0x10000;
        return Snd_vol_ax_to_syn(vol);
    } else {
        return art->pan;
    }
}
