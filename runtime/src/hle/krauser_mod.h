#pragma once
// Mod « Krauser jouable » : le Krauser joueur (pl0a) avec les coups du Krauser boss (em39).
// Plan et recherches : docs/MOD_KRAUSER_2026-10-19.md.
#include "runtime/cpu_context.h"

namespace nj::krauser {

// En tete de gameInit, quand la partie demarre en Krauser : tampon des animations du boss dans le
// tas du jeu (le meme que Game.pBuf), rempli depuis em/em39.drs du disque du joueur.
void game_init(nj::runtime::GuestCpu& cpu);

// En tete de gameRoomInit : coups du boss actifs ou non pour cette salle (Krauser en histoire).
void room_init(nj::runtime::GuestCpu& cpu, bool on);

}  // namespace nj::krauser

// Enveloppes ([hle_wraps]) : cPlayer::move (0x800579B4) lit les commandes et lance un coup ;
// pl_R1_Aux (0x80059D10, routine 0/0xA) joue le coup en cours a la place de pAuxFunc.
void nj_kr_PlayerMove(nj::runtime::GuestCpu& cpu, void (*orig)(nj::runtime::GuestCpu&));
void nj_kr_PlAux(nj::runtime::GuestCpu& cpu, void (*orig)(nj::runtime::GuestCpu&));
// cEmMgr::construct (0x800ED258) : au moment de creer le joueur, remplace les pieces du modele de pl0a
// par celles du boss em39 dans l'archive du joueur (modele, mains, forme mutante, couteau).
void nj_kr_EmConstruct(nj::runtime::GuestCpu& cpu, void (*orig)(nj::runtime::GuestCpu&));
// cPlayer::setDamage (0x80173A98) : annule les coups de face pendant la garde de la forme mutante.
void nj_kr_PlDamage(nj::runtime::GuestCpu& cpu, void (*orig)(nj::runtime::GuestCpu&));
// Event::ExeBeginEvt (0x80137520) : enregistre les modeles de Leon pour les cinematiques de Krauser.
void nj_kr_BeginEvt(nj::runtime::GuestCpu& cpu, void (*orig)(nj::runtime::GuestCpu&));
