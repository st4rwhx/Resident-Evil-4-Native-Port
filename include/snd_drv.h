#ifndef SND_DRV_H
#define SND_DRV_H

// Capcom sound library (D:/Bio4/Prog/sound_driver, src/game/snd_*.cpp): C++ with C linkage, compiled
// unoptimised (-O0, see UNIT_CFLAG_OVERRIDES in config/G4BE08/objects.py). Work areas live in
// snd_ram.c. Field offsets come from the disassembly; pads mark unknown bytes. Only extend.

#include "snd_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SND_REQ_BANK_MAX 2
#define SND_REQ_MAX 64
#define SND_VOICE_MAX 64
#define SND_AXV_MAX 64
#define SND_ISS_BLK_MAX 14
#define SND_STR_BLK_MAX 2
#define SND_SEQ_MAX 8
#define SND_STR_MAX 4

// Sound information table entry (SIT, 0x18 bytes) inside an ISS block.
typedef struct {
    u16 prog;       // 0x00  bank << 8 | program
    s8 voice_start; // 0x02  first Snd_voice_work slot (sequences)
    s8 voice_num;   // 0x03  last slot offset (inclusive)
    s8 prio;        // 0x04
    s8 pan;         // 0x05  < 0: from the DLS
    s8 vol;         // 0x06  < 0: from the DLS
    s8 x7;          // 0x07  aux A
    s8 x8;          // 0x08  aux B
    s8 curve_no;    // 0x09  distance curve selector (game/snd.cpp SndCall), -1 = none
    u16 pitch_lo;   // 0x0A  random pitch range
    u16 pitch_hi;   // 0x0C
    u8 inner_vol;   // 0x0E  volume % while the player is on a type-3 floor attribute (0 = off)
    u8 xF;
    u8 srd_type;    // 0x10
    s8 span;        // 0x11
    s8 svol;        // 0x12
    s8 rnd_no;      // 0x13  random SE table selector (game/snd.cpp seRandomCheck)
    u8 se_flag;     // 0x14  0x1/0x2/0x4 -> Snd_ctrl_work.x56 bits, 0x20 area volume control
    u8 wall_vol;    // 0x15  volume % behind a wall (1..99), also the room BGM start volume
    u16 flag;       // 0x16  0x4 seq, 0x100 type 1, 0x2000 chained, 0x4000 may steal an equal priority voice, 0x8000 dummy
} SND_SIT;

// Room information table entry (RIT, 0x10 bytes) inside a stream block.
typedef struct {
    s16 shd_no;     // 0x00  index into the block's stream header offset table
    s8 voice_start; // 0x02  first Snd_voice_work slot
    s8 voice_num;   // 0x03  last slot offset (inclusive)
    s8 vol;         // 0x04
    s8 pan;         // 0x05
    u8 pad_6[2];
    s8 auxA;        // 0x08
    s8 auxB;        // 0x09
    s8 str_no;      // 0x0A  default Snd_str_work slot
    u8 pad_B[1];
    u16 flag;       // 0x0C  0x1 = surround type
    s8 span;        // 0x0E  < 0: 0x7F
    s8 svol;        // 0x0F  surround volume, < 0: use vol
} SND_RIT;

// Stream header (SHD): ADPCM stream file description.
typedef struct {
    u32 flag;       // 0x00  0x1 stereo, 0x2 ?, 0x4 no loop, 0x8 ?
    u32 x4;
    u32 len;        // 0x08  file length (bytes, both channels)
    u32 rate;       // 0x0C  sample rate
    u32 x10;
    u32 loop_start; // 0x14  nibble offset
    u32 loop_end;   // 0x18  nibble offset
    u32 aram;       // 0x1C  ARAM buffer address
    u16 coefL[16];  // 0x20
    u16 coefR[16];  // 0x40
    u16 gain[2];    // 0x60  L, R
    u16 pred_scale[2];      // 0x64
    u16 yn1[2];             // 0x68
    u16 yn2[2];             // 0x6C
    u16 loop_pred_scale[2]; // 0x70
    u16 loop_yn1[2];        // 0x74
    u16 loop_yn2[2];        // 0x78
} SND_SHD;

typedef struct {
    u32 num;        // 0x00  number of SITs
    SND_SIT* sit;   // 0x04
    u8* dls;        // 0x08  wavetable (SND_WT_HDR + SYN WT tables)
    u8* seq;        // 0x0C  sequence table (SND_SEQ_TBL)
    u32 aram;       // 0x10  ARAM address of the block's samples
    u8 pad_14[0xC];
} SND_ISS_BLK;

// SND_ISS_BLK::seq: u32 count followed by one offset (from the table start) per bank.

typedef struct {
    u32 num;        // 0x00
    SND_RIT* rit;   // 0x04
    u8* shd;        // 0x08  table of offsets to the stream headers
    u8 pad_C[4];
} SND_STR_BLK;

// Wavetable (DLS) file header: offsets to the SYN WTINST / WTREGION / WTART tables.
typedef struct {
    u32 x0;
    u32 inst_ofs;   // 0x04
    u32 rgn_ofs;    // 0x08
    u32 art_ofs;    // 0x0C
    u32 sample_ofs; // 0x10
    u32 adpcm_ofs;  // 0x14
} SND_WT_HDR;

// Main control work (Snd_ctrl_work, 0xB4 bytes).
typedef struct {
    u32 frame;              // 0x00  audio frame counter
    u32 sound_mode;         // 0x04  0 mono, 1 stereo, 2 DPL2
    u32 req_id;             // 0x08  last issued sound id (never 0)
    u32 aram_base;          // 0x0C
    u32 aram_free;          // 0x10
    s32 dvd_err;            // 0x14  last DVD error status (-1 = none)
    s32 req_bank;           // 0x18  Snd_req_work bank in use
    s32 req_bank_sub;       // 0x1C  the other Snd_req_work bank
    u16 rnd;                // 0x20  random seed
    u16 reset_flag;         // 0x22  0x1 soft reset requested, 0x10 done
    u16 se_state;           // 0x24  0x1 paused, 0x2 volume down
    u16 se_ctrl;            // 0x26  pending SE commands (se_ctrl_sub): 0x80 reset pan, 0x100 reset vol
    s16 se_fade_time;       // 0x28
    s16 se_vdown_vol;       // 0x2A
    u8 srd_type;            // 0x2C  surround type of the request being issued
    u8 pad_2D[1];
    u16 rnd_pitch;          // 0x2E
    s32 multi_req;          // 0x30  set while issuing a chained (0x2000) request
    s16 se_pause_type;      // 0x34  block number to pause, -1 = all
    u8 pad_36[6];
    s16 sys_vol[6];         // 0x3C  system volumes (<< 8), bit 1..0x20 selects
    u8 x48;                 // 0x48  request override parameters (copied when flag_58 bit set)
    u8 x49;                 // 0x49
    u8 x4A;                 // 0x4A
    u8 x4B;                 // 0x4B
    u8 x4C;                 // 0x4C
    u8 x4D;                 // 0x4D
    u8 x4E;                 // 0x4E
    u8 x4F;                 // 0x4F
    u8 x50;                 // 0x50  surround type override (flag_58 & 0x100)
    u8 pad_51[1];
    u16 x52;                // 0x52
    u16 x54;                // 0x54
    u16 x56;                // 0x56
    u16 flag_58;            // 0x58  which override parameters are valid
    u8 pad_5A[2];
    u32 seq_tick;           // 0x5C  sequencer clock (1/1000 ms units, wraps at 999000)
    u32 seq_msec;           // 0x60  milliseconds elapsed this audio frame
    u8 midi_msg[3];         // 0x64  MIDI event being decoded (snd_seq1/snd_seq2)
    u8 pad_67[1];
    u8 midi_type;           // 0x68  status & 0xF0
    u8 midi_ch;             // 0x69  status & 0x0F
    u8 pad_6A[2];
    s32 efx_err;            // 0x6C
    u32 dsp_cycles_max;     // 0x70
    u32 dsp_cycles_peak;    // 0x74
    u32 dsp_cycles;         // 0x78
    u16 voice_peak;         // 0x7C
    u16 voice_num;          // 0x7E
    u16 axv_peak;           // 0x80
    u16 axv_num;            // 0x82
    u16 str_peak;           // 0x84
    u16 str_num;            // 0x86
    u16 seq_peak;           // 0x88
    u16 seq_num;            // 0x8A
    u16 total_peak;         // 0x8C
    u16 total_num;          // 0x8E
    ARQRequest arq;         // 0x90
    volatile s32 dma_busy;  // 0xB0 (snd_test cb_dma_end/aram_dump_dma address the field through the struct base)
} SND_CTRL_WORK;

// Sound request (Snd_req_work[2][64], 0x2C bytes).
typedef struct {
    u16 status;     // 0x00
    u16 no;         // 0x02
    u8 type;        // 0x04  1 / 2 (issue), 4 = command
    u8 srd_type;    // 0x05
    u16 cmd;        // 0x06  command number (type 4)
    u16 blk_no;     // 0x08
    u16 req_no;     // 0x0A
    u32 snd_id;     // 0x0C
    SND_SIT* sit;   // 0x10
    u16 flag;       // 0x14  override flags (from ctrl->flag_58)
    u16 para;       // 0x16  command parameter
    u8 pad_18[2];
    s8 x1A;         // 0x1A
    s8 x1B;         // 0x1B
    s8 x1C;         // 0x1C
    s8 x1D;         // 0x1D
    s8 x1E;         // 0x1E
    s8 x1F;         // 0x1F
    s8 x20;         // 0x20
    s8 x21;         // 0x21
    s16 x22;        // 0x22  pitch offset (cents)
    u16 x24;        // 0x24  pitch offset 2
    u16 x26;        // 0x26  SE flags (SND_AXV_WORK::flag)
    s16 pitch;      // 0x28  random pitch (cents)
    u8 pad_2A[2];
} SND_REQ_WORK;

struct SND_AXV_WORK_;

// Voice slot (Snd_voice_work[64], 0x20 bytes).
typedef struct {
    u16 status;     // 0x00
    u16 no;         // 0x02
    u32 snd_id;     // 0x04
    u8 x8;          // 0x08
    u8 out_mode;    // 0x09  1 / 2
    s8 type;        // 0x0A  1 iss, 2 seq, 3 str
    u8 pad_B[1];
    u32 count;      // 0x0C
    s32 rel_time;   // 0x10  release time for the note off
    struct SND_AXV_WORK_* axv;  // 0x14
    u16 blk_no;     // 0x18
    u16 req_no;     // 0x1A
    s8 prio;        // 0x1C
    s8 seq_no;      // 0x1D
    s8 seq_ch;      // 0x1E
    s8 seq_note;    // 0x1F
} SND_VOICE_WORK;

// AX voice work (Snd_axv_work[64], 0x80 bytes): one AX voice playing a wavetable sample.
typedef struct SND_AXV_WORK_ {
    u16 status;     // 0x00  0x1 in use, 0x2 attack envelope, 0x4 release envelope, 0x8 keep,
                    //       0x10 volume down
    u16 no;         // 0x02
    u32 snd_id;     // 0x04
    s8 srd_type;    // 0x08
    u8 adsr_on;     // 0x09
    u16 upd;        // 0x0A  0x1 volume, 0x2 pan, 0x4 aux A, 0x8 aux B, 0x10 lpf on/off, 0x20 lpf coefs,
                    //       0x40 pitch, 0x100 stop, 0x200 start
    u16 flag;       // 0x0C  request parameter (SND_REQ_WORK::x26): 0x2 = no volume down
    u8 pad_E[2];
    AXVPB* voice;   // 0x10
    SND_VOICE_WORK* vw;     // 0x14
    u32 aram;       // 0x18
    u8* wt;         // 0x1C  wavetable
    SND_SIT* sit;   // 0x20
    SND_WT_HDR* hdr;        // 0x24
    WTINST* inst;   // 0x28
    WTREGION* rgn;  // 0x2C
    WTART* art;     // 0x30
    WTSAMPLE* sample;       // 0x34
    WTADPCM* adpcm; // 0x38
    f32 rate;       // 0x3C
    s32 ax_vol;     // 0x40
    s32 ax_auxA;    // 0x44
    s32 ax_auxB;    // 0x48
    s16 vol;        // 0x4C  << 8
    s16 svol;       // 0x4E
    s16 vdown_src_vol;      // 0x50
    s16 vdown_src_svol;     // 0x52
    s16 vdown_vol;  // 0x54
    s16 vdown_svol; // 0x56
    s16 now_vol;    // 0x58
    s16 calc_vol;   // 0x5A
    u8 pad_5C[4];
    u32 attack_steps;       // 0x60
    s32 rel_time;   // 0x64
    s16 env_vol;    // 0x68
    s16 env_target; // 0x6A
    s16 env_step;   // 0x6C
    u16 env_cnt;    // 0x6E
    s16 pitch_base; // 0x70  cents
    u16 pitch_ofs;  // 0x72
    s16 pitch;      // 0x74
    s8 pan;         // 0x76
    s8 span;        // 0x77
    s8 out_span;    // 0x78
    s8 auxA;        // 0x79
    s8 auxB;        // 0x7A
    s8 lpf_on;      // 0x7B
    s8 lpf_no;      // 0x7C
    u8 pad_7D[3];
} SND_AXV_WORK;

// MIDI sequence work (Snd_seq_work[8], 0x328C bytes).
typedef struct {
    u16 status;             // 0x00  0x100 = fading
    s16 no;                 // 0x02
    u32 snd_id;             // 0x04
    u8 type;                // 0x08  1 / 2 (2 = surround)
    u8 pad_9[1];
    u16 flag;               // 0x0A  0x1 = reset volume
    u32 req;                // 0x0C  request bits: 0x1 fade, 0x2 stop, 0x4 volume
    u16 vol;                // 0x10
    u8 pad_12[2];
    s16 calc_vol;           // 0x14  system volume product (Snd_seq_work_calc_ax_vol)
    s16 vol2;               // 0x16  request / SIT volume << 8
    s32 ax_vol;             // 0x18
    u16 fade_time;          // 0x1C  requested fade: steps
    u16 fade_vol;           // 0x1E  requested fade: target volume
    s16 fade_step;          // 0x20
    s16 fade_target;        // 0x22  target volume << 8
    SYNSYNTH synth;         // 0x24 .. 0x3164
    u32 aram;               // 0x3164  ARAM base of the samples (SYNInitSynth)
    u8* wt;                 // 0x3168  wavetable
    SND_SIT* sit;           // 0x316C
    u8* seq_top;            // 0x3170  sequence data
    u8* seq_pos;            // 0x3174  read position
    u8* seq_loop;           // 0x3178
    s32 tempo;              // 0x317C  1000
    s32 x3180;              // 0x3180  480
    s32 delta;              // 0x3184  ticks to the next event
    s8 tpr_num;             // 0x3188  pending track parameter changes (seq_tpr_check)
    s8 tpr_kind[8];         // 0x3189  8 = volume (CC 7), else pan (CC 10)
    u8 tpr_val[8];          // 0x3191
    u8 pad_3199[1];
    u16 tpr_mask[8];        // 0x319A  channel mask
    s8 ch_flag[16];         // 0x31AA  per MIDI channel: 0x1 = drums (forced to channel 9)
    s8 ch_prio[16];         // 0x31BA  voice priority (CC 0x68)
    s8 ch_prog[16];         // 0x31CA  program, -1
    u8 ch_vol[16];          // 0x31DA  CC 7
    u8 ch_exp[16];          // 0x31EA  CC 11
    s8 ch_pan[16];          // 0x31FA  CC 10, -1
    u8 ch_pitch_lo[16];     // 0x320A
    u8 ch_pitch_hi[16];     // 0x321A
    s8 ch_data_msb[16];     // 0x322A  CC 6, -1
    s8 ch_data_lsb[16];     // 0x323A  CC 38, -1
    u8 ch_mod[16];          // 0x324A  CC 1
    u8 ch_hold[16];         // 0x325A  CC 64
    u8 ch_reverb[16];       // 0x326A  CC 91
    u8 ch_chorus[16];       // 0x327A  CC 92
    u8 pad_328A[2];
} SND_SEQ_WORK;

// Stream work (Snd_str_work[4], 0x14C bytes). Streams are ADPCM files read from DVD in
// read_size blocks into buff and DMA'd to an ARAM ring of aram_blks blocks per channel.
typedef struct {
    u16 status;     // 0x00  0x1 open, 0x2 last block sent, 0x4 ready, 0x10 playing, 0x20 started,
                    //       0x100 fading, 0x200 error fade, 0x1000/0x2000 voice L/R dropped,
                    //       0x4000 abort, 0x8000 DVD error
    u16 no;         // 0x02
    u32 snd_id;     // 0x04
    u8 type;        // 0x08  1 / 2 (2 = surround)
    s8 state;       // 0x09  str_player_tbl index
    s8 prev_state;  // 0x0A
    u8 upd;         // 0x0B  0x1 volume, 0x2 pan, 0x4 keep ax_vol
    SND_SHD* shd;   // 0x0C
    SND_RIT* rit;   // 0x10
    u8* buff;       // 0x14  DVD read buffer (Snd_str_buff[no])
    u32 dvd_status; // 0x18  DVDGetCommandBlockStatus (unsigned: -1 sorts last in the switch)
    u32 flag;       // 0x1C  shd->flag
    u32 req;        // 0x20  0x1 ready, 0x2 play, 0x4 fade, 0x8 stop, 0x10 volume
    u8 err;         // 0x24  0x1 DVD fatal, 0x2 DVD retry/no disk, 0x4 read starvation
    u8 cancel;      // 0x25  1 / 2
    u8 loop_top;    // 0x26  loop restarted from the top
    u8 shortflag;   // 0x27  0x1 fits in the ARAM ring, 0x2 loops, 0x4 one shot
    s8 pan;         // 0x28
    s8 span;        // 0x29
    s8 vol;         // 0x2A
    s8 x2B;         // 0x2B
    s8 auxA;        // 0x2C
    s8 auxB;        // 0x2D
    u8 pad_2E[2];
    f32 rate;       // 0x30
    s32 ax_vol;     // 0x34
    s32 ax_auxA;    // 0x38
    s32 ax_auxB;    // 0x3C
    s8 play_span;   // 0x40
    s8 out_span;    // 0x41
    u16 req_vol;    // 0x42
    s16 calc_vol;   // 0x44
    s16 vol2;       // 0x46  current volume << 8
    s16 fade_time;  // 0x48  request
    s16 fade_vol;   // 0x4A  request
    s16 fade_step;  // 0x4C
    s16 fade_target;// 0x4E
    s16 err_step;   // 0x50  error fade-out
    s16 err_target; // 0x52
    u32 aram;       // 0x54
    u32 read_ofs;   // 0x58  next DVD read offset
    u32 read_end;   // 0x5C  bytes to read (rounded up to read_size)
    u32 blk_half;   // 0x60  0x4000 / 0x8000
    u32 blk_size;   // 0x64  ARAM block bytes (per channel)
    u32 read_size;  // 0x68  bytes per DVD read
    u32 aram_L;     // 0x6C
    u32 aram_R;     // 0x70
    u32 aram_L_nbl; // 0x74  nibble addresses
    u32 aram_R_nbl; // 0x78
    u32 play_nbl;   // 0x7C
    u32 play_pos;   // 0x80  play position (bytes)
    u32 x84;        // 0x84
    u32 loop_start; // 0x88
    u32 loop_end;   // 0x8C
    s16 play_blk;   // 0x90  ARAM block being played (-1 = not started)
    s16 prev_blk;   // 0x92
    s16 blk_cnt;    // 0x94
    s8 dvd_busy;    // 0x96
    s8 read_done;   // 0x97
    s8 read_cnt;    // 0x98
    s8 read_blk;    // 0x99  buffer block being read
    s8 dma_blk;     // 0x9A  buffer block to DMA
    s8 buff_blks;   // 0x9B
    s8 dma_busy;    // 0x9C
    s8 dma_cnt;     // 0x9D
    s8 dma_aram_blk;// 0x9E
    s8 dma_last_blk;// 0x9F
    s8 aram_blks;   // 0xA0  ARAM ring blocks (8)
    u8 pad_A1[3];
    u32 cur_L;      // 0xA4  voice addresses (nibbles)
    u32 cur_R;      // 0xA8
    u32 loop_L;     // 0xAC
    u32 loop_R;     // 0xB0
    u32 end_L;      // 0xB4
    u32 end_R;      // 0xB8
    u16 pred_L;     // 0xBC  first byte (pred_scale) of the stream
    u16 pred_R;     // 0xBE
    SND_VOICE_WORK* vwL;    // 0xC0
    SND_VOICE_WORK* vwR;    // 0xC4
    AXVPB* voiceL;  // 0xC8
    AXVPB* voiceR;  // 0xCC
    DVDFileInfo dvd;        // 0xD0
    ARQRequest arqL;        // 0x10C
    ARQRequest arqR;        // 0x12C
} SND_STR_WORK;

// Sound test / debug work (Snd_test_work, 0x7C0 bytes, 32-aligned).
typedef struct {
    u8 x0;
    u8 x1;
    u8 x2;
    u8 x3;
    u8 pad_4[8];
    u16 xC;             // 0x0C
    u16 xE;             // 0x0E
    u8 pad_10[8];
    u16 x18;            // 0x18
    u16 x1A;            // 0x1A
    u8 pad_1C[0x9C - 0x1C];
    char path0[0x100];  // 0x9C
    char path1[0x380];  // 0x19C
    u32 x51C[14];       // 0x51C
    u32 x554[2];        // 0x554
    u8 pad_55C[0x6B8 - 0x55C];
    u32 aram_base;      // 0x6B8
    u8 pad_6BC[0x7C0 - 0x6BC];
} SND_TEST_WORK;

// AUX effect slot (Snd_efx_work[2], 0x278 bytes).
typedef void (*SND_AUX_CB)(void*, void*);
typedef struct {
    u16 status;     // 0x00  0x1 running, 0x2 stopping (buffers being cleared)
    s16 aux;        // 0x02  0 = AUX A, 1 = AUX B
    s16 type;       // 0x04  1 reverb hi, 2 reverb std, 3 chorus, 4 delay, 5 reverb hi DPL2, 6 stop, 7 error
    u8 pad_6[2];
    s32 err;        // 0x08
    union {
        AXFX_REVERBHI hi;
        AXFX_REVERBSTD std;
        AXFX_CHORUS chorus;
        AXFX_DELAY delay;
        AXFX_REVERBHI_DPL2 dpl2;
    } fx;           // 0x0C
} SND_EFX_WORK;

// Random number state (Snd_rnd): the word and its two bytes.
typedef union {
    s16 w;
    u8 b[2];
} SND_RND;

// Low-pass filter table entry (Snd_lpf_tbl[24]).
typedef struct {
    u16 a0;
    u16 b0;
    const char* name;
} SND_LPF;

// snd_ram.c
extern SND_CTRL_WORK Snd_ctrl_work;
extern SND_EFX_WORK Snd_efx_work[2];
extern SND_REQ_WORK Snd_req_work[SND_REQ_BANK_MAX][SND_REQ_MAX];
extern SND_VOICE_WORK Snd_voice_work[SND_VOICE_MAX];
extern SND_AXV_WORK Snd_axv_work[SND_AXV_MAX];
extern SND_ISS_BLK Snd_iss_blk[SND_ISS_BLK_MAX];
extern SND_STR_BLK Snd_str_blk[SND_STR_BLK_MAX];
extern SND_SEQ_WORK Snd_seq_work[SND_SEQ_MAX];
extern SND_STR_WORK Snd_str_work[SND_STR_MAX];
extern u8* Snd_str_buff[SND_STR_MAX];
extern SND_TEST_WORK Snd_test_work;

#ifdef SND_DRV_GAME_API
// Game-side view of the driver entry points (game/snd.cpp): the game's header declared the small
// integer parameters as int, so callers pass ints and u32s without the clrlwi the u16/u8/s16
// driver prototypes below would force (e.g. sndExistCheck -> Snd_iss_get_sit_type, seRandomCheck ->
// Snd_get_sit_adrs). Return types match the driver.
void Snd_soft_reset_req(void);
int Snd_soft_reset_ck(void);
void Snd_reset_pan_all(void);
void Snd_reset_vol_all(void);
void Snd_set_system_vol(int type, int vol);
s16 Snd_get_system_vol(int type);
void Snd_iss_blk_init(u32 blk_no, void* data);
void Snd_str_blk_init(u32 blk_no, void* data);
SND_SIT* Snd_get_sit_adrs(int blk_no, int req_no);
SND_SHD* Snd_get_shd_adrs(u16 blk_no, u16 req_no);
u16 Snd_iss_get_sit_type(int blk_no, int req_no);
s8 Snd_iss_get_sit_vol(int blk_no, int req_no);
s8 Snd_iss_get_sit_svol(int blk_no, int req_no);
s8 Snd_iss_get_sit_pan(int blk_no, int req_no);
s8 Snd_iss_get_sit_span(int blk_no, int req_no);
void Snd_system_init(void);
u32 Snd_sound_mode_init_load(u32 mode);
u32 Snd_get_sound_mode(void);
void Snd_set_sound_mode(u32 mode);
void Snd_iss_control(void);
int Snd_iss_req_para(int blk_no, int req_no, u8* para);
int Snd_get_play_type(u32 snd_id);
int Snd_se_set_paras(u32 snd_id);
int Snd_se_end_check(u32 snd_id);
int Snd_se_stop_one(u32 snd_id);
int Snd_se_fade_out_all(s16 time);
int Snd_se_fade_out_all2(s16 time);
int Snd_se_pronounce_ck_all(void);
int Snd_se_pause_on2(int type);
int Snd_se_pause_on3(void);
int Snd_se_pause_off2(int type);
int Snd_efx_req(int no, int type);
int Snd_efx_get_status(int no);
int Snd_seq_req(u32 snd_id, u32 cmd, u32 time, u32 vol);
void Snd_seq_fade_out_type(int type, s16 time);
int Snd_seq_fade_check(u32 snd_id);
int Snd_seq_end_check(u32 snd_id);
int Snd_seq_pronounce_ck_type(int type);
SND_SEQ_WORK* Snd_search_seq_work_snd_id(u32 snd_id);
u32 Snd_str_prepare(u16 blk_no, u16 req_no, char* name, int no);
int Snd_str_req(u32 snd_id, u32 cmd, u32 time, u32 vol);
int Snd_str_get_status(u32 snd_id);
int Snd_str_end_check(u32 snd_id);
void Snd_str_aram_adrs_set(int no, u32 adr);
SND_STR_WORK* Snd_search_str_work_snd_id(u32 snd_id);
int Snd_str_init_para(u32 snd_id, int flag, int val);
int Snd_str_init_pos(u32 snd_id, u32 pos);
u32 Snd_str_get_buff_smp(u16 blk_no, u16 req_no);
#else
// snd_sub0.c
extern s32 Snd_dls_vol_tbl[128];
extern SND_LPF Snd_lpf_tbl[24];
int Snd_pronounce_ck(void);
void Snd_soft_reset_req(void);
int Snd_soft_reset_ck(void);
void Snd_reset_pan_all(void);
void Snd_reset_vol_all(void);
void Snd_set_system_vol(s16 type, u16 vol);
s16 Snd_get_system_vol(s16 type);
s32 Snd_vol_syn_to_ax(s32 vol);
s32 Snd_vol_ax_to_syn(s32 vol);
u8 Snd_rnd(void);
s16 Snd_get_rnd_pitch(SND_SIT* sit);
void Snd_test_work_clear(void);

// snd_sub1.c
void Snd_voice_work_clear(void);
void Snd_stop_voice_work(SND_VOICE_WORK* voice);
SND_VOICE_WORK* Snd_open_voice_work_str(SND_STR_WORK* str, s8 no);
SND_VOICE_WORK* Snd_open_voice_work_seq(SND_SEQ_WORK* seq, s8 prio);
SND_VOICE_WORK* Snd_voice_work_open_ck(SND_SIT* info, s8 prio);
SND_VOICE_WORK* Snd_search_voice_work_snd_id(u32 snd_id);
SND_VOICE_WORK* Snd_search_voice_work_seq(SND_SEQ_WORK* seq, u8 ch, u8 note);

// snd_sub2.c
int Snd_se_reset_check(SND_CTRL_WORK* ctrl);
void Snd_req_work_clear(void);
void Snd_req_work_copy_para(SND_CTRL_WORK* ctrl, SND_REQ_WORK* req);
SND_REQ_WORK* Snd_open_req_work(void);
SND_REQ_WORK* Snd_search_req_work_snd_id(u32 snd_id, u8 type);

// snd_sub3.c
void Snd_iss_blk_init(u32 blk_no, void* data);
void Snd_str_blk_init(u32 blk_no, void* data);
SND_ISS_BLK* Snd_get_blk_adrs(u16 blk_no, u16 req_no);
SND_SIT* Snd_get_sit_adrs(u16 blk_no, u16 req_no);
SND_RIT* Snd_get_rit_adrs(u16 blk_no, u16 req_no);
SND_SHD* Snd_get_shd_adrs(u16 blk_no, u16 req_no);
u16 Snd_iss_get_sit_type(u16 blk_no, u16 req_no);
s8 Snd_iss_get_sit_vol(u16 blk_no, u16 req_no);
s8 Snd_iss_get_sit_svol(u16 blk_no, u16 req_no);
s8 Snd_iss_get_sit_pan(u16 blk_no, u16 req_no);
s8 Snd_iss_get_sit_span(u16 blk_no, u16 req_no);
s8 get_dls_vol_pan(u16 blk_no, u16 req_no, int mode);

// snd_main.c
extern u8 zero_tbl[0x100];
void Snd_system_init(void);
void snd_work_clear(void);
void zero_buff_clear(void);
void cb_audio_frame(void);
void Snd_sound_mode_init(void);
u32 Snd_sound_mode_init_load(u32 mode);
u32 Snd_get_sound_mode(void);
void Snd_set_sound_mode(u32 mode);
void snd_mode_set_ax_mix(SND_CTRL_WORK* ctrl);
void Snd_iss_control(void);
void Snd_dev_voice_ck(void);

// snd_iss0.c
int Snd_iss_req_para(u16 blk_no, u16 req_no, u8* para);
int req_iss_main(u16 blk_no, u16 req_no, u8* para);
void req_set_srd_type(SND_CTRL_WORK* ctrl, SND_SIT* sit, u8* para);
int req_iss_one(SND_CTRL_WORK* ctrl, SND_SIT* sit, u16 blk_no, u16 req_no);
int req_iss_one_sub(SND_CTRL_WORK* ctrl, SND_SIT* sit, u16 blk_no, u16 req_no);
int Snd_get_play_type(u32 snd_id);

// snd_iss1.c
int Snd_se_set_paras(u32 snd_id);
int se_set_paras_sub(u32 snd_id);
int Snd_se_end_check(u32 snd_id);
int Snd_se_stop_one(u32 snd_id);
int se_cmd_req_work(u16 cmd, u32 snd_id, u16 para);
int Snd_se_fade_out_all(s16 time);
int Snd_se_fade_out_all2(s16 time);
int se_ctrl_sub(u16 cmd, s16 para);
int Snd_se_pronounce_ck_all(void);
int se_pro_ck_req_work(int bank);
int se_pro_ck_axv_work(void);
int Snd_se_pause_on2(s16 type);
int Snd_se_pause_on3(void);
int Snd_se_pause_off2(s16 type);

// snd_iss2.c
void Snd_iss_manager(void);
void se_ctrl_execute(SND_CTRL_WORK* ctrl);
void se_ctrl_fade_out(SND_CTRL_WORK* ctrl, int mode);
void se_ctrl_pause_on(SND_CTRL_WORK* ctrl);
void se_ctrl_pause_on2(SND_CTRL_WORK* ctrl);
void seCtrlPauseOn_sub(SND_AXV_WORK* axv, SND_CTRL_WORK* ctrl);
void se_ctrl_pause_off(SND_CTRL_WORK* ctrl);
void se_ctrl_pause_off2(SND_CTRL_WORK* ctrl);
void seCtrlPauseOff_sub(SND_AXV_WORK* axv);
void se_ctrl_vdown_on(SND_CTRL_WORK* ctrl);
void se_ctrl_vdown_off(SND_CTRL_WORK* ctrl);
void se_ctrl_reset_pan_or_vol(SND_CTRL_WORK* ctrl, int mode);
void iss_req_execute(SND_CTRL_WORK* ctrl);
void iss_req_command(SND_REQ_WORK* req);
void req_cmd_se_stop(SND_REQ_WORK* req);
void req_cmd_se_para(SND_REQ_WORK* req);
void req_cmd_se_pan(SND_AXV_WORK* axv, SND_REQ_WORK* req, u16 bit);
void req_cmd_se_vol(SND_AXV_WORK* axv, SND_REQ_WORK* req, u16 bit);
void req_cmd_se_aux(SND_AXV_WORK* axv, SND_REQ_WORK* req, u16 bit);
void req_cmd_se_lpf(SND_AXV_WORK* axv, SND_REQ_WORK* req, u16 bit);
void req_cmd_se_pitch(SND_AXV_WORK* axv, SND_REQ_WORK* req, u16 bit);

// snd_iss3.c
void Snd_req_iss_new_play(SND_REQ_WORK* req);
void iss_new_voice_work(SND_ISS_BLK* blk, SND_SIT* sit, SND_REQ_WORK* req);
void iss_voice_work_init(SND_VOICE_WORK* vw, SND_AXV_WORK* axv, SND_REQ_WORK* req, s8 prio);
void iss_ax_set_wt_ptr(SND_AXV_WORK* axv, SND_SIT* sit);
void iss_ax_set_adsr(SND_AXV_WORK* axv);
void iss_ax_set_vol(SND_AXV_WORK* axv, SND_REQ_WORK* req, SND_SIT* sit);
void iss_ax_set_pan(SND_AXV_WORK* axv, SND_REQ_WORK* req, SND_SIT* sit);
void iss_ax_set_aux(SND_AXV_WORK* axv, SND_REQ_WORK* req, SND_SIT* sit);
void iss_ax_set_pitch(SND_AXV_WORK* axv, SND_REQ_WORK* req, SND_SIT* sit);
void iss_ax_set_lpf(SND_AXV_WORK* axv, SND_REQ_WORK* req);
void iss_ax_set_para(SND_AXV_WORK* axv, SND_REQ_WORK* req);
void cb_drop_voice(void* voice);

// snd_iss4.c
void Snd_axv_work_clear(void);
SND_AXV_WORK* Snd_open_axv_work(void);
void Snd_axv_work_close_check(void);
void axv_close_ck_main(SND_AXV_WORK* axv);
void Snd_axv_work_note_off(SND_AXV_WORK* axv, s32 time);
int Snd_axv_work_get_out_mode(SND_AXV_WORK* axv);
void Snd_axv_work_choice_now_vol(SND_AXV_WORK* axv);
void Snd_axv_work_calc_vdown_vol(SND_AXV_WORK* axv);
void Snd_axv_work_calc_ax_vol(SND_AXV_WORK* axv);
void Snd_axv_work_choice_out_span(SND_AXV_WORK* axv);
void Snd_axv_work_control(void);
void axv_work_adsr(SND_AXV_WORK* axv);
void axv_work_update(SND_AXV_WORK* axv);
void axv_work_update_vol_pan(SND_AXV_WORK* axv);
void axv_work_update_aux(SND_AXV_WORK* axv);
void axv_work_update_lpf(SND_AXV_WORK* axv);
void axv_work_update_pitch(SND_AXV_WORK* axv);

// snd_efx.c
void Snd_efx_work_clear(void);
int Snd_efx_req(s16 no, s16 type);
void efx_req_off(SND_EFX_WORK* efx);
void efx_req_stop(SND_EFX_WORK* efx);
int efx_req_set_new(SND_EFX_WORK* efx, s16 type);
int efx_req_set_update(SND_EFX_WORK* efx, s16 type);
void efx_buffer_free(SND_EFX_WORK* efx, s16 type);
void cb_efx_clear_bass(AXFX_BUFFERUPDATE* buf, void* context);
int Snd_efx_get_status(s16 no);
s16 Snd_efx_get_type(s16 no);

// snd_seq0.c
int Snd_seq_req(u32 snd_id, u32 cmd, u32 time, u32 vol);  // cmd 0x1 fade (time steps to vol), 0x2 stop, 0x4 volume (= time)
int seq_req_sub(u32 snd_id, u32 cmd, u32 time, u32 vol);
void Snd_seq_reset_vol_type(u8 type);
void Snd_seq_fade_out_type(u8 type, s16 time);
void seq_type_sub(u8 type, int mode, s16 time);
int Snd_seq_fade_check(u32 snd_id);
int Snd_seq_end_check(u32 snd_id);
int Snd_seq_pronounce_ck_type(u8 type);
int seq_pro_ck_req_work(int bank, u8 type);
int seq_pro_ck_seq_work(u8 type);

// snd_seq1.c
void Snd_midi_sequencer(void);
void seq_player(SND_CTRL_WORK* ctrl, SND_SEQ_WORK* seq);
int seq_reset_check(SND_SEQ_WORK* seq);
void seq_tpr_check(SND_SEQ_WORK* seq);
void seq_req_check(SND_SEQ_WORK* seq);
void seq_req_vol_set(SND_SEQ_WORK* seq);
void seq_req_fade_set(SND_SEQ_WORK* seq, s16 time, s16 vol);
int seq_fade_check(SND_SEQ_WORK* seq);
void seq_fade_new_vol_set(SND_SEQ_WORK* seq, s16 step, s16 target);
void seq_one_msec(SND_CTRL_WORK* ctrl, SND_SEQ_WORK* seq);
void seq_one_msec_main(SND_CTRL_WORK* ctrl, SND_SEQ_WORK* seq);
void seq_play_end(SND_SEQ_WORK* seq);
void seq_play_update(SND_SEQ_WORK* seq);

// snd_seq2.c
void Snd_seq_midi_message(SND_CTRL_WORK* m, SND_SEQ_WORK* seq);
void seq_note_on(SND_CTRL_WORK* m, SND_SEQ_WORK* seq);
void seq_note_off(SND_CTRL_WORK* m, SND_SEQ_WORK* seq);
void seq_prog_change(SND_CTRL_WORK* m, SND_SEQ_WORK* seq);
void seq_ctrl_change(SND_CTRL_WORK* m, SND_SEQ_WORK* seq);
void seq_ctrl_data_entry(SND_CTRL_WORK* m, SND_SEQ_WORK* seq);
void seq_pitch(SND_CTRL_WORK* m, SND_SEQ_WORK* seq);
void seq_event(SND_CTRL_WORK* m, SND_SEQ_WORK* seq);
void seq_drums_flag_ck(SND_CTRL_WORK* m, SND_SEQ_WORK* seq);

// snd_seq3.c
void Snd_seq_work_clear(void);
SND_SEQ_WORK* Snd_search_seq_work_snd_id(u32 snd_id);
void Snd_seq_work_close_check(void);
u32 Snd_seq_get_delta(SND_SEQ_WORK* seq);
void Snd_seq_send_midi(SND_CTRL_WORK* msg, SND_SEQ_WORK* seq);
void Snd_send_midi(SYNSYNTH* synth, u8 status, u8 data1, u8 data2);
void Snd_seq_work_calc_ax_vol(SND_SEQ_WORK* seq);
void Snd_iss_new_seq_work(SND_ISS_BLK* blk, SND_SIT* sit, SND_REQ_WORK* req);
SND_SEQ_WORK* open_seq_work(void);
void seq_work_init_track(SND_SEQ_WORK* seq);

// snd_str0.c
u32 Snd_str_prepare(u16 blk_no, u16 req_no, char* name, s8 no);
u32 Snd_str_init(SND_SHD* shd, SND_RIT* rit, u32 aram, char* name, s8 no);
s8 str_init_get_span(SND_RIT* rit);
s8 str_init_get_vol(SND_RIT* rit);
int Snd_str_req(u32 snd_id, u32 cmd, u32 time, u32 vol);
int str_req_sub(u32 snd_id, u32 cmd, u32 time, u32 vol);
void Snd_str_reset_vol_type(u8 type);
void Snd_str_reset_pan_type(u8 type);
void Snd_str_fade_out_type(u8 type, s16 time);
void str_type_sub(u8 type, u32 mode, s16 time);
int Snd_str_get_status(u32 snd_id);
int Snd_str_end_check(u32 snd_id);
int Snd_str_pronounce_ck_type(u8 type);
int str_pro_ck_str_work(u8 type);
void Snd_str_aram_adrs_set(int no, u32 adr);

// snd_str1.c
void Snd_stream_player(void);
void str_player_idle(SND_STR_WORK* str);
void str_player_ready(SND_STR_WORK* str);
void str_player_normal(SND_STR_WORK* str);
void str_player_noread(SND_STR_WORK* str);
void str_player_close(SND_STR_WORK* str);
void str_player_error(SND_STR_WORK* str);
void str_abort_init(SND_STR_WORK* str);
void str_abort_wait(SND_STR_WORK* str);
void str_req_check(SND_STR_WORK* str);
void str_req_to_ready(SND_STR_WORK* str);
void str_req_to_play(SND_STR_WORK* str);
void str_req_vol_set(SND_STR_WORK* str);
void str_req_nml_fade_set(SND_STR_WORK* str, s16 time, s16 vol);
int str_fade_check(SND_STR_WORK* str);
void str_fade_new_vol_set(SND_STR_WORK* str, s16 step, s16 target);
int str_reset_check(SND_STR_WORK* str);
void str_play_cancel(SND_STR_WORK* str);
void str_play_end(SND_STR_WORK* str);
void str_recovery_check(SND_STR_WORK* str);

// snd_str2.c
void Snd_str_dvd_read_sub(SND_STR_WORK* str);
void Snd_str_aram_dma_sub(SND_STR_WORK* str);
void cb_dvd_read_end(s32 result, DVDFileInfo* info);
void cb_aram_dma_end(u32 task);
void Snd_str_get_now_play_nbl(SND_STR_WORK* str);
void str_ax_voice_to_next_block(SND_STR_WORK* str);
void str_ax_voice_loop_to_top(SND_STR_WORK* str);
void str_ax_voice_loop_to_end(SND_STR_WORK* str);
void str_ax_voice_last_to_top(SND_STR_WORK* str);

// snd_str3.c
int Snd_str_ax_voice_init(SND_STR_WORK* str, s8 start, s8 num);
void str_ax_adrs_set_long(SND_STR_WORK* str);
void str_ax_adrs_set_short(SND_STR_WORK* str);
void str_ax_voice_para_set(SND_STR_WORK* str);
void cb_str_voice_drop(void* voice);
int str_secure_voice_work(SND_STR_WORK* str, s8 start, s8 num);
void Snd_str_ax_voice_play(SND_STR_WORK* str);
void Snd_str_ax_voice_stop(SND_STR_WORK* str);
void Snd_str_ax_voice_recovery(SND_STR_WORK* str);
void Snd_str_ax_voice_free(SND_STR_WORK* str);

// snd_str4.c
void Snd_str_work_clear(void);
SND_STR_WORK* Snd_search_str_work_snd_id(u32 snd_id);
void Snd_str_work_close_check(void);
void Snd_str_work_calc_ax_vol(SND_STR_WORK* str);
void Snd_str_work_choice_out_span(SND_STR_WORK* str);
void Snd_str_get_dvd_status(SND_STR_WORK* str);
void Snd_str_err_check(SND_STR_WORK* str);
void Snd_str_player_update(SND_STR_WORK* str);
int Snd_str_init_para(u32 snd_id, s16 flag, s16 val);
int Snd_str_init_pos(u32 snd_id, u32 pos);
u32 Snd_str_get_buff_smp(u16 blk_no, u16 req_no);
#endif  // SND_DRV_GAME_API

#ifdef __cplusplus
}
#endif

#endif
