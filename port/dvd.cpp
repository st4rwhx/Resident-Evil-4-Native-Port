// -*- coding: utf-8 -*-
// Couche DVD du portage : le lecteur GameCube devient le disque de l'utilisateur.
//
// L'application mobile demande son jeu au premier lancement ; `port_set_game_dir()` recoit ce
// chemin, et tout ce qui suit lit dedans. Aucune donnee du jeu n'est embarquee.
//
// Le jeu n'ouvre pas des fichiers par nom a chaque fois : il traduit une fois un chemin en
// **entrynum** (l'index dans la FST du disque) puis rouvre par ce numero, et une partie du code
// range ces numeros. On construit donc une table d'index stable, une seule fois, en parcourant
// l'arborescence extraite - l'ordre n'a pas a coincider avec la FST d'origine tant qu'il est le
// meme d'un appel a l'autre.
//
// Les lectures asynchrones sont faites **synchronement** puis leur callback est appele tout de
// suite. C'est faux pour le rythme (le jeu croit que le lecteur met du temps) mais juste pour la
// semantique, et cela evite d'avoir besoin de threads avant d'avoir un ordonnanceur.
//
// Pas de `std::string` ni de `std::vector` ici : les en-tetes du decompile definissent un
// `operator new` global et des surcharges membres qui font echouer la compilation de libc++
// (`<vector>` ne trouve plus `__libcpp_allocate`). Des tampons fixes suffisent, et une couche
// de fichiers a de toute facon interet a ne pas allouer a chaque lecture.
#include "port_log.h"

#include <cstdio>
#include <cstring>

#include <dolphin.h>

namespace {

const int MAX_FILES = 8192;
const int MAX_PATH_LEN = 160;

struct Entry {
    char disc[MAX_PATH_LEN];   // chemin tel que le jeu le demande, minuscules, sans / initial
    char host[512];            // chemin reel sur la machine
    u32 size;
};

Entry g_files[MAX_FILES];
int g_count = 0;
char g_root[512] = {0};
bool g_ready = false;

void lower_inplace(char *s)
{
    for (; *s; ++s)
        if (*s >= 'A' && *s <= 'Z')
            *s = (char)(*s - 'A' + 'a');
}

// Le jeu ecrit tantot "st1/r100.das", tantot "/st1/r100.das", tantot "St1/R100.DAS" : la FST du
// GameCube est insensible a la casse pour ce code, et les chemins y sont relatifs a files/.
void normalise(const char *in, char *out, int cap)
{
    int i = 0;
    while (in && *in && (*in == '/' || *in == '\\' || *in == '.'))
        ++in;
    for (; in && *in && i < cap - 1; ++in) {
        char c = *in;
        if (c == '\\')
            c = '/';
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        out[i++] = c;
    }
    out[i] = 0;
}

void add_file(const char *host, const char *rel, u32 size)
{
    if (g_count >= MAX_FILES)
        return;
    Entry &e = g_files[g_count];
    normalise(rel, e.disc, MAX_PATH_LEN);
    std::snprintf(e.host, sizeof(e.host), "%s", host);
    e.size = size;
    ++g_count;
}

void scan(const char *dir, const char *rel);

} // namespace

void port_set_game_dir(const char *dir)
{
    std::snprintf(g_root, sizeof(g_root), "%s", dir ? dir : "");
    for (char *p = g_root; *p; ++p)
        if (*p == '\\')
            *p = '/';
    for (int n = (int)std::strlen(g_root); n > 0 && g_root[n - 1] == '/'; --n)
        g_root[n - 1] = 0;
    g_count = 0;
    g_ready = false;
}

// ---------------------------------------------------------------- parcours

#ifdef _WIN32
#include <windows.h>
namespace {
void scan(const char *dir, const char *rel)
{
    char pat[512];
    std::snprintf(pat, sizeof(pat), "%s/*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        if (!std::strcmp(fd.cFileName, ".") || !std::strcmp(fd.cFileName, ".."))
            continue;
        char full[512], sub[MAX_PATH_LEN];
        std::snprintf(full, sizeof(full), "%s/%s", dir, fd.cFileName);
        if (rel[0])
            std::snprintf(sub, sizeof(sub), "%s/%s", rel, fd.cFileName);
        else
            std::snprintf(sub, sizeof(sub), "%s", fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            scan(full, sub);
        else
            add_file(full, sub, fd.nFileSizeLow);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
} // namespace
#else
#include <dirent.h>
#include <sys/stat.h>
namespace {
void scan(const char *dir, const char *rel)
{
    DIR *d = opendir(dir);
    if (!d)
        return;
    while (dirent *de = readdir(d)) {
        if (!std::strcmp(de->d_name, ".") || !std::strcmp(de->d_name, ".."))
            continue;
        char full[512], sub[MAX_PATH_LEN];
        std::snprintf(full, sizeof(full), "%s/%s", dir, de->d_name);
        if (rel[0])
            std::snprintf(sub, sizeof(sub), "%s/%s", rel, de->d_name);
        else
            std::snprintf(sub, sizeof(sub), "%s", de->d_name);
        struct stat st;
        if (stat(full, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
            scan(full, sub);
        else
            add_file(full, sub, (u32)st.st_size);
    }
    closedir(d);
}
} // namespace
#endif

namespace {
void ensure_index()
{
    if (g_ready)
        return;
    g_ready = true;
    if (!g_root[0]) {
        std::printf("[PORT] DVD : aucun dossier de jeu (port_set_game_dir non appele)\n");
        return;
    }
    // Un disque extrait est sys/ + files/ ; le jeu adresse tout relativement a files/.
    char sub[512];
    std::snprintf(sub, sizeof(sub), "%s/files", g_root);
    scan(sub, "");
    if (g_count == 0)
        scan(g_root, "");
    std::printf("[PORT] DVD : %d fichiers indexes sous %s\n", g_count, g_root);
}
} // namespace

// ---------------------------------------------------------------- API DVD

s32 DVDConvertPathToEntrynum(const char *pathPtr)
{
    ensure_index();
    char want[MAX_PATH_LEN];
    normalise(pathPtr, want, MAX_PATH_LEN);
    for (int i = 0; i < g_count; ++i)
        if (!std::strcmp(g_files[i].disc, want))
            return (s32)i;
    std::printf("[PORT] DVD introuvable : %s\n", pathPtr ? pathPtr : "(null)");
    return -1;
}

BOOL DVDFastOpen(s32 entrynum, DVDFileInfo *fileInfo)
{
    ensure_index();
    if (entrynum < 0 || entrynum >= g_count || !fileInfo)
        return FALSE;
    std::memset(fileInfo, 0, sizeof(*fileInfo));
    fileInfo->startAddr = (u32)entrynum;   // on y range l'index, pas un LBA
    fileInfo->length = g_files[entrynum].size;
    return TRUE;
}

BOOL DVDOpen(const char *fileName, DVDFileInfo *fileInfo)
{
    const s32 e = DVDConvertPathToEntrynum(fileName);
    return e < 0 ? FALSE : DVDFastOpen(e, fileInfo);
}

BOOL DVDClose(DVDFileInfo *fileInfo)
{
    if (fileInfo)
        fileInfo->length = 0;
    return TRUE;
}

BOOL DVDReadAsyncPrio(DVDFileInfo *fileInfo, void *addr, s32 length, s32 offset,
                      DVDCallback callback, s32 prio)
{
    (void)prio;
    ensure_index();
    if (!fileInfo || !addr || length <= 0)
        return FALSE;
    const u32 idx = fileInfo->startAddr;
    if ((int)idx >= g_count)
        return FALSE;

    s32 got = 0;
    if (std::FILE *f = std::fopen(g_files[idx].host, "rb")) {
        std::fseek(f, offset, SEEK_SET);
        got = (s32)std::fread(addr, 1, (size_t)length, f);
        std::fclose(f);
    }
    fileInfo->cb.transferredSize = (u32)got;
    fileInfo->cb.state = 0;
    // Synchrone puis callback immediat : la semantique est juste, seul le rythme est faux.
    if (callback)
        callback(got, fileInfo);
    return TRUE;
}

s32 DVDGetDriveStatus(void) { return 0; }
s32 DVDGetTransferredSize(DVDFileInfo *fileInfo) { return fileInfo ? (s32)fileInfo->cb.transferredSize : 0; }
s32 DVDCancelAll(void) { return 0; }
