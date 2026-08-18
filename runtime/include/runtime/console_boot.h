#pragma once
// Demarrage de la console : ce que le BS2 de la GameCube laisse en memoire avant de sauter dans le
// programme du disque (YAGCD, « Boot Process » et « Low Memory »), pour RE4.
#include "boot/program.h"

namespace nj::runtime {

struct GuestCpu;

// Copie le programme en memoire invitee (et remet ses zones bss a zero), declare son code comme
// recompile, monte le disque vu par le jeu (`game_dir` ou l'image de la configuration), puis
// prepare la memoire basse : en-tete du disque, FST en haut de la MEM1, BI2, taille memoire,
// bornes de l'arene, region video, horloges ; pile initiale et frequence de la base de temps.
void boot_console(GuestCpu &ctx, const boot::Program &exe, const std::string &game_dir);

}  // namespace nj::runtime
