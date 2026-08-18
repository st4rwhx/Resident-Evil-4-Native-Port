#pragma once
#include <cstdint>

// Visee au pointeur de la Wii Edition, portee a la souris (PC).
//
// Ce que fait la Wii (research/PASSATION_VISEE_2026-09-22.md, DOL Wii) : le pointeur infrarouge
// est ecrit dans joy->sx/sy, les octets du stick, apres multiplication par 127 (0x80129F68), avec
// une zone morte KPAD de 0,05 / 0,95. La routine de visee (0x8013AF94, la meme que sur GameCube)
// integre une vitesse : le pointeur est un stick virtuel dont l'inclinaison est l'ecart au centre
// de l'ecran, sur une plage de +-127 au lieu des +-72 du stick GameCube, sans octogone.
//
// Ici : en visee, la souris deplace un pointeur a l'ecran (dessine par l'incrustation), son ecart
// au centre devient le stick. Le disque etant celui du GameCube, PADClamp bornerait le stick a 72
// dans un octogone : pendant la visee on ouvre sa table (ClampRegion, 0x8024DC30) a 127 sans
// octogone, et on la rend a la sortie. NJ_AIM_WII=0 garde l'ancienne visee souris.

namespace nj::runtime::input {

struct WiiPointer {
    bool on = false;
    float x = 0.0f, y = 0.0f;  // -1..1, centre de l'image, y vers le bas
};

bool wii_aim_enabled();
// Visee PC de l'UHD (F10 « Aiming » = PC) : deplacement de la souris en joue, aux unites de
// bio4.exe (0xC63008 / 0xC6300C), cumule par la source souris et consomme par PlWepLockCtrl.
bool pc_aim_enabled();
void pc_mouse_add(float dx, float dy);
void pc_mouse_take(float& dx, float& dy);
void wii_pointer_set(bool on, float x, float y);
WiiPointer wii_pointer_get();
// L'arme en main a-t-elle la visee au pointeur (table Wii 0x802BAF90) ? src/hle/wii_aim.cpp
bool wii_pointer_weapon(int wep_no);

// Reprise de la visee de RE4 UHD (mod_voiture/uhd/visee_wii_uhd.cpp) :
// - ce que la souris pousse au-dela du bord (unites du pointeur, -1..1 = largeur / hauteur de
//   l'image) : cumule par la source souris, consomme par la pose (wii_aim.cpp), qui en fait une
//   rotation de Leon ou un tangage a la vitesse de la main ;
// - un ennemi sous le curseur (wep.target, image precedente) : le curseur y ralentit (accroche).
void wii_pointer_add_overflow(float ox, float oy);
void wii_pointer_take_overflow(float& ox, float& oy);
void wii_pointer_set_target(bool on_target);
bool wii_pointer_target();
// Derniere touche d'un ennemi (ms, horloge steady), pour le marqueur ; 0 : aucune.
void wii_pointer_set_hit(uint32_t ms);
uint32_t wii_pointer_hit();
uint32_t wii_now_ms();

} // namespace nj::runtime::input
