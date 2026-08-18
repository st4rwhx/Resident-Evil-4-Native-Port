#pragma once
// Reglages du joueur (config.toml a cote de l'exe, ecrit aussi par le menu F10 et le lanceur) et
// etat de lancement (disque choisi). Une seule instance, lue au demarrage.
#include <cstdint>
#include <string>

namespace nj::runtime {

// Le moteur ne fait tourner que la GameCube ; l'enumeration reste pour les quelques branches du
// code qui distinguent encore l'adressage Wii.
enum class Platform { GameCube, Wii };
enum class Backend { OpenGL };

// Particularites d'un jeu (toutes a zero pour RE4 : valeurs par defaut du runtime).
struct TitleProfile {
  uint32_t callback_stack_top = 0;
  uint32_t ext_interrupt_dispatch = 0;
  uint32_t sched_lock_offset = 0;
};

class Config {
 public:
  static Config &get();
  void load(const std::string &path);

  Platform platform = Platform::GameCube;
  Backend backend{Backend::OpenGL};
  TitleProfile game_profile;

  // affichage
  bool vsync_on = true;
  int window_width = 1280;   // 16:9 (touch::wide_rect)
  int window_height = 720;
  bool fullscreen = false;   // plein ecran a la taille du bureau ; la fenetre suit sinon

  // disque
  std::string game_dir;      // disque extrait (developpement)
  std::string disc_image;    // image choisie dans le lanceur, prioritaire
  std::string disc2_image;   // second disque ([disc] second, NJ_DISC2)
  std::string ui_language = "auto";   // langue de l'interface ([ui] language) : auto (appareil) ou un code (fr, pt, es...)
  std::string game_id = "G4BE08";

  // commandes
  int input_mode = 6;        // 2 manette, 3-4 manette + pointeur, 6 clavier-souris, 7 tactile
  bool mouse_turn = false;   // la souris fait tourner Leon hors visee
  float mouse_sensitivity = 1.0f;

  // visee Wii ([wii_aiming], reprise de wii_aiming.ini du mod UHD)
  float wii_sensitivity = 1.0f;    // vitesse du curseur
  bool wii_edge_turn = false;      // ce qui depasse le bord tourne Leon (coupe : sans les reglages
                                   // camera de re4_tweaks, cela donnait un tour sur soi-meme)
  int wii_turn_sensitivity = 100;  // % de cette rotation
  int wii_edge_zone = 0;           // % de l'ecran ou Leon tourne seul
  int wii_edge_zone_speed = 100;   // % de la vitesse de cette rotation
  int wii_friction = 35;           // % de ralentissement du curseur sur un ennemi
  bool wii_hit_marker = true;      // le curseur pulse et la touche s'affiche
  bool wii_keep_point = true;      // a la mise en joue, la visee glisse vers le point vise

 private:
  Config() = default;
};

}  // namespace nj::runtime
