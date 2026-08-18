// P3 : en-tete commun a tout le code traduit chez le joueur (precompile au build, embarque chiffre).
// Identique pour toutes les versions du jeu : le preambule de functions.h (natives du SDK et
// enveloppes du moteur) plus ce que chaque fichier traduit inclut. Les lots ne contiennent ensuite
// que leurs propres declarations (p3_host.cpp).
#pragma once
#include "runtime/cpu_context.h"
#include "runtime/rel_aot.h"
#include "runtime/hw/hw.h"

extern "C" {
void DCFlushRangeNoSync(nj::runtime::GuestCpu& ctx);
void DCStoreRangeNoSync(nj::runtime::GuestCpu& ctx);
void GXLoadNrmMtxImm(nj::runtime::GuestCpu& ctx);
void GXLoadPosMtxImm(nj::runtime::GuestCpu& ctx);
void GXSetAlphaCompare(nj::runtime::GuestCpu& ctx);
void GXSetDstAlpha(nj::runtime::GuestCpu& ctx);
void GXSetScissor(nj::runtime::GuestCpu& ctx);
void GXSetZMode(nj::runtime::GuestCpu& ctx);
void OSReport(nj::runtime::GuestCpu& ctx);
void PADInit(nj::runtime::GuestCpu& ctx);
void PADRead(nj::runtime::GuestCpu& ctx);
void VIWaitForRetrace(nj::runtime::GuestCpu& ctx);
}
void nj_case_MemCreateHeap(nj::runtime::GuestCpu& ctx, void (*orig)(nj::runtime::GuestCpu&));
void nj_char_gameRoomInit(nj::runtime::GuestCpu& ctx, void (*orig)(nj::runtime::GuestCpu&));
void nj_dvd_CompareDiskID(nj::runtime::GuestCpu& ctx, void (*orig)(nj::runtime::GuestCpu&));
void nj_wii_PlWepHitCheck2(nj::runtime::GuestCpu& ctx, void (*orig)(nj::runtime::GuestCpu&));
void nj_wii_PlWepLockCtrl(nj::runtime::GuestCpu& ctx, void (*orig)(nj::runtime::GuestCpu&));
void nj_wii_drawLaserSight(nj::runtime::GuestCpu& ctx, void (*orig)(nj::runtime::GuestCpu&));
void nj_wii_getMarkerPos(nj::runtime::GuestCpu& ctx, void (*orig)(nj::runtime::GuestCpu&));
void nj_wii_getPitch(nj::runtime::GuestCpu& ctx, void (*orig)(nj::runtime::GuestCpu&));
void nj_kr_PlayerMove(nj::runtime::GuestCpu& ctx, void (*orig)(nj::runtime::GuestCpu&));
void nj_kr_PlAux(nj::runtime::GuestCpu& ctx, void (*orig)(nj::runtime::GuestCpu&));

#include <atomic>
#include <cmath>
#include "nj_ops.h"
