#pragma once
// Moteur natif : le noyau du jeu (bibliotheques libre4_*) charge dans le processus, et ce que
// Nightjar lui demande chaque image : manette, son, cadence, images graphiques.
//
// Les noms de l'interface (re4_core_*) sont ceux du noyau ; tout le reste est a nous.
#include <cstdint>
#include <string>

#include "runtime/native/njlink_proto.h"

namespace nj::native {

// Une image graphique du noyau, telle qu'elle est publiee (ordre des champs de l'interface).
struct RawChunk {
  uint32_t version, sequence, flags;
  const uint32_t *ops;
  uint32_t op_words;
  const uint8_t *vertices;
  uint32_t vertex_bytes;
  const uint16_t *indices;
  uint32_t index_count;
  const uint8_t *blob;
  uint32_t blob_bytes;
  uint32_t xfb_address, xfb_width, xfb_height;
  uint32_t draws, vertex_count, texture_bytes;
};

// Etat de la manette pour le noyau (boutons du SDK GameCube, sticks, gachettes).
struct Pad {
  uint16_t buttons;
  int8_t stick_x, stick_y, substick_x, substick_y;
  uint8_t trigger_l, trigger_r;
  uint16_t accel_cg;   // commandes Wii (17/11) : acceleration totale de la « Wiimote » en centiemes de g (0 = aucune)
};

enum class State { Idle = 0, Running = 1, Ended = 2, Failed = 3 };

// Charge les bibliotheques du noyau (dossier `lib_dir`) ; faux avec la raison dans `error`.
bool load(const std::string &lib_dir, std::string &error);
// Disque extrait (dossier avec disc0/{sys,files}) et dossier des modules, puis demarre le jeu.
bool start(const std::string &data_dir, const std::string &module_dir, std::string &error);
State state();
std::string message();

void pad(const Pad &p);
// Image du coeur (notre moteur natif) : une image neuve est prete ; la copier dans le tampon lie
// (rectangle x, y, w, h de celui-ci). Rend faux sans image neuve.
bool frame_ready();
// Attend un message du coeur (image, etat) au plus timeout_ms ; rend true si un message est arrive.
bool wait_message(int timeout_ms);
bool frame_blit(int x, int y, int w, int h);
unsigned rumble();
// Son : `frames` images stereo 32 kHz ; rend le nombre ecrit.
int audio(int16_t *out, int frames);
void vsync(int64_t ns);
void pause(bool on);

// Images graphiques : la plus ancienne finie, ou nul ; a rendre avec release().
const RawChunk *gx_acquire();
void gx_release(const RawChunk *chunk);
void gx_detach();

// Instantanes d'etat du jeu (pour le tactile adaptatif et la camera)
struct CoreSnapshot {
  uint32_t apiVersion;
  uint32_t serial;
  uint32_t frame;
  uint8_t rno[4];
  uint16_t room;
  int8_t disc;
  uint8_t cameraValid;
  float cameraEye[3];
  float cameraTarget[3];
  float cameraRoll;
  float cameraFovy;
  uint16_t nModels;
  uint16_t nParts;
  uint32_t droppedModels;
  uint32_t screen;
};
const CoreSnapshot *snapshot_acquire();
void snapshot_release(const CoreSnapshot *snapshot);

bool loaded();

// Etat du jeu envoye par le coeur a chaque image (port/njstate.cpp) : faux tant qu'aucun n'est arrive.
bool game_state(NjState &out);
// Zone de l'EFB que le jeu copie dans son XFB (512 x 448 en jeu), envoyee par le coeur avec chaque image.
bool xfb_size(unsigned &w, unsigned &h);
// Mallette au doigt : op 1 curseur (int32 i, j), 2 piece en main (f32 x, y) ; board 0 mallette, 1 zone verte.
void case_command(int op, int board, const void *data, unsigned size);
// Menu titre de Nightjar (18/11) : commande au titre (1 continuer, 2 nouvelle partie, 3 ecran de chargement du jeu,
// 4 charger la sauvegarde `slot`, 5 options du jeu) ; dossier des sauvegardes du coeur (bh4_dataNN.bin).
void title_command(int cmd, int slot);
std::string saves_dir();
// Reglage applique en direct par le coeur (NJ_MSG_SETTING) : 1 = tenue de Leon (-1 automatique, 0 veste, 1 chemise,
// 2 gilet, 3 RPD).
void setting(int id, int value);

}  // namespace nj::native
