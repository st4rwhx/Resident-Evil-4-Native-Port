#pragma once
// Police d'interface (menu F10, lanceur), 19/10/2026 : la police Latin du systeme (Segoe UI, Roboto) avec les glyphes de la langue
// active (Latin etendu pour le polonais et le turc, cyrillique pour le russe), plus la police CJK du systeme pour le japonais, le
// coreen et le chinois. Aucune police n'est livree ; sans police CJK sur l'appareil, ces trois langues retombent sur l'anglais.
struct ImFont;
struct ImGuiIO;

namespace nj::runtime::ui {

// Ajoute (et rend) la police de `latin_path` a la taille `px` avec les glyphes de la langue active. nullptr si le fichier manque.
ImFont *add_font(ImGuiIO &io, const char *latin_path, float px);
// La langue active a-t-elle sa police sur cet appareil ? Sinon i18n::init est refait en anglais (a appeler apres init, avant les polices).
bool language_font_available();

}  // namespace nj::runtime::ui
