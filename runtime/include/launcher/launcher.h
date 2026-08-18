#pragma once
// Lanceur : ce que voit le joueur avant que le jeu ne demarre, sur Windows comme sur Android.
//
//   1. version : la derniere Release publiee sur GitHub est lue a chaque lancement. Une version
//      depassee, ou une verification impossible, bloque tout : il ne reste que le lien vers la
//      derniere version et celui du Discord ;
//   2. accueil : invitation a rejoindre le Discord, puis « Next » ;
//   3. disques : le disque 1 (.iso, .gcm ou .rvz) de la version que cet executable contient (USA,
//      Europe ou debug, reconnue a l'empreinte de son main.dol), puis le disque 2 (cherche a cote
//      du disque 1, sinon demande). Les deux sont retenus pour les lancements suivants.
//
// Actif dans les builds de diffusion (NJ_RELEASE) ; ailleurs seulement avec NJ_LAUNCHER=1,
// pour que les scripts de test (autostart, bootdirect) gardent leur comportement.

#include <string>

struct SDL_Window;

namespace nj::launcher {

// Version de l'application : jamais affichee ni en clair dans le binaire (13/10, demande de
// l'utilisateur) ; reconstruite dans discord_auth.cpp, brouille par Polaris en diffusion.
// Pour changer de version : python tools/version_cachee.py <x.y.z> regenere les octets.
const char *app_version();
constexpr const char *kRepo = "st4rwhx/Resident-Evil-4-Native-Port";
// Pas de Release GitHub (distribution par le serveur Discord seulement, 12/10) : la mise a jour se
// telecharge sur le serveur.
constexpr const char *kLatestUrl = "https://discord.gg/AXAzExECSv";
constexpr const char *kDiscordUrl = "https://discord.gg/AXAzExECSv";

bool enabled();

// Deroule les trois ecrans dans la fenetre et le contexte GL deja crees. Rend true avec le
// repertoire de jeu prepare (sys/main.dol + sys/boot.bin extraits du disque) et les chemins des
// deux images disque ; false si le joueur ferme l'application.
bool run(SDL_Window *window, std::string &game_dir, std::string &disc_image, std::string &disc2_image);

}  // namespace nj::launcher
