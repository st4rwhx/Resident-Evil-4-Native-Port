#pragma once
// Assemble les sources d'entree en quatre manettes GameCube. Le fil principal sonde les sources
// (update) ; le jeu lit l'etat publie depuis son propre fil (pad.cpp, hw_si.cpp).
#include <memory>
#include <mutex>
#include <vector>

#include "input/input_source.h"

namespace nj::runtime::input {

class InputHub {
 public:
  static InputHub &get();

  void plug_source(std::unique_ptr<PadSource> source);
  void update();
  PadState pad(int port);

 private:
  InputHub() = default;
  // L'etat publie n'est jamais construit en place : update() remplit un brouillon et le copie
  // d'un bloc sous le verrou. Sinon le fil du jeu tombait parfois sur l'instant ou tout valait
  // zero (la visee qui se coupe toute seule).
  std::mutex lock_;
  PadState published_[4];
  std::vector<std::unique_ptr<PadSource>> sources_;
};

}  // namespace nj::runtime::input
