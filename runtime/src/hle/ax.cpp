// High-level emulation of the GameCube AX DSP ucode (the late variant with
// Dolby Pro Logic II aux paths and a compressor, as used by Resident Evil 4).
//
// The CPU side of AX (recompiled SDK code) keeps one parameter block (PB) per
// voice, builds a command list every 5 ms and mails its address to the DSP.
// The DSP mixes every running voice from ARAM into 32 kHz buffers and writes
// the stereo result where the command list says; the SDK then hands that
// buffer to the audio interface. Without this the output buffer stayed
// silent: the mail protocol alone kept the game running, but made no sound.
//
// Behaviour follows the documented ucode semantics (Dolphin's AX HLE was used
// as the reference for command meanings, buffer layout and sample decoding).

#include "runtime/cpu_context.h"
#include "runtime/log.h"
#include "runtime/game_version.h"
#include "runtime/env.h"
#include "runtime/perf_journal.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace nj::runtime {
extern MMU* g_guest_mem;
}

namespace nj::runtime::hle {

namespace {

constexpr int kSamplesPerMs = 32;
constexpr int kMsPerFrame = 5;
constexpr int kFrame = kSamplesPerMs * kMsPerFrame; // 160 samples

// Mixing buffers of the current frame, in DSP order.
enum Buf { MAIN_L, MAIN_R, MAIN_S, AUXA_L, AUXA_R, AUXA_S, AUXB_L, AUXB_R, AUXB_S, BUF_COUNT };
int32_t g_buf[BUF_COUNT][kFrame];
uint16_t g_compressor_pos = 0;

// NJ_AXTRACE: per-interval counters printed from ax_hle_command_list.
struct AxTrace {
    uint64_t lists = 0, pbs = 0, running = 0, outputs = 0;
    uint32_t last_lr = 0, last_surround = 0, last_pb_addr = 0;
    int32_t peak = 0;
};
AxTrace g_trace;
int g_stage_frames_left = 0; // lists whose per-command buffer peaks get printed
int g_spike_index = -1;      // main L sample index of an isolated spike being traced
int g_voice_dumps = 0;       // running voices described so far
bool g_dump_voice = false;   // describe the voice being mixed right now

bool ax_trace_on() {
    static const bool on = NJ_ENV("NJ_AXTRACE") != nullptr;
    return on;
}

// Parameter block: 244 bytes = 122 big-endian words. Word indices follow the
// layout the game's own AXSetVoice* setters and __AXSyncPBs write.
constexpr int kPBWords = 122;
enum PBWord {
    W_NEXT_HI = 0, W_NEXT_LO = 1,
    W_SRC_TYPE = 4, W_COEF_SELECT = 5, W_MIXER_CONTROL = 6, W_RUNNING = 7, W_IS_STREAM = 8,
    W_MIXER = 9,        // 18 words: volume/delta pairs, see kMixChannels
    W_ITD_ON = 27,      // initial time delay (7 words, not emulated)
    W_NUM_UPDATES = 34, // 5 words, then data hi/lo
    W_UPDATES_HI = 39, W_UPDATES_LO = 40,
    W_DPOP = 41,        // 9 words, as __AXDepopVoice reads them
    // Volume envelope right after the depop block: running voices carry their
    // volume in word 50 (e.g. 0x70c6) while words 53-54 stay zero. Taking the
    // envelope from 53 silenced every voice. Words 52-54 are unidentified.
    W_VE_CUR = 50, W_VE_DELTA = 51,
    W_LOOPING = 55, W_FORMAT = 56, W_LOOP_HI = 57, W_LOOP_LO = 58,
    W_END_HI = 59, W_END_LO = 60, W_CUR_HI = 61, W_CUR_LO = 62,
    W_COEFS = 63,       // 16 words
    W_GAIN = 79, W_PRED_SCALE = 80, W_YN1 = 81, W_YN2 = 82,
    W_RATIO_HI = 83, W_RATIO_LO = 84, W_FRAC = 85, W_LAST_SAMPLES = 86, // 4 words
    W_LOOP_PRED_SCALE = 90, W_LOOP_YN1 = 91, W_LOOP_YN2 = 92,
    W_LPF_ON = 93, W_LPF_YN1 = 94, W_LPF_A0 = 95, W_LPF_B0 = 96,
};

constexpr uint16_t kFormatADPCM = 0x0000;
constexpr uint16_t kFormatPCM16 = 0x000A;
constexpr uint16_t kFormatPCM8 = 0x0019;

struct MixChannel {
    Buf buf;
    int vol_word;       // offset in the mixer block
    uint16_t enable;    // mixer_control bit
    uint16_t ramp;      // mixer_control bit enabling the volume delta
    int dpop_word;      // offset in the depop block
};
// mixer_control bits of the late GC ucode; mixer block order is
// L, R, auxA L, auxA R, auxB L, auxB R, auxB S, S, auxA S.
constexpr std::array<MixChannel, 9> kMixChannels = {{
    {MAIN_L, 0, 0x0001, 0x0008, 0},  {MAIN_R, 2, 0x0002, 0x0008, 3},
    {MAIN_S, 14, 0x0004, 0x0008, 6}, {AUXA_L, 4, 0x0010, 0x0040, 1},
    {AUXA_R, 6, 0x0020, 0x0040, 4},  {AUXA_S, 16, 0x0080, 0x0100, 7},
    {AUXB_L, 8, 0x0200, 0x0800, 2},  {AUXB_R, 10, 0x0400, 0x0800, 5},
    {AUXB_S, 12, 0x1000, 0x2000, 8},
}};

// Coeur natif (port/audio_port.cpp, phase C) : la bibliotheque AX du jeu tourne en code natif ; ses blocs de voix,
// listes de commandes et tampons sont des structures en memoire du processus, dans l'ordre de la machine, a leurs
// adresses completes (statiques hors de l'arene comprises). L'ARAM est le tampon du portage (port/aram.cpp),
// gros-boutiste comme le disque.
bool g_ax_native = false;
const uint8_t* g_ax_aram = nullptr;
uint32_t g_ax_aram_size = 0;
// Coeur 64 bits (N7, 18/11) : les adresses du jeu sont des decalages dans une fenetre de 4 Go alignee ; base | adresse.
// En 32 bits la base reste 0 (adresse = pointeur).
uintptr_t g_ax_base = 0;
// iOS (19/11, coeur dans le processus du moteur) : deux bases choisies par le bit haut, comme port/ptr32.h. Les adresses
// GameCube (>= 0x80000000 : MEM1) -> g_ax_base ; le reste (listes de commandes et tampons dans les donnees statiques du
// programme) -> g_ax_img. Ailleurs les deux sont egales (ax_hle_set_base les pose ensemble).
uintptr_t g_ax_img = 0;
inline uintptr_t ax_at(uint32_t addr) { return ((addr & 0x80000000u) ? g_ax_base : g_ax_img) | addr; }

inline uint32_t va(uint32_t addr) { return (addr & 0x3FFFFFFF) | 0x80000000u; }
inline uint16_t rd16(uint32_t addr) {
    if (g_ax_native) { uint16_t v; std::memcpy(&v, reinterpret_cast<const void*>(ax_at(addr)), 2); return v; }
    return g_guest_mem->read16(va(addr));
}
inline uint32_t rd32(uint32_t addr) {
    if (g_ax_native) { uint32_t v; std::memcpy(&v, reinterpret_cast<const void*>(ax_at(addr)), 4); return v; }
    return g_guest_mem->read32(va(addr));
}
inline void wr16(uint32_t addr, uint16_t v) {
    if (g_ax_native) { std::memcpy(reinterpret_cast<void*>(ax_at(addr)), &v, 2); return; }
    g_guest_mem->write16(va(addr), v);
}
inline void wr32(uint32_t addr, uint32_t v) {
    if (g_ax_native) { std::memcpy(reinterpret_cast<void*>(ax_at(addr)), &v, 4); return; }
    g_guest_mem->write32(va(addr), v);
}

inline int16_t clamp16(int64_t v) {
    return (int16_t)std::clamp<int64_t>(v, -32768, 32767);
}

inline uint8_t aram8(uint32_t addr) {
    if (g_ax_native) {
        addr &= 0x00FFFFFF;
        return (g_ax_aram && addr < g_ax_aram_size) ? g_ax_aram[addr] : 0;
    }
    const auto& ram = g_guest_mem->mem2;
    addr &= 0x00FFFFFF;
    return addr < ram.size() ? ram[addr] : 0;
}

// Samples handed back by the CPU (aux effect returns, previous-frame buffers)
// are kept to the 16-bit range: RE4's reverb returned 0x80000000 (a NaN
// converted to int), which OUTPUT wrote to the surround buffer and SET_LR
// read back next frame, latching the mix at full scale -- a loud beep.
uint64_t g_sentinel_min = 0, g_sentinel_max = 0; // NJ_AXTRACE counters

inline int32_t read_cpu_sample(uint32_t addr) {
    const int32_t v = (int32_t)rd32(addr);
    // 0x80000000 and 0x7FFFFFFF are what a float-to-int conversion saturates
    // to (NaN or overflow), never real samples. Clamped, the first pinned the
    // output at -32768; the second, in the last surround sample of RE4's
    // reverb return, came back through SET_LR as a click every 5 ms.
    if (v == INT32_MIN) {
        ++g_sentinel_min;
        return 0;
    }
    if (v == INT32_MAX) {
        ++g_sentinel_max;
        return 0;
    }
    return std::clamp<int32_t>(v, -32768, 32767);
}

uint64_t g_cpu_frames_dropped = 0; // NJ_AXTRACE counter

// True when a block of `count` samples handed back by the CPU contains a
// float-to-int saturation value. RE4's reverb returned such blocks almost
// entirely (478 of 480 samples); the few remaining values were garbage too,
// and one of them clicked every 5 ms. Such a block is dropped as a whole.
bool cpu_block_invalid(uint32_t addr, int count) {
    for (int i = 0; i < count; ++i) {
        const int32_t v = (int32_t)rd32(addr + (uint32_t)i * 4);
        if (v == INT32_MIN || v == INT32_MAX) {
            ++g_cpu_frames_dropped;
            return true;
        }
    }
    return false;
}

void write_buffer(uint32_t addr, const int32_t* samples) {
    for (int i = 0; i < kFrame; ++i)
        wr32(addr + (uint32_t)i * 4, (uint32_t)samples[i]);
}

// ---------------------------------------------------------------------------
// Voice sample source (the DSP "accelerator": ARAM decode with loop/end).

int16_t read_raw_sample(std::array<uint16_t, kPBWords>& pb) {
    if (!pb[W_RUNNING])
        return 0;
    uint32_t cur = ((uint32_t)pb[W_CUR_HI] << 16) | pb[W_CUR_LO];
    const uint32_t end = ((uint32_t)pb[W_END_HI] << 16) | pb[W_END_LO];
    const uint16_t format = pb[W_FORMAT];
    int16_t sample;

    if (format == kFormatADPCM) {
        // Addresses count nibbles; every 8-byte frame starts with a
        // predictor/scale byte.
        if ((cur & 15) == 0) {
            pb[W_PRED_SCALE] = aram8(cur >> 1);
            cur += 2;
        }
        const uint8_t byte = aram8(cur >> 1);
        int nibble = (cur & 1) ? (byte & 0xF) : (byte >> 4);
        if (nibble >= 8)
            nibble -= 16;
        const uint16_t ps = pb[W_PRED_SCALE];
        const int idx = (ps >> 4) & 7;
        const int32_t c1 = (int16_t)pb[W_COEFS + idx * 2];
        const int32_t c2 = (int16_t)pb[W_COEFS + idx * 2 + 1];
        const int32_t yn1 = (int16_t)pb[W_YN1];
        const int32_t yn2 = (int16_t)pb[W_YN2];
        const int64_t predicted = (0x400 + (int64_t)c1 * yn1 + (int64_t)c2 * yn2) >> 11;
        sample = clamp16((int64_t)nibble * (1 << (ps & 0xF)) + predicted);
    } else if (format == kFormatPCM16) {
        sample = (int16_t)((aram8(cur * 2) << 8) | aram8(cur * 2 + 1));
    } else if (format == kFormatPCM8) {
        sample = (int16_t)((int8_t)aram8(cur) * 256);
    } else {
        sample = 0;
    }
    pb[W_YN2] = pb[W_YN1];
    pb[W_YN1] = (uint16_t)sample;

    // Past the end as well as on it: skipping an ADPCM frame header moves the
    // address by two nibbles, which can jump over an end address that sits on
    // a header, and the voice then played whatever followed in ARAM (a noisy
    // tail with near full-scale peaks where Dolphin is silent).
    if (cur >= end) {
        if (pb[W_LOOPING]) {
            cur = ((uint32_t)pb[W_LOOP_HI] << 16) | pb[W_LOOP_LO];
            if (format == kFormatADPCM) {
                pb[W_PRED_SCALE] = pb[W_LOOP_PRED_SCALE];
                if (!pb[W_IS_STREAM]) { // streams keep their running history
                    pb[W_YN1] = pb[W_LOOP_YN1];
                    pb[W_YN2] = pb[W_LOOP_YN2];
                }
            }
        } else {
            pb[W_RUNNING] = 0;
        }
    } else {
        ++cur;
    }
    pb[W_CUR_HI] = (uint16_t)(cur >> 16);
    pb[W_CUR_LO] = (uint16_t)cur;
    return sample;
}

// Resamples the voice to 32 kHz: ratio is 16.16, cur_addr_frac keeps the
// position between the last two decoded samples, last_samples the history.
void read_input_samples(std::array<uint16_t, kPBWords>& pb, int16_t* out, int count) {
    const uint32_t ratio = ((uint32_t)pb[W_RATIO_HI] << 16) | pb[W_RATIO_LO];
    uint32_t frac = pb[W_FRAC];
    int16_t hist[4];
    for (int i = 0; i < 4; ++i)
        hist[i] = (int16_t)pb[W_LAST_SAMPLES + i];
    const bool nearest = pb[W_SRC_TYPE] == 2;

    for (int i = 0; i < count; ++i) {
        frac += ratio;
        while (frac >= 0x10000) {
            hist[0] = hist[1];
            hist[1] = hist[2];
            hist[2] = hist[3];
            hist[3] = read_raw_sample(pb);
            frac -= 0x10000;
        }
        if (nearest)
            out[i] = hist[3];
        else
            out[i] = (int16_t)(hist[2] + (((int32_t)hist[3] - hist[2]) * (int32_t)frac >> 16));
    }
    pb[W_FRAC] = (uint16_t)frac;
    for (int i = 0; i < 4; ++i)
        pb[W_LAST_SAMPLES + i] = (uint16_t)hist[i];
}

// One millisecond of one voice into the mixing buffers at offset `at`.
void process_voice_ms(std::array<uint16_t, kPBWords>& pb, int at) {
    if (!pb[W_RUNNING])
        return;
    int16_t samples[kSamplesPerMs];
    read_input_samples(pb, samples, kSamplesPerMs);
    if (g_dump_voice) {
        int pk = 0;
        for (int s = 0; s < kSamplesPerMs; ++s)
            pk = std::max(pk, std::abs((int)samples[s]));
        NJ_LOG << "[AXTRACE]     decoded:";
        for (int s = 0; s < 8; ++s)
            NJ_LOG << " " << samples[s];
        NJ_LOG << " peak=" << pk << std::endl;
    }

    int32_t vol = (int16_t)pb[W_VE_CUR];
    const int32_t vol_delta = (int16_t)pb[W_VE_DELTA];
    for (int i = 0; i < kSamplesPerMs; ++i) {
        samples[i] = clamp16((int64_t)samples[i] * vol >> 15);
        vol += vol_delta;
    }
    pb[W_VE_CUR] = (uint16_t)vol;

    if (pb[W_LPF_ON]) {
        int32_t yn1 = (int16_t)pb[W_LPF_YN1];
        const int32_t a0 = pb[W_LPF_A0];
        const int32_t b0 = pb[W_LPF_B0];
        for (int i = 0; i < kSamplesPerMs; ++i) {
            yn1 = clamp16(((int64_t)a0 * samples[i] + (int64_t)b0 * yn1) >> 15);
            samples[i] = (int16_t)yn1;
        }
        pb[W_LPF_YN1] = (uint16_t)yn1;
    }

    const uint16_t control = pb[W_MIXER_CONTROL];
    for (const auto& ch : kMixChannels) {
        if (!(control & ch.enable))
            continue;
        int32_t v = pb[W_MIXER + ch.vol_word];
        const int32_t dv = (control & ch.ramp) ? (int16_t)pb[W_MIXER + ch.vol_word + 1] : 0;
        int16_t last = 0;
        int32_t* dst = g_buf[ch.buf] + at;
        for (int i = 0; i < kSamplesPerMs; ++i) {
            last = clamp16((int64_t)samples[i] * v >> 15);
            dst[i] += last;
            v += dv;
        }
        if (dv)
            pb[W_MIXER + ch.vol_word] = (uint16_t)v;
        pb[W_DPOP + ch.dpop_word] = (uint16_t)last;
    }
}

void process_pb_list(uint32_t pb_addr) {
    std::array<uint16_t, kPBWords> pb;
    for (int guard = 0; pb_addr && guard < 256; ++guard) {
        for (int w = 0; w < kPBWords; ++w)
            pb[w] = rd16(pb_addr + (uint32_t)w * 2);
        ++g_trace.pbs;
        if (pb[W_RUNNING]) {
            ++g_trace.running;
            nj::perf::g_ax_voices.fetch_add(1, std::memory_order_relaxed);
        }
        g_dump_voice = ax_trace_on() && pb[W_RUNNING] && g_voice_dumps < 12 && g_trace.lists > 3000;
        if (g_dump_voice) {
            ++g_voice_dumps;
            NJ_LOG << std::hex << "[AXTRACE] voice pb=0x" << pb_addr << " fmt=" << pb[W_FORMAT]
                      << " loop=" << pb[W_LOOPING] << " stream=" << pb[W_IS_STREAM]
                      << " cur=" << pb[W_CUR_HI] << ":" << pb[W_CUR_LO]
                      << " end=" << pb[W_END_HI] << ":" << pb[W_END_LO]
                      << " loopaddr=" << pb[W_LOOP_HI] << ":" << pb[W_LOOP_LO]
                      << " ratio=" << pb[W_RATIO_HI] << ":" << pb[W_RATIO_LO]
                      << " src=" << pb[W_SRC_TYPE] << " mixctl=" << pb[W_MIXER_CONTROL]
                      << " volL=" << pb[W_MIXER + 0] << " volR=" << pb[W_MIXER + 2]
                      << " volS=" << pb[W_MIXER + 14] << " ve=" << pb[W_VE_CUR] << "/" << pb[W_VE_DELTA]
                      << " ps=" << pb[W_PRED_SCALE] << " gain=" << pb[W_GAIN]
                      << " c0=" << pb[W_COEFS] << " c1=" << pb[W_COEFS + 1]
                      << " lpf=" << pb[W_LPF_ON] << " upd=" << pb[W_NUM_UPDATES] << std::dec << std::endl;
            // What ARAM holds at the voice's current byte, and how much of the
            // first 16 MB is non-zero (were samples ever uploaded?).
            const uint32_t cur_byte = ((((uint32_t)pb[W_CUR_HI] << 16) | pb[W_CUR_LO]) >> 1) & 0x00FFFFFF;
            NJ_LOG << "[AXTRACE]     aram@0x" << std::hex << cur_byte << ":";
            for (uint32_t k = 0; k < 12; ++k)
                NJ_LOG << " " << (int)aram8(cur_byte + k);
            static size_t nonzero = SIZE_MAX;
            if (nonzero == SIZE_MAX) {
                nonzero = 0;
                if (g_ax_native) {
                    for (size_t k = 0; k < g_ax_aram_size && k < 0x01000000; k += 16)
                        nonzero += g_ax_aram[k] != 0;
                } else {
                    const auto& ram = g_guest_mem->mem2;
                    for (size_t k = 0; k < ram.size() && k < 0x01000000; k += 16)
                        nonzero += ram[k] != 0;
                }
            }
            NJ_LOG << std::dec << " | aram nonzero (sampled 1/16 of 16MB)=" << nonzero << std::endl;
            // Show the word neighbourhood where the envelope may really live.
            NJ_LOG << "[AXTRACE]     w48..56:";
            for (int w = 48; w <= 56; ++w)
                NJ_LOG << " " << std::hex << pb[w];
            NJ_LOG << std::dec << std::endl;
        }

        // Parameter updates spread over the 5 ms: (word offset, value) pairs.
        const uint32_t upd_addr = ((uint32_t)pb[W_UPDATES_HI] << 16) | pb[W_UPDATES_LO];
        uint32_t upd_index = 0;
        for (int ms = 0; ms < kMsPerFrame; ++ms) {
            const uint16_t n = pb[W_NUM_UPDATES + ms];
            for (uint16_t k = 0; k < n && upd_addr; ++k, ++upd_index) {
                const uint16_t off = rd16(upd_addr + upd_index * 4);
                const uint16_t val = rd16(upd_addr + upd_index * 4 + 2);
                if (off < kPBWords)
                    pb[off] = val;
            }
            process_voice_ms(pb, ms * kSamplesPerMs);
        }

        g_dump_voice = false;
        for (int w = 0; w < kPBWords; ++w)
            wr16(pb_addr + (uint32_t)w * 2, pb[w]);
        pb_addr = ((uint32_t)pb[W_NEXT_HI] << 16) | pb[W_NEXT_LO];
    }
}

void setup_processing(uint32_t addr) {
    static const bool trace = NJ_ENV("NJ_AXTRACE") != nullptr;
    // First 3 setups, then any non-zero setup every 400 lists (the depop
    // accumulators: a value that never decays shows up here as a DC offset).
    static int dumped = 0;
    static uint64_t calls = 0;
    ++calls;
    bool nonzero = false;
    for (int w = 0; w < BUF_COUNT * 3 && !nonzero; ++w)
        nonzero = rd16(addr + (uint32_t)w * 2) != 0;
    if (trace && (dumped < 3 || (nonzero && (calls % 400) == 0))) {
        ++dumped;
        NJ_LOG << "[AXTRACE] SETUP at 0x" << std::hex << addr << ":";
        for (int w = 0; w < 36; ++w)
            NJ_LOG << " " << rd16(addr + (uint32_t)w * 2);
        NJ_LOG << std::dec << std::endl;
    }
    for (int b = 0; b < BUF_COUNT; ++b) {
        const int32_t init = (int32_t)(((uint32_t)rd16(addr) << 16) | rd16(addr + 2));
        const int32_t delta = (int16_t)rd16(addr + 4);
        addr += 6;
        if (init == 0) {
            std::memset(g_buf[b], 0, sizeof(g_buf[b]));
        } else {
            for (int i = 0; i < kFrame; ++i)
                g_buf[b][i] = init + delta * i;
        }
    }
}

void mix_aux(int aux, uint32_t write_addr, uint32_t read_addr) {
    const Buf base = aux == 0 ? AUXA_L : AUXB_L;
    if (write_addr) {
        for (int c = 0; c < 3; ++c)
            write_buffer(write_addr + (uint32_t)(c * kFrame * 4), g_buf[base + c]);
    }
    if (cpu_block_invalid(read_addr, 3 * kFrame))
        return;
    for (int c = 0; c < 3; ++c)
        for (int i = 0; i < kFrame; ++i)
            g_buf[MAIN_L + c][i] += read_cpu_sample(read_addr + (uint32_t)((c * kFrame + i) * 4));
}

void run_compressor(uint16_t threshold, uint16_t release_frames, uint32_t table_addr) {
    bool triggered = false;
    for (int i = 0; i < kFrame && !triggered; ++i)
        triggered = std::abs(g_buf[MAIN_L][i]) > (int)threshold ||
                    std::abs(g_buf[MAIN_R][i]) > (int)threshold;
    constexpr uint32_t kFrameBytes = kFrame * 2;
    constexpr uint32_t kAttackEntries = 11;
    uint32_t table_offset;
    if (triggered) {
        table_offset = g_compressor_pos * kFrameBytes;
        g_compressor_pos = release_frames;
    } else if (g_compressor_pos) {
        --g_compressor_pos;
        table_offset = (kAttackEntries + g_compressor_pos) * kFrameBytes;
    } else {
        return;
    }
    for (int i = 0; i < kFrame; ++i) {
        const int64_t coef = rd16(table_addr + table_offset + (uint32_t)i * 2);
        g_buf[MAIN_L][i] = (int32_t)((g_buf[MAIN_L][i] * coef) >> 15);
        g_buf[MAIN_R][i] = (int32_t)((g_buf[MAIN_R][i] * coef) >> 15);
    }
}

// NJ_AXTRACE: the last word of each LR output buffer as this mixer left it,
// checked again at the start of the next command list.
struct TailWatch {
    uint32_t addr = 0;
    uint32_t written = 0;
};
std::array<TailWatch, 4> g_tails;

void check_tails(uint64_t list_no) {
    static int reports = 0;
    for (auto& t : g_tails) {
        if (!t.addr || reports >= 12)
            continue;
        const uint32_t now = rd32(t.addr);
        if (now != t.written) {
            ++reports;
            NJ_LOG << "[AXTRACE] TAIL CHANGED 0x" << std::hex << t.addr << ": wrote 0x" << t.written
                      << " now 0x" << now << std::dec << " (before list " << list_no << ")" << std::endl;
            t.written = now;
        }
    }
}

void output_samples(uint32_t lr_addr, uint32_t surround_addr) {
    ++g_trace.outputs;
    g_trace.last_lr = lr_addr;
    g_trace.last_surround = surround_addr;
    int32_t frame_peak = 0;
    for (int i = 0; i < kFrame; ++i)
        frame_peak = std::max({frame_peak, std::abs(g_buf[MAIN_L][i]), std::abs(g_buf[MAIN_R][i])});
    g_trace.peak = std::max(g_trace.peak, frame_peak);
    // Isolated full-scale sample on main L (neighbours near silence): report
    // its index once and follow it through the commands of the next frames.
    static int spike_reports = 0;
    if (ax_trace_on() && spike_reports < 3) {
        // Every index, including the last sample of the frame (where the
        // click actually sits): compare only with the neighbours that exist.
        for (int i = 0; i < kFrame; ++i) {
            const bool quiet_before = i == 0 || std::abs(g_buf[MAIN_L][i - 1]) < 1000;
            const bool quiet_after = i + 1 == kFrame || std::abs(g_buf[MAIN_L][i + 1]) < 1000;
            if (std::abs(g_buf[MAIN_L][i]) > 15000 && quiet_before && quiet_after) {
                ++spike_reports;
                NJ_LOG << "[AXTRACE] SPIKE main L[" << i << "]=" << g_buf[MAIN_L][i]
                          << " R=" << g_buf[MAIN_R][i] << " S=" << g_buf[MAIN_S][i]
                          << " at list " << g_trace.lists << std::endl;
                g_spike_index = i;
                g_stage_frames_left = 2;
                break;
            }
        }
    }
    static int bad_reports = 0;
    if (ax_trace_on() && frame_peak > 1000000 && bad_reports < 3) {
        ++bad_reports;
        NJ_LOG << "[AXTRACE] BAD FRAME at list " << g_trace.lists << " peaks:";
        for (int b = 0; b < BUF_COUNT; ++b) {
            int32_t pk = 0;
            for (int s = 0; s < kFrame; ++s)
                pk = std::max(pk, std::abs(g_buf[b][s]));
            NJ_LOG << " " << pk;
        }
        NJ_LOG << " mainL[0..3]=" << g_buf[MAIN_L][0] << "," << g_buf[MAIN_L][1] << ","
                  << g_buf[MAIN_L][2] << "," << g_buf[MAIN_L][3] << std::endl;
        g_stage_frames_left = 2;
    }
    write_buffer(surround_addr, g_buf[MAIN_S]);
    for (int i = 0; i < kFrame; ++i) {
        wr16(lr_addr + (uint32_t)i * 4, (uint16_t)clamp16(g_buf[MAIN_R][i]));
        wr16(lr_addr + (uint32_t)i * 4 + 2, (uint16_t)clamp16(g_buf[MAIN_L][i]));
    }
    if (ax_trace_on()) {
        // Read the last left sample back from guest memory right after the
        // write and compare it with the mixing buffer it came from.
        static int readback_reports = 0;
        const int16_t rb = (int16_t)rd16(lr_addr + (kFrame - 1) * 4 + 2);
        if (std::abs((int)rb) > 15000 && readback_reports < 5) {
            ++readback_reports;
            NJ_LOG << "[AXTRACE] OUT L159 readback=" << rb << " lr=0x" << std::hex << lr_addr
                      << std::dec << " g_buf L[157..159]=" << g_buf[MAIN_L][157] << ","
                      << g_buf[MAIN_L][158] << "," << g_buf[MAIN_L][159]
                      << " R[159]=" << g_buf[MAIN_R][159] << " S[159]=" << g_buf[MAIN_S][159]
                      << " list " << g_trace.lists << std::endl;
        }
        const uint32_t tail = lr_addr + (kFrame - 1) * 4;
        TailWatch* slot = nullptr;
        for (auto& t : g_tails)
            if (t.addr == tail || (!slot && t.addr == 0))
                slot = &t;
        if (slot) {
            slot->addr = tail;
            slot->written = rd32(tail);
        }
    }
}

} // namespace

// Executes one AX command list (addr, size in words) as the DSP would.
// NJ_SNDDBG=1 : les compteurs de voix **du jeu**, pas les notres.
//
// Snd_ctrl_work = .bss:0x802A10C0 (config/G4BE08/symbols.txt), et
// include/snd_drv.h donne la disposition : voice_num 0x7E, seq_num 0x8A,
// total_num 0x8E, dsp_cycles 0x78. Savoir ce que le jeu croit avoir comme
// voix, et le comparer a ce que l'AX en voit, situe le blocage d'un cote ou
// de l'autre de la frontiere - au lieu de le supposer.
static void snd_probe() {
    static const bool on = NJ_ENV("NJ_SNDDBG") != nullptr;
    if (!on || !g_guest_mem)
        return;
    static std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - t0).count() < 2)
        return;
    t0 = now;
    const uint32_t b = nj::runtime::ga(0x802A10C0);
    NJ_LOG << "[SND] voix=" << g_guest_mem->read16(b + 0x7E)
              << " sequenceur=" << g_guest_mem->read16(b + 0x8A)
              << " flux=" << g_guest_mem->read16(b + 0x86)
              << " total=" << g_guest_mem->read16(b + 0x8E)
              << " cycles_dsp=" << g_guest_mem->read32(b + 0x78)
              << std::endl;
}

// Coeur natif : bascule le lecteur en memoire native (voir rd16) et lui donne l'ARAM du portage.
void ax_hle_set_native(const uint8_t* aram, uint32_t aram_size) {
    g_ax_native = true;
    g_ax_aram = aram;
    g_ax_aram_size = aram_size;
}

void ax_hle_set_base(uintptr_t base) { g_ax_base = base; g_ax_img = base; }
void ax_hle_set_img_base(uintptr_t base) { g_ax_img = base; }

void ax_hle_command_list(uint32_t list_addr, uint32_t size_words) {
    if (!g_ax_native)
        snd_probe();
    static const bool disabled = NJ_ENV("NJ_NOAXHLE") != nullptr;
    if (disabled || (!g_guest_mem && !g_ax_native) || size_words == 0)
        return;
    std::vector<uint16_t> list(size_words);
    for (uint32_t i = 0; i < size_words; ++i)
        list[i] = rd16(list_addr + i * 2);

    static const bool trace = NJ_ENV("NJ_AXTRACE") != nullptr;
    if (trace) {
        check_tails(g_trace.lists + 1);
        ++g_trace.lists;
        nj::perf::g_ax_blocks.fetch_add(1, std::memory_order_relaxed);
        if (g_trace.lists == 1 || g_trace.lists == 400) {
            NJ_LOG << "[AXTRACE] list #" << g_trace.lists << " at 0x" << std::hex << list_addr
                      << " words=" << std::dec << size_words << ":";
            for (uint32_t k = 0; k < size_words && k < 48; ++k)
                NJ_LOG << " " << std::hex << list[k];
            NJ_LOG << std::dec << std::endl;
        }
        if ((g_trace.lists % 400) == 0) {
            NJ_LOG << "[AXTRACE] lists=" << g_trace.lists << " pbs=" << g_trace.pbs
                      << " running=" << g_trace.running << " outputs=" << g_trace.outputs
                      << " lr=0x" << std::hex << g_trace.last_lr << " surround=0x"
                      << g_trace.last_surround << std::dec << " peak=" << g_trace.peak
                      << " cpu_nan_min=" << g_sentinel_min << " cpu_sat_max=" << g_sentinel_max
                      << " cpu_blocks_dropped=" << g_cpu_frames_dropped << std::endl;
            g_sentinel_min = g_sentinel_max = g_cpu_frames_dropped = 0;
            g_trace.pbs = g_trace.running = g_trace.outputs = 0;
            g_trace.peak = 0;
        }
    }

    auto arg = [&](size_t& i) -> uint16_t { return i < list.size() ? list[i++] : 0; };
    auto addr32 = [&](size_t& i) -> uint32_t {
        const uint32_t hi = arg(i);
        return (hi << 16) | arg(i);
    };

    // NJ_AXTRACE: peak of every mixing buffer after each command, for the
    // first frames that actually run voices (finds where values blow up).
    const bool stage_trace = trace && g_stage_frames_left > 0;
    if (stage_trace)
        --g_stage_frames_left;
    auto stage = [&](uint16_t cmd) {
        if (!stage_trace)
            return;
        NJ_LOG << "[AXTRACE]   after cmd " << std::hex << cmd << std::dec << ":";
        for (int b = 0; b < BUF_COUNT; ++b) {
            int32_t pk = 0;
            for (int s = 0; s < kFrame; ++s)
                pk = std::max(pk, std::abs(g_buf[b][s]));
            NJ_LOG << " " << pk;
        }
        if (g_spike_index >= 0)
            NJ_LOG << " | L[" << g_spike_index << "]=" << g_buf[MAIN_L][g_spike_index]
                      << " S[" << g_spike_index << "]=" << g_buf[MAIN_S][g_spike_index];
        NJ_LOG << std::endl;
    };

    uint32_t pb_addr = 0;
    size_t i = 0;
    for (int guard = 0; guard < 4096 && i < list.size(); ++guard) {
        const uint16_t cmd = list[i++];
        // Trace the buffers once this command has run (its case breaks out
        // of the switch; END and unknown commands return before this).
        struct StageAfter {
            decltype(stage)& fn;
            uint16_t cmd;
            ~StageAfter() { fn(cmd); }
        } stage_after{stage, cmd};
        switch (cmd) {
        case 0x00: setup_processing(addr32(i)); break;
        case 0x01: { // download and mix with volume
            uint32_t addr = addr32(i);
            const uint16_t vols[3] = {arg(i), arg(i), arg(i)};
            for (int g = 0; g < 3; ++g) {
                for (int c = 0; c < 3; ++c) {
                    for (int s = 0; s < kFrame; ++s) {
                        const int64_t v = (int32_t)rd32(addr);
                        addr += 4;
                        g_buf[g * 3 + c][s] += (int32_t)((v * vols[g]) >> 15);
                    }
                }
            }
            break;
        }
        case 0x02: pb_addr = addr32(i); break;
        case 0x03: process_pb_list(pb_addr); break;
        case 0x04:
        case 0x05: {
            const uint32_t w = addr32(i);
            const uint32_t r = addr32(i);
            mix_aux(cmd - 0x04, w, r);
            break;
        }
        case 0x06: { // upload main L/R/S
            const uint32_t a = addr32(i);
            for (int c = 0; c < 3; ++c)
                write_buffer(a + (uint32_t)(c * kFrame * 4), g_buf[MAIN_L + c]);
            break;
        }
        case 0x07: { // set main L/R from one buffer
            const uint32_t a = addr32(i);
            if (stage_trace) {
                int64_t mx = 0;
                for (int s = 0; s < kFrame; ++s)
                    mx = std::max<int64_t>(mx, std::abs((int64_t)(int32_t)rd32(a + (uint32_t)s * 4)));
                NJ_LOG << "[AXTRACE]   SET_LR src 0x" << std::hex << a << std::dec << " max=" << mx
                          << " first=" << (int32_t)rd32(a) << "," << (int32_t)rd32(a + 4) << ","
                          << (int32_t)rd32(a + 8) << "," << (int32_t)rd32(a + 12) << std::endl;
            }
            for (int s = 0; s < kFrame; ++s) {
                const int32_t v = read_cpu_sample(a + (uint32_t)s * 4);
                g_buf[MAIN_L][s] = v;
                g_buf[MAIN_R][s] = v;
                g_buf[MAIN_S][s] = 0;
            }
            break;
        }
        case 0x08: i += 10; break;
        case 0x09: mix_aux(1, 0, addr32(i)); break;
        case 0x0A: case 0x0B: case 0x0C: break;
        case 0x0D: { // continue in another list
            const uint32_t a = addr32(i);
            const uint16_t n = arg(i);
            list.assign(n, 0);
            for (uint16_t k = 0; k < n; ++k)
                list[k] = rd16(a + (uint32_t)k * 2);
            i = 0;
            break;
        }
        case 0x0E: {
            const uint32_t surround = addr32(i);
            const uint32_t lr = addr32(i);
            output_samples(lr, surround);
            break;
        }
        case 0x0F: return;
        case 0x10: { // aux B L/R: upload, then take them back and mix into main
            const uint32_t ul = addr32(i);
            const uint32_t dl = addr32(i);
            write_buffer(ul, g_buf[AUXB_L]);
            write_buffer(ul + kFrame * 4, g_buf[AUXB_R]);
            if (cpu_block_invalid(dl, 2 * kFrame))
                break;
            for (int s = 0; s < kFrame; ++s) {
                const int32_t l = read_cpu_sample(dl + (uint32_t)s * 4);
                const int32_t r = read_cpu_sample(dl + (uint32_t)(kFrame + s) * 4);
                g_buf[AUXB_L][s] = l;
                g_buf[MAIN_L][s] += l;
                g_buf[AUXB_R][s] = r;
                g_buf[MAIN_R][s] += r;
            }
            break;
        }
        case 0x11: { // set main L/R from one buffer, left inverted
            const uint32_t a = addr32(i);
            for (int s = 0; s < kFrame; ++s) {
                const int32_t v = read_cpu_sample(a + (uint32_t)s * 4);
                g_buf[MAIN_L][s] = -v;
                g_buf[MAIN_R][s] = v;
                g_buf[MAIN_S][s] = 0;
            }
            break;
        }
        case 0x12: {
            const uint16_t threshold = arg(i);
            const uint16_t frames = arg(i);
            run_compressor(threshold, frames, addr32(i));
            break;
        }
        case 0x13: { // send aux A L/R/S and aux B S, mix returned main and aux B
            const uint32_t auxa_up = addr32(i);
            const uint32_t auxb_s_up = addr32(i);
            const uint32_t dl[4] = {addr32(i), addr32(i), addr32(i), addr32(i)};
            for (int c = 0; c < 3; ++c)
                write_buffer(auxa_up + (uint32_t)(c * kFrame * 4), g_buf[AUXA_L + c]);
            write_buffer(auxb_s_up, g_buf[AUXB_S]);
            const Buf targets[4] = {MAIN_L, MAIN_R, AUXB_L, AUXB_R};
            for (int t = 0; t < 4; ++t) {
                if (cpu_block_invalid(dl[t], kFrame))
                    continue;
                for (int s = 0; s < kFrame; ++s)
                    g_buf[targets[t]][s] += read_cpu_sample(dl[t] + (uint32_t)s * 4);
            }
            break;
        }
        default: {
            static int unknown_logged = 0;
            if (unknown_logged++ < 8)
                NJ_LOG << "[AX HLE] unknown command 0x" << std::hex << cmd
                          << " in list at 0x" << list_addr << std::dec << std::endl;
            return;
        }
        }
    }
}

} // namespace nj::runtime::hle
