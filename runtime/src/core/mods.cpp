// Gestionnaire de mods : liste, priorite, installation, index de ce que fournissent les mods
// actives (runtime/mods.h).
#include "runtime/mods.h"
#include "runtime/config.h"
#include "runtime/disc_image.h"
#include "zstd.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

#include <SDL.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "../input/touch/stb_image.h"   // inflate des zips (stbi_zlib_decode_noheader_buffer)

namespace fs = std::filesystem;

namespace nj::mods {

namespace {

std::mutex g_m;                          // liste, index, etat de l'installation
std::vector<Mod> g_list;
bool g_loaded = false;
std::shared_ptr<const Index> g_index;
std::atomic<uint32_t> g_gen{1};
std::atomic<bool> g_new_textures{false};

struct InstallState {
    std::atomic<bool> running{false};
    std::atomic<uint64_t> done{0}, total{0};
    std::string name, message;
    bool ok = false;
} g_inst;

fs::path u8(const std::string &s) { return fs::path(std::u8string(s.begin(), s.end())); }
std::string str(const fs::path &p) {
    const std::u8string u = p.u8string();
    return std::string(u.begin(), u.end());
}

#if defined(_WIN32) || defined(__APPLE__)   // iPhone : memes icones integrees que sur PC (pas d'APK ou les copier)
#include "ui_buttons_mobile.inc"   // kNjUiButtonsMobile (tools/p3/embarquer_boutons.py)

// PC (15/10) : les icones mobiles n'existaient que dans l'APK ; le style « mobile » laissait les
// boutons GameCube. Deposees a cote du dossier des mods, reecrites si absentes ou de taille fausse.
fs::path ui_buttons_root() {
    const fs::path root = u8(mods_dir()).parent_path() / "ui_buttons";
    const fs::path dir = root / "mobile";
    std::error_code ec;
    fs::create_directories(dir, ec);
    for (const NjUiButton &b : kNjUiButtonsMobile) {
        const fs::path f = dir / b.name;
        if (fs::file_size(f, ec) == b.size && !ec) continue;
        ec.clear();
        std::ofstream o(f, std::ios::binary | std::ios::trunc);
        o.write(b.bytes, b.size);
    }
    return root;
}
#else
fs::path ui_buttons_root() { return fs::path("ui_buttons"); }   // Android : copiees depuis l'APK
#endif
std::string gstr(const fs::path &p) {
    const std::u8string u = p.generic_u8string();
    return std::string(u.begin(), u.end());
}

std::string lower(std::string s) {
    for (char &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// JSON minimal : "cle": "valeur" (les manifestes sont courts et plats).
std::string json_str(const std::string &body, const char *key) {
    const std::string k = std::string("\"") + key + "\"";
    size_t p = body.find(k);
    if (p == std::string::npos) return {};
    p = body.find(':', p + k.size());
    if (p == std::string::npos) return {};
    p = body.find_first_not_of(" \t\r\n", p + 1);
    if (p == std::string::npos || body[p] != '"') return {};
    std::string out;
    for (size_t i = p + 1; i < body.size() && body[i] != '"'; ++i) {
        if (body[i] == '\\' && i + 1 < body.size()) ++i;
        out += body[i];
    }
    return out;
}

std::string json_escape(const std::string &s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        if ((unsigned char)c >= 0x20) o += c;
    }
    return o;
}

std::string read_text(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Identifiant de dossier : lettres, chiffres, - et _.
std::string make_id(const std::string &s) {
    std::string o;
    for (char c : s) {
        const unsigned char u = (unsigned char)c;
        if (std::isalnum(u) || c == '-' || c == '_') o += (char)std::tolower(u);
        else if (c == ' ' || c == '.') o += '-';
    }
    while (!o.empty() && o.back() == '-') o.pop_back();
    return o.empty() ? std::string("mod") : o.substr(0, 64);
}

fs::path state_file() { return u8(mods_dir()) / "mods.txt"; }

// Lit un dossier de mod (manifeste, contenu, taille).
Mod read_mod(const fs::path &dir, const std::string &id) {
    Mod m;
    m.id = id;
    m.dir = str(dir);
    const std::string man = read_text(dir / "manifest.json");
    m.name = json_str(man, "name");
    m.version = json_str(man, "version");
    m.author = json_str(man, "author");
    m.description = json_str(man, "description");
    m.features = json_str(man, "features");
    if (m.name.empty()) m.name = id;
    std::error_code ec;
    m.has_textures = fs::is_directory(dir / "textures", ec);
    m.has_files = fs::is_directory(dir / "files", ec);
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec)) m.bytes += it->file_size(ec);
    return m;
}

void save_state_locked() {
    std::error_code ec;
    fs::create_directories(u8(mods_dir()), ec);
    std::ofstream o(state_file(), std::ios::binary | std::ios::trunc);
    for (const Mod &m : g_list)
        o << (m.enabled ? "1 " : "0 ") << m.id << "\n";
}

// Index des mods actives, du moins prioritaire au plus prioritaire (le dernier ecrit gagne).
void rebuild_index_locked() {
    auto ix = std::make_shared<Index>();
    // Icones internes de Nightjar (Android : ui_buttons/mobile, copiees depuis l'APK) : pas un mod,
    // la plus basse priorite (14/10).
    {
        std::error_code ec;
        for (fs::directory_iterator s(ui_buttons_root(), ec), end; !ec && s != end; s.increment(ec)) {
            if (!s->is_directory(ec)) continue;
            auto &b = ix->buttons[lower(str(s->path().filename()))];
            for (fs::directory_iterator f(s->path(), ec), e2; !ec && f != e2; f.increment(ec))
                if (lower(str(f->path().extension())) == ".png") b[str(f->path().stem())] = str(f->path());
            ec.clear();
        }
    }
    for (auto it = g_list.rbegin(); it != g_list.rend(); ++it) {
        const Mod &m = *it;
        if (!m.enabled) continue;
        std::error_code ec;
        const fs::path tex = m.local ? u8(m.dir) : u8(m.dir) / "textures";
        if (fs::is_directory(tex, ec)) {
            for (fs::directory_iterator d(tex, ec), end; !ec && d != end; d.increment(ec)) {
                const fs::path &p = d->path();
                const std::string ext = lower(str(p.extension()));
                const std::string stem = str(p.stem());
                if (ext == ".png") {
                    ix->textures[stem] = str(p);
                } else if (ext == ".txt" && stem.size() > 5 && stem.compare(stem.size() - 5, 5, "_keys") == 0) {
                    auto &set = ix->lists[stem.substr(0, stem.size() - 5)];
                    std::ifstream f(p);
                    for (std::string l; std::getline(f, l);) {
                        while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
                        if (!l.empty()) set.insert(l);
                    }
                }
            }
            for (fs::directory_iterator s(tex / "buttons", ec), end; !ec && s != end; s.increment(ec)) {
                if (!s->is_directory(ec)) continue;
                auto &b = ix->buttons[lower(str(s->path().filename()))];
                for (fs::directory_iterator f(s->path(), ec), e2; !ec && f != e2; f.increment(ec))
                    if (lower(str(f->path().extension())) == ".png") b[str(f->path().stem())] = str(f->path());
                ec.clear();
            }
            ec.clear();
            for (fs::recursive_directory_iterator r(tex / "models", ec), end; !ec && r != end; r.increment(ec))
                if (r->is_regular_file(ec))
                    ix->extra[lower(gstr(fs::relative(r->path(), tex, ec)))] = str(r->path());
            ec.clear();
        }
        const fs::path files = u8(m.dir) / "files";
        if (!m.local && fs::is_directory(files, ec)) {
            for (fs::recursive_directory_iterator r(files, ec), end; !ec && r != end; r.increment(ec)) {
                if (!r->is_regular_file(ec)) continue;
                const std::string rel = lower(gstr(fs::relative(r->path(), files, ec)));
                auto f = std::find_if(ix->disc_files.begin(), ix->disc_files.end(),
                                      [&](const auto &x) { return x.first == rel; });
                if (f != ix->disc_files.end()) f->second = str(r->path());
                else ix->disc_files.push_back({rel, str(r->path())});
            }
        }
    }
    g_index = ix;
    g_gen.fetch_add(1);
}

void load_locked() {
    if (g_loaded) return;
    g_loaded = true;
    g_list.clear();
    std::error_code ec;
    const fs::path root = u8(mods_dir());
    fs::create_directories(root, ec);   // le joueur trouve le dossier sans le creer (14/10)
    ec.clear();
    // ordre et etat enregistres, puis les dossiers nouveaux (actives, en fin de liste)
    std::vector<std::pair<std::string, bool>> order;
    {
        std::ifstream f(state_file());
        for (std::string l; std::getline(f, l);) {
            while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
            if (l.size() > 2) order.push_back({l.substr(2), l[0] == '1'});
        }
    }
    for (const auto &[id, on] : order) {
        if (id == "textures_hd") {   // le dossier local garde sa place
            if (fs::is_directory("textures_hd", ec)) {
                Mod m = read_mod(fs::path("textures_hd"), "textures_hd");
                m.name = "textures_hd folder (next to Nightjar)";
                m.local = true;
                m.has_textures = true;
                m.enabled = on;
                g_list.push_back(m);
            }
            continue;
        }
        if (fs::is_directory(root / u8(id), ec)) {
            Mod m = read_mod(root / u8(id), id);
            m.enabled = on;
            g_list.push_back(m);
        }
    }
    auto known = [&](const std::string &id) {
        return std::any_of(g_list.begin(), g_list.end(), [&](const Mod &m) { return m.id == id; });
    };
    for (fs::directory_iterator d(root, ec), end; !ec && d != end; d.increment(ec)) {
        const std::string id = str(d->path().filename());
        if (d->is_directory(ec) && id[0] != '.' && !known(id)) g_list.push_back(read_mod(d->path(), id));
    }
    ec.clear();
    if (!known("textures_hd") && fs::is_directory("textures_hd", ec)) {
        Mod m = read_mod(fs::path("textures_hd"), "textures_hd");
        m.name = "textures_hd folder (next to Nightjar)";
        m.local = true;
        m.has_textures = true;
        g_list.push_back(m);
    }
    rebuild_index_locked();
}

// ---- zip --------------------------------------------------------------------------------------

uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

struct ZipEntry {
    std::string name;
    uint32_t method = 0;
    uint64_t csize = 0, usize = 0, local = 0;
};

bool zip_list(std::ifstream &f, std::vector<ZipEntry> &out, std::string &err) {
    f.seekg(0, std::ios::end);
    const uint64_t size = (uint64_t)f.tellg();
    const uint64_t tail = std::min<uint64_t>(size, 66000);
    std::vector<uint8_t> t(tail);
    f.seekg((std::streamoff)(size - tail));
    f.read((char *)t.data(), (std::streamsize)tail);
    size_t e = SIZE_MAX;
    for (size_t i = tail >= 22 ? tail - 22 : 0; i != SIZE_MAX; --i)
        if (rd32(&t[i]) == 0x06054b50) { e = i; break; }
    if (e == SIZE_MAX) { err = "not a zip file"; return false; }
    const uint32_t count = rd16(&t[e + 10]), cd_size = rd32(&t[e + 12]), cd_off = rd32(&t[e + 16]);
    if (count == 0xFFFF || cd_off == 0xFFFFFFFFu) { err = "zip64 archives are not supported"; return false; }
    std::vector<uint8_t> cd(cd_size);
    f.seekg(cd_off);
    f.read((char *)cd.data(), cd_size);
    if (!f) { err = "damaged zip"; return false; }
    for (size_t p = 0, n = 0; n < count; ++n) {
        if (p + 46 > cd.size() || rd32(&cd[p]) != 0x02014b50) { err = "damaged zip"; return false; }
        ZipEntry z;
        z.method = rd16(&cd[p + 10]);
        z.csize = rd32(&cd[p + 20]);
        z.usize = rd32(&cd[p + 24]);
        const uint16_t nl = rd16(&cd[p + 28]), xl = rd16(&cd[p + 30]), cl = rd16(&cd[p + 32]);
        z.local = rd32(&cd[p + 42]);
        z.name.assign((const char *)&cd[p + 46], nl);
        std::replace(z.name.begin(), z.name.end(), '\\', '/');
        out.push_back(z);
        p += 46 + nl + xl + cl;
    }
    return true;
}

bool zip_read(std::ifstream &f, const ZipEntry &z, std::vector<char> &out) {
    uint8_t h[30];
    f.seekg((std::streamoff)z.local);
    f.read((char *)h, 30);
    if (!f || rd32(h) != 0x04034b50) return false;
    f.seekg((std::streamoff)(z.local + 30 + rd16(h + 26) + rd16(h + 28)));
    std::vector<char> c(z.csize);
    f.read(c.data(), (std::streamsize)z.csize);
    if (!f) return false;
    if (z.method == 0) {
        out.swap(c);
        return true;
    }
    if (z.method != 8) return false;
    out.resize(z.usize);
    if (z.usize == 0) return true;
    return stbi_zlib_decode_noheader_buffer(out.data(), (int)z.usize, c.data(), (int)z.csize) == (int)z.usize;
}

bool safe_rel(const std::string &n) {
    if (n.empty() || n[0] == '/' || n.find(':') != std::string::npos) return false;
    for (size_t s = 0; s <= n.size();) {
        size_t e = n.find('/', s);
        if (e == std::string::npos) e = n.size();
        if (n.compare(s, e - s, "..") == 0 && e - s == 2) return false;
        s = e + 1;
    }
    return true;
}

// Chemin d'une entree dans le mod : dossier commun retire, textures_hd/ -> textures/.
std::string map_entry(std::string n, const std::string &strip) {
    if (!strip.empty() && n.compare(0, strip.size(), strip) == 0) n = n.substr(strip.size());
    if (lower(n.substr(0, 12)) == "textures_hd/") n = "textures/" + n.substr(12);
    return n;
}

// ---- mods de langue en patchs (13/10) -------------------------------------------------------
// patches/index.json : pour chaque fichier, le patch zstd (fichier d'origine = dictionnaire brut),
// taille et CRC32 avant / apres. Applique a l'installation sur les disques du joueur
// (Config::disc_image, disc2_image) ; resultat ecrit dans files/ du mod, comme un mod ordinaire.

uint32_t crc32(const uint8_t *p, size_t n) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

uint64_t json_num(const std::string &body, const char *key) {
    const std::string k = std::string("\"") + key + "\"";
    size_t p = body.find(k);
    if (p == std::string::npos) return 0;
    p = body.find(':', p + k.size());
    return p == std::string::npos ? 0 : std::strtoull(body.c_str() + p + 1, nullptr, 10);
}

// Vue des disques du joueur comme le moteur : disque 1, plus les fichiers du disque 2 absents du 1.
struct PlayerDiscs {
    std::unique_ptr<nj::runtime::DiscImage> d1, d2;
    struct Where { nj::runtime::DiscImage *img; uint64_t off; uint32_t size; };
    std::unordered_map<std::string, Where> files;
    std::string game_id;

    bool add(nj::runtime::DiscImage *img) {
        uint8_t h[0x430];
        if (!img->read(0, h, sizeof h)) return false;
        if (game_id.empty()) game_id.assign((const char *)h, 6);
        const uint32_t fo = (uint32_t)h[0x424] << 24 | h[0x425] << 16 | h[0x426] << 8 | h[0x427];
        const uint32_t fs = (uint32_t)h[0x428] << 24 | h[0x429] << 16 | h[0x42A] << 8 | h[0x42B];
        std::vector<uint8_t> fst(fs);
        if (!fs || fs > (64u << 20) || !img->read(fo, fst.data(), fs)) return false;
        nj::runtime::walk_fst(fst, [&](const nj::runtime::FstEntry &e) {
            if (!e.dir) files[e.lower] = {img, e.offset, e.size};
        });
        return true;
    }
    bool open() {
        const auto &cfg = nj::runtime::Config::get();
        if (!cfg.disc2_image.empty() && (d2 = nj::runtime::DiscImage::open(cfg.disc2_image))) add(d2.get());
        if (cfg.disc_image.empty() || !(d1 = nj::runtime::DiscImage::open(cfg.disc_image))) return false;
        game_id.clear();
        return add(d1.get());   // le disque 1 passe devant
    }
};

// Applique les patchs de tmp/patches dans tmp/files ; message d'erreur sinon.
bool apply_patches(const fs::path &tmp, std::string &err) {
    const std::string idx = read_text(tmp / "patches" / "index.json");
    const std::string man = read_text(tmp / "manifest.json");
    PlayerDiscs discs;
    if (!discs.open()) {
        err = "This mod changes game files: choose your game discs in Nightjar first, then install it again.";
        return false;
    }
    const std::string need = json_str(man, "requires");
    if (!need.empty() && discs.game_id != need) {
        err = "This mod needs the " + std::string(need == "G4BP08" ? "European" : need == "G4BE08" ? "USA" : need) +
              " version of the game (your disc: " + discs.game_id + ").";
        return false;
    }
    ZSTD_DCtx *dctx = ZSTD_createDCtx();
    ZSTD_DCtx_setParameter(dctx, ZSTD_d_windowLogMax, 27);
    size_t n = 0;
    for (size_t at = idx.find('{'); at != std::string::npos; at = idx.find('{', at + 1)) {
        const std::string obj = idx.substr(at, idx.find('}', at) - at);
        const std::string path = json_str(obj, "path"), patch = json_str(obj, "patch");
        if (path.empty() || patch.empty() || !safe_rel(path) || !safe_rel(patch)) continue;
        const auto w = discs.files.find(path);
        if (w == discs.files.end()) { err = "Your disc has no " + path + "."; break; }
        std::vector<uint8_t> src(w->second.size);
        if (!w->second.img->read(w->second.off, src.data(), src.size())) { err = "Couldn't read " + path + " from your disc."; break; }
        if (src.size() != json_num(obj, "src_size") || crc32(src.data(), src.size()) != (uint32_t)json_num(obj, "src_crc")) {
            err = "Your disc's " + path + " is not the one this mod was made for (different version or modified disc).";
            break;
        }
        const std::string pz = read_text(tmp / u8(patch));
        std::vector<uint8_t> dst((size_t)json_num(obj, "dst_size"));
        const size_t r = ZSTD_decompress_usingDict(dctx, dst.data(), dst.size(), pz.data(), pz.size(), src.data(), src.size());
        if (ZSTD_isError(r) || r != dst.size() || crc32(dst.data(), dst.size()) != (uint32_t)json_num(obj, "dst_crc")) {
            err = "The mod's patch for " + path + " is damaged.";
            break;
        }
        std::error_code ec;
        const fs::path out = tmp / "files" / u8(path);
        fs::create_directories(out.parent_path(), ec);
        std::ofstream o(out, std::ios::binary | std::ios::trunc);
        o.write((const char *)dst.data(), (std::streamsize)dst.size());
        if (!o) { err = "Couldn't write " + path + " (disk full?)"; break; }
        ++n;
    }
    ZSTD_freeDCtx(dctx);
    if (!err.empty()) return false;
    std::error_code ec;
    fs::remove_all(tmp / "patches", ec);
    return n > 0;
}

void set_result(bool ok, const std::string &msg) {
    std::lock_guard<std::mutex> l(g_m);
    g_inst.ok = ok;
    g_inst.message = msg;
}

void install_worker(std::string path) {
    std::error_code ec;
    const fs::path src = u8(path);
    const fs::path root = u8(mods_dir());
    fs::create_directories(root, ec);
    const std::string stem = str(src.stem());
    std::string id, name;
    const fs::path tmp = root / ".install";
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);
    bool textures = false;

    if (fs::is_directory(src, ec)) {
        // dossier : un mod (manifest.json), ou un ancien textures_hd/ (ou le dossier qui le contient)
        fs::path from = src;
        if (fs::is_directory(src / "textures_hd", ec) && !fs::exists(src / "manifest.json", ec)) from = src / "textures_hd";
        const bool bare = !fs::exists(from / "manifest.json", ec);
        std::vector<fs::path> files;
        for (fs::recursive_directory_iterator r(from, ec), end; !ec && r != end; r.increment(ec))
            if (r->is_regular_file(ec)) {
                files.push_back(r->path());
                g_inst.total += r->file_size(ec);
            }
        for (const fs::path &p : files) {
            const fs::path rel = fs::relative(p, from, ec);
            const fs::path dst = (bare ? tmp / "textures" : tmp) / rel;
            fs::create_directories(dst.parent_path(), ec);
            fs::copy_file(p, dst, fs::copy_options::overwrite_existing, ec);
            if (ec) { set_result(false, "Couldn't copy " + str(rel) + ": " + ec.message()); fs::remove_all(tmp, ec); return; }
            g_inst.done += fs::file_size(p, ec);
        }
    } else {
        std::ifstream f(src, std::ios::binary);
        std::vector<ZipEntry> entries;
        std::string err;
        if (!f || !zip_list(f, entries, err)) { set_result(false, "Couldn't read " + str(src.filename()) + ": " + (f ? err : "can't open the file")); return; }
        // dossier commun a toutes les entrees (zip d'un dossier) : retire s'il ne s'appelle pas textures_hd
        std::string strip;
        {
            const size_t s = entries.empty() ? std::string::npos : entries[0].name.find('/');
            if (s != std::string::npos) {
                const std::string first = entries[0].name.substr(0, s + 1);
                if (lower(first) != "textures_hd/" &&
                    std::all_of(entries.begin(), entries.end(), [&](const ZipEntry &z) { return z.name.compare(0, first.size(), first) == 0; }))
                    strip = first;
            }
        }
        for (const ZipEntry &z : entries) g_inst.total += z.usize;
        for (const ZipEntry &z : entries) {
            if (!z.name.empty() && z.name.back() == '/') continue;
            const std::string rel = map_entry(z.name, strip);
            if (!safe_rel(rel)) { set_result(false, "Unsafe path in the archive: " + z.name); fs::remove_all(tmp, ec); return; }
            std::vector<char> data;
            if (!zip_read(f, z, data)) { set_result(false, "Couldn't extract " + z.name + " (damaged or unsupported zip)"); fs::remove_all(tmp, ec); return; }
            const fs::path dst = tmp / u8(rel);
            fs::create_directories(dst.parent_path(), ec);
            std::ofstream o(dst, std::ios::binary | std::ios::trunc);
            o.write(data.data(), (std::streamsize)data.size());
            if (!o) { set_result(false, "Couldn't write " + rel + " (disk full?)"); fs::remove_all(tmp, ec); return; }
            g_inst.done += z.usize;
        }
    }

    if (fs::exists(tmp / "patches" / "index.json", ec)) {
        std::string err;
        if (!apply_patches(tmp, err)) {
            fs::remove_all(tmp, ec);
            return set_result(false, err.empty() ? std::string("This mod's patches are empty.") : err);
        }
    }
    textures = fs::is_directory(tmp / "textures", ec);
    // 21/10 : un mod peut n'allumer qu'une fonction du moteur (« features », ex. krauser.njmod) sans textures ni fichiers.
    if (!textures && !fs::is_directory(tmp / "files", ec) && json_str(read_text(tmp / "manifest.json"), "features").empty() &&
        !fs::exists(tmp / "octets.txt", ec)) {   // 21/10 : mod de modifications d'octets (Studio du Nightjar Engine)
        fs::remove_all(tmp, ec);
        return set_result(false, str(src.filename()) + " is not a Nightjar mod (no textures/ or files/ inside)");
    }
    const std::string man = read_text(tmp / "manifest.json");
    id = make_id(json_str(man, "id").empty() ? stem : json_str(man, "id"));
    name = json_str(man, "name").empty() ? stem : json_str(man, "name");
    if (man.empty()) {
        std::ofstream o(tmp / "manifest.json", std::ios::binary);
        o << "{\n  \"id\": \"" << json_escape(id) << "\",\n  \"name\": \"" << json_escape(name) << "\",\n  \"version\": \"\"\n}\n";
    }
    const fs::path dst = root / u8(id);
    {
        std::lock_guard<std::mutex> l(g_m);
        load_locked();
        fs::remove_all(dst, ec);   // mise a jour d'un mod deja installe
        ec.clear();
        fs::rename(tmp, dst, ec);
        if (ec) {
            g_inst.ok = false;
            g_inst.message = "Couldn't install " + name + ": " + ec.message();
            fs::remove_all(tmp, ec);
            return;
        }
        g_list.erase(std::remove_if(g_list.begin(), g_list.end(), [&](const Mod &m) { return m.id == id; }), g_list.end());
        Mod m = read_mod(dst, id);
        g_list.insert(g_list.begin(), m);   // le dernier installe passe en tete
        save_state_locked();
        rebuild_index_locked();
        g_inst.ok = true;
        g_inst.message = name + " installed.";
    }
    // archive posee dans le dossier des mods : plus utile une fois installee
    if (!fs::is_directory(src, ec) && fs::equivalent(src.parent_path(), root, ec)) fs::remove(src, ec);
    if (textures) g_new_textures = true;
}

}  // namespace

std::string mods_dir() {
#if defined(_WIN32)
    if (const char *l = std::getenv("LOCALAPPDATA")) return std::string(l) + "\\Nightjar\\mods";
    return "mods";
#elif defined(__ANDROID__)
    // coeur natif (processus sans Java) : l'appli lui donne son dossier de stockage
    if (const char *s = std::getenv("NJ_STORAGE_DIR")) return std::string(s) + "/mods";
    const char *d = SDL_AndroidGetExternalStoragePath();
    return d ? std::string(d) + "/mods" : std::string("mods");
#else
    return "mods";
#endif
}

std::vector<Mod> list() {
    std::lock_guard<std::mutex> l(g_m);
    load_locked();
    return g_list;
}

std::shared_ptr<const Index> index() {
    std::lock_guard<std::mutex> l(g_m);
    load_locked();
    return g_index;
}

uint32_t generation() {
    {
        std::lock_guard<std::mutex> l(g_m);
        load_locked();
    }
    return g_gen.load();
}

void set_enabled(const std::string &id, bool on) {
    std::lock_guard<std::mutex> l(g_m);
    load_locked();
    for (Mod &m : g_list)
        if (m.id == id && m.enabled != on) {
            m.enabled = on;
            save_state_locked();
            rebuild_index_locked();
        }
}

void move(const std::string &id, int delta) {
    std::lock_guard<std::mutex> l(g_m);
    load_locked();
    for (size_t i = 0; i < g_list.size(); ++i)
        if (g_list[i].id == id) {
            const long j = (long)i + delta;
            if (j < 0 || j >= (long)g_list.size()) return;
            std::swap(g_list[i], g_list[(size_t)j]);
            save_state_locked();
            rebuild_index_locked();
            return;
        }
}

bool remove(const std::string &id) {
    std::lock_guard<std::mutex> l(g_m);
    load_locked();
    for (size_t i = 0; i < g_list.size(); ++i)
        if (g_list[i].id == id && !g_list[i].local) {
            std::error_code ec;
            fs::remove_all(u8(g_list[i].dir), ec);
            g_list.erase(g_list.begin() + (long)i);
            save_state_locked();
            rebuild_index_locked();
            return !ec;
        }
    return false;
}

void install_async(const std::string &path) {
    if (g_inst.running.exchange(true)) return;
    {
        std::lock_guard<std::mutex> l(g_m);
        g_inst.name = str(u8(path).filename());
        g_inst.message.clear();
        g_inst.done = 0;
        g_inst.total = 0;
    }
    std::thread([path] {
        install_worker(path);
        g_inst.running = false;
    }).detach();
}

bool install_status(std::string &name, float &fraction, std::string &message, bool &ok) {
    std::lock_guard<std::mutex> l(g_m);
    name = g_inst.name;
    const uint64_t t = g_inst.total.load(), d = g_inst.done.load();
    fraction = t ? (float)((double)d / (double)t) : 0.0f;
    message = g_inst.message;
    ok = g_inst.ok;
    return g_inst.running.load();
}

bool take_new_textures() { return g_new_textures.exchange(false); }

std::vector<std::string> archives_in_folder() {
    std::vector<std::string> out;
    std::error_code ec;
    for (fs::directory_iterator d(u8(mods_dir()), ec), end; !ec && d != end; d.increment(ec)) {
        const std::string ext = lower(str(d->path().extension()));
        if (d->is_regular_file(ec) && (ext == ".njmod" || ext == ".zip")) out.push_back(str(d->path()));
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::string pick_mod_file() {
#if defined(_WIN32)
    wchar_t file[MAX_PATH * 4] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"Nightjar mod (*.njmod;*.zip)\0*.njmod;*.zip\0All files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH * 4;
    ofn.lpstrTitle = L"Install a Nightjar mod";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return {};
    const fs::path p(file);
    return str(p);
#else
    return {};
#endif
}

bool feature_enabled(const char *name) {
    const std::string want(name);
    for (const Mod &m : list()) {
        if (!m.enabled) continue;
        size_t p = 0;
        while (p <= m.features.size()) {
            size_t q = m.features.find(',', p);
            if (q == std::string::npos) q = m.features.size();
            std::string f = m.features.substr(p, q - p);
            f.erase(0, f.find_first_not_of(' '));
            f.erase(f.find_last_not_of(' ') + 1);
            if (f == want) return true;
            p = q + 1;
        }
    }
    return false;
}

}  // namespace nj::mods
