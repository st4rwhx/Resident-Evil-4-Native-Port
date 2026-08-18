// Profileur par echantillonnage des fils de Nightjar sur Android (NJ_APROF=1).
//
// Pas de simpleperf sur le Galaxy S5 (Android 9) : un fil interrompt environ mille fois par
// seconde chacun des fils inscrits (signal SIGPROF) et note l'adresse ou il s'execute. Toutes les
// 10 s, pour chaque fil, les adresses les plus frequentes sont ecrites dans le journal ([APROF]),
// en decalage dans libmain.so (a traduire sur PC : tools/re4/aprof.py) ou par bibliotheque
// (pilote GL, libc...). Sur les autres plateformes, tout est vide.
#pragma once

namespace nj::runtime::njprof {

// A appeler au debut de chaque fil a mesurer (nom court : "hote", "jeu", "decodage"...).
void register_thread(const char *name);
// Demarre l'echantillonneur si NJ_APROF est defini (une seule fois).
void start();

}  // namespace nj::runtime::njprof
