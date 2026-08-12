// -*- coding: utf-8 -*-
// Son du coeur natif (phase C du plan, nj-work/docs/PLAN_NATIF_REPRISE_2026-11-12.md).
//
// La bibliotheque AX du SDK (src/lib/AX*.c) est compilee telle quelle : toutes les 5 ms, son rappel d'interruption AI
// (__AXOutAiCallback) prepare une image : il met a jour les blocs de voix, construit une liste de commandes et l'envoie
// au DSP par deux courriers (0xBABE0180 puis l'adresse). Ici :
//   - le « DSP » execute la liste tout de suite avec l'interpreteur AX de Nightjar (runtime/src/hle/ax.cpp, mode memoire
//     native) puis signale la fin de l'image (rappel de reprise) ;
//   - l'« AI » est une sortie OpenSL ES a 32 kHz stereo par tampons de 5 ms. Le fil audio ne fait que compter les
//     tampons consommes ; l'interruption AI est livree dans le fil du jeu, aux points d'interruption (OSDisableInterrupts,
//     comme le GX : port_gx_deliver), parce que les fils du jeu se passent un jeton et qu'aucun code du jeu ne doit
//     tourner sur le fil audio.
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <atomic>
#include <cstdio>
#include <cstring>

#include <dolphin.h>
#include <dolphin/dsp.h>
#include <dolphin/ai.h>

namespace nj::runtime::hle {
void ax_hle_command_list(uint32_t list_addr, uint32_t size_words);   // runtime/src/hle/ax.cpp
void ax_hle_set_native(const uint8_t* aram, uint32_t aram_size);
}
extern "C" void* port_aram_host(u32 addr, u32 length);   // port/aram.cpp

namespace {

// ------------------------------------------------------------------ sortie OpenSL ES
const int kFrames = 160;                 // 5 ms a 32 kHz
const int kRing = 16;                    // tampons d'avance (80 ms)
short g_ring[kRing][kFrames * 2];        // L, R entrelaces
std::atomic<unsigned> g_ring_w{0}, g_ring_r{0};
std::atomic<unsigned> g_ai_ticks{0};     // tampons consommes par la sortie, pas encore livres au jeu
short g_silence[kFrames * 2];

SLObjectItf g_engine_obj, g_mix_obj, g_player_obj;
SLEngineItf g_engine;
SLPlayItf g_play;
SLAndroidSimpleBufferQueueItf g_queue;
bool g_sl_ok;

void sl_callback(SLAndroidSimpleBufferQueueItf q, void*)
{
    const unsigned r = g_ring_r.load(std::memory_order_relaxed);
    if (r != g_ring_w.load(std::memory_order_acquire)) {
        (*q)->Enqueue(q, g_ring[r % kRing], sizeof g_ring[0]);
        g_ring_r.store(r + 1, std::memory_order_release);
    } else {
        (*q)->Enqueue(q, g_silence, sizeof g_silence);   // le jeu est en retard : silence plutot qu'un trou
    }
    g_ai_ticks.fetch_add(1, std::memory_order_release);
}

bool sl_start()
{
    if (g_sl_ok)
        return true;
    if (slCreateEngine(&g_engine_obj, 0, nullptr, 0, nullptr, nullptr) != SL_RESULT_SUCCESS)
        return false;
    (*g_engine_obj)->Realize(g_engine_obj, SL_BOOLEAN_FALSE);
    (*g_engine_obj)->GetInterface(g_engine_obj, SL_IID_ENGINE, &g_engine);
    (*g_engine)->CreateOutputMix(g_engine, &g_mix_obj, 0, nullptr, nullptr);
    (*g_mix_obj)->Realize(g_mix_obj, SL_BOOLEAN_FALSE);
    SLDataLocator_AndroidSimpleBufferQueue loc = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 4};
    SLDataFormat_PCM fmt = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_32, SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_PCMSAMPLEFORMAT_FIXED_16, SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                            SL_BYTEORDER_LITTLEENDIAN};
    SLDataSource src = {&loc, &fmt};
    SLDataLocator_OutputMix out = {SL_DATALOCATOR_OUTPUTMIX, g_mix_obj};
    SLDataSink sink = {&out, nullptr};
    const SLInterfaceID ids[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    if ((*g_engine)->CreateAudioPlayer(g_engine, &g_player_obj, &src, &sink, 1, ids, req) != SL_RESULT_SUCCESS)
        return false;
    (*g_player_obj)->Realize(g_player_obj, SL_BOOLEAN_FALSE);
    (*g_player_obj)->GetInterface(g_player_obj, SL_IID_PLAY, &g_play);
    (*g_player_obj)->GetInterface(g_player_obj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &g_queue);
    (*g_queue)->RegisterCallback(g_queue, sl_callback, nullptr);
    for (int i = 0; i < 4; ++i)   // amorce : 20 ms de silence, puis le rappel enchaine
        (*g_queue)->Enqueue(g_queue, g_silence, sizeof g_silence);
    (*g_play)->SetPlayState(g_play, SL_PLAYSTATE_PLAYING);
    g_sl_ok = true;
    std::printf("[AUDIO] sortie OpenSL ES : 32 kHz stereo, tampons de 5 ms\n");
    return true;
}

// ------------------------------------------------------------------ AI
AIDCallback g_ai_cb;
const short* g_ai_buf;      // tampon de sortie courant (ecrit par le DSP : paires R, L de s16 natifs)
u32 g_ai_len;
bool g_ai_running;

// ------------------------------------------------------------------ DSP
DSPTaskInfo* g_task;
int g_mail_state;           // 0 attente, 1 adresse de liste attendue
u32 g_list_words;
bool g_resume_pending;      // la liste a ete mixee : le DSP « rend la main » (rappel de reprise) apres la livraison
bool g_delivering;

void push_current_buffer()
{
    const unsigned w = g_ring_w.load(std::memory_order_relaxed);
    if (w - g_ring_r.load(std::memory_order_acquire) >= (unsigned) kRing)
        return;                                    // la sortie est pleine : on garde l'avance qu'on a
    short* d = g_ring[w % kRing];
    if (g_ai_buf && g_ai_len >= (u32) (kFrames * 4)) {
        for (int i = 0; i < kFrames; ++i) {        // R, L -> L, R
            d[2 * i] = g_ai_buf[2 * i + 1];
            d[2 * i + 1] = g_ai_buf[2 * i];
        }
    } else {
        std::memset(d, 0, sizeof g_ring[0]);
    }
    g_ring_w.store(w + 1, std::memory_order_release);
}

}  // namespace

extern "C" {

// Livree depuis OSDisableInterrupts (port/os.cpp) : les tampons consommes par la sortie deviennent des interruptions AI
// du jeu ; apres chaque image mixee, le rappel de reprise du DSP.
void port_audio_deliver(void)
{
    if (g_delivering || !g_ai_running)
        return;
    unsigned n = g_ai_ticks.exchange(0, std::memory_order_acq_rel);
    if (n == 0)
        return;
    if (n > 4)
        n = 4;                                     // le jeu a pris du retard : ne pas rattraper en rafale
    g_delivering = true;
    for (unsigned k = 0; k < n; ++k) {
        push_current_buffer();
        if (g_ai_cb)
            g_ai_cb();
        if (g_resume_pending && g_task) {
            g_resume_pending = false;
            if (g_task->res_cb)
                g_task->res_cb(g_task);
        }
    }
    g_delivering = false;
}

// --- AI
void AIInit(u8* stack) { (void) stack; }
void AIReset(void) {}
AIDCallback AIRegisterDMACallback(AIDCallback callback)
{
    AIDCallback old = g_ai_cb;
    g_ai_cb = callback;
    return old;
}
void AIInitDMA(u32 start_addr, u32 length)
{
    g_ai_buf = (const short*) (unsigned long) start_addr;
    g_ai_len = length;
}
void AIStartDMA(void)
{
    static bool aram_set = false;
    if (!aram_set) {
        aram_set = true;
        nj::runtime::hle::ax_hle_set_native((const uint8_t*) port_aram_host(0, 0x1000000u), 0x1000000u);
    }
    g_ai_running = sl_start();
}
void AIStopDMA(void) { g_ai_running = false; }
void AISetStreamVolLeft(u8 vol) { (void) vol; }
void AISetStreamVolRight(u8 vol) { (void) vol; }

// --- DSP
void DSPInit(void) {}
BOOL DSPCheckInit(void) { return TRUE; }
u32 DSPCheckMailToDSP(void) { return 0; }      // la boite est vide : le courrier a ete lu aussitot
DSPTaskInfo* DSPAddTask(DSPTaskInfo* task)
{
    g_task = task;
    if (task && task->init_cb)
        task->init_cb(task);                      // le microcode est « demarre » tout de suite
    return task;
}
DSPTaskInfo* DSPAssertTask(DSPTaskInfo* task)
{
    g_resume_pending = true;                      // le DSP rend la main a la prochaine livraison
    return task;
}
DSPTaskInfo* DSPCancelTask(DSPTaskInfo* task)
{
    if (task && task->done_cb)
        task->done_cb(task);
    if (task == g_task)
        g_task = nullptr;
    return task;
}
void DSPSendMailToDSP(u32 mail)
{
    if (g_mail_state == 1) {                      // l'adresse de la liste de commandes : on mixe l'image
        nj::runtime::hle::ax_hle_command_list(mail, g_list_words);
        g_mail_state = 0;
        g_resume_pending = true;
        return;
    }
    if ((mail >> 16) == 0xBABEu) {
        g_list_words = mail & 0xFFFFu;
        g_mail_state = 1;
    }
}

}  // extern "C"
