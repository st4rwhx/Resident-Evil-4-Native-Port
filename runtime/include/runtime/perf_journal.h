#pragma once
// Journal de performance chiffre (12/10) : ce que le build de diffusion ne dit pas (il n'ecrit aucun
// journal lisible). Par seconde : images, temps moyen et pire image, part du jeu, de la lecture du
// flux, du rendu et de la presentation, salle du jeu ; chaque image de plus de 100 ms a part
// (saccades) ; machine et reglages au debut et a chaque changement. Chiffre (ChaCha20, cle du
// projet), dans %LOCALAPPDATA%/Nightjar/journal/ (Android : dossier de l'application), 10 seances
// gardees. Lecture : tools/p3/journal_perf.py.
#include <atomic>
#include <cstdint>
#include <string>

namespace nj::perf {

// Debut de seance (fil hote, contexte GL courant) : machine, reglages.
void session_start(const char *gl_renderer);
// Une image du jeu rendue (fil hote) : temps en millisecondes.
void frame(float dt, float parse, float render, float swap, float game);
// Evenement ponctuel (installation P3, chargement...) : texte court.
void event(const char *what);
// Ecrit sur le disque ce qui reste en tampon (fermeture rapide de Windows).
void flush();

// Lectures du disque (hw_di) : temps passe et octets, cumules ; le journal en tire la part de chaque
// image et de chaque seconde (saccades de chargement).
extern std::atomic<uint64_t> g_disc_us, g_disc_bytes;
// Textures du cache retrouvees perimees par la verification du contenu (texture_cache_gl.cpp).
extern std::atomic<uint32_t> g_tex_stale;
// Voix audio (hle/ax.cpp) : voix en cours cumulees sur les blocs mixes, et nombre de blocs ; le journal note la moyenne par seconde (axv=),
// donc le silence d'une replique manquante (marchand, Hunnigan) se voit : axv retombe a 0 alors que le jeu en demande.
extern std::atomic<uint32_t> g_ax_voices, g_ax_blocks;
// 20/10 : images AX servies (tick de 5 ms du DMA audio, hw_dsp.cpp) et rustines des alarmes (os_dispatch.cpp) : le journal les note pendant un gel (axt=, unst=)
extern std::atomic<uint32_t> g_ax_ticks, g_unstick;
// Millisecondes depuis la derniere image rendue (0 : aucune image encore). Sert a reconnaitre une attente du jeu qui ne dessine plus (hw_dsp.cpp : AX accelere).
int64_t ms_since_frame();
// Incremente a chaque changement de salle (premier rXYZ.das lu) : le rendu s'en sert pour compiler tous les nuanceurs sans sauter de dessin pendant le chargement.
extern std::atomic<uint32_t> g_room_serial;
// Fichier lu sur le disque (hw_di) : une salle (st?/rXYZ.das) devient la salle courante du journal,
// la meme pour toutes les versions du jeu.
void note_file(const std::string &path);
// Salle courante (stage_no << 8 | room_no, 0 : inconnue). r100 = 0x100, r101 = 0x101.
uint32_t current_room();

}  // namespace nj::perf
