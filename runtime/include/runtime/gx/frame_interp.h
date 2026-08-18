#pragma once
// Images intermediaires entre deux images du jeu (src/hle/gx/frame_interp.cpp).
#include <vector>
#include "runtime/gx_command.h"
#include "runtime/gx/frame_renderer.h"

namespace nj::runtime::gx {

// Vrai si le reglage de cadence demande plus que les 30 i/s du jeu.
bool interp_enabled();

// Une image du jeu vient d'etre decodee : elle devient B (la precedente devient A), `frame` est
// repris (vide au retour). Si l'image est fiable, le rendu dessine tout de suite l'image
// intermediaire (t = 0,5) et B suit une demi-periode plus tard ; sinon B est dessinee telle quelle.
// parse_s : ce qu'a coute l'analyse de cette image, pour juger du temps libre.
void interp_new_frame(std::vector<GxOp>& frame, FrameRenderer* r, double parse_s);

// Entre deux images du jeu : dessine B exacte quand son heure est venue. Rend faux s'il n'y
// avait rien a dessiner.
bool interp_tick(FrameRenderer* r);

// Banc de test (FifoReplay) : dessine B exacte tout de suite, sans attendre l'heure.
bool interp_test_final(FrameRenderer* r);

}  // namespace nj::runtime::gx
