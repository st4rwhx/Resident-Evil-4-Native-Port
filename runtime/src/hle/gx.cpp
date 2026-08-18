#include "common/gl.h"
#include <algorithm>
#include "runtime/gx/chunk_adapter.h"
#include "runtime/native/native_core.h"
#include "runtime/dev_tour.h"
#include "runtime/log.h"
#include "runtime/game_version.h"
#include "runtime/host_clock.h"
#include <unordered_map>
#include <cstdio>

#include "runtime/env.h"
#include "runtime/njprof.h"
#include "runtime/thread_place.h"
#include "runtime/gfx_settings.h"
#include <atomic>
#include <chrono>
#include <SDL.h>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "runtime/cpu_context.h"
#include "runtime/gx/fifo_parser.h"
#include "runtime/gx/frame_interp.h"
#include "runtime/gx/frame_renderer.h"
#include "runtime/gx_state.h"
#include "runtime/hw/hw.h"
#include "runtime/perf_journal.h"
#include "runtime/report.h"
#include "common/gl.h"

using namespace nj::runtime;
using namespace nj::runtime::gx;

namespace nj::runtime {
extern GuestCpu *g_guest;
extern MMU *g_guest_mem;
}  // namespace nj::runtime

// Registres GX tels que le flux les a poses (lus par le rendu et les raccourcis de gx_direct).
GxRegs nj::runtime::gx::g_gxregs = {};

// Cote hote : la fenetre (posee par host_video ou le rejoueur) et le rendu, cree a la premiere
// image qui trouve une fenetre.
namespace {
void *s_host_window = nullptr;
std::unique_ptr<FrameRenderer> g_renderer;
}  // namespace

void gx_bind_window(void *w) { s_host_window = w; }

std::mutex g_fifo_lock;
std::vector<uint8_t> g_fifo_bytes;
// Taille courante du flux, lue sans verrou par la contre-pression.
std::atomic<size_t> g_fifo_len{0};
// Bytes ever pushed to g_fifo_bytes: a stream position the parser can match.
static uint64_t g_fifo_pushed = 0;

// Memory the game flushed for the GPU (DC seams below), copied at the flush
// and tagged with the stream position it applies from. Guarded by
// g_fifo_lock; handed to the parser by gx_pump_fifo.
struct PendingFlush {
    uint64_t pos;
    uint32_t addr;
    uint32_t len;
    std::shared_ptr<const std::vector<uint8_t>> bytes;  // null: tracked only
};
static std::vector<PendingFlush> g_volatile_pending;
static std::atomic<uint64_t> g_prof_vol_calls{0}, g_prof_vol_bytes{0};

// Listes d'affichage photographiees a l'appel (GXCallDisplayList) : le FIFO n'est analyse que bien
// plus tard, et le jeu a pu reecrire entre-temps ces tampons (il les double) ; l'analyseur prend la
// photo quand elle existe. Les photos sont partagees, pas recopiees : ~9 000 listes par seconde en
// jeu (~32 Mo), dont la recopie pesait lourd sur l'allocateur du fil de rendu.
namespace {
struct DisplayListPhotos {
    using Bytes = std::shared_ptr<const std::vector<uint8_t>>;
    static uint64_t key(uint32_t addr, uint32_t size) { return ((uint64_t)(addr & 0x03FFFFFF) << 24) ^ size; }
    std::mutex mu;
    std::unordered_map<uint64_t, Bytes> photos;
};
DisplayListPhotos g_dl;
}  // namespace

namespace nj::runtime {
std::shared_ptr<const std::vector<uint8_t>> GX_DL_Fetch(uint32_t addr, uint32_t size) {
    std::lock_guard<std::mutex> lk(g_dl.mu);
    const auto found = g_dl.photos.find(DisplayListPhotos::key(addr, size));
    return found != g_dl.photos.end() && found->second->size() == size ? found->second : nullptr;
}
}  // namespace nj::runtime


// Commands of the frame still being received. Each frame is taken off the front.
//
// Un vector efface par le debut deplacait tout le reste (~30k commandes) a chaque image ; un
// deque, lui, allouait un bloc de 512 octets toutes les deux commandes (une GxOp fait ~200
// octets) : 30 % des allocations du fil hote au releve NJ_SHAPE du 27/09. File a indice de
// tete sur un vector : la capacite reste d'une image a l'autre, et on ne compacte que quand la
// tete depasse le reste (qui n'est que le debut de l'image suivante).
namespace {
class CarryQueue {
public:
    size_t size() const { return m_v.size() - m_head; }
    nj::runtime::gx::GxOp& operator[](size_t i) { return m_v[m_head + i]; }
    template <class It> void append(It first, It last) { m_v.insert(m_v.end(), first, last); }
    // Ajout d'un lot entier : file vide, on echange les vecteurs (rien n'est deplace).
    void append_all(std::vector<nj::runtime::gx::GxOp>& v) {
        if (m_v.empty()) {
            m_v.swap(v);
            m_head = 0;
            v.clear();
        } else {
            m_v.insert(m_v.end(), std::make_move_iterator(v.begin()), std::make_move_iterator(v.end()));
            v.clear();
        }
    }
    // Les n premieres commandes passent dans out (vide), dans l'ordre. Tete a 0 : on echange les
    // vecteurs et on ne rend a la file que la queue (le debut de l'image suivante, quelques
    // commandes), au lieu de deplacer une a une les ~20 000 commandes de l'image. Au Galaxy S5
    // (echantillons de pile du 30/09), ces deplacements occupaient ~40 % du fil de rendu.
    void take_prefix(size_t n, std::vector<nj::runtime::gx::GxOp>& out) {
        if (m_head != 0) {
            for (size_t i = 0; i < n; ++i) out.push_back(std::move(m_v[m_head + i]));
            pop_front(n);
            return;
        }
        out.swap(m_v);   // out : toute la file ; m_v : l'ancien stockage de out (vide)
        m_v.clear();
        m_v.insert(m_v.end(), std::make_move_iterator(out.begin() + n), std::make_move_iterator(out.end()));
        out.erase(out.begin() + n, out.end());
    }
    void pop_front(size_t n) {
        m_head += n;
        const size_t rest = m_v.size() - m_head;
        if (rest == 0) {
            m_v.clear();
            m_head = 0;
        } else if (m_head >= rest) {
            std::move(m_v.begin() + m_head, m_v.end(), m_v.begin());
            m_v.erase(m_v.begin() + rest, m_v.end());
            m_head = 0;
        }
    }
    // Sans fin d'image (video : aucune copie EFB -> XFB), la file ne se vide plus. On jette les
    // dessins (leurs sommets decodes font le poids) de tout sauf les `keep` dernieres commandes ;
    // les ecritures de registres restent, dans l'ordre : l'etat du GX apres la video est le bon.
    void drop_old_draws(size_t keep) {
        const size_t n = size();
        if (n <= keep) return;
        std::vector<nj::runtime::gx::GxOp> kept;
        kept.reserve(n / 8 + keep);
        for (size_t i = 0; i < n - keep; ++i)
            if (m_v[m_head + i].type != nj::runtime::gx::GxOpKind::DrawPrimitive)
                kept.push_back(std::move(m_v[m_head + i]));
        for (size_t i = n - keep; i < n; ++i) kept.push_back(std::move(m_v[m_head + i]));
        m_v.swap(kept);
        m_head = 0;
    }
private:
    std::vector<nj::runtime::gx::GxOp> m_v;
    size_t m_head = 0;
};
} // namespace
static CarryQueue s_carry;

uint64_t g_st_frames = 0;
uint64_t g_stat_draws = 0;


uint64_t g_st_parse_us = 0;
// Attente du pool de decodage, mesuree separement (voir gx_pump_fifo).
uint64_t g_prof_pool_us = 0;
// Temps d analyse accumule **pour la fenetre de GXPROF**. g_st_parse_us ne
// pouvait pas servir : il est remis a zero par la ligne [STAT], toutes les cinq
// secondes, alors que GXPROF imprime toutes les deux - les deux compteurs ne
// couvraient donc pas la meme periode et pool_ms pouvait depasser le total
// cense le contenir. Toute comparaison entre eux etait fausse.
uint64_t g_prof_parse_us = 0;
uint64_t g_st_render_us = 0;

// NJ_FRAMELOG=<s>[,<n>] : a partir de s secondes, une ligne [FRAME] par image du jeu (n images,
// 600 par defaut) : duree depuis l'image precedente, analyse du flux, rendu, temps bloque dans
// l'echange de tampons (main.cpp), et le temps du fil CPU reparti entre « le jeu dort »
// (boucle d'inactivite de SelectThread), « le jeu attend la vsync » (boucles de re4_main) et le
// reste, qui est le calcul du jeu (ios.cpp, drain_callbacks). Plus les GXDrawDone servis.
std::atomic<uint64_t> g_fl_idle_us{0}, g_fl_vspin_us{0}, g_fl_drawdones{0};
// Copies EFB -> XFB vues par le fil de lecture : une par image du jeu. Avec NJ_HEADLESS (pas de
// rendu, donc pas de [FRAME]), la ligne [GXPROF] en tire le calcul du jeu par image (30/09).
std::atomic<uint64_t> g_fl_xfb_copies{0};
uint64_t g_fl_parse_us = 0, g_fl_swap_us = 0, g_fl_swaps = 0;
bool framelog_active() {
    static const char* env = NJ_ENV("NJ_FRAMELOG");
    if (!env) return false;
    static const auto t0 = std::chrono::steady_clock::now();
    static const double after = std::atof(env);
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() >= after;
}
bool framelog_enabled() {
    static const bool on = NJ_ENV("NJ_FRAMELOG") != nullptr;
    return on;
}

void gx_clear_rgba(unsigned char rgba[4]) {
    rgba[0] = (unsigned char)(g_gxregs.clear[0] & 0xFF);
    rgba[1] = (unsigned char)(g_gxregs.clear[1] >> 8);
    rgba[2] = (unsigned char)(g_gxregs.clear[1] & 0xFF);
    rgba[3] = (unsigned char)(g_gxregs.clear[0] >> 8);
}

void gx_xfb_info(uint32_t* addr, unsigned* w, unsigned* h, unsigned* stride) {
    *addr   = g_gxregs.xfb.addr;
    *w      = g_gxregs.xfb.w;
    *h      = g_gxregs.xfb.h;
    {   // jeu natif : le rendu est dans le coeur, qui envoie cette taille avec chaque image
        unsigned nw = 0, nh = 0;
        if (nj::native::xfb_size(nw, nh)) { *w = nw; *h = nh; }
    }
    *stride = g_gxregs.xfb.stride;
}

namespace nj::runtime::gx {
extern std::atomic<uint64_t> g_prof_decoded_verts;
extern std::atomic<uint64_t> g_prof_vtx_hit, g_prof_vtx_miss, g_prof_vtx_hit_verts;
extern std::atomic<uint64_t> g_prof_xf_bank, g_prof_xf_single, g_prof_xf_none;
extern std::atomic<uint64_t> g_prof_flush, g_prof_flush_verts, g_prof_flush_prim, g_prof_flush_mtx;
extern std::atomic<uint64_t> g_prof_mtx_frames, g_prof_mtx_sum, g_prof_mtx_max, g_prof_mtx_draws, g_prof_mtx_changes;
void xfstat_frame_end();
void FlushDeferredPeSignals();
void SetParseStreamBase(uint64_t base);
void QueueVolatileRange(uint64_t pos, uint32_t addr, uint32_t len,
                        std::shared_ptr<const std::vector<uint8_t>> bytes);
extern std::atomic<uint64_t> g_prof_vol_draws;
}

// Draw decode worker pool. Each draw decodes independently (it reads its own
// raw bytes and guest RAM, and writes only its own vertices), and in RE4
// gameplay decoding was about half of the render thread's time. The render
// thread hands out the frame's draws through an atomic index and decodes
// alongside the workers. NJ_DECODE_THREADS sets the worker count (default
// 2, 0 on Windows ; 0 = decode on the parse thread only).
namespace nj::runtime::gx { void DecodeDrawsPipelined(std::vector<GxOp*>& draws); }  // fifo_parser.cpp
namespace {
// 20/10 : le pool de decodage s'allume tout seul quand le jeu est en retard (image moyenne > 37 ms) : dans les salles de nuit le fil de lecture est le chemin
// critique (analyse 17-23 ms dont 7-8 ms de decodage) alors que les autres coeurs sont a 30-45 %. Hors retard, pas de fils (mesure du 16/10 : le pool coute
// +15 a +27 points de coeur sans gain quand le jeu tient deja 30 i/s). Les fils sont crees a la premiere demande.
std::atomic<bool> g_decode_behind{false};
class DecodePool {
public:
    static DecodePool& get() { static DecodePool pool; return pool; }

    void run(std::vector<nj::runtime::gx::GxOp*>& draws) {
        nj::runtime::gx::DecodeEpochAdvance();   // aucun lecteur du cache de decodage en cours
        const bool off = nj::dev::knob("nodecpool", 0) != 0;   // knobs.txt : coupe le pool en direct (mesure A/B dans une meme salle)
        if (m_workers.empty() && g_decode_behind.load(std::memory_order_relaxed) && !m_pinned_off && !off) spawn(2);
        const bool use = !off && !m_workers.empty() && (m_always || g_decode_behind.load(std::memory_order_relaxed));
        if (!use || draws.size() < 64) {
            nj::runtime::gx::DecodeDrawsPipelined(draws);   // un fil : avec preextraction de la memoire
            return;
        }
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_draws = &draws;
            m_next.store(0, std::memory_order_relaxed);
            m_busy = m_workers.size();
            ++m_generation;
        }
        m_cv.notify_all();
        work();
        std::unique_lock<std::mutex> lk(m_mtx);
        m_done_cv.wait(lk, [&] { return m_busy == 0; });
        m_draws = nullptr;
    }

private:
    DecodePool() {
        // Pool de decodage : 2 fils par defaut (mesure du 14/10 : -4.6 ms de flux dans r101,
        // 37.9 -> 33.3 ms). Mesure du 01/10 invalide : pas de difference car test a 8x HD + OBS.
#ifdef _WIN32
        int n = 2;
#else
        // Mobile (16/10) : mesure sur la tour (village, chateau, ile ; 0/1/2 fils) : le pool ne change pas la
        // cadence (29,9-30 i/s) mais coute +15 a +27 points de coeur (reveils, attente du lot) ; le fil de
        // lecture attend le lot, donc des petits coeurs le ralentiraient. Des fils seulement s'il reste de
        // gros coeurs au-dela du jeu, de la lecture, du rendu et du pilote GL : 0 jusqu'a 4 gros coeurs.
        const unsigned big = nj::runtime::place::big_cores();
        int n = big > 4 ? (int)std::min(2u, big - 4) : 0;
#endif
        if (const char* e = NJ_ENV("NJ_DECODE_THREADS")) {
            n = std::atoi(e);
            m_pinned_off = n == 0;   // NJ_DECODE_THREADS=0 : jamais de fils, meme en retard (comparaisons)
        }
        m_always = n > 0;
        spawn(n);
    }
    void spawn(int n) {
        for (int i = 0; i < n; ++i)
            m_workers.emplace_back([this] {
                nj::runtime::njprof::register_thread("decodage");
                loop();
            });
        for (auto& t : m_workers) t.detach();
    }
    bool m_always = false, m_pinned_off = false;
    void work() {
        auto& draws = *m_draws;
        for (size_t i; (i = m_next.fetch_add(1, std::memory_order_relaxed)) < draws.size();)
            nj::runtime::gx::DecodeDraw(*draws[i]);
    }
    void loop() {
        uint64_t seen = 0;
        for (;;) {
            {
                std::unique_lock<std::mutex> lk(m_mtx);
                m_cv.wait(lk, [&] { return m_generation != seen; });
                seen = m_generation;
            }
            work();
            std::lock_guard<std::mutex> lk(m_mtx);
            if (--m_busy == 0) m_done_cv.notify_one();
        }
    }
    std::vector<std::thread> m_workers;
    std::mutex m_mtx;
    std::condition_variable m_cv, m_done_cv;
    std::vector<nj::runtime::gx::GxOp*>* m_draws = nullptr;
    std::atomic<size_t> m_next{0};
    size_t m_busy = 0;
    uint64_t m_generation = 0;
};
} // namespace


// NJ_SHAPE=1 : forme du travail de l'analyse, pour choisir un remede avant de l'ecrire.
//  - reutilisation : empreinte des sommets decodes de chaque dessin (positions, normales,
//    couleurs, coordonnees de texture, drapeaux, primitive), comparee a l'image precedente.
//    Part des dessins et des sommets qu'un cache parfait n'aurait pas eu a redecoder.
//  - allocations : operator new/delete chronometres (core/alloc_prof.cpp), fil hote et autres.
// Rapport [SHAPE] toutes les 60 images. Fausse le chronometrage : jamais pour mesurer des i/s.
namespace nj {
struct AllocStats { uint64_t host_n, host_ticks, other_n, other_ticks; };
extern AllocStats g_alloc_stats;
extern unsigned long g_host_tid;
extern bool g_allocprof_on;
}
#ifdef _WIN32
extern "C" __declspec(dllimport) unsigned long __stdcall GetCurrentThreadId();
#endif

namespace nj { void alloc_sites_report(); }
void alloc_sites_report_c() { nj::alloc_sites_report(); }

namespace nj::runtime::gx {
extern uint64_t g_fi_renders, g_fi_tsum_milli, g_fi_refuse_period, g_fi_refuse_budget;
}
static void shape_report(const std::vector<nj::runtime::gx::GxOp>& frame) {
    static const bool on = NJ_ENV("NJ_SHAPE") != nullptr;
    if (!on) return;
#ifdef _WIN32
    if (!nj::g_allocprof_on) {
        nj::g_host_tid = GetCurrentThreadId();
        nj::g_allocprof_on = true;
    }
#endif
    static std::unordered_map<uint64_t, uint32_t> prev, cur;   // empreinte -> occurrences
    static uint64_t n_frames = 0, d_all = 0, v_all = 0, d_re = 0, v_re = 0, d_prexf = 0,
                    v_prexf = 0, d_small = 0;
    static nj::AllocStats a0{};
    cur.clear();
    for (const GxOp &c : frame) {
        if (c.type != nj::runtime::gx::GxOpKind::DrawPrimitive || c.verts().empty())
            continue;
        uint64_t h = 1469598103934665603ull ^ c.prim_type ^ ((uint64_t)c.verts().size() << 8);
        auto mix = [&h](const void* p, size_t n) {
            const uint8_t* b = (const uint8_t*)p;
            for (size_t i = 0; i + 8 <= n; i += 8) {
                uint64_t w;
                std::memcpy(&w, b + i, 8);
                h = (h ^ w) * 1099511628211ull;
                h ^= h >> 29;
            }
            for (size_t i = n & ~(size_t)7; i < n; ++i)
                h = (h ^ b[i]) * 1099511628211ull;
        };
        bool prexf = false;
        for (const auto& v : c.verts()) {
            mix(v.pos, sizeof v.pos);
            mix(v.norm, sizeof v.norm);
            mix(v.color, sizeof v.color);
            mix(v.tex, sizeof v.tex);
            const uint8_t f[4] = {(uint8_t)(v.has_pos | v.has_nrm << 1 | v.pretransformed << 2 |
                                            v.has_color << 3 | v.has_tex[0] << 4),
                                  v.posMtxIdx, v.texMtxIdx[0], 0};
            mix(f, 4);
            prexf |= v.pretransformed;
        }
        const uint64_t n = c.verts().size();
        ++d_all;
        v_all += n;
        if (n <= 4) ++d_small;
        if (prexf) { ++d_prexf; v_prexf += n; }
        auto it = prev.find(h);
        if (it != prev.end() && it->second > 0) {
            --it->second;   // chaque occurrence de l'image precedente ne sert qu'une fois
            ++d_re;
            v_re += n;
        }
        ++cur[h];
    }
    prev.swap(cur);
    if (++n_frames % 60 == 0 && d_all) {
        const nj::AllocStats a = nj::g_alloc_stats;
        extern bool framelog_enabled();
        // rdtsc -> ms : pente de host_clock (runtime/host_clock.h)
        const double ms_per_tick = nj::g_tsc_clock.on ? nj::g_tsc_clock.us_per_tick / 1000.0 : 0.0;
        NJ_PRINTF("[SHAPE] %llu images : %.0f dessins/img, %.0f sommets/img ; reutilisables "
                    "%.1f %% des dessins, %.1f %% des sommets ; pretransformed (skinning CPU) %.1f %% "
                    "des sommets ; dessins <= 4 sommets %.1f %% | allocations fil hote %.0f/img "
                    "%.2f ms/img, autres fils %.0f/img %.2f ms/img\n",
                    (unsigned long long)n_frames, d_all / 60.0, v_all / 60.0,
                    100.0 * d_re / d_all, 100.0 * v_re / (v_all ? v_all : 1),
                    100.0 * v_prexf / (v_all ? v_all : 1), 100.0 * d_small / d_all,
                    (a.host_n - a0.host_n) / 60.0, (a.host_ticks - a0.host_ticks) * ms_per_tick / 60.0,
                    (a.other_n - a0.other_n) / 60.0,
                    (a.other_ticks - a0.other_ticks) * ms_per_tick / 60.0);
        a0 = a;
        d_all = v_all = d_re = v_re = d_prexf = v_prexf = d_small = 0;
        if (n_frames % 1200 == 0) {
            extern void alloc_sites_report_c();
            alloc_sites_report_c();
        }
    }
}

// Fil de lecture du FIFO (NJ_NOPARSETHREAD=1 : tout sur le fil hote, comme avant).
//
// Le jeu attend les signaux de fin de dessin (PE finish / jetons), que la lecture ne libere
// qu'apres avoir analyse et decode les dessins. Sur le fil hote, cette lecture s'arretait pendant
// tout le rendu de l'image precedente (~130 ms au Galaxy S5) : le jeu attendait le rendu. Le fil
// de lecture analyse, decode et libere les signaux sans attendre ; le fil hote ne fait plus que
// rendre les images completes deposees dans s_carry (sous s_carry_mutex). La lecture a son
// propre etat GX (fifo_parser.cpp, g_pstate).
static std::mutex s_carry_mutex;
static std::atomic<bool> s_parse_thread_on{false};
static std::chrono::steady_clock::time_point s_parse_t0 = std::chrono::steady_clock::now();
static std::atomic<size_t> s_parse_buf_n{0};
// Releve des videos (main.cpp) : commandes en file, octets en attente d'analyse.
size_t gx_carry_size();
size_t gx_parse_pending() { return s_parse_buf_n.load(std::memory_order_relaxed); }

// Microsecondes ecoulees depuis t.
static inline uint64_t us_since(std::chrono::steady_clock::time_point t) {
    return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now() - t).count();
}

// Etage d'analyse : reprend les octets arrives dans le FIFO (et les copies de memoire marquees de
// leur position dans le flux), les analyse en commandes, decode tout de suite les sommets des
// dessins, puis les met a la suite de l'image en cours. false = rien a faire.
static bool ParseStage() {
    static std::vector<uint8_t> pending;       // octets pas encore analyses
    static uint64_t pending_pos = 0;           // position dans le flux de pending[0]
    // Reste incomplet (commande coupee) : sans octet neuf l'analyse n'avance pas ; repondre
    // « travail fait » faisait tourner ce fil sans dormir contre le verrou du fil du jeu.
    static bool stalled = false;
    bool fresh = false;
    {
        std::lock_guard<std::mutex> lock(g_fifo_lock);
        if (!g_fifo_bytes.empty()) {
            fresh = true;
            if (pending.empty())
                pending_pos = g_fifo_pushed - g_fifo_bytes.size();
            pending.insert(pending.end(), g_fifo_bytes.begin(), g_fifo_bytes.end());
            g_fifo_bytes.clear();
            g_fifo_len.store(0, std::memory_order_relaxed);
        }
        for (auto& f : g_volatile_pending)
            nj::runtime::gx::QueueVolatileRange(f.pos, f.addr, f.len, std::move(f.bytes));
        g_volatile_pending.clear();
    }
    if (pending.empty() || (stalled && !fresh))
        return false;

    const auto t0 = std::chrono::steady_clock::now();
    s_parse_t0 = t0;
    static std::vector<nj::runtime::gx::GxOp> commands;   // capacite gardee
    commands.clear();
    const size_t before = pending.size();
    nj::runtime::gx::SetParseStreamBase(pending_pos);
    nj::runtime::gx::ParseFifo(pending, commands);
    pending_pos += before - pending.size();
    stalled = pending.size() == before;
    // Levier A (early_drawdone) : le jeu repart des l'analyse. Ses tampons reecrits sont deja
    // figes (copies videes, overlays des dessins) ; ce qui reste lu en memoire vivante est
    // surveille (empreinte, g_race_draws, colonne rc= du journal).
    if (nj::runtime::gfx::g_early_drawdone) nj::runtime::gx::FlushDeferredPeSignals();

    // Decoder maintenant, tant que la memoire du jeu contient ce que ces dessins indexent : RE4
    // habille ses personnages sur le processeur dans un tampon que l'image suivante reecrit ; decoder
    // a l'assemblage de l'image melangeait deux images (Leon, ennemis et vehicules explosaient).
    // L'analyseur retient les signaux de fin du PE jusque-la.
    {
        static std::vector<nj::runtime::gx::GxOp*> draws;
        draws.clear();
        for (auto& c : commands)
            if (c.type == nj::runtime::gx::GxOpKind::DrawPrimitive && c.raw && !c.shared_vertices && c.vertices.empty())
                draws.push_back(&c);
        if (!draws.empty()) {
            extern uint64_t g_prof_pool_us;
            const auto p0 = std::chrono::steady_clock::now();
            DecodePool::get().run(draws);
            g_prof_pool_us += us_since(p0);
        }
    }
    nj::runtime::gx::FlushDeferredPeSignals();
    const uint64_t spent = us_since(t0);
    g_st_parse_us += spent;
    g_fl_parse_us += spent;
    g_prof_parse_us += spent;

    if (pending.size() > (4u << 20))   // flux illisible : on repart a vide plutot que de grossir
        pending.clear();
    s_parse_buf_n.store(pending.size(), std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(s_carry_mutex);
    s_carry.append_all(commands);
    // 13/10 : pendant une video la file grossissait jusqu'au std::bad_alloc (fil d'analyse). Une
    // image du jeu fait ~20 000 commandes : au-dela de 200 000 sans fin d'image, les vieux dessins partent.
    if (s_carry.size() > 200000) s_carry.drop_old_draws(20000);
    return true;
}

// NJ_DETERMINISTIC=1 (validation, avec NJ_INSTTB) : le FIFO est lu par le fil du jeu lui-meme
// a chaque point de verification (GX_DeterministicParse, ios.cpp), jamais par le fil de lecture ni
// par l'hote. Les signaux PE (GXDrawDone, jetons) tombent alors a un compte d'instructions
// reproductible : deux parties donnent le meme flux GX (oracle CPU, NJ_FIFOHASH). Plus lent.
static bool gx_deterministic() {
    static const bool d = NJ_ENV("NJ_DETERMINISTIC") != nullptr;
    return d;
}
void GX_DeterministicParse() {
    if (gx_deterministic()) ParseStage();
}

// Fil de lecture : lit des qu'il y a du neuf, sinon attend 200 us.
void GX_StartParseThread() {
    if (NJ_ENV("NJ_NOPARSETHREAD") || gx_deterministic() || s_parse_thread_on.exchange(true)) return;
    std::thread([] {
        nj::runtime::njprof::register_thread("lecture");
        for (;;) {
            // Sommeil precis (minuteur haute resolution sous Windows, ou sleep_for arrondissait a la
            // milliseconde) : le jeu attend les signaux de fin de dessin que ce fil libere.
            if (!ParseStage())
                nj::precise_sleep_us(200);
        }
    }).detach();
    NJ_LOG << "[GX] fil de lecture du FIFO demarre" << std::endl;
}

// Etape de rendu : extrait l'image complete la plus recente de s_carry et la rend.
// Fin d'image du GX : copie de l'EFB vers l'XFB (BP 0x52, bit 14).
static inline bool is_frame_end(const nj::runtime::gx::GxOp& c) {
    return c.type == nj::runtime::gx::GxOpKind::BPRegister && c.reg == 0x52 && (c.val & 0x4000);
}

// Images du jeu par fenetre de 2 s et calcul du jeu par image (hors sommeil et attente de la
// vsync), lisibles meme sans rendu (NJ_HEADLESS).
static void report_frame_rate() {
    static std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
    static uint64_t l_xfb = 0, l_idle = 0, l_vspin = 0;
    const auto now = std::chrono::steady_clock::now();
    if (now - last < std::chrono::seconds(2))
        return;
    const uint64_t xfb = g_fl_xfb_copies.load(), idle = g_fl_idle_us.load(), vspin = g_fl_vspin_us.load();
    const double win_ms = std::chrono::duration<double, std::milli>(now - last).count();
    const double idle_ms = (idle - l_idle) / 1000.0, vspin_ms = (vspin - l_vspin) / 1000.0;
    const uint64_t n = xfb - l_xfb;
    NJ_PRINTF("[GXPROF] images=%llu fenetre_ms=%.0f dort_ms=%.0f vsync_ms=%.0f jeu_ms_par_image=%.1f\n",
                (unsigned long long)n, win_ms, idle_ms, vspin_ms, n ? (win_ms - idle_ms - vspin_ms) / n : 0.0);
    last = now;
    l_xfb = xfb;
    l_idle = idle;
    l_vspin = vspin;
    // NJ_GXPROF_T : ou part le temps de l'analyse du flux (fifo_parser.cpp), par image
    static const bool detail = NJ_ENV("NJ_GXPROF_T") != nullptr;
    if (detail && n) {
        using namespace nj::runtime::gx;
        NJ_PRINTF("[GXFLUX] par image : listes=%llu (en cache %llu) octets_listes=%llu listes_ms=%.1f "
                    "dessins=%llu dessins_ms=%.1f zones=%llu\n",
                    (unsigned long long)(g_prof_dl_calls / n), (unsigned long long)(g_prof_dl_hits / n),
                    (unsigned long long)(g_pf_list_bytes / n), g_prof_dl_us / 1000.0 / n,
                    (unsigned long long)(g_prof_draws / n), g_prof_snap_us / 1000.0 / n,
                    (unsigned long long)(g_prof_vol_draws.load() / n));
        // 16/10 : l'analyse entiere par image et la part du decodage des sommets (DecodePool::run, en ligne
        // quand le pool a 0 fil), pour savoir ou va le temps du fil de lecture.
        NJ_PRINTF("[GXFLUX] analyse_ms=%.1f dont decodage_ms=%.1f (sommets passes au decodeur/img=%llu)\n", g_prof_parse_us / 1000.0 / n,
                    g_prof_pool_us / 1000.0 / n, (unsigned long long)(g_prof_decoded_verts.exchange(0) / n));
        g_prof_parse_us = g_prof_pool_us = 0;
        g_prof_dl_calls = g_prof_dl_hits = g_pf_list_bytes = g_prof_dl_us = g_prof_draws = g_prof_snap_us = 0;
        g_prof_vol_draws = 0;
    }
}

// Etage de rendu : des qu'une image complete est dans la file, elle en est retiree et rendue.
// Deux fins d'image dans le meme lot (battement entre la cadence du jeu et celle de la boucle
// hote) : les dessins de la premiere sont jetes, ses ecritures de registres gardees.
size_t gx_carry_size() {
    std::lock_guard<std::mutex> lk(s_carry_mutex);
    return s_carry.size();
}

static void RenderStage() {
    const auto t0 = s_parse_t0;
    std::unique_lock<std::mutex> carry_lk(s_carry_mutex);
    // La file ne grandit que par la fin entre deux images : on ne relit que ce qui est arrive
    // depuis le dernier passage (la relire entiere a chaque tour coutait 15 % du fil hote).
    static size_t scanned = 0;
    if (scanned > s_carry.size())
        scanned = 0;
    int last_end = -1, prev_end = -1;
    for (size_t i = scanned; i < s_carry.size(); ++i)
        if (is_frame_end(s_carry[i])) {
            prev_end = last_end;
            last_end = (int)i;
        }
    if (last_end < 0) {
        scanned = s_carry.size();
        return;
    }
    scanned = 0;

    static std::vector<nj::runtime::gx::GxOp> frame;
    frame.clear();
    if (prev_end < 0) {
        s_carry.take_prefix((size_t)last_end + 1, frame);   // cas courant : sans deplacement
    } else {
        for (int i = 0; i <= last_end; ++i) {
            GxOp &c = s_carry[i];
            if (!(i <= prev_end && c.type == nj::runtime::gx::GxOpKind::DrawPrimitive))
                frame.emplace_back(std::move(c));
        }
        s_carry.pop_front((size_t)last_end + 1);
    }
    carry_lk.unlock();

    nj::runtime::gx::xfstat_frame_end();
    ++g_st_frames;
    // Sommets de l'image retenue (deja decodes par l'etage d'analyse quand il a son fil).
    const auto d0 = std::chrono::steady_clock::now();
    {
        static std::vector<nj::runtime::gx::GxOp*> draws;
        draws.clear();
        for (auto& c : frame)
            if (c.type == nj::runtime::gx::GxOpKind::DrawPrimitive)
                draws.push_back(&c);
        g_stat_draws += draws.size();
        if (!draws.empty() && !s_parse_thread_on.load(std::memory_order_acquire))
            DecodePool::get().run(draws);
    }
    const uint64_t decode_us = us_since(d0);
    g_st_parse_us += decode_us;
    g_fl_parse_us += decode_us;
    {   // retard : intervalle moyen entre images rendues (voir DecodePool)
        static auto last = std::chrono::steady_clock::now();
        static double ema = 33.0;
        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double, std::milli>(now - last).count();
        last = now;
        if (dt < 500.0) ema = ema * 0.9 + dt * 0.1;
        g_decode_behind.store(ema > 37.0, std::memory_order_relaxed);
    }
    report_frame_rate();

    if (s_host_window) {
        if (g_renderer == nullptr) {
            g_renderer = nj::runtime::gx::make_gl_renderer();
            g_renderer->attach(nullptr);
        }
        if (g_renderer) {
            const auto r0 = std::chrono::steady_clock::now();
            shape_report(frame);
            nj::runtime::gx::interp_new_frame(
                frame, g_renderer.get(),
                std::chrono::duration<double>(r0 - t0).count());
            const auto r1 = std::chrono::steady_clock::now();
            g_st_render_us +=
                (r1 - r0) / std::chrono::microseconds(1);

            // Journal de performance chiffre (perf_journal.h) : chaque image, build de diffusion compris.
            {
                static const bool started = [] {
                    nj::perf::session_start((const char*)glGetString(GL_RENDERER));
                    nj::report::set_gpu((const char*)glGetString(GL_RENDERER), (const char*)glGetString(GL_VERSION));
                    return true;
                }();
                (void)started;
                static auto j_last = r1;
                static uint64_t j_parse = g_fl_parse_us, j_swap = g_fl_swap_us, j_idle = g_fl_idle_us.load(),
                                j_vspin = g_fl_vspin_us.load();
                const uint64_t idle = g_fl_idle_us.load(), vspin = g_fl_vspin_us.load();
                const float dt = std::chrono::duration<float, std::milli>(r1 - j_last).count();
                const float idle_ms = (idle - j_idle) / 1000.0f, vspin_ms = (vspin - j_vspin) / 1000.0f;
                nj::perf::frame(dt, (g_fl_parse_us - j_parse) / 1000.0f,
                                  std::chrono::duration<float, std::milli>(r1 - r0).count(),
                                  (g_fl_swap_us - j_swap) / 1000.0f, dt - idle_ms - vspin_ms);
                nj::runtime::place::frame_work(dt - idle_ms - vspin_ms);   // ADPF (Android 13+)
                j_last = r1;
                j_parse = g_fl_parse_us;
                j_swap = g_fl_swap_us;
                j_idle = idle;
                j_vspin = vspin;
            }

            if (framelog_enabled()) {
                static auto last = r1;
                static uint64_t last_parse = 0, last_swap = 0, last_swaps = 0, last_idle = 0,
                                last_vspin = 0, last_dd = 0, last_vi = 0, last_skip[4] = {};
                extern uint64_t g_vi_fired, g_vi_skip[4], g_vi_late;
                static uint64_t last_fr = 0, last_ft = 0, last_rp = 0, last_rb = 0;
                static uint64_t last_late = 0;
                static int printed = 0;
                static const int limit = [] {
                    const char* e = NJ_ENV("NJ_FRAMELOG");
                    const char* c = e ? std::strchr(e, ',') : nullptr;
                    return c ? std::atoi(c + 1) : 600;
                }();
                const uint64_t idle = g_fl_idle_us.load(), vspin = g_fl_vspin_us.load(),
                               dd = g_fl_drawdones.load();
                if (framelog_active() && printed < limit) {
                    ++printed;
                    const double dt = std::chrono::duration<double, std::milli>(r1 - last).count();
                    const double idle_ms = (idle - last_idle) / 1000.0,
                                 vspin_ms = (vspin - last_vspin) / 1000.0;
                    NJ_PRINTF("[FRAME] %llu dt=%.1f parse=%.1f render=%.1f swap=%.1f/%llu "
                                "dort=%.1f vsync=%.1f jeu=%.1f dd=%llu cmds=%zu vi=%llu "
                                "saut=%llu/%llu/%llu/%llu tard=%llu inter=%llu t=%.2f "
                                "refus=%llu/%llu\n",
                                (unsigned long long)g_st_frames, dt,
                                (g_fl_parse_us - last_parse) / 1000.0,
                                std::chrono::duration<double, std::milli>(r1 - r0).count(),
                                (g_fl_swap_us - last_swap) / 1000.0,
                                (unsigned long long)(g_fl_swaps - last_swaps), idle_ms, vspin_ms,
                                dt - idle_ms - vspin_ms, (unsigned long long)(dd - last_dd),
                                frame.size(), (unsigned long long)(g_vi_fired - last_vi),
                                (unsigned long long)(g_vi_skip[0] - last_skip[0]),
                                (unsigned long long)(g_vi_skip[1] - last_skip[1]),
                                (unsigned long long)(g_vi_skip[2] - last_skip[2]),
                                (unsigned long long)(g_vi_skip[3] - last_skip[3]),
                                (unsigned long long)(g_vi_late - last_late),
                                (unsigned long long)(nj::runtime::gx::g_fi_renders - last_fr),
                                (nj::runtime::gx::g_fi_renders - last_fr)
                                    ? (nj::runtime::gx::g_fi_tsum_milli - last_ft) / 1000.0 /
                                          (double)(nj::runtime::gx::g_fi_renders - last_fr)
                                    : 0.0,
                                (unsigned long long)(nj::runtime::gx::g_fi_refuse_period - last_rp),
                                (unsigned long long)(nj::runtime::gx::g_fi_refuse_budget - last_rb));
                }
                last = r1; last_parse = g_fl_parse_us; last_swap = g_fl_swap_us;
                last_swaps = g_fl_swaps; last_idle = idle; last_vspin = vspin; last_dd = dd;
                last_vi = g_vi_fired;
                last_late = g_vi_late;
                last_fr = nj::runtime::gx::g_fi_renders;
                last_ft = nj::runtime::gx::g_fi_tsum_milli;
                last_rp = nj::runtime::gx::g_fi_refuse_period;
                last_rb = nj::runtime::gx::g_fi_refuse_budget;
                for (int k = 0; k < 4; ++k) last_skip[k] = g_vi_skip[k];
            }
        }
    }
}

void gx_pump_fifo() {
    if (!s_parse_thread_on.load(std::memory_order_acquire) && !gx_deterministic())
        ParseStage();
    RenderStage();
}

// Moteur natif : les images que publie le noyau passent par les memes etages de rendu que celles du
// flux GX du jeu recompile (chunk_adapter.cpp les traduit en commandes de notre rendu).
void gx_native_pump() {
    // Notre coeur natif dessine lui-meme : son image va dans la zone que l'appli presente
    // (640 x 480 a l'echelle interne, en haut du tampon), comme une image du jeu.
    // Cadence fixe (15/11, « images qui reviennent en arriere ») : le coeur produit 30 images par seconde a des instants
    // irreguliers (rendu 30-36 ms). Montrer chaque image des son arrivee donnait 3 balayages a l'une, 1 a la suivante
    // (ecarts mesures 48-90 ms au lieu de 33). Ici une image neuve ne remplace l'affichee qu'apres deux periodes
    // d'ecran (moins une demi-periode de marge) ; l'appli presente a chaque balayage (main.cpp, render_gx).
    // NJ_PACING=0 : ancien comportement (A/B).
    static const bool pacing = [] { const char* e = NJ_ENV("NJ_PACING"); return !(e && e[0] == '0'); }();
    static std::chrono::steady_clock::time_point last_switch{};
    static double period_ms = 0;
    if (period_ms == 0) {
        SDL_DisplayMode dm{};
        period_ms = (SDL_GetCurrentDisplayMode(0, &dm) == 0 && dm.refresh_rate >= 30) ? 1000.0 / dm.refresh_rate : 1000.0 / 60.0;
    }
    const auto now = std::chrono::steady_clock::now();
    const bool may_switch = !pacing || nj::runtime::gfx::g_fps_target > 30 ||
                            std::chrono::duration<double, std::milli>(now - last_switch).count() >= 1.5 * period_ms;
    if (!may_switch && nj::native::frame_ready())
        return;   // image neuve en reserve : elle sera montree au prochain balayage permis
    if (may_switch && nj::native::frame_ready()) {
        last_switch = now;
        const int s = nj::runtime::gfx::g_scale;
        if (nj::native::frame_blit(0, nj::runtime::gfx::efb_h() - 480 * s, 640 * s, 480 * s)) {
            ++g_st_frames;
            ++nj::runtime::gfx::g_game_images;
            if (g_st_frames % 300 == 0) {   // diagnostic N3 : l'image du coeur apres copie dans la zone presentee
                GLint fb = 0;
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fb);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)fb);
                unsigned sum = 0, lit = 0;
                unsigned char px[4] = {};
                for (int j = 0; j < 4; ++j)
                    for (int i = 0; i < 4; ++i) {
                        glReadPixels((80 + 160 * i) * s, nj::runtime::gfx::efb_h() - (60 + 120 * j) * s, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
                        sum += px[0] + px[1] + px[2];
                        lit += (px[0] | px[1] | px[2]) > 8;
                    }
                NJ_LOG << "[NATIF] recu " << g_st_frames << " : fb " << fb << ", " << lit << "/16 points allumes, somme " << sum
                       << ", alpha " << (unsigned)px[3] << std::endl;
            }
        }
        return;
    }
    static nj::runtime::gx::chunk::Adapter adapter;
    static std::vector<GxOp> frame;
    // Enregistrement d'essai (NJ_NATIVE_DUMP=<dossier>, NJ_NATIVE_DUMP_AT=images separees par des virgules) :
    // le rendu se detache, l'image suivante arrive complete (etat + textures) et part dans un fichier
    // RE4GXCH1 que l'outil ChunkReplay rejoue sur PC.
    static const char* dump_dir = NJ_ENV("NJ_NATIVE_DUMP");
    static std::vector<uint64_t> dump_at = [] {
        std::vector<uint64_t> v;
        if (const char* e = NJ_ENV("NJ_NATIVE_DUMP_AT"))
            for (const char* q = e; *q;) { v.push_back(std::strtoull(q, nullptr, 10)); const char* n = std::strchr(q, ','); if (!n) break; q = n + 1; }
        return v;
    }();
    static size_t dump_next = 0;
    static bool dump_wait = false;
    if (dump_dir && !dump_wait && dump_next < dump_at.size() && g_st_frames >= dump_at[dump_next]) {
        nj::native::gx_detach();
        dump_wait = true;
    }
    while (const nj::native::RawChunk* c = nj::native::gx_acquire()) {
        static int dump_run_left = 0, dump_run_index = 0;
        static size_t dump_cur = 0;
        static const int dump_run = NJ_ENV("NJ_NATIVE_DUMP_RUN") ? std::atoi(NJ_ENV("NJ_NATIVE_DUMP_RUN")) : 1;
        const bool starting = dump_wait && (c->flags & 1u);
        if (starting) { dump_run_left = dump_run; dump_run_index = 0; dump_cur = dump_next++; }
        if (dump_dir && dump_run_left > 0) {
            --dump_run_left;
            char path[512];
            std::snprintf(path, sizeof path, "%s/gx-%04zu_%03d.bin", dump_dir, dump_cur, dump_run_index++);
            if (FILE* f = std::fopen(path, "wb")) {
                static const char magic[8] = {'R', 'E', '4', 'G', 'X', 'C', 'H', '1'};
                const uint32_t head[14] = {c->version, c->sequence, c->flags, c->op_words, c->vertex_bytes, c->index_count,
                                           c->blob_bytes, c->xfb_address, c->xfb_width, c->xfb_height, c->draws,
                                           c->vertex_count, c->texture_bytes, (uint32_t)g_st_frames};
                std::fwrite(magic, 8, 1, f);
                std::fwrite(head, sizeof head, 1, f);
                std::fwrite(c->ops, 4, c->op_words, f);
                std::fwrite(c->vertices, 1, c->vertex_bytes, f);
                std::fwrite(c->indices, 2, c->index_count, f);
                std::fwrite(c->blob, 1, c->blob_bytes, f);
                std::fclose(f);
                NJ_LOG << "[Natif] enregistrement " << path << " (image " << g_st_frames << ")" << std::endl;
            }
            if (starting) dump_wait = false;
        }
        static const bool trace = NJ_ENV("NJ_NATIVE_TRACE") != nullptr;
        if (trace && (g_st_frames % 60 == 0))
            NJ_LOG << "[Natif] chunk " << g_st_frames << " flags=" << c->flags << " ops=" << c->op_words
                   << " dessins=" << c->draws << " sommets=" << c->vertex_count << " textures=" << c->texture_bytes
                   << " blob=" << c->blob_bytes << std::endl;
        frame.clear();
        if (c->flags & 1u)   // l'image repart de zero : etat complet et textures renvoyees
            adapter = nj::runtime::gx::chunk::Adapter();
        const nj::runtime::gx::chunk::View view{c->ops,      c->op_words,   c->vertices,    c->vertex_bytes,
                                                c->indices,  c->index_count, c->blob,       c->blob_bytes};
        adapter.translate(view, frame);
        nj::native::gx_release(c);
        ++g_st_frames;
        if (!s_host_window)
            continue;
        if (g_renderer == nullptr) {
            g_renderer = nj::runtime::gx::make_gl_renderer();
            g_renderer->attach(nullptr);
        }
        if (trace && (g_st_frames % 60 == 1))
            NJ_LOG << "[Natif] liste de rendu : " << frame.size() << " ordres" << std::endl;
        if (g_renderer) {
            // Le noyau publie ses images a la cadence du jeu : on les dessine tout de suite
            // (pas de file d'interpolation, comme l'outil de rejeu ChunkReplay).
            g_renderer->draw_frame(frame);
            // Une image dessinee = une image presentee. Quand le noyau fournit plus vite que le rendu ne
            // dessine, la suivante est deja la : sans cette sortie la boucle ne rendait jamais la main, et
            // l'ecran, les entrees et le son de l'hote restaient figes (mesure S5 du 11/11 : 152 images
            // presentees en 8 min). La suivante est prise au tour d'apres, une fois celle-ci a l'ecran.
            break;
        }
    }
}

namespace nj::runtime {

// Contre-pression du flux GX, comme sur la console.
//
// Sur GameCube le flux est un anneau de **taille fixe** : quand il se
// remplit, le processeur cale jusqu'a ce que le GPU consomme. C'est ce
// blocage qui l'empeche de prendre de l'avance et d'ecraser un tampon pas
// encore lu.
//
// Notre g_fifo_bytes est un vector sans borne : l'invite peut courir des images
// entieres devant l'analyseur. Or tous les modeles a peau calculee ecrivent
// dans UN tampon partage a adresse fixe (src/game/TmpBuf.cpp du decompile,
// verifie : GetDrawTmpBufAddr rend toujours la meme adresse) et le jeu ne
// synchronise qu'une fois par image. L'avance du processeur ecrase donc des
// sommets avant qu'on les lise - et aucune copie ne rattrape ca, ce qui
// explique pourquoi dix essais de copie ont echoue.
//
// NJ_FIFOLIMIT=<octets> retablit la borne. 0 = sans borne (l'ancien
// comportement).
static size_t fifo_limit() {
    static const size_t n = [] {
        const char* e = NJ_ENV("NJ_FIFOLIMIT");
        return e ? (size_t) std::strtoul(e, nullptr, 0) : (size_t) 0;
    }();
    return n;
}

// Appelee **avant** la prise du verrou : attendre en le tenant empecherait
// justement le vidage.
static inline void wgp_backpressure() {
    // Fil CPU seulement : une lecture ordinaire au lieu de la garde de static local de
    // fifo_limit() (une barriere par ecriture du tuyau sur ARM).
    static size_t s_lim = SIZE_MAX;
    if (s_lim == SIZE_MAX) [[unlikely]]
        s_lim = fifo_limit();
    const size_t lim = s_lim;
    if (lim == 0)
        return;
    int tours = 0;
    while (g_fifo_len.load(std::memory_order_relaxed) > lim) {
        std::this_thread::yield();
        // Filet : si l'hote ne vide plus, mieux vaut un rendu faux qu'un
        // blocage definitif.
        if (++tours > 200000)
            break;
    }
}


// Oracle CPU (NJ_FIFOHASH=<fichier>, 30/09) : hachage FNV-1a 64 de tous les octets que le jeu
// ecrit dans le tuyau GX, dans l'ordre, et une ligne par VI du jeu (GX_FifoHashMark, ios.cpp).
// Avec NJ_INSTTB (horloge au compteur d'instructions), deux parties doivent donner la meme suite :
// alors deux builds du CPU qui la donnent aussi produisent les memes commandes, donc les memes
// images (le rendu etant valide par les oracles FIFO).
static const char* const s_fifohash_path = NJ_ENV("NJ_FIFOHASH");
static uint64_t s_fifohash = 0xCBF29CE484222325ull, s_fifohash_n = 0;
// Jalons par octets (<fichier>.octets) : « octets hachage » toutes les 64 Ko du flux, quel que soit
// le moment. Les attentes du jeu sur des evenements a l'heure hote decalent le flux d'une VI a
// l'autre sans en changer le contenu (30/09) : ce sont ces jalons qu'on compare entre builds.
static inline void fifohash(const uint8_t* p, size_t n) {
    if (!s_fifohash_path) [[likely]] return;
    static FILE* jf = [] {
        char name[1024];
        std::snprintf(name, sizeof name, "%s.octets", s_fifohash_path);
        return std::fopen(name, "w");
    }();
    uint64_t h = s_fifohash, c = s_fifohash_n;
    for (size_t i = 0; i < n; ++i) {
        h = (h ^ p[i]) * 0x100000001B3ull;
        if ((++c & 0xFFFF) == 0 && jf) {
            std::fprintf(jf, "%llu %016llx\n", (unsigned long long)c, (unsigned long long)h);
            if ((c & 0xFFFFF) == 0) std::fflush(jf);
        }
    }
    s_fifohash = h;
    s_fifohash_n = c;
}

// Octets ajoutes au FIFO materiel (sous g_fifo_lock) : hachage de l'oracle, taille publiee pour
// la contre-pression, position du flux avancee.
static inline void fifo_append_locked(const uint8_t* bytes, size_t n) {
    fifohash(bytes, n);
    g_fifo_bytes.insert(g_fifo_bytes.end(), bytes, bytes + n);
    g_fifo_len.store(g_fifo_bytes.size(), std::memory_order_relaxed);
    g_fifo_pushed += n;
}

// Tampon d'ecriture du fil CPU. Chaque ecriture du tuyau prenait g_fifo_lock, soit un
// pthread_mutex_lock de libwinpthread par mot : 7 % du fil CPU au profil du village (27/09). Le
// fil CPU est le seul producteur : il remplit ce tampon sans verrou et le verse d'un bloc dans
// g_fifo_bytes quand il est plein, avant chaque copie de memoire horodatee (CopyFlushedRange, pour
// que g_fifo_pushed reste la position exacte) et a chaque passage de drain_callbacks
// (toutes les 512 instructions invitees : un GXDrawDone est livre ~1 us apres son ecriture, la
// boucle d'attente de SelectThread passant elle aussi par la). Actif seulement quand main.cpp
// l'active (les outils comme FifoReplay ecrivent et analysent depuis le meme fil) ;
// NJ_NOSTAGE=1 le coupe (A/B).
static bool s_stage_on = false;
static uint8_t s_stage[16384];
NjWgp g_nj_wgp{s_stage, s_stage};   // nj::runtime (ouvert plus haut)
// Position d'ecriture du tampon : nj::runtime::g_nj_wgp.p (cpu_context.h). Le code genere y
// ecrit aussi directement (nj::FM, ecritures gardees a 0xCC008xxx) tant que p + n <= lim ; lim
// vaut la fin du tampon quand le tampon est actif et le tuyau non redirige, sinon p (tout passe
// alors par GX_WGPIPE_Write*). Coupe = lim au debut du tampon : rien n'y tient jamais.
// wgp_refresh_inline() le tient a jour.
#define s_stage_n ((size_t)(nj::runtime::g_nj_wgp.p - s_stage))

// Sous g_fifo_lock.
static inline void stage_drain_locked() {
    if (s_stage_n == 0) return;
    fifo_append_locked(s_stage, s_stage_n);
    nj::runtime::g_nj_wgp.p = s_stage;
}

static inline void wgp_backpressure();
static inline void stage_put(const uint8_t* b, size_t n) {
    if (s_stage_n + n > sizeof(s_stage)) {
        {
            std::lock_guard<std::mutex> lock(g_fifo_lock);
            stage_drain_locked();
        }
        // Les ecritures en ligne du code genere ne passent pas par wgp_backpressure : on la fait
        // ici, a chaque tampon plein (16 Ko), et dans GX_StageFlush.
        wgp_backpressure();
    }
    std::memcpy(nj::runtime::g_nj_wgp.p, b, n);
    nj::runtime::g_nj_wgp.p += n;
}


// DCStoreRangeNoSync / DCFlushRangeNoSync (void *addr, u32 len): the game has
// written this memory for the GPU. Copy it now, tagged with the FIFO position,
// so draws issued after the flush read these bytes even once the game has
// reused the buffer (RE4 skins every model into the same scratch buffer).
static void CopyFlushedRange(GuestCpu& ctx) {
    // Off unless NJ_VOLCOPY is set: RE4 also flushes ranges before writing
    // them, and such a copy hid the real vertices. Decoding right after the
    // parse (gx_pump_fifo) already reads the buffer while it is current.
    // NJ_VOLDUMP alone tracks the flushed ranges without copying them, so
    // the dump can pick out the draws that read those buffers.
    // NJ_VOLSITE=1 : d'ou vient chaque vidage, et de quelle taille.
    //
    // Le jeu vide une plage **avant** de l'ecrire (pour l'invalider) et
    // **apres** l'avoir ecrite (pour la pousser en memoire). Copier sur le
    // premier cas fige les octets de l'ancien modele et masque les vrais
    // sommets ; ne copier sur aucun laisse lire de la memoire deja ecrasee.
    // Mesure : sans copie 100 % des dessins indexes sont a nu, avec copie
    // 0 % - et l'arme saute dans les deux cas, ce qui est coherent avec
    // "deux facons differentes d'avoir tort".
    //
    // Il faut donc distinguer les points d'appel. Celui-ci les recense.
    {
        static const bool site = NJ_ENV("NJ_VOLSITE") != nullptr;
        if (site) {
            static std::map<uint32_t, std::pair<uint64_t, uint64_t>> vus;
            static std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
            auto& e = vus[(uint32_t) ctx.lr];
            ++e.first;
            e.second += ctx.gpr[4];
            const auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - t0).count() >= 3) {
                for (const auto& kv : vus)
                    NJ_LOG << "[VOLSITE] lr=0x" << std::hex << kv.first << std::dec
                              << "  " << kv.second.first << " vidages, "
                              << kv.second.second << " octets" << std::endl;
                vus.clear();
                t0 = now;
            }
        }
    }

    // Copier **seulement aux vidages par modele**.
    //
    // Les points d'appel, releves avec NJ_VOLSITE sur la scene du village :
    //   0x801a5fd4   413 vidages, ~4,5 Ko    ~13 par image
    //   0x801a605c   412 vidages, ~4,2 Ko    ~13 par image
    //   0x801a5360    29 vidages, ~112 Ko    une fois par image
    //   0x80089f44   538 vidages, ~1,9 Ko
    //
    // Les deux premiers tombent treize fois par image : un par modele, juste
    // apres son ecriture. Ce sont les instantanes utiles. Le gros de 112 Ko
    // n'arrive qu'une fois par image et ne contient donc que le dernier
    // modele ecrit - c'est lui qui "masquait les vrais sommets" et qui a fait
    // desactiver la copie a l'epoque.
    //
    // NJ_VOLSITES=<hex,hex,...> remplace la liste ; vide = tous les points,
    // l'ancien comportement.
    static const std::vector<uint32_t> sites = [] {
        std::vector<uint32_t> v;
        const char* e = NJ_ENV("NJ_VOLSITES");
        // Par defaut : **tous** les points d'appel.
        //
        // J'ai essaye de ne copier qu'aux deux vidages par modele, en
        // raisonnant qu'ils tombent juste apres l'ecriture de chaque modele.
        // Mesure : 48 % d'images anormales contre 28 % en copiant partout.
        // Nettement pire. Le raisonnement etait plausible et faux - ces deux
        // points ne couvrent pas ce qu'il faut. NJ_VOLSITES=<hex,...>
        // permet de rejouer l'essai, la liste vide garde tous les points.
        if (e == nullptr) {
            v.clear();
        } else if (*e) {
            const char* p = e;
            while (*p) {
                v.push_back((uint32_t) std::strtoul(p, const_cast<char**>(&p), 16));
                while (*p == ',' || *p == ' ') ++p;
            }
        }
        return v;
    }();

    // Copie active par defaut depuis le 28/09 : sans elle, la tete de Leon (salle 3-17, camera
    // proche) se dessinait avec les sommets d'un autre modele (cartes de cheveux decalees, visage
    // visible a travers) ; avec elle, la tete est juste sur presque toutes les images, et le taux
    // d'images anormales de l'arme en visee ne change pas (27-29 % sans, 28 % avec). Le vrai
    // remede, capturer les sommets au moment ou le dessin entre dans le FIFO, est pour la
    // reecriture de la pile graphique. NJ_NOVOLCOPY=1 : l'ancien comportement.
    static const bool copy = NJ_ENV("NJ_NOVOLCOPY") == nullptr;
    static const bool track = copy || NJ_ENV("NJ_VOLDUMP") != nullptr ||
                              NJ_ENV("NJ_VOLSITE") != nullptr;
    if (!track) return;
    if (copy && !sites.empty()) {
        bool voulu = false;
        for (uint32_t a : sites)
            if ((uint32_t) ctx.lr == a) { voulu = true; break; }
        if (!voulu) return;
    }
    const uint32_t phys = ctx.gpr[3] & 0x01FFFFFFu;
    const uint32_t len = ctx.gpr[4];
    if (!nj::runtime::g_guest_mem || len == 0 || len > (1u << 20) ||
        phys + len > 0x01800000u)
        return;
    const uint8_t* p = nj::runtime::g_guest_mem->get_ptr(0x80000000u | phys);
    if (!p) return;
    // Gros vidage (>= 64 Ko, une fois par image, point 801685A8 : ~100 Ko, jusqu'a 125 Ko) : il couvre tout le tampon de
    // travail et ne contient que le dernier modele ecrit. Il est 3 fois plus frequent dans r100 (la ou les copies
    // volatiles font des surfaces geantes) que dans r101 ; on ne le copie pas, les petites copies par modele restent
    // actives (28/10). NJ_VOLBIG=1 : ancien comportement.
    static const bool keep_big = NJ_ENV("NJ_VOLNOBIG") == nullptr;   // 28/10 : gros vidage garde par defaut (r101 correcte avec)
    if (len >= 0x10000 && !keep_big) return;
    // Salles sans copie volatile (28/10, constate sur l'iPhone) : r100 (0x100) est correcte SANS copies (avec elles : surfaces
    // geantes) ; r101 (0x101) a besoin des copies (sans elles : scintillements, puis SND SEQ data error). NJ_VOLNOROOM=<hex,hex>
    // remplace la liste, « - » : aucune salle.
    static const std::vector<uint32_t> no_rooms = [] {
        std::vector<uint32_t> v;
        const char* e = NJ_ENV("NJ_VOLNOROOM");
        if (!e) e = "100";
        for (const char* p = e; *p;) {
            if (*p == '-') break;
            v.push_back((uint32_t)std::strtoul(p, const_cast<char**>(&p), 16));
            while (*p == ',' || *p == ' ') ++p;
        }
        return v;
    }();
    // Dans ces salles on enregistre quand meme le vidage, SANS octets : l'analyseur efface alors les copies restees actives
    // de la salle d'avant (sans cela, r100 gardait les copies de r120 : surfaces geantes). Meme effet que NJ_NOVOLCOPY=1.
    bool room_off = false;
    if (!no_rooms.empty()) {
        const uint32_t room = nj::perf::current_room();
        for (uint32_t r : no_rooms)
            if (r == room) room_off = true;
        static bool said = false;
        if (room_off && !said) { said = true; NJ_LOG << "[Vol] salle 0x" << std::hex << room << std::dec << " : copies effacees, plus de copie (build 28/10 17h)" << std::endl; }
    }
    // Comme NJ_NOVOLCOPY=1 (r100 correcte) : rien n'est empile, et a l'entree dans la salle un marqueur (longueur 0, sans octets)
    // fait effacer TOUTES les copies encore actives (celles de la salle d'avant ; un vidage sans octets ne les effacait que
    // s'il les recouvrait, 28/10 : r100 gardait ses surfaces geantes).
    static bool was_off = false;
    if (room_off) {
        if (!was_off) {
            was_off = true;
            std::lock_guard<std::mutex> lock(g_fifo_lock);
            stage_drain_locked();
            g_volatile_pending.push_back({g_fifo_pushed, 0, 0, nullptr});
        }
        return;
    }
    was_off = false;
    {
        // Journal des points d'appel qui vident un tampon (28/10) : copies volatiles correctes dans r101 mais fausses dans
        // r100 ; ce releve compare les deux zones (appels, taille moyenne et maximale, zones recouvertes par point d'appel).
        struct Site { uint64_t n = 0, bytes = 0; uint32_t mx = 0, lo = 0xFFFFFFFFu, hi = 0; };
        static std::unordered_map<uint32_t, Site> sites_stat;
        static uint64_t calls = 0;
        Site& s = sites_stat[(uint32_t)ctx.lr];
        ++s.n; s.bytes += len; s.mx = std::max(s.mx, len); s.lo = std::min(s.lo, phys); s.hi = std::max(s.hi, phys + len);
        if (++calls % 4000 == 0) {
            std::vector<std::pair<uint32_t, Site>> v(sites_stat.begin(), sites_stat.end());
            std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.second.n > b.second.n; });
            std::string line = "[VolSites] 4000 vidages :";
            char b[160];
            for (size_t i = 0; i < v.size() && i < 6; ++i) {
                std::snprintf(b, sizeof b, " lr=%08X n=%llu moy=%u max=%u zone=%06X-%06X |", v[i].first,
                              (unsigned long long)v[i].second.n, (unsigned)(v[i].second.bytes / v[i].second.n), v[i].second.mx,
                              v[i].second.lo, v[i].second.hi);
                line += b;
            }
            NJ_LOG << line << " autres_points=" << (v.size() > 6 ? v.size() - 6 : 0) << std::endl;
            sites_stat.clear();
        }
    }
    std::shared_ptr<const std::vector<uint8_t>> bytes;
    if (copy)
        bytes = std::make_shared<const std::vector<uint8_t>>(p, p + len);
    ++g_prof_vol_calls;
    g_prof_vol_bytes += len;
    std::lock_guard<std::mutex> lock(g_fifo_lock);
    stage_drain_locked();
    g_volatile_pending.push_back({g_fifo_pushed, phys, len, std::move(bytes)});
}

extern "C" void DCStoreRangeNoSync(GuestCpu& ctx) { CopyFlushedRange(ctx); }
extern "C" void DCFlushRangeNoSync(GuestCpu& ctx) { CopyFlushedRange(ctx); }

// Banc d'essai (FifoReplay) : meme chemin que CopyFlushedRange, sans le jeu. La copie est mise en file a la position
// courante du flux ; l'analyseur l'active quand il atteint cette position, comme sur l'appareil.
static void ReplayVolatileImpl(uint32_t phys, std::shared_ptr<const std::vector<uint8_t>> bytes) {
    if (!bytes || bytes->empty()) return;
    std::lock_guard<std::mutex> lock(g_fifo_lock);
    stage_drain_locked();
    const uint32_t len = (uint32_t)bytes->size();
    g_volatile_pending.push_back({g_fifo_pushed, phys & 0x01FFFFFFu, len, std::move(bytes)});
}





namespace hw { extern uint32_t g_fifo_cfg_gen; }   // hw_pi.cpp

// Aiguillage du tuyau (vers la memoire quand le PI ecrit ailleurs que la FIFO du CP : liste
// d'affichage en construction). Il etait recalcule a chaque octet ecrit : deux appels hors du
// fichier et la MMU, ~7 % du fil du jeu au Galaxy S5 (profil du 29/09). Il ne change qu'aux
// ecritures des bases de FIFO (g_fifo_cfg_gen). NJ_WGPNOCACHE=1 : recalcul a chaque octet.
static uint32_t s_redir_gen = 0;
static bool s_redir = false;

// Le PI ecrit-il ailleurs que dans la FIFO du CP (liste d'affichage construite en memoire) ?
static bool pipe_targets_memory(uint32_t& base, uint32_t& end, uint32_t& wptr) {
    nj::runtime::hw::cpu_fifo_get(base, end, wptr);
    return base != 0 && (base & 0x03FFFFFF) != (nj::runtime::hw::cp_fifo_base() & 0x03FFFFFF);
}

// Un octet du tuyau ecrit en memoire a la position d'ecriture du PI (avec retour au debut de la
// FIFO a sa fin) ; false quand le tuyau va au CP.
static bool wgp_reroute(uint8_t b) {
    static const bool nocache = NJ_ENV("NJ_WGPNOCACHE") != nullptr;
    uint32_t base = 0, end = 0, wptr = 0;
    if (!nocache) {
        if (s_redir_gen != hw::g_fifo_cfg_gen) [[unlikely]] {
            s_redir = pipe_targets_memory(base, end, wptr);
            s_redir_gen = hw::g_fifo_cfg_gen;
        }
        if (!s_redir) [[likely]]
            return false;
    }
    if (!pipe_targets_memory(base, end, wptr) || !nj::runtime::g_guest_mem)
        return false;
    nj::runtime::g_guest_mem->write8(0x80000000u | (wptr & 0x01FFFFFFu), b);
    ++wptr;
    if ((wptr & 0x03FFFFFFu) > (end & 0x03FFFFFFu))
        wptr = base;
    nj::runtime::hw::cpu_fifo_set_wptr(wptr);
    return true;
}

// Ecritures en ligne du code genere (g_nj_wgp) : permises seulement si le tampon est actif et le
// tuyau n'est pas redirige vers une FIFO en memoire. Appele a l'activation et a chaque ecriture
// d'une base de FIFO (GX_FifoCfgChanged, hw_pi / hw_cp). NJ_NOWGPINLINE=1 : jamais (A/B).
static void wgp_refresh_inline() {
    static const bool off = NJ_ENV("NJ_NOWGPINLINE") != nullptr || NJ_ENV("NJ_WGPNOCACHE") != nullptr;
    uint32_t base = 0, end = 0, wptr = 0;
    const bool redir = pipe_targets_memory(base, end, wptr);
    nj::runtime::g_nj_wgp.lim = (s_stage_on && !redir && !off) ? s_stage + sizeof(s_stage) : s_stage;
}
void GX_FifoCfgChanged() { wgp_refresh_inline(); }

// main.cpp, avant le demarrage du fil CPU.
void GX_EnableStaging(bool on) {
    s_stage_on = on;
    wgp_refresh_inline();
}

// Fil CPU seulement (drain_callbacks).
void GX_StageFlush() {
    if (s_stage_n == 0) return;
    wgp_backpressure();
    std::lock_guard<std::mutex> lock(g_fifo_lock);
    stage_drain_locked();
}

// NJ_FIFOHASH (voir fifohash) : une ligne par VI du jeu (ios.cpp), apres versement du tampon :
// « vi hachage octets ». Aux VI 100, 300 et 600, le hachage de MEM1 en plus (informatif : des
// champs d'horloge peuvent y differer sans changer l'image).
void GX_FifoHashMark(uint64_t vi, GuestCpu& ctx) {
    if (!s_fifohash_path) return;
    {
        std::lock_guard<std::mutex> lock(g_fifo_lock);
        stage_drain_locked();
    }
    static FILE* f = std::fopen(s_fifohash_path, "w");
    if (!f) return;
    std::fprintf(f, "%llu %016llx %llu", (unsigned long long)vi, (unsigned long long)s_fifohash,
                 (unsigned long long)s_fifohash_n);
    if (vi == 100 || vi == 300 || vi == 600) {
        uint64_t h = 0xCBF29CE484222325ull;
        const uint8_t* m = ctx.mmu.mem1.data();
        for (size_t i = 0; i < ctx.mmu.mem1.size(); ++i) h = (h ^ m[i]) * 0x100000001B3ull;
        std::fprintf(f, " mem1=%016llx", (unsigned long long)h);
        // NJ_FIFOHASH_DUMP=1 : MEM1 entiere a cote (<fichier>.vi<N>.bin), pour chercher ce qui
        // differe entre deux parties.
        if (NJ_ENV("NJ_FIFOHASH_DUMP")) {
            char name[1024];
            std::snprintf(name, sizeof name, "%s.vi%llu.bin", s_fifohash_path, (unsigned long long)vi);
            if (FILE* d = std::fopen(name, "wb")) {
                std::fwrite(m, 1, ctx.mmu.mem1.size(), d);
                std::fclose(d);
            }
        }
    }
    std::fprintf(f, "\n");
    std::fflush(f);
}

// Ecriture du jeu dans le tuyau (gros-boutiste, comme sur le bus) : vers la memoire si le PI y
// pointe (le premier octet decide pour toute l'ecriture), sinon dans le tampon du fil CPU, sinon
// directement dans le FIFO.
static void wgp_write(const uint8_t* bytes, size_t n) {
    wgp_backpressure();
    if (wgp_reroute(bytes[0])) {
        for (size_t i = 1; i < n; ++i) wgp_reroute(bytes[i]);
        return;
    }
    if (s_stage_on) {
        stage_put(bytes, n);
        return;
    }
    std::lock_guard<std::mutex> lock(g_fifo_lock);
    fifo_append_locked(bytes, n);
}

template <typename T>
static inline void wgp_write_be(T v) {
    uint8_t bytes[sizeof(T)];
    for (size_t k = 0; k < sizeof(T); ++k) bytes[k] = uint8_t(v >> (8 * (sizeof(T) - 1 - k)));
    wgp_write(bytes, sizeof(T));
}

void GX_WGPIPE_Write8(uint8_t val) { wgp_write(&val, 1); }
void GX_WGPIPE_Write16(uint16_t val) { wgp_write_be(val); }
void GX_WGPIPE_Write32(uint32_t val) { wgp_write_be(val); }
void gx_wgpipe_f32(float val) {
    uint32_t bits;
    std::memcpy(&bits, &val, 4);
    wgp_write_be(bits);
}
void gx_wgpipe_f64(double val) {
    uint64_t bits;
    std::memcpy(&bits, &val, 8);
    wgp_write_be(bits);
}

} 
uint64_t g_prof_decode_us_accum = 0;

// Boucle hote, entre deux images du jeu : une image intermediaire si la cadence le demande.
bool GXInterpTick() {
    return s_host_window && g_renderer && nj::runtime::gx::interp_tick(g_renderer.get());
}

// Banc FifoReplay : dessine B exacte tout de suite (voir NJ_REPLAY_INTERP).
void GXReplayVolatile(uint32_t phys, std::shared_ptr<const std::vector<uint8_t>> bytes) {
    ReplayVolatileImpl(phys, std::move(bytes));
}
bool GXInterpTestFinal() {
    return g_renderer && nj::runtime::gx::interp_test_final(g_renderer.get());
}
