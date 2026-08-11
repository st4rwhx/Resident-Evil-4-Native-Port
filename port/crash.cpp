// -*- coding: utf-8 -*-
// Un gestionnaire de faute qui dit *ou* on est tombe.
//
// Deux approches ont ete essayees avant celle-ci, et les deux donnaient des adresses fausses :
//  * le tombstone d'Android n'affiche qu'une seule image pour un executable statique ;
//  * les registres du contexte de signal (`uc_mcontext.arm_pc`) pointaient chaque fois sur une
//    instruction qui **ne peut pas** provoquer de faute d'acces - un `b`, un `mov` immediat.
//    Une valeur qui contredit le jeu d'instructions n'est pas une piste, c'est un piege : on la
//    verifie contre `fault_address` et on ne s'y fie pas seule.
//
// Ce qui marche : `_Unwind_Backtrace`, la meme mecanique que celle des exceptions C++, qui
// s'appuie sur les tables `.ARM.exidx` que le portage genere (`-funwind-tables`). Elle rend la
// vraie chaine d'appels.
//
// Les adresses sont imprimees relativement a la base du module, prêtes pour
// `llvm-symbolizer --obj=<binaire> <adresse>`.
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unwind.h>
#include <ucontext.h>
#include <unistd.h>

namespace {

unsigned long g_load_bias = 0;
int g_depth = 0;
char g_module[256] = {0};

// Le decalage de chargement du module qui contient l'adresse donnee, lu dans /proc/self/maps.
// Prendre la premiere plage executable venue ne marche pas : l'editeur de liens dynamique et la
// libc en ont aussi, et on obtient des offsets de plusieurs megaoctets pour un binaire qui en
// fait trois.
// Rend le decalage **et** le nom du fichier mappe. Sans le nom, une adresse tombee dans la
// libc ou dans le pilote GLES se lit comme une adresse du jeu, et l on symbolise dans le
// mauvais binaire - c est exactement ce qui a fait pointer une faute d acces sur un `mov`
// immediat, instruction incapable d en provoquer une.
void compute_load_bias(unsigned long pc)
{
    std::FILE *f = std::fopen("/proc/self/maps", "r");
    if (!f)
        return;
    char line[512];
    while (std::fgets(line, sizeof(line), f)) {
        unsigned long lo = 0, hi = 0, off = 0;
        char perms[8] = {0};
        if (std::sscanf(line, "%lx-%lx %7s %lx", &lo, &hi, perms, &off) != 4)
            continue;
        if (pc >= lo && pc < hi) {
            g_load_bias = lo - off;
            const char *slash = std::strrchr(line, '/');
            if (slash) {
                std::snprintf(g_module, sizeof(g_module), "%s", slash + 1);
                char *nl = std::strchr(g_module, '\n');
                if (nl)
                    *nl = 0;
            } else {
                std::snprintf(g_module, sizeof(g_module), "%s", "?");
            }
            break;
        }
    }
    std::fclose(f);
}

_Unwind_Reason_Code on_frame(struct _Unwind_Context *ctx, void *)
{
    const unsigned long ip = (unsigned long) _Unwind_GetIP(ctx);
    if (!ip)
        return _URC_END_OF_STACK;
    if (!g_load_bias)
        compute_load_bias(ip);
    // Chaque image est resituee dans **son** fichier : une pile qui traverse le pilote GLES
    // melange trois modules, et un offset sans son nom ne veut rien dire.
    unsigned long bias = 0;
    char mod[256] = {0};
    {
        const unsigned long save_bias = g_load_bias;
        char save_mod[256];
        std::memcpy(save_mod, g_module, sizeof(save_mod));
        g_load_bias = 0;
        g_module[0] = 0;
        compute_load_bias(ip);
        bias = g_load_bias;
        std::memcpy(mod, g_module, sizeof(mod));
        g_load_bias = save_bias;
        std::memcpy(g_module, save_mod, sizeof(g_module));
    }
    std::printf("   #%-2d  %s + 0x%lx\n", g_depth, mod[0] ? mod : "?", ip - bias);
    return (++g_depth >= 32) ? _URC_END_OF_STACK : _URC_NO_REASON;
}

void on_fault(int sig, siginfo_t *info, void *ctx)
{
    // Les messages du jeu (printf de cLog, erreurs d'allocation...) sont dans le tampon de stdout :
    // sans ce vidage ils disparaissent avec le processus, et ce sont eux qui disent la cause.
    std::fflush(stdout);
    const ucontext_t *uc = (const ucontext_t *) ctx;
    const unsigned long pc = (unsigned long) uc->uc_mcontext.arm_pc;
    const unsigned long fault = (unsigned long) uc->uc_mcontext.fault_address;
    compute_load_bias(pc);

    std::printf("\n[PORT] signal %d, adresse fautive %p\n", sig, info->si_addr);
    // Si ces deux valeurs different, c'est la lecture du contexte qui est fausse, pas le jeu.
    std::printf("[PORT] pc = %s + 0x%lx   (fault_address du contexte : 0x%lx)\n",
                g_module[0] ? g_module : "?", pc - g_load_bias, fault);
    // `lr` nomme l appelant : quand la faute vient d un saut sur un pointeur nul, c est la
    // seule valeur qui reste exploitable.
    {
        const unsigned long lr = (unsigned long) uc->uc_mcontext.arm_lr;
        const unsigned long save = g_load_bias;
        char save_mod[256];
        std::memcpy(save_mod, g_module, sizeof(save_mod));
        g_load_bias = 0;
        g_module[0] = 0;
        compute_load_bias(lr);
        std::printf("[PORT] lr = %s + 0x%lx\n", g_module[0] ? g_module : "?", lr - g_load_bias);
        g_load_bias = save;
        std::memcpy(g_module, save_mod, sizeof(g_module));
    }
    std::printf("[PORT] pile :\n");
    g_depth = 0;
    _Unwind_Backtrace(on_frame, nullptr);
    std::fflush(stdout);
    std::_Exit(1);
}

} // namespace

extern "C" void port_install_crash_handler(void)
{
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_fault;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
}
