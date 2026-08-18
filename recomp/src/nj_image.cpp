#include "nj_image.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include "runtime/obf.h"
#include "runtime/diag.h"
using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire

namespace nj {

namespace {

uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
uint16_t be16(const uint8_t* p) { return uint16_t((p[0] << 8) | p[1]); }
void put32(uint8_t* p, uint32_t v) {
    p[0] = uint8_t(v >> 24); p[1] = uint8_t(v >> 16); p[2] = uint8_t(v >> 8); p[3] = uint8_t(v);
}
void put16(uint8_t* p, uint16_t v) { p[0] = uint8_t(v >> 8); p[1] = uint8_t(v); }

bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

// Types de relocation (src/lib/OSLink.c).
enum : uint8_t {
    R_PPC_NONE = 0, R_PPC_ADDR32 = 1, R_PPC_ADDR24 = 2, R_PPC_ADDR16 = 3, R_PPC_ADDR16_LO = 4,
    R_PPC_ADDR16_HI = 5, R_PPC_ADDR16_HA = 6, R_PPC_ADDR14 = 7, R_PPC_ADDR14_BRTAKEN = 8,
    R_PPC_ADDR14_BRNTAKEN = 9, R_PPC_REL24 = 10, R_PPC_REL14 = 11, R_PPC_REL14_BRTAKEN = 12,
    R_PPC_REL14_BRNTAKEN = 13,
    R_DOLPHIN_NOP = 201, R_DOLPHIN_SECTION = 202, R_DOLPHIN_END = 203, R_DOLPHIN_MRKREF = 204,
};

}  // namespace

bool Image::load_dol(const std::string& path) {
    std::vector<uint8_t> d;
    if (!read_file(path, d)) {
        std::cerr << "[image] DOL illisible : "_g << path << "\n"_g;
        return false;
    }
    return load_dol_bytes(std::move(d));
}

// main.dol deja en memoire (P3 : lu dans le disque du joueur par le lanceur).
bool Image::load_dol_bytes(std::vector<uint8_t> d) {
    if (d.size() < 0x100) {
        std::cerr << "[image] DOL trop court\n"_g;
        return false;
    }
    auto sec = [&](int i, bool code) {
        const uint32_t off = be32(&d[0x00 + 4 * i]);
        const uint32_t addr = be32(&d[0x48 + 4 * i]);
        const uint32_t size = be32(&d[0x90 + 4 * i]);
        if (!off || !size) return;
        if (off + size > d.size()) {
            std::cerr << "[image] section DOL hors du fichier\n"_g;
            return;
        }
        Segment s;
        s.addr = addr;
        s.size = size;
        s.code = code;
        s.bytes.assign(d.begin() + off, d.begin() + off + size);
        segments.push_back(std::move(s));
    };
    for (int i = 0; i < 7; ++i) sec(i, true);
    for (int i = 7; i < 18; ++i) sec(i, false);
    Segment bss;
    bss.addr = be32(&d[0xD8]);
    bss.size = be32(&d[0xDC]);
    if (bss.size) segments.push_back(std::move(bss));
    entry = be32(&d[0xE0]);
    return true;
}

bool Image::declare_rel(const std::string& path, uint32_t base, uint32_t bss) {
    std::vector<uint8_t> image;
    if (!read_file(path, image)) {
        std::cerr << "[image] REL illisible : "_g << path << "\n"_g;
        return false;
    }
    return declare_rel_bytes(path, std::move(image), base, bss);
}

// Module REL deja en memoire (P3) ; `name` sert aux messages.
bool Image::declare_rel_bytes(const std::string& name, std::vector<uint8_t> image, uint32_t base, uint32_t bss) {
    const std::string& path = name;
    RelModule m;
    m.path = name;
    m.base = base;
    m.bss = bss;
    m.image = std::move(image);
    if (m.image.size() < 0x40) {
        std::cerr << "[image] REL trop court : "_g << path << "\n"_g;
        return false;
    }
    const uint8_t* h = m.image.data();
    m.id = be32(h + 0x00);
    const uint32_t nsec = be32(h + 0x0C);
    const uint32_t sec_off = be32(h + 0x10);
    m.bss_size = be32(h + 0x20);
    m.image_size = uint32_t(m.image.size());
    if (sec_off + nsec * 8 > m.image.size()) {
        std::cerr << "[image] table de sections hors du fichier : "_g << path << "\n"_g;
        return false;
    }
    m.sec_addr.assign(nsec, 0);
    for (uint32_t i = 0; i < nsec; ++i) {
        const uint32_t off = be32(h + sec_off + 8 * i) & ~3u;
        const uint32_t size = be32(h + sec_off + 8 * i + 4);
        if (off)
            m.sec_addr[i] = base + off;
        else if (size)
            m.sec_addr[i] = bss;     // section sans contenu dans le fichier : le bss
    }
    modules.push_back(std::move(m));
    return true;
}

const RelModule* Image::module_by_id(uint32_t id) const {
    for (const auto& m : modules)
        if (m.id == id) return &m;
    return nullptr;
}

bool Image::relocate(RelModule& m) {
    uint8_t* img = m.image.data();
    const uint32_t imp_off = be32(img + 0x28);
    const uint32_t imp_size = be32(img + 0x2C);
    if (imp_off + imp_size > m.image.size()) return false;
    const uint32_t nsec = uint32_t(m.sec_addr.size());
    const uint32_t sec_tab = be32(img + 0x10);

    for (uint32_t i = 0; i + 8 <= imp_size; i += 8) {
        const uint32_t target_id = be32(img + imp_off + i);
        uint32_t p = be32(img + imp_off + i + 4);
        const RelModule* target = target_id ? module_by_id(target_id) : nullptr;
        if (target_id && !target) continue;   // module absent de cette image
        uint32_t cur_sec = 0;       // section courante du module relogé
        uint32_t cur_off = 0;       // decalage dans le fichier de cette section
        uint32_t where = 0;         // position courante dans la section
        while (p + 8 <= m.image.size()) {
            const uint16_t adv = be16(img + p);
            const uint8_t type = img[p + 2];
            const uint8_t section = img[p + 3];
            const uint32_t addend = be32(img + p + 4);
            p += 8;
            where += adv;
            if (type == R_DOLPHIN_END) break;
            if (type == R_DOLPHIN_NOP || type == R_DOLPHIN_MRKREF || type == R_PPC_NONE) continue;
            if (type == R_DOLPHIN_SECTION) {
                cur_sec = section;
                cur_off = cur_sec < nsec ? (be32(img + sec_tab + 8 * cur_sec) & ~3u) : 0;
                where = 0;
                continue;
            }
            // S : adresse de la section visee dans le module cible (0 pour le DOL)
            uint32_t S = 0;
            bool target_bss = false;
            if (target) {
                if (section >= target->sec_addr.size()) continue;
                S = target->sec_addr[section];
                target_bss = (S == target->bss && target->bss_size);
            }
            const uint32_t value = S + addend;
            if (!cur_off) continue;                       // section sans contenu (bss) : rien a patcher
            const uint32_t file_pos = cur_off + where;
            if (file_pos + 4 > m.image.size() && type != R_PPC_ADDR16_LO && type != R_PPC_ADDR16_HI &&
                type != R_PPC_ADDR16_HA && type != R_PPC_ADDR16)
                continue;
            uint8_t* at = img + file_pos;
            const uint32_t P = m.base + file_pos;         // adresse AOT de l'emplacement patche
            const bool code_sec = cur_sec < nsec && (be32(img + sec_tab + 8 * cur_sec) & 1u);
            switch (type) {
            case R_PPC_ADDR32: put32(at, value); break;
            case R_PPC_ADDR24: put32(at, (be32(at) & ~0x03FFFFFCu) | (value & 0x03FFFFFCu)); break;
            case R_PPC_ADDR16: put16(at, uint16_t(value)); break;
            case R_PPC_ADDR16_LO: put16(at, uint16_t(value & 0xFFFF)); break;
            case R_PPC_ADDR16_HI:
            case R_PPC_ADDR16_HA: {
                uint16_t hi = uint16_t(value >> 16);
                if (type == R_PPC_ADDR16_HA && (value & 0x8000)) ++hi;
                put16(at, hi);
                // l'immediat porte la moitie haute d'une adresse de module : a corriger au chargement
                if (target && code_sec) fixups[P & ~3u] = Fixup{target->id, target_bss};
                break;
            }
            case R_PPC_ADDR14:
            case R_PPC_ADDR14_BRTAKEN:
            case R_PPC_ADDR14_BRNTAKEN:
                put32(at, (be32(at) & ~0xFFFCu) | (value & 0xFFFCu));
                break;
            case R_PPC_REL24: {
                const int64_t delta = int64_t(value) - int64_t(P);
                if (delta >= -0x2000000 && delta < 0x2000000) {
                    put32(at, (be32(at) & ~0x03FFFFFCu) | (uint32_t(delta) & 0x03FFFFFCu));
                } else {
                    // hors de portee depuis la fenetre AOT (appel vers le DOL) : cible absolue notee.
                    // Le mot garde le deplacement tronque, comme l'ecrirait OSLink.
                    branch_targets[P] = value;
                    put32(at, (be32(at) & ~0x03FFFFFCu) | (uint32_t(delta) & 0x03FFFFFCu));
                }
                break;
            }
            case R_PPC_REL14:
            case R_PPC_REL14_BRTAKEN:
            case R_PPC_REL14_BRNTAKEN:
                put32(at, (be32(at) & ~0xFFFCu) | ((value - P) & 0xFFFCu));
                break;
            default:
                std::cerr << "[image] relocation de type "_g << int(type) << " ignoree dans "_g << m.path << "\n"_g;
                break;
            }
        }
    }
    return true;
}

bool Image::load_rels() {
    for (auto& m : modules)
        if (!relocate(m)) {
            std::cerr << "[image] relocation impossible : "_g << m.path << "\n"_g;
            return false;
        }
    // segments : code et donnees de chaque module, a leurs adresses AOT
    for (const auto& m : modules) {
        const uint8_t* h = m.image.data();
        const uint32_t sec_tab = be32(h + 0x10);
        for (uint32_t i = 0; i < m.sec_addr.size(); ++i) {
            const uint32_t raw = be32(h + sec_tab + 8 * i);
            const uint32_t off = raw & ~3u;
            const uint32_t size = be32(h + sec_tab + 8 * i + 4);
            if (!off || !size || off + size > m.image.size()) continue;
            Segment s;
            s.addr = m.base + off;
            s.size = size;
            s.code = raw & 1u;
            s.module = int(m.id);
            s.bytes.assign(m.image.begin() + off, m.image.begin() + off + size);
            segments.push_back(std::move(s));
        }
    }
    std::sort(segments.begin(), segments.end(),
              [](const Segment& a, const Segment& b) { return a.addr < b.addr; });
    return true;
}

const Segment* Image::segment_at(uint32_t addr) const {
    for (const auto& s : segments)
        if (addr >= s.addr && addr - s.addr < s.size) return &s;
    return nullptr;
}

uint32_t Image::word(uint32_t addr) const {
    const Segment* s = segment_at(addr);
    if (!s || s->bytes.empty() || addr - s->addr + 4 > s->bytes.size()) return 0;
    return be32(&s->bytes[addr - s->addr]);
}

bool Image::is_code(uint32_t addr) const {
    const Segment* s = segment_at(addr);
    return s && s->code;
}

}  // namespace nj
