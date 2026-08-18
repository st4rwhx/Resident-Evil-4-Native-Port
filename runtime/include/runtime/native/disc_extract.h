#pragma once
// Extraction du disque du joueur vers le dossier de donnees du noyau natif.
#include <functional>
#include <string>

namespace nj::native {

// Ecrit <dest>/disc<N>/{sys,files,ready.properties}. `progress` recoit 0..100. Faux avec la raison.
bool extract_disc(const std::string &source, const std::string &dest, int disc_number,
                  const std::function<void(int)> &progress, std::string &error);
// Vrai si l'extraction du disque N est terminee.
bool disc_ready(const std::string &dest, int disc_number);

}  // namespace nj::native
