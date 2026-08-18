#pragma once
// Habillage anime du lanceur (26/10/2026). Le lanceur s'affiche AVANT que le disque soit choisi : il ne peut donc rien emprunter au jeu.
// Tout est dessine par nous (ImDrawList) : degrade sombre et vignette, halo qui respire derriere le titre, fondu a l'ouverture,
// boutons a curseur rouge comme le menu. Sobre a dessein (pas de particules). Aucune texture, aucun fichier.
#include "imgui.h"

namespace nj::launcher::fx {

// Fond plein ecran (a appeler juste apres Begin, avant les widgets) : degrade sombre et vignette.
void background(ImDrawList *dl, ImVec2 size, float t);
// Halo rouge qui respire derriere le titre.
void title_glow(ImDrawList *dl, ImVec2 center, float radius, float t);
// Fondu depuis le noir a l'ouverture (a appeler en dernier : recouvre tout) ; renvoie vrai tant qu'il court.
bool fade_in(ImDrawList *dl, ImVec2 size, float t, float duration = 0.9f);
// Bouton du lanceur : panneau sombre, filet, surbrillance rouge et petit triangle qui glisse au survol. `accent` = couleur du filet.
bool button(const char *label, ImU32 accent, float width, float height);

}  // namespace nj::launcher::fx
