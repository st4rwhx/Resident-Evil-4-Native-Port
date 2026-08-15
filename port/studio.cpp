// -*- coding: utf-8 -*-
// Nightjar Studio, brique S1 (15/11, nj-work/docs/NIGHTJAR_STUDIO_NATIF_2026-11-15.md) : le jeu parle en donnees et se
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

#include "global.h"
#include "player.h"
#include "snd_drv.h"
#include "njlink.h"
#include <dolphin/pad.h>
#include "room_jmp.h"
#include "main.h"
#include "mes.h"

extern "C" void njstate_fill(NjState* out);   // port/njstate.cpp
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
int g_capture = 0;   // capture demandee (lue par port/njgfx a la fin de l'image)

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
                  "\"flux\":%s,\"evenements_en_attente\":%u}",
                  g_frame, (unsigned) G_ROOM_ID, (unsigned) st.mode, (unsigned) st.bits, DbMenuActiveCheck(),
                  pl ? pPL->pos.x : 0.f, pl ? pPL->pos.y : 0.f, pl ? pPL->pos.z : 0.f, pl ? pPL->rot.y : 0.f,
                  (int) (s16) pG->pl_life, (unsigned) pG->flags_60, (unsigned) pG->flags_170, (int) st.cursor,
                  (int) st.cursor_count, (int) st.choice_count, (int) st.choice_cursor, (unsigned) st.prompt,
                  st.title_menu_alpha, st.fade, (unsigned) st.qte, flux, g_ev_count);
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
        if (std::sscanf(l + 8, "%x %u", &a, &n) != 2 || n == 0 || n > 2048 || a < 0x80000000u || a + n > 0x82000000u || a + n < a) {
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
    if (!std::strcmp(l, "aide")) {
        send_line("{\"commandes\":[\"etat\",\"evenements\",\"outil <NOM>\",\"texte 0|1\",\"drapeau 60|64 <masque> 0|1\","
                  "\"pad <boutons> <sx> <sy> <cx> <cy> <images>\",\"salle <chapitre> <salle hex> [point]\",\"reglage <nom> <valeur>\",\"capture\",\"memoire <adresse> <octets>\"]}");
        return;
    }
    send_line("{\"erreur\":\"commande inconnue (aide)\"}");
}

}  // namespace

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

// Une fois par image, depuis la boucle principale (main.cpp)
extern "C" void port_studio_frame(void)
{
    ++g_frame;
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
