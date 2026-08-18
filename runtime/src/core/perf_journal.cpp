#include "runtime/perf_journal.h"
#include "runtime/dev_tour.h"
#include "runtime/thread_place.h"
#include "runtime/gx/fifo_parser.h"
#include "runtime/hw/hw.h"

#include "common/platform.h"
#include "runtime/cpu_context.h"
#include "runtime/game_version.h"
#include "runtime/gfx_settings.h"
#include "input/re4_context.h"
#include "runtime/diag.h"
#include "runtime/obf.h"
// Temps cumules (hle/gx.cpp, toujours comptes) : analyse du flux et part du decodage des sommets ; le journal en note la moyenne par image (an=, dec=).
extern uint64_t g_prof_parse_us, g_prof_pool_us;
extern uint64_t g_stat_draws;   // gx.cpp (espace global)
namespace nj::runtime::gx { extern std::atomic<uint32_t> g_jr_gldraw, g_jr_gluni, g_jr_setup; std::string rprof_journal_fields(double frames); }
using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif
#else
#include <unistd.h>
#endif

namespace nj::runtime {
extern GuestCpu *g_guest;
}

namespace nj::perf {

std::atomic<uint64_t> g_disc_us{0}, g_disc_bytes{0};
std::atomic<uint32_t> g_tex_stale{0};
std::atomic<uint32_t> g_ax_voices{0}, g_ax_blocks{0};
std::atomic<uint32_t> g_ax_ticks{0}, g_unstick{0};
std::atomic<uint32_t> g_room_serial{0};
std::atomic<uint32_t> g_room_file{0};   // salle d'apres le dernier rXYZ.das lu (0 : aucune)

namespace {

namespace fs = std::filesystem;

// Cle du journal (le depot reste prive ; tools/p3/journal_perf.py porte la meme). Rangee masquee
// dans l'executable, demasquee a l'ouverture de la seance.
constexpr uint8_t kMask = 0xA7;
constexpr uint8_t kKeyMasked[32] = {
    0x35 ^ kMask, 0x68 ^ kMask, 0xe2 ^ kMask, 0x33 ^ kMask, 0xb8 ^ kMask, 0x6d ^ kMask, 0x7b ^ kMask, 0xfd ^ kMask,
    0xa1 ^ kMask, 0x50 ^ kMask, 0xfb ^ kMask, 0x1e ^ kMask, 0x1a ^ kMask, 0x58 ^ kMask, 0x01 ^ kMask, 0xa2 ^ kMask,
    0x1f ^ kMask, 0x5d ^ kMask, 0x2f ^ kMask, 0xbf ^ kMask, 0x09 ^ kMask, 0xec ^ kMask, 0x2e ^ kMask, 0xb0 ^ kMask,
    0xf6 ^ kMask, 0x75 ^ kMask, 0xce ^ kMask, 0xfd ^ kMask, 0x6b ^ kMask, 0xd1 ^ kMask, 0xca ^ kMask, 0x59 ^ kMask};

// ---- ChaCha20 (RFC 8439) ----------------------------------------------------------------------

uint32_t rotl(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

void chacha_block(const uint32_t key[8], uint32_t counter, const uint32_t nonce[3], uint8_t out[64]) {
    uint32_t s[16] = {0x61707865, 0x3320646e, 0x79622d32, 0x6b206574, key[0], key[1], key[2], key[3],
                      key[4],     key[5],     key[6],     key[7],     counter, nonce[0], nonce[1], nonce[2]};
    uint32_t x[16];
    std::memcpy(x, s, sizeof x);
    auto qr = [&](int a, int b, int c, int d) {
        x[a] += x[b]; x[d] = rotl(x[d] ^ x[a], 16);
        x[c] += x[d]; x[b] = rotl(x[b] ^ x[c], 12);
        x[a] += x[b]; x[d] = rotl(x[d] ^ x[a], 8);
        x[c] += x[d]; x[b] = rotl(x[b] ^ x[c], 7);
    };
    for (int i = 0; i < 10; ++i) {
        qr(0, 4, 8, 12); qr(1, 5, 9, 13); qr(2, 6, 10, 14); qr(3, 7, 11, 15);
        qr(0, 5, 10, 15); qr(1, 6, 11, 12); qr(2, 7, 8, 13); qr(3, 4, 9, 14);
    }
    for (int i = 0; i < 16; ++i) {
        const uint32_t v = x[i] + s[i];
        out[4 * i] = uint8_t(v);
        out[4 * i + 1] = uint8_t(v >> 8);
        out[4 * i + 2] = uint8_t(v >> 16);
        out[4 * i + 3] = uint8_t(v >> 24);
    }
}

// ---- seance ----------------------------------------------------------------------------------

struct Second {
    uint32_t frames = 0;
    float sum_dt = 0, max_dt = 0, parse = 0, render = 0, swap = 0, game = 0, disc_ms = 0;
    uint64_t disc_bytes = 0;
    uint32_t over50 = 0, over100 = 0;
};

struct Session {
    std::mutex m;
    FILE *f = nullptr;
    uint32_t key[8]{}, nonce[3]{};
    uint64_t offset = 0;   // octets chiffres ecrits (position dans le flot ChaCha20)
    std::chrono::steady_clock::time_point t0, sec_start;
    Second cur;
    int last_scale = -1, last_setting = -1, last_aniso = -1, last_fps = -1;
    uint64_t last_disc_us = 0, last_disc_bytes = 0;
    std::vector<std::string> early;   // evenements d'avant l'ouverture (installation P3)
} g;

std::string journal_dir() {
#if defined(_WIN32)
    char d[1024];
    const DWORD n = GetEnvironmentVariableA("LOCALAPPDATA"_g, d, sizeof d);
    if (!n || n >= sizeof d) return {};
    return std::string(d) + "/Nightjar/journal"_g;
#else
    const std::string base = nj::writable_path(""_g);
    return base.empty() ? std::string() : base + "/journal"_g;
#endif
}

// Ecrit une ligne chiffree (appelant : verrou tenu) : flot ChaCha20 continu sur toute la seance.
void put(const std::string &line) {
    if (!g.f) return;
    std::string data = line + "\n"_g;
    static uint8_t ks[64];
    static uint64_t ks_block = UINT64_MAX;
    for (char &ch : data) {
        const uint64_t blk = g.offset / 64;
        if (blk != ks_block) {
            chacha_block(g.key, uint32_t(blk + 1), g.nonce, ks);
            ks_block = blk;
        }
        ch = char(uint8_t(ch) ^ ks[g.offset % 64]);
        ++g.offset;
    }
    std::fwrite(data.data(), 1, data.size(), g.f);
}

double secs() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - g.t0).count(); }

uint32_t room_id() {
    if (const uint32_t r = g_room_file.load()) return r;   // d'apres les fichiers lus : toutes versions
    using namespace nj::runtime;
    GuestCpu *c = g_guest;
    if (!c) return 0;
    const uint32_t pg = c->mmu.read32(ga(0x80314BC8u));   // Global *pG
    if (pg < 0x80000000u || pg >= 0x81800000u) return 0;
    return c->mmu.read16(pg + nj::runtime::pg(0x4F9C));                    // stage_no << 8 | room_no
}

// 20/10 : memoire residente du processus en Mo (une fuite lente : std::bad_alloc apres 2 h 24 sur MuMu le 17/10, sans trace de ce qui grossissait).
unsigned rss_mb() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) return (unsigned)(pmc.WorkingSetSize >> 20);
    return 0;
#else
    unsigned long total = 0, res = 0;
    if (FILE *f = std::fopen("/proc/self/statm", "r")) {
        if (std::fscanf(f, "%lu %lu", &total, &res) != 2) res = 0;
        std::fclose(f);
    }
    return (unsigned)((res * 4096ul) >> 20);
#endif
}

void settings_line_if_changed() {
    namespace gfx = nj::runtime::gfx;
    if (gfx::g_scale == g.last_scale && gfx::g_scale_setting == g.last_setting && gfx::g_aniso == g.last_aniso &&
        gfx::g_fps_target == g.last_fps)
        return;
    g.last_scale = gfx::g_scale;
    g.last_setting = gfx::g_scale_setting;
    g.last_aniso = gfx::g_aniso;
    g.last_fps = gfx::g_fps_target;
    char b[160];
    std::snprintf(b, sizeof b, "R %.1f scale=%d setting=%d aniso=%d fps=%d"_g, secs(), g.last_scale, g.last_setting,
                  g.last_aniso, g.last_fps);
    put(b);
}

std::string cpu_name() {
#if defined(_WIN32) && (defined(__x86_64__) || defined(__i386__))
    unsigned regs[12] = {};
    if (__get_cpuid(0x80000000u, &regs[0], &regs[1], &regs[2], &regs[3]) && regs[0] >= 0x80000004u) {
        char name[49] = {};
        for (unsigned i = 0; i < 3; ++i) {
            unsigned a, b, c, d;
            __get_cpuid(0x80000002u + i, &a, &b, &c, &d);
            std::memcpy(name + 16 * i, &a, 4);
            std::memcpy(name + 16 * i + 4, &b, 4);
            std::memcpy(name + 16 * i + 8, &c, 4);
            std::memcpy(name + 16 * i + 12, &d, 4);
        }
        std::string s(name);
        s.erase(0, s.find_first_not_of(' '));
        return s;
    }
#endif
    return "?"_g;
}

uint64_t ram_mb() {
#if defined(_WIN32)
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    return GlobalMemoryStatusEx(&ms) ? ms.ullTotalPhys >> 20 : 0;
#else
    const long pages = sysconf(_SC_PHYS_PAGES), page = sysconf(_SC_PAGESIZE);
    return pages > 0 && page > 0 ? ((uint64_t)pages * (uint64_t)page) >> 20 : 0;
#endif
}

}  // namespace

uint32_t current_room() { return room_id(); }

// Detecteur de gel (15/10) : plus aucune image du jeu pendant 1,5 s (19/10 : 6 s avant ; les journaux
// MuMu du 18/10 montrent des gels de 2,9 s ; les deux de 14 s etaient au repos dans SelectThread) -> evenement « gel » avec
// 6 echantillons du fil du jeu (pc/lr, 100 ms d'ecart), les instructions executees pendant ce temps
// (0 : le jeu attend un evenement), l'etat du lecteur de disque et ses dernieres commandes ; « gel fin »
// a la reprise. Les gels des joueurs (reset du jeu, r224) disent ainsi ou ils tournent.
std::atomic<int64_t> g_last_frame_ms{0};
int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
int64_t ms_since_frame() {
    const int64_t l = g_last_frame_ms.load(std::memory_order_relaxed);
    return l ? now_ms() - l : 0;
}
void gel_watch() {
    int64_t gel_t0 = 0;
    uint32_t axt0 = 0, unst0 = 0;
    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        const int64_t last = g_last_frame_ms.load(std::memory_order_relaxed);
        if (!last) continue;
        const int64_t idle = now_ms() - last;
        if (idle < 1500) {
            if (gel_t0) {
                char b[64];
                std::snprintf(b, sizeof b, "gel fin %.1f s axt=+%u unst=+%u"_g, (now_ms() - gel_t0) / 1000.0, g_ax_ticks.load() - axt0, g_unstick.load() - unst0);
                event(b);
                gel_t0 = 0;
            }
            continue;
        }
        if (gel_t0) continue;
        gel_t0 = last;
        axt0 = g_ax_ticks.load();
        unst0 = g_unstick.load();
        std::string s = "gel"_g;
        char b[48];
        if (nj::runtime::GuestCpu *c = nj::runtime::g_guest) {
            std::snprintf(b, sizeof b, " th=%08x"_g, c->mmu.read32(0x800000E4u));   // OS : fil courant
            s += b;
            const uint64_t r0 = c->retired;
            for (int i = 0; i < 6; ++i) {
                std::snprintf(b, sizeof b, " %08x/%08x"_g, c->pc, c->lr);
                s += b;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            std::snprintf(b, sizeof b, " ret=%llu cb=%d poll=%d msr=%x"_g, (unsigned long long)(c->retired - r0),
                          (int)c->inside_cb, (int)nj::runtime::hw::poll_now.load(), (unsigned)c->msr);
            s += b;
            s += nj::runtime::hw::di_debug_dump();
        }
        event(s.c_str());
    }
}

void session_start(const char *gl_renderer) {
    static const bool watch = [] {
        std::thread(gel_watch).detach();
        return true;
    }();
    (void)watch;
    std::lock_guard<std::mutex> l(g.m);
    if (g.f) return;
    const std::string dir = journal_dir();
    if (dir.empty()) return;
    std::error_code ec;
    fs::create_directories(dir, ec);
    // 10 seances gardees : les plus anciennes partent
    std::vector<fs::path> old;
    for (const auto &e : fs::directory_iterator(dir, ec))
        if (e.path().extension() == ".njl"_g) old.push_back(e.path());
    std::sort(old.begin(), old.end());
    while (old.size() >= 10) {
        fs::remove(old.front(), ec);
        old.erase(old.begin());
    }
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char name[64];
    std::strftime(name, sizeof name, "%Y%m%d_%H%M%S.njl"_g, std::localtime(&now));
    g.f = std::fopen((dir + "/"_g + name).c_str(), "wb"_g);
    if (!g.f) return;
    for (int i = 0; i < 8; ++i) {
        uint32_t w = 0;
        for (int b = 0; b < 4; ++b) w |= uint32_t(kKeyMasked[4 * i + b] ^ kMask) << (8 * b);
        g.key[i] = w;
    }
    std::random_device rd;
    for (uint32_t &n : g.nonce) n = rd();
    std::fwrite("NJPJ1"_g, 1, 5, g.f);   // en clair : format et nonce
    std::fwrite(g.nonce, 1, sizeof g.nonce, g.f);
    g.t0 = g.sec_start = std::chrono::steady_clock::now();
    char b[512];
    std::snprintf(b, sizeof b, "H cpu=\"%s\" cores=%u ram_mb=%llu gpu=\"%s\" version=%s"_g, cpu_name().c_str(),
                  std::thread::hardware_concurrency(), (unsigned long long)ram_mb(),
                  gl_renderer ? gl_renderer : "?"_g, nj::runtime::game_version_name(nj::runtime::game_version()));
    put(b);
    settings_line_if_changed();
    for (const std::string &e : g.early) put(e);
    g.early.clear();
    std::fflush(g.f);
}

void frame(float dt, float parse, float render, float swap, float game) {
    nj::dev::tour_tick();   // 20/10 : visite automatique des salles (vide hors NJ_DEVTOOLS)
    g_last_frame_ms.store(now_ms(), std::memory_order_relaxed);
    std::lock_guard<std::mutex> l(g.m);
    if (!g.f) return;
    Second &s = g.cur;
    const uint64_t dus = g_disc_us.load(), dby = g_disc_bytes.load();
    const float disc_ms = (dus - g.last_disc_us) / 1000.0f;
    const uint64_t disc_b = dby - g.last_disc_bytes;
    g.last_disc_us = dus;
    g.last_disc_bytes = dby;
    s.disc_ms += disc_ms;
    s.disc_bytes += disc_b;
    ++s.frames;
    s.sum_dt += dt;
    s.max_dt = std::max(s.max_dt, dt);
    s.parse += parse;
    s.render += render;
    s.swap += swap;
    s.game += game;
    if (dt > 50) ++s.over50;
    if (dt > 100) {
        ++s.over100;
        char b[160];   // saccade : l'image entiere, avec la salle
        std::snprintf(b, sizeof b, "F %.2f room=%04x dt=%.0f parse=%.1f render=%.1f swap=%.1f game=%.1f disc=%.0f/%llu"_g,
                      secs(), room_id(), dt, parse, render, swap, game, disc_ms, (unsigned long long)disc_b);
        put(b);
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - g.sec_start < std::chrono::seconds(1)) return;
    settings_line_if_changed();
    const float n = (float)std::max<uint32_t>(1, s.frames);
    char b[200];
    const int mode = (int)nj::runtime::input::re4_context_read().mode;   // 1 jeu, 2 cinematique, 4 mallette...
    std::snprintf(b, sizeof b,
                  "S %.0f room=%04x mode=%d n=%u avg=%.1f max=%.0f p=%.1f r=%.1f w=%.1f g=%.1f o50=%u o100=%u d=%.0f "
                  "db=%llu"_g,
                  secs(), room_id(), mode, s.frames, s.sum_dt / n, s.max_dt, s.parse / n, s.render / n, s.swap / n,
                  s.game / n, s.over50, s.over100, s.disc_ms, (unsigned long long)s.disc_bytes);
    // 15/10 : temps processeur reel par fil, coeur, frequence, marge thermique (thread_place.h)
    std::string line = std::string(b) + nj::runtime::place::second_fields();
    if (nj::runtime::gfx::g_early_drawdone) {   // levier A : dessins dont la memoire a bouge
        char e[32];
        const uint32_t rc = nj::runtime::gx::g_race_draws.exchange(0);
        std::snprintf(e, sizeof e, " edd=1 rc=%u"_g, rc);
        line += e;
        if (rc) line += " rp="_g + nj::runtime::gx::TakeRacePages();
    }
    {   // 20/10 : ou va le temps du fil de lecture, par image (analyse totale, dont decodage des sommets), en ms
        static uint64_t last_parse = 0, last_pool = 0;
        const uint64_t pa = g_prof_parse_us, po = g_prof_pool_us;
        char d[40];
        std::snprintf(d, sizeof d, " an=%.1f dec=%.1f"_g, (double)(pa - last_parse) / 1000.0 / n, (double)(po - last_pool) / 1000.0 / n);
        line += d;
        last_parse = pa;
        last_pool = po;
    }
    {   // phase A (27/10) : dessins GX, dessins GL, uniformes GL et changements d'etat par image (fil hote)
        static uint64_t last_gd = 0;
        const uint64_t gd = ::g_stat_draws;
        const uint32_t gl = nj::runtime::gx::g_jr_gldraw.exchange(0), un = nj::runtime::gx::g_jr_gluni.exchange(0), su = nj::runtime::gx::g_jr_setup.exchange(0);
        char d[64];
        std::snprintf(d, sizeof d, " gx=%.0f gl=%.0f un=%.0f su=%.0f"_g, (double)(gd - last_gd) / n, (double)gl / n, (double)un / n, (double)su / n);
        line += d;
        last_gd = gd;
    }
    line += nj::runtime::gx::rprof_journal_fields((double)n);   // chronos du rendu par poste (ms par image)
    if (const uint32_t ts = g_tex_stale.exchange(0)) line += " ts="_g + std::to_string(ts);
    {
        const uint32_t av = g_ax_voices.exchange(0), ab = g_ax_blocks.exchange(0);
        char a[24];
        std::snprintf(a, sizeof a, " axv=%.1f"_g, ab ? (double)av / ab : 0.0);
        line += a;
    }
    {
        char m[24];
        std::snprintf(m, sizeof m, " rss=%u"_g, rss_mb());
        line += m;
    }
    put(line);
    s = Second{};
    g.sec_start = now;
    std::fflush(g.f);
}

void note_file(const std::string &path) {
    // ".../rXYZ.das" : stage X, salle YZ (hexadecimal, comme les noms de fichiers du disque)
    const size_t slash = path.find_last_of('/');
    const std::string name = path.substr(slash == std::string::npos ? 0 : slash + 1);
    if (name.size() != 8 || name[0] != 'r' || name.compare(4, 4, ".das"_g) != 0) return;
    const uint32_t room = (uint32_t)std::strtoul(name.substr(1, 3).c_str(), nullptr, 16);
    if (!room || room == g_room_file.exchange(room)) return;
    g_room_serial.fetch_add(1, std::memory_order_relaxed);
    char b[32];
    std::snprintf(b, sizeof b, "room r%03x"_g, room);
    event(b);
}

void flush() {
    std::lock_guard<std::mutex> l(g.m);
    if (g.f) std::fflush(g.f);
}

void event(const char *what) {
    std::lock_guard<std::mutex> l(g.m);
    char b[640];
    std::snprintf(b, sizeof b, "E %.1f %s"_g, g.f ? secs() : 0.0, what);
    if (!g.f) {
        g.early.push_back(b);
        return;
    }
    put(b);
    std::fflush(g.f);
}

}  // namespace nj::perf

// NJ_DIAG du build de diffusion (runtime/diag.h) : empreinte du message et ses valeurs
void nj::diag::record(uint32_t code, const std::string &values) {
    char b[24];
    std::snprintf(b, sizeof b, "D %08x"_g, code);
    nj::perf::event((std::string(b) + values).c_str());
}
