#include "types.h"
#include "db_widget.h"

// t_esp REL: mouse/keyboard snapshots and the primitive/window array of the effect tool (file name
// unknown, "db_window.cpp").

DB_MOUSE::DB_MOUSE()
{
    u32 i;

    pos.x = 0.0f;
    pos.y = 0.0f;
    oldPos.x = 0.0f;
    oldPos.y = 0.0f;
    move.x = 0.0f;
    move.y = 0.0f;
    for (i = 0; i < 3; i++) {
        on[i] = 0;
        trg[i] = 0;
        click[i] = 0;
        up[i] = 0;
        dblClick[i] = 0;
        drag[i] = 0;
    }
    for (i = 0; i < 3; i++) {
        drag[i] = 0;
    }
    pos.y = pos.x = 0.0f;
    oldPos = pos;
}

DB_KEYBORD::DB_KEYBORD()
{
    ClearAllKey();
    cnt[0] = 1;
    cnt[1] = 1;
    cnt[2] = 1;
    cnt[3] = 1;
    cnt[4] = 1;
    cnt[5] = 1;
    cnt[6] = 1;
    cnt[7] = 1;
    cnt[8] = 1;
    cnt[9] = 1;
    cnt[10] = 1;
    cnt[11] = 1;
}

// key k (counter/repeat slot) reads pad flag n: k = n for up/down/left/right, n = k + 1 above
#define DB_KEY_UPDATE(k, n)                     \
    if (on[n]) {                                \
        if (cnt[k] == 0) {                      \
            trg[n] = 1;                         \
        }                                       \
        cnt[k]++;                               \
    } else {                                    \
        cnt[k] = 0;                             \
    }

#define DB_KEY_REPEAT(k, first, every)                  \
    if (cnt[k] == first || cnt[k] > every) {            \
        rep[k] = 1;                                     \
        cnt[k] = first;                                 \
    }

void DB_KEYBORD::Update()
{
    if (stickX > 0.77f) {
        on[3] = 1;
        stickRight = 1;
    }
    if (stickX < -0.77f) {
        on[2] = 1;
        stickLeft = 1;
    }
    if (stickY > 0.77f) {
        on[0] = 1;
        stickUp = 1;
    }
    if (stickY < -0.77f) {
        on[1] = 1;
        stickDown = 1;
    }
    DB_KEY_UPDATE(0, 0)
    DB_KEY_UPDATE(1, 1)
    DB_KEY_UPDATE(2, 2)
    DB_KEY_UPDATE(3, 3)
    DB_KEY_UPDATE(4, 5)
    DB_KEY_UPDATE(5, 6)
    DB_KEY_UPDATE(6, 7)
    DB_KEY_UPDATE(7, 8)
    DB_KEY_UPDATE(8, 9)
    DB_KEY_UPDATE(9, 10)
    DB_KEY_UPDATE(10, 11)
    DB_KEY_UPDATE(11, 12)
    if (trg[0]) rep[0] = 1;
    if (trg[1]) rep[1] = 1;
    if (trg[2]) rep[2] = 1;
    if (trg[3]) rep[3] = 1;
    if (trg[5]) rep[4] = 1;
    if (trg[6]) rep[5] = 1;
    if (trg[7]) rep[6] = 1;
    if (trg[8]) rep[7] = 1;
    if (trg[9]) rep[8] = 1;
    if (trg[10]) rep[9] = 1;
    if (trg[11]) rep[10] = 1;
    if (trg[12]) rep[11] = 1;
    DB_KEY_REPEAT(0, 6, 7)
    DB_KEY_REPEAT(1, 6, 7)
    DB_KEY_REPEAT(2, 6, 7)
    DB_KEY_REPEAT(3, 6, 7)
    DB_KEY_REPEAT(4, 6, 7)
    DB_KEY_REPEAT(5, 6, 7)
    DB_KEY_REPEAT(6, 6, 7)
    DB_KEY_REPEAT(7, 6, 7)
    DB_KEY_REPEAT(8, 6, 7)
    DB_KEY_REPEAT(9, 8, 10)
    DB_KEY_REPEAT(10, 8, 10)
    DB_KEY_REPEAT(11, 6, 7)
}

void DB_KEYBORD::ClearAllKey()
{
    x0 = 0;
    chr = 0;
    stickUp = 0;
    stickDown = 0;
    stickLeft = 0;
    stickRight = 0;
    on[0] = 0;
    on[1] = 0;
    on[2] = 0;
    on[3] = 0;
    on[4] = 0;
    on[5] = 0;
    on[6] = 0;
    on[7] = 0;
    on[8] = 0;
    on[9] = 0;
    on[10] = 0;
    on[11] = 0;
    on[12] = 0;
    trg[0] = 0;
    trg[1] = 0;
    trg[2] = 0;
    trg[3] = 0;
    trg[4] = 0;
    trg[5] = 0;
    trg[6] = 0;
    trg[7] = 0;
    trg[8] = 0;
    trg[9] = 0;
    trg[10] = 0;
    trg[11] = 0;
    trg[12] = 0;
    rep[0] = 0;
    rep[1] = 0;
    rep[2] = 0;
    rep[3] = 0;
    rep[4] = 0;
    rep[5] = 0;
    rep[6] = 0;
    rep[7] = 0;
    rep[8] = 0;
    rep[9] = 0;
    rep[10] = 0;
    rep[11] = 0;
    xC = stickY = stickX = 0.0f;
}

DB_PRIM_ARRAY::DB_PRIM_ARRAY()
{
    int i;
    u32 j;

    numPrim = 0;
    for (i = 0; i < 0x400; i++) {
        prim[i] = 0;
    }
    active = 0;
    numWin = 0;
    for (i = 0; i < 0x100; i++) {
        win[i] = 0;
    }
    activeWin = 0;
    oldActiveWin = 0;
    numPrim = 0;
    for (j = 0; j < 0x400; j++) {
        prim[j] = 0;
    }
    active = 0;
    activeWin = 0;
    oldActiveWin = 0;
}

DB_PRIM_ARRAY::~DB_PRIM_ARRAY()
{
    u32 i;

    for (i = 0; i < 0x400; i++) {
        if (prim[i]) {
            delete prim[i];
            prim[i] = 0;
        }
    }
}

int DB_PRIM_ARRAY::ChkMouseButton(DB_PRIMITIVE* p, DB_MOUSE m, int btn)
{
    int ret = 0;
    DB_POINT pt;

    pt = m.pos;
    if (m.dblClick[btn]) {
        p->ChkDoubleClick(&pt, btn);
    }
    if (m.up[btn]) {
        p->ChkMouseUp(&pt, btn);
    }
    if (m.click[btn]) {
        ret = p->ChkClick(&pt, btn);
    }
    return ret;
}

int DB_PRIM_ARRAY::RegistPrimitive(DB_PRIMITIVE* p)
{
    DB_PRIMITIVE** slot;

    int ret;

    slot = PullPrimitivePtr();
    if (slot == 0) {
        ret = 0;
    } else {
        *slot = p;
        ret = 1;
        numPrim++;
    }
    return ret;
}

int DB_PRIM_ARRAY::DeletePrimitive(DB_PRIMITIVE* p)
{
    u32 i;

    for (i = 0; i < 0x400; i++) {
        if (prim[i] == p) {
            prim[i] = 0;
            return 1;
        }
    }
    return 0;
}

int DB_PRIM_ARRAY::RegistWindow(DB_WINDOW* w)
{
    DB_WINDOW** slot;

    int ret;

    slot = PullWindowPtr();
    if (slot == 0) {
        ret = 0;
    } else {
        *slot = w;
        ret = 1;
        numWin++;
    }
    return ret;
}

DB_WINDOW* DB_PRIM_ARRAY::MakeWindowPrimitive()
{
    DB_WINDOW* w;

    w = new DB_WINDOW;
    if (RegistPrimitive(w)) {
        if (RegistWindow(w) == 0) {
            DeletePrimitive(w);
            delete w;
            w = 0;
        }
    } else {
        delete w;
        w = 0;
    }
    return w;
}

DB_WINDOW_TITLE* DB_PRIM_ARRAY::MakeWindowTitlePrimitive()
{
    DB_WINDOW_TITLE* w;

    w = new DB_WINDOW_TITLE("NoTitle");
    if (RegistPrimitive(w) == 0) {
        delete w;
        w = 0;
    }
    return w;
}

DB_BUTTON_CLOSE* DB_PRIM_ARRAY::MakeButtonClosePrimitive()
{
    DB_BUTTON_CLOSE* w;

    w = new DB_BUTTON_CLOSE;
    if (RegistPrimitive(w) == 0) {
        delete w;
        w = 0;
    }
    return w;
}

DB_STRING* DB_PRIM_ARRAY::MakeStringPrimitive()
{
    DB_STRING* w;

    w = new DB_STRING(255, "");
    if (RegistPrimitive(w) == 0) {
        delete w;
        w = 0;
    }
    return w;
}

DB_BUTTON* DB_PRIM_ARRAY::MakeButtonPrimitive()
{
    DB_BUTTON* w;

    w = new DB_BUTTON;
    if (RegistPrimitive(w) == 0) {
        delete w;
        w = 0;
    }
    return w;
}

DB_NUMERIC* DB_PRIM_ARRAY::MakeNumericPrimitive()
{
    DB_NUMERIC* w;

    w = new DB_NUMERIC;
    if (RegistPrimitive(w) == 0) {
        delete w;
        w = 0;
    }
    return w;
}

DB_NUMERIC2* DB_PRIM_ARRAY::MakeNumeric2Primitive()
{
    DB_NUMERIC2* w;

    w = new DB_NUMERIC2;
    if (RegistPrimitive(w) == 0) {
        delete w;
        w = 0;
    }
    return w;
}

DB_WINDOW* DB_PRIM_ARRAY::CreateNormalWindow(const char* title, DB_POINT* pos, f32* w, f32* h, u32* keyFlag)
{
    DB_WINDOW* win;

    win = MakeWindowPrimitive();
    if (win) {
        win->pos = *pos;
        win->SetSize(*w, *h);
        if (!(*keyFlag & DB_WIN_KEY_NO_CLOSE)) {
            DB_BUTTON_CLOSE* c;
            DB_POINT cp;

            c = MakeButtonClosePrimitive();
            cp.x = win->size.x - 12.0f;
            cp.y = -13.0f;
            c->pos = cp;
            win->AddChild(c);
        }
        if (!(*keyFlag & DB_WIN_KEY_NO_TITLE)) {
            DB_WINDOW_TITLE* t;
            DB_POINT tp;

            t = MakeWindowTitlePrimitive();
            tp.x = 0.0f;
            tp.y = -16.0f;
            t->pos = tp;
            t->SetSize(win->size.x, 16.0f);
            t->SetString(title);
            win->AddChild(t);
        }
        win->keyFlag = *keyFlag;
    }
    return win;
}

// never called (dead-stripped body; its range table survives in .rodata)
static f32 dbNumRangeOf(DB_NUMERIC* n, int hi)
{
    static const f32 tbl[7][2] = DB_NUM_RANGE_INIT;

    return tbl[n->numType][hi];
}

DB_STRING* DB_PRIM_ARRAY::CreateString(DB_PRIMITIVE* parent, const char* s, DB_POINT* pos)
{
    DB_STRING* p;

    p = MakeStringPrimitive();
    if (p) {
        p->SetString(s);
        p->pos = *pos;
        parent->AddChild(p);
    }
    return p;
}

#define DB_CREATE_NUMERIC(T)                                                                                    \
    DB_NUMERIC* DB_PRIM_ARRAY::CreateNumeric(DB_WINDOW* w, T* num, DB_POINT* pos, int* selX, int selY, u32 flg) \
    {                                                                                                           \
        DB_NUMERIC* p;                                                                                          \
                                                                                                                \
        p = MakeNumericPrimitive();                                                                             \
        if (p) {                                                                                                \
            p->SetNumPointer(num);                                                                              \
            p->pos = *pos;                                                                                      \
            p->SetNumFlg(flg);                                                                                  \
            w->AddSelectablePrimitive(p, *selX, selY);                                                          \
        }                                                                                                       \
        return p;                                                                                               \
    }

DB_CREATE_NUMERIC(s8)
DB_CREATE_NUMERIC(u8)
DB_CREATE_NUMERIC(u16)
DB_CREATE_NUMERIC(s32)
DB_CREATE_NUMERIC(u32)
DB_CREATE_NUMERIC(f32)

#define DB_CREATE_NUMERIC2(T)                                                                                            \
    DB_NUMERIC2* DB_PRIM_ARRAY::CreateNumeric2(DB_WINDOW* w, T* num, T* num2, DB_POINT* pos, int* selX, int selY, u32 flg) \
    {                                                                                                                    \
        DB_NUMERIC2* p;                                                                                                  \
                                                                                                                         \
        p = MakeNumeric2Primitive();                                                                                     \
        if (p) {                                                                                                         \
            p->SetNumPointer(num);                                                                                       \
            p->SetNumPointer2(num2);                                                                                     \
            p->pos = *pos;                                                                                               \
            p->SetNumFlg(flg);                                                                                           \
            w->AddSelectablePrimitive(p, *selX, selY);                                                                   \
        }                                                                                                                \
        return p;                                                                                                        \
    }

DB_CREATE_NUMERIC2(s8)
DB_CREATE_NUMERIC2(u8)
DB_CREATE_NUMERIC2(s16)
DB_CREATE_NUMERIC2(u16)
DB_CREATE_NUMERIC2(s32)
DB_CREATE_NUMERIC2(f32)

DB_BUTTON* DB_PRIM_ARRAY::CreateButton(DB_WINDOW* w, const char* s, DB_POINT* pos, DB_PRIM_CALLBACK cb, int* selX, int selY)
{
    DB_BUTTON* p;

    p = MakeButtonPrimitive();
    if (p) {
        p->pos = *pos;
        p->SetString(s);
        p->SetCallback(cb);
        w->AddSelectablePrimitive(p, *selX, selY);
    }
    return p;
}

DB_WINDOW* DB_PRIM_ARRAY::BringWindow(DB_WINDOW* w)
{
    u32 i, j;

    for (i = 0; i < 0x100; i++) {
        if (i != 0 && win[i] == w) {
            for (j = i; j != 0; j--) {
                win[j] = win[j - 1];
            }
            win[0] = w;
            break;
        }
    }
    return w;
}

DB_PRIMITIVE** DB_PRIM_ARRAY::PullPrimitivePtr()
{
    DB_PRIMITIVE** slot = 0;
    u32 i;

    for (i = 0; i < 0x400; i++) {
        if (prim[i] == 0) {
            slot = &prim[i];
            break;
        }
    }
    return slot;
}

DB_WINDOW** DB_PRIM_ARRAY::PullWindowPtr()
{
    DB_WINDOW** slot = 0;
    u32 i;

    for (i = 0; i < 0x100; i++) {
        if (win[i] == 0) {
            slot = &win[i];
            break;
        }
    }
    return slot;
}

void DB_PRIM_ARRAY::ClearAllActive()
{
    u32 i;

    for (i = 0; i < numPrim; i++) {
        if (prim[i]) {
            prim[i]->select = 0;
        }
    }
}

void DB_PRIM_ARRAY::ButtonUpdate(DB_MOUSE* m)
{
    u32 i;
    u32 btn;
    DB_WINDOW* w;
    DB_PRIMITIVE* p;
    DB_POINT mv;

    for (i = 0; i < numWin; i++) {
        w = win[i];
        if (w) {
            if (ChkMouseButton(w, *m, 0)) {
                BringWindow(w);
                break;
            }
        }
    }
    for (i = 0; i < numWin; i++) {
        w = win[i];
        if (w) {
            if (ChkMouseButton(w, *m, 1)) {
                break;
            }
        }
    }
    for (i = 0; i < numPrim; i++) {
        p = prim[i];
        mv = m->move;
        if (p) {
            for (btn = 0; btn <= 2; btn++) {
                if (m->up[btn]) {
                    p->click[btn] = 0;
                }
                p->ChkMouseDrag(&mv, btn);
            }
        }
    }
}

void DB_PRIM_ARRAY::SelectUpdate(DB_MOUSE* m)
{
    u32 i;
    DB_WINDOW* w;
    DB_POINT pt;

    for (i = 0; i < numPrim; i++) {
        if (prim[i]) {
            prim[i]->mouseOn = 0;
        }
    }
    if (m->on[0] == 0) {
        for (i = 0; i < numWin; i++) {
            w = win[i];
            if (w) {
                pt = m->pos;
                w->ChkMouseOn(&pt);
            }
        }
    }
}

void DB_PRIM_ARRAY::ActivePrimitiveUpdate()
{
    u32 i;
    DB_WINDOW* w = 0;
    DB_PRIMITIVE* p;

    active = 0;
    activeWin = 0;
    for (i = 0; i < numWin; i++) {
        w = win[i];
        if (w && w->bring) {
            w->bring = 0;
            BringWindow(w);
            w->active = 1;
        }
    }
    for (i = 0; i < numWin; i++) {
        w = win[i];
        if (w && w->active) {
            break;
        }
    }
    if (i != numWin) {
        activeWin = w;
        if (oldActiveWin != w) {
            ClearAllActive();
            oldActiveWin = activeWin;
        }
        w->select = 1;
        if (i != 0) {
            BringWindow(w);
        }
        for (i = 0; i < numPrim; i++) {
            p = prim[i];
            if (p && p->select && (p->flag & DB_PRIM_FLAG_SELECTABLE)) {
                if (active) {
                    exit(1);
                }
                active = p;
            }
        }
    }
}

void DB_PRIM_ARRAY::ActiveChangeKeybordNormal(DB_KEYBORD* k)
{
    DB_PRIMITIVE* old;
    int shift;

    old = active;
    activeWin->sel.SetActivePrimitive(old);
    shift = k->on[5];
    if (shift == 0) {
        if (k->rep[1]) {
            active = activeWin->sel.SetActiveDown();
        }
        if (k->rep[0]) {
            active = activeWin->sel.SetActiveUp();
        }
        if (k->rep[2]) {
            active = activeWin->sel.SetActiveLeft();
        }
        if (k->rep[3]) {
            active = activeWin->sel.SetActiveRight();
        }
        if (k->trg[4]) {
            active = activeWin->sel.SetActiveNext();
            if (old != active) {
                k->chr = 0;
            }
        }
    } else {
        if (k->rep[1]) {
            active->OnCalcMsg(DB_CALC_SUB_X10);
        }
        if (k->rep[0]) {
            active->OnCalcMsg(DB_CALC_ADD_X10);
        }
        if (k->rep[2]) {
            active->OnCalcMsg(DB_CALC_SUB);
        }
        if (k->rep[3]) {
            active->OnCalcMsg(DB_CALC_ADD);
        }
    }
    old->select = 0;
    active->select = 1;
}

void DB_PRIM_ARRAY::ActiveChangeKeybordMiniWin(DB_KEYBORD* k)
{
    DB_PRIMITIVE* old;
    DB_PRIMITIVE* p;

    old = active;
    activeWin->sel.SetActivePrimitive(old);
    if (k->rep[1]) {
        active = activeWin->sel.SetActiveDown();
    }
    if (k->rep[0]) {
        active = activeWin->sel.SetActiveUp();
    }
    if (k->on[7]) {
        if (k->rep[2]) {
            active = activeWin->sel.SetActiveLeft();
        }
        if (k->rep[3]) {
            active = activeWin->sel.SetActiveRight();
        }
    } else if (k->on[5]) {
        if (k->on[9]) {
            if (k->rep[2]) {
                active->OnCalcMsg(DB_CALC_SUB_X1000);
            }
            if (k->rep[3]) {
                active->OnCalcMsg(DB_CALC_ADD_X1000);
            }
            p = active;
            p->OnCalcMsgFloat(k->stickX * 2000.0f);
        } else if (k->on[8]) {
            if (k->rep[2]) {
                active->OnCalcMsg(DB_CALC_SUB_X100);
            }
            if (k->rep[3]) {
                active->OnCalcMsg(DB_CALC_ADD_X100);
            }
            p = active;
            p->OnCalcMsgFloat(k->stickX * 100.0f);
        } else {
            if (k->rep[2]) {
                active->OnCalcMsg(DB_CALC_SUB_X10);
            }
            if (k->rep[3]) {
                active->OnCalcMsg(DB_CALC_ADD_X10);
            }
            p = active;
            p->OnCalcMsgFloat(k->stickX * 10.0f);
        }
    } else if (k->on[9]) {
        if (k->rep[2]) {
            active->OnCalcMsg(DB_CALC_SUB_X1000);
        }
        if (k->rep[3]) {
            active->OnCalcMsg(DB_CALC_ADD_X1000);
        }
        p = active;
        p->OnCalcMsgFloat(k->stickX * 1000.0f);
    } else if (k->on[8]) {
        if (k->rep[2]) {
            active->OnCalcMsg(DB_CALC_SUB_X01);
        }
        if (k->rep[3]) {
            active->OnCalcMsg(DB_CALC_ADD_X01);
        }
        p = active;
        p->OnCalcMsgFloat(k->stickX * 0.1f);
    } else {
        if (k->rep[2]) {
            active->OnCalcMsg(DB_CALC_SUB);
        }
        if (k->rep[3]) {
            active->OnCalcMsg(DB_CALC_ADD);
        }
        active->OnCalcMsgFloat(k->stickX);
    }
    if (k->trg[4]) {
        active = activeWin->sel.SetActiveNext();
        if (old != active) {
            k->chr = 0;
        }
    }
    if (k->on[7] && k->trg[5]) {
        active->OnCalcMsg(DB_CALC_DEFAULT);
    }
    old->select = 0;
    active->select = 1;
}

void DB_PRIM_ARRAY::ActiveChangeKeybord(DB_KEYBORD* k)
{
    if (activeWin) {
        if (active) {
            if (activeWin->CallActiveChangeCallback(active, k) == 0) {
                switch (activeWin->sel.keyMode) {
                case 0:
                    ActiveChangeKeybordNormal(k);
                    break;
                case 1:
                    ActiveChangeKeybordMiniWin(k);
                    break;
                }
            }
        } else {
            active = activeWin->sel.GetActivePrimitive();
            if (active) {
                active->select = 1;
            }
        }
    }
}

void DB_PRIM_ARRAY::Update(DB_MOUSE* m, DB_KEYBORD* k)
{
    u32 i;
    DB_PRIMITIVE* p;

    if (m->click[0]) {
        ClearAllActive();
    }
    ButtonUpdate(m);
    SelectUpdate(m);
    ActivePrimitiveUpdate();
    ActiveChangeKeybord(k);
    if (activeWin) {
        activeWin->OnKeybord(k);
    }
    if (active) {
        if (k->trg[5]) {
            active->CallOnHitCallback();
        }
        active->OnKeybord(k);
    }
    for (i = 0; i < numPrim; i++) {
        p = prim[i];
        if (p) {
            p->CallUpdateCallback();
            p->Update();
        }
    }
}

void DB_PRIM_ARRAY::Draw()
{
    int i;

    for (i = numWin - 1; i >= 0; i--) {
        if (win[i]) {
            win[i]->DrawRequest();
        }
    }
}
