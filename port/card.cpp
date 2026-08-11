// -*- coding: utf-8 -*-
// Couche CARD du portage : la carte memoire GameCube, remplacee par un dossier de fichiers.
//
// Le jeu n'utilise ni le format de carte ni le catalogue de blocs : il appelle une quinzaine de
// fonctions (monter, ouvrir, creer, lire, ecrire, effacer, statut) et **lit le resultat avec
// CARDGetResultCode** apres chaque appel "Async". On peut donc executer chaque operation sur le
// champ et poser le resultat tout de suite - c'est exactement ce que le jeu attend, sans fil ni
// temporisation.
//
// Un fichier de carte = un fichier du dossier `card/` : un en-tete de CARDStat suivi des donnees.
// Dossier : $RE4_SAVE_DIR, sinon "card" a cote du programme. Une seule carte (canal 0, la A).
#include "port_log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>

#include <dolphin.h>
#include <dolphin/card.h>

namespace {

const s32 kBlock = 8192;
const s32 kMemoryBlocks = 251;          // une carte de 16 Mbit : 251 blocs utilisables
s32 g_result[2] = { 0, -3 };            // dernier resultat, par canal (la B est absente)
char g_dir[256] = { 0 };

struct Header {
    u32 magic;                           // 'CARD'
    u32 length;                          // taille des donnees
    CARDStat stat;
};
const u32 kMagic = 0x43415244u;

const char *dir()
{
    if (!g_dir[0]) {
        const char *e = std::getenv("RE4_SAVE_DIR");
        std::snprintf(g_dir, sizeof(g_dir), "%s", e ? e : "card");
        mkdir(g_dir, 0777);
    }
    return g_dir;
}

void path_of(const char *name, char *out, size_t n)
{
    char safe[CARD_FILENAME_MAX + 1];
    std::snprintf(safe, sizeof(safe), "%s", name);
    for (char *p = safe; *p; ++p)
        if (*p == '/' || *p == '\\' || *p == ':')
            *p = '_';
    std::snprintf(out, n, "%s/%s.bin", dir(), safe);
}

// Les noms des fichiers presents, dans l'ordre alphabetique : l'indice est le numero de fichier.
int list_files(char names[CARD_MAX_FILE][CARD_FILENAME_MAX + 1])
{
    int n = 0;
    // Pas de readdir ici : un petit fichier d'index tient l'ordre de creation.
    char idx[300];
    std::snprintf(idx, sizeof(idx), "%s/index.txt", dir());
    std::FILE *f = std::fopen(idx, "rb");
    if (!f) return 0;
    char line[128];
    while (n < CARD_MAX_FILE && std::fgets(line, sizeof(line), f)) {
        size_t l = std::strlen(line);
        while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
        if (!l) continue;
        char p[320];
        path_of(line, p, sizeof(p));
        struct stat st;
        if (stat(p, &st) != 0) continue;   // supprime hors du jeu : on l'oublie
        std::snprintf(names[n++], CARD_FILENAME_MAX + 1, "%s", line);
    }
    std::fclose(f);
    return n;
}

void save_index(char names[CARD_MAX_FILE][CARD_FILENAME_MAX + 1], int n)
{
    char idx[300];
    std::snprintf(idx, sizeof(idx), "%s/index.txt", dir());
    std::FILE *f = std::fopen(idx, "wb");
    if (!f) return;
    for (int i = 0; i < n; ++i)
        std::fprintf(f, "%s\n", names[i]);
    std::fclose(f);
}

int find(const char *name, char names[CARD_MAX_FILE][CARD_FILENAME_MAX + 1], int &n)
{
    n = list_files(names);
    for (int i = 0; i < n; ++i)
        if (std::strncmp(names[i], name, CARD_FILENAME_MAX) == 0)
            return i;
    return -1;
}

bool read_header(const char *name, Header &h)
{
    char p[320];
    path_of(name, p, sizeof(p));
    std::FILE *f = std::fopen(p, "rb");
    if (!f) return false;
    const bool ok = std::fread(&h, sizeof(h), 1, f) == 1 && h.magic == kMagic;
    std::fclose(f);
    return ok;
}

s32 set(s32 chan, s32 r)
{
    if (chan >= 0 && chan < 2) g_result[chan] = r;
    return r;
}

s32 done(s32 chan, s32 r, CARDCallback cb)
{
    set(chan, r);
    if (cb) cb(chan, r);
    return r;
}

} // namespace

extern "C" {

void CARDInit() { dir(); }

s32 CARDProbeEx(s32 chan, s32 *memSize, s32 *sectorSize)
{
    if (chan != 0) return CARD_RESULT_NOCARD;
    if (memSize) *memSize = 16;           // en megabits
    if (sectorSize) *sectorSize = kBlock;
    return CARD_RESULT_READY;
}

s32 CARDMountAsync(s32 chan, void *workArea, CARDCallback detach, CARDCallback attach)
{
    (void)workArea; (void)detach;
    if (chan != 0) return done(chan, CARD_RESULT_NOCARD, attach);
    return done(chan, CARD_RESULT_READY, attach);
}

s32 CARDCheckAsync(s32 chan, CARDCallback cb) { return done(chan, chan == 0 ? CARD_RESULT_READY : CARD_RESULT_NOCARD, cb); }
s32 CARDUnmount(s32 chan) { return set(chan, CARD_RESULT_READY); }
s32 CARDGetResultCode(s32 chan) { return (chan >= 0 && chan < 2) ? g_result[chan] : CARD_RESULT_NOCARD; }

s32 CARDGetSerialNo(s32 chan, u64 *serialNo)
{
    if (serialNo) *serialNo = 0x0123456789ABCDEFull;
    return set(chan, chan == 0 ? CARD_RESULT_READY : CARD_RESULT_NOCARD);
}

s32 CARDFreeBlocks(s32 chan, s32 *byteNotUsed, s32 *filesNotUsed)
{
    char names[CARD_MAX_FILE][CARD_FILENAME_MAX + 1];
    const int n = list_files(names);
    s32 used = 0;
    for (int i = 0; i < n; ++i) {
        Header h;
        if (read_header(names[i], h)) used += (s32)((h.length + kBlock - 1) / kBlock);
    }
    if (byteNotUsed) *byteNotUsed = (kMemoryBlocks - used) * kBlock;
    if (filesNotUsed) *filesNotUsed = CARD_MAX_FILE - n;
    return set(chan, CARD_RESULT_READY);
}

s32 CARDOpen(s32 chan, const char *fileName, CARDFileInfo *fi)
{
    char names[CARD_MAX_FILE][CARD_FILENAME_MAX + 1];
    int n;
    const int i = find(fileName, names, n);
    if (i < 0) return set(chan, CARD_RESULT_NOFILE);
    Header h;
    if (!read_header(names[i], h)) return set(chan, CARD_RESULT_BROKEN);
    if (fi) { fi->chan = chan; fi->fileNo = i; fi->offset = 0; fi->length = (s32)h.length; fi->iBlock = 0; }
    return set(chan, CARD_RESULT_READY);
}

s32 CARDClose(CARDFileInfo *fi) { return set(fi ? fi->chan : 0, CARD_RESULT_READY); }

s32 CARDCreateAsync(s32 chan, const char *fileName, u32 size, CARDFileInfo *fi, CARDCallback cb)
{
    if (std::strlen(fileName) > CARD_FILENAME_MAX) return done(chan, CARD_RESULT_NAMETOOLONG, cb);
    char names[CARD_MAX_FILE][CARD_FILENAME_MAX + 1];
    int n;
    if (find(fileName, names, n) >= 0) return done(chan, CARD_RESULT_EXIST, cb);
    if (n >= CARD_MAX_FILE) return done(chan, CARD_RESULT_NOENT, cb);
    s32 used = 0;
    for (int i = 0; i < n; ++i) { Header t; if (read_header(names[i], t)) used += (s32)((t.length + kBlock - 1) / kBlock); }
    if (used + (s32)((size + kBlock - 1) / kBlock) > kMemoryBlocks) return done(chan, CARD_RESULT_INSSPACE, cb);

    Header h;
    std::memset(&h, 0, sizeof(h));
    h.magic = kMagic;
    h.length = size;
    std::snprintf(h.stat.fileName, sizeof(h.stat.fileName), "%s", fileName);
    h.stat.length = size;
    h.stat.time = (u32)(std::time(nullptr) - 946684800);   // secondes depuis le 1/1/2000
    h.stat.iconAddr = h.stat.commentAddr = 0xFFFFFFFFu;
    char p[320];
    path_of(fileName, p, sizeof(p));
    std::FILE *f = std::fopen(p, "wb");
    if (!f) return done(chan, CARD_RESULT_IOERROR, cb);
    std::fwrite(&h, sizeof(h), 1, f);
    static const char zero[1024] = { 0 };
    for (u32 left = size; left;) { const u32 c = left > sizeof(zero) ? (u32)sizeof(zero) : left; std::fwrite(zero, 1, c, f); left -= c; }
    std::fclose(f);
    std::snprintf(names[n], CARD_FILENAME_MAX + 1, "%s", fileName);
    save_index(names, n + 1);
    if (fi) { fi->chan = chan; fi->fileNo = n; fi->offset = 0; fi->length = (s32)size; fi->iBlock = 0; }
    return done(chan, CARD_RESULT_READY, cb);
}

s32 CARDDeleteAsync(s32 chan, const char *fileName, CARDCallback cb)
{
    char names[CARD_MAX_FILE][CARD_FILENAME_MAX + 1];
    int n;
    const int i = find(fileName, names, n);
    if (i < 0) return done(chan, CARD_RESULT_NOFILE, cb);
    char p[320];
    path_of(fileName, p, sizeof(p));
    std::remove(p);
    for (int k = i; k + 1 < n; ++k) std::memcpy(names[k], names[k + 1], sizeof(names[k]));
    save_index(names, n - 1);
    return done(chan, CARD_RESULT_READY, cb);
}

namespace {
bool open_file(const CARDFileInfo *fi, char *name, std::FILE **out)
{
    char names[CARD_MAX_FILE][CARD_FILENAME_MAX + 1];
    const int n = list_files(names);
    if (!fi || fi->fileNo < 0 || fi->fileNo >= n) return false;
    std::snprintf(name, CARD_FILENAME_MAX + 1, "%s", names[fi->fileNo]);
    char p[320];
    path_of(name, p, sizeof(p));
    *out = std::fopen(p, "r+b");
    return *out != nullptr;
}
} // namespace

s32 CARDReadAsync(CARDFileInfo *fi, void *buf, s32 length, s32 offset, CARDCallback cb)
{
    const s32 chan = fi ? fi->chan : 0;
    char name[CARD_FILENAME_MAX + 1];
    std::FILE *f;
    if (!open_file(fi, name, &f)) return done(chan, CARD_RESULT_NOFILE, cb);
    std::fseek(f, (long)(sizeof(Header) + offset), SEEK_SET);
    const size_t got = std::fread(buf, 1, (size_t)length, f);
    std::fclose(f);
    if ((s32)got != length) std::memset((char *)buf + got, 0, (size_t)length - got);
    return done(chan, CARD_RESULT_READY, cb);
}

s32 CARDWriteAsync(CARDFileInfo *fi, void *buf, s32 length, s32 offset, CARDCallback cb)
{
    const s32 chan = fi ? fi->chan : 0;
    char name[CARD_FILENAME_MAX + 1];
    std::FILE *f;
    if (!open_file(fi, name, &f)) return done(chan, CARD_RESULT_NOFILE, cb);
    std::fseek(f, (long)(sizeof(Header) + offset), SEEK_SET);
    std::fwrite(buf, 1, (size_t)length, f);
    std::fclose(f);
    return done(chan, CARD_RESULT_READY, cb);
}

s32 CARDGetStatus(s32 chan, s32 fileNo, CARDStat *stat)
{
    char names[CARD_MAX_FILE][CARD_FILENAME_MAX + 1];
    const int n = list_files(names);
    if (fileNo < 0 || fileNo >= n) return set(chan, CARD_RESULT_NOFILE);
    Header h;
    if (!read_header(names[fileNo], h)) return set(chan, CARD_RESULT_BROKEN);
    if (stat) {
        *stat = h.stat;
        stat->length = h.length;
        stat->offsetData = 0;
    }
    return set(chan, CARD_RESULT_READY);
}

s32 CARDSetStatusAsync(s32 chan, s32 fileNo, CARDStat *stat, CARDCallback cb)
{
    char names[CARD_MAX_FILE][CARD_FILENAME_MAX + 1];
    const int n = list_files(names);
    if (fileNo < 0 || fileNo >= n) return done(chan, CARD_RESULT_NOFILE, cb);
    Header h;
    if (!read_header(names[fileNo], h) || !stat) return done(chan, CARD_RESULT_BROKEN, cb);
    const u32 len = h.length;
    h.stat = *stat;
    h.stat.length = len;
    char p[320];
    path_of(names[fileNo], p, sizeof(p));
    std::FILE *f = std::fopen(p, "r+b");
    if (f) { std::fwrite(&h, sizeof(h), 1, f); std::fclose(f); }
    return done(chan, f ? CARD_RESULT_READY : CARD_RESULT_IOERROR, cb);
}

} // extern "C"
