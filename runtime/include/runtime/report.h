#pragma once
// Rapport lisible pour l'onglet « Report » du menu F10 (13/10) : ce que le joueur colle dans le salon
// d'aide. Volontairement court et sans rien d'interne (pas de noms du moteur, pas de jetons, pas de
// chemins personnels) : machine, identifiant du build (empreinte de l'exe / de libmain.so), reglages,
// evenements de la seance (connexion, disque, preparation, sauvegardes), fin anormale de la seance
// precedente (plantage.txt, module+decalage). Le detail chiffre reste dans le journal de performance
// (runtime/perf_journal.h), joint au rapport.
#include <string>
#include <vector>

namespace nj::report {

// Evenement de la seance, une ligne (horodatee). Sans danger depuis n'importe quel fil.
void add(const std::string &line);
// Carte graphique (fil hote, contexte GL courant), une fois.
void set_gpu(const char *renderer, const char *version);

struct Text {
    std::string full;     // tout le rapport
    std::string brief;    // < 1900 caracteres (un message Discord)
    std::string crash;    // une ligne si la seance precedente s'est mal terminee, sinon vide
};
// settings : lignes des reglages (fournies par le menu).
Text build(const std::vector<std::string> &settings);

// Fichiers a joindre : journaux chiffres des deux dernieres seances, plantage.txt.
std::vector<std::string> attachments();
// Dossier a ouvrir (PC) : celui des journaux.
std::string folder();

}  // namespace nj::report
