#include "disc.h"

#include "runtime/disc_image.h"
#include "runtime/game_version.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

// Empreinte du main.dol traduit dans cet executable (njrecomp, fichier principal genere).
extern "C" const unsigned long long g_nj_dol_fnv;

namespace nj::launcher {

namespace {

uint32_t be32(const uint8_t *p) { return (uint32_t(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

uint64_t fnv1a(const std::vector<uint8_t> &d) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (uint8_t c : d) h = (h ^ c) * 0x100000001B3ull;
    return h;
}

bool write_file(const std::filesystem::path &p, const void *data, size_t n) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    return o && o.write(static_cast<const char *>(data), (std::streamsize)n);
}

bool is_re4(const uint8_t *boot) {
    return std::memcmp(boot, "G4BE08", 6) == 0 || std::memcmp(boot, "G4BP08", 6) == 0;
}

// En-tete commun aux deux disques : image lisible, GameCube, Resident Evil 4.
DiscCheck read_header(nj::runtime::DiscImage *f, uint8_t boot[0x440]) {
    if (!f) return DiscCheck::Unreadable;
    if (!f->read(0, boot, 0x440)) return DiscCheck::Unreadable;
    if (be32(&boot[0x1C]) != 0xC2339F3Du && be32(&boot[0x1C]) != 0)  // mot magique GameCube
        return DiscCheck::NotGameCube;
    if (!is_re4(boot)) return DiscCheck::WrongGame;
    return DiscCheck::Ok;
}

}  // namespace

DiscCheck check_and_prepare(const std::string &image, const std::string &game_dir, DiscInfo *info) {
    auto f = nj::runtime::DiscImage::open(image);
    uint8_t boot[0x440];
    if (DiscCheck c = read_header(f.get(), boot); c != DiscCheck::Ok) return c;
    if (boot[6] != 0) return DiscCheck::WrongDiscNumber;   // disque 2 donne a la place du 1

    const uint32_t dol_off = be32(&boot[0x420]);
    uint8_t hdr[0x100];
    if (!dol_off || !f->read(dol_off, hdr, sizeof(hdr))) return DiscCheck::Unreadable;
    uint32_t dol_size = 0x100;
    for (int i = 0; i < 18; ++i) {
        const uint32_t o = be32(&hdr[i * 4]), s = be32(&hdr[0x90 + i * 4]);
        if (o && s && o + s > dol_size) dol_size = o + s;
    }
    if (dol_size > 16u * 1024 * 1024) return DiscCheck::WrongBuild;
    std::vector<uint8_t> dol(dol_size);
    if (!f->read(dol_off, dol.data(), dol.size())) return DiscCheck::Unreadable;
    // debug (Nov 25 2004), USA ou Europe : tout autre DOL est refuse
    const auto version = nj::runtime::detect_game_version(dol.data(), dol.size());
    if (version == nj::runtime::GameVersion::Unknown) return DiscCheck::WrongBuild;
    // ... et parmi elles, celle dont cet executable contient le code traduit ; 0 = le moteur traduit
    // lui-meme le disque du joueur (P3) : toute version reconnue convient
    if (g_nj_dol_fnv && g_nj_dol_fnv != fnv1a(dol)) return DiscCheck::OtherRegion;
    if (info) {
        info->id.assign(reinterpret_cast<const char *>(boot), 6);
        info->version = version;
    }

    std::vector<uint8_t> bi2(0x2000);
    if (!f->read(0x440, bi2.data(), bi2.size())) return DiscCheck::Unreadable;

    const std::filesystem::path sys = std::filesystem::path(game_dir) / "sys";
    if (!write_file(sys / "main.dol", dol.data(), dol.size()) ||
        !write_file(sys / "boot.bin", boot, sizeof(boot)) ||
        !write_file(sys / "bi2.bin", bi2.data(), bi2.size()))
        return DiscCheck::Unreadable;
    return DiscCheck::Ok;
}

DiscCheck check_second(const std::string &image, const DiscInfo &first) {
    auto f = nj::runtime::DiscImage::open(image);
    uint8_t boot[0x440];
    if (DiscCheck c = read_header(f.get(), boot); c != DiscCheck::Ok) return c;
    if (std::memcmp(boot, first.id.data(), 6) != 0) return DiscCheck::OtherRegion;
    if (boot[6] != 1) return DiscCheck::WrongDiscNumber;
    return DiscCheck::Ok;
}

std::string find_second_beside(const std::string &first_image, const DiscInfo &first) {
    namespace fs = ::std::filesystem;
    std::error_code ec;
    const fs::path dir = fs::u8path(first_image).parent_path();
    if (dir.empty() || !fs::is_directory(dir, ec)) return {};
    for (const auto &e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file(ec)) continue;
        std::string ext = e.path().extension().string();
        for (char &c : ext) c = (char)std::tolower((unsigned char)c);
        if (ext != ".iso" && ext != ".gcm" && ext != ".rvz") continue;
        const auto u8 = e.path().u8string();
        const std::string p(u8.begin(), u8.end());
        if (check_second(p, first) == DiscCheck::Ok) return p;
    }
    return {};
}

const char *describe(DiscCheck c) {
    switch (c) {
    case DiscCheck::Ok: return "Disc verified.";
    case DiscCheck::Unreadable: return "This file can't be read. Please select a disc image (.iso, .gcm or .rvz).";
    case DiscCheck::NotGameCube: return "This isn't a GameCube disc image.";
    case DiscCheck::WrongGame: return "This isn't Resident Evil 4 for GameCube.";
    case DiscCheck::WrongBuild:
        return "This Resident Evil 4 disc isn't supported (modified or unknown version). "
               "Please use an unmodified copy of your USA or European disc.";
    case DiscCheck::OtherRegion:
        return "This disc is from another region than this download. "
               "Please get the download that matches your disc (USA or Europe).";
    case DiscCheck::WrongDiscNumber: return "Wrong disc number: please select the disc asked above.";
    }
    return "";
}

const char *region_name(const DiscInfo &d) {
    switch (d.version) {
    case nj::runtime::GameVersion::Usa: return "USA";
    case nj::runtime::GameVersion::Europe: return "Europe";
    case nj::runtime::GameVersion::Debug: return "Debug";
    default: return "";
    }
}

}  // namespace nj::launcher
