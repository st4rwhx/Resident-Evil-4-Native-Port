#pragma once
// Le disque que voit le jeu : le disque 1 du joueur (image .iso/.gcm/.rvz ou dossier extrait),
// avec le disque 2 fusionne dedans (ses fichiers absents du disque 1 sont ajoutes a la FST et
// places au-dela de la fin du disque 1 : le jeu ne demande jamais l'echange), des fichiers
// remplaces par des fichiers hote (modeles de la mallette de l'UHD, NJ_FILE_OVERRIDE) et des
// correctifs en memoire de meme longueur (debug/config.txt du build debug). Le disque du joueur
// n'est jamais modifie.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "runtime/disc_image.h"

namespace nj::runtime {

class DiscView {
public:
    static DiscView& get();

    // `game_dir` sert quand aucune image n'est choisie (Config::disc_image vide) : dossier extrait.
    bool init(const std::string &root);
    bool valid() const { return m_ok || !m_folder.empty(); }
    // Jeu natif (18/11) : le disque est un dossier extrait (disc1/files) ; read_file y lit les fichiers (icones
    // d'armes generees chez le joueur, logo...). Sans image disque montee seulement.
    void set_folder(const std::string &files_dir) { m_folder = files_dir; }

    size_t read(uint64_t offset, uint32_t len, uint8_t *out);
    // Chemin (minuscules, sous files/) du fichier qui contient cette position ; "" hors fichiers.
    std::string file_at(uint64_t offset);
    // Fichier entier du disque vu par le jeu (chemin de la FST, ex. "rel/st1_0.rel" ; casse ignoree).
    bool read_file(const std::string& path, std::vector<uint8_t>& out);
    std::string m_folder;

    // Le disque 2 est monte (cDvd::DiscChange n'a rien a demander : nj_dvd_CompareDiskID).
    bool has_second() const { return m_has_second; }

    uint32_t fst_size() const { return (uint32_t)m_fst.size(); }
    const std::vector<uint8_t>& fst_data() const { return m_fst; }
    const std::vector<uint8_t>& boot_data() const { return m_boot; }

private:
    DiscView() = default;

    // Plage du disque servie par une autre source que le disque 1.
    struct Piece {
        uint64_t offset, size;
        DiscImage* image;     // disque 2 (position src) ...
        uint64_t src;
        std::string host;     // ... ou fichier hote (UTF-8), lu depuis son debut
    };
    struct Patch {
        uint64_t offset;
        std::vector<std::uint8_t> data;
    };

    bool mount_second(const std::string& path);
    void apply_overrides();
    void apply_byte_mods();   // <mod>/octets.txt (Studio du Nightjar Engine)
    void release_config();
    bool find_file(const std::string& lower_path, uint64_t& off, uint32_t& len) const;
    size_t read_raw(uint64_t offset, uint32_t len, uint8_t* dst);
    void set_fst_entry(uint32_t index, uint64_t offset, uint32_t size);

    static constexpr uint64_t kSecondBase = 0x60000000ull;     // fichiers du disque 2
    static constexpr uint64_t kOverrideBase = 0xA0000000ull;   // remplacements plus grands

    bool m_ok = false, m_inited = false, m_has_second = false;
    std::unique_ptr<DiscImage> m_first, m_second;
    std::vector<uint8_t> m_boot, m_fst;
    std::vector<Piece> m_pieces;   // tries par position
    std::vector<Patch> m_patches;
    uint64_t m_override_next = kOverrideBase;
};

}  // namespace nj::runtime
