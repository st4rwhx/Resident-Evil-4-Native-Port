// Rapport de plantage. Le build de diffusion n'ecrit aucun journal : si le moteur s'arrete sur une
// exception, un abort, une fin de std::terminate ou une erreur fatale du compilateur (P3), la cause
// tient en quelques lignes ajoutees a <LOCALAPPDATA>/Nightjar/plantage.txt, que le testeur nous envoie.
// Adresses relatives a leur module (module+decalage) : comparables d'une machine a l'autre pour le
// meme build (carte du lien gardee sur la tour, C:/re4n/symboles/<empreinte de l'exe>).
//
// 12/10 : un crash pendant l'installation P3 ne laissait aucune trace. LLVM remplace le filtre
// d'exceptions du processus : rearm() le remet (appele apres chaque compilation). Une exception
// fatale ecrit aussi plantage.dmp (piles de tous les fils, dbghelp charge a la demande : aucune
// importation de plus), depuis un fil a part (la pile du fil fautif peut etre epuisee). Une sortie
// par exit() est notee aussi : on distingue un arret propre d'un arret silencieux.
//
// 13/10 : plantage.dmp restait vide (la structure d'exception de dbghelp est alignee sur 4 octets,
// la notre ne l'etait pas) ; l'adresse etait toujours comptee depuis l'exe (une exception levee par
// RaiseException tombait « a exe+6452180DA ») ; une exception C++ sans gestionnaire ne disait pas
// laquelle. Maintenant : module+decalage, pile du fil fautif (16 niveaux), message de l'exception C++.
//
// 13/10 soir : un repere « session start » a chaque lancement (l'onglet Report du menu F10 lit la
// seance precedente entre deux reperes) ; fichier borne a 256 Ko. Android : plantage.txt dans le
// dossier de l'application, et les signaux de plantage natifs (SIGSEGV, SIGBUS, SIGILL, SIGFPE,
// SIGABRT) notes avec la pile en module+decalage (bibliotheque non depouillee gardee sur la tour).
#include "runtime/crash_report.h"

#include "common/platform.h"

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#if defined(__ANDROID__) || defined(__APPLE__)
#include <dlfcn.h>
#include <signal.h>
#if defined(__APPLE__)
#include <sys/ucontext.h>
#include "../launcher/host.h"
#else
#include <ucontext.h>
#endif
#include <unistd.h>
#include <unwind.h>
#endif

namespace nj::crash {
namespace {

char g_path[MAX_PATH_LEN] = {};
char g_dump[MAX_PATH_LEN] = {};
std::atomic<bool> g_quit_ok{false};

void write_line(const char *what, const char *detail) {
    if (!g_path[0])
        return;
    if (FILE *f = std::fopen(g_path, "a")) {
        const std::time_t now = std::time(nullptr);
        char when[32] = {};
        std::strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        std::fprintf(f, "%s %s%s%s\n", when, what, detail && *detail ? " : " : "", detail ? detail : "");
        std::fclose(f);
    }
}

#ifdef _WIN32
constexpr DWORD kGccThrow = 0x20474343;   // exception C++ de libgcc (RaiseException de __cxa_throw)

// « module+decalage » d'une adresse ; « ?+adresse » hors de tout module (code traduit, charge par le JIT).
void where(uintptr_t a, char *out, size_t n) {
    HMODULE m = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(a), &m) && m) {
        char path[MAX_PATH] = {};
        GetModuleFileNameA(m, path, sizeof path);
        const char *base = std::strrchr(path, '\\');
        base = base ? base + 1 : path;
        std::snprintf(out, n, "%s+%llX", base, (unsigned long long)(a - (uintptr_t)m));
    } else {
        std::snprintf(out, n, "?+%llX", (unsigned long long)a);
    }
}

// Pile du fil fautif depuis son contexte (tables de deroulement x64 des modules).
std::string stack_of(const CONTEXT *ctx) {
    std::string s;
#if defined(_M_X64) || defined(__x86_64__)
    CONTEXT c = *ctx;
    for (int i = 0; i < 16 && c.Rip; ++i) {
        char w[160];
        where((uintptr_t)c.Rip, w, sizeof w);
        if (!s.empty()) s += " < ";
        s += w;
        DWORD64 image = 0;
        PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(c.Rip, &image, nullptr);
        if (!fn) {   // fonction feuille (ou code sans table) : l'adresse de retour est au sommet de la pile
            c.Rip = *reinterpret_cast<DWORD64 *>(c.Rsp);
            c.Rsp += 8;
            continue;
        }
        void *handler_data = nullptr;
        DWORD64 frame = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, image, c.Rip, fn, &c, &handler_data, &frame, nullptr);
    }
#else
    (void)ctx;
#endif
    return s;
}

// MINIDUMP_EXCEPTION_INFORMATION de dbghelp.h, alignee sur 4 octets comme la vraie (pshpack4.h),
// sans l'en-tete ni la bibliotheque d'importation
#pragma pack(push, 4)
struct DumpExc {
    DWORD ThreadId;
    EXCEPTION_POINTERS *ExceptionPointers;
    BOOL ClientPointers;
};
#pragma pack(pop)
using DumpFn = BOOL(WINAPI *)(HANDLE, DWORD, HANDLE, int, const DumpExc *, const void *, const void *);

struct DumpJob {
    EXCEPTION_POINTERS *e;
    DWORD tid;
    DWORD error;
};

DWORD WINAPI dump_thread(void *p) {
    DumpJob *j = static_cast<DumpJob *>(p);
    HMODULE dbg = LoadLibraryW(L"dbghelp.dll");
    auto fn = dbg ? reinterpret_cast<DumpFn>(reinterpret_cast<void *>(GetProcAddress(dbg, "MiniDumpWriteDump")))
                  : nullptr;
    if (!fn) {
        j->error = GetLastError() ? GetLastError() : 1;
        return 1;
    }
    HANDLE f = CreateFileA(g_dump, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        j->error = GetLastError();
        return 1;
    }
    DumpExc x{j->tid, j->e, FALSE};
    // MiniDumpWithThreadInfo (0x1000) : piles et registres de chaque fil, sans la memoire du jeu
    if (!fn(GetCurrentProcess(), GetCurrentProcessId(), f, 0x1000, &x, nullptr, nullptr))
        j->error = GetLastError() ? GetLastError() : 2;
    CloseHandle(f);
    return 0;
}

LONG WINAPI on_exception(EXCEPTION_POINTERS *e) {
    const DWORD code = e->ExceptionRecord->ExceptionCode;
    // Exception C++ sans gestionnaire : on rend la main a __cxa_throw, qui appelle std::terminate
    // (on_terminate, avec le message de l'exception). La pile du lanceur est notee ici.
    if (code == kGccThrow) {
        write_line("c++ exception, thrown from", stack_of(e->ContextRecord).c_str());
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    static std::atomic<bool> once{false};
    if (once.exchange(true))
        return EXCEPTION_CONTINUE_SEARCH;
    char at[160];
    where((uintptr_t)e->ExceptionRecord->ExceptionAddress, at, sizeof at);
    char detail[256];
    std::snprintf(detail, sizeof detail, "code %08lX at %s (thread %lu)", code, at, GetCurrentThreadId());
    write_line("exception", detail);
    write_line("stack", stack_of(e->ContextRecord).c_str());
    if (g_dump[0]) {
        DumpJob j{e, GetCurrentThreadId(), 0};
        if (HANDLE t = CreateThread(nullptr, 0, dump_thread, &j, 0, nullptr)) {
            WaitForSingleObject(t, 30000);
            CloseHandle(t);
        }
        if (j.error) {
            char m[64];
            std::snprintf(m, sizeof m, "error %lu", j.error);
            write_line("dump failed", m);
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

#if defined(__ANDROID__) || defined(__APPLE__)
// « bibliotheque+decalage » d'une adresse (libmain.so+... : bibliotheque non depouillee sur la tour)
void where_so(uintptr_t a, char *out, size_t n) {
    Dl_info i{};
    if (a && dladdr(reinterpret_cast<void *>(a), &i) && i.dli_fbase) {
        const char *base = i.dli_fname ? std::strrchr(i.dli_fname, '/') : nullptr;
        base = base ? base + 1 : (i.dli_fname ? i.dli_fname : "?");
        std::snprintf(out, n, "%s+%llX", base, (unsigned long long)(a - reinterpret_cast<uintptr_t>(i.dli_fbase)));
    } else {
        std::snprintf(out, n, "?+%llX", (unsigned long long)a);
    }
}

struct Trace {
    uintptr_t pc[20];
    int n;
};
_Unwind_Reason_Code trace_step(_Unwind_Context *c, void *arg) {
    Trace *t = static_cast<Trace *>(arg);
    const uintptr_t pc = _Unwind_GetIP(c);
    if (pc && t->n < 20) t->pc[t->n++] = pc;
    return t->n < 20 ? _URC_NO_REASON : _URC_END_OF_STACK;
}

const int kSignals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE};
struct sigaction g_old[4];

// Note le plantage, puis rend la main au gestionnaire d'avant (Android, ou le filet du fastmem) :
// l'instruction refaute et le plantage suit son cours normal (trace du systeme comprise).
void on_signal(int sig, siginfo_t *si, void *ucv) {
    static std::atomic<bool> once{false};
    if (!once.exchange(true)) {
        uintptr_t pc = 0;
        const ucontext_t *uc = static_cast<const ucontext_t *>(ucv);
#if defined(__APPLE__) && defined(__aarch64__)
        pc = (uintptr_t)__darwin_arm_thread_state64_get_pc(uc->uc_mcontext->__ss);
#elif defined(__aarch64__)
        pc = uc->uc_mcontext.pc;
#elif defined(__arm__)
        pc = uc->uc_mcontext.arm_pc;
#elif defined(__x86_64__)
        pc = uc->uc_mcontext.gregs[REG_RIP];
#endif
        char at[160], detail[300];
        where_so(pc, at, sizeof at);
        std::snprintf(detail, sizeof detail, "signal %d (code %d) at %s, address %p", sig, si ? si->si_code : 0, at,
                      si ? si->si_addr : nullptr);
        write_line("crash", detail);
        Trace t{{}, 0};
        _Unwind_Backtrace(trace_step, &t);
        std::string s;
        for (int i = 0; i < t.n; ++i) {
            char w[160];
            where_so(t.pc[i], w, sizeof w);
            if (!s.empty()) s += " < ";
            s += w;
        }
        write_line("stack", s.c_str());
    }
    for (int i = 0; i < 4; ++i)
        if (kSignals[i] == sig) sigaction(sig, &g_old[i], nullptr);
}

void install_signals() {
    static char alt[64 * 1024];
    stack_t ss{};
    ss.ss_sp = alt;
    ss.ss_size = sizeof alt;
    sigaltstack(&ss, nullptr);
    struct sigaction sa{};
    sa.sa_sigaction = on_signal;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    for (int i = 0; i < 4; ++i) sigaction(kSignals[i], &sa, &g_old[i]);
}
#endif

// Fichier borne : au-dela de 256 Ko, on garde la fin.
void trim_file() {
    if (!g_path[0]) return;
    FILE *f = std::fopen(g_path, "rb");
    if (!f) return;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    if (n <= 256 * 1024) {
        std::fclose(f);
        return;
    }
    std::string keep(size_t(128 * 1024), '\0');
    std::fseek(f, n - long(keep.size()), SEEK_SET);
    const size_t got = std::fread(&keep[0], 1, keep.size(), f);
    std::fclose(f);
    keep.resize(got);
    const size_t nl = keep.find('\n');
    if (FILE *w = std::fopen(g_path, "wb")) {
        std::fwrite(keep.data() + (nl == std::string::npos ? 0 : nl + 1), 1,
                    keep.size() - (nl == std::string::npos ? 0 : nl + 1), w);
        std::fclose(w);
    }
}

void on_abort(int) {
    write_line("abort", nullptr);
#if defined(__ANDROID__) || defined(__APPLE__)
    // pile de l'appelant d'abort() (19/11 : iPhone, abort muet du coeur natif juste apres etc/memcard.das)
    Trace t{{}, 0};
    _Unwind_Backtrace(trace_step, &t);
    std::string s;
    for (int i = 0; i < t.n; ++i) {
        char w[160];
        where_so(t.pc[i], w, sizeof w);
        if (!s.empty()) s += " < ";
        s += w;
    }
    write_line("stack", s.c_str());
#endif
    std::_Exit(3);
}

void on_terminate() {
    // exception en cours (lancee sans gestionnaire) : son message
    if (std::exception_ptr p = std::current_exception()) {
        try {
            std::rethrow_exception(p);
        } catch (const std::exception &x) {
            write_line("terminate, c++ exception", x.what());
        } catch (...) {
            write_line("terminate, c++ exception", "(not a std::exception)");
        }
    } else {
        write_line("terminate", nullptr);
    }
    std::_Exit(3);
}

void on_exit() {
    if (!g_quit_ok.load())
        write_line("exit (not a player quit)", nullptr);
}

}  // namespace

void install() {
#ifdef _WIN32
    char dir[MAX_PATH_LEN];
    const DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", dir, sizeof dir);
    if (n && n < sizeof dir) {
        std::snprintf(g_path, sizeof g_path, "%s/Nightjar", dir);
        CreateDirectoryA(g_path, nullptr);
        std::snprintf(g_path, sizeof g_path, "%s/Nightjar/plantage.txt", dir);
        std::snprintf(g_dump, sizeof g_dump, "%s/Nightjar/plantage.dmp", dir);
    }
    SetUnhandledExceptionFilter(on_exception);
#elif defined(__ANDROID__) || defined(__APPLE__)
#if defined(__APPLE__)
    // iPhone : <dossier de donnees>/plantage.txt, a cote du journal.txt (lisible par USB / Fichiers)
    const std::string p = nj::launcher::host::data_dir() + "plantage.txt";
#else
    const std::string p = nj::writable_path("plantage.txt");
#endif
    if (p.size() < sizeof g_path) std::snprintf(g_path, sizeof g_path, "%s", p.c_str());
    install_signals();
#endif
    trim_file();
    write_line("session start", nullptr);
    std::signal(SIGABRT, on_abort);
    std::set_terminate(on_terminate);
    std::atexit(on_exit);
}

void rearm() {
#ifdef _WIN32
    SetUnhandledExceptionFilter(on_exception);
#endif
}

void quit_requested() { g_quit_ok.store(true); }

void note(const char *what, const char *detail) { write_line(what, detail); }

}  // namespace nj::crash
