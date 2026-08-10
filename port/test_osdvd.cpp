// -*- coding: utf-8 -*-
// Harnais de validation des couches OS et DVD, execute sur l'appareil.
//
// Ecrire 31 fonctions puis empiler les 218 suivantes dessus sans jamais les avoir executees
// serait la meilleure facon de chercher un bug de tas dans six semaines. Ce fichier les met a
// l'epreuve isolement, sur la cible reelle (armeabi-v7a), avant que le jeu ne s'en serve.
//
//   adb push test_osdvd /data/local/tmp/re4/ && adb shell /data/local/tmp/re4/test_osdvd <jeu>
#include <cstdio>
#include <cstring>

#include <dolphin.h>

void port_set_game_dir(const char *dir);
extern "C" void port_pad_set(u16, int, int, int, int, int, int);

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
    std::printf("  [%s] %s\n", ok ? "OK " : "NON", what);
    if (!ok)
        ++g_fail;
}

// ---------------------------------------------------------------- tas

void test_heap()
{
    std::printf("\n== tas ==\n");
    OSInit();
    void *lo = OSGetArenaLo();
    void *hi = OSGetArenaHi();
    check(lo && hi && hi > lo, "l'arene existe et est ordonnee");

    void *start = OSInitAlloc(lo, hi, 4);
    const int h = OSCreateHeap(start, hi);
    check(h >= 0, "un tas se cree sur l'arene");

    void *a = OSAllocFromHeap(h, 1024);
    void *b = OSAllocFromHeap(h, 4096);
    void *c = OSAllocFromHeap(h, 64);
    check(a && b && c, "trois allocations reussissent");
    check(a != b && b != c && a != c, "elles ne se recouvrent pas");
    check((((unsigned long)a) & 31u) == 0, "l'alignement 32 octets est respecte");

    // Ecrire dans chaque bloc : une allocation qui deborde sur la voisine se voit ici.
    std::memset(a, 0xAA, 1024);
    std::memset(b, 0xBB, 4096);
    std::memset(c, 0xCC, 64);
    bool intact = true;
    for (int i = 0; i < 1024; ++i) intact &= ((unsigned char *)a)[i] == 0xAA;
    for (int i = 0; i < 4096; ++i) intact &= ((unsigned char *)b)[i] == 0xBB;
    for (int i = 0; i < 64; ++i)   intact &= ((unsigned char *)c)[i] == 0xCC;
    check(intact, "les blocs ne se marchent pas dessus");

    // Le vrai test : liberer au milieu puis fragmenter. Sans fusion des blocs voisins, la
    // boucle ci-dessous epuise l'arene ; c'est exactement ce que fait un chargement de salle.
    OSFreeToHeap(h, b);
    void *b2 = OSAllocFromHeap(h, 4096);
    check(b2 == b, "le bloc libere est repris tel quel");
    OSFreeToHeap(h, b2);
    OSFreeToHeap(h, a);
    OSFreeToHeap(h, c);

    bool churn = true;
    for (int i = 0; i < 20000 && churn; ++i) {
        void *p = OSAllocFromHeap(h, 8192);
        churn = (p != nullptr);
        OSFreeToHeap(h, p);
    }
    check(churn, "20000 cycles alloc/liberation sans epuiser l'arene (fusion effective)");
}

// ---------------------------------------------------------------- temps

void test_time()
{
    std::printf("\n== temps ==\n");
    const OSTime t0 = OSGetTime();
    volatile double sink = 0;
    for (int i = 0; i < 4000000; ++i)
        sink += i * 0.5;
    const OSTime t1 = OSGetTime();
    (void)sink;
    check(t1 > t0, "l'horloge avance");
    // Le time base tourne a 40,5 MHz ; quelques millions d'iterations doivent donner
    // bien plus de 1000 ticks, et rester en deca d'une minute.
    const long long d = (long long)(t1 - t0);
    std::printf("      %lld ticks ecoules\n", d);
    check(d > 1000 && d < 40500000LL * 60, "la cadence est plausible");
}

// ---------------------------------------------------------------- disque

void test_dvd(const char *dir, const char *probe)
{
    std::printf("\n== disque ==\n");
    port_set_game_dir(dir);
    const s32 e = DVDConvertPathToEntrynum(probe);
    check(e >= 0, "un chemin connu se traduit en entrynum");
    if (e < 0)
        return;

    // La casse et le slash de tete ne doivent rien changer : le jeu ecrit les deux.
    char alt[256];
    std::snprintf(alt, sizeof(alt), "/%s", probe);
    for (char *p = alt; *p; ++p)
        if (*p >= 'a' && *p <= 'z')
            *p = (char)(*p - 'a' + 'A');
    check(DVDConvertPathToEntrynum(alt) == e, "meme entrynum en majuscules et avec un / initial");

    DVDFileInfo fi;
    check(DVDOpen(probe, &fi) == TRUE, "le fichier s'ouvre");
    std::printf("      longueur annoncee : %u octets\n", (unsigned)fi.length);
    check(fi.length > 0, "la longueur est renseignee");

    unsigned char buf[256];
    std::memset(buf, 0, sizeof(buf));
    check(DVDReadAsyncPrio(&fi, buf, (s32)sizeof(buf), 0, nullptr, 2) == TRUE, "la lecture repond");
    check(DVDGetTransferredSize(&fi) == sizeof(buf), "256 octets ont bien ete transferes");

    // Comparaison avec le fichier reel : une couche disque qui rend des zeros passerait tous
    // les tests precedents.
    char host[512];
    std::snprintf(host, sizeof(host), "%s/files/%s", dir, probe);
    std::FILE *f = std::fopen(host, "rb");
    if (!f) {
        std::snprintf(host, sizeof(host), "%s/%s", dir, probe);
        f = std::fopen(host, "rb");
    }
    if (f) {
        unsigned char ref[256];
        const size_t n = std::fread(ref, 1, sizeof(ref), f);
        std::fclose(f);
        check(n == sizeof(ref) && std::memcmp(ref, buf, n) == 0,
              "les octets lus sont identiques au fichier reel");
    } else {
        std::printf("  [ ? ] fichier hote introuvable, comparaison sautee\n");
    }

    // Une lecture a un offset non nul, parce que le jeu en fait tout le temps.
    std::memset(buf, 0, sizeof(buf));
    DVDReadAsyncPrio(&fi, buf, 64, 128, nullptr, 2);
    check(DVDGetTransferredSize(&fi) == 64, "lecture a un offset de 128");

    DVDClose(&fi);
    check(DVDConvertPathToEntrynum("ce/fichier/n/existe/pas.bin") < 0, "un chemin inconnu echoue");
}


// ---------------------------------------------------------------- video

int g_retrace_seen = 0;
void on_retrace(u32 count) { (void)count; ++g_retrace_seen; }

void test_vi()
{
    std::printf("\n== video ==\n");
    VIInit();
    VIRetraceCallback old = VISetPostRetraceCallback(on_retrace);
    check(old == nullptr, "la callback precedente est rendue");

    u8 fb[64];
    VISetNextFrameBuffer(fb);
    VIFlush();

    // Le vrai test : la cadence. 30 champs NTSC doivent prendre ~500 ms. Un VIWaitForRetrace
    // qui rend la main tout de suite passerait tous les autres tests et ferait tourner le jeu
    // en accelere sans que rien ne le signale.
    const OSTime t0 = OSGetTime();
    for (int i = 0; i < 30; ++i)
        VIWaitForRetrace();
    const OSTime t1 = OSGetTime();
    const double ms = (double)(long long)(t1 - t0) / 40500.0;
    std::printf("      30 champs en %.0f ms (attendu ~500)\n", ms);
    check(ms > 400.0 && ms < 700.0, "la cadence est bien celle du NTSC");
    check(g_retrace_seen == 30, "la callback de post-retrace est appelee a chaque champ");
    check(VIGetNextField() <= 1, "le champ alterne dans 0/1");
    VISetPostRetraceCallback(nullptr);
}

// ---------------------------------------------------------------- manette

void test_pad()
{
    std::printf("\n== manette ==\n");
    check(PADInit() == TRUE, "l'initialisation repond");

    PADStatus st[PAD_MAX_CONTROLLERS];
    PADRead(st);
    check(st[0].err == PAD_ERR_NONE, "le canal 0 est branche");
    check(st[1].err == PAD_ERR_NO_CONTROLLER &&
          st[2].err == PAD_ERR_NO_CONTROLLER &&
          st[3].err == PAD_ERR_NO_CONTROLLER, "les canaux 1 a 3 sont vides");
    check(st[0].button == 0 && st[0].stickX == 0 && st[0].stickY == 0,
          "au repos : aucun bouton, sticks centres");

    // Ce que fera la couche tactile.
    port_pad_set(PAD_BUTTON_A | PAD_BUTTON_START, -90, 70, 0, 0, 0, 255);
    PADRead(st);
    check((st[0].button & PAD_BUTTON_A) && (st[0].button & PAD_BUTTON_START), "les boutons passent");
    check(st[0].stickX == -90 && st[0].stickY == 70, "le stick est signe et centre sur zero");
    check(st[0].triggerRight == 255, "la gachette analogique passe");
    check(st[0].analogA == 255, "analogA suit le bouton A");

    // Debordement : une couche tactile maladroite enverrait 300.
    port_pad_set(0, 300, -300, 0, 0, 0, 0);
    PADRead(st);
    check(st[0].stickX == 127 && st[0].stickY == -128, "les valeurs hors bornes sont ecretees");

    PADClamp(st);
    check(st[1].err == PAD_ERR_NO_CONTROLLER, "PADClamp laisse les canaux vides tranquilles");
    port_pad_set(0, 0, 0, 0, 0, 0, 0);
}

} // namespace

int main(int argc, char **argv)
{
    const char *dir = (argc > 1) ? argv[1] : "/data/local/tmp/re4/game";
    const char *probe = (argc > 2) ? argv[2] : "st1/r100.das";
    std::printf("harnais OS/DVD  -  jeu : %s  -  sonde : %s\n", dir, probe);

    test_heap();
    test_time();
    test_dvd(dir, probe);
    test_vi();
    test_pad();

    std::printf("\n%s : %d echec(s)\n", g_fail ? "ECHEC" : "TOUT PASSE", g_fail);
    return g_fail ? 1 : 0;
}
