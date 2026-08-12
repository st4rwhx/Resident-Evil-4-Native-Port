// -*- coding: utf-8 -*-
// Rendu natif, cote jeu (phase N3 du plan natif, nj-work/docs/PLAN_NATIF_NIGHTJAR_2026-11-11.md).
//
// La bibliotheque GX du SDK (src/lib/GX*.c, compilee telle quelle) calcule les registres du GPU et les
// ecrit dans le tuyau. Ici le tuyau est un tampon d'octets, ranges dans l'ordre du materiel
// (gros-boutiste), que le rendu Nightjar lit dans le meme processus (port/njgfx/njgfx.cpp) :
//   - registres BP/XF/CP et sommets immediats -> tampon de l'image en cours ;
//   - liste d'affichage en cours d'enregistrement (GXBeginDisplayList) -> la memoire de la liste ;
//   - registres materiels (CP/PE/PI/MEM) -> tableaux du portage ;
//   - fin de dessin et jetons (BP 0x45 / 0x48) -> « interruptions » delivrees a OSDisableInterrupts,
//     apres que le rendu a lu et copie tout ce qui precede (le jeu peut alors reutiliser sa memoire).
#include <cstdlib>
#include <cstring>

#include <dolphin.h>
#include <dolphin/os/OSInterrupt.h>

#include "njgfx/njgfx_bridge.h"

namespace {

// flux de l'image en cours (tampon simple : la bibliotheque standard C++ n'est pas disponible avec les en-tetes du jeu)
struct Bytes {
    unsigned char* p = nullptr;
    size_t n = 0, cap = 0;
    void push(unsigned char b)
    {
        if (n == cap) {
            cap = cap ? cap * 2 : (1u << 20);
            p = (unsigned char*) std::realloc(p, cap);
        }
        p[n++] = b;
    }
} g_fifo;
unsigned char* g_list = nullptr;     // liste en cours d'enregistrement
u32 g_list_size, g_list_used;
bool g_list_over;

inline void put(unsigned char b)
{
    if (g_list) {
        if (g_list_used < g_list_size) g_list[g_list_used++] = b;
        else g_list_over = true;
    } else {
        g_fifo.push(b);
    }
}
inline void put16(u16 v) { put((unsigned char) (v >> 8)); put((unsigned char) v); }
inline void put32(u32 v) { put16((u16) (v >> 16)); put16((u16) v); }

u32 g_pi[0x40];
u16 g_cp[0x80], g_pe[0x80], g_mem[0x80];

__OSInterruptHandler g_handlers[64];
bool g_pend_finish, g_delivering;
u16 g_pend_tokens[64];
int g_ntokens;
OSContext g_irq_context;

}  // namespace

extern "C" {

void port_wg_push_u8(u8 v) { put(v); }
void port_wg_push_u16(u16 v) { put16(v); }
void port_wg_push_u32(u32 v) { put32(v); }
void port_wg_push_s8(s8 v) { put((unsigned char) v); }
void port_wg_push_s16(s16 v) { put16((u16) v); }
void port_wg_push_s32(s32 v) { put32((u32) v); }
void port_wg_push_f32(f32 v)
{
    u32 u;
    std::memcpy(&u, &v, 4);
    put32(u);
}

// Registre BP ecrit par le SDK (GX_WRITE_RAS_REG, __gx.h) : fin de dessin et jetons.
void port_gx_ras(u32 value)
{
    if (g_list)
        return;   // dans une liste : l'evenement viendra quand elle sera jouee (lecteur de flux)
    const u32 reg = value >> 24;
    if (reg == 0x45 && (value & 0xF) == 2)
        g_pend_finish = true;
    else if (reg == 0x48)
    {
        if (g_ntokens < 64) g_pend_tokens[g_ntokens++] = (u16) value;
    }
}

void port_wg_begin_list(void* list, u32 size)
{
    g_list = (unsigned char*) list;
    g_list_size = size;
    g_list_used = 0;
    g_list_over = false;
}

u32 port_wg_end_list(int* overflow)
{
    const u32 n = g_list_used;
    if (overflow) *overflow = g_list_over ? 1 : 0;
    g_list = nullptr;
    return n;
}

void* port_gx_reg_base(int unit)
{
    switch (unit) {
    case 0: return g_pi;
    case 1: return g_cp;
    case 2: return g_pe;
    default: return g_mem;
    }
}

__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler)
{
    if (interrupt < 0 || interrupt >= 64) return nullptr;
    __OSInterruptHandler old = g_handlers[interrupt];
    g_handlers[interrupt] = handler;
    return old;
}

__OSInterruptHandler __OSGetInterruptHandler(__OSInterrupt interrupt)
{
    return (interrupt >= 0 && interrupt < 64) ? g_handlers[interrupt] : nullptr;
}

// Appelee par OSDisableInterrupts (os.cpp) : le rendu lit et copie tout ce qui a ete envoye (le jeu
// pourra reutiliser sa memoire), puis la fin de dessin et les jetons sont signales au SDK.
void port_gx_deliver(void)
{
    if (g_delivering || (!g_pend_finish && g_ntokens == 0))
        return;
    g_delivering = true;
    njgfx_sync(g_fifo.p, (unsigned) g_fifo.n);
    g_fifo.n = 0;
    for (int i = 0; i < g_ntokens; ++i) {
        g_pe[7] = g_pend_tokens[i];   // GXTokenInterruptHandler lit le jeton ici
        if (g_handlers[0x12]) g_handlers[0x12](0x12, &g_irq_context);
    }
    g_ntokens = 0;
    if (g_pend_finish) {
        g_pend_finish = false;
        if (g_handlers[0x13]) g_handlers[0x13](0x13, &g_irq_context);
    }
    g_delivering = false;
}

// Fin d'une image (VIFlush, vi.cpp) : le rendu dessine tout le flux et envoie l'image a l'appli.
void port_gx_frame(void)
{
    njgfx_frame(g_fifo.p, (unsigned) g_fifo.n);
    g_fifo.n = 0;
}

// --- sommets immediats (GXVert.h, fonctions externes en DEBUG) : dans le tuyau, comme le materiel
#define P1(name, T, put_) void name##1##T(T x) { put_(x); }
#define P2(name, T, put_) void name##2##T(T x, T y) { put_(x); put_(y); }
#define P3(name, T, put_) void name##3##T(T x, T y, T z) { put_(x); put_(y); put_(z); }
#define P4(name, T, put_) void name##4##T(T x, T y, T z, T w) { put_(x); put_(y); put_(z); put_(w); }
P1(GXCmd, u8, port_wg_push_u8) P1(GXCmd, u16, port_wg_push_u16) P1(GXCmd, u32, port_wg_push_u32)
P1(GXParam, u8, port_wg_push_u8) P1(GXParam, u16, port_wg_push_u16) P1(GXParam, u32, port_wg_push_u32)
P1(GXParam, s8, port_wg_push_s8) P1(GXParam, s16, port_wg_push_s16) P1(GXParam, s32, port_wg_push_s32)
P1(GXParam, f32, port_wg_push_f32) P3(GXParam, f32, port_wg_push_f32) P4(GXParam, f32, port_wg_push_f32)
P3(GXPosition, f32, port_wg_push_f32) P3(GXPosition, u8, port_wg_push_u8) P3(GXPosition, s8, port_wg_push_s8)
P3(GXPosition, u16, port_wg_push_u16) P3(GXPosition, s16, port_wg_push_s16)
P2(GXPosition, f32, port_wg_push_f32) P2(GXPosition, u8, port_wg_push_u8) P2(GXPosition, s8, port_wg_push_s8)
P2(GXPosition, u16, port_wg_push_u16) P2(GXPosition, s16, port_wg_push_s16)
void GXPosition1x16(u16 x) { port_wg_push_u16(x); }
void GXPosition1x8(u8 x) { port_wg_push_u8(x); }
P3(GXNormal, f32, port_wg_push_f32) P3(GXNormal, s16, port_wg_push_s16) P3(GXNormal, s8, port_wg_push_s8)
void GXNormal1x16(u16 x) { port_wg_push_u16(x); }
void GXNormal1x8(u8 x) { port_wg_push_u8(x); }
P4(GXColor, u8, port_wg_push_u8) P1(GXColor, u32, port_wg_push_u32) P3(GXColor, u8, port_wg_push_u8)
P1(GXColor, u16, port_wg_push_u16)
void GXColor1x16(u16 x) { port_wg_push_u16(x); }
void GXColor1x8(u8 x) { port_wg_push_u8(x); }
P2(GXTexCoord, f32, port_wg_push_f32) P2(GXTexCoord, s16, port_wg_push_s16) P2(GXTexCoord, u16, port_wg_push_u16)
P2(GXTexCoord, s8, port_wg_push_s8) P2(GXTexCoord, u8, port_wg_push_u8)
P1(GXTexCoord, f32, port_wg_push_f32) P1(GXTexCoord, s16, port_wg_push_s16) P1(GXTexCoord, u16, port_wg_push_u16)
P1(GXTexCoord, s8, port_wg_push_s8) P1(GXTexCoord, u8, port_wg_push_u8)
void GXTexCoord1x16(u16 x) { port_wg_push_u16(x); }
void GXTexCoord1x8(u8 x) { port_wg_push_u8(x); }
P1(GXMatrixIndex, u8, port_wg_push_u8)

}
