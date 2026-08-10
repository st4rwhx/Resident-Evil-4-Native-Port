#include "snd_drv.h"

void Snd_efx_work_clear(void)
{
    SND_EFX_WORK* efx;
    u32 i;
    u32 j;
    u8* p;

    for (i = 0; i < 2; i++) {
        efx = &Snd_efx_work[i];
        p = (u8*) efx;
        for (j = 0; j < sizeof(SND_EFX_WORK); j++) {
            *p++ = 0;
        }
        efx->aux = i;
        efx->type = 0;
    }
}

int Snd_efx_req(s16 no, s16 type)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
    SND_EFX_WORK* efx;
    int ret;

    efx = &Snd_efx_work[no];
    if (type == 0) {
        efx_req_off(efx);
        return 0;
    }
    if (type == 6) {
        efx_req_stop(efx);
        return 0;
    }
    ctrl->efx_err = 0;
    if (efx->status & 0x1) {
        if (type == efx->type) {
            ret = efx_req_set_update(efx, type);
        } else {
            ret = efx_req_set_new(efx, type);
            efx_buffer_free(efx, efx->type);
        }
    } else {
        ret = efx_req_set_new(efx, type);
    }
    if (ret == 1) {
        OSReport("SND EFX is not executed !! : %d\n", type);
        efx->err = ctrl->efx_err;
        efx->status = 0;
        efx->type = 7;
        return 1;
    }
    efx->status = 1;
    efx->type = type;
    return 0;
}

void efx_req_off(SND_EFX_WORK* efx)
{
    if (efx->status & 0x3) {
        if (efx->aux == 0) {
            AXRegisterAuxACallback(NULL, NULL);
        } else {
            AXRegisterAuxBCallback(NULL, NULL);
        }
        if (efx->status & 0x1) {
            efx_buffer_free(efx, efx->type);
        }
    }
    efx->status = 0;
    efx->type = 0;
}

void efx_req_stop(SND_EFX_WORK* efx)
{
    if (efx->status & 0x1) {
        if (efx->aux == 0) {
            AXRegisterAuxACallback((SND_AUX_CB) cb_efx_clear_bass, NULL);
        } else {
            AXRegisterAuxBCallback((SND_AUX_CB) cb_efx_clear_bass, NULL);
        }
        efx_buffer_free(efx, efx->type);
        efx->status = 2;
        efx->type = 6;
    } else {
    }
}

int efx_req_set_new(SND_EFX_WORK* efx, s16 type)
{
    SND_AUX_CB cb;
    void* param;
    int ret;

    switch (type) {
    case 1:
        ret = AXFXReverbHiInit(&efx->fx.hi);
        cb = (SND_AUX_CB) AXFXReverbHiCallback;
        param = &efx->fx;
        break;
    case 2:
        ret = AXFXReverbStdInit(&efx->fx.std);
        cb = (SND_AUX_CB) AXFXReverbStdCallback;
        param = &efx->fx;
        break;
    case 3:
        ret = AXFXChorusInit(&efx->fx.chorus);
        cb = (SND_AUX_CB) AXFXChorusCallback;
        param = &efx->fx;
        break;
    case 4:
        ret = AXFXDelayInit(&efx->fx.delay);
        cb = (SND_AUX_CB) AXFXDelayCallback;
        param = &efx->fx;
        break;
    case 5:
        if (Snd_ctrl_work.sound_mode != 2) {
            OSReport("SOUND MODE is not DPL2\n");
            return 1;
        }
        ret = AXFXReverbHiInitDpl2(&efx->fx.dpl2);
        cb = (SND_AUX_CB) AXFXReverbHiCallbackDpl2;
        param = &efx->fx;
        break;
    default:
        ret = 0;
        cb = NULL;
        param = NULL;
        break;
    }
    if (ret != 1) {
        return 1;
    } else {
        if (efx->aux == 0) {
            AXRegisterAuxACallback(cb, param);
        } else {
            AXRegisterAuxBCallback(cb, param);
        }
        return 0;
    }
}

int efx_req_set_update(SND_EFX_WORK* efx, s16 type)
{
    int ret;

    switch (type) {
    case 1:
        ret = AXFXReverbHiSettings(&efx->fx.hi);
        break;
    case 2:
        ret = AXFXReverbStdSettings(&efx->fx.std);
        break;
    case 3:
        ret = AXFXChorusSettings(&efx->fx.chorus);
        break;
    case 4:
        ret = AXFXDelaySettings(&efx->fx.delay);
        break;
    case 5:
        ret = AXFXReverbHiSettingsDpl2(&efx->fx.dpl2);
        break;
    default:
        ret = 0;
        break;
    }
    if (ret != 1) {
        return 1;
    } else {
        return 0;
    }
}

void efx_buffer_free(SND_EFX_WORK* efx, s16 type)
{
    switch (type) {
    case 1:
        AXFXReverbHiShutdown(&efx->fx.hi);
        break;
    case 2:
        AXFXReverbStdShutdown(&efx->fx.std);
        break;
    case 3:
        AXFXChorusShutdown(&efx->fx.chorus);
        break;
    case 4:
        AXFXDelayShutdown(&efx->fx.delay);
        break;
    case 5:
        AXFXReverbHiShutdownDpl2(&efx->fx.dpl2);
        break;
    }
}

void cb_efx_clear_bass(AXFX_BUFFERUPDATE* buf, void* context)
{
    memclr_asm(buf->left, 0x280);
    memclr_asm(buf->right, 0x280);
    memclr_asm(buf->surround, 0x280);
}

int Snd_efx_get_status(s16 no)
{
    SND_EFX_WORK* efx;

    efx = &Snd_efx_work[no];
    if (efx->status & 0x1) {
        return 1;
    } else {
        return 0;
    }
}

s16 Snd_efx_get_type(s16 no)
{
    return Snd_efx_work[no].type;
}
