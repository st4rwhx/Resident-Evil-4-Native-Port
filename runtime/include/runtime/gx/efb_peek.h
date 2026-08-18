#pragma once
// Lectures de l'EFB par le processeur (GXPeekZ / GXPeekARGB : adresses physiques 0x08xxxxxx).
//
// RE4 s'en sert pour savoir si une source de lumiere est cachee par le decor : lens flare
// (espgen01), halos (esp0e), flou radial des lumieres (esp45), traineees (esp09). Il appelle
// GXDrawDone, puis lit la profondeur en 12 points autour de la source.
//
// Notre rendu OpenGL d'une image n'a lieu qu'apres que le jeu l'a finie : au moment de la
// lecture, l'image en cours n'est pas encore dessinee. On repond donc avec une image de retard :
// chaque lecture est reperee par son rang (nombre de GXDrawDone depuis la derniere copie vers
// l'ecran) et ses coordonnees ; le rendu de l'image suivante, arrive au meme rang, lit ces
// points dans son tampon de profondeur ; le jeu recoit la valeur mesuree la fois d'avant.
// Pour un test d'occultation, une image de retard ne se voit pas.
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace nj::runtime::gx::peek {

// Cote jeu (MMU) : lecture 32 bits a l'adresse physique paddr (0x08000000..0x08FFFFFF).
uint32_t read(uint32_t paddr);

// Cote analyse du flux : un GXDrawDone (BP 0x45 = 2), une copie vers l'ecran (BP 0x52, bit 14).
void parse_finish();
void parse_xfb_copy();

// Cote rendu, dans le meme ordre : read_depth(x, y) rend la profondeur GL [0, 1] au pixel
// d'EFB (x, y) (origine en haut a gauche, 640 x 480).
void render_finish(const std::function<float(int, int)>& read_depth);
// Variante sans attente (rendu OpenGL de bureau) : render_points avance le rang comme
// render_finish et rend les points demandes a ce rang (faux s'il n'y en a pas) ; la profondeur,
// lue plus tard dans un tampon GPU, est deposee par render_store (memes points, meme ordre).
bool render_points(int& rank, std::vector<std::pair<int, int>>& pts);
void render_store(int rank, const std::vector<std::pair<int, int>>& pts, const std::vector<float>& depth);
void render_xfb_copy();
// Mesure par point en fin d'image (defaut, 01/10) : tous les points demandes pendant l'image, lus
// sur la scene terminee juste avant la copie vers l'image affichee ; render_store(-1, ...) les
// depose, et read() repond par point quel que soit le rang. NJ_PEEKRANK=1 : ancien appariement
// par rang de GXDrawDone.
bool rank_mode();
bool frame_points(std::vector<std::pair<int, int>>& pts);

}  // namespace nj::runtime::gx::peek
