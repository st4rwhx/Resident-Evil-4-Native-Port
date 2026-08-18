// Enregistreur de flux GX au format .dff (celui du lecteur FIFO de Dolphin, version 6) : les octets bruts du flux
// principal, image par image, l'etat initial (BP, CP, XF), et la memoire du jeu (MEM1) telle que le moteur la
// lisait : integrale au depart, puis seulement les pages changees a chaque fin d'image (copie vers l'XFB).
// Le fichier se rejoue avec FifoReplay (et avec le lecteur de Dolphin, pour une reference).
//
// Limite connue : la memoire est relevee a la fin de l'analyse de chaque image, pas a chaque dessin. Un tampon que le
// jeu reecrit plusieurs fois DANS une image (habillage des personnages) apparait avec sa derniere valeur.
#pragma once
#include <cstddef>
#include <cstdint>

namespace nj::runtime::gx::rec {

// Demande : enregistrer `frames` images a partir de la prochaine copie vers l'XFB. Utilisable depuis n'importe
// quel fil (touche F11, appui a quatre doigts, banc d'essai).
void request(int frames);
bool armed();    // demande en attente du debut d'une image
bool active();   // enregistrement en cours

// Fil de lecture du FIFO, flux principal (pas les listes d'affichage), apres chaque commande complete.
// `xfb_copy` : la commande est la copie EFB -> XFB qui termine l'image.
void begin(const uint32_t bp[256], const uint32_t cp[256], const uint32_t xfmem[4096], const uint32_t xfregs[88]);
void on_command(const uint8_t *p, size_t n, bool xfb_copy);

// Copie volatile activee par l'analyseur (fifo_parser.cpp) a la position courante du flux : enregistree comme mise a jour
// memoire a cette position, exactement ce que le moteur lisait pour les dessins qui suivent.
void on_volatile(uint32_t phys_addr, const uint8_t *data, uint32_t len);

}  // namespace nj::runtime::gx::rec
