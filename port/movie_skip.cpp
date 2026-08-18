// -*- coding: utf-8 -*-
// Films Sofdec : PROVISOIRE (phase N1 du plan natif, documentation interne).
//
// Le lecteur de films CRI (src/lib/sfd_*, mpv_*, dct_*...) contient des boucles de decodage en
// assembleur PowerPC : il sera rendu natif en phase N4. D'ici la, un film se termine des son
// lancement, comme si le joueur l'avait passe : le jeu enchaine sur la suite.
//
// Sans cela, mwPlyCreateSofdec (souche) rendait NULL, cSofdec::startApp appelait sa fonction
// d'erreur - une boucle infinie que clang a le droit de supprimer en C++ - et le code tombait
// dans le vide (SIGILL au film d'ouverture).
//
// Ces definitions sont fortes : elles remplacent les souches faibles de stubs_link.cpp.
#include <dolphin.h>
#include <cstdio>
#include <cstring>

#include "mwply.h"
#include "port_log.h"

namespace {

void nop(MWPLY) {}
u32 one(MWPLY) { return 1; }
void* none(MWPLY, void*) { return nullptr; }
void start(MWPLY, const char* fname) { OSReport("[PORT] film passe (lecture native en phase N4) : %s\n", fname); }
int stat_end(MWPLY) { return MWE_PLY_STAT_PLAYEND; }
void time0(MWPLY, int* ncount, int* tscale)
{
    if (ncount) *ncount = 0;
    if (tscale) *tscale = 1;
}
void pause(MWPLY, int) {}

MWPLY_IF kIf = {none, one, one, nop, nop, nop, start, nop, stat_end, time0, pause};
MWPLY_OBJ g_player = {&kIf};

}  // namespace

extern "C" {

void mwPlyInitSfdFx(MWS_PLY_INIT_SFD*) {}
int mwPlyCalcWorkCprmSfd(MWS_PLY_CPRM_SFD*) { return 64; }
MWPLY mwPlyCreateSofdec(MWS_PLY_CPRM_SFD*) { return &g_player; }
void mwPlyGetHdrInf(void*, int, MWS_SFD_HDRINF* info)
{
    if (info) {
        std::memset(info, 0, sizeof(*info));
        info->width = 512;
        info->height = 448;
    }
}
void mwPlyGetCurFrm(MWPLY, MWS_FRM* frm)
{
    if (frm) std::memset(frm, 0, sizeof(*frm));   // aucune image : bufadr NULL
}
void mwPlyRelCurFrm(MWPLY) {}
int mwPlyGetNumSkipDec(MWPLY) { return 0; }
int mwPlyGetNumSkipDisp(MWPLY) { return 0; }

}
