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

// Le decalage de chargement du module qui contient l'adresse donnee, lu dans /proc/self/maps.
// Prendre la premiere plage executable venue ne marche pas : l'editeur de liens dynamique et la
// libc en ont aussi, et on obtient des offsets de plusieurs megaoctets pour un binaire qui en
// fait trois.
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
    std::printf("   #%-2d  module + 0x%lx\n", g_depth, ip - g_load_bias);
    return (++g_depth >= 32) ? _URC_END_OF_STACK : _URC_NO_REASON;
}

void on_fault(int sig, siginfo_t *info, void *ctx)
{
    const ucontext_t *uc = (const ucontext_t *) ctx;
    const unsigned long pc = (unsigned long) uc->uc_mcontext.arm_pc;
    const unsigned long fault = (unsigned long) uc->uc_mcontext.fault_address;
    compute_load_bias(pc);

    std::printf("\n[PORT] signal %d, adresse fautive %p\n", sig, info->si_addr);
    // Si ces deux valeurs different, c'est la lecture du contexte qui est fausse, pas le jeu.
    std::printf("[PORT] pc = module + 0x%lx   (fault_address du contexte : 0x%lx)\n",
                pc - g_load_bias, fault);
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
