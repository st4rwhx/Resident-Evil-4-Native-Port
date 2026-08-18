#pragma once
// Visite automatique des salles (20/10/2026) : OUTIL DE DEVELOPPEMENT, compile seulement avec -DNJ_DEVTOOLS (jamais dans une diffusion publique).
// Le moteur apprend le point d'entree de chaque salle pendant qu'on joue (tour_points.txt), puis, sur demande (fichier tour.go),
// saute de salle en salle, y reste N secondes (Leon ne meurt pas) et note chaque etape dans le journal de performance : on lit ensuite
// le tableau par salle avec tools/p3/journal_salles.py. Pilote : tools/tour/run_tour.py (adb ou dossier local).
namespace nj::runtime {
struct GuestCpu;
namespace input { struct PadState; }
}  // namespace nj::runtime

#include <string>
namespace nj::dev {
// Une fois par image du jeu (fil hote). Sans NJ_DEVTOOLS : ne fait rien.
void tour_tick();
// Enregistreur / rejoueur d'entrees (20/10, NJ_DEVTOOLS) : appele a CHAQUE lecture de la manette par le jeu (PADRead), sur le fil du jeu.
// Fichiers dans le dossier de donnees : rec.go -> enregistre l'etat de la manette a chaque lecture jusqu'a rec.stop (fichier pad_rec.bin) ;
// play.go -> rejoue pad_rec.bin depuis son debut (a lancer au meme endroit du jeu que l'enregistrement : meme sauvegarde, meme position),
// ecrit play.done a la fin ; le journal note la position de Leon au debut et a la fin, l'ecart mesure le determinisme du rejeu.
// Capture demandee par la visite (une par salle, a la mi-mesure) : la boucle hote la prend (main.cpp, maybe_screenshot), reduite a ~480 px, en BMP.
#if defined(NJ_DEVTOOLS)
bool shot_pending(std::string &path);   // vrai une seule fois par demande ; path : fichier a ecrire
#else
inline bool shot_pending(std::string &) { return false; }
#endif
// Reglages en direct (20/10, NJ_DEVTOOLS) : fichier knobs.txt du dossier de donnees, une ligne « nom valeur », relu toutes les 0,4 s. Permet de changer le rendu
// sur une image figee (bisection des dessins : drawstart / drawstop ; nozclip ; texnearest ; logdraw N) sans relancer le jeu. Sans NJ_DEVTOOLS : toujours `def`.
#if defined(NJ_DEVTOOLS)
int knob(const char *name, int def);
#else
inline int knob(const char *, int def) { return def; }
#endif
#if defined(NJ_DEVTOOLS)
void pad_hook(nj::runtime::GuestCpu &ctx, nj::runtime::input::PadState &st);
#else
inline void pad_hook(nj::runtime::GuestCpu &, nj::runtime::input::PadState &) {}
#endif
}  // namespace nj::dev
