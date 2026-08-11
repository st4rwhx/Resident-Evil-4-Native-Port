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

// **Entrees-sorties POSIX, jamais <cstdio>.** Le decompile embarque la libc de ProDG
// (`src/game/fopen.c`, `fread.c`, ...) et ses symboles remplacent ceux de la plateforme pour
// tout le processus. Son fopen tient une table de quatre flux et repond "too many open
// streams" des le premier appel du portage - erreur observee, qui faisait echouer la lecture de
// l image disque sans rien dire d autre. `open`/`pread` ne sont pas redefinis, et `pread` porte
// sa position en argument : pas d etat partage, pas de seek a oublier.
#include <fcntl.h>

// Windows : sans O_BINARY, la libc traduit les fins de ligne et arrete la lecture sur un
// 0x1A. Sur une image disque, qui est du binaire pur, cela donne des lectures courtes
// silencieuses - ici l'en-tete de la FST revenait faux et le jeu ne trouvait aucun fichier.
#ifndef O_BINARY
#define O_BINARY 0
#endif
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dolphin.h>

namespace {

const int MAX_FILES = 8192;
const int MAX_PATH_LEN = 160;

struct Entry {
    char disc[MAX_PATH_LEN];   // chemin tel que le jeu le demande, minuscules, sans / initial
    char host[512];            // chemin reel sur la machine, vide si l entree vient d une image
    u32 offset;                // position dans l image disque
    u32 size;
};

Entry g_files[MAX_FILES];
int g_count = 0;
char g_root[512] = {0};
bool g_ready = false;

// Quand l utilisateur designe une **image disque** plutot qu un dossier extrait, on la garde
// ouverte et on lit dedans. C est le cas normal de l application mobile : elle demande son jeu
// et recoit un fichier unique, qu il n y a aucune raison de deplier sur le telephone - cela
// doublerait la place occupee et prendrait plusieurs minutes.
int g_image = -1;

// Les 32 premiers octets du disque : identifiant du jeu, editeur, numero de disque, version.
// Le jeu les lit pour verifier qu il tourne sur le bon support et pour choisir son disque 2.
DVDDiskID g_disk_id;
bool g_disk_id_ok = false;

// La FST recopiee en MEM1, la ou la console la laisse apres le chargement. Le jeu se sert de
// son adresse pour calculer combien de memoire haute lui reste (cDvd::Init).
u8 *g_fst_in_mem1 = nullptr;
u32 g_fst_size = 0;

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
    std::snprintf(e.host, sizeof(e.host), "%s", host ? host : "");
    e.offset = 0;
    e.size = size;
    ++g_count;
}

void add_image_file(const char *rel, u32 offset, u32 size)
{
    if (g_count >= MAX_FILES)
        return;
    Entry &e = g_files[g_count];
    normalise(rel, e.disc, MAX_PATH_LEN);
    e.host[0] = 0;
    e.offset = offset;
    e.size = size;
    ++g_count;
}

u32 be32_at(const unsigned char *d)
{
    return ((u32) d[0] << 24) | ((u32) d[1] << 16) | ((u32) d[2] << 8) | (u32) d[3];
}


// MinGW n'a pas pread. On le recompose, parce que la version en deux temps est correcte ici :
// l'acces a l'image est mono-fil (nos OSThread sont des coroutines a jeton unique, voir
// port/thread.cpp), donc le decalage partage du descripteur ne peut pas etre deplace entre le
// positionnement et la lecture. Le decalage est pris en 64 bits : une image GameCube tient sur
// 32 bits de justesse, et rien ne justifie de compter dessus.
#if defined(_WIN32)
static ssize_t port_pread(int fd, void *buf, size_t n, long long off)
{
    if (::_lseeki64(fd, off, SEEK_SET) < 0)
        return -1;
    return (ssize_t) ::read(fd, buf, (unsigned int) n);
}
#else
static ssize_t port_pread(int fd, void *buf, size_t n, long long off)
{
    return ::pread(fd, buf, n, (off_t) off);
}
#endif

// Lit la FST d une image GameCube et en tire la liste des fichiers.
//
// La FST est un tableau d entrees de 12 octets suivi d un tas de chaines. Chaque entree dit si
// c est un fichier (position, taille) ou un dossier (indice du parent, indice de fin) ; les
// noms sont des decalages dans le tas. Les chemins se reconstruisent en suivant la profondeur,
// ce qui evite de tenir une pile de noms.
bool load_image(const char *path)
{
    const int f = ::open(path, O_RDONLY | O_BINARY);
    if (f < 0)
        return false;

    unsigned char hdr[8];
    if (port_pread(f, hdr, 8, 0x424) != 8) {
        ::close(f);
        return false;
    }
    const u32 fst_off = be32_at(hdr), fst_sz = be32_at(hdr + 4);
    if (fst_sz < 12 || fst_sz > 16u * 1024 * 1024) {
        ::close(f);
        return false;
    }
    unsigned char *fst = (unsigned char *) std::malloc(fst_sz);
    if (!fst) {
        ::close(f);
        return false;
    }
    if (port_pread(f, &g_disk_id, sizeof(g_disk_id), 0) == (ssize_t) sizeof(g_disk_id))
        g_disk_id_ok = true;
    if (port_pread(f, fst, fst_sz, (off_t) fst_off) != (ssize_t) fst_sz) {
        std::free(fst);
        ::close(f);
        return false;
    }

    const u32 nent = be32_at(fst + 8);
    if (nent == 0 || (unsigned long long) nent * 12 > fst_sz) {
        std::free(fst);
        ::close(f);
        return false;
    }
    const char *strtab = (const char *) (fst + nent * 12);
    const u32 strmax = fst_sz - nent * 12;

    // ends[d] est l indice ou se termine le dossier ouvert a la profondeur d : quand on
    // l atteint, on remonte. C est la seule information necessaire pour reconstituer les
    // chemins en une passe.
    char dirs[16][MAX_PATH_LEN];
    u32 ends[16];
    int depth = 0;
    dirs[0][0] = 0;
    ends[0] = nent;

    for (u32 i = 1; i < nent; ++i) {
        const unsigned char *e = fst + i * 12;
        while (depth > 0 && i >= ends[depth])
            --depth;
        const u32 noff = ((u32) e[1] << 16) | ((u32) e[2] << 8) | (u32) e[3];
        const char *name = (noff < strmax) ? (strtab + noff) : "";
        char full[MAX_PATH_LEN];
        if (dirs[depth][0])
            std::snprintf(full, sizeof(full), "%s/%s", dirs[depth], name);
        else
            std::snprintf(full, sizeof(full), "%s", name);

        if (e[0]) {
            if (depth < 15) {
                ++depth;
                std::snprintf(dirs[depth], MAX_PATH_LEN, "%s", full);
                ends[depth] = be32_at(e + 8);
            }
        } else {
            add_image_file(full, be32_at(e + 4), be32_at(e + 8));
        }
    }

    // La FST est recopiee en haut de MEM1, comme sur console : `cDvd::Init` lit son adresse
    // pour en deduire la memoire libre restante, et une adresse hors MEM1 y donnerait une
    // taille absurde. C est ce calcul qui donnait "FST Address = 0x0" au premier essai.
    {
        // PORT_MEM1 : sur PC, aurora alloue MEM1 ailleurs qu a 0x80000000 (voir
        // port/port_prelude.h). Ecrire a l adresse GameCube brute plantait ici meme.
        const u32 MEM1_TOP = (u32) PORT_MEM1(0x81800000u);
        const u32 sz = (fst_sz + 31u) & ~31u;
        g_fst_in_mem1 = (u8 *) (unsigned long) (MEM1_TOP - sz);
        g_fst_size = fst_sz;
        std::memcpy(g_fst_in_mem1, fst, fst_sz);
    }

    std::free(fst);
    g_image = f;
    return true;
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
    // Deux formes acceptees : une image disque (.iso / .gcm), ce que l utilisateur possede,
    // ou un dossier deja extrait (sys/ + files/), plus pratique pour travailler.
    if (load_image(g_root)) {
        std::printf("[PORT] DVD : %d fichiers dans l image %s\n", g_count, g_root);
        return;
    }
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
    const Entry &en = g_files[idx];
    if (en.host[0]) {
        const int f = ::open(en.host, O_RDONLY | O_BINARY);
        if (f >= 0) {
            const ssize_t n = port_pread(f, addr, (size_t) length, (off_t) offset);
            got = (n > 0) ? (s32) n : 0;
            ::close(f);
        }
    } else if (g_image >= 0) {
        // Depuis l image : la position du fichier plus le decalage demande.
        //
        // **On ne borne pas a la taille declaree du fichier**, et ce n'est pas une negligence.
        // Le lecteur du GameCube lit des secteurs bruts : une demande de 0x400 octets qui
        // depasse la fin logique du fichier rend quand meme 0x400 octets, pris sur le
        // remplissage qui suit. Le jeu compte dessus - `dvdread_callback` traite comme une
        // **erreur** toute lecture qui rend moins que demande, et le chargement des polices
        // tournait en boucle sur "DVD: Read Error!!!" tant que la couche rognait.
        if (offset >= 0) {
            const ssize_t r = port_pread(g_image, addr, (size_t) length,
                                      (off_t) (en.offset + (u32) offset));
            got = (r > 0) ? (s32) r : 0;
        }
    }
    fileInfo->cb.transferredSize = (u32)got;
    fileInfo->cb.state = 0;
    // Synchrone puis callback immediat : la semantique est juste, seul le rythme est faux.
    if (callback)
        callback(got, fileInfo);
    return TRUE;
}

s32 DVDGetDriveStatus(void) { return 0; }

DVDDiskID *DVDGetCurrentDiskID(void)
{
    ensure_index();
    if (!g_disk_id_ok) {
        // Plutot que de rendre un pointeur nul - que le jeu dereference aussitot -, on rend une
        // identite plausible. Le portage sans image reste ainsi debogable.
        std::memset(&g_disk_id, 0, sizeof(g_disk_id));
        std::memcpy(g_disk_id.gameName, "G4BE", 4);
        std::memcpy(g_disk_id.company, "08", 2);
        g_disk_id_ok = true;
    }
    return &g_disk_id;
}

void *DVDGetFSTLocation(void)
{
    ensure_index();
    return g_fst_in_mem1;
}

u32 DVDGetFSTSize(void) { ensure_index(); return g_fst_size; }
s32 DVDGetTransferredSize(DVDFileInfo *fileInfo) { return fileInfo ? (s32)fileInfo->cb.transferredSize : 0; }
s32 DVDCancelAll(void) { return 0; }
