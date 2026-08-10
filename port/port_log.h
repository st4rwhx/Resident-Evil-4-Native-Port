// Journal du portage : chaque fonction de plateforme encore absente se signale une seule fois.
// Le but n'est pas de tracer, c'est d'obtenir la **liste ordonnee** de ce que le jeu reclame
// vraiment au demarrage - qui est beaucoup plus courte que les 276 fonctions du SDK.
#pragma once
#include <cstdio>

void port_todo(const char *fn);
#define PORT_TODO(name) port_todo(name)
