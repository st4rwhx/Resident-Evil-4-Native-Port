#include "types.h"
#include "global.h"
#include "main_mem.h"
#include "scheduler.h"
#include "joy.h"
#include "eprintf.h"
#include "cString.h"
#include "sofdec.h"
#include "dvd.h"
#include "t_util.h"
#include "snd_test.h"

// Movie / sound test REL entry (t_movie: this object carries the module's _prolog/_epilog/_unresolved,
// the movie player test and the sound test's task loop). No __FILE__ string: the real name is unknown.

#define _DOLPHIN_TYPES_H_
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#define ATTRIBUTE_ALIGN(num) __attribute__((aligned(num)))
#include <dolphin/dvd.h>

extern "C" {
void OSReport(const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
char* strcpy(char* dst, const char* src);
int strcmp(const char* a, const char* b);
char* strrchr(const char* s, int c);
}

// newlib ctype.h
extern "C" const char _ctype_[];
#define _L 02
#define islower(c) ((_ctype_ + 1)[(int) (c)] & _L)

extern void (*_ctors[])(void);
extern void (*_dtors[])(void);
extern int DebugMenuSelected;

void ToolSndVolEdit();
void ToolSeAt();
void MovieTest();
void SoundTest();
int file_search(const char* dir, const char* ext);

// One disc directory entry found by file_search.
struct MovieFile {
    char name[0x20];
    int x20;
};

struct MovieTestWork {
    s16 routine;  // 0x00  movie_test_tbl index
    s16 step;     // 0x02
    int nFiles;   // 0x04  file_search result
    int cursor;   // 0x08
    int quit;     // 0x0C
    int x10;
    int flag;     // 0x14  init: pG->flags_68 bit 0x40000000 backup; play: pause toggle
    int debugMode;  // 0x18  pG->debug_mode backup (restored as its low byte)
};

static void movie_test_init(MovieTestWork* w);
static void movie_test_main(MovieTestWork* w);
static void movie_test_exit(MovieTestWork* w);

static void (*movie_test_tbl[4])(MovieTestWork*) = {movie_test_init, movie_test_main, movie_test_exit, 0};

cString movie_name;  // global in the original (relocation fields hold the addend only)
static MovieFile movie_file[1024];

extern "C" void _prolog()
{
    void (**p)(void);

    for (p = _ctors; *p; p++) {
        (*p)();
    }
    OSReport("prolog...\n");
    switch (DebugMenuSelected) {
    case 4:
        MovieTest();
        break;
    case 11:
        SoundTest();
        break;
    case 23:
        ToolSndVolEdit();
        break;
    case 24:
        ToolSeAt();
        break;
    }
}

extern "C" void _epilog()
{
    void (**p)(void);

    for (p = _dtors; *p; p++) {
        (*p)();
    }
    OSReport("epilog...\n");
}

extern "C" void _unresolved()
{
    OSReport("unresolved...\n");
}

void MovieTest()
{
    MovieTestWork* w;

    w = (MovieTestWork*) Debug_alloc(sizeof(MovieTestWork), 1);
    if (w == 0) {
        TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x80000000;
        TaskExit();
    }
    memclr_asm(w, sizeof(MovieTestWork));
    do {
        movie_test_tbl[w->routine](w);
        TaskSleep(1);
    } while (w->quit == 0);
    Debug_free(w);
    TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x80000000;
    TaskSignal(0);
    TaskExit();
}

static void movie_test_init(MovieTestWork* w)
{
    int on = 1;

    w->routine++;
    if ((*(u32*) ((u8*) pGS + 0x68) & 0x40000000) == 0) {
        on = 0;
    }
    w->flag = on;
    TaskSuspend(0);
}

static void movie_test_main(MovieTestWork* w)
{
    char path[0x80];

    switch (w->step) {
    case 0:
        w->nFiles = file_search("movie", "H4M");
        w->step++;
    case 1:
        if (w->nFiles <= 0) {
            eprintf2(10, 16, 32, 112, 0, 0, "NO FILE.");
            if (Joy[0].trg & 0x300) {
                w->routine++;
            }
        } else {
            int i;
            int cursor;

            for (i = 0; i < w->nFiles; i++) {
                int col = (w->cursor == i) ? 6 : 0;
                MovieFile f;

                f = movie_file[i];
                eprintf2(8, 11, 32 + (i % 4) * 120, 112 + (i / 4) * 13, (u8) col, 0, "%s", f.name);
            }
            if (Joy[0].rep & 0x00020002) {
                w->cursor++;
            } else if (Joy[0].rep & 0x00010001) {
                w->cursor--;
            } else if (Joy[0].rep & 0x00040004) {
                w->cursor += 4;
            } else if (Joy[0].rep & 0x00080008) {
                w->cursor -= 4;
            } else if (Joy[0].trg & 0x100) {
                w->step++;
            } else if (Joy[0].trg & 0x200) {
                w->routine++;
            }
            cursor = w->cursor;
            if (cursor >= 0) {
                if (cursor > w->nFiles - 1) {
                    cursor = w->nFiles - 1;
                }
            } else {
                cursor = 0;
            }
            w->cursor = cursor;
        }
        break;
    case 2:
        movie_name = cString("MOVIE/");
        movie_name += movie_file[w->cursor].name;
        sprintf(path, "MOVIE/%s", movie_file[w->cursor].name);
        Sofdec.Initialize(movie_name, 0x200);
        w->debugMode = pG->debug_mode;
        w->flag = 1;
        w->step++;
        break;
    case 3:
        if (Sofdec.Move() == 1) {
            w->step = 0;
            pGS->debug_mode = (u8) w->debugMode;
        } else if (Joy[0].trg & 0x10) {
            int pause = 1;

            if (w->flag == 1) {
                pause = 0;
            }
            w->flag = pause;
            if (pause == 0) {
                pG->debug_mode = pause;
            } else {
                pG->debug_mode = (u8) w->debugMode;
            }
        }
        break;
    }
}

static void movie_test_exit(MovieTestWork* w)
{
    if (w->flag) {
        TOOL_FLAG(0x68) |= 0x40000000;
    }
    w->quit = 1;
}

// Lists the files of `dir` with the SFD extension (`ext` is not used) into movie_file[]; the count, or -1
// when the directory does not exist. Names are upper-cased in place.
int file_search(const char* dir, const char* ext)
{
    DVDDir d;
    DVDDirEntry ent;
    int n;
    int cmp;

    if (!DVDOpenDir(dir, &d)) {
        return -1;
    }
    n = 0;
    while (DVDReadDir(&d, &ent)) {
        if (ent.isDir) {
            continue;
        }
        {
            char* p = ent.name;
            while (*p) {
                int c = *p;
                if (islower(c)) {
                    c -= 0x20;
                }
                *p = c;
                p++;
            }
        }
        cmp = strcmp(strrchr(ent.name, '.') + 1, "SFD");
        if (cmp == 0) {
            strcpy(movie_file[n].name, ent.name);
            movie_file[n].x20 = cmp;
            n++;
        }
    }
    DVDCloseDir(&d);
    return n;
}

// Never called: the original object keeps three more strings after file_search's and a 16-byte .bss
// object behind movie_file[]; an unused inline reproduces both (its body is a guess).
static char snd_test_dir[16];

static inline int snd_test_file_search(int type)
{
    if (type == 0) {
        return file_search("se", "SND");
    }
    return file_search("bgm", snd_test_dir);
}

void SoundTest()
{
    TaskSuspend(0);
    for (;;) {
        TaskSleep(1);
        Snd_test_work.on = Joy[0].on;
        Snd_test_work.old = Joy[0].old;
        Snd_test_work.trg = Joy[0].trg;
        Snd_test_work.rep = Joy[0].rep;
        if (Snd_test_mode()) {
            break;
        }
    }
    TaskSignal(0);
    TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x80000000;
    TaskExit();
}

const char* Snd_test_get_str_name(int type)
{
    switch (type) {
    case 0:
        return FileTbl[1].name;
    case 1:
        return FileTbl[95].name;
    }
    return FileTbl[1].name;
}
