// Journal du portage : chaque fonction de plateforme encore absente se signale une seule fois.
// Le but n'est pas de tracer, c'est d'obtenir la **liste ordonnee** de ce que le jeu reclame
// vraiment au demarrage - qui est beaucoup plus courte que les 276 fonctions du SDK.
#pragma once
#include <cstdio>

void port_todo(const char *fn);
// Un drapeau par site d'appel : la table de port_todo() se parcourt avec strcmp (20 % de l'ecran titre).
#define PORT_TODO(name) do { static bool port_todo_once_; if (!port_todo_once_) { port_todo_once_ = true; port_todo(name); } } while (0)
