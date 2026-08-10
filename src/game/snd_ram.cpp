#include "snd_drv.h"

// Sound library work areas, in the order the other snd_*.cpp units expect them.
SND_CTRL_WORK Snd_ctrl_work;
SND_EFX_WORK Snd_efx_work[2];
SND_REQ_WORK Snd_req_work[SND_REQ_BANK_MAX][SND_REQ_MAX];
SND_VOICE_WORK Snd_voice_work[SND_VOICE_MAX];
SND_AXV_WORK Snd_axv_work[SND_AXV_MAX];
SND_ISS_BLK Snd_iss_blk[SND_ISS_BLK_MAX];
SND_STR_BLK Snd_str_blk[SND_STR_BLK_MAX];
SND_SEQ_WORK Snd_seq_work[SND_SEQ_MAX];
SND_STR_WORK Snd_str_work[SND_STR_MAX];
u8* Snd_str_buff[SND_STR_MAX];
SND_TEST_WORK Snd_test_work __attribute__((aligned(32)));
