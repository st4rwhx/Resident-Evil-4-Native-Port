#include <sstream>
#include <fstream>
#include "runtime/mods.h"
#include "runtime/virtual_disc.h"
#include "runtime/log.h"
#include "runtime/gfx_settings.h"
#include "runtime/cpu_context.h"
#include "runtime/env.h"
#include "runtime/config.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <system_error>

namespace nj::runtime::gfx { extern int g_debug_mode; }  // settings_menu.cpp
namespace fs = ::std::filesystem;

namespace nj::runtime {

namespace {

uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

void put32(std::vector<uint8_t>& v, size_t at, uint32_t x) {
    v[at] = uint8_t(x >> 24);
    v[at + 1] = uint8_t(x >> 16);
    v[at + 2] = uint8_t(x >> 8);
    v[at + 3] = uint8_t(x);
}

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// En-tete (0x440 octets) et FST d'un disque.
bool read_header(DiscImage& d, std::vector<uint8_t>& boot, std::vector<uint8_t>& fst) {
    boot.assign(0x440, 0);
    if (!d.read(0, boot.data(), boot.size())) return false;
    const uint64_t off = be32(&boot[0x424]);
    const uint32_t size = be32(&boot[0x428]);
    if (!off || !size || size > (16u << 20)) return false;
    fst.assign(size, 0);
    return d.read(off, fst.data(), size);
}

// Arbre de fichiers : FST du disque 1, puis ce que le disque 2 ajoute. Ordre d'insertion garde.
struct Node {
    std::string raw, low;
    bool dir = false;
    uint64_t off = 0;
    uint32_t size = 0;
    std::vector<Node> kids;
    Node* child(const std::string& l) {
        for (Node& k : kids)
            if (k.low == l) return &k;
        return nullptr;
    }
};

// Le dossier parent d'une entree (cree au besoin) ; `raw` : elements du chemin.
Node* parent_of(Node& root, const FstEntry& e) {
    Node* n = &root;
    for (size_t i = 0; i + 1 < e.raw.size(); ++i) {
        const std::string l = lower(e.raw[i]);
        Node* k = n->child(l);
        if (!k) {
            n->kids.push_back(Node{e.raw[i], l, true, 0, 0, {}});
            k = &n->kids.back();
        }
        n = k;
    }
    return n;
}

// Ecrit les entrees (12 octets) et la table des noms, dans l'ordre de l'arbre.
void serialize(const Node& n, uint32_t parent, uint32_t& index, std::vector<uint8_t>& entries,
               std::vector<uint8_t>& names) {
    for (const Node& k : n.kids) {
        const uint32_t me = index++;
        const uint32_t name_off = (uint32_t)names.size();
        names.insert(names.end(), k.raw.begin(), k.raw.end());
        names.push_back(0);
        const size_t at = entries.size();
        entries.resize(at + 12, 0);
        entries[at] = k.dir ? 1 : 0;
        entries[at + 1] = uint8_t(name_off >> 16);
        entries[at + 2] = uint8_t(name_off >> 8);
        entries[at + 3] = uint8_t(name_off);
        if (k.dir) {
            put32(entries, at + 4, parent);
            serialize(k, me, index, entries, names);
            put32(entries, at + 8, index);   // premier index apres le dossier
        } else {
            put32(entries, at + 4, (uint32_t)k.off);
            put32(entries, at + 8, k.size);
        }
    }
}

// Fichiers du disque servis depuis un fichier hote : fichiers des mods, modeles de la mallette de l'UHD (F10
// « Attache case models »), et NJ_FILE_OVERRIDE="chemin/disque=fichier hote|..." (chemins sous
// files/, casse ignoree).
std::vector<std::pair<std::string, std::string>> file_overrides() {
    std::vector<std::pair<std::string, std::string>> out;
    // fichiers des mods actives (files/ de chaque mod, runtime/mods.h), le plus prioritaire gagne
    const auto ix = nj::mods::index();
    out = ix->disc_files;
    if (gfx::g_uhd_case_models) {
        const auto m = ix->extra.find("models/ss_pzzl.dat");
        if (m != ix->extra.end())
            out.push_back({"ss/eng/ss_pzzl.dat", m->second});
    }
    const char* env = NJ_ENV("NJ_FILE_OVERRIDE");
    if (!env) return out;
    const std::string all(env);
    for (size_t start = 0; start <= all.size();) {
        size_t end = all.find('|', start);
        if (end == std::string::npos) end = all.size();
        const std::string item = all.substr(start, end - start);
        const size_t eq = item.find('=');
        if (eq != std::string::npos && eq > 0) {
            std::string disc = lower(item.substr(0, eq));
            std::replace(disc.begin(), disc.end(), '\\', '/');
            out.push_back({disc, item.substr(eq + 1)});
        }
        start = end + 1;
    }
    return out;
}

}  // namespace

DiscView& DiscView::get() {
    static DiscView instance;
    return instance;
}

bool DiscView::init(const std::string& game_dir) {
    if (m_inited) return m_ok;
    m_inited = true;
    const std::string& image = Config::get().disc_image;
    const std::string& source = image.empty() ? game_dir : image;
    m_first = DiscImage::open(source);
    if (!m_first || !read_header(*m_first, m_boot, m_fst)) {
        NJ_LOG << "[VDisc] disque illisible : " << source << std::endl;
        return false;
    }
    m_ok = true;
    NJ_LOG << "[VDisc] disque 1 (" << m_first->format() << ") : FST " << m_fst.size() << " octets" << std::endl;

    const std::string& second = Config::get().disc2_image;
    if (second.empty())
        NJ_LOG << "[VDisc] pas de second disque ([disc] second / NJ_DISC2)" << std::endl;
    else if (!mount_second(second))
        NJ_LOG << "[VDisc] second disque refuse : " << second << std::endl;
    apply_overrides();
    apply_byte_mods();
    release_config();
    std::sort(m_pieces.begin(), m_pieces.end(), [](const Piece& a, const Piece& b) { return a.offset < b.offset; });
    return true;
}

// RE4 tient sur deux disques ; le second n'apporte que les fichiers des chapitres 3 a 5 (St3/, Evd/,
// Movie/, modules st3_*.rel...), le reste est identique. On sert une seule FST : celle du disque 1,
// plus les fichiers que seul le disque 2 possede, places a partir de kSecondBase.
bool DiscView::mount_second(const std::string& path) {
    m_second = DiscImage::open(path);
    std::vector<uint8_t> boot2, fst2;
    if (!m_second || !read_header(*m_second, boot2, fst2)) return false;
    if (std::memcmp(boot2.data(), m_boot.data(), 6) != 0) {
        NJ_LOG << "[VDisc] second disque : autre jeu (" << std::string(boot2.begin(), boot2.begin() + 6) << ")\n";
        return false;
    }
    Node root;
    root.dir = true;
    walk_fst(m_fst, [&](const FstEntry& e) {
        Node* p = parent_of(root, e);
        const std::string l = lower(e.raw.back());
        if (!p->child(l)) p->kids.push_back(Node{e.raw.back(), l, e.dir, e.offset, e.dir ? 0 : e.size, {}});
    });
    uint64_t next = kSecondBase, bytes = 0;
    size_t added = 0;
    walk_fst(fst2, [&](const FstEntry& e) {
        Node* p = parent_of(root, e);
        const std::string l = lower(e.raw.back());
        if (e.dir) {
            if (!p->child(l)) p->kids.push_back(Node{e.raw.back(), l, true, 0, 0, {}});
            return;
        }
        if (p->child(l)) return;   // deja sur le disque 1
        p->kids.push_back(Node{e.raw.back(), l, false, next, e.size, {}});
        m_pieces.push_back({next, e.size, m_second.get(), e.offset, {}});
        next = (next + e.size + 0x7FFF) & ~0x7FFFull;
        bytes += e.size;
        ++added;
    });
    if (!added) {
        NJ_LOG << "[VDisc] second disque : rien de nouveau\n";
        return false;
    }
    std::vector<uint8_t> entries(12, 0), names;
    entries[0] = 1;
    uint32_t index = 1;
    serialize(root, 0, index, entries, names);
    put32(entries, 8, index);
    m_fst = std::move(entries);
    m_fst.insert(m_fst.end(), names.begin(), names.end());
    // L'en-tete annonce la nouvelle taille (0x428) et le maximum sur les disques (0x42C) : en
    // memoire et sur le disque servi, pour que le jeu alloue assez.
    const uint32_t size = (uint32_t)m_fst.size();
    put32(m_boot, 0x42C, std::max(be32(&m_boot[0x42C]), size));
    put32(m_boot, 0x428, size);
    m_patches.push_back({0x428, std::vector<uint8_t>(m_boot.begin() + 0x428, m_boot.begin() + 0x430)});
    m_has_second = true;
    NJ_LOG << "[VDisc] second disque (" << m_second->format() << ") monte : " << added << " fichiers ("
              << bytes / 1000000 << " Mo), FST " << size << " octets" << std::endl;
    return true;
}

void DiscView::set_fst_entry(uint32_t index, uint64_t offset, uint32_t size) {
    put32(m_fst, size_t(index) * 12 + 4, (uint32_t)offset);
    put32(m_fst, size_t(index) * 12 + 8, size);
}

void DiscView::apply_overrides() {
    const auto overrides = file_overrides();
    if (overrides.empty()) return;
    walk_fst(m_fst, [&](const FstEntry& e) {
        if (e.dir) return;
        for (const auto& o : overrides) {
            if (o.first != e.lower) continue;
            std::error_code ec;
            const uint64_t size = fs::file_size(fs::u8path(o.second), ec);
            if (ec) {
                NJ_LOG << "[VDisc] remplacement de " << e.lower << " introuvable : " << o.second << std::endl;
                return;
            }
            if (e.lower == "ss/eng/ss_pzzl.dat" && size > 0xE4000)
                gfx::g_case_pzzl_extra = (uint32_t)(size - 0xE4000);
            if (size <= e.size) {
                // a sa place ; la fin de l'emplacement se lit a zero
                m_pieces.push_back({e.offset, e.size, nullptr, 0, o.second});
            } else {
                // plus grand : servi au-dela du disque, l'entree de la FST pointe la-bas
                const uint64_t at = m_override_next;
                m_override_next = (at + size + 0x7FFF) & ~0x7FFFull;
                m_pieces.push_back({at, size, nullptr, 0, o.second});
                set_fst_entry(e.index, at, (uint32_t)size);
            }
            NJ_LOG << "[VDisc] " << e.lower << " <- " << o.second << " (" << size << " octets, emplacement "
                      << e.size << ")" << std::endl;
            return;
        }
    });
}

// Modifications d'octets des mods (21/10, Nightjar Engine Studio) : <mod>/octets.txt, une ligne par modification :
// « chemin_sous_files decalage_hex octets_hex » (ex. « etc/emleon01.esl 2c 00fa »). Le mod ne contient que NOS valeurs,
// aucun fichier du disque : elles sont posees par-dessus le disque du joueur a la lecture (m_patches, read()).
void DiscView::apply_byte_mods() {
    for (const nj::mods::Mod& m : nj::mods::list()) {
        if (!m.enabled || m.local) continue;
        std::ifstream f(fs::u8path(m.dir) / "octets.txt");
        if (!f) continue;
        std::string line;
        size_t n = 0;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            std::string path, off_hex, bytes_hex;
            if (!(ss >> path >> off_hex >> bytes_hex) || bytes_hex.size() % 2) continue;
            for (char& ch : path) ch = (char)std::tolower((unsigned char)ch);
            uint64_t file_off = 0;
            uint32_t file_len = 0;
            if (!find_file(path, file_off, file_len)) continue;
            const uint64_t rel = std::strtoull(off_hex.c_str(), nullptr, 16);
            std::vector<uint8_t> data;
            for (size_t k = 0; k + 1 < bytes_hex.size(); k += 2)
                data.push_back((uint8_t)std::strtoul(bytes_hex.substr(k, 2).c_str(), nullptr, 16));
            if (data.empty() || rel + data.size() > file_len) continue;   // jamais hors du fichier
            m_patches.push_back({file_off + rel, std::move(data)});
            ++n;
        }
        if (n) NJ_LOG << "[VDisc] mod " << m.id << " : " << n << " modification(s) d'octets" << std::endl;
    }
}

std::string DiscView::file_at(uint64_t offset) {
    // index trie (debut, fin, chemin), construit au premier appel apres la fusion des deux disques
    struct Span {
        uint64_t lo, hi;
        std::string path;
    };
    static std::vector<Span> spans;
    static size_t built_for = 0;
    if (built_for != m_fst.size()) {
        spans.clear();
        walk_fst(m_fst, [&](const FstEntry& e) {
            if (!e.dir && e.size) spans.push_back({e.offset, e.offset + e.size, e.lower});
        });
        std::sort(spans.begin(), spans.end(), [](const Span& a, const Span& b) { return a.lo < b.lo; });
        built_for = m_fst.size();
    }
    auto it = std::upper_bound(spans.begin(), spans.end(), offset, [](uint64_t o, const Span& s) { return o < s.lo; });
    if (it == spans.begin()) return {};
    --it;
    return offset < it->hi ? it->path : std::string();
}

bool DiscView::read_file(const std::string& path, std::vector<uint8_t>& out) {
    std::string lower = path;
    for (char& c : lower) c = (char)std::tolower((unsigned char)c);
    uint64_t off = 0;
    uint32_t len = 0;
    if (!m_ok && !m_folder.empty()) {   // jeu natif : dossier extrait (casse du disque, puis minuscules)
        for (const std::string *p : {&path, (const std::string *) &lower}) {
            FILE *f = std::fopen((m_folder + "/" + *p).c_str(), "rb");
            if (!f) continue;
            out.clear();
            uint8_t buf[65536];
            size_t r;
            while ((r = std::fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + r);
            std::fclose(f);
            return true;
        }
        return false;
    }
    if (!m_ok || !find_file(lower, off, len)) return false;
    out.resize(len);
    return read(off, len, out.data()) == len;
}

bool DiscView::find_file(const std::string& lower_path, uint64_t& off, uint32_t& len) const {
    bool found = false;
    walk_fst(m_fst, [&](const FstEntry& e) {
        if (!found && !e.dir && e.lower == lower_path) {
            off = e.offset;
            len = e.size;
            found = true;
        }
    });
    return found;
}

// Le build debug (25/11/2004) allume par son debug/config.txt l'affichage de debug (PRINT_PAGE 1 ->
// pG->debug_mode : horloge, P[...], Rank, memoire, barres de temps) et les barres de temps
// (PROCESS_BAR). Le jeu recoit ce fichier corrige, a la meme longueur : PRINT_PAGE 0, PROCESS_BAR /
// WARNING_LOG OFF. NJ_DEBUG_HUD=1 garde le fichier tel quel. Les disques commerciaux n'ont pas
// ce fichier.
void DiscView::release_config() {
    // NJ_BOOTROOM=<stage>,<salle>[,<point>] (hex, comme le fichier : "2,00" = r200) : salle de
    // depart des parties automatisees. Chaque valeur du fichier s'ecrit 0xNN : remplacee a
    // longueur constante.
    const char* bootroom = NJ_ENV("NJ_BOOTROOM");
    const bool keep_debug = NJ_ENV("NJ_DEBUG_HUD") || nj::runtime::gfx::g_debug_mode;   // Settings > Debug mode
    if (keep_debug && !bootroom) return;
    uint64_t off = 0;
    uint32_t len = 0;
    if (!find_file("debug/config.txt", off, len) || len == 0 || len > 0x10000) return;
    std::vector<uint8_t> buf(len);
    if (read_raw(off, len, buf.data()) != len) return;
    std::string s(buf.begin(), buf.end());
    auto value_at = [&](const char* key) -> size_t {
        const size_t k = s.find(key);
        if (k == std::string::npos) return std::string::npos;
        size_t v = k + std::strlen(key);
        while (v < s.size() && (s[v] == ' ' || s[v] == '\t')) ++v;
        return v;
    };
    int changed = 0;
    size_t v;
    if (bootroom) {
        const char* p = bootroom;
        for (const char* key : {"[STAGE]", "[ROOM]", "[JUMP_POINT]"}) {
            if (!*p) break;
            const unsigned long val = std::strtoul(p, const_cast<char**>(&p), 16);
            if (*p == ',') ++p;
            v = value_at(key);
            if (v == std::string::npos || v + 4 > s.size() || s.compare(v, 2, "0x") != 0) continue;
            static const char hex[] = "0123456789ABCDEF";
            s[v + 2] = hex[(val >> 4) & 0xF];
            s[v + 3] = hex[val & 0xF];
            ++changed;
        }
        NJ_LOG << "[VDisc] debug/config.txt : salle de depart " << bootroom << "\n";
    }
    if (keep_debug) {
        if (changed) m_patches.push_back({off, std::vector<uint8_t>(s.begin(), s.end())});
        return;
    }
    v = value_at("[PRINT_PAGE]");   // n -> 0, les chiffres suivants deviennent des espaces
    if (v != std::string::npos && v < s.size() && std::isdigit((unsigned char)s[v])) {
        s[v++] = '0';
        while (v < s.size() && std::isalnum((unsigned char)s[v])) s[v++] = ' ';
        ++changed;
    }
    for (const char* key : {"[PROCESS_BAR]", "[WARNING_LOG]"}) {   // ON -> OFF (prend le blanc d'avant)
        v = value_at(key);
        const size_t kend = s.find(key) + std::strlen(key);
        if (v != std::string::npos && s.compare(v, 2, "ON") == 0 && v > kend) {
            s.replace(v - 1, 3, "OFF");
            ++changed;
        }
    }
    if (changed) {
        m_patches.push_back({off, std::vector<uint8_t>(s.begin(), s.end())});
        NJ_LOG << "[VDisc] debug/config.txt : mode debug coupe (" << changed
                  << " cles, NJ_DEBUG_HUD=1 pour le garder)" << std::endl;
    }
}

size_t DiscView::read(uint64_t offset, uint32_t len, uint8_t* dst) {
    const size_t got = read_raw(offset, len, dst);
    for (const Patch& p : m_patches) {   // correctifs par-dessus ce que le disque a rendu
        const uint64_t lo = std::max<uint64_t>(offset, p.offset);
        const uint64_t hi = std::min<uint64_t>(offset + len, p.offset + p.data.size());
        if (lo < hi) std::memcpy(dst + (lo - offset), p.data.data() + (lo - p.offset), (size_t)(hi - lo));
    }
    return got;
}

size_t DiscView::read_raw(uint64_t offset, uint32_t len, uint8_t* dst) {
    if (!m_ok) return 0;
    std::memset(dst, 0, size_t(len));
    const uint64_t end = offset + len;
    bool any = false;
    // disque 1 (au-dela de sa fin : zeros)
    if (offset < m_first->size()) {
        const size_t n = (size_t)std::min<uint64_t>(len, m_first->size() - offset);
        if (!m_first->read(offset, dst, n)) return 0;
        any = true;
    }
    // disque 2 et remplacements, par-dessus
    for (const Piece& p : m_pieces) {
        if (p.offset >= end) break;
        if (p.offset + p.size <= offset) continue;
        const uint64_t lo = std::max(offset, p.offset), hi = std::min(end, p.offset + p.size);
        uint8_t* out = dst + (lo - offset);
        const size_t n = (size_t)(hi - lo);
        if (p.image) {
            if (!p.image->read(p.src + (lo - p.offset), out, n)) continue;
        } else {
            std::memset(out, 0, n);
            FILE* f = nullptr;
#ifdef _WIN32
            f = _wfopen(fs::u8path(p.host).wstring().c_str(), L"rb");
#else
            f = std::fopen(p.host.c_str(), "rb");
#endif
            if (!f) continue;
#ifdef _WIN32
            _fseeki64(f, (long long)(lo - p.offset), SEEK_SET);
#else
            fseeko(f, (off_t)(lo - p.offset), SEEK_SET);
#endif
            (void)std::fread(out, 1, n, f);   // fichier plus court que l'emplacement : zeros
            std::fclose(f);
        }
        any = true;
    }
    if (!any) {
        static int budget = 20;
        if (budget > 0 && budget--)
            NJ_LOG << "[VDisc] lecture hors disque : 0x" << std::hex << offset << " +0x" << len << std::dec << "\n";
    }
    return any ? len : 0;
}

}  // namespace nj::runtime

// Crochet HLE (cDvd::DiscChange -> DVDCompareDiskID(courant, voulu), adresse dans la config de
// njrecomp) : avec le disque 2 fusionne, tout est deja la ; on repond « identique » et on rend la
// main comme le corps d'origine (pc = lr). Sans second disque, comportement d'origine.
void nj_dvd_CompareDiskID(nj::runtime::GuestCpu& ctx, void (*orig)(nj::runtime::GuestCpu&)) {
    if (nj::runtime::DiscView::get().has_second()) {
        ctx.gpr[3] = 1;
        ctx.pc = ctx.lr;
        return;
    }
    orig(ctx);
}
