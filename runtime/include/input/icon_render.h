#pragma once
// Rendu des icones d'armes du bouton de visee, a partir des modeles d'objet du disque du joueur (ss/Item/idmXXX.bin + .tpl), 19/10/2026.
//
// Les icones ne sont plus livrees (docs/ICONES_GENEREES_CHEZ_LE_JOUEUR.md) : le moteur les rend au premier lancement avec le disque du
// joueur et les garde dans son cache local. Ce fichier ne connait que des octets : il lit le TPL (formats GX) et le BIN d'objet, rend
// le modele avec un petit rasteriseur logiciel (profil de l'arme, canon a gauche, ombrage simple), puis le met dans une case ronde
// de bouton : rogne, mis a l'echelle pour tenir dans le cercle, cerne de blanc, comme les autres icones.
#include <cstdint>
#include <vector>

namespace nj::icons {

struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;   // alpha droit (non premultiplie)
};

// Identifiants d'objet (wep_info de item.cpp) dont on genere l'icone, dans l'ordre de l'atlas.
const std::vector<int>& weapon_item_ids();

// TPL (magic 0x0020AF30) -> images RGBA ; faux si illisible.
bool decode_tpl(const std::vector<uint8_t>& tpl, std::vector<Image>& out);

// Icone de l'objet `item` (bin et tpl du disque) dans une case cell x cell. Vue : plat (0x29, 0x2A, 0x2B), de face (grenades, oeufs, TMP...),
// sinon de profil. Faux si le modele est vide.
bool render_item_icon(const std::vector<uint8_t>& bin, const std::vector<uint8_t>& tpl, int item, int cell, Image& out);
// Nombre de sommets d'un modele (-1 : illisible) ; couteau rendu depuis l'archive d'arme (lame a gauche).
int model_vertex_count(const std::vector<uint8_t>& bin);
bool render_knife_icon(const std::vector<uint8_t>& bin, const std::vector<uint8_t>& tpl, int cell, Image& out);
// Mallette S / M / L / LL (ss/Eng/ss_pzzl.dat, entrees 418 a 421, texture 417), vue de trois quarts : `yaw` et `pitch` en degres.
// Modele d'une archive d'arme (em/wepNN.drs) sous un angle donne, avec miroir horizontal facultatif (lame a gauche) : banc des angles.
bool render_archive_icon(const std::vector<uint8_t>& bin, const std::vector<uint8_t>& tpl, int cell, float yaw, float pitch, float roll,
                         bool mirror, Image& out);
bool render_case_icon(const std::vector<uint8_t>& bin, const std::vector<uint8_t>& tpl, int cell, float yaw, float pitch, Image& out);

}  // namespace nj::icons
