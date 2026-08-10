// game/option: pause/title option menu, result screens, chapter end screen (D:/Bio4/Prog/option.cpp).
#include "types.h"
#include "global.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "card.h"
#include "id_sys.h"
#include "mes.h"
#include "main.h"
#include "main_sub.h"
#include "pad.h"
#include "snd.h"
#include "cockpit.h"
#include "sscrn.h"
#include "sce.h"
#include "game.h"
#include "math_sub.h"
#include "option.h"

#define OPT_PTR(ofs) ((void*) (*(u32*) ((u8*) pG->pOptionData + (ofs)) + (u32) pG->pOptionData))
#define DATA_PTR(d, ofs) ((void*) (*(u32*) ((u8*) (d) + (ofs)) + (u32) (d)))

#define ID_OPT 0x2A
#define ID_OPT_BG 0x2B
#define ID_RESULT 0x28

#define KEY_START 0x2000
#define KEY_A 0x80000000
#define KEY_B 0x40000000
#define KEY_UP 0x01000000
#define KEY_DOWN 0x02000000
#define KEY_LEFT 0x08000000
#define KEY_RIGHT 0x04000000

extern "C" {
int top_menu(OptionScreen* o);
void back_to_top_menu(OptionScreen* o);
int retry_load_menu(OptionScreen* o);
int controller_menu(OptionScreen* o);
int brightness_menu(OptionScreen* o);
int audio_menu(OptionScreen* o);
void num(int val, int n, int mode, int base, u8 type, int reverse);
}

OptionScreen OptScrn;

static inline int isLang(u8 lang, int n)
{
    return lang == n;
}

static inline int isEurope(u8 lang)
{
    if (isLang(lang, 2) || isLang(lang, 3) || isLang(lang, 4) || isLang(lang, 5) || isLang(lang, 6)) {
        return 1;
    }
    return 0;
}

void setLangExt3(char* name)
{
    u8 lang = pSys->language;

    if (lang == 0) {
        name[0] = 'j';
        name[1] = 'p';
        name[2] = 'n';
    } else if (lang == 1) {
        name[0] = 'e';
        name[1] = 'n';
        name[2] = 'g';
    } else if (isLang(lang, 2)) {
        name[0] = 'e';
        name[1] = 'n';
        name[2] = 'g';
    } else if (isLang(lang, 3)) {
        name[0] = 'g';
        name[1] = 'e';
        name[2] = 'r';
    } else if (isLang(lang, 4)) {
        name[0] = 'f';
        name[1] = 'r';
        name[2] = 'a';
    } else if (isLang(lang, 5)) {
        name[0] = 'e';
        name[1] = 's';
        name[2] = 'p';
    } else if (isLang(lang, 6)) {
        name[0] = 'i';
        name[1] = 't';
        name[2] = 'a';
    } else if (lang == 7) {
        name[0] = 'e';
        name[1] = 'n';
        name[2] = 'g';
    } else if (isEurope(lang)) {
        name[0] = 'e';
        name[1] = 'n';
        name[2] = 'g';
    }
}

int OptionOpenCheck()
{
    if (SubScreenWk.wait > 0) {
        return 0;
    }
    u32 f = pG->flags_500C;
    if (f & 0x400) {
        return 0;
    }
    u32 t = f & 0x40;
    return t == 0;
}

void OptionScreen::init(int title)
{
    fromTitle = title;
    if (title != 0) {
        mesAttr = 0x94;
    } else {
        mesAttr = 0x91;
    }
    IdSys.dispSw(0x21, 0);
    IdSys.dispSw(0x20, 0);
    IdSys.dispSw(0x23, 0);
    IdTexDataLoad(OPT_PTR(0x20), 10);
    IdSys.set(OPT_PTR(0x24), 0xFF, ID_OPT_BG, 0x13, 4, 0);
    if (fromTitle != 0) {
        IdUnit* u = IdSys.unitPtr(0, ID_OPT_BG);
        Hermite1* h = u->curve[2];
        IdSys.setTimeS(u, (s16) (int) h->key[h->num - 1].t);
    }
    IdSys.set(OPT_PTR(0x28), 0xFF, ID_OPT, 0x13, 3, 0);
    mode = 0;
    cursor = 0;
    sub = 0;
    step = 0;
    if (pG->flags_5014 & 0x8000) {
        cursor = 1;
    }
    SndCall(0, 0x33, 0, 0, 0, 0);
}

int OptionScreen::move()
{
    int ret = 0;

    switch (mode) {
    case 0:
        ret = top_menu(this);
        break;
    case 1:
        switch (cursor) {
        case 0:
            retry_load_menu(this);
            break;
        case 1:
            controller_menu(this);
            break;
        case 2:
            brightness_menu(this);
            break;
        case 3:
            audio_menu(this);
            break;
        case 4:
            ret = 1;
            break;
        }
        break;
    }
    return ret;
}

void OptionScreen::quit()
{
    IdTexRelease(10);
    IdSys.kill(0xFF, ID_OPT_BG);
    IdSys.kill(0xFF, ID_OPT);
}

// Copies the highlight colour of the cursor unit into a menu item.
static inline void setColor(IdUnit* u, IdUnit* base)
{
    u->col0[0] = (u8) base->col[0];
    u->col0[1] = (u8) base->col[1];
    u->col0[2] = (u8) base->col[2];
    u->col0[3] = (u8) base->col[3];
}

int top_menu(OptionScreen* o)
{
    static int x0 = 100;
    static int y0 = 245;
    s8 old = o->cursor;
    IdUnit* base;
    IdUnit* u;
    int i;

    if (Key.trg & KEY_START) {
        return 1;
    }
    if (Key.trg & KEY_B) {
        if (old == 4) {
            o->mode = 1;
            o->sub = 0;
            o->step = 0;
            return 0;
        }
        o->cursor = 4;
        SndCall(0, 0x39, 0, 0, 0, 0);
    } else if (Key.trg & KEY_A) {
        void* data = 0;

        switch (old) {
        case 0:
            data = OPT_PTR(0x2C);
            break;
        case 1:
            data = OPT_PTR(0x30);
            break;
        case 2:
            data = OPT_PTR(0x34);
            break;
        case 3:
            data = OPT_PTR(0x38);
            break;
        }
        if (o->cursor != 4) {
            IdSys.kill(0xFF, ID_OPT);
            IdSys.set(data, 0xFF, ID_OPT, 0x13, 3, 0);
            SndCall(0, 0x36, 0, 0, 0, 0);
        }
        if (o->cursor == 2) {
            IdSys.unitPtr(0, ID_OPT_BG)->dir |= 0xF;
        }
        if (pSys->flags & 0x80000000) {
            o->keyA = 1;
        } else {
            o->keyA = 0;
        }
        if (pSys->flags & 0x08000000) {
            o->keyB = 1;
        } else {
            o->keyB = 0;
        }
        if (pSys->flags & 0x04000000) {
            o->keyC = 1;
        } else {
            o->keyC = 0;
        }
        switch (pSys->sound_mode) {
        case 0:
            o->sound = 1;
            break;
        case 1:
            o->sound = 0;
            break;
        case 2:
            o->sound = 2;
            break;
        default:
            o->sound = 0;
            break;
        }
        o->mode = 1;
        o->sub = 0;
        o->step = 0;
        if (o->cursor == 3) {
            o->sub = o->sound;
        }
        return 0;
    } else {
        if (Key.trg & KEY_UP) {
            o->cursor--;
        }
        if (Key.trg & KEY_DOWN) {
            o->cursor++;
        }
        o->cursor = o->cursor < 0 ? 0 : (o->cursor > 4 ? 4 : o->cursor);
        if ((pG->flags_5014 & 0x8000) && o->cursor == 0 && (Key.trg & KEY_UP)) {
            o->cursor = 1;
        }
        if (old != o->cursor) {
            SndCall(0, 0x34, 0, 0, 0, 0);
        }
    }
    base = IdSys.unitPtr(8, ID_OPT);
    if (old != o->cursor) {
        IdSys.setTime(base, 0);
    }
    for (i = 0; i < 5; i++) {
        u = IdSys.unitPtr((u8) (i + 2), ID_OPT);
        if (o->cursor == i) {
            setColor(u, base);
        } else {
            u->col0[3] = u->col0[2] = u->col0[1] = u->col0[0] = 0xFF;
        }
        if ((pG->flags_5014 & 0x8000) && i == 0) {
            u->col0[0] = 0x40;
            u->col0[1] = 0x40;
            u->col0[2] = 0x40;
            u->col0[3] = 0xFF;
        }
    }
    {
        u8 mes[5] = {0x81, 0x82, 0x83, 0x84, 0x85};

        cMes.MesSet(mes[o->cursor], x0, y0, o->mesAttr, 0, 0, 4);
    }
    return 0;
}

void back_to_top_menu(OptionScreen* o)
{
    IdSys.kill(0xFF, ID_OPT);
    IdSys.set(OPT_PTR(0x28), 0xFF, ID_OPT, 0x13, 3, 0);
    o->mode = 0;
    SndCall(0, 0x39, 0, 0, 0, 0);
}

int retry_load_menu(OptionScreen* o)
{
    static int yes = 0;
    static u32 snd_id = 0;
    register int old asm("r29") = o->sub;  // COMPILER-DIFF: o must outrank old for r31
    int confirm = 0;
    IdUnit* base;
    IdUnit* u;
    int i;

    switch (o->step) {
    case 0:
        if (Key.trg & KEY_B) {
            if (old == 3) {
                back_to_top_menu(o);
                return 0;
            }
            o->sub = 3;
            SndCall(0, 0x39, 0, 0, 0, 0);
        } else if (Key.trg & KEY_A) {
            switch (old) {
            case 0:
            case 2: {
                static int x0 = 100;
                static int y0 = 245;
                int no;

                if (o->sub == 0) {
                    no = 0x98;
                } else {
                    no = 0x8A;
                }
                cMes.MesSet(no, x0, y0, (o->mesAttr | 0x40) & ~0x80, 0, 0, 4);
                cMes.getWork()->cursor = 1;
                o->step = 1;
                confirm = 1;
                yes = 1;
                SndCall(0, 0x37, 0, 0, 0, 0);
                break;
            }
            case 1:
                o->step = 2;
                SndCall(0, 0x36, 0, 0, 0, 0);
                break;
            case 3:
                back_to_top_menu(o);
                return 0;
            }
        } else {
            if (Key.trg & KEY_UP) {
                o->sub--;
            }
            if (Key.trg & KEY_DOWN) {
                o->sub++;
            }
            o->sub = o->sub < 0 ? 0 : (o->sub > 3 ? 3 : o->sub);
            if ((s32) pG->flags_54 < 0 || (pG->flags_54 & 0x40000000)) {
                if (o->sub == 1) {
                    if (Key.trg & KEY_UP) {
                        o->sub = 0;
                    }
                    if (Key.trg & KEY_DOWN) {
                        o->sub = 2;
                    }
                }
            }
            if (old != o->sub) {
                SndCall(0, 0x34, 0, 0, 0, 0);
            }
        }
        base = IdSys.unitPtr(8, ID_OPT);
        if (old != o->sub) {
            IdSys.setTime(base, 0);
        }
        for (i = 0; i < 4; i++) {
            u = IdSys.unitPtr((u8) (i + 2), ID_OPT);
            if (o->sub == i) {
                setColor(u, base);
            } else {
                u->col0[3] = u->col0[2] = u->col0[1] = u->col0[0] = 0xFF;
            }
            if (((s32) pG->flags_54 < 0 || (pG->flags_54 & 0x40000000)) && i == 1) {
                u->col0[0] = 0x40;
                u->col0[1] = 0x40;
                u->col0[2] = 0x40;
                u->col0[3] = 0xFF;
            }
        }
        if (confirm == 0) {
            static int x0 = 100;
            static int y0 = 245;
            u8 mes[4] = {0x87, 0x88, 0x89, 0x86};

            cMes.MesSet(mes[o->sub], x0, y0, o->mesAttr, 0, 0, 4);
        }
        break;
    case 1:
        if (Key.trg & KEY_B) {
            o->step = 0;
            cMes.Delete(0);
            SndCall(0, 0x39, 0, 0, 0, 0);
        } else {
            s8 res = cMes.getWork()->result;

            if (res != 0) {
                cMes.Delete(0);
                if (res == 1) {
                    switch (o->sub) {
                    case 0:
                        GameContinue(1);
                        SndCall(0, 0x38, 0, 0, 0, 0);
                        break;
                    case 2:
                        snd_id = SndCall(0, 0x38, 0, 0, 0, 0);
                        o->step = 3;
                        break;
                    }
                } else {
                    o->step = 0;
                    SndCall(0, 0x39, 0, 0, 0, 0);
                }
            } else {
                if (Key.trg & KEY_LEFT) {
                    yes = 1;
                } else if (Key.trg & KEY_RIGHT) {
                    yes = 0;
                }
                if (Key.trg & (KEY_LEFT | KEY_RIGHT)) {
                    SndCall(0, 0x34, 0, 0, 0, 0);
                }
            }
        }
        break;
    case 2:
        if (CardLoad() == 1) {
            ScreenReSize(0x200, 0x1C0);
            GameContinue(1);
            GameLoad();
        } else {
            ScreenReSize(0x200, 0x1C0);
            if (o->fromTitle != 1) {
                Cckpt.roomInit();
                Cckpt.move();
                Cockpit* ck = &Cckpt;
                ck->life.fix(1);
                ck->lifeMeterDisp(0);
            }
            IdTexDataLoad(OPT_PTR(0x20), 10);
            IdSys.set(OPT_PTR(0x24), 0xFF, ID_OPT_BG, 0x13, 4, 0);
            {
                IdUnit* bg = IdSys.unitPtr(0, ID_OPT_BG);
                Hermite1* h = bg->curve[2];
                IdSys.setTimeS(bg, (s16) (int) h->key[h->num - 1].t);
            }
            IdSys.kill(0xFF, ID_OPT);
            IdSys.set(OPT_PTR(0x2C), 0xFF, ID_OPT, 0x13, 3, 0);
            o->step = 0;
        }
        break;
    case 3:
        if (SndEndCheck(snd_id)) {
            pG->flags_54 |= 0x04000000;
        }
        break;
    }
    return 0;
}

int controller_menu(OptionScreen* o)
{
    static int vib_time = 10;
    static int vib_level = 0xFF;
    static int x0 = 100;
    static int y0 = 245;
    int old = o->sub;
    IdUnit* base;
    IdUnit* u;
    IdUnit* off;
    int i;
    int j;

    if (Key.trg & KEY_B) {
        if (old == 3) {
            back_to_top_menu(o);
            return 0;
        }
        o->sub = 3;
        SndCall(0, 0x39, 0, 0, 0, 0);
    } else if (Key.trg & KEY_A) {
        switch (old) {
        case 0:
            if (o->keyA) {
                pSys->flags |= 0x80000000;
            } else {
                pSys->flags &= ~0x80000000;
            }
            break;
        case 1:
            if (o->keyB) {
                BitOn(pSys->flags, 0x08000000);
                VibSet(vib_time, vib_level, 0, 4);
            } else {
                pSys->flags &= ~0x08000000;
            }
            break;
        case 2:
            if (o->keyC) {
                pSys->flags |= 0x04000000;
            } else {
                pSys->flags &= ~0x04000000;
            }
            break;
        case 3:
            back_to_top_menu(o);
            return 0;
        }
        SndCall(0, 0x3A, 0, 0, 0, 0);
    } else {
        int no = old;
        s8 now;

        switch (no) {
        case 0:
            old = o->keyA;
            if (Key.trg & KEY_LEFT) {
                o->keyA = 1;
            }
            if (Key.trg & KEY_RIGHT) {
                o->keyA = 0;
            }
            now = o->keyA;
            break;
        case 1:
            old = o->keyB;
            if (Key.trg & KEY_LEFT) {
                o->keyB = 1;
            }
            if (Key.trg & KEY_RIGHT) {
                o->keyB = 0;
            }
            now = o->keyB;
            break;
        case 2:
            old = o->keyC;
            if (Key.trg & KEY_LEFT) {
                o->keyC = 0;
            }
            if (Key.trg & KEY_RIGHT) {
                o->keyC = 1;
            }
            now = o->keyC;
            break;
        default:
            goto updown;
        }
        if (old != now) {
            SndCall(0, 0x35, 0, 0, 0, 0);
        } else {
        updown:
            old = o->sub;
            if (Key.trg & KEY_UP) {
                o->sub--;
            }
            if (Key.trg & KEY_DOWN) {
                o->sub++;
            }
            if (pG->x4FB8 != 0 && pG->x4FB8 != 4 && o->sub == 2) {
                if (Key.trg & KEY_UP) {
                    o->sub = 1;
                }
                if (Key.trg & KEY_DOWN) {
                    o->sub = 3;
                }
            }
            o->sub = o->sub < 0 ? 0 : (o->sub > 3 ? 3 : o->sub);
            if (old != o->sub) {
                SndCall(0, 0x34, 0, 0, 0, 0);
            }
        }
    }
    base = IdSys.unitPtr(8, ID_OPT);
    IdUnit* sel = 0;
    IdUnit* uns = 0;
    if (old != o->sub) {
        IdSys.setTime(base, 0);
    }
    for (i = 0; i < 4; i++) {
        u8 id = 0;

        switch (i) {
        case 0:
            id = 2;
            break;
        case 1:
            id = 5;
            break;
        case 2:
            id = 0x11;
            break;
        case 3:
            id = 9;
            break;
        }
        u = IdSys.unitPtr(id, ID_OPT);
        if (o->sub == i) {
            setColor(u, base);
        } else {
            u->col0[3] = u->col0[2] = u->col0[1] = u->col0[0] = 0xFF;
        }
        if (pG->x4FB8 != 0 && pG->x4FB8 != 4 && i == 2) {
            u->col0[0] = 0x40;
            u->col0[1] = 0x40;
            u->col0[2] = 0x40;
            u->col0[3] = 0xFF;
        }
    }
    off = IdSys.unitPtr(0xE, ID_OPT);
    for (j = 0; j < 3; j++) {
        switch (j) {
        case 0:
            if (o->keyA) {
                sel = IdSys.unitPtr(3, ID_OPT);
                uns = IdSys.unitPtr(4, ID_OPT);
            } else {
                uns = IdSys.unitPtr(3, ID_OPT);
                sel = IdSys.unitPtr(4, ID_OPT);
            }
            break;
        case 1:
            if (o->keyB) {
                sel = IdSys.unitPtr(6, ID_OPT);
                uns = IdSys.unitPtr(7, ID_OPT);
            } else {
                uns = IdSys.unitPtr(6, ID_OPT);
                sel = IdSys.unitPtr(7, ID_OPT);
            }
            break;
        case 2:
            if (o->keyC) {
                uns = IdSys.unitPtr(0x12, ID_OPT);
                sel = IdSys.unitPtr(0x13, ID_OPT);
            } else {
                sel = IdSys.unitPtr(0x12, ID_OPT);
                uns = IdSys.unitPtr(0x13, ID_OPT);
            }
            break;
        }
        if (j == o->sub) {
            setColor(sel, base);
        } else {
            sel->col0[3] = sel->col0[2] = sel->col0[1] = sel->col0[0] = 0xFF;
        }
        uns->col0[0] = off->col0[0];
        uns->col0[1] = off->col0[1];
        uns->col0[2] = off->col0[2];
        uns->col0[3] = off->col0[3];
    }
    asm("" : : "r"(sel));  // COMPILER-DIFF: candidate (global.c allocno order: sel 29/338 must outrank o 42/600 for r31)
    if ((s32) pSys->flags < 0) {
        IdSys.unitPtr(0xA, ID_OPT)->flags |= 8;
        IdSys.unitPtr(0xB, ID_OPT)->flags &= ~8;
    } else {
        IdSys.unitPtr(0xA, ID_OPT)->flags &= ~8;
        IdSys.unitPtr(0xB, ID_OPT)->flags |= 8;
    }
    if (pSys->flags & 0x08000000) {
        IdSys.unitPtr(0xC, ID_OPT)->flags |= 8;
        IdSys.unitPtr(0xD, ID_OPT)->flags &= ~8;
    } else {
        IdSys.unitPtr(0xC, ID_OPT)->flags &= ~8;
        IdSys.unitPtr(0xD, ID_OPT)->flags |= 8;
    }
    if (pSys->flags & 0x04000000) {
        IdSys.unitPtr(0x14, ID_OPT)->flags &= ~8;
        IdSys.unitPtr(0x15, ID_OPT)->flags |= 8;
    } else {
        IdSys.unitPtr(0x14, ID_OPT)->flags |= 8;
        IdSys.unitPtr(0x15, ID_OPT)->flags &= ~8;
    }
    {
        u8 mes[4] = {0x8B, 0x8C, 0x92, 0x86};

        cMes.MesSet(mes[o->sub], x0, y0, o->mesAttr, 0, 0, 4);
    }
    return 0;
}

int brightness_menu(OptionScreen* o)
{
    static int DEFAULT = 0x40;
    static int MIN_OFS = -30;
    static int MAX_OFS = 50;
    static int x0 = 100;
    static int y0 = 245;
    s8 old = o->sub;
    IdUnit* base;
    IdUnit* u;
    int level;
    int digits;
    int i;
    int j;
    Vec a;
    Vec b;
    Vec c;
    f32 rate;

    if (Key.trg & KEY_B) {
        if (old == 1) {
            back_to_top_menu(o);
            IdSys.unitPtr(0, ID_OPT_BG)->dir &= ~0xF;
            return 0;
        }
        o->sub = 1;
        SndCall(0, 0x39, 0, 0, 0, 0);
    } else if (Key.trg & KEY_A) {
        switch (old) {
        case 0:
            break;
        case 1:
            back_to_top_menu(o);
            IdSys.unitPtr(0, ID_OPT_BG)->dir &= ~0xF;
            return 0;
        }
    } else {
        if (old == 0) {
            u8 bright = pSys->brightness;
            int n;

            if (Key.rep2 & KEY_LEFT) {
                pSys->brightness--;
            }
            if (Key.rep2 & KEY_RIGHT) {
                pSys->brightness++;
            }
            {
                // COMPILER-DIFF: candidate (global alloc order): pSys must be allocated after DEFAULT
                // (target r10/r11; ours has pSys 4 refs/18 = 0.444 > DEFAULT 3/9 = 0.333).
                register SystemWork* s asm("r10") = pSys;

                if (s->brightness < DEFAULT + MIN_OFS) {
                    // COMPILER-DIFF: candidate (local-alloc qty order): the byte-narrowed DEFAULT must take
                    // r9 (D dies into the sum) so the two `stb r9,0xa(r10)` tails cross-jump.
                    register u8 d asm("r9") = DEFAULT;
                    s->brightness = d + MIN_OFS;
                } else {
                    n = s->brightness;
                    if (n > DEFAULT + MAX_OFS) {
                        n = DEFAULT + MAX_OFS;
                    }
                    s->brightness = n;
                }
            }
            pRK->brightness = pSys->brightness;
            if (bright != pSys->brightness) {
                SndCall(0, 0x3B, 0, 0, 0, 0);
            }
        }
        {
            old = o->sub;
            if (Key.trg & KEY_UP) {
                o->sub--;
            }
            if (Key.trg & KEY_DOWN) {
                o->sub++;
            }
            o->sub = o->sub < 0 ? 0 : (o->sub > 1 ? 1 : o->sub);
            if (old != o->sub) {
                SndCall(0, 0x34, 0, 0, 0, 0);
            }
        }
    }
    level = pSys->brightness - DEFAULT;
    base = IdSys.unitPtr(8, ID_OPT);
    if (old != o->sub) {
        IdSys.setTime(base, 0);
    }
    digits = (int) fabsf((f32) level);
    if (level == 0) {
        IdSys.unitPtr(4, ID_OPT)->flags &= ~8;
        IdSys.unitPtr(5, ID_OPT)->flags &= ~8;
    } else if (level > 0) {
        IdSys.unitPtr(4, ID_OPT)->flags &= ~8;
        IdSys.unitPtr(5, ID_OPT)->flags |= 8;
    } else if (level < 0) {
        IdSys.unitPtr(4, ID_OPT)->flags |= 8;
        IdSys.unitPtr(5, ID_OPT)->flags &= ~8;
    }
    for (i = 0; i < 2; i++) {
        u = IdSys.unitPtr((u8) (i + 4), ID_OPT);
        if (o->sub == 0) {
            setColor(u, base);
        } else {
            u->col0[0] = 0xFF;
            u->col0[1] = 0xFF;
            u->col0[2] = 0xFF;
            u->col0[3] = 0xFF;
        }
    }
    for (j = 0; j < 2; j++) {  // its own counter: a shared `i` outranks o/digits for r29
        int d = digits % 10;

        digits /= 10;
        u = IdSys.unitPtr((u8) (3 - j), ID_OPT);
        u->no = d;
        u->flags_7F |= 2;
        if (o->sub == 0) {
            setColor(u, base);
        } else {
            u->col0[0] = 0xFF;
            u->col0[1] = 0xFF;
            u->col0[2] = 0xFF;
            u->col0[3] = 0xFF;
        }
    }
    a = IdSys.unitPtr(9, ID_OPT)->scr;
    b = IdSys.unitPtr(0x10, ID_OPT)->scr;
    rate = (f32) (pSys->brightness - (DEFAULT + MIN_OFS)) / (f32) (MAX_OFS - MIN_OFS);
    PSVECSubtract(&b, &a, &c);
    PSVECScale(&c, &c, rate);
    PSVECAdd(&a, &c, &IdSys.unitPtr(1, ID_OPT)->scr);
    u = IdSys.unitPtr(6, ID_OPT);
    if (o->sub == 1) {
        setColor(u, base);
    } else {
        u->col0[0] = 0xFF;
        u->col0[1] = 0xFF;
        u->col0[2] = 0xFF;
        u->col0[3] = 0xFF;
    }
    {
        u8 mes[2] = {0x8D, 0x86};

        cMes.MesSet(mes[o->sub], x0, y0, o->mesAttr, 0, 0, 4);
    }
    return 0;
}

int audio_menu(OptionScreen* o)
{
    static int x0 = 100;
    static int y0 = 245;
    s8 old = o->sub;
    IdUnit* base;
    IdUnit* cur;
    IdUnit* u;
    int i;

    if (Key.trg & KEY_B) {
        if (old == 3) {
            back_to_top_menu(o);
            return 0;
        }
        o->sub = 3;
        SndCall(0, 0x39, 0, 0, 0, 0);
    } else if (Key.trg & KEY_A) {
        switch (old) {
        case 0:
            SndSetOutputMode(1, 0);
            break;
        case 1:
            SndSetOutputMode(0, 0);
            break;
        case 2:
            SndSetOutputMode(2, 0);
            break;
        case 3:
            back_to_top_menu(o);
            return 0;
        }
        if (o->sub != 3) {
            o->sound = o->sub;
        }
        SndCall(0, 0x3A, 0, 0, 0, 0);
    } else {
        old = o->sub;
        if (Key.trg & KEY_UP) {
            o->sub--;
        }
        if (Key.trg & KEY_DOWN) {
            o->sub++;
        }
        o->sub = o->sub < 0 ? 0 : (o->sub > 3 ? 3 : o->sub);
        if (old != o->sub) {
            SndCall(0, 0x34, 0, 0, 0, 0);
        }
    }
    base = IdSys.unitPtr(8, ID_OPT);
    cur = IdSys.unitPtr(0xE, ID_OPT);
    if (old != o->sub) {
        IdSys.setTime(base, 0);
    }
    for (i = 0; i < 4; i++) {
        u = IdSys.unitPtr((u8) (i + 2), ID_OPT);
        if (o->sub == i) {
            setColor(u, base);
        } else if (i == 3 || i == o->sound) {
            u->col0[0] = 0xFF;
            u->col0[1] = 0xFF;
            u->col0[2] = 0xFF;
            u->col0[3] = 0xFF;
        } else {
            u->col0[0] = cur->col0[0];
            u->col0[1] = cur->col0[1];
            u->col0[2] = cur->col0[2];
            u->col0[3] = cur->col0[3];
        }
    }
    IdSys.unitPtr(0xA, ID_OPT)->flags &= ~8;
    IdSys.unitPtr(0xB, ID_OPT)->flags &= ~8;
    IdSys.unitPtr(0xC, ID_OPT)->flags &= ~8;
    switch (pSys->sound_mode) {
    case 1:
        u = IdSys.unitPtr(0xA, ID_OPT);
        break;
    case 0:
        u = IdSys.unitPtr(0xB, ID_OPT);
        break;
    case 2:
        u = IdSys.unitPtr(0xC, ID_OPT);
        break;
    default:
        u = IdSys.unitPtr(0xA, ID_OPT);
        break;
    }
    u->flags |= 8;
    {
        u8 mes[4] = {0x8E, 0x8E, 0x8E, 0x86};

        cMes.MesSet(mes[o->sub], x0, y0, o->mesAttr, 0, 0, 4);
    }
    return 0;
}

// Shows `val` as `n` decimal digits on the id units base.. (reverse: base - i); mode 1 hides
// leading zeros.
void num(int val, int n, int mode, int base, u8 type, int reverse)
{
    u8 d[8];
    int show;
    int i;

    for (int j = 0; j < n; j++) {
        d[j] = val % 10;
        val /= 10;
    }
    show = 1;
    if (mode == 1) {
        show = 0;
    }
    for (i = n - 1; i >= 0; i--) {
        IdUnit* u;

        if (reverse == 0) {
            u = IdSys.unitPtr((u8) (base + i), type);
        } else {
            u = IdSys.unitPtr((u8) (base - i), type);
        }
        if (show == 0 && d[i] == 0 && i != 0) {
            u->flags &= ~8;
        } else {
            u->flags |= 8;
            show = 1;
            u->flags_7F |= 2;
            u->no = d[i];
        }
    }
}

void GameResult::init(void* d)
{
    data = d;
    IdTexRelease(4);
    IdSys.roomInit();
    IdTexDataLoad(DATA_PTR(data, 0x10), 7);
    IdSys.set(DATA_PTR(data, 0x14), 0xFF, ID_RESULT, 0x13, 6, 0);
    x4 = 0;
    x5 = 0;
    x6 = 0;
    x7 = 0;
}

int GameResult::move()
{
    u32 h;
    u32 m;
    u32 s;
    IdUnit* u;
    int hit;

    if (pG->shotTotal2 != 0) {
        hit = (int) ((f32) pG->shotHit2 * 100.0f / (f32) pG->shotTotal2 + 0.5f);
    } else {
        hit = 0;
    }
    num(hit, 3, 1, 1, ID_RESULT, 0);
    num(pG->em_die_cnt2, 4, 1, 0x11, ID_RESULT, 0);
    num(pG->x833A, 3, 1, 0x21, ID_RESULT, 0);
    SecToTime(pG->play_time, &h, &m, &s);
    num(h, 2, 0, 0x35, ID_RESULT, 0);
    num(m, 2, 0, 0x33, ID_RESULT, 0);
    num(s, 2, 0, 0x31, ID_RESULT, 0);
    u = IdSys.unitPtr(0x30, ID_RESULT);
    u->no = 0xB;
    u->flags |= 8;
    u->flags_7F |= 2;
    u = IdSys.unitPtr(0, ID_RESULT);
    if (pG->x4F8E > 1) {
        u->flags |= 8;
    } else {
        u->flags &= ~8;
    }
    if (Key.trg & KEY_A) {
        return 1;
    }
    return 0;
}

void GameResult::quit()
{
    Cckpt.roomInit();
    Cckpt.move();
}

void GameResult::omake_init(void* d)
{
    data = d;
    IdTexRelease(4);
    IdSys.roomInit();
    IdTexDataLoad(DATA_PTR(data, 0x10), 7);
    IdSys.set(DATA_PTR(data, 0x18), 0xFF, ID_RESULT, 0x13, 6, 0);
}

int GameResult::omake_move()
{
    if (Key.trg & KEY_A) {
        return 1;
    }
    return 0;
}

void ChapterEnd::init(void* d, u8 ch)
{
    data = d;
    IdTexRelease(4);
    IdSys.roomInit();
    IdTexDataLoad(DATA_PTR(data, 0x10), 7);
    IdSys.set(DATA_PTR(data, 0x14), 0xFF, ID_RESULT, 0x13, 6, 0);
    chapter = ch;
}

int ChapterEnd::move()
{
    static u8 char_per = 0xA;
    static u8 char_bar = 0xB;
    // 16 unused frame bytes before the address-taken ints (0x18..0x28 in the original): an
    // aggregate local that is never referenced still takes its slot.
    Vec unused;
    int chap;
    int sec;
    int chap2;
    int sec2;
    IdUnit* u;
    int hit;

    getChapterSection(chapter, &chap, &sec);
    u = IdSys.unitPtr(0, ID_RESULT);
    u->flags_7F |= 2;
    u->no = chap;
    u = IdSys.unitPtr(1, ID_RESULT);
    u->flags_7F |= 2;
    u->no = char_bar;
    u = IdSys.unitPtr(2, ID_RESULT);
    u->flags_7F |= 2;
    u->no = sec;
    u = IdSys.unitPtr(0x1A, ID_RESULT);
    u->flags_7F |= 2;
    u->no = sec - 1;
    getChapterSection(chapter + 1, &chap2, &sec2);
    u = IdSys.unitPtr(3, ID_RESULT);
    u->flags_7F |= 2;
    u->no = chap2;
    u = IdSys.unitPtr(4, ID_RESULT);
    u->flags_7F |= 2;
    u->no = char_bar;
    u = IdSys.unitPtr(5, ID_RESULT);
    u->flags_7F |= 2;
    u->no = sec2;
    if (pG->shotTotal != 0) {
        hit = (int) ((f32) pG->shotHit * 100.0f / (f32) pG->shotTotal + 0.5f);
    } else {
        hit = 0;
    }
    num(hit, 3, 1, 8, ID_RESULT, 1);
    u = IdSys.unitPtr(9, ID_RESULT);
    u->flags_7F |= 2;
    u->no = char_per;
    if (pG->shotTotal2 != 0) {
        hit = (int) ((f32) pG->shotHit2 * 100.0f / (f32) pG->shotTotal2 + 0.5f);
    } else {
        hit = 0;
    }
    num(hit, 3, 1, 0xC, ID_RESULT, 1);
    u = IdSys.unitPtr(0xD, ID_RESULT);
    u->flags_7F |= 2;
    u->no = char_per;
    num(pG->em_die_cnt, 3, 1, 0x10, ID_RESULT, 1);
    num(pG->em_die_cnt2, 3, 1, 0x13, ID_RESULT, 1);
    num(pG->x8338, 3, 1, 0x16, ID_RESULT, 1);
    num(pG->x833A, 3, 1, 0x19, ID_RESULT, 1);
    return 0;
}

void ChapterEnd::quit()
{
    Cckpt.roomInit();
    Cckpt.move();
}
