// -*- coding: utf-8 -*-
// Couche DVD du portage : le lecteur GameCube devient le disque de l'utilisateur.
//
// L'application mobile demande son jeu au premier lancement ; `port_set_game_dir()` recoit ce
// chemin, et tout ce qui suit lit dedans. Aucune donnee du jeu n'est embarquee.
//
// Le jeu n'ouvre pas des fichiers par nom a chaque fois : il traduit une fois un chemin en
// **entrynum** (l'index dans la FST du disque) puis rouvre par ce numero, et une partie du code
// range ces numeros. On construit donc une table d'index stable, une seule fois, en parcourant
// l'arborescence extraite - l'ordre n'a pas a coincider avec la FST d'origine tant qu'il est
// le meme d'un appel a l'autre.
//
// Les lectures asynchrones sont faites **synchronement** puis leur callback est appele tout de
// suite. C'est faux pour le rythme (le jeu croit que le lecteur met du temps) mais juste pour
// la semantique, et cela evite d'avoir besoin de threads avant d'avoir un ordonnanceur.
#include "port_log.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <dolphin.h>

namespace {

struct Entry {
    std::string disc;   // chemin tel que le jeu le demande, en minuscules
    std::string host;   // chemin reel sur la machine
    u32 size;
};

std::vector<Entry> g_files;
std::string g_root;
bool g_ready = false;

std::string lower(std::string s)
{
    for (char &c : s)
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
    return s;
}

// Le jeu ecrit tantot "st1/r100.das", tantot "/st1/r100.das", tantot "St1/R100.DAS" : la FST
// du GameCube est insensible a la casse pour ce code, et les chemins y sont relatifs a files/.
std::string norm(const char *p)
{
    std::string s = p ? p : "";
    for (char &c : s)
        if (c == '\\')
            c = '/';
    size_t i = 0;
    while (i < s.size() && (s[i] == '/' || s[i] == '.'))
        ++i;
    return lower(s.substr(i));
}

void scan(const std::string &dir, const std::string &rel);

} // namespace

void port_set_game_dir(const char *dir)
{
    g_root = dir ? dir : "";
    for (char &c : g_root)
        if (c == '\\')
            c = '/';
    while (!g_root.empty() && g_root.back() == '/')
        g_root.pop_back();
    g_files.clear();
    g_ready = false;
}

// ---------------------------------------------------------------- index

#ifdef _WIN32
#include <windows.h>
namespace {
void scan(const std::string &dir, const std::string &rel)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "/*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        const std::string n = fd.cFileName;
        if (n == "." || n == "..")
            continue;
        const std::string sub = rel.empty() ? n : rel + "/" + n;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            scan(dir + "/" + n, sub);
        } else {
            Entry e;
            e.disc = lower(sub);
            e.host = dir + "/" + n;
            e.size = fd.nFileSizeLow;
            g_files.push_back(e);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
} // namespace
#else
#include <dirent.h>
#include <sys/stat.h>
namespace {
void scan(const std::string &dir, const std::string &rel)
{
    DIR *d = opendir(dir.c_str());
    if (!d)
        return;
    while (dirent *de = readdir(d)) {
        const std::string n = de->d_name;
        if (n == "." || n == "..")
            continue;
        const std::string full = dir + "/" + n;
        const std::string sub = rel.empty() ? n : rel + "/" + n;
        struct stat st;
        if (stat(full.c_str(), &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode)) {
            scan(full, sub);
        } else {
            Entry e;
            e.disc = lower(sub);
            e.host = full;
            e.size = (u32)st.st_size;
            g_files.push_back(e);
        }
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
    if (g_root.empty()) {
        std::printf("[PORT] DVD : aucun dossier de jeu (port_set_game_dir non appele)\n");
        return;
    }
    // Un disque extrait est sys/ + files/ ; le jeu adresse tout relativement a files/.
    scan(g_root + "/files", "");
    if (g_files.empty())
        scan(g_root, "");
    std::printf("[PORT] DVD : %zu fichiers indexes sous %s\n", g_files.size(), g_root.c_str());
}
} // namespace

// ---------------------------------------------------------------- API DVD

s32 DVDConvertPathToEntrynum(const char *pathPtr)
{
    ensure_index();
    const std::string want = norm(pathPtr);
    for (size_t i = 0; i < g_files.size(); ++i)
        if (g_files[i].disc == want)
            return (s32)i;
    std::printf("[PORT] DVD introuvable : %s\n", pathPtr ? pathPtr : "(null)");
    return -1;
}

BOOL DVDFastOpen(s32 entrynum, DVDFileInfo *fileInfo)
{
    ensure_index();
    if (entrynum < 0 || (size_t)entrynum >= g_files.size() || !fileInfo)
        return FALSE;
    std::memset(fileInfo, 0, sizeof(*fileInfo));
    fileInfo->startAddr = (u32)entrynum;     // on y range l'index, pas un LBA
    fileInfo->length = g_files[entrynum].size;
    fileInfo->cb.state = 0;
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
    if (idx >= g_files.size())
        return FALSE;

    s32 got = 0;
    if (std::FILE *f = std::fopen(g_files[idx].host.c_str(), "rb")) {
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
u32 DVDGetTransferredSize(DVDFileInfo *fileInfo) { return fileInfo ? fileInfo->cb.transferredSize : 0; }
s32 DVDGetCommandBlockStatus(const DVDCommandBlock *block) { (void)block; return 0; }
BOOL DVDCancelAll(void) { return TRUE; }
