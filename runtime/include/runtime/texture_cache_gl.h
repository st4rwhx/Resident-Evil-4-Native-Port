#pragma once
#include <string>

#include "common/gl.h"
#include "runtime/cpu_context.h"
#include "runtime/gx_state.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <cstdlib>

namespace nj::runtime::hle {

// Identite d'une texture du cache : ou elle est, comment la lire, et l'empreinte de son contenu
// (texels et palette) au moment du chargement.
struct TexKey {
    uint32_t addr, width, height, format;
    uint32_t tlut_addr, tlut_fmt;
    uint32_t content_hash;
    uint32_t levels;   // niveaux de mip envoyes (1 = base seule)
    bool operator==(const TexKey&) const = default;
};

struct TexKeyHasher {
    std::size_t operator()(const TexKey& k) const {
        const uint32_t f[] = {k.addr, k.width, k.height, k.format, k.tlut_addr, k.tlut_fmt, k.content_hash, k.levels};
        uint64_t h = 0xcbf29ce484222325ull;
        for (uint32_t v : f) h = (h ^ v) * 0x100000001b3ull;
        return (std::size_t)h;
    }
};

class TexCache {
public:
    static TexCache& get();

    // Texture GL d'une etape de texture ; mip_levels : niveaux de la chaine du jeu (ranges apres la
    // base) a decoder et envoyer.
    GLuint get_texture(GuestCpu& ctx, const gx::TexUnit& stage,
                       uint32_t mip_levels = 1);
    // Niveaux envoyes pour une texture du cache ; 1 pour les autres (copies de l'EFB, sans mips).
    uint32_t levels_of(GLuint tex) const;
    // Texture remplacee par sa version HD (textures_hd/) : mips completes, LOD du jeu ignores.
    bool is_hd(GLuint tex) const { return hd_textures.count(tex) != 0; }
    // Avance a chaque destruction de textures : GL redonne les noms liberes, tout etat garde ailleurs
    // par nom de texture est alors a jeter.
    uint64_t generation() const { return m_generation; }

    // Copies de l'EFB vers une texture : le rendu range la texture GL de la copie sous l'adresse visee
    // par le jeu ; une etape de texture lue a cette adresse prend la copie (la memoire du jeu n'a
    // jamais ete ecrite : la decoder montrerait n'importe quoi).
    // w, h : taille GameCube de la copie ; gw, gh : taille reelle de la texture GL (x S en copies
    // haute resolution). GL ES 3.0 n'a pas glGetTexLevelParameteriv : on la retient ici.
    void register_efb_texture(uint32_t addr, GLuint tex, uint32_t w, uint32_t h, uint32_t gw,
                              uint32_t gh);
    // Copie encore valable a cette adresse, sinon 0 : si le jeu a recrit la memoire depuis la
    // copie (une texture de la salle suivante chargee a la meme adresse), la copie est perimee.
    GLuint find_efb_texture(uint32_t addr);
    // Texture GL attribuee aux copies de cette adresse, perimee ou non (chemin de copie : on
    // reutilise son stockage).
    GLuint efb_slot(uint32_t addr) const;
    // Taille de la copie en texels GameCube (sans le facteur de resolution interne). Une texture
    // declaree d'une autre taille lit la memoire texel pour texel (meme largeur : meme ligne), pas
    // l'image etiree : le lac copie 256 x 196 et l'echantillonne en 256 x 168.
    bool efb_dims(uint32_t addr, uint32_t& w, uint32_t& h);
    // Copie EFB lue comme texture a palette (C4 / C8) : la copie relue, chaque indice passe
    // par la TLUT courante (masque de profondeur de champ de RE4, filter02). Mise en cache
    // jusqu'a la copie suivante ou un changement de palette.
    GLuint efb_paletted(GLuint efb, const gx::TexUnit& stage);

    void clear();

    // Moteur natif : l'adaptateur d'images signale chaque ecriture dans la memoire des textures
    // (adresse, octets). Des le premier signalement, l'empreinte d'echantillon d'une texture n'est
    // recalculee que si ses pages ont ete recrites depuis (sample_hash).
    void guest_texels_written(uint32_t addr, uint32_t len);

private:
    TexCache() = default;   // une seule instance : get()

    struct SampleMemo { uint32_t hash = 0; uint64_t seq = 0; };
    std::unordered_map<uint64_t, SampleMemo> sample_memo;   // (adresse << 32 | octets) -> empreinte
    std::vector<uint64_t> page_seq;                         // par page de 64 Ko : rang de la derniere ecriture signalee
    uint64_t write_seq = 0;                                 // 0 : aucune ecriture signalee, pas de reprise
    uint32_t sample_hash(uint32_t addr, const uint8_t* p, uint32_t size);

    struct CachedTexture {
        GLuint tex;
        uint64_t last_use;     // use_tick du dernier usage
        std::string hd_path;   // non vide : version GameCube provisoire, PNG HD en decodage
        uint32_t hd_gen;       // HdAsync::gen au dernier essai de remplacement
        uint64_t full = 0;     // empreinte de tous les octets au moment de la creation (verification)
        uint32_t bytes = 0;    // octets de GPU (tous les niveaux, RGBA8) : le cache est borne en octets (26/10)
    };
    void promote_hd(CachedTexture& e);
    using Cache = std::unordered_map<TexKey, CachedTexture, TexKeyHasher>;
    Cache cache;
    std::unordered_map<uint32_t, GLuint> efb_texs;   // adresse physique -> texture de la copie
    std::unordered_map<uint32_t, uint64_t> efb_sizes;
    // adresse -> empreinte de la memoire invitee au moment de la copie ; absente = copie perimee
    std::unordered_map<uint32_t, uint64_t> efb_ram_sig;
    std::unordered_map<uint32_t, uint32_t> efb_ram_len;
    std::unordered_map<GLuint, uint64_t> efb_gl_sizes;  // texture -> (largeur << 32) | hauteur   // adresse -> (largeur << 32) | hauteur
    std::unordered_map<GLuint, uint32_t> efb_copy_gen;   // nombre de copies ecrites dans la texture
    struct PalettedEfb { GLuint tex = 0; uint32_t gen = ~0u; uint32_t key = 0; };
    std::unordered_map<GLuint, PalettedEfb> efb_paletted_cache;
    std::unordered_map<GLuint, uint32_t> tex_levels;
    std::unordered_set<GLuint> hd_textures;
    uint64_t cache_bytes = 0;   // somme des CachedTexture::bytes
    uint64_t use_tick = 0;
    uint64_t evicted = 0;
    uint64_t m_generation = 0;

    // Retire a peu pres le quart le moins recemment utilise du cache.
    void evict_old();
    Cache::iterator drop(Cache::iterator it);

};

} 
