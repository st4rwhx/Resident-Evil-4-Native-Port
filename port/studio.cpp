// -*- coding: utf-8 -*-
// Nightjar Studio, brique S1 (15/11, documentation interne) : le jeu parle en donnees et se
// pilote sans recompiler.
//
// Serveur TCP sur 127.0.0.1:7770 (adb forward tcp:7770 tcp:7770 : MuMu comme le S5), lu une fois par image depuis la
// boucle principale du jeu (port_studio_frame, main.cpp) : aucune commande ne touche l'etat du jeu depuis un autre fil.
// Protocole : une commande texte par ligne, une reponse JSON par ligne.
//   etat                              etat du jeu (image, salle, mode, Leon, flux son, cadence)
//   evenements                        sentinelles accumulees depuis la derniere lecture (puis videes)
//   outil <NOM DU MENU>               lance un outil de debug de Capcom par son nom (DbMenuSetExecTool + DbMenuExec)
//   texte 0|1                         texte de debug du jeu (eprintf) affiche ou non
//   drapeau 60|64 <masque hex> 0|1    interrupteurs de debug (FLAG EDIT : collisions, squelettes, ralenti...)
//   pad <boutons hex> <sx> <sy> <cx> <cy> <images>   manette imposee pendant <images> lectures (0 : relache)
//   aide
// Les evenements viennent de port_studio_event() appele par le reste du portage (son, rendu, fichiers, plantage).
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>

// Sites de conversions flottant -> entier hors intervalle (version PORT_UBSAN_FLOAT=1), voir plus bas.
extern "C" int port_guard_page(unsigned long a);
extern "C" int port_guard_page2(unsigned long a, int lecture);
extern "C" unsigned long port_guard_hit(void);
// Enregistreur FIFO du moteur + journal [FL] de l'image (port/njgfx/njgfx.cpp ; <atomic> ici heurte math_sub.h).
extern "C" void njgfx_studio_fifo(int frames);
// Modeles rendus (commande « rendus », 16/11) : trans.cpp ModelRender appelle port_studio_rendu(m, couche OT, saute)
// a chaque modele ; on garde la liste de l'image precedente (complete) et celle en cours.
namespace {
struct Rendu { unsigned m; short ot; short saute; };
Rendu g_rendus[2][1024];
int g_nrendus[2] = {0, 0};
int g_rcur = 0;
}

namespace {
struct ConvSite { void* pc; unsigned n; };
ConvSite g_conv[256];
unsigned g_nconv = 0, g_conv_lost = 0;
}

#include "global.h"
#include "player.h"
#include "snd_drv.h"
#include "njlink.h"
#include <dolphin/pad.h>
#include "room_jmp.h"
#include "main.h"
#include "mes.h"
#include "em.h"

extern "C" void njstate_fill(NjState* out);   // port/njstate.cpp
extern "C" void port_studio_frame(void);
void DbMenuSetExecTool(const char* name);     // src/game/db_menu.cpp
void DbMenuExec();
int DbMenuActiveCheck();

namespace {

int g_listen = -1, g_client = -1;
char g_in[512];
int g_in_len = 0;
unsigned g_frame = 0;

// sentinelles : anneau de 64 lignes
char g_ev[64][200];
unsigned g_ev_head = 0, g_ev_count = 0, g_ev_lost = 0;

// manette imposee
unsigned g_pad_left = 0;
u16 g_pad_buttons = 0;
int g_pad_sx = 0, g_pad_sy = 0, g_pad_cx = 0, g_pad_cy = 0;

int g_text = -1;   // -1 : reglage d'origine (RE4_EPRINTF)

// reglages en direct (S4 en germe) : nom -> valeur, lus par le portage avec port_knob()
struct Knob { char name[24]; int value; };
Knob g_knobs[32];
int g_nknobs = 0;
static int g_sec_val = 0, g_sec_n = 0;   // commande « secousse » (W3) : dixiemes de g, images restantes (port_accel)
int g_capture = 0;   // capture demandee (lue par port/njgfx a la fin de l'image)

// S2 (16/11) : pause, images a pas compte, surveillance d'adresses aux points de la boucle de jeu (ProcessTickGet).
int g_paused = 0;          // 1 : le fil du jeu attend dans le Studio (port_studio_frame ou un point surveille)
int g_steps = 0;           // images a jouer avant de se remettre en pause (0 : selon g_paused)
const char* g_last_point = "debut_image";
const char* g_paused_at = "";
struct Watch { unsigned long addr; unsigned len; unsigned char last[64]; int stop; int zero; int on; unsigned hits; };
Watch g_watch[8];
int g_memfd = -1;

// Lecture / ecriture sures n'importe ou dans le processus : /proc/self/mem rend une erreur au lieu d'une faute.
bool mem_read(unsigned long a, void* out, unsigned n)
{
    if (g_memfd < 0) g_memfd = open("/proc/self/mem", O_RDWR);
#ifdef __APPLE__
    return g_memfd >= 0 && pread(g_memfd, out, n, (off_t) a) == (ssize_t) n;
#else
    return g_memfd >= 0 && pread64(g_memfd, out, n, (off64_t) a) == (ssize_t) n;
#endif
}
bool mem_write(unsigned long a, const void* in, unsigned n)
{
    if (g_memfd < 0) g_memfd = open("/proc/self/mem", O_RDWR);
#ifdef __APPLE__
    return g_memfd >= 0 && pwrite(g_memfd, in, n, (off_t) a) == (ssize_t) n;
#else
    return g_memfd >= 0 && pwrite64(g_memfd, in, n, (off64_t) a) == (ssize_t) n;
#endif
}
void hexs(char* out, const unsigned char* b, unsigned n)
{
    static const char hx[] = "0123456789abcdef";
    for (unsigned i = 0; i < n; ++i) { out[2 * i] = hx[b[i] >> 4]; out[2 * i + 1] = hx[b[i] & 15]; }
    out[2 * n] = 0;
}
void studio_poll();   // plus bas : lit et execute les commandes en attente

void send_line(const char* s)
{
    if (g_client < 0)
        return;
    const size_t n = std::strlen(s);
    if (send(g_client, s, n, MSG_NOSIGNAL) < 0 || send(g_client, "\n", 1, MSG_NOSIGNAL) < 0) {
        close(g_client);
        g_client = -1;
    }
}

void open_server()
{
    static bool tried = false;
    if (tried)
        return;
    tried = true;
    if (std::getenv("NJ_STUDIO_OFF"))
        return;
    g_listen = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen < 0)
        return;
    int one = 1;
    setsockopt(g_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a = {};
    a.sin_family = AF_INET;
    a.sin_port = htons(7770);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(g_listen, (sockaddr*) &a, sizeof a) != 0 || listen(g_listen, 1) != 0) {
        std::printf("[STUDIO] prise 7770 indisponible (%d)\n", errno);
        close(g_listen);
        g_listen = -1;
        return;
    }
    fcntl(g_listen, F_SETFL, fcntl(g_listen, F_GETFL) | O_NONBLOCK);
    std::printf("[STUDIO] a l'ecoute sur 127.0.0.1:7770\n");
}

void json_escape(char* dst, size_t cap, const char* src)
{
    size_t o = 0;
    for (; *src && o + 2 < cap; ++src) {
        const char c = *src;
        if (c == '"' || c == '\\') { dst[o++] = '\\'; dst[o++] = c; }
        else if ((unsigned char) c >= 0x20) dst[o++] = c;
    }
    dst[o] = 0;
}

void cmd_etat()
{
    NjState st;
    njstate_fill(&st);
    char flux[600];
    size_t o = 0;
    o += std::snprintf(flux + o, sizeof flux - o, "[");
    for (int i = 0; i < SND_STR_MAX; ++i) {
        const SND_STR_WORK* s = &Snd_str_work[i];
        o += std::snprintf(flux + o, sizeof flux - o, "%s{\"id\":%u,\"etat\":%d,\"statut\":%u,\"voix\":%d,\"lu\":%u,\"fin\":%u}",
                           i ? "," : "", (unsigned) s->snd_id, (int) s->state, (unsigned) s->status,
                           s->voiceL ? (int) s->voiceL->pb.state : -1, (unsigned) s->read_ofs, (unsigned) s->read_end);
        if (o >= sizeof flux) break;
    }
    if (o < sizeof flux) std::snprintf(flux + o, sizeof flux - o, "]");
    char out[1400];
    const bool pl = pPL != nullptr;
    std::snprintf(out, sizeof out,
                  "{\"image\":%u,\"salle\":\"%03x\",\"mode\":%u,\"bits\":\"%05x\",\"menu_debug\":%d,"
                  "\"leon\":{\"x\":%.1f,\"y\":%.1f,\"z\":%.1f,\"cap\":%.3f,\"vie\":%d},\"flags_60\":\"%08x\",\"flags_170\":\"%08x\","
                  "\"ecran\":{\"curseur\":%d,\"nb_curseur\":%d,\"choix\":%d,\"choix_curseur\":%d,\"invite\":%u,"
                  "\"titre_alpha\":%.2f,\"fondu\":%.2f,\"qte\":%u},"
                  "\"flux\":%s,\"evenements_en_attente\":%u,"
                  "\"camera\":{\"pos\":[%.1f,%.1f,%.1f],\"vise\":[%.1f,%.1f,%.1f],\"haut\":[%.3f,%.3f,%.3f],\"roulis\":%.3f}}",
                  g_frame, (unsigned) G_ROOM_ID, (unsigned) st.mode, (unsigned) st.bits, DbMenuActiveCheck(),
                  pl ? pPL->pos.x : 0.f, pl ? pPL->pos.y : 0.f, pl ? pPL->pos.z : 0.f, pl ? pPL->rot.y : 0.f,
                  (int) (s16) pG->pl_life, (unsigned) pG->flags_60, (unsigned) pG->flags_170, (int) st.cursor,
                  (int) st.cursor_count, (int) st.choice_count, (int) st.choice_cursor, (unsigned) st.prompt,
                  st.title_menu_alpha, st.fade, (unsigned) st.qte, flux, g_ev_count,
                  pG->Cam.param.pos.x, pG->Cam.param.pos.y, pG->Cam.param.pos.z, pG->Cam.param.at.x, pG->Cam.param.at.y,
                  pG->Cam.param.at.z, pG->Cam.up.x, pG->Cam.up.y, pG->Cam.up.z, pG->Cam.param.roll);
    send_line(out);
}

void cmd_evenements()
{
    char line[260];
    std::snprintf(line, sizeof line, "{\"evenements\":%u,\"perdus\":%u}", g_ev_count, g_ev_lost);
    send_line(line);
    const unsigned start = (g_ev_head + 64 - g_ev_count) % 64;
    for (unsigned k = 0; k < g_ev_count; ++k) {
        char esc[220];
        json_escape(esc, sizeof esc, g_ev[(start + k) % 64]);
        std::snprintf(line, sizeof line, "{\"evenement\":\"%s\"}", esc);
        send_line(line);
    }
    g_ev_count = 0;
    g_ev_lost = 0;
}

void handle(char* l)
{
    while (*l == ' ') ++l;
    char* e = l + std::strlen(l);
    while (e > l && (e[-1] == '\r' || e[-1] == ' ')) *--e = 0;
    if (!*l) return;
    if (!std::strcmp(l, "etat")) { cmd_etat(); return; }
    if (!std::strcmp(l, "evenements")) { cmd_evenements(); return; }
    if (!std::strncmp(l, "outil ", 6)) {
        if (DbMenuActiveCheck()) { send_line("{\"erreur\":\"menu de debug deja ouvert\"}"); return; }
        static char name[64];   // DbMenuSetExecTool garde le pointeur
        std::snprintf(name, sizeof name, "%s", l + 6);
        DbMenuSetExecTool(name);
        DbMenuExec();
        if (g_text < 0) g_text = 1;   // un outil sans son texte est un menu invisible qui arrete le jeu
        send_line("{\"ok\":\"outil lance\"}");
        return;
    }
    if (!std::strncmp(l, "texte ", 6)) { g_text = std::atoi(l + 6) ? 1 : 0; send_line("{\"ok\":\"texte\"}"); return; }
    if (!std::strncmp(l, "drapeau ", 8)) {
        int champ = 0, on = 0;
        unsigned mask = 0;
        if (std::sscanf(l + 8, "%d %x %d", &champ, &mask, &on) == 3 && (champ == 60 || champ == 64)) {
            u32* f = champ == 60 ? (u32*) &pG->flags_60 : (u32*) &pG->flags_64;
            if (on) *f |= mask; else *f &= ~mask;
            char out[96];
            std::snprintf(out, sizeof out, "{\"ok\":\"flags_%d\",\"valeur\":\"%08x\"}", champ, (unsigned) *f);
            send_line(out);
        } else send_line("{\"erreur\":\"drapeau 60|64 <masque hex> 0|1\"}");
        return;
    }
    if (!std::strncmp(l, "pad ", 4)) {
        unsigned b = 0, n = 0;
        int sx = 0, sy = 0, cx = 0, cy = 0;
        if (std::sscanf(l + 4, "%x %d %d %d %d %u", &b, &sx, &sy, &cx, &cy, &n) == 6) {
            g_pad_buttons = (u16) b; g_pad_sx = sx; g_pad_sy = sy; g_pad_cx = cx; g_pad_cy = cy; g_pad_left = n;
            send_line("{\"ok\":\"pad\"}");
        } else send_line("{\"erreur\":\"pad <boutons hex> <sx> <sy> <cx> <cy> <images>\"}");
        return;
    }
    if (!std::strncmp(l, "salle ", 6)) {   // salle <chapitre> <salle hex> [point] : comme AREA JUMP (room_jmp.cpp)
        unsigned st = 0, rm = 0, pt = 0;
        const int n = std::sscanf(l + 6, "%u %x %u", &st, &rm, &pt);
        if (n < 2 || !roomInfoAddr || !pPL) { send_line("{\"erreur\":\"salle <chapitre> <salle hex> [point] (en jeu seulement)\"}"); return; }
        cRoomJmp rj(roomInfoAddr);
        if (rm > 0xFF) rm &= 0xFF;   // 0x100 -> salle 00 du chapitre 1
        const u8 idx = rj.getRoomIdx((u8) st, (u8) rm);
        if (pt >= (unsigned) rj.getPointNum((s8) st, (s8) idx)) pt = 0;
        CRoomInfo* ri = rj.getRoomInfo((u8) st, (u8) (idx + pt));
        if (!ri) { send_line("{\"erreur\":\"salle inconnue de roomInfo.dat\"}"); return; }
        // Comme roomJumpExec (room_jmp.cpp) : sans la remise a zero des messages, une fenetre de texte de la salle
        // precedente etait mesuree sur une memoire liberee (boucle sans fin dans Message::WidthCk, 15/11).
        ri->setNextPos();
        // Comme roomJumpExec (room_jmp.cpp) : marqueur « saut de salle » lu au chargement de la salle suivante. Oublie
        // jusqu'au 16/11 : la table de messages de la salle precedente restait referencee (WidthCk sur memoire ecrasee,
        // plantage au saut vers r101) et des sauts repetes laissaient le jeu fige.
        pG->flags_68 |= 0x80000000;
        pG->x4F9F = (u8) pt;
        cMes.roomInit();
        for (int i = 0; i <= 0xF; i++)
            cMes.Delete(i);
        pG->pl_life = pG->pl_life_max;
        pG->x4F90 = 0;
        pG->x20 = 4;
        pG->x21 = 0;
        pG->x22 = 0;
        pG->x23 = 0;
        pG->flags_54 &= ~0x40;
        char out[96];
        std::snprintf(out, sizeof out, "{\"ok\":\"saut vers %x%02x point %u\"}", st, rm, pt);
        send_line(out);
        return;
    }
    if (!std::strncmp(l, "secousse ", 9)) {   // secousse <dixiemes de g> <images> : acceleration Wii simulee (W3)
        int v = 0, n = 0;
        if (std::sscanf(l + 9, "%d %d", &v, &n) == 2) {
            g_sec_val = v;
            g_sec_n = n;
            send_line("{\"ok\":\"secousse\"}");
        } else send_line("{\"erreur\":\"secousse <dixiemes_de_g> <images>\"}");
        return;
    }
    if (!std::strncmp(l, "reglage ", 8)) {   // reglage <nom> <valeur> : lu par port_knob(nom) sans recompiler
        char name[24] = {0};
        int v = 0;
        if (std::sscanf(l + 8, "%23s %d", name, &v) == 2) {
            int k = 0;
            while (k < g_nknobs && std::strcmp(g_knobs[k].name, name)) ++k;
            if (k == g_nknobs && g_nknobs < 32) { std::snprintf(g_knobs[k].name, sizeof g_knobs[k].name, "%s", name); ++g_nknobs; }
            if (k < 32) g_knobs[k].value = v;
            send_line("{\"ok\":\"reglage\"}");
        } else send_line("{\"erreur\":\"reglage <nom> <valeur>\"}");
        return;
    }
    if (!std::strcmp(l, "capture")) {   // image courante -> <stockage>/studio_<image>.ppm (640x480), lue par njgfx
        g_capture = 1;
        send_line("{\"ok\":\"capture a l'image suivante (studio_<n>.ppm)\"}");
        return;
    }
    if (!std::strncmp(l, "memoire ", 8)) {   // memoire <adresse hex> <octets> : lecture de MEM1 (2048 octets au plus)
        unsigned a = 0, n = 0;
        if (std::sscanf(l + 8, "%x %u", &a, &n) != 2 || n == 0 || n > 2048 || a < 0x80000000u || a + n > 0x83000000u || a + n < a) {
            send_line("{\"erreur\":\"memoire <adresse hex 80000000..81FFFFFF> <octets 1..2048>\"}");
            return;
        }
        static char out[2048 * 2 + 64];
        int k = std::snprintf(out, sizeof out, "{\"adresse\":\"%08x\",\"octets\":\"", a);
        const u8* p = (const u8*) (unsigned long) a;
        static const char hx[] = "0123456789abcdef";
        for (unsigned i = 0; i < n; ++i) {
            out[k++] = hx[p[i] >> 4];
            out[k++] = hx[p[i] & 15];
        }
        std::snprintf(out + k, sizeof out - k, "\"}");
        send_line(out);
        return;
    }
    if (!std::strncmp(l, "message ", 8)) {   // message <no> <attr hex> [x y] : affiche un texte comme le jeu (MesSet, case 0)
        int no = 0, x = 120, y = 360;
        unsigned attr = 0x20088;
        if (std::sscanf(l + 8, "%d %x %d %d", &no, &attr, &x, &y) < 2) {
            send_line("{\"erreur\":\"message <no> <attr hex> [x y] (0x20088 : nom d'objet, table 3)\"}");
            return;
        }
        cMes.MesSet(no, x, y, attr | 0x80, 0, 0, 4);   // 0x80 : ne bloque pas les commandes du joueur
        char out[96];
        std::snprintf(out, sizeof out, "{\"ok\":\"message %d attr %x\"}", no, attr | 0x80);
        send_line(out);
        return;
    }
    if (!std::strcmp(l, "ennemis")) {   // ennemis vivants : numero, modele, type, vie, statut, position, drapeaux
        static char out[2400];
        int k = std::snprintf(out, sizeof out, "{\"ennemis\":[");
        int first = 1;
        for (u32 i = 0; i < EmMgr.nArray && k < (int) sizeof out - 160; ++i) {
            cEm* e = EmMgrWork(i);
            if (!e || !(e->be_flag & 1) || e == (cEm*) pPL)
                continue;
            k += std::snprintf(out + k, sizeof out - k,
                               "%s{\"n\":%u,\"id\":\"%02x\",\"type\":%u,\"vie\":%d,\"vie_max\":%d,\"stat\":\"%08x\",\"pos\":[%.0f,%.0f,%.0f],\"be\":\"%08x\"}",
                               first ? "" : ",", (unsigned) i, (unsigned) e->id, (unsigned) e->type, (int) e->hp, (int) e->hpMax,
                               (unsigned) e->stat, e->pos.x, e->pos.y, e->pos.z, (unsigned) e->be_flag);
            first = 0;
        }
        std::snprintf(out + k, sizeof out - k, "]}");
        send_line(out);
        return;
    }
    if (!std::strcmp(l, "pause")) { g_paused = 1; g_steps = 0; send_line("{\"ok\":\"pause a la prochaine image\"}"); return; }
    if (!std::strcmp(l, "reprendre")) { g_paused = 0; g_steps = 0; send_line("{\"ok\":\"reprise\"}"); return; }
    if (!std::strncmp(l, "image", 5) && (l[5] == 0 || l[5] == ' ')) {   // image [n] : jouer n images puis pause
        g_steps = l[5] ? std::atoi(l + 6) : 1;
        if (g_steps < 1) g_steps = 1;
        g_paused = 0;
        char out[64];
        std::snprintf(out, sizeof out, "{\"ok\":\"%d image(s) puis pause\"}", g_steps);
        send_line(out);
        return;
    }
    if (!std::strcmp(l, "ou")) {   // ou en est le fil du jeu : image, pause, dernier point de la boucle
        char out[200];
        std::snprintf(out, sizeof out, "{\"image\":%u,\"pause\":%d,\"arret\":\"%s\",\"dernier_point\":\"%s\"}", g_frame, g_paused,
                      g_paused_at, g_last_point);
        send_line(out);
        return;
    }
    if (!std::strcmp(l, "base")) {   // adresse de chargement du coeur : symbole = base + valeur de llvm-nm
        Dl_info di;
        const unsigned long b = (dladdr((void*) &port_studio_frame, &di) && di.dli_fbase) ? (unsigned long) di.dli_fbase : 0;
        char out[96];
        std::snprintf(out, sizeof out, "{\"base\":\"%lx\",\"pG\":\"%lx\",\"pPL\":\"%lx\"}", b, (unsigned long) (void*) pG,
                      (unsigned long) (void*) pPL);
        send_line(out);
        return;
    }
    if (!std::strncmp(l, "lire ", 5)) {   // lire <adresse hex> <octets> : n'importe ou (memoire du jeu, globales natives)
        unsigned long a = 0;
        unsigned n = 0;
        if (std::sscanf(l + 5, "%lx %u", &a, &n) != 2 || n == 0 || n > 2048) { send_line("{\"erreur\":\"lire <adresse hex> <1..2048>\"}"); return; }
        static unsigned char buf[2048];
        static char out[2048 * 2 + 64];
        if (!mem_read(a, buf, n)) { send_line("{\"erreur\":\"adresse illisible\"}"); return; }
        int k = std::snprintf(out, sizeof out, "{\"adresse\":\"%lx\",\"octets\":\"", a);
        hexs(out + k, buf, n);
        k += 2 * n;
        std::snprintf(out + k, sizeof out - k, "\"}");
        send_line(out);
        return;
    }
    if (!std::strncmp(l, "ecrire ", 7)) {   // ecrire <adresse hex> <octets hex> : changer une valeur en direct
        unsigned long a = 0;
        char hx[260] = {0};
        if (std::sscanf(l + 7, "%lx %258s", &a, hx) != 2 || (std::strlen(hx) & 1)) { send_line("{\"erreur\":\"ecrire <adresse hex> <octets hex, 128 max>\"}"); return; }
        unsigned char b[130];
        const unsigned n = (unsigned) std::strlen(hx) / 2;
        for (unsigned i = 0; i < n; ++i) { unsigned v = 0; std::sscanf(hx + 2 * i, "%2x", &v); b[i] = (unsigned char) v; }
        send_line(mem_write(a, b, n) ? "{\"ok\":\"ecrit\"}" : "{\"erreur\":\"adresse non inscriptible\"}");
        return;
    }
    if (!std::strncmp(l, "surveiller ", 11)) {   // surveiller <adresse hex> <octets> [arret] : dit QUI la change
        unsigned long a = 0;
        unsigned n = 0;
        char opt[40] = {0};
        const int c = std::sscanf(l + 11, "%lx %u %39[a-z ]", &a, &n, opt);
        int slot = -1;
        for (int i = 0; i < 8; ++i) if (!g_watch[i].on) { slot = i; break; }
        if (c < 2 || n == 0 || n > 64 || slot < 0) { send_line("{\"erreur\":\"surveiller <adresse hex> <1..64> [arret] (8 au plus)\"}"); return; }
        Watch& w = g_watch[slot];
        if (!mem_read(a, w.last, n)) { send_line("{\"erreur\":\"adresse illisible\"}"); return; }
        w.addr = a; w.len = n; w.stop = c == 3 && std::strstr(opt, "arret") != nullptr; w.zero = c == 3 && std::strstr(opt, "zero") != nullptr;
        w.on = 1; w.hits = 0;
        char out[96];
        std::snprintf(out, sizeof out, "{\"ok\":\"surveillance %d\",\"arret\":%d}", slot, w.stop);
        send_line(out);
        return;
    }
    if (!std::strncmp(l, "rendus", 6)) {   // rendus : modeles passes par ModelRender dans l'image precedente (adresse:couche:saute)
        static char out[1024 * 20 + 64];
        const int b = g_rcur ^ 1;
        int k = std::snprintf(out, sizeof out, "{\"n\":%d,\"rendus\":\"", g_nrendus[b]);
        for (int i = 0; i < g_nrendus[b] && k < (int) sizeof out - 32; ++i)
            k += std::snprintf(out + k, sizeof out - k, "%x:%d:%d ", g_rendus[b][i].m, g_rendus[b][i].ot, g_rendus[b][i].saute);
        std::snprintf(out + k, sizeof out - k, "\"}");
        send_line(out);
        return;
    }
    if (!std::strncmp(l, "fifo", 4)) {   // fifo [n] : enregistre n images (.dff, 6 par defaut) + journal [FL] de l'image
        int n = l[4] ? std::atoi(l + 5) : 6;
        if (n <= 0 || n > 60) n = 6;
        njgfx_studio_fifo(n);
        char out[96];
        std::snprintf(out, sizeof out, "{\"ok\":\"fifo\",\"images\":%d,\"fichier\":\"fifo_*.dff (dossier de l'appli)\"}", n);
        send_line(out);
        return;
    }
    if (!std::strncmp(l, "garder ", 7)) {   // garder <adr> : page en lecture seule, auteur de la 1re ecriture -> journal [GARDE]
        const unsigned long a = std::strtoul(l + 7, nullptr, 16);
        const int r = port_guard_page2(a, std::strstr(l + 7, "lecture") != nullptr);   // garder <adr> [lecture]
        char out[96];
        std::snprintf(out, sizeof out, "{\"ok\":\"garde\",\"page\":\"%08lx\",\"resultat\":%d}", a & ~4095ul, r);
        send_line(out);
        return;
    }
    if (!std::strncmp(l, "garde", 5)) {   // garde : pc de la derniere ecriture attrapee (0 = rien encore)
        char out[64];
        std::snprintf(out, sizeof out, "{\"garde_pc\":\"%lx\"}", port_guard_hit());
        send_line(out);
        return;
    }
    if (!std::strncmp(l, "oublier", 7)) {   // oublier [n] : retirer une surveillance (toutes sans numero)
        const int n = l[7] ? std::atoi(l + 8) : -1;
        for (int i = 0; i < 8; ++i) if (n < 0 || n == i) g_watch[i].on = 0;
        send_line("{\"ok\":\"oublie\"}");
        return;
    }
    if (!std::strncmp(l, "appeler ", 8)) {
        // appeler <adresse hex> [a0..a5] : appelle une fonction du jeu (ou d'un outil de Capcom) sur le fil du jeu, avec
        // jusqu'a 6 arguments entiers (hex 0x.. ou decimal). Le client resout les noms (llvm-nm). Les arguments flottants
        // ne passent pas (registres VFP sur ARM) : pour eux, poser les valeurs en memoire et appeler une fonction sans.
        unsigned long fa = 0;
        unsigned long a[6] = {0, 0, 0, 0, 0, 0};
        char* q = l + 8;
        fa = std::strtoul(q, &q, 16);
        int na = 0;
        while (na < 6 && *q) {
            while (*q == ' ') ++q;
            if (!*q) break;
            a[na++] = std::strtoul(q, &q, 0);
        }
        Dl_info di;
        if (!fa || !dladdr((void*) fa, &di)) { send_line("{\"erreur\":\"appeler <adresse de fonction hex> [args] : adresse inconnue\"}"); return; }
        typedef unsigned long (*Fn6)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
        const unsigned long r = ((Fn6) fa)(a[0], a[1], a[2], a[3], a[4], a[5]);
        char out[128];
        std::snprintf(out, sizeof out, "{\"ok\":\"appel\",\"resultat\":\"%lx\",\"arguments\":%d}", r, na);
        send_line(out);
        return;
    }
    if (!std::strncmp(l, "position ", 9)) {   // position <x> <y> <z> : teleporter Leon dans la salle courante
        float x, y, z;
        if (!pPL || std::sscanf(l + 9, "%f %f %f", &x, &y, &z) != 3) { send_line("{\"erreur\":\"position <x> <y> <z> (en jeu)\"}"); return; }
        pPL->pos.x = x; pPL->pos.y = y; pPL->pos.z = z;
        send_line("{\"ok\":\"Leon deplace\"}");
        return;
    }
    if (!std::strcmp(l, "conversions")) {   // sites de conversions flottant -> entier hors intervalle (PORT_UBSAN_FLOAT=1)
        static char out[256 * 28 + 64];
        int k = std::snprintf(out, sizeof out, "{\"perdus\":%u,\"sites\":[", g_conv_lost);
        for (unsigned i = 0; i < g_nconv && k < (int) sizeof out - 40; ++i) {
            Dl_info di;
            const unsigned long base = (dladdr(g_conv[i].pc, &di) && di.dli_fbase) ? (unsigned long) di.dli_fbase : 0;
            k += std::snprintf(out + k, sizeof out - k, "%s[\"%lx\",%u]", i ? "," : "", (unsigned long) g_conv[i].pc - base, g_conv[i].n);
        }
        std::snprintf(out + k, sizeof out - k, "]}");
        send_line(out);
        return;
    }
    if (!std::strcmp(l, "aide")) {
        send_line("{\"commandes\":[\"etat\",\"evenements\",\"outil <NOM>\",\"texte 0|1\",\"drapeau 60|64 <masque> 0|1\","
                  "\"pad <boutons> <sx> <sy> <cx> <cy> <images>\",\"salle <chapitre> <salle hex> [point]\",\"reglage <nom> <valeur>\",\"capture\",\"memoire <adresse> <octets>\",\"pause\",\"image [n]\",\"reprendre\",\"ou\",\"base\",\"lire <adr> <n>\",\"ecrire <adr> <hex>\",\"surveiller <adr> <n> [arret] [zero]\",\"oublier [n]\",\"position <x> <y> <z>\",\"message <no> <attr>\",\"ennemis\",\"conversions\"]}");
        return;
    }
    send_line("{\"erreur\":\"commande inconnue (aide)\"}");
}

}  // namespace

// Conversions flottant -> entier hors intervalle (version PORT_UBSAN_FLOAT=1, 15/11). Le compilateur appelle ces fonctions
// juste apres la conversion fautive ; on retient l'adresse de retour (un site du code) et le nombre de passages. Le
// resultat de la conversion reste celui de la machine : c'est un releve, pas une correction.
extern "C" void port_studio_event(const char* fmt, ...);
static void port_conv_note(void* pc)
{
    for (unsigned i = 0; i < g_nconv; ++i)
        if (g_conv[i].pc == pc) { ++g_conv[i].n; return; }
    if (g_nconv < 256) {
        g_conv[g_nconv].pc = pc;
        g_conv[g_nconv].n = 1;
        ++g_nconv;
        Dl_info di;
        const unsigned long base = (dladdr(pc, &di) && di.dli_fbase) ? (unsigned long) di.dli_fbase : 0;
        port_studio_event("conversion_flottante site=%lx", (unsigned long) pc - base);
    } else ++g_conv_lost;
}
extern "C" void __ubsan_handle_float_cast_overflow_minimal() { port_conv_note(__builtin_return_address(0)); }
extern "C" void __ubsan_handle_float_cast_overflow_minimal_abort() { port_conv_note(__builtin_return_address(0)); }

extern "C" void port_studio_point(const char* name);   // plus bas
extern "C" void port_studio_event(const char* fmt, ...)
{
    char buf[200];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    std::snprintf(g_ev[g_ev_head], sizeof g_ev[0], "%u %s", g_frame, buf);
    g_ev_head = (g_ev_head + 1) % 64;
    if (g_ev_count < 64) ++g_ev_count; else ++g_ev_lost;
}

// Texte de debug : reglage du Studio, sinon RE4_EPRINTF (src/game/eprintf.cpp)
extern "C" int port_studio_text(void) { return g_text; }

// Capture demandee par le Studio : 1 une seule fois (port/njgfx)
extern "C" int port_studio_capture(void)
{
    const int c = g_capture;
    g_capture = 0;
    return c;
}

// Reglage en direct (commande « reglage ») ; def si jamais pose
extern "C" int port_knob(const char* name, int def)
{
    for (int k = 0; k < g_nknobs; ++k)
        if (!std::strcmp(g_knobs[k].name, name))
            return g_knobs[k].value;
    return def;
}

// Commandes Wii (W3, 17/11) : acceleration totale de la « Wiimote » en g, lue une fois par image par PadRead (pad.cpp),
// qui en tire les actions 48/49/50 comme la Wii (2 / 3 / 1,5 g). Sources : la commande Studio « secousse <dixiemes de g>
// <images> » (essais), puis Nightjar (g_nj_accel : glisser / accelerometre / souris, a brancher par njlink).
float g_nj_accel = 0.0f;
extern "C" float port_accel()
{
    if (g_sec_n > 0) {
        --g_sec_n;
        return (float) g_sec_val / 10.0f;
    }
    return g_nj_accel;
}

// Manette imposee par le Studio (port/pad.cpp, apres celle de l'appli)
extern "C" int port_studio_pad(PADStatus* s)
{
    if (!g_pad_left)
        return 0;
    --g_pad_left;
    s->button = g_pad_buttons;
    s->stickX = (s8) g_pad_sx;
    s->stickY = (s8) g_pad_sy;
    s->substickX = (s8) g_pad_cx;
    s->substickY = (s8) g_pad_cy;
    s->triggerLeft = (g_pad_buttons & PAD_TRIGGER_L) ? 255 : 0;
    s->triggerRight = (g_pad_buttons & PAD_TRIGGER_R) ? 255 : 0;
    return 1;
}

extern "C" void port_studio_rendu(void* m, int ot, int saute)
{
    int& n = g_nrendus[g_rcur];
    if (n < 1024) g_rendus[g_rcur][n++] = Rendu{(unsigned) (unsigned long) m, (short) ot, (short) saute};
}

// Une fois par image, depuis la boucle principale (main.cpp)
extern "C" void port_studio_frame(void)
{
    ++g_frame;
    g_rcur ^= 1;
    g_nrendus[g_rcur] = 0;
    open_server();
    if (g_listen < 0)
        return;
    if (g_client < 0) {
        g_client = accept(g_listen, nullptr, nullptr);
        if (g_client >= 0) {
            fcntl(g_client, F_SETFL, fcntl(g_client, F_GETFL) | O_NONBLOCK);
            g_in_len = 0;
            send_line("{\"studio\":\"Nightjar Studio S1\",\"aide\":\"aide\"}");
        }
    }
    if (g_client < 0)
        return;
    studio_poll();
    g_last_point = "debut_image";
    port_studio_point("debut_image");
    // pause / images comptees : le fil du jeu attend ici, entre deux images, en servant les commandes
    if (g_steps > 0) {
        if (--g_steps == 0) g_paused = 1;
    }
    while (g_paused && g_client >= 0) {
        g_paused_at = "debut_image";
        studio_poll();
        if (!g_paused || g_steps > 0) break;
        usleep(4000);
    }
    g_paused_at = "";
}

namespace {
void studio_poll()
{
    if (g_client < 0)
        return;
    for (;;) {
        const ssize_t n = recv(g_client, g_in + g_in_len, sizeof g_in - 1 - g_in_len, 0);
        if (n == 0) { close(g_client); g_client = -1; return; }
        if (n < 0) break;   // EAGAIN : plus rien cette image
        g_in_len += (int) n;
        g_in[g_in_len] = 0;
        char* nl;
        while ((nl = std::strchr(g_in, '\n')) != nullptr) {
            *nl = 0;
            handle(g_in);
            if (g_client < 0) return;
            const int rest = g_in_len - (int) (nl + 1 - g_in);
            std::memmove(g_in, nl + 1, rest);
            g_in_len = rest;
            g_in[g_in_len] = 0;
        }
        if (g_in_len >= (int) sizeof g_in - 1) g_in_len = 0;   // ligne trop longue : jetee
    }
}
}  // namespace

// Appele par ProcessTickGet (src/game/debug.cpp) entre chaque sous-systeme de la boucle de jeu : une surveillance qui
// change entre deux points designe le sous-systeme qui l'a modifiee ; avec « arret », le jeu s'arrete juste apres lui.
extern "C" void port_studio_point(const char* name)
{
    g_last_point = name;
    for (int i = 0; i < 8; ++i) {
        Watch& w = g_watch[i];
        if (!w.on) continue;
        unsigned char now[64];
        if (!mem_read(w.addr, now, w.len) || !std::memcmp(now, w.last, w.len)) continue;
        if (w.zero) {   // option zero : seulement quand la valeur devient entierement nulle
            bool z = true;
            for (unsigned k = 0; k < w.len; ++k) z = z && now[k] == 0;
            if (!z) { std::memcpy(w.last, now, w.len); continue; }
        }
        char a[130], b[130];
        hexs(a, w.last, w.len);
        hexs(b, now, w.len);
        port_studio_event("surveillance %d adresse=%lx apres=%s avant=%s maintenant=%s", i, w.addr, name, a, b);
        std::memcpy(w.last, now, w.len);
        ++w.hits;
        if (w.stop) {
            g_paused = 1; g_steps = 0;
            g_paused_at = name;
            while (g_paused && g_client >= 0) {   // arret au milieu de l'image, juste apres le sous-systeme fautif
                studio_poll();
                if (!g_paused || g_steps > 0) break;
                usleep(4000);
            }
            g_paused_at = "";
        }
    }
}
