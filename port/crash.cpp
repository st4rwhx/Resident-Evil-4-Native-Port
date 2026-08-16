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

#include <dlfcn.h>
#include <unwind.h>
#include <ucontext.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <sys/mman.h>

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
    // dladdr rend la base de chargement du module (adresse virtuelle 0). Le calcul par /proc/self/maps
    // ci-dessous se trompe de 0x1000 quand le segment de code est decale dans le fichier (c'est le
    // cas de notre ELF) : toutes les adresses imprimees etaient alors fausses, et les piles
    // symbolisees designaient une autre fonction.
    Dl_info di;
    if (dladdr((void *) pc, &di) && di.dli_fbase) {
        g_load_bias = (unsigned long) di.dli_fbase;
        const char *base = di.dli_fname ? std::strrchr(di.dli_fname, '/') : nullptr;
        std::snprintf(g_module, sizeof(g_module), "%s", base ? base + 1 : (di.dli_fname ? di.dli_fname : "?"));
        return;
    }
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

// Registres du contexte de signal : ARM32 (telephone) ou x86 32 bits (banc G0 dans MuMu, 14/11 ; pas de registre de
// lien sur x86 : l'appelant est le mot en sommet de pile).
#if defined(__i386__)
#define CTX_PC(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_EIP])
#define CTX_LR(uc) (*(const unsigned long *) (uc)->uc_mcontext.gregs[REG_ESP])
#define CTX_FAULT(uc, info) ((unsigned long) (info)->si_addr)
#else
#define CTX_PC(uc) ((unsigned long) (uc)->uc_mcontext.arm_pc)
#define CTX_LR(uc) ((unsigned long) (uc)->uc_mcontext.arm_lr)
#define CTX_FAULT(uc, info) ((unsigned long) (uc)->uc_mcontext.fault_address)
#endif

// Garde en ecriture du Studio (commande « garder <adr> », 16/11) : la page est mise en lecture seule ; la premiere
// ecriture leve SIGSEGV ici : on journalise l'auteur (fil, pc, pile), on rend la page inscriptible et l'ecriture se
// refait au retour du signal. Sert a trouver qui ecrase une donnee quand ce n'est pas une tache du jeu.
volatile unsigned long g_guard = 0;
volatile unsigned long g_guard_hit = 0;   // dernier pc fautif (lu par le Studio)

void on_fault(int sig, siginfo_t *info, void *ctx)
{
    {
        const unsigned long fa = (unsigned long) info->si_addr;
        const unsigned long gp = g_guard;
        if (sig == SIGSEGV && gp && fa >= gp && fa < gp + 4096) {
            const ucontext_t *gu = (const ucontext_t *) ctx;
            const unsigned long gpc = CTX_PC(gu);
            g_load_bias = 0;
            g_module[0] = 0;
            compute_load_bias(gpc);
            std::printf("[GARDE] ecriture a %lx par le fil %d : pc = %s + 0x%lx ; pile :\n", fa, (int) gettid(),
                        g_module[0] ? g_module : "?", gpc - g_load_bias);
            g_depth = 0;
            _Unwind_Backtrace(on_frame, nullptr);
            std::fflush(stdout);
            mprotect((void *) gp, 4096, PROT_READ | PROT_WRITE);
            g_guard_hit = gpc - g_load_bias;
            g_guard = 0;
            return;
        }
    }
    // Les messages du jeu (printf de cLog, erreurs d'allocation...) sont dans le tampon de stdout :
    // sans ce vidage ils disparaissent avec le processus, et ce sont eux qui disent la cause.
    std::fflush(stdout);
    const ucontext_t *uc = (const ucontext_t *) ctx;
    const unsigned long pc = CTX_PC(uc);
    const unsigned long fault = CTX_FAULT(uc, info);
    compute_load_bias(pc);

    std::printf("\n[PORT] signal %d, adresse fautive %p\n", sig, info->si_addr);
    // Si ces deux valeurs different, c'est la lecture du contexte qui est fausse, pas le jeu.
    std::printf("[PORT] pc = %s + 0x%lx   (fault_address du contexte : 0x%lx)\n",
                g_module[0] ? g_module : "?", pc - g_load_bias, fault);
    // `lr` nomme l appelant : quand la faute vient d un saut sur un pointeur nul, c est la
    // seule valeur qui reste exploitable.
    {
        const unsigned long lr = CTX_LR(uc);
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
#if defined(__i386__)
    {
        const greg_t *g = uc->uc_mcontext.gregs;
        std::printf("[PORT] eax ebx ecx edx esi edi ebp esp : %lx %lx %lx %lx %lx %lx %lx %lx\n", (unsigned long) g[REG_EAX],
                    (unsigned long) g[REG_EBX], (unsigned long) g[REG_ECX], (unsigned long) g[REG_EDX], (unsigned long) g[REG_ESI],
                    (unsigned long) g[REG_EDI], (unsigned long) g[REG_EBP], (unsigned long) g[REG_ESP]);
    }
#else
    std::printf("[PORT] r0-r12 : %lx %lx %lx %lx %lx %lx %lx %lx %lx %lx %lx %lx %lx  sp=%lx\n",
                (unsigned long) uc->uc_mcontext.arm_r0, (unsigned long) uc->uc_mcontext.arm_r1,
                (unsigned long) uc->uc_mcontext.arm_r2, (unsigned long) uc->uc_mcontext.arm_r3,
                (unsigned long) uc->uc_mcontext.arm_r4, (unsigned long) uc->uc_mcontext.arm_r5,
                (unsigned long) uc->uc_mcontext.arm_r6, (unsigned long) uc->uc_mcontext.arm_r7,
                (unsigned long) uc->uc_mcontext.arm_r8, (unsigned long) uc->uc_mcontext.arm_r9,
                (unsigned long) uc->uc_mcontext.arm_r10, (unsigned long) uc->uc_mcontext.arm_fp,
                (unsigned long) uc->uc_mcontext.arm_ip, (unsigned long) uc->uc_mcontext.arm_sp);
#endif
    std::printf("[PORT] pile :\n");
    g_depth = 0;
    _Unwind_Backtrace(on_frame, nullptr);
    std::fflush(stdout);
    std::_Exit(1);
}

} // namespace

extern "C" unsigned int port_blk_checks;   // src/game/block.cpp : une fois par image
namespace {
// Sentinelle de blocage : si l'image du jeu n'avance plus (compteur port_blk_checks, une fois par image dans la boucle
// principale) pendant 6 s, chaque fil du processus recoit SIGUSR2 et imprime sa pile, une seule fois par lancement.
// Sert a trouver les attentes sans fin (boucles qui comptaient sur une interruption de la console).
volatile int g_dump_busy = 0;

void on_dump(int, siginfo_t *, void *ctx)
{
    while (__sync_lock_test_and_set(&g_dump_busy, 1))
        ;
    ucontext_t *uc = (ucontext_t *) ctx;
    const unsigned long pc = CTX_PC(uc), lr = CTX_LR(uc);
    g_load_bias = 0;
    compute_load_bias(pc);
    std::printf("[BLOQUE] fil %d : pc = %s + 0x%lx", (int) gettid(), g_module[0] ? g_module : "?", pc - g_load_bias);
    g_load_bias = 0;
    compute_load_bias(lr);
    std::printf("  lr = %s + 0x%lx\n", g_module[0] ? g_module : "?", lr - g_load_bias);
    g_depth = 0;
    g_load_bias = 0;
    _Unwind_Backtrace(on_frame, nullptr);
    std::fflush(stdout);
    __sync_lock_release(&g_dump_busy);
}

void *watchdog(void *)
{
    unsigned int last = 0, still = 0;
    bool done = false;
    for (;;) {
        sleep(1);
        const unsigned int now = port_blk_checks;
        if (now == 0 || now != last) {
            last = now;
            still = 0;
            continue;
        }
        if (++still < 6 || done)
            continue;
        done = true;
        std::printf("[BLOQUE] l'image du jeu n'avance plus depuis 6 s (image %u) : piles de tous les fils\n", now);
        std::fflush(stdout);
        DIR *d = opendir("/proc/self/task");
        if (!d)
            continue;
        const int self = (int) gettid();
        while (dirent *e = readdir(d)) {
            const int tid = std::atoi(e->d_name);
            if (tid <= 0 || tid == self)
                continue;
            syscall(SYS_tgkill, getpid(), tid, SIGUSR2);
            usleep(150000);
        }
        closedir(d);
    }
}

} // namespace (sentinelle)

#if defined(__i386__)
namespace {
// Division entiere par zero (banc x86, 15/11) : sur la console et sur ARM elle ne plante pas (ARM : quotient 0) ; le jeu
// d'origine en contient (shadow.cpp SoftShadowGXDraw : size / div avec div = 0). Sur x86 le processeur leve SIGFPE.
// On reproduit le comportement ARM du telephone : quotient 0, reste = dividende (a % 0 = a - 0 * 0), puis on reprend a
// l'instruction suivante. Instructions traitees : F6/F7 avec /6 (div) ou /7 (idiv), prefixes 66 et de segment.
void on_intdiv(int sig, siginfo_t *info, void *ctx)
{
    ucontext_t *uc = (ucontext_t *) ctx;
    greg_t *g = uc->uc_mcontext.gregs;
    const unsigned char *ip = (const unsigned char *) g[REG_EIP];
    const unsigned char *p = ip;
    bool op16 = false;
    while (*p == 0x66 || *p == 0x2E || *p == 0x3E || *p == 0x26 || *p == 0x36 || *p == 0x64 || *p == 0x65) {
        if (*p == 0x66) op16 = true;
        ++p;
    }
    const unsigned char op = *p++;
    const unsigned char modrm = *p++;
    const int mod = modrm >> 6, reg = (modrm >> 3) & 7, rm = modrm & 7;
    if (info->si_code != FPE_INTDIV || (op != 0xF6 && op != 0xF7) || (reg != 6 && reg != 7)) {
        on_fault(sig, info, ctx);   // autre chose : rapport de plantage habituel
        return;
    }
    if (mod != 3 && rm == 4) {   // octet SIB
        const unsigned char sib = *p++;
        if (mod == 0 && (sib & 7) == 5) p += 4;
    }
    if (mod == 1) p += 1;
    else if (mod == 2 || (mod == 0 && rm == 5)) p += 4;
    if (op == 0xF6) {   // 8 bits : AL = quotient, AH = reste
        const unsigned al = (unsigned) g[REG_EAX] & 0xFF;
        g[REG_EAX] = (g[REG_EAX] & ~0xFFFF) | (al << 8);
    } else if (op16) {   // 16 bits : AX quotient, DX reste
        const unsigned ax = (unsigned) g[REG_EAX] & 0xFFFF;
        g[REG_EAX] = g[REG_EAX] & ~0xFFFF;
        g[REG_EDX] = (g[REG_EDX] & ~0xFFFF) | ax;
    } else {   // 32 bits : EAX quotient, EDX reste
        g[REG_EDX] = g[REG_EAX];
        g[REG_EAX] = 0;
    }
    g[REG_EIP] = (greg_t) (unsigned long) p;
    static int n = 0;
    if (n++ < 20) {
        g_load_bias = 0;
        g_module[0] = 0;
        compute_load_bias((unsigned long) ip);
        std::printf("[PORT] division entiere par zero a %s + 0x%lx : quotient 0 (comme sur ARM)\n",
                    g_module[0] ? g_module : "?", (unsigned long) ip - g_load_bias);
    }
}
}  // namespace
#endif

extern "C" void port_install_crash_handler(void)
{
    {
        struct sigaction sd;
        std::memset(&sd, 0, sizeof(sd));
        sd.sa_sigaction = on_dump;
        sd.sa_flags = SA_SIGINFO | SA_RESTART;
        sigaction(SIGUSR2, &sd, nullptr);
        pthread_t th;
        if (pthread_create(&th, nullptr, watchdog, nullptr) == 0)
            pthread_detach(th);
    }
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_fault;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
#if defined(__i386__)
    sa.sa_sigaction = on_intdiv;   // division par zero : comportement ARM (voir on_intdiv)
#endif
    sigaction(SIGFPE, &sa, nullptr);
}

// Studio : garder la page contenant `a` (lecture seule jusqu'a la premiere ecriture). 0 = posee.
extern "C" int port_guard_page(unsigned long a)
{
    const unsigned long page = a & ~4095ul;
    if (g_guard)
        mprotect((void *) g_guard, 4096, PROT_READ | PROT_WRITE);
    g_guard = 0;
    if (mprotect((void *) page, 4096, PROT_READ) != 0)
        return -1;
    g_guard_hit = 0;
    g_guard = page;
    return 0;
}

extern "C" unsigned long port_guard_hit(void) { return g_guard_hit; }
