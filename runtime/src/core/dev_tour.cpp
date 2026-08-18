// Visite automatique des salles : voir runtime/dev_tour.h. Tout le code est sous NJ_DEVTOOLS.
#include "runtime/dev_tour.h"

#if defined(NJ_DEVTOOLS)

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iterator>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "common/platform.h"
#include "input/input_source.h"
#include "input/re4_context.h"
#include "runtime/cpu_context.h"
#include "runtime/game_version.h"
#include "runtime/env.h"
#include "runtime/perf_journal.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace nj::runtime {
extern GuestCpu *g_guest;
}

namespace nj::dev {
namespace {

using Clock = std::chrono::steady_clock;

struct Point {
    float x, y, z, ang;
};
struct Stop {
    uint16_t room;
    int secs;
    bool has_cam = false;   // « cam=x,y,z,ax,ay,az,fov » : camera libre imposee pendant la mesure (comparaison avec Dolphin, tools/re4/live_debug.py)
    float cam[7] = {};
};
enum class St { Idle, WaitReady, Jump, Arrive, Measure };

struct Tour {
    St st = St::Idle;
    std::map<uint16_t, Point> points;   // salle -> point d'entree appris
    bool points_loaded = false;
    std::vector<Stop> stops;
    size_t idx = 0;
    Clock::time_point t_state, t_check, t_life, t_room, t_cam;
    uint16_t last_room = 0;
    bool learned = false;
    int ok = 0, fail = 0;
    bool shot_done = false;
} T;

std::string dir();
}  // namespace
int knob(const char *name, int def) {
    // Coeur natif (NJ_NATIVE) : pas de reglages en direct. Chaque appel prenait un verrou, lisait l'horloge et cherchait
    // une chaine (~5 % du fil du jeu au S5, simpleperf 13/11, appele par liste et par dessin rejoue).
    static const bool native = std::getenv("NJ_NATIVE") != nullptr && std::getenv("NJ_KNOBS") == nullptr;
    if (native) return def;
    static std::mutex mu;
    static std::map<std::string, int> kv;
    static Clock::time_point last;
    std::lock_guard<std::mutex> lk(mu);
    const auto now = Clock::now();
    if (std::chrono::duration<double>(now - last).count() > 0.4) {
        last = now;
        kv.clear();
        std::ifstream f(dir() + "/knobs.txt");
        std::string k;
        int v;
        while (f >> k >> v) kv[k] = v;
    }
    const auto it = kv.find(name);
    return it == kv.end() ? def : it->second;
}
namespace {
std::string dir() {
#if defined(_WIN32)
    char d[1024];
    const DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", d, sizeof d);
    return n && n < sizeof d ? std::string(d) + "/Nightjar" : std::string();
#else
    return nj::writable_path("");
#endif
}

double since(Clock::time_point t) { return std::chrono::duration<double>(Clock::now() - t).count(); }

void note(const char *fmt, unsigned a = 0, unsigned b = 0, unsigned c = 0) {
    char s[96];
    std::snprintf(s, sizeof s, fmt, a, b, c);
    nj::perf::event(s);
}

void load_points() {
    T.points_loaded = true;
    std::ifstream f(dir() + "/tour_points.txt");
    unsigned r;
    Point p;
    while (f >> std::hex >> r >> std::dec >> p.x >> p.y >> p.z >> p.ang) T.points[(uint16_t)r] = p;
}

void save_point(uint16_t room, const Point &p) {
    std::ofstream f(dir() + "/tour_points.txt", std::ios::app);
    char s[160];
    std::snprintf(s, sizeof s, "%03x %.2f %.2f %.2f %.4f\n", room, p.x, p.y, p.z, p.ang);
    f << s;
}

bool file_exists(const std::string &p) {
    std::ifstream f(p);
    return (bool)f;
}

bool parse_tour() {
    T.stops.clear();
    std::ifstream f(dir() + "/tour.txt");
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream is(line);
        std::string w;
        int secs = 20;
        is >> w;
        if (w == "auto") {   // toutes les salles apprises, dans l'ordre
            is >> secs;
            for (const auto &kv : T.points) T.stops.push_back(Stop{kv.first, secs ? secs : 20});
        } else if (w.size() >= 2 && (w[0] == 'r' || w[0] == 'R')) {
            is >> secs;
            std::string opt;
            Stop st{};
            while (is >> opt)
                if (opt.compare(0, 4, "cam=") == 0 &&
                    std::sscanf(opt.c_str() + 4, "%f,%f,%f,%f,%f,%f,%f", &st.cam[0], &st.cam[1], &st.cam[2], &st.cam[3], &st.cam[4], &st.cam[5], &st.cam[6]) == 7)
                    st.has_cam = true;
            const uint16_t room = (uint16_t)std::strtoul(w.c_str() + 1, nullptr, 16);
            if (!T.points.count(room)) {   // point jamais appris : zeros = « entree par defaut » (room_jmp.cpp setNextPos : drapeau 1 absent -> 0)
                T.points[room] = Point{0.f, 0.f, 0.f, 0.f};
                note("tour r%03x : entree par defaut (point jamais appris)", room);
            }
            st.room = room;
            st.secs = secs ? secs : 20;
            T.stops.push_back(st);
        }
    }
    return !T.stops.empty();
}

// Profil statistique du code du jeu pendant la mesure d'une salle : un fil lit le pc de l'invite toutes les ~1 ms (le pc n'est ecrit qu'aux appels : la
// precision est celle de la fonction), compte par adresse ; a la fin de la salle, les 48 adresses les plus frequentes vont au journal (« tour prof »),
// resolues ensuite par tools/tour/prof_table.py contre les symboles du jeu.
std::atomic<bool> g_sampling{false};
std::mutex g_prof_mu;
std::unordered_map<uint32_t, uint32_t> g_prof;
std::atomic<uint32_t> g_prof_n{0};

void sampler_thread() {
    for (;;) {
        std::this_thread::sleep_for(std::chrono::microseconds(900));
        if (!g_sampling.load(std::memory_order_relaxed)) continue;
        nj::runtime::GuestCpu *c = nj::runtime::g_guest;
        if (!c) continue;
        const uint32_t pc = c->pc;
        std::lock_guard<std::mutex> l(g_prof_mu);
        ++g_prof[pc];
        g_prof_n.fetch_add(1, std::memory_order_relaxed);
    }
}

std::mutex g_shot_mu;
std::string g_shot_path;

void prof_start() {
    static const bool once = [] {
        std::thread(sampler_thread).detach();
        return true;
    }();
    (void)once;
    {
        std::lock_guard<std::mutex> l(g_prof_mu);
        g_prof.clear();
        g_prof_n = 0;
    }
    g_sampling = true;
}

void prof_report(uint16_t room) {
    g_sampling = false;
    std::vector<std::pair<uint32_t, uint32_t>> v;
    uint32_t total = 0;
    {
        std::lock_guard<std::mutex> l(g_prof_mu);
        total = g_prof_n.load();
        v.assign(g_prof.begin(), g_prof.end());
    }
    std::sort(v.begin(), v.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
    if (v.size() > 48) v.resize(48);
    char head[48];
    std::snprintf(head, sizeof head, "tour prof r%03x n=%u", (unsigned)room, total);
    std::string line = head;
    for (const auto &e : v) {
        char b[24];
        std::snprintf(b, sizeof b, " %x:%u", (unsigned)e.first, (unsigned)e.second);
        line += b;
        if (line.size() > 560) {
            nj::perf::event(line.c_str());
            line = head;
        }
    }
    if (line != head) nj::perf::event(line.c_str());
}

}  // namespace

bool shot_pending(std::string &path) {
    std::lock_guard<std::mutex> l(g_shot_mu);
    if (g_shot_path.empty()) return false;
    path = g_shot_path;
    g_shot_path.clear();
    return true;
}

void tour_tick() {
    using namespace nj::runtime;
    GuestCpu *c = g_guest;
    if (!c) return;
    if (knob("camon", 0)) {   // camera libre pilotee par knobs.txt (cx cy cz ax ay az en unites du jeu, fov en degres) : balayage d'angles pour les rayures de la grange
        const uint32_t g0 = c->mmu.read32(ga(0x80314BC8u));
        if (g0 >= 0x80000000u && g0 < 0x81800000u) {
            c->mmu.write32(g0 + 0x60, c->mmu.read32(g0 + 0x60) | 0x10000000u);
            c->mmu.write_f32(g0 + 0x118, (float)knob("cx", 0));
            c->mmu.write_f32(g0 + 0x11C, (float)knob("cy", 0));
            c->mmu.write_f32(g0 + 0x120, (float)knob("cz", 0));
            c->mmu.write_f32(g0 + 0x124, (float)knob("ax", 0));
            c->mmu.write_f32(g0 + 0x128, (float)knob("ay", 0));
            c->mmu.write_f32(g0 + 0x12C, (float)knob("az", 0));
            c->mmu.write_f32(g0 + 0x134, (float)knob("fov", 50));
            c->mmu.write_f32(g0 + 0x138, 0.f);
            c->mmu.write_f32(g0 + 0x13C, 1.f);
            c->mmu.write_f32(g0 + 0x140, 0.f);
        }
    }
    if (since(T.t_check) < 0.1) return;
    T.t_check = Clock::now();
    if (!T.points_loaded) load_points();
    {   // NJ_BOOTDIRECT (dev_env.txt) : logos sautes. Le crochet d'OSReport (guest_services.cpp) attend « RESET_KEEP_WORK »,
        // absent du chemin Android : on pose le meme drapeau (pRK+0x17, lu par titleWait) jusqu'au premier passage en jeu.
        static const bool on = NJ_ENV("NJ_BOOTDIRECT") != nullptr;
        static bool fini = false;
        if (on && !fini) {
            const auto m = nj::runtime::input::re4_context_read().mode;
            if (m == nj::runtime::input::Re4Mode::Gameplay) {
                fini = true;
            } else {
                const uint32_t prk = c->mmu.read32(ga(0x80315AD4u));   // pRK (symboles du decompile)
                if (prk >= 0x80000000u && prk < 0x81800000u) c->mmu.write8(prk + 0x17, 1);
            }
        }
    }

    const uint32_t g = c->mmu.read32(ga(0x80314BC8u));   // Global *pG
    if (g < 0x80000000u || g >= 0x81800000u) return;
    const uint16_t room = c->mmu.read16(g + pg(0x4F9C));
    const auto ctx = nj::runtime::input::re4_context_read();
    const bool play = ctx.mode == nj::runtime::input::Re4Mode::Gameplay && ctx.player && ctx.fade < 0.05f;
    const uint32_t ppl = c->mmu.read32(ga(0x803159F4u));   // cPlayer *pPL
    const bool leon = ppl >= 0x80000000u && ppl < 0x81800000u;

    // Apprentissage : le point d'entree d'une salle (premiere fois qu'on y est 3 s en jeu)
    if (room != T.last_room) {
        T.last_room = room;
        T.t_room = Clock::now();
        T.learned = false;
    }
    if (!T.learned && play && leon && room && since(T.t_room) > 3.0) {
        T.learned = true;
        if (!T.points.count(room)) {
            Point p{c->mmu.read_f32(ppl + 0x94), c->mmu.read_f32(ppl + 0x98), c->mmu.read_f32(ppl + 0x9C), c->mmu.read_f32(ppl + 0xA4)};
            T.points[room] = p;
            save_point(room, p);
            note("tour appris r%03x", room);
        }
    }

    // Vie pleine pendant la visite (le sol de test ne doit pas tuer Leon)
    if (T.st != St::Idle && since(T.t_life) > 1.0) {
        T.t_life = Clock::now();
        c->mmu.write16(g + pg(0x4FA4), c->mmu.read16(g + pg(0x4FA6)));
    }

    switch (T.st) {
    case St::Idle:
        if (file_exists(dir() + "/tour.go")) {
            std::remove((dir() + "/tour.go").c_str());
            std::remove((dir() + "/tour.done").c_str());
            if (parse_tour()) {
                T.idx = 0;
                T.ok = T.fail = 0;
                T.st = St::WaitReady;
                T.t_state = Clock::now();
                note("tour debut %u salles", (unsigned)T.stops.size());
            } else {
                note("tour : tour.txt absent ou vide");
            }
        }
        break;
    case St::WaitReady:
        if (!play || !leon) T.t_state = Clock::now();
        if (since(T.t_state) > 3.0) T.st = St::Jump;
        break;
    case St::Jump: {
        if (T.idx >= T.stops.size()) {
            note("tour termine ok=%u echec=%u", T.ok, T.fail);
            std::ofstream(dir() + "/tour.done") << "ok " << T.ok << " echec " << T.fail << "\n";
            T.st = St::Idle;
            break;
        }
        const Stop &s = T.stops[T.idx];
        const Point &p = T.points[s.room];
        // room_jmp.cpp / tools/re4/live_debug.py : next_room, next_point, next_pos + next_angle, puis x20 = 4
        c->mmu.write16(g + pg(0x4FA0), c->mmu.read16(g + pg(0x4F9C)));
        c->mmu.write16(g + 0x28, s.room);
        c->mmu.write8(g + 0x2A, 0);
        c->mmu.write_f32(g + 0x2C, p.x);
        c->mmu.write_f32(g + 0x30, p.y);
        c->mmu.write_f32(g + 0x34, p.z);
        c->mmu.write_f32(g + 0x38, p.ang);
        c->mmu.write32(g + 0x54, c->mmu.read32(g + 0x54) & ~0x40u);
        c->mmu.write8(g + 0x20, 4);
        c->mmu.write8(g + 0x21, 0);
        c->mmu.write8(g + 0x22, 0);
        c->mmu.write8(g + 0x23, 0);
        note("tour saut r%03x (%u/%u)", s.room, (unsigned)T.idx + 1, (unsigned)T.stops.size());
        T.st = St::Arrive;
        T.t_state = Clock::now();
        break;
    }
    case St::Arrive: {
        const Stop &s = T.stops[T.idx];
        if (room == s.room && play && leon && since(T.t_state) > 2.0) {
            note("tour arrivee r%03x", s.room);
            prof_start();
            T.shot_done = false;
            T.st = St::Measure;
            T.t_state = Clock::now();
        } else if (since(T.t_state) > 45.0) {
            note("tour echec r%03x (salle non atteinte)", s.room);
            ++T.fail;
            ++T.idx;
            T.st = St::Jump;
        }
        break;
    }
    case St::Measure: {
        const Stop &s = T.stops[T.idx];
        {   // camera du jeu : offsets de pG (G4BE08, include/global.h : Cam a +0x74, pos +0x118, at +0x124, fovy +0x134, up +0x138) ; flags_60 bit 28 = camera libre
            if (s.has_cam && since(T.t_cam) > 0.03) {
                T.t_cam = Clock::now();
                c->mmu.write32(g + 0x60, c->mmu.read32(g + 0x60) | 0x10000000u);
                for (int k = 0; k < 3; ++k) c->mmu.write_f32(g + 0x118 + 4 * k, s.cam[k]);
                for (int k = 0; k < 3; ++k) c->mmu.write_f32(g + 0x124 + 4 * k, s.cam[3 + k]);
                c->mmu.write_f32(g + 0x134, s.cam[6]);
                c->mmu.write_f32(g + 0x138, 0.f);
                c->mmu.write_f32(g + 0x13C, 1.f);
                c->mmu.write_f32(g + 0x140, 0.f);
            }
            if (!T.shot_done && since(T.t_state) >= 7.5) {   // pose de la camera juste avant la capture
                char b[200];
                std::snprintf(b, sizeof b, "tour cam r%03x pos=%.1f,%.1f,%.1f at=%.1f,%.1f,%.1f fov=%.2f", (unsigned)s.room, c->mmu.read_f32(g + 0x118),
                              c->mmu.read_f32(g + 0x11C), c->mmu.read_f32(g + 0x120), c->mmu.read_f32(g + 0x124), c->mmu.read_f32(g + 0x128), c->mmu.read_f32(g + 0x12C),
                              c->mmu.read_f32(g + 0x134));
                nj::perf::event(b);
            }
        }
        if (!T.shot_done && since(T.t_state) >= 8.0) {   // une capture reduite par salle, a 8 s de l'arrivee
            T.shot_done = true;
            char nm[40];
            std::snprintf(nm, sizeof nm, "/shots/r%03x.bmp", (unsigned)s.room);
            std::error_code ec;
            std::filesystem::create_directories(dir() + "/shots", ec);
            std::lock_guard<std::mutex> l(g_shot_mu);
            g_shot_path = dir() + nm;
        }
        if (since(T.t_state) >= s.secs || room != s.room) {
            prof_report(s.room);
            note("tour fin r%03x", s.room);
            ++T.ok;
            ++T.idx;
            T.st = St::Jump;
        }
        break;
    }
    }
}


// ---------------------------------------------------------------------------------------------------------------------------------
// Enregistreur / rejoueur d'entrees : un etat de manette par lecture du jeu (PADRead), donc aligne sur les images du jeu et non sur le temps
// de l'hote. Format : "NJPR1", puis des enregistrements de 12 octets (boutons u16, sticks x4 s8, gachettes x2 u8, err s8, 1 octet libre),
// puis 3 floats de position de Leon a la fin (a l'arret). Le rejeu compare la position de fin a celle de l'enregistrement : c'est la mesure
// du determinisme (un jeu qui rejoue pareil donne un ecart de l'ordre de 0).
namespace {
struct Rec {
    enum { Off, Recording, Playing } mode = Off;
    std::FILE *f = nullptr;
    std::vector<unsigned char> frames;   // rejeu
    size_t pos = 0;
    float start[3] = {}, end[3] = {};
    bool has_end = false;
    Clock::time_point t_check;
} R;

bool leon_pos(nj::runtime::GuestCpu &c, float out[3]) {
    using namespace nj::runtime;
    const uint32_t ppl = c.mmu.read32(ga(0x803159F4u));
    if (ppl < 0x80000000u || ppl >= 0x81800000u) return false;
    out[0] = c.mmu.read_f32(ppl + 0x94);
    out[1] = c.mmu.read_f32(ppl + 0x98);
    out[2] = c.mmu.read_f32(ppl + 0x9C);
    return true;
}

// Etat en direct pour le Studio du Nightjar Engine (21/10) : <donnees>/live_state.json deux fois par seconde (salle,
// Leon, ennemis vivants). Lecture seule. EmMgr (0x802DE700) : tableau +0, nombre +4, taille d'un ennemi +8 ; ennemi :
// bit 0 de +0 = vivant, id +0x100, type +0x101, position +0x94 (flottants), vie +0x320 (s16) (include/em.h, model.h).
void live_state(nj::runtime::GuestCpu &c, const std::string &d) {
    using namespace nj::runtime;
    auto ram = [](uint32_t a) { return a >= 0x80000000u && a < 0x81800000u; };
    const uint32_t g = c.mmu.read32(ga(0x80314BC8u));
    if (!ram(g)) return;
    std::string j = "{\"room\":" + std::to_string(c.mmu.read16(g + pg(0x4F9C)));
    float p[3];
    if (leon_pos(c, p)) {
        char b[160];
        std::snprintf(b, sizeof b, ",\"leon\":{\"pos\":[%.1f,%.1f,%.1f],\"hp\":%d}", p[0], p[1], p[2],
                      (int)(int16_t)c.mmu.read16(g + pg(0x4FA4)));
        j += b;
    }
    const uint32_t mgr = ga(0x802DE700u), arr = c.mmu.read32(mgr), n = c.mmu.read32(mgr + 4), sz = c.mmu.read32(mgr + 8);
    j += ",\"enemies\":[";
    bool first = true;
    if (ram(arr) && n < 1024 && sz >= 0x324 && sz < 0x4000) {
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t e = arr + i * sz;
            if (!(c.mmu.read32(e) & 1)) continue;
            char b[200];
            std::snprintf(b, sizeof b, "%s{\"slot\":%u,\"id\":%u,\"type\":%u,\"hp\":%d,\"pos\":[%.1f,%.1f,%.1f]}",
                          first ? "" : ",", i, c.mmu.read8(e + 0x100), c.mmu.read8(e + 0x101), (int)(int16_t)c.mmu.read16(e + 0x320),
                          c.mmu.read_f32(e + 0x94), c.mmu.read_f32(e + 0x98), c.mmu.read_f32(e + 0x9C));
            j += b;
            first = false;
        }
    }
    j += "]}";
    const std::string tmp = d + "/live_state.tmp";
    if (std::FILE *f = std::fopen(tmp.c_str(), "wb")) {
        std::fwrite(j.data(), 1, j.size(), f);
        std::fclose(f);
        std::remove((d + "/live_state.json").c_str());
        std::rename(tmp.c_str(), (d + "/live_state.json").c_str());
    }
}

void rec_note(const char *what, const float *p, float extra = 0) {
    char s[120];
    std::snprintf(s, sizeof s, "pad %s %.1f,%.1f,%.1f %.1f", what, p ? p[0] : 0.f, p ? p[1] : 0.f, p ? p[2] : 0.f, extra);
    nj::perf::event(s);
}
}  // namespace

void pad_hook(nj::runtime::GuestCpu &ctx, nj::runtime::input::PadState &st) {
    using nj::runtime::input::PadState;
    if (since(R.t_check) > 0.5) {   // les fichiers de commande : deux fois par seconde suffisent
        R.t_check = Clock::now();
        const std::string d = dir();
        live_state(ctx, d);   // Studio du Nightjar Engine
        if (R.mode == Rec::Off && file_exists(d + "/rec.go")) {
            std::remove((d + "/rec.go").c_str());
            R.f = std::fopen((d + "/pad_rec.bin").c_str(), "wb");
            if (R.f) {
                std::fwrite("NJPR1", 1, 5, R.f);
                R.mode = Rec::Recording;
                R.start[0] = R.start[1] = R.start[2] = 0;
                leon_pos(ctx, R.start);
                rec_note("enregistrement debut", R.start);
            }
        } else if (R.mode == Rec::Recording && file_exists(d + "/rec.stop")) {
            std::remove((d + "/rec.stop").c_str());
            float p[3] = {};
            leon_pos(ctx, p);
            std::fwrite(p, sizeof p, 1, R.f);
            std::fclose(R.f);
            R.f = nullptr;
            R.mode = Rec::Off;
            rec_note("enregistrement fin", p);
        } else if (R.mode == Rec::Off && file_exists(d + "/play.go")) {
            std::remove((d + "/play.go").c_str());
            std::remove((d + "/play.done").c_str());
            std::ifstream in(d + "/pad_rec.bin", std::ios::binary);
            std::vector<unsigned char> all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (all.size() > 5 + 12 && std::memcmp(all.data(), "NJPR1", 5) == 0) {
                std::memcpy(R.end, all.data() + all.size() - 12, 12);   // position de fin enregistree
                R.has_end = true;
                R.frames.assign(all.begin() + 5, all.end() - 12);
                R.pos = 0;
                R.mode = Rec::Playing;
                float p[3] = {};
                leon_pos(ctx, p);
                rec_note("rejeu debut", p, (float)(R.frames.size() / 12));
            }
        }
    }
    if (R.mode == Rec::Recording && R.f) {
        const unsigned char b[12] = {(unsigned char)(st.buttons >> 8), (unsigned char)st.buttons, (unsigned char)st.stick_x,
                                     (unsigned char)st.stick_y, (unsigned char)st.substick_x, (unsigned char)st.substick_y,
                                     st.trigger_l, st.trigger_r, (unsigned char)st.err, 0, 0, 0};
        std::fwrite(b, 1, 12, R.f);
    } else if (R.mode == Rec::Playing) {
        if (R.pos + 12 <= R.frames.size()) {
            const unsigned char *b = &R.frames[R.pos];
            st.buttons = (uint16_t)((b[0] << 8) | b[1]);
            st.stick_x = (int8_t)b[2];
            st.stick_y = (int8_t)b[3];
            st.substick_x = (int8_t)b[4];
            st.substick_y = (int8_t)b[5];
            st.trigger_l = b[6];
            st.trigger_r = b[7];
            st.analog_a = (st.buttons & 0x0100) ? 0xFF : 0;
            st.analog_b = (st.buttons & 0x0200) ? 0xFF : 0;
            st.err = (int8_t)b[8];
            R.pos += 12;
        } else {
            float p[3] = {};
            leon_pos(ctx, p);
            const float dx = p[0] - R.end[0], dy = p[1] - R.end[1], dz = p[2] - R.end[2];
            rec_note("rejeu fin", p, std::sqrt(dx * dx + dy * dy + dz * dz));   // dernier nombre = ecart avec la fin enregistree
            std::ofstream(dir() + "/play.done") << "ecart " << std::sqrt(dx * dx + dy * dy + dz * dz) << '\n';
            R.mode = Rec::Off;
        }
    }
}

}  // namespace nj::dev

#else

namespace nj::dev {
void tour_tick() {}
}  // namespace nj::dev

#endif
