// Sauvegardes (13/10) : voir runtime/saves.h. Format des cartes GameCube, d'apres sa description
// publique : bloc 0 en-tete (taille en Mbit a 0x22), blocs 1 et 2 repertoire (127 entrees de 0x40,
// compteur de mise a jour a 0x1FFA, sommes a 0x1FFC/0x1FFE), blocs 3 et 4 table des blocs (sommes a
// 0x0000/0x0002, compteur 0x0004, blocs libres 0x0006, dernier alloue 0x0008, chainage des le
// 0x000A pour le bloc 5). Des deux copies, la plus recente valide fait foi ; on ecrit les deux.
#include "runtime/saves.h"

#include "common/platform.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace nj::saves {
namespace {

constexpr size_t kBlock = 0x2000;
constexpr size_t kEntry = 0x40;
constexpr int kEntries = 127;
constexpr int kKeepBackups = 5;

uint16_t be16(const uint8_t *p) { return uint16_t(p[0] << 8 | p[1]); }
uint32_t be32(const uint8_t *p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
void put16(uint8_t *p, uint16_t v) {
    p[0] = uint8_t(v >> 8);
    p[1] = uint8_t(v);
}

// Somme et somme inverse des mots de 16 bits ; 0xFFFF devient 0.
void sums(const uint8_t *p, size_t n, uint16_t &a, uint16_t &b) {
    a = b = 0;
    for (size_t i = 0; i + 1 < n; i += 2) {
        const uint16_t w = be16(p + i);
        a = uint16_t(a + w);
        b = uint16_t(b + uint16_t(~w));
    }
    if (a == 0xFFFF) a = 0;
    if (b == 0xFFFF) b = 0;
}

fs::path u8(const std::string &s) { return fs::u8path(s); }
std::string str(const fs::path &p) {
    const auto s = p.u8string();
    return std::string(s.begin(), s.end());
}

bool read_file(const fs::path &p, std::vector<uint8_t> &out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

bool write_file(const fs::path &p, const uint8_t *d, size_t n) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    const fs::path tmp = p.string() + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(reinterpret_cast<const char *>(d), (std::streamsize)n);
        if (!f) return false;
    }
    fs::rename(tmp, p, ec);
    if (ec) {   // Windows : la cible existe
        fs::remove(p, ec);
        fs::rename(tmp, p, ec);
    }
    return !ec;
}

std::string stamp() {
    const std::time_t t = std::time(nullptr);
    char b[32];
    std::strftime(b, sizeof b, "%Y%m%d_%H%M%S", std::localtime(&t));
    return b;
}

// Une carte en memoire : blocs, copies actives du repertoire et de la table.
struct Card {
    std::vector<uint8_t> d;
    int blocks = 0;
    int dir = 1, bat = 3;
    uint8_t *blk(int i) { return d.data() + size_t(i) * kBlock; }
    const uint8_t *blk(int i) const { return d.data() + size_t(i) * kBlock; }
};

bool dir_ok(const uint8_t *b) {
    uint16_t a, c;
    sums(b, 0x1FFC, a, c);
    return a == be16(b + 0x1FFC) && c == be16(b + 0x1FFE);
}
bool bat_ok(const uint8_t *b) {
    uint16_t a, c;
    sums(b + 4, kBlock - 4, a, c);
    return a == be16(b) && c == be16(b + 2);
}
// la plus recente des deux copies valides
int newest(const Card &c, int i, bool (*ok)(const uint8_t *), size_t counter_at) {
    const bool v0 = ok(c.blk(i)), v1 = ok(c.blk(i + 1));
    if (v0 != v1) return v0 ? i : i + 1;
    if (!v0) return -1;
    const int16_t d = int16_t(be16(c.blk(i + 1) + counter_at) - be16(c.blk(i) + counter_at));
    return d > 0 ? i + 1 : i;
}

bool parse(std::vector<uint8_t> data, Card &c, std::string &err) {
    if (data.size() < 5 * kBlock || data.size() % kBlock) {
        err = "not a GameCube memory card";
        return false;
    }
    c.d = std::move(data);
    int blocks = int(c.d.size() / kBlock);
    const int mbit = be16(c.blk(0) + 0x22);
    if (mbit > 0 && mbit * 16 < blocks) blocks = mbit * 16;
    c.blocks = blocks;
    c.dir = newest(c, 1, dir_ok, 0x1FFA);
    c.bat = newest(c, 3, bat_ok, 0x0004);
    if (c.dir < 0 || c.bat < 0) {
        err = "the memory card is not formatted yet";
        return false;
    }
    return true;
}

uint16_t bat_next(const Card &c, int block) { return be16(c.blk(c.bat) + 0x0A + size_t(block - 5) * 2); }
void bat_set(Card &c, int block, uint16_t v) { put16(c.blk(c.bat) + 0x0A + size_t(block - 5) * 2, v); }

std::string field(const uint8_t *p, size_t n) {
    std::string s;
    for (size_t i = 0; i < n && p[i]; ++i) s += (p[i] >= 0x20 && p[i] < 0x7F) ? char(p[i]) : '?';
    return s;
}

bool is_re4(const uint8_t *e) { return e[0] == 'G' && e[1] == '4' && e[2] == 'B'; }

File make_file(const uint8_t *entry, std::vector<uint8_t> data) {
    File f;
    f.game = field(entry, 4);
    f.maker = field(entry + 4, 2);
    f.name = field(entry + 8, 32);
    f.modtime = be32(entry + 0x28);
    f.blocks = int(data.size() / kBlock);
    const uint32_t co = be32(entry + 0x3C);
    if (co != 0xFFFFFFFF && co + 64 <= data.size()) {
        const std::string a = field(data.data() + co, 32), b = field(data.data() + co + 32, 32);
        f.comment = b.empty() ? a : a + " - " + b;
    }
    f.gci.assign(entry, entry + kEntry);
    f.gci.insert(f.gci.end(), data.begin(), data.end());
    return f;
}

void files_of(const Card &c, bool re4_only, std::vector<File> &out) {
    const uint8_t *dir = c.blk(c.dir);
    for (int i = 0; i < kEntries; ++i) {
        const uint8_t *e = dir + size_t(i) * kEntry;
        if (be32(e) == 0xFFFFFFFF || (re4_only && !is_re4(e))) continue;
        const int n = be16(e + 0x38);
        int b = be16(e + 0x36);
        std::vector<uint8_t> data;
        bool ok = n > 0;
        for (int k = 0; k < n && ok; ++k) {
            if (b < 5 || b >= c.blocks) {
                ok = false;
                break;
            }
            data.insert(data.end(), c.blk(b), c.blk(b) + kBlock);
            b = bat_next(c, b);
        }
        if (ok) out.push_back(make_file(e, std::move(data)));
    }
}

// Ecrit un fichier dans la carte (meme jeu, editeur et nom : remplace).
bool insert(Card &c, const File &f, std::string &err) {
    if (f.gci.size() < kEntry + kBlock || (f.gci.size() - kEntry) % kBlock) {
        err = f.name + ": damaged file";
        return false;
    }
    const int need = int((f.gci.size() - kEntry) / kBlock);
    uint8_t *dir = c.blk(c.dir);
    int slot = -1;
    for (int i = 0; i < kEntries; ++i) {
        uint8_t *e = dir + size_t(i) * kEntry;
        if (be32(e) == 0xFFFFFFFF) {
            if (slot < 0) slot = i;
            continue;
        }
        if (std::memcmp(e, f.gci.data(), 6) == 0 && std::memcmp(e + 8, f.gci.data() + 8, 32) == 0) {
            // meme fichier : ses blocs sont rendus, son entree reprise
            int b = be16(e + 0x36);
            for (int k = 0, n = be16(e + 0x38); k < n && b >= 5 && b < c.blocks; ++k) {
                const int next = bat_next(c, b);
                bat_set(c, b, 0);
                b = next;
            }
            std::memset(e, 0xFF, kEntry);
            slot = i;
        }
    }
    if (slot < 0) {
        err = "the memory card is full (127 files)";
        return false;
    }
    std::vector<int> free_blocks;
    for (int b = 5; b < c.blocks; ++b)
        if (bat_next(c, b) == 0) free_blocks.push_back(b);
    if ((int)free_blocks.size() < need) {
        err = "not enough free blocks on the memory card";
        return false;
    }
    free_blocks.resize(size_t(need));
    for (int k = 0; k < need; ++k) {
        std::memcpy(c.blk(free_blocks[k]), f.gci.data() + kEntry + size_t(k) * kBlock, kBlock);
        bat_set(c, free_blocks[k], k + 1 < need ? uint16_t(free_blocks[k + 1]) : 0xFFFF);
    }
    uint8_t *e = dir + size_t(slot) * kEntry;
    std::memcpy(e, f.gci.data(), kEntry);
    put16(e + 0x36, uint16_t(free_blocks[0]));
    put16(e + 0x38, uint16_t(need));
    return true;
}

// Recalcule et recopie repertoire et table dans leurs deux emplacements (compteur + 1).
void commit(Card &c) {
    int free_n = 0;
    for (int b = 5; b < c.blocks; ++b)
        if (bat_next(c, b) == 0) ++free_n;
    uint8_t *bat = c.blk(c.bat);
    put16(bat + 0x0006, uint16_t(free_n));
    put16(bat + 0x0004, uint16_t(be16(bat + 0x0004) + 1));
    uint16_t a, b;
    sums(bat + 4, kBlock - 4, a, b);
    put16(bat, a);
    put16(bat + 2, b);
    uint8_t *dir = c.blk(c.dir);
    put16(dir + 0x1FFA, uint16_t(be16(dir + 0x1FFA) + 1));
    sums(dir, 0x1FFC, a, b);
    put16(dir + 0x1FFC, a);
    put16(dir + 0x1FFE, b);
    std::memcpy(c.blk(c.dir == 1 ? 2 : 1), dir, kBlock);
    std::memcpy(c.blk(c.bat == 3 ? 4 : 3), bat, kBlock);
}

fs::path pending_dir() { return u8(nj::writable_path("saves_pending")); }

std::string backup(const std::string &card_file, const char *why) {
    std::vector<uint8_t> cur;
    if (!read_file(u8(card_file), cur) || cur.empty()) return {};
    const fs::path dir = u8(backups_dir());
    std::error_code ec;
    fs::create_directories(dir, ec);
    std::vector<fs::path> old;
    for (const auto &e : fs::directory_iterator(dir, ec))
        if (e.path().extension() == ".raw") old.push_back(e.path());
    std::sort(old.begin(), old.end());
    if (!why && !old.empty()) {   // copie de lancement : seulement si la carte a change
        std::vector<uint8_t> last;
        if (read_file(old.back(), last) && last == cur) return {};
    }
    const fs::path out = dir / ("memcard_" + stamp() + (why ? std::string("_") + why : std::string()) + ".raw");
    if (!write_file(out, cur.data(), cur.size())) return {};
    old.push_back(out);
    while ((int)old.size() > kKeepBackups) {
        fs::remove(old.front(), ec);
        old.erase(old.begin());
    }
    return str(out.filename());
}

}  // namespace

std::string card_path() { return nj::writable_path("memcard_a.raw"); }
std::string backups_dir() { return nj::writable_path("backups"); }

bool list_card(std::vector<File> &out, std::string &status) {
    out.clear();
    std::vector<uint8_t> d;
    if (!read_file(u8(card_path()), d)) {
        status = "No memory card yet (it is created the first time the game saves).";
        return false;
    }
    Card c;
    if (!parse(std::move(d), c, status)) return false;
    files_of(c, false, out);
    return true;
}

bool read_import(const std::string &path, std::vector<File> &out, std::string &err) {
    out.clear();
    std::vector<uint8_t> d;
    if (!read_file(u8(path), d)) {
        err = "can't read the file";
        return false;
    }
    std::string ext = u8(path).extension().string();
    for (char &ch : ext) ch = char(std::tolower((unsigned char)ch));
    const bool gci_shape = d.size() > kEntry && (d.size() - kEntry) % kBlock == 0 &&
                           be16(d.data() + 0x38) == (d.size() - kEntry) / kBlock;
    if (ext == ".gci" || (gci_shape && ext != ".raw")) {
        if (!gci_shape) {
            err = "damaged .gci file";
            return false;
        }
        if (!is_re4(d.data())) {
            err = "this save is not from Resident Evil 4";
            return false;
        }
        out.push_back(make_file(d.data(), std::vector<uint8_t>(d.begin() + kEntry, d.end())));
        return true;
    }
    Card c;
    if (!parse(std::move(d), c, err)) return false;
    files_of(c, true, out);
    if (out.empty()) err = "no Resident Evil 4 save on this memory card";
    return !out.empty();
}

void set_region(File &f, const std::string &game4) {
    if (game4.size() < 4 || f.gci.size() < kEntry) return;
    std::memcpy(f.gci.data(), game4.data(), 4);
    f.game = game4.substr(0, 4);
}

bool queue_import(const std::vector<File> &files, std::string &err) {
    std::error_code ec;
    fs::create_directories(pending_dir(), ec);
    int n = pending_count();
    for (const File &f : files) {
        char name[96];
        std::snprintf(name, sizeof name, "%03d_%s-%s-%s.gci", n++, f.maker.c_str(), f.game.c_str(), f.name.c_str());
        if (!write_file(pending_dir() / name, f.gci.data(), f.gci.size())) {
            err = "can't write the import";
            return false;
        }
    }
    return true;
}

int pending_count() {
    int n = 0;
    std::error_code ec;
    for (const auto &e : fs::directory_iterator(pending_dir(), ec))
        if (e.path().extension() == ".gci") ++n;
    return n;
}

bool export_all(std::string &folder, std::string &err) {
    std::vector<uint8_t> d;
    if (!read_file(u8(card_path()), d)) {
        err = "no memory card yet";
        return false;
    }
    const fs::path dir = u8(nj::writable_path("save_exports")) / stamp();
    if (!write_file(dir / "Nightjar-memcard.raw", d.data(), d.size())) {
        err = "can't write the export";
        return false;
    }
    Card c;
    std::string perr;
    if (parse(std::move(d), c, perr)) {
        std::vector<File> files;
        files_of(c, true, files);
        for (const File &f : files)
            write_file(dir / (f.maker + "-" + f.game + "-" + f.name + ".gci"), f.gci.data(), f.gci.size());
    }
    folder = str(dir);
    return true;
}

std::string on_card_open(const std::string &card_file) {
    std::string report;
    std::vector<fs::path> todo;
    std::error_code ec;
    for (const auto &e : fs::directory_iterator(pending_dir(), ec))
        if (e.path().extension() == ".gci") todo.push_back(e.path());
    std::sort(todo.begin(), todo.end());
    if (!todo.empty()) {
        std::vector<uint8_t> d;
        Card c;
        std::string err;
        if (!read_file(u8(card_file), d) || !parse(std::move(d), c, err)) {
            report = "Saves: import waiting, the memory card is not formatted yet (save once in the game first).";
        } else {
            backup(card_file, "before-import");
            int done = 0;
            for (const fs::path &p : todo) {
                std::vector<uint8_t> g;
                if (!read_file(p, g) || g.size() < kEntry) continue;
                File f = make_file(g.data(), std::vector<uint8_t>(g.begin() + kEntry, g.end()));
                std::string e2;
                if (insert(c, f, e2)) {
                    ++done;
                } else {
                    report += "Saves: " + f.name + " not imported (" + e2 + "). ";
                }
            }
            commit(c);
            if (write_file(u8(card_file), c.d.data(), c.d.size())) {
                for (const fs::path &p : todo) fs::remove(p, ec);
                report = "Saves: " + std::to_string(done) + " file(s) imported. " + report;
            } else {
                report = "Saves: can't write the memory card, import kept for the next start.";
            }
        }
    }
    const std::string b = backup(card_file, nullptr);
    if (!b.empty()) report += (report.empty() ? "" : " ") + std::string("Saves: backup ") + b + ".";
    return report;
}

std::vector<std::string> dolphin_candidates() {
    std::vector<std::string> out;
#if defined(_WIN32)
    std::vector<fs::path> bases;
    char b[1024];
    DWORD n = GetEnvironmentVariableA("USERPROFILE", b, sizeof b);
    if (n && n < sizeof b) bases.push_back(u8(b) / "Documents" / "Dolphin Emulator" / "GC");
    n = GetEnvironmentVariableA("APPDATA", b, sizeof b);
    if (n && n < sizeof b) bases.push_back(u8(b) / "Dolphin Emulator" / "GC");
    std::error_code ec;
    for (const fs::path &base : bases) {
        for (const auto &e : fs::directory_iterator(base, ec))
            if (e.path().extension() == ".raw" && e.path().filename().string().rfind("MemoryCard", 0) == 0)
                out.push_back(str(e.path()));
        for (const char *reg : {"USA", "EUR", "JAP"})
            for (const char *slot : {"Card A", "Card B"})
                for (const auto &e : fs::directory_iterator(base / reg / slot, ec))
                    if (e.path().extension() == ".gci" && e.path().filename().string().find("G4B") != std::string::npos)
                        out.push_back(str(e.path()));
    }
#endif
    return out;
}

}  // namespace nj::saves
