#pragma once
// Habillage du menu Nightjar a partir du disque du joueur (26/10/2026, docs/MENUS_OFFICIELS_RE4_ANALYSE.md).
//
// Rien n'est livre : au premier lancement avec un disque, un fil de fond lit ss/Eng/ss_cmmn.dat, ss_file.dat, ss_item.dat et ss_cap.dat,
// en tire quelques textures (cloison, cadre, ligne de liste, papier, rack, pictos), les range dans le cache local (jamais ailleurs, jamais
// dans un rapport) et le menu les utilise. Sans disque ou sans le fichier : piece() renvoie nullptr et le menu garde son habillage dessine.
#include <functional>
#include <string>
#include <vector>

#include "input/icon_render.h"

namespace nj::ui_skin {

enum Piece { Bg, Frame, Row, Head, Paper, Rack, GlyphStick, GlyphLR, GlyphOrb, FontAtlas, Scroll, GlyphA, GlyphR, GlyphB, GlyphY, GlyphStart, kPieceCount };

// Pour les essais : lit les .dat par `read` (chemin du disque, par ex. « ss/Eng/xxx.dat » ou « font/sscrn_p.fnt » -> octets) au lieu du disque ; nombre de pieces trouvees.
using Reader = std::function<bool(const std::string &, std::vector<uint8_t> &)>;
int build_from(const Reader &read);

// A appeler quand le menu s'ouvre (peu couteux) : charge le cache ou lance la generation en fond.
void poll();
bool ready();
// Image de la piece, ou nullptr si pas (encore) disponible.
const nj::icons::Image *piece(int p);

}  // namespace nj::ui_skin
