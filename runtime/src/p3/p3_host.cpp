// P3, jalon 3 (docs/P3_TRADUCTEUR.md) : le moteur seul traduit le jeu du joueur. Aucun code du jeu
// dans l'executable : au lancement, njrecomp (bibliotheque, nj_lib.h) analyse main.dol et les .rel,
// le moteur construit ses tables (bornes, cases, modules REL), puis :
//  1. charge le cache : tranches fixes de kChunk fonctions consecutives, compilees en -O2, un fichier
//     objet par tranche ;
//  2. compile en -O0 (rapide) les fonctions de la liste chaude que le cache ne couvre pas (bloquant) ;
//  3. en arriere-plan : les fonctions que l'interpreteur rencontre (promotion, -O0, petits lots),
//     puis les tranches manquantes en -O2 (d'abord celles des fonctions chaudes), ecrites au cache et
//     branchees a chaud par-dessus le code -O0.
// Tant qu'une fonction n'est pas compilee, l'interpreteur (exact) l'execute.
//
// Developpement : NJ_P3_HOTE=<config njrecomp .toml> [NJ_P3_CHAUDES=<liste>]
//                 [NJ_P3_JIT_ARGS="-I... -D..."] [NJ_P3_FILS=n] [NJ_P3_CACHE=<dossier>]
//                 [NJ_P3_JOURNAL=1]
// (plus tard : profil de version embarque, disque lu par le lanceur, en-tetes et cache chiffres).
#if defined(__APPLE__)
#include <os/proc.h>
#include <pthread/qos.h>
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

#include "boot/program.h"
#include "nj_config.h"
#include "nj_image.h"
#include "nj_lib.h"

#include "runtime/config.h"
#include "runtime/cpu_context.h"
#include "runtime/env.h"
#include "runtime/dispatch.h"
#include "runtime/p3_restart.h"
#include "runtime/game_version.h"
#include "runtime/perf_journal.h"
#include "runtime/report.h"
#include "runtime/virtual_disc.h"
#include "runtime/rel_aot.h"
#include "p3_jit.h"
#include "p3_table.h"
#include "runtime/crash_report.h"
#include "runtime/obf.h"
#include "runtime/diag.h"
using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire

extern "C" void nj_interp(nj::runtime::GuestCpu& ctx);   // core/dispatch.cpp
namespace nj::runtime::p3 {
std::string profile_text(GameVersion v);   // p3_profils.cpp
}

// En-tetes precompiles embarques (tools/p3/gen_pch_blob.py, fabriques au build par
// NJ_P3_FABRIQUER_PCH) : le joueur n'a besoin d'aucun en-tete.
#if __has_include("pch_embarque.inc")
#include "pch_embarque.inc"
#define NJ_PCH_EMBARQUE 1
#endif

namespace nj::runtime::p3 {
namespace {

namespace fs = ::std::filesystem;
using clk = std::chrono::steady_clock;
const char NL[] = {10, 0};   // fin de ligne
double secs_since(clk::time_point t) { return std::chrono::duration<double>(clk::now() - t).count(); }

constexpr size_t kChunk = 64;          // fonctions par tranche du cache (fixe : le cache en depend)
const char kFast[] = "-O0", kFull[] = "-O2";   // compares par adresse : pas de _g

struct Host {
    nj::Translator tr;
    size_t n = 0;
    std::vector<FuncBound> bounds;
    std::unique_ptr<GameFn[]> slots;
    std::unique_ptr<bool[]> native;
    std::vector<const char*> names;
    std::vector<RelAotModule> rel;
    std::string head_pre, head_post;     // functions.h sans ses declarations g:: (par lot)
    std::string prelude;                 // en-tete commun a tous les lots (precompile)
    std::vector<std::string> args;       // options de Clang communes
    std::string pch_fast, pch_full;      // en-tetes precompiles par niveau ("" = pas encore, "-" = sans)
    std::string pch_mem_fast, pch_mem_full;   // contenu des PCH embarques (dechiffres en memoire)
    std::string cache_dir, tmp_dir;
    std::mutex emit_mu;                  // l'emission partage l'etat d'analyse : un lot a la fois
    std::mutex pch_mu;
    std::atomic<uint32_t> batch_no{0};
    std::atomic<size_t> compiled_fast{0}, compiled_full{0}, failed{0};
    bool journal = false;
};

std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {});
}

uint64_t fnv(uint64_t h, const void* p, size_t n) {
    const auto* b = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 0x100000001B3ull;
    return h;
}

#ifdef NJ_PCH_EMBARQUE
std::string decrypt_blob(const unsigned char* d, size_t n, unsigned long long seed) {
    std::string out(n, '\0');
    unsigned long long x = seed;
    const volatile unsigned char* src = d;
    for (size_t i = 0; i < n; ++i) {
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        out[i] = (char)(src[i] ^ (unsigned char)((x * 0x2545F4914F6CDD1DULL) >> 56));
    }
    return out;
}
#endif

// En-tete precompile du niveau `opt` : embarque (dechiffre en memoire, fichier virtuel), sinon
// fabrique a la premiere demande depuis les en-tetes (developpement).
const std::string& pch_for(Host& h, const char* opt) {
    std::lock_guard<std::mutex> lk(h.pch_mu);
    std::string& p = (opt == kFast) ? h.pch_fast : h.pch_full;
    if (!p.empty()) return p;
#ifdef NJ_PCH_EMBARQUE
    {
        std::string& mem = (opt == kFast) ? h.pch_mem_fast : h.pch_mem_full;
        if (opt == kFast) mem = decrypt_blob(nj_pch_O0, (size_t)(nj_pch_O0_fin - nj_pch_O0), kPchSeed_O0);
        else mem = decrypt_blob(nj_pch_O2, (size_t)(nj_pch_O2_fin - nj_pch_O2), kPchSeed_O2);
        p = std::string("C:/nj_p3_virtuel/prelude"_g) + opt + ".pch"_g;
        return p;
    }
#endif
    const std::string path = h.tmp_dir + "/prelude"_g + opt + ".pch"_g;
    std::string err;
    const auto t0 = clk::now();
    if (jit_make_pch({{"C:/nj_p3_virtuel/nj_prelude.h"_g, h.prelude}}, "C:/nj_p3_virtuel/nj_prelude.h"_g, h.args, opt,
                     path, err)) {
        p = path;
        NJ_DIAG("[P3] en-tete precompile %s en %.1f s%s", opt, secs_since(t0), NL);
    } else {
        p = "-"_g;   // echec : on compile sans
        NJ_DIAG("[P3] en-tete precompile %s : %s (on continue sans)%s", opt, err.c_str(), NL);
    }
    return p;
}

// Compile un lot de fonctions au niveau `opt` dans le fichier objet `obj`, le charge et le branche.
// keep = garder le fichier (cache) ; sinon il est efface apres chargement.
bool compile_unit(Host& h, const std::vector<uint32_t>& fns, const char* opt, const std::string& obj, bool keep) {
    if (fns.empty()) return true;
    const auto t0 = clk::now();
    std::string text;
    {
        std::lock_guard<std::mutex> lk(h.emit_mu);
        std::vector<nj::Translator::Resume> res;   // inutiles : repartition par les bornes
        text = h.tr.emit_batch(fns, res);
    }
    const uint32_t n = h.batch_no++;
    // Garde-fou (21/10) : le C++ genere ne contient jamais de caractere de controle. Un mauvais tirage du brouillage du
    // traducteur avait mis 12 octets chiffres a la place de l'indentation (Android, 381 lots refuses par Clang sans
    // explication) : on le dit clairement, une fois, au journal et dans le rapport.
    for (size_t k = 0; k < text.size(); ++k) {
        const unsigned char c = (unsigned char)text[k];
        if (c < 0x09 || (c > 0x0D && c < 0x20) || c == 0x7F) {
            static std::atomic<bool> told{false};
            if (!told.exchange(true)) {
                NJ_DIAG("[P3] texte genere corrompu (brouillage du traducteur) lot %u octet %llu%s", n, (unsigned long long)k, NL);
                nj::perf::event("P3 : texte genere corrompu (build a refaire)"_g);
            }
            return false;
        }
    }
    // fichiers virtuels : le lot, son functions.h (seulement ses propres fonctions : les appels
    // passent par les cases), l'en-tete commun ; dans un dossier qui n'existe pas sur le disque
    const std::string dir = "C:/nj_p3_virtuel/"_g;
    const std::string main = dir + "lot_"_g + std::to_string(n) + ".cpp"_g;
    const std::string& pch = pch_for(h, opt);
    const bool with_pch = pch != "-"_g;
    std::string head = with_pch ? std::string() : h.head_pre;   // preambule : deja dans le PCH
    head += "namespace g {"_g;
    head += NL;
    const std::string sig = "(GuestCpu& ctx) {"_g;
    for (size_t p = 0; (p = text.find(sig, p)) != std::string::npos; p += sig.size()) {
        const size_t ls = text.rfind(NL[0], p) + 1;
        if (text.compare(ls, 5, "void "_g) != 0) continue;
        head += "void "_g;
        head += text.substr(ls + 5, p - ls - 5);
        head += "(nj::runtime::GuestCpu& ctx);"_g;
        head += NL;
    }
    head += h.head_post;
    // le lot n'inclut que son functions.h : le reste (std, nj_ops.h, moteur) est dans l'en-tete
    // precompile ; ainsi le joueur n'a besoin d'aucun en-tete, seulement des deux PCH
    for (size_t p = 0; with_pch && (p = text.find("#include "_g, p)) != std::string::npos && p < 4096;) {
        const size_t e = text.find(NL[0], p);
        if (text.compare(p, 22, "#include \"functions.h\""_g) == 0) {
            p = e;
            continue;
        }
        text.erase(p, e - p);
    }
    const size_t kb = text.size() / 1024;
    VirtualFiles files = {{main, std::move(text)}, {dir + "functions.h"_g, std::move(head)},
                          {dir + "nj_prelude.h"_g, h.prelude}};
    {
        const std::string& mem = (opt == kFast) ? h.pch_mem_fast : h.pch_mem_full;
        if (!mem.empty()) files.push_back({pch, ""_g, &mem});   // PCH embarque, sans copie
    }
    std::vector<std::string> args;   // avec PCH, seulement les -D : les en-tetes viennent du PCH
    for (const std::string& a : h.args)
        if (!with_pch || a.compare(0, 2, "-D"_g) == 0) args.push_back(a);
    if (with_pch) {
        args.push_back("-include-pch"_g);
        args.push_back(pch);
        args.push_back("-Xclang"_g);
        args.push_back("-fno-validate-pch"_g);
    }
    const std::string part = obj + ".part"_g;
    std::string err;
    std::vector<Compiled> out;
    bool ok = jit_compile_object(files, main, args, opt, part, err);
    nj::crash::rearm();   // LLVM remplace le filtre d'exceptions du processus (12/10)
    bool compiled = false;   // objet valide sur le disque (le chargement peut encore echouer)
    if (ok) {
        std::error_code ec;
        fs::rename(part, obj, ec);
        if (ec) {
            err = "renommage : "_g + ec.message();
            ok = false;
        } else {
            compiled = true;
            ok = jit_load_object(obj, out, err);
        }
    }
    if (!ok) {
        NJ_DIAG("[P3] lot %u (%s) : %s%s", n, opt, err.c_str(), NL);
        for (size_t k = 0; k < err.size() && k < 400; k += 40)   // 21/10 : le journal coupe chaque texte a 40 caracteres
            NJ_DIAG("[P3] erreur %u %s%s", (unsigned)(k / 40), err.substr(k, 40).c_str(), NL);
        // build de diffusion sans journal : les premieres erreurs vont au rapport de plantage
        static std::atomic<int> reported{0};
        if (reported++ < 3)
            nj::crash::note(opt, err.substr(0, 1500).c_str());
        h.failed += fns.size();
        std::error_code ec;
        fs::remove(part, ec);
        // compile mais pas charge (symbole du moteur manquant, 12/10) : l'objet du cache est garde, une
        // mise a jour du moteur le recharge sans refaire la compilation (la cle ne depend que du code traduit)
        if (!(keep && compiled)) fs::remove(obj, ec);
        return false;
    }
    patch_slots(out);
    if (!keep) {
        std::error_code ec;
        fs::remove(obj, ec);
    }
    (opt == kFast ? h.compiled_fast : h.compiled_full) += fns.size();
    if (h.journal)
        NJ_DIAG("[P3] lot %u (%s) : %zu fonctions, %zu Ko de C++, %.2f s%s", n, opt, fns.size(), kb,
                     secs_since(t0), NL);
    return true;
}

// Fonctions (non natives) de la tranche c.
std::vector<uint32_t> chunk_fns(const Host& h, size_t c) {
    std::vector<uint32_t> v;
    const auto& F = h.tr.functions();
    for (size_t i = c * kChunk; i < std::min(h.n, (c + 1) * kChunk); ++i)
        if (!F[i].native) v.push_back((uint32_t)i);
    return v;
}
std::string chunk_path(const Host& h, size_t c) { return h.cache_dir + "/t"_g + std::to_string(c) + ".o"_g; }

// Etat : tranche chargee ou prise ; fonction couverte (tranche chargee) ou prise pour -O0.
std::unique_ptr<std::atomic<uint8_t>[]> g_chunk_taken;
std::unique_ptr<std::atomic<uint8_t>[]> g_fn_taken;
std::mutex g_q_mu;
std::condition_variable g_q_cv;
std::deque<uint32_t> g_queue;   // promotions demandees par l'interpreteur
std::FILE* g_seen = nullptr;    // NJ_P3_VUES : releve des fonctions promues (liste de demarrage)

void want(uint32_t fn) {
    uint8_t z = 0;
    if (!g_fn_taken || !g_fn_taken[fn].compare_exchange_strong(z, 1)) return;
    {
        std::lock_guard<std::mutex> lk(g_q_mu);
        g_queue.push_back(fn);
    }
    g_q_cv.notify_one();
}

// Fil de travail qui ne doit jamais prendre le temps du jeu : priorite la plus basse du systeme
// (06/10, video de l'utilisateur : la compilation de fond du premier lancement, 2 fils sur une tour de
// 4 coeurs, faisait tomber le jeu a 4 i/s).
// idle : seulement quand rien d'autre ne veut le processeur (cache -O2) ; sinon sous la normale
// (promotions : utiles a la cadence, elles passent apres le jeu mais sans attendre l'inactivite).
void lowest_priority(bool idle = true) {
#if defined(_WIN32)
    SetThreadPriority(GetCurrentThread(), idle ? THREAD_PRIORITY_IDLE : THREAD_PRIORITY_BELOW_NORMAL);
#elif defined(__APPLE__)
    // iOS : qualite de service du fil (pas de gettid sur Darwin) (14/10)
    pthread_set_qos_class_self_np(idle ? QOS_CLASS_BACKGROUND : QOS_CLASS_UTILITY, 0);
#else
    setpriority(PRIO_PROCESS, (id_t)gettid(), idle ? 19 : 5);
#endif
}

// Fil des promotions : jamais derriere une tranche -O2 ; lots -O0 de 32 au plus (le frontal coute
// ~1 s par lot quelle que soit sa taille).
void promoter(Host* h) {
    lowest_priority(false);
    for (;;) {
        std::vector<uint32_t> b;
        {
            std::unique_lock<std::mutex> lk(g_q_mu);
            g_q_cv.wait(lk, [] { return !g_queue.empty(); });
            while (!g_queue.empty() && b.size() < 32) {
                b.push_back(g_queue.front());
                g_queue.pop_front();
            }
        }
        if (g_seen) {
            const auto& F = h->tr.functions();
            for (uint32_t f : b) std::fprintf(g_seen, "%08X %08X 0 promue%s"_g, F[f].start, F[f].end, NL);
            std::fflush(g_seen);
        }
        compile_unit(*h, b, kFast, h->tmp_dir + "/p"_g + std::to_string(b[0]) + ".o"_g, false);
    }
}

// Module REL non compile (g_p3_need) : sa tranche en -O0, tout de suite, dans le fil du jeu.
Host* g_host = nullptr;
bool need(uint32_t fn) {
    static std::mutex mu;
    std::lock_guard<std::mutex> lk(mu);
    const DispatchTables& t = dispatch_tables();
    if (__atomic_load_n(&t.native[fn], __ATOMIC_ACQUIRE)) return true;
    const auto t0 = clk::now();
    std::vector<uint32_t> b;
    for (uint32_t f : chunk_fns(*g_host, fn / kChunk)) {
        uint8_t z = 0;
        if (g_fn_taken[f].compare_exchange_strong(z, 1) || f == fn) b.push_back(f);
    }
    compile_unit(*g_host, b, kFast, g_host->tmp_dir + "/n"_g + std::to_string(fn) + ".o"_g, false);
    const bool ok = __atomic_load_n(&t.native[fn], __ATOMIC_ACQUIRE);
    if (g_host->journal)
        NJ_DIAG("[P3] module REL : %zu fonctions compilees sur demande en %.2f s%s", b.size(),
                     secs_since(t0), NL);
    return ok;
}

// Fils de l'installation : les coeurs moins un (le systeme reste reactif), et pas plus que la
// memoire libre n'en tient, au moins un. Mesure du 16/10 (tour, 2 fils, 365 tranches -O2, Clang qui
// libere sa memoire entre les tranches : DisableFree) : pic du processus 540 Mo au total, soit
// ~250 Mo par fil au-dessus d'une base d'environ 350 Mo. On compte 400 Mo par fil + 500 Mo de
// reserve (avant : 1 Go par fil, mesure faussee par la fuite de Clang, +2,7 Go sur une preparation).
unsigned install_threads() {
    const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
    unsigned n = cores > 1 ? cores - 1 : 1;
    uint64_t free_bytes = 0;
#if defined(_WIN32)
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) free_bytes = ms.ullAvailPhys;
#else
    // 13/10 : MemAvailable (memoire reprenable, caches compris) et non _SC_AVPHYS_PAGES (MemFree) :
    // Android garde la memoire en cache, MemFree y reste bas et l'installation tombait a un seul fil
    // (MuMu : 1,4 Go libres, 2,7 Go disponibles, 1 fil sur 3 coeurs, ~90 min annoncees).
    if (FILE* f = std::fopen("/proc/meminfo"_g, "r"_g)) {
        char line[128];
        unsigned long long kb = 0;
        while (std::fgets(line, sizeof line, f))
            if (std::sscanf(line, "MemAvailable: %llu"_g, &kb) == 1) {
                free_bytes = (uint64_t)kb * 1024;
                break;
            }
        std::fclose(f);
    }
#if defined(__APPLE__)
    // iOS : memoire que l'application peut encore prendre avant d'etre tuee par le systeme
    if (!free_bytes) free_bytes = (uint64_t)os_proc_available_memory();
#else
    if (!free_bytes) {
        const long pages = sysconf(_SC_AVPHYS_PAGES), page = sysconf(_SC_PAGESIZE);
        if (pages > 0 && page > 0) free_bytes = (uint64_t)pages * (uint64_t)page;
    }
#endif
#endif
    if (free_bytes) {
        constexpr uint64_t kReserve = 500ull << 20, kPerThread = 400ull << 20;
        const uint64_t fit = free_bytes > kReserve ? (free_bytes - kReserve) / kPerThread : 0;
        n = std::min<unsigned>(n, (unsigned)std::max<uint64_t>(1, fit));
    }
    if (const char* s = NJ_ENV("NJ_P3_FILS_INSTALL"_g)) n = std::max(1, std::atoi(s));
    return std::max(1u, n);
}

void background(Host* h, std::vector<size_t> order, unsigned threads) {
    std::atomic<size_t> cursor{0};
    const auto t0 = clk::now();
    auto work = [&] {
        // sous la normale, pas au repos : au repos, le jeu (qui ne dort jamais) ne lui laissait
        // qu'environ une tranche par minute, ~5 h pour le cache complet (essai du 07/10)
        lowest_priority(false);
        for (;;) {
            // la tranche suivante en -O2, ecrite au cache
            size_t c = SIZE_MAX;
            while (c == SIZE_MAX) {
                const size_t k = cursor.fetch_add(1);
                if (k >= order.size()) break;
                uint8_t z = 0;
                if (g_chunk_taken[order[k]].compare_exchange_strong(z, 1)) c = order[k];
            }
            if (c == SIZE_MAX) return;
            compile_unit(*h, chunk_fns(*h, c), kFull, chunk_path(*h, c), true);
        }
    };
    std::vector<std::thread> ts;
    for (unsigned i = 0; i < threads; ++i) ts.emplace_back(work);
    for (auto& t : ts) t.join();
    NJ_DIAG("[P3] arriere-plan termine : %zu fonctions en -O2, %zu en -O0, %zu en echec, %.1f s ; "
                         "le cache est complet%s",
                 h->compiled_full.load(), h->compiled_fast.load(), h->failed.load(), secs_since(t0), NL);
}

// Version du moteur pour le cache : le code compile depend des en-tetes que voit Clang. Empreinte de
// tous les .h des dossiers -I (plus tard : du bloc d'en-tetes embarque).
uint64_t headers_tag(const std::vector<std::string>& args, uint64_t key) {
    std::vector<std::string> files;
    for (const std::string& a : args) {
        if (a.size() < 3 || a.compare(0, 2, "-I"_g) != 0) continue;
        std::error_code ec;
        // dossiers include : recursif ; les autres (racine du projet, src) : leurs seuls .h directs
        const bool deep = fs::path(a.substr(2)).filename() == "include"_g;
        for (auto it = fs::recursive_directory_iterator(a.substr(2), ec); !ec && it != fs::recursive_directory_iterator();
             it.increment(ec)) {
            if (it->is_directory()) {
                if (!deep) it.disable_recursion_pending();
                continue;
            }
            if (it->is_regular_file() && it->path().extension() == ".h"_g) files.push_back(it->path().generic_string());
        }
    }
    std::sort(files.begin(), files.end());
    for (const std::string& f : files) {
        const auto b = read_file(f);
        key = fnv(key, f.data(), f.size());
        key = fnv(key, b.data(), b.size());
    }
    return key;
}

}  // namespace
}  // namespace nj::runtime::p3

// Appele par run_game quand aucun code traduit n'a enregistre de tables (moteur seul).
extern "C" bool nj_p3_host_open() {
    using namespace nj::runtime;
    using namespace nj::runtime::p3;
    // Fabrication des en-tetes precompiles (build) : NJ_P3_FABRIQUER_PCH=<dossier>, avec
    // NJ_P3_JIT_ARGS (-I, --sysroot, -resource-dir de la machine de build) ; puis on s'arrete.
#ifndef NJ_RELEASE
    if (const char* outdir = NJ_ENV("NJ_P3_FABRIQUER_PCH"_g)) {
        std::vector<std::string> args;
        if (const char* a = NJ_ENV("NJ_P3_JIT_ARGS"_g)) {
            std::istringstream ex(a);
            for (std::string s; ex >> s;) args.push_back(s);
        }
        // PCH de diffusion : les en-tetes du moteur vus comme dans le build de diffusion
        const std::string prelude = std::string(NJ_ENV("NJ_P3_PCH_DIFFUSION"_g) ? "#define NJ_RELEASE 1"_g : ""_g) +
                                    NL + "#include \"p3/nj_prelude.h\""_g + NL;
        int bad = 0;
        for (const char* opt : {kFast, kFull}) {
            std::string err;
            const std::string out = std::string(outdir) + "/prelude"_g + opt + ".pch"_g;
            if (!jit_make_pch({{"C:/nj_p3_virtuel/nj_prelude.h"_g, prelude}}, "C:/nj_p3_virtuel/nj_prelude.h"_g, args,
                              opt, out, err)) {
                NJ_DIAG("[P3] %s : %s%s", out.c_str(), err.c_str(), NL);
                ++bad;
            } else {
                NJ_DIAG("[P3] %s fabrique%s", out.c_str(), NL);
            }
        }
        std::exit(bad ? 1 : 0);
    }
#endif
    const auto t0 = clk::now();
    g_p3_progress.phase = 1;
    auto& vd = DiscView::get();
    (void)vd.init(Config::get().game_dir);
    std::vector<uint8_t> dol = read_file(Config::get().game_dir + "/sys/main.dol"_g);
    // disque donne en image (Android, essais) : le DOL vient de l'image, comme au demarrage
    if (dol.empty() && !Config::get().disc_image.empty()) {
        nj::boot::Program prog;
        std::string id;
        if (nj::boot::load_program(Config::get().disc_image, prog, id)) dol = std::move(prog.file);
    }

    // profil de la version : embarque (reconnu par l'empreinte du DOL), ou configuration de
    // developpement (NJ_P3_HOTE)
    nj::Config cfg;
    if (const char* cfg_path = NJ_ENV("NJ_P3_HOTE"_g)) {
        if (!cfg.load(cfg_path)) {
            NJ_DIAG("[P3] configuration illisible : %s%s", cfg_path, NL);
            return false;
        }
        const auto s = read_file(cfg.symbols_csv);
        cfg.symbols_text.assign(s.begin(), s.end());
        if (dol.empty()) dol = read_file(cfg.input_game_dir + "/sys/main.dol"_g);
    } else {
        const GameVersion v = detect_game_version(dol.data(), dol.size());
        if (!cfg.load_text(profile_text(v))) {
            NJ_DIAG("[P3] version du jeu non prise en charge par ce build%s", NL);
            return false;
        }
        NJ_DIAG("[P3] profil %s%s", game_version_name(v), NL);
    }
    // empreinte du jeu traduit : DOL, REL, liste des fonctions, version du moteur, taille des tranches
    uint64_t key = 0xCBF29CE484222325ull;
    // le jeu du joueur : main.dol dans le dossier prepare par le lanceur, modules REL lus sur le
    // disque vu par le jeu (image .iso/.gcm/.rvz, disque 2 fusionne) ; la configuration de
    // developpement ne sert que de secours
    auto img = std::make_unique<nj::Image>();
    {
        key = fnv(key, dol.data(), dol.size());
        if (!img->load_dol_bytes(dol)) {
            NJ_DIAG("[P3] main.dol illisible (%zu octets)%s", dol.size(), NL);
            return false;
        }
    }
    size_t rel_from_files = 0;
    for (const auto& r : cfg.rel_aot) {
        std::string p{r.path};
        if (!(p.size() > 1 && (p[1] == ':' || p[0] == '/'))) p = cfg.input_game_dir + "/"_g + p;
        std::vector<std::uint8_t> bytes;
        // profil : chemin sur le disque (+ decalage dans une archive) ; configuration de
        // developpement : rel/<nom du fichier>, sinon le fichier extrait
        const std::string on_disc = r.size ? r.path : "rel/"_g + fs::path(p).filename().string();
        if (vd.read_file(on_disc, bytes)) {
            if (r.size) {
                if (uint64_t(r.offset) + r.size > bytes.size()) bytes.clear();
                else bytes = std::vector<uint8_t>(bytes.begin() + r.offset, bytes.begin() + r.offset + r.size);
            }
        } else {
            bytes = read_file(p);
            ++rel_from_files;
        }
        key = fnv(key, bytes.data(), bytes.size());
        if (bytes.empty()) {
            NJ_DIAG("[P3] module %s absent du disque (disque 2 monte : %s)%s",
                         fs::path(p).filename().string().c_str(), vd.has_second() ? "oui"_g : "non"_g, NL);
            return false;
        }
        if (!img->declare_rel_bytes(p, std::move(bytes), r.module_base, r.bss_base)) return false;
    }
    if (!img->load_rels()) {
        NJ_DIAG("[P3] modules REL inutilisables%s", NL);
        return false;
    }
    key = fnv(key, cfg.symbols_text.data(), cfg.symbols_text.size());
    // 14/10 : les sites gardes du fastmem changent le code traduit (profils USA / Europe refaits :
    // 186 000 -> 2 000 sites) ; sans eux dans la cle, l'ancien cache lent restait charge.
    key = fnv(key, cfg.fastmem_guard_text.data(), cfg.fastmem_guard_text.size());
    for (uint32_t a : cfg.guard_fns) key = fnv(key, &a, sizeof a);
    key = fnv(key, &kChunk, sizeof kChunk);
    // 19/10 : une enveloppe change le code traduit de sa fonction (appel au moteur) ; sans elle dans la
    // cle, un cache fait avant l'ajout gardait l'ancienne fonction (mod Krauser jamais appele).
    for (const auto& [a, name] : cfg.hle_wraps) {
        key = fnv(key, &a, sizeof a);
        key = fnv(key, name.data(), name.size());
    }
#if defined(__x86_64__) && defined(__ANDROID__)
    // 16/10 : x86_64 Android (emulateurs) partage le dossier de donnees d'un APK ARM : sans l'architecture dans la cle, le cache ARM
    // etait charge et execute (plantage). Les cles ARM restent inchangees.
    key = fnv(key, "android-x86_64", 14);
#endif

    auto* h = new Host;   // vit jusqu'a la fin du processus (le code charge y renvoie)
    h->journal = NJ_ENV("NJ_P3_JOURNAL"_g) != nullptr;
    if (!h->tr.open(cfg, std::move(img))) {
        NJ_DIAG("[P3] analyse du jeu impossible%s", NL);
        return false;
    }
    const auto& F = h->tr.functions();
    const size_t n = h->n = F.size();
    h->bounds.resize(n);
    h->slots.reset(new GameFn[n]);
    h->native.reset(new bool[n]);
    h->names.resize(n);
    size_t natives = 0;
    for (size_t i = 0; i < n; ++i) {
        h->bounds[i] = {F[i].start, F[i].end, nullptr, (uint32_t)i, F[i].native};
        h->names[i] = F[i].name.c_str();
        h->slots[i] = nj_interp;
        h->native[i] = false;
        if (F[i].native) {   // fonction remplacee par le moteur : son adresse vient de la table
            for (unsigned k = 0; k < kTableSize; ++k)
                if (name_hash(F[i].native_name.c_str()) == kTable[k].hash) {
                    h->slots[i] = reinterpret_cast<GameFn>(kTable[k].addr);
                    h->native[i] = true;
                    ++natives;
                }
        }
    }
    for (const auto& m : h->tr.modules()) h->rel.push_back({m.id, m.base, m.image_size, m.bss, m.bss_size});
    h->rel.push_back({0, 0, 0, 0, 0});
    rel_aot_register(h->rel.data(), (uint32_t)(h->rel.size() - 1));

    DispatchTables t;
    t.bounds = h->bounds.data();
    t.bound_count = n;
    t.slots = h->slots.get();
    t.native = h->native.get();
    t.slot_names = h->names.data();
    t.slot_count = n;
    t.rel_scan_lo = h->tr.dol_end();
    t.entry = h->tr.entry();
    dispatch_register(t);
    {
        const std::string full = h->tr.functions_header();
        const size_t a = full.find("namespace g {"_g), b = full.find("}  // namespace g"_g);
        h->head_pre = full.substr(0, a);
        h->head_post = full.substr(b);
        // en-tete commun (precompile) : p3/nj_prelude.h du moteur, identique pour toutes les versions
        h->prelude = "#include \"p3/nj_prelude.h\""_g + std::string(NL);
    }
    NJ_DIAG("[P3] jeu analyse : %zu fonctions (%zu natives), %zu modules REL (%zu hors du disque), %.1f s%s",
                 n, natives, h->rel.size() - 1, rel_from_files, secs_since(t0), NL);

    if (const char* a = NJ_ENV("NJ_P3_JIT_ARGS"_g)) {
        std::istringstream ex(a);
        for (std::string s; ex >> s;) h->args.push_back(s);
    } else {
#if defined(_WIN32)
        h->args.push_back("-D__USE_MINGW_SETJMP_NON_SEH"_g);   // comme le moteur (runtime/CMakeLists.txt)
#endif
        // NJ_RELEASE : defini dans l'en-tete precompile de diffusion (NJ_P3_PCH_DIFFUSION)
    }
    unsigned threads = std::max(1u, std::thread::hardware_concurrency() - 1);
    if (const char* s = NJ_ENV("NJ_P3_FILS"_g)) threads = std::max(1, std::atoi(s));

    // dossier du cache : <base>/<empreinte du jeu et des en-tetes du moteur>
    key = headers_tag(h->args, key);
    {   // le code compile avec FMA materiel ne se melange pas a l'ancien (20/10)
        const char tag = x86_fma_flags(h->args) ? 'F' : 'f';
        key = fnv(key, &tag, 1);
    }
#ifdef NJ_PCH_EMBARQUE
#ifdef NJ_PCH_SOURCE_HASH
    // en-tetes sources du code traduit (gen_pch_blob.py --sources) : stable d'un build a l'autre,
    // une mise a jour garde le cache tant qu'ils ne changent pas (12/10)
    {
        const uint64_t src = NJ_PCH_SOURCE_HASH;
        key = fnv(key, &src, sizeof src);
    }
#else
    key = fnv(key, nj_pch_O2, (size_t)(nj_pch_O2_fin - nj_pch_O2));   // en-tetes embarques
#endif
#endif
    {
        std::string base;
        if (const char* c = NJ_ENV("NJ_P3_CACHE"_g)) base = c;
        else if (const char* l = std::getenv("LOCALAPPDATA"_g)) base = std::string(l) + "/Nightjar/p3"_g;
        else base = "nj_p3_cache"_g;
        char hex[24];
        std::snprintf(hex, sizeof hex, "%016llx"_g, (unsigned long long)key);
        h->cache_dir = base + "/"_g + hex;
        h->tmp_dir = h->cache_dir + "/tmp"_g;
        std::error_code ec;
        fs::create_directories(h->tmp_dir, ec);
    }

    // 1. cache : tranches deja compilees
    const size_t chunks = (n + kChunk - 1) / kChunk;
    g_chunk_taken.reset(new std::atomic<uint8_t>[chunks]);
    g_fn_taken.reset(new std::atomic<uint8_t>[n]);
    for (size_t c = 0; c < chunks; ++c) g_chunk_taken[c] = 0;
    for (size_t i = 0; i < n; ++i) g_fn_taken[i] = F[i].native ? 1 : 0;
    {
        const auto tc = clk::now();
        std::vector<size_t> have;
        for (size_t c = 0; c < chunks; ++c)
            if (fs::exists(chunk_path(*h, c))) have.push_back(c);
        std::atomic<size_t> next{0}, loaded{0}, bad{0};
        g_p3_progress.total = (uint32_t)have.size();
        g_p3_progress.done = 0;
        g_p3_progress.phase = 2;
        auto work = [&] {
            for (size_t k; (k = next.fetch_add(1)) < have.size();) {
                const size_t c = have[k];
                std::vector<Compiled> out;
                std::string err;
                if (!jit_load_object(chunk_path(*h, c), out, err)) {
                    NJ_DIAG("[P3] cache t%zu : %s%s", c, err.c_str(), NL);
                    std::error_code ec;
                    fs::remove(chunk_path(*h, c), ec);
                    ++bad;
                    continue;
                }
                patch_slots(out);
                ++g_p3_progress.done;
                g_chunk_taken[c] = 1;
                for (uint32_t f : chunk_fns(*h, c)) g_fn_taken[f] = 1;
                ++loaded;
            }
        };
        std::vector<std::thread> ts;
        for (unsigned i = 0; i < threads; ++i) ts.emplace_back(work);
        for (auto& th : ts) th.join();
        NJ_DIAG("[P3] cache %s : %zu tranches sur %zu chargees (%zu illisibles), %.1f s%s",
                     h->cache_dir.c_str(), loaded.load(), chunks, bad.load(), secs_since(tc), NL);
    }

    // 2. liste chaude non couverte par le cache : -O0, avant le demarrage du jeu
    std::vector<uint32_t> hot;
    std::vector<char> hot_chunk(chunks, 0);
    {
        // liste du profil, ou NJ_P3_CHAUDES (developpement)
        std::vector<uint32_t> addrs = cfg.hot_list;
        if (const char* path = NJ_ENV("NJ_P3_CHAUDES"_g)) {
            addrs.clear();
            std::ifstream f(path);
            for (std::string line; std::getline(f, line);)
                if (!line.empty() && line[0] != '#') addrs.push_back((uint32_t)std::strtoul(line.c_str(), nullptr, 16));
        }
        std::sort(addrs.begin(), addrs.end());
        for (size_t i = 0; i < n; ++i)
            if (!F[i].native && std::binary_search(addrs.begin(), addrs.end(), F[i].start)) {
                hot_chunk[i / kChunk] = 1;
                uint8_t z = 0;
                if (g_fn_taken[i].compare_exchange_strong(z, 1)) hot.push_back((uint32_t)i);
            }
    }
    // 2 bis. installation (12/10) : au premier lancement, tout le jeu en -O2 avant de jouer, sur
    // tous les coeurs moins un et a la priorite normale (personne ne joue encore), plafonne par la
    // memoire libre. Avant : le jeu demarrait sur le code -O0 de la liste chaude et le -O2 venait
    // en arriere-plan sur un fil bride, soit ~30 min de jeu lent et de longs chargements.
    // NJ_P3_SANS_INSTALL (developpement) : l'ancien demarrage rapide.
    if (!NJ_ENV("NJ_P3_SANS_INSTALL"_g)) {
        std::vector<size_t> todo;
        for (size_t c = 0; c < chunks; ++c)
            if (!g_chunk_taken[c] && hot_chunk[c]) todo.push_back(c);
        for (size_t c = 0; c < chunks; ++c)
            if (!g_chunk_taken[c] && !hot_chunk[c]) todo.push_back(c);
        if (!todo.empty()) {
            const auto ti = clk::now();
            const unsigned it = install_threads();
            std::atomic<size_t> cur{0};
            g_p3_progress.total = (uint32_t)todo.size();
            g_p3_progress.done = 0;
            g_p3_progress.phase = 4;
            auto work = [&] {
                // sous la normale : tous les coeurs libres, mais la fenetre (barre de progression)
                // et le systeme passent devant (12/10 : fenetre figee pendant l'installation)
                lowest_priority(false);
                for (size_t k; (k = cur.fetch_add(1)) < todo.size();) {
                    const size_t c = todo[k];
                    uint8_t z = 0;
                    if (g_chunk_taken[c].compare_exchange_strong(z, 1))
                        compile_unit(*h, chunk_fns(*h, c), kFull, chunk_path(*h, c), true);
                    ++g_p3_progress.done;
                }
            };
            std::vector<std::thread> ts;
            for (unsigned i = 0; i < it; ++i) ts.emplace_back(work);
            for (auto& t : ts) t.join();
            NJ_DIAG("[P3] installation : %zu tranches en -O2 en %.1f s (%u fils), %zu en echec%s",
                         todo.size(), secs_since(ti), it, h->failed.load(), NL);
            char ev[128];
            std::snprintf(ev, sizeof ev, "install chunks=%zu secs=%.0f threads=%u failed=%zu"_g, todo.size(),
                          secs_since(ti), it, h->failed.load());
            nj::perf::event(ev);
            nj::report::add(std::string("Preparation: ") + ev);
            if (h->failed.load() == 0) nj::runtime::g_p3_restart_wanted = true;   // relance a la fin (p3_restart.h)
        }
        hot.clear();   // tout est en -O2 : plus de liste chaude en -O0
    }
    if (!hot.empty()) {
        const auto th = clk::now();
        std::atomic<size_t> next{0};
        g_p3_progress.total = (uint32_t)hot.size();
        g_p3_progress.done = 0;
        g_p3_progress.phase = 3;
        auto work = [&] {
            for (size_t a; (a = next.fetch_add(kChunk)) < hot.size();) {
                std::vector<uint32_t> b(hot.begin() + a, hot.begin() + std::min(hot.size(), a + kChunk));
                compile_unit(*h, b, kFast, h->tmp_dir + "/h"_g + std::to_string(a) + ".o"_g, false);
                g_p3_progress.done += (uint32_t)b.size();
            }
        };
        std::vector<std::thread> ts;
        for (unsigned i = 0; i < threads; ++i) ts.emplace_back(work);
        for (auto& th2 : ts) th2.join();
        NJ_DIAG("[P3] liste chaude : %zu fonctions compilees en -O0 en %.1f s (%u fils)%s", hot.size(),
                     secs_since(th), threads, NL);
    }

    // 3. arriere-plan : promotions, puis tranches manquantes (chaudes d'abord) en -O2
    std::vector<size_t> order;
    for (size_t c = 0; c < chunks; ++c)
        if (!g_chunk_taken[c] && hot_chunk[c]) order.push_back(c);
    for (size_t c = 0; c < chunks; ++c)
        if (!g_chunk_taken[c] && !hot_chunk[c]) order.push_back(c);
    g_host = h;
    g_p3_need = need;
    if (!order.empty()) {
        if (const char* v = NJ_ENV("NJ_P3_VUES"_g)) g_seen = std::fopen(v, "a"_g);
        g_p3_want = want;
        std::thread(promoter, h).detach();
        // fond : la moitie des coeurs au plus, laissant toujours au jeu son fil, celui du rendu et
        // celui de l'analyse du FIFO (4 coeurs : 1 fil de fond, 8 : 3, 16 : 7)
        const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
        const unsigned bg = std::max(1u, cores / 2 > 1 ? cores / 2 - 1 : 1);
        NJ_DIAG("[P3] arriere-plan : %zu tranches a compiler en -O2 sur %u fils%s", order.size(), bg, NL);
        std::thread(background, h, std::move(order), bg).detach();
    }
    g_p3_progress.phase = 0;
    NJ_DIAG("[P3] pret en %.1f s%s", secs_since(t0), NL);
    {
        char ev[64];
        std::snprintf(ev, sizeof ev, "ready secs=%.1f"_g, secs_since(t0));
        nj::perf::event(ev);
        nj::report::add(std::string("Preparation: ") + ev);
    }
    return true;
}
