#include "types.h"
#include "db_widget.h"

// t_esp REL: the window-system primitives of the effect tool (file name unknown, "db_widget.cpp").

int primIdCounter = 0;  // global in the original (.data+0x780 reloc fields are 0 in the REL; a static keeps the offset)
static char hexDigit[] = "0123456789ABCDEF";
static const f32 dbNumRange[7][2] = DB_NUM_RANGE_INIT;

int DB_RECT::ChkHitRect(DB_POINT* p)
{
    int hit = 0;

    if (p->x > x && p->x < x + w && p->y > y && p->y < y + h) {
        hit = 1;
    }
    return hit;
}

DB_PRIMITIVE::DB_PRIMITIVE()
{
    int i;
    // the first store block comes out in source order in the target: the rect fields go through a DB_RECT* (rect.x via
    // `this`, y/w/h via the pointer, like the SetBase temp), the two zeros are locals
    DB_RECT* r = &rect;
    f32 fz = 0.0f;
    int iz = 0;

    pos.x = fz;
    pos.y = fz;
    drawPos.x = fz;
    drawPos.y = fz;
    r->x = fz;
    r->y = fz;
    r->w = fz;
    r->h = fz;
    base.y = fz;
    flag = iz;
    size.x = fz;
    size.y = fz;
    base.x = fz;
    type = iz;
    parent = 0;
    child = 0;
    prev = 0;
    next = 0;
    id = iz;
    // COMPILER-DIFF: #13 -- the target issues base.x/id last (source order) although fz/iz die there; the keep-alive is
    // anchored on a different-mode view of the block's first store (an output dependence, no barrier)
    asm("" : "=m"(*(u32*) &pos.x) : "f"(fz), "r"(iz));
    for (i = 0; i < 3; i++) {
        click[i] = 0;
    }
    active = 0;
    mouseOn = 0;
    select = 0;
    onHitCb = 0;
    updateCb = 0;
    drawCb = 0;

    {
        // the id counter through a reference: its MEMs carry neither struct nor scalar flag, so the member stores
        // order the counter load/store (the target's zero stores lead the block and the pool loads follow them),
        // and the second `id = c` after the parent..next zeros survives cse as the target's `lwz counter; stw id`
        // re-read (a plain global read is forwarded from the store)
        int& c = primIdCounter;
        type = DB_PRIM_BASE;
        c += 0x10;
        id = c;
        parent = 0;
        child = 0;
        prev = 0;
        next = 0;
        id = c;
        rect = DB_RECT(0.0f, 0.0f, 10.0f, 10.0f);
        flag = 0;
    }
    SetSize(16.0f, 16.0f);
    SetBase(0.0f, 0.0f);
    active = 1;
    mouseOn = 0;
    select = 0;
    SetOnHitCallback(0);
    SetUpdateCallback(0);
    {
        // the target's loop stores through register offsets (`stwx r0,r9,{r0,r10,r11}` with `li r0,0; li r10,4;
        // li r11,8` hoisted): the indices are variables set before the loop (the body is a fresh cse ebb, so the
        // `j*4` products survive to loop.c, which hoists them; cse2 folds the hoisted products to constants), and
        // the stored zero is the index-0 product itself (one register for the value and the first offset)
        int j0 = 0;
        int j1 = 1;
        int j2 = 2;
        for (i = 0; i < 3; i++) {
            int z = j0 * 4;
            click[j0] = z;
            click[j1] = z;
            click[j2] = z;
        }
    }
}

DB_PRIMITIVE::~DB_PRIMITIVE()
{
}

void DB_PRIMITIVE::SetOnHitCallback(DB_PRIM_CALLBACK cb)
{
    onHitCb = cb;
}

void DB_PRIMITIVE::CallOnHitCallback()
{
    if (onHitCb) {
        onHitCb(this);
    }
}

void DB_PRIMITIVE::SetUpdateCallback(DB_PRIM_CALLBACK cb)
{
    updateCb = cb;
}

void DB_PRIMITIVE::CallUpdateCallback()
{
    if (updateCb) {
        updateCb(this);
    }
}

void DB_PRIMITIVE::SetDrawCallback(DB_PRIM_CALLBACK cb)
{
    drawCb = cb;
}

void DB_PRIMITIVE::CallDrawCallback()
{
    if (drawCb) {
        drawCb(this);
    }
}

// the .y store goes through a DB_POINT* (`(mem (plus p 4))`: cse1 does not forward it into the DB_RECT temp's
// re-read, the load survives to sched1 behind the store and reload_cse turns it into `fmr`); the .x store precedes
// the rect copy (its output dependence on the rect stores otherwise sinks it below them)
void DB_PRIMITIVE::SetSize(f32 w, f32 h)
{
    DB_POINT* s = &size;

    s->y = h;
    size.x = w;
    rect = DB_RECT(base.x, base.y, size.x, size.y);
}

void DB_PRIMITIVE::SetBase(f32 x, f32 y)
{
    DB_POINT* b = &base;

    b->y = y;
    base.x = x;
    rect = DB_RECT(base.x, base.y, size.x, size.y);
}

int DB_PRIMITIVE::AddChild(DB_PRIMITIVE* p)
{
    int ret = 1;

    if (child == 0) {
        child = p;
        p->parent = this;
    } else {
        ret = child->AddBrother(p);
    }
    return ret;
}

int DB_PRIMITIVE::AddBrother(DB_PRIMITIVE* p)
{
    int ret = 1;

    if (next == 0) {
        next = p;
        p->prev = this;
        p->parent = parent;
    } else {
        ret = next->AddBrother(p);
    }
    return ret;
}

void DB_PRIMITIVE::Update()
{
}

void DB_PRIMITIVE::DrawRequest()
{
    if (active) {
        if (parent) {
            drawPos.x = pos.x + parent->drawPos.x;
            drawPos.y = pos.y + parent->drawPos.y;
        } else {
            drawPos.x = pos.x;
            drawPos.y = pos.y;
        }
        Draw();
        CallDrawCallback();
        if (next) {
            next->DrawRequest();
        }
        if (child) {
            child->DrawRequest();
        }
    }
}

void DB_PRIMITIVE::Draw()
{
    if (select) {
        DB_DrawBox(drawPos.x + base.x, drawPos.y + base.y, 10.0f, 10.0f, 1.0f, 1.0f, 1.0f, 0.2f);
    } else {
        DB_DrawBox(drawPos.x + base.x, drawPos.y + base.y, 10.0f, 10.0f, 0.7f, 0.7f, 0.7f, 0.2f);
    }
}

int DB_PRIMITIVE::ChkClick(DB_POINT* p, int btn)
{
    int hit = 0;
    DB_POINT lp;

    lp.x = 0.0f;
    lp.y = 0.0f;
    lp.x = p->x - pos.x;
    lp.y = p->y - pos.y;
    if (active && rect.ChkHitRect(&lp)) {
        if (child) {
            hit = child->ChkClick(&lp, btn);
        }
        if (hit == 0) {
            hit = 1;
            if (btn == 0) {
                CallOnHitCallback();
            }
            OnClick(&lp, btn);
            click[btn] = hit;
            if (btn == 0 && (flag & DB_PRIM_FLAG_SELECTABLE)) {
                select = hit;
            }
        }
    }
    if (next) {
        hit = next->ChkClick(p, btn);
    }
    return hit;
}

int DB_PRIMITIVE::ChkDoubleClick(DB_POINT* p, int btn)
{
    int hit = 0;
    DB_POINT lp;

    lp.x = 0.0f;
    lp.y = 0.0f;
    lp.x = p->x - pos.x;
    lp.y = p->y - pos.y;
    if (active && rect.ChkHitRect(&lp)) {
        if (child) {
            hit = child->ChkDoubleClick(&lp, btn);
        }
        if (hit == 0) {
            hit = 1;
            OnDoubleClick(&lp, btn);
        }
    }
    if (next) {
        hit = next->ChkDoubleClick(p, btn);
    }
    return hit;
}

int DB_PRIMITIVE::ChkMouseUp(DB_POINT* p, int btn)
{
    int hit = 0;
    DB_POINT lp;

    lp.x = 0.0f;
    lp.y = 0.0f;
    lp.x = p->x - pos.x;
    lp.y = p->y - pos.y;
    if (active && rect.ChkHitRect(&lp)) {
        hit = 1;
        OnMouseUp(&lp, btn);
        if (child) {
            child->ChkMouseUp(&lp, btn);
        }
    }
    return hit;
}

int DB_PRIMITIVE::ChkMouseOn(DB_POINT* p)
{
    int hit = 0;
    DB_POINT lp;

    lp.x = 0.0f;
    lp.y = 0.0f;
    lp.x = p->x - pos.x;
    lp.y = p->y - pos.y;
    if (active && rect.ChkHitRect(&lp)) {
        if ((flag & DB_PRIM_FLAG_MOUSE_ON) && type != DB_PRIM_WINDOW) {
            mouseOn = 1;
            hit = 1;
        } else if (child) {
            child->ChkMouseOn(&lp);
        }
    }
    if (hit == 0 && next) {
        hit = next->ChkMouseOn(p);
    }
    return hit;
}

int DB_PRIMITIVE::ChkMouseDrag(DB_POINT* p, int btn)
{
    int hit = 0;

    if (click[btn]) {
        hit = 1;
        OnMouseDrag(p, btn);
    }
    return hit;
}

void DB_PRIMITIVE::OnClick(DB_POINT* p, int btn)
{
}

void DB_PRIMITIVE::OnDoubleClick(DB_POINT* p, int btn)
{
}

void DB_PRIMITIVE::OnMouseUp(DB_POINT* p, int btn)
{
}

void DB_PRIMITIVE::OnMouseDrag(DB_POINT* p, int btn)
{
}

void DB_PRIMITIVE::OnKeybord(DB_KEYBORD* key)
{
}

void DB_PRIMITIVE::OnCalcMsg(int msg)
{
}

void DB_PRIMITIVE::OnCalcMsgFloat(f32 v)
{
}

DB_ACTIVE_SELECT::DB_ACTIVE_SELECT()
{
    tbl = 0;
    w = 1;
    active = 0;
    num = 0;
    selX = 0;
    selY = 0;
    h = 1;
    keyMode = 0;
    tbl = new DB_PRIMITIVE*[1];
    memclr_asm(tbl, sizeof(DB_PRIMITIVE*));
}

DB_ACTIVE_SELECT::~DB_ACTIVE_SELECT()
{
    if (tbl) {
        delete[] tbl;
    }
}

void DB_ACTIVE_SELECT::SetSelX(u32 x)
{
    DB_PRIMITIVE* p;

    if (x >= w) {
        x = w - 1;
    }
    selX = x;
    p = tbl[w * selY + x];
    if (p) {
        active = p;
    }
}

void DB_ACTIVE_SELECT::SetSelY(u32 y)
{
    DB_PRIMITIVE* p;

    if (y >= h) {
        y = h - 1;
    }
    selY = y;
    p = tbl[w * y + selX];
    if (p) {
        active = p;
    }
}

int DB_ACTIVE_SELECT::AddPrimitive(DB_PRIMITIVE* p, int x, int y)
{
    int ret;

    if (x == -1 || y == -1) {
        return 0;
    }
    if ((u32) x >= w || (u32) y >= h) {
        u32 ow = w;
        u32 oh = h;
        DB_PRIMITIVE** ntbl;
        u32 sz;
        u32 i, j;
        DB_PRIMITIVE** otbl;

        if ((u32) x >= w) {
            w = x + 1;
        }
        if ((u32) y >= h) {
            h = y + 1;
        }
        sz = w * h * sizeof(DB_PRIMITIVE*);
        ntbl = new DB_PRIMITIVE*[w * h];
        memclr_asm(ntbl, sz);
        otbl = tbl;
        for (i = 0; i < oh; i++) {
            for (j = 0; j < ow; j++) {
                ntbl[w * i + j] = otbl[ow * i + j];
            }
        }
        tbl = ntbl;
        if (otbl) {
            delete[] otbl;
        }
    }
    // value-select return: the join's dead `mr r3,ret` becomes a (use r3) before the return label,
    // which keeps the first `return 0` copy from being cross-jumped into this one (see docs/matching.md #6)
    if (tbl[w * y + x] == 0) {
        tbl[w * y + x] = p;
        num++;
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}

DB_PRIMITIVE* DB_ACTIVE_SELECT::GetActivePrimitive()
{
    if (num == 0) {
        return 0;
    }
    if (active == 0) {
        active = tbl[0];
    }
    return active;
}

int DB_ACTIVE_SELECT::SetActivePrimitive(DB_PRIMITIVE* p)
{
    u32 i, j;

    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            if (tbl[w * i + j] == p) {
                active = p;
                selX = j;
                selY = i;
                return 1;
            }
        }
    }
    return 0;
}

DB_PRIMITIVE* DB_ACTIVE_SELECT::SetActiveUp()
{
    u32 x, y;
    DB_PRIMITIVE* p;

    if (active == 0) {
        return 0;
    }
    x = selX;
    y = selY - 1;
    for (;;) {
        if (y >= h) {
            y = h - 1;
        }
        p = tbl[w * y + x];
        if (p) {
            return p;
        }
        y--;
        selX = x;
        selY = y;
    }
}

DB_PRIMITIVE* DB_ACTIVE_SELECT::SetActiveDown()
{
    u32 x, y;
    DB_PRIMITIVE* p;

    if (active == 0) {
        return 0;
    }
    x = selX;
    y = selY + 1;
    for (;;) {
        if (y >= h) {
            y = 0;
        }
        p = tbl[w * y + x];
        if (p) {
            return p;
        }
        y++;
        selX = x;
        selY = y;
    }
}

DB_PRIMITIVE* DB_ACTIVE_SELECT::SetActiveLeft()
{
    u32 x, y;
    DB_PRIMITIVE* p;

    if (active == 0) {
        return 0;
    }
    x = selX - 1;
    y = selY;
    for (;;) {
        if (x >= w) {
            x = w - 1;
        }
        p = tbl[w * y + x];
        if (p) {
            return p;
        }
        x--;
        selY = y;
        selX = x;
    }
}

DB_PRIMITIVE* DB_ACTIVE_SELECT::SetActiveRight()
{
    u32 x, y;
    DB_PRIMITIVE* p;

    if (active == 0) {
        return 0;
    }
    x = selX + 1;
    y = selY;
    for (;;) {
        if (x >= w) {
            x = 0;
        }
        p = tbl[w * y + x];
        if (p) {
            return p;
        }
        x++;
        selY = y;
        selX = x;
    }
}

DB_PRIMITIVE* DB_ACTIVE_SELECT::SetActiveNext()
{
    u32 x, y;
    DB_PRIMITIVE* p;

    p = active;
    if (p == 0) {
        return 0;
    }
    if (p->type == DB_PRIM_NUMERIC && ((DB_NUMERIC*) p)->edit) {
        return p;
    }
    x = selX;
    y = selY + 1;
    for (;;) {
        if (y >= h) {
            y = 0;
        }
        p = tbl[w * y + x];
        if (p) {
            return p;
        }
        y++;
        selX = x;
        selY = y;
    }
}

DB_PRIMITIVE* DB_ACTIVE_SELECT::SetActiveDefault()
{
    if (active == 0) {
        return 0;
    }
    selX = 0;
    selY = 0;
    active = tbl[0];
    return active;
}

// The colour temp is built inside an inline taking the destination by pointer: the block copy's
// loads then stay frame-relative (`lwz 8..20(r1)`) while the ctor stores go through the temp's
// `this` (`addi r9,r1,8`); written as a member assignment cse rewrites the copy's `fp+12` into
// `this+4`. The temp's store order a, b, g is sched1's register-pressure rank of the ctor's RTL
// order r, g, b, a: g +1, b 0 (the 0.1 pseudo dies), a -1 (the 0.3 pseudo and the temp's `this` die).
static inline void DB_ColorSet(DB_COLOR* c, f32 r, f32 g, f32 b, f32 a)
{
    *c = DB_COLOR(r, g, b, a);
}

// `color(1.0f)`: the 1.0 is expanded in this function (an unchanging pool MEM) so its `lfs` is
// free to issue before the vptr store; the inlined default ctor's own constant load is not
// RTX_UNCHANGING_P after integrate and waits for the store.
DB_WINDOW::DB_WINDOW() : color(1.0f)
{
    keyFlag = 0;
    bring = 0;
    closeCb = 0;
    activeChangeCb = 0;
    type = DB_PRIM_WINDOW;
    SetSize(140.0f, 100.0f);
    SetBase(0.0f, -16.0f);
    DB_ColorSet(&color, 0.1f, 0.1f, 0.1f, 0.3f);
    keyFlag = 0;
    bring = 0;
    SetCloseCallback(0);
    SetActiveChangeCallback(0);
}

int DB_WINDOW::CallActiveChangeCallback(DB_PRIMITIVE* p, DB_KEYBORD* k)
{
    // COMPILER-DIFF: candidate #17 (the original allocates `ret` to the return register first and copies
    // `this` to r9; ours gives `this` r3 and `ret` r9 -- no plain form flips the order)
    register int ret asm("r3");
    ret = 0;

    if (activeChangeCb != 0) {
        activeChangeCb(this);
        ret = 1;
    }
    return ret;
}

void DB_WINDOW::SetActiveChangeCallback(DB_WINDOW_CALLBACK cb)
{
    activeChangeCb = cb;
}

void DB_WINDOW::SetCloseCallback(DB_WINDOW_CALLBACK cb)
{
    closeCb = cb;
}

void DB_WINDOW::OnClick(DB_POINT* p, int btn)
{
}

int DB_WINDOW::AddSelectablePrimitive(DB_PRIMITIVE* p, int x, int y)
{
    sel.AddPrimitive(p, x, y);
    return AddChild(p);
}

void DB_WINDOW::Close()
{
    if (closeCb) {
        closeCb(this);
    }
    active = 0;
}

void DB_WINDOW::Draw()
{
    if (select) {
        DB_DrawBox(drawPos.x + base.x, drawPos.y + base.y - 1.0f, size.x, size.y + 2.0f, 1.0f, 1.0f, 1.0f, 0.2f);
    } else {
        DB_DrawBox(drawPos.x + base.x, drawPos.y + base.y - 1.0f, size.x, size.y + 2.0f, 0.4f, 0.4f, 0.4f, 0.2f);
    }
}

void DB_WINDOW::OnKeybord(DB_KEYBORD* key)
{
    if ((keyFlag & DB_WIN_KEY_ESC_CLOSE) && key->trg[6]) {
        Close();
    }
}

DB_WINDOW_TITLE::DB_WINDOW_TITLE(const char* s)
{
    ca = cb = cg = cr = 0.0f;
    type = DB_PRIM_WINDOW_TITLE;
    SetStringColor(1.0f, 1.0f, 0.5f, 1.0f);
    SetString(s);
}

void DB_WINDOW_TITLE::SetStringColor(f32 r, f32 g, f32 b, f32 a)
{
    cr = r;
    cg = g;
    cb = b;
    ca = a;
    if (cr < 0.0f) cr = 0.0f;
    if (cr > 1.0f) cr = 1.0f;
    if (cg < 0.0f) cg = 0.0f;
    if (cg > 1.0f) cg = 1.0f;
    if (cb < 0.0f) cb = 0.0f;
    if (cb > 1.0f) cb = 1.0f;
    if (ca < 0.0f) ca = 0.0f;
    if (ca > 1.0f) ca = 1.0f;
}

int DB_WINDOW_TITLE::SetString(const char* s)
{
    int ret = 0;

    if (strlen(s) <= 255) {
        ret = 1;
        strcpy(str, s);
    }
    return ret;
}

// the .y reads of base/size go through a DB_POINT* accessor: gcse PREs `&base`/`&size` into
// callee-saved registers (`lfs 4(r30)`) while the .x reads stay `this`-relative
static inline f32 DB_PointY(DB_POINT* p)
{
    return p->y;
}

static inline f32 DB_AddY(f32 a, DB_POINT* p)
{
    return a + p->y;
}

void DB_WINDOW_TITLE::Draw()
{
    f32 r, g, b;

    g = 0.7f;
    r = g;
    b = g;
    if (mouseOn) {
        b = 0.8f;
        g = b;
        r = b;
    }
    DB_DrawBox(drawPos.x + base.x + 1.0f, DB_AddY(drawPos.y, &base), size.x - 2.0f, size.y - 1.0f, r, g, b, 0.2f);
    DB_DrawString(drawPos.x + base.x + 2.0f, drawPos.y + DB_PointY(&base), str, cr, cg, cb, ca);
}

void DB_WINDOW_TITLE::OnMouseDrag(DB_POINT* p, int btn)
{
    DB_POINT np;

    np.x = 0.0f;
    np.y = 0.0f;
    if (btn == 0 && parent) {
        np = parent->pos;
        np.x += p->x;
        np.y += p->y;
        parent->pos = np;
    }
}

void DB_WINDOW_TITLE::OnDoubleClick(DB_POINT* p, int btn)
{
    if (btn == 0 && parent) {
        if (parent->type == DB_PRIM_WINDOW) {
            ((DB_WINDOW*) parent)->Close();
        } else {
            parent->active = btn;
        }
    }
}

DB_BUTTON_CLOSE::DB_BUTTON_CLOSE()
{
    type = DB_PRIM_BUTTON_CLOSE;
    flag |= DB_PRIM_FLAG_MOUSE_ON;
    SetSize(10.0f, 10.0f);
    rect = DB_RECT(base.x - 2.0f, base.y - 2.0f, size.x + 4.0f, size.y + 4.0f);
}

void DB_BUTTON_CLOSE::Draw()
{
}

void DB_BUTTON_CLOSE::OnClick(DB_POINT* p, int btn)
{
    if (btn == 0 && parent) {
        if (parent->type == DB_PRIM_WINDOW) {
            ((DB_WINDOW*) parent)->Close();
        } else {
            parent->active = btn;
        }
    }
}

DB_STRING::DB_STRING(u32 max_, const char* s)
{
    u32 zero = 0;
    max = max_;
    // COMPILER-DIFF: candidate (local-alloc qty order): four sched1-only anchors. Each is a
    // `"=m"` store the colour chain below overwrites; written above the chain, flow1 keeps them
    // (the pool `lfs` sits between anchor and store), sched1 hoists the `lfs` above them, and
    // flow2 deletes them, so sched2 never sees them. At sched1 each is a prio-13 insn issued
    // before the prio-12 vptr store (vt qty life 12 -> 16 = LC's 5000, LC r9 / vt r11) and the
    // two `zero` reads make it a 5-ref qty above the type constant (zero r0, type r9).
    asm("" : "=m"(cr) : "r"(zero));
    asm("" : "=m"(cg) : "r"(zero));
    asm("" : "=m"(cb));
    asm("" : "=m"(ca));
    ca = cb = cg = cr = 0.0f;
    type = DB_PRIM_STRING;
    len = zero;
    str = (char*) zero;
    str = new char[max_];
    strcpy(str, s);
    len = strlen(s);
    ca = cb = cg = cr = 1.0f;
}

DB_STRING::~DB_STRING()
{
    if (str) {
        delete[] str;
    }
}

void DB_STRING::SetColor(f32 r, f32 g, f32 b, f32 a)
{
    cr = r;
    cg = g;
    cb = b;
    ca = a;
    if (cr < 0.0f) cr = 0.0f;
    if (cr > 1.0f) cr = 1.0f;
    if (cg < 0.0f) cg = 0.0f;
    if (cg > 1.0f) cg = 1.0f;
    if (cb < 0.0f) cb = 0.0f;
    if (cb > 1.0f) cb = 1.0f;
    if (ca < 0.0f) ca = 0.0f;
    if (ca > 1.0f) ca = 1.0f;
}

int DB_STRING::SetString(const char* s)
{
    int ret = 0;

    if (strlen(s) <= max) {
        ret = 1;
        strcpy(str, s);
        len = strlen(str);
        SetSize((f32) len * 8.0f, 16.0f);
    }
    return ret;
}

void DB_STRING::Draw()
{
    DB_DrawString(drawPos.x + base.x, DB_AddY(drawPos.y, &base), str, cr, cg, cb, ca);
    if (select) {
        DB_DrawBox(drawPos.x + base.x - 1.0f, drawPos.y + DB_PointY(&base) + 1.0f, size.x + 5.0f, DB_PointY(&size) + 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        DB_DrawBox(drawPos.x + base.x - 2.0f, drawPos.y + DB_PointY(&base) - 0.0f, size.x + 4.0f, DB_PointY(&size) + 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    } else if (mouseOn) {
        DB_DrawBox(drawPos.x + base.x - 2.0f, drawPos.y + DB_PointY(&base) - 0.0f, size.x + 4.0f, DB_PointY(&size) + 0.0f, 1.0f, 1.0f, 0.0f, 1.0f);
    }
}

DB_BUTTON::DB_BUTTON() : DB_STRING(255, "")
{
    cb = 0;
    type = DB_PRIM_BUTTON;
    flag |= DB_PRIM_FLAG_SELECTABLE | DB_PRIM_FLAG_MOUSE_ON;
    SetCallback(0);
}

void DB_BUTTON::SetCallback(DB_PRIM_CALLBACK cb_)
{
    cb = cb_;
}

void DB_BUTTON::OnClick(DB_POINT* p, int btn)
{
    if (cb) {
        cb(this);
    }
}

void DB_BUTTON::OnKeybord(DB_KEYBORD* key)
{
    if (key->trg[5] && cb) {
        cb(this);
    }
}

DB_NUMERIC::DB_NUMERIC() : DB_STRING(255, "")
{
    pNum = 0;
    numType = 0;
    def = 0.0f;
    step = 0.0f;
    max = 0.0f;
    min = 0.0f;
    keta = 0;
    ketaFloat = 0;
    edit = 0;
    minus = 0;
    unit = 0.0f;
    nameTbl = 0;
    nameNum = 0;
    // the target stores 2 (DB_NUM_FLAG_NO_SELECT) here before SetNumFlg(0) overwrites it, after the
    // nameTbl/nameNum zeros; type before flag
    numFlg = DB_NUM_FLAG_NO_SELECT;
    type = DB_PRIM_NUMERIC;
    flag |= DB_PRIM_FLAG_SELECTABLE | DB_PRIM_FLAG_MOUSE_ON;
    SetNumFlg(0);
    {
        // COMPILER-DIFF: #13 -- the original stores its REG_EQUIV constants as constants: the block
        // is issued in pure source order (no death at the last zero / 1.0 store) and reload puts 1.0
        // in the spill register f13 (255.0 gets f0). The non-volatile asm with a memory output keeps
        // our zero/1.0 pseudos live past the block without a scheduling barrier and is not deleted
        // by flow (a store), `fr13` pins the 1.0. It must sit AFTER the SetDefault call: inside the
        // block it is a free-unit insn ready at the same cycle as `fmr f1,f31` and, ranking above the
        // fmr by LUID, it takes the block's second issue slot (issue rate 2), which pushes the fmr one
        // store later; `step = one; unit = one;` is the order that survives the asm's move.
        int zero = 0;
        register f32 one asm("fr13");
        minus = zero;
        pNum = (void*) zero;
        min = 0.0f;
        ketaFloat = zero;
        edit = zero;
        max = 255.0f;
        keta = 3;
        one = 1.0f;
        step = one;
        unit = one;
        SetDefault(0.0f);
        asm("" : "=m"(def) : "r"(zero), "f"(one));
    }
}

void DB_NUMERIC::SetNumFlg(u32 flg)
{
    numFlg = flg;
    if (flg & DB_NUM_FLAG_NO_SELECT) {
        flag &= ~(DB_PRIM_FLAG_SELECTABLE | DB_PRIM_FLAG_MOUSE_ON);
    } else {
        flag |= DB_PRIM_FLAG_SELECTABLE | DB_PRIM_FLAG_MOUSE_ON;
    }
}

f32 DB_NUMERIC::GetNumFloat()
{
    f32 v;

    if (numFlg & DB_NUM_FLAG_SIGNED_VIEW) {
        switch (numType) {
        case DB_NUM_S8:
            v = (f32) *(u8*) pNum;
            break;
        case DB_NUM_U8:
            v = (f32) *(u8*) pNum;
            break;
        case DB_NUM_S16:
            v = (f32) *(u16*) pNum;
            break;
        case DB_NUM_U16:
            v = (f32) *(u16*) pNum;
            break;
        case DB_NUM_S32:
            v = (f32) *(u32*) pNum;
            break;
        case DB_NUM_U32:
            v = (f32) *(u32*) pNum;
            break;
        case DB_NUM_F32:
            v = *(f32*) pNum;
            break;
        default:
            v = 0.0f;
            break;
        }
    } else {
        switch (numType) {
        case DB_NUM_S8:
            v = (f32) *(s8*) pNum;
            break;
        case DB_NUM_U8:
            v = (f32) *(u8*) pNum;
            break;
        case DB_NUM_S16:
            v = (f32) *(s16*) pNum;
            break;
        case DB_NUM_U16:
            v = (f32) *(u16*) pNum;
            break;
        case DB_NUM_S32:
            v = (f32) *(s32*) pNum;
            break;
        case DB_NUM_U32:
            v = (f32) *(u32*) pNum;
            break;
        case DB_NUM_F32:
            v = *(f32*) pNum;
            break;
        default:
            v = 0.0f;
            break;
        }
    }
    return v;
}

void DB_NUMERIC::SetNumFloat(f32 v)
{
    if (!(numFlg & DB_NUM_FLAG_NO_LIMIT)) {
        if (numFlg & DB_NUM_FLAG_LOOP) {
            if (v > max) {
                v -= max - min + step;
            }
            if (v < min) {
                v += max - min + step;
            }
        } else {
            if (v > max) {
                v = max;
            }
            if (v < min) {
                v = min;
            }
        }
    }
    if (v != 0.0f) {
        if (v < 0.0f) {
            minus = 1;
        } else {
            minus = 0;
        }
    }
    switch (numType) {
    case DB_NUM_S8:
        *(s8*) pNum = (int) v;
        break;
    case DB_NUM_U8:
        *(u8*) pNum = (int) v;
        break;
    case DB_NUM_S16:
        *(s16*) pNum = (int) v;
        break;
    case DB_NUM_U16:
        *(u16*) pNum = (int) v;
        break;
    case DB_NUM_S32:
        *(s32*) pNum = (int) v;
        break;
    case DB_NUM_U32:
        *(u32*) pNum = (int) v;
        break;
    case DB_NUM_F32:
        *(f32*) pNum = v;
        break;
    }
}

void DB_NUMERIC::SetDefault(f32 v)
{
    def = v;
    if (!(numFlg & DB_NUM_FLAG_NO_LIMIT)) {
        if (v < min) {
            v = min;
        }
        if (v > max) {
            v = max;
        }
        def = v;
    }
}

void DB_NUMERIC::SetKeta(u32 n)
{
    if (n < 2) {
        n = 1;
    }
    keta = n;
}

void DB_NUMERIC::SetKetaFloat(int n)
{
    ketaFloat = n;
}

void DB_NUMERIC::SetNumPointer(s8* p)
{
    numType = DB_NUM_S8;
    keta = 4;
    max = 127.0f;
    min = -128.0f;
    pNum = p;
}

void DB_NUMERIC::SetNumPointer(u8* p)
{
    numType = DB_NUM_U8;
    keta = 3;
    max = 255.0f;
    min = 0.0f;
    pNum = p;
}

void DB_NUMERIC::SetNumPointer(s16* p)
{
    numType = DB_NUM_S16;
    keta = 7;
    max = 32767.0f;
    min = -32768.0f;
    pNum = p;
}

void DB_NUMERIC::SetNumPointer(u16* p)
{
    numType = DB_NUM_U16;
    keta = 6;
    max = 65535.0f;
    min = 0.0f;
    pNum = p;
}

void DB_NUMERIC::SetNumPointer(s32* p)
{
    numType = DB_NUM_S32;
    keta = 11;
    max = 10000000.0f;
    min = -10000000.0f;
    pNum = p;
}

void DB_NUMERIC::SetNumPointer(u32* p)
{
    numType = DB_NUM_U32;
    keta = 10;
    max = 10000000.0f;
    min = 0.0f;
    pNum = p;
}

void DB_NUMERIC::SetNumPointer(f32* p)
{
    numType = DB_NUM_F32;
    keta = 7;
    ketaFloat = 1;
    max = 32767.0f;
    min = -32768.0f;
    pNum = p;
}

void DB_NUMERIC::ClearToDefault()
{
    SetNumFloat(def);
    if (def >= 0.0f) {
        minus = 0;
    } else {
        minus = 1;
    }
}

void DB_NUMERIC::OnDoubleClick(DB_POINT* p, int btn)
{
    if ((numFlg & DB_NUM_FLAG_LOCK) == 0 && btn == 0) {
        ClearToDefault();
    }
}

int DB_NUMERIC::SetStringInt()
{
    char buf[16];
    f32 v;
    int i;
    int minusDone = 0;
    u32 radix;
    u8 digit;

    v = GetNumFloat();
    if (!(numFlg & DB_NUM_FLAG_NO_LIMIT) && (v < min || v > max)) {
        for (i = 0; i < keta - 3; i++) {
            buf[i] = ' ';
        }
        if (keta != 3) buf[keta - 3] = 'E';
        if (keta != 2) buf[keta - 2] = 'r';
        if (keta != 1) buf[keta - 1] = 'r';
        buf[keta] = 0;
        return 0;
    }
    if (v < 0.0f) {
        minus = 1;
        v = -v;
    } else {
        minus = 0;
    }
    buf[keta] = 0;
    if (v == 0.0f) {
        for (i = 0; i < keta - 1; i++) {
            if (numFlg & DB_NUM_FLAG_HEX) {
                buf[i] = '0';
            } else {
                buf[i] = ' ';
            }
        }
        if (minus && edit == 0) {
            buf[i] = '-';
        } else {
            buf[i] = '0';
        }
    } else {
        radix = 10;
        if (numFlg & DB_NUM_FLAG_HEX) {
            radix = 16;
        }
        for (i = keta - 1; i >= 0; i--) {
            digit = (u8) (v - (f32) ((int) (v / radix) * radix));
            if (digit == 0 && v < radix) {
                if (minus && minusDone == 0) {
                    buf[i] = '-';
                    minusDone = 1;
                } else if (radix == 10) {
                    buf[i] = ' ';
                } else {
                    buf[i] = '0';
                }
            } else {
                buf[i] = hexDigit[digit];
            }
            v = v / radix;
        }
    }
    SetString(buf);
    return 1;
}

int DB_NUMERIC::SetStringFloat()
{
    char buf[16];
    f32 v;
    int i;
    int minusDone = 0;
    int ret;
    int state;
    u8 digit;

    v = GetNumFloat();
    if (!(numFlg & DB_NUM_FLAG_NO_LIMIT) && (v < min || v > max)) {
        for (i = 0; i < keta - 3; i++) {
            buf[i] = ' ';
        }
        if (keta != 3) buf[keta - 3] = 'E';
        if (keta != 2) buf[keta - 2] = 'r';
        if (keta != 1) buf[keta - 1] = 'r';
        buf[keta] = 0;
        SetString(buf);
        return 0;
    }
    if (v < 0.0f) {
        minus = 1;
        v = -v;
    } else {
        minus = 0;
    }
    buf[keta] = 0;
    if (v == 0.0f) {
        state = 0;
        for (i = keta - 1; i >= keta - ketaFloat; i--) {
            buf[i] = '0';
        }
        for (; i >= 0; i--) {
            if (state == 0) {
                buf[i] = '.';
                state = 1;
            } else if (state == 1) {
                buf[i] = '0';
                state = 2;
            } else if ((u32) state > 1) {
                buf[i] = ' ';
            }
        }
    } else {
        f32 ten = 10.0f;

        for (i = 0; i < ketaFloat; i++) {
            v *= ten;
        }
        ret = 0;
        for (i = keta - 1; i >= 0; i--) {
            int lim = ketaFloat + 1;

            if (i == keta - lim) {
                buf[i] = '.';
            } else {
                digit = (u8) (v - (f32) (int) (v / ten) * ten);
                if (i == keta - 1) {
                    f32 rest = v - (f32) (int) digit;
                    if (rest - (f32) (int) (rest / ten) * ten > 0.5f) {
                        ret = 1;
                    }
                }
                if (ret == 1) {
                    digit++;
                    ret = 0;
                }
                if (digit == 10) {
                    digit = 0;
                    ret = 1;
                }
                if (digit == 0 && v < ten) {
                    if (i == keta - ketaFloat - 3) {
                        if (minus && minusDone == 0) {
                            buf[i] = '-';
                            minusDone = 1;
                        } else {
                            buf[i] = ' ';
                        }
                    } else if (i < keta - ketaFloat - 2) {
                        if (minus && minusDone == 0) {
                            buf[i] = '-';
                            minusDone = 1;
                        } else {
                            buf[i] = ' ';
                        }
                    } else {
                        buf[i] = '0';
                    }
                } else {
                    buf[i] = digit + '0';
                }
                v = v / ten;
            }
        }
    }
    SetString(buf);
    return 1;
}

void DB_NUMERIC::Update()
{
    int ret;
    u32 idx;
    int ok;

    if (select == 0) {
        edit = 1;
    }
    if (pNum == 0) {
        SetString("NULL pointer!");
        return;
    }
    SetSize((f32) (u32) keta * 8.0f, 16.0f);
    if (nameNum) {
        idx = (u32) GetNumFloat();
        if (idx < nameNum) {
            SetString(nameTbl[idx]);
            SetSize((f32) strlen(nameTbl[idx]) * 8.0f, 16.0f);
            ret = 1;
        } else {
            ret = SetStringInt();
        }
    } else if (numType != DB_NUM_F32 || ketaFloat == 0) {
        ret = SetStringInt();
    } else {
        ret = SetStringFloat();
    }
    ok = (numFlg & DB_NUM_FLAG_LOCK) == 0;
    if (ok) {
      if (!(numFlg & DB_NUM_FLAG_NO_SELECT)) {
        if (ret == 0) {
            SetColor(1.0f, 0.0f, 0.0f, 1.0f);
        } else if (edit) {
            SetColor(1.0f, 1.0f, 1.0f, 1.0f);
        } else {
            SetColor(1.0f, 1.0f, 0.0f, 1.0f);
        }
      }
    }
}

void DB_NUMERIC::OnCalcMsg(int msg)
{
    f32 v;
    f32 k10, k100, k01;

    if (numFlg & DB_NUM_FLAG_LOCK) {
        return;
    }
    if (numFlg & DB_NUM_FLAG_HEX) {
        k10 = 16.0f;
        k100 = 256.0f;
        k01 = 0.0625f;
    } else {
        k10 = 10.0f;
        k100 = 100.0f;
        k01 = 0.1f;
    }
    v = GetNumFloat();
    switch (msg) {
    case DB_CALC_MIN:
        v = min;
        break;
    case DB_CALC_SUB_X100:
        v -= unit * k100;
        break;
    case DB_CALC_SUB_X10:
        v -= unit * k10;
        break;
    case DB_CALC_SUB:
        v -= unit;
        break;
    case DB_CALC_SUB_X01:
        v -= unit * k01;
        break;
    case DB_CALC_ADD_X01:
        v += unit * k01;
        break;
    case DB_CALC_ADD:
        v += unit;
        break;
    case DB_CALC_ADD_X10:
        v += unit * k10;
        break;
    case DB_CALC_ADD_X100:
        v += unit * k100;
        break;
    case DB_CALC_MAX:
        v = max;
        break;
    case DB_CALC_DEFAULT:
        v = def;
        break;
    }
    SetNumFloat(v);
}

void DB_NUMERIC::OnCalcMsgFloat(f32 d)
{
    if ((numFlg & DB_NUM_FLAG_LOCK) == 0) {
        if ((numFlg & DB_NUM_FLAG_NO_FLOAT_MSG) == 0) {
            SetNumFloat(GetNumFloat() + d * unit);
        }
    }
}

void DB_NUMERIC::OnKeybord(DB_KEYBORD* key)
{
    u8 c;
    f32 v;
    int d;

    if (numFlg & DB_NUM_FLAG_LOCK) {
        return;
    }
    c = key->chr;
    if ((u32) (c - '0') <= 9 || c == '-' || c == '.' || c == 8 || c == '\n') {
        if (c == '\n') {
            edit = 0;
        }
        if (edit == 1 && c != 8) {
            edit = 0;
            minus = 0;
            v = 0.0f;
        } else {
            edit = 0;
            v = GetNumFloat();
        }
        if (minus) {
            v = -v;
        }
        if (c == '-' && v == 0.0f) {
            minus = 1;
        }
        d = c - '0';
        if ((u8) d <= 9) {
            f32 fd = (f32) d;

            v *= 10.0f;
            if (!(numFlg & DB_NUM_FLAG_NO_LIMIT) && v > max) {
                v /= 10.0f;
            } else {
                v += fd;
            }
        }
        if (c == 8) {
            v /= 10.0f;
        }
        if (minus && v > 0.0f) {
            v = -v;
        }
        SetNumFloat(v);
    }
}

DB_NUMERIC2::DB_NUMERIC2()
{
    pNum2 = 0;
    lastMsg = DB_CALC_NONE;
}

void DB_NUMERIC2::SetNumPointer2(s8* p)
{
    pNum2 = p;
}

void DB_NUMERIC2::SetNumPointer2(u8* p)
{
    pNum2 = p;
}

void DB_NUMERIC2::SetNumPointer2(s16* p)
{
    pNum2 = p;
}

void DB_NUMERIC2::SetNumPointer2(u16* p)
{
    pNum2 = p;
}

void DB_NUMERIC2::SetNumPointer2(s32* p)
{
    pNum2 = p;
}

void DB_NUMERIC2::SetNumPointer2(f32* p)
{
    pNum2 = p;
}

void DB_NUMERIC2::SetNumFloat2(f32 v)
{
    switch (numType) {
    case DB_NUM_S8:
        *(s8*) pNum2 = (int) v;
        break;
    case DB_NUM_U8:
        *(u8*) pNum2 = (int) v;
        break;
    case DB_NUM_S16:
        *(s16*) pNum2 = (int) v;
        break;
    case DB_NUM_U16:
        *(u16*) pNum2 = (int) v;
        break;
    case DB_NUM_S32:
        *(s32*) pNum2 = (int) v;
        break;
    case DB_NUM_U32:
        *(u32*) pNum2 = (int) v;
        break;
    case DB_NUM_F32:
        *(f32*) pNum2 = v;
        break;
    }
}

f32 DB_NUMERIC2::GetNumFloat2()
{
    f32 v;

    if (numFlg & DB_NUM_FLAG_SIGNED_VIEW) {
        switch (numType) {
        case DB_NUM_S8:
            v = (f32) *(u8*) pNum2;
            break;
        case DB_NUM_U8:
            v = (f32) *(u8*) pNum2;
            break;
        case DB_NUM_S16:
            v = (f32) *(u16*) pNum2;
            break;
        case DB_NUM_U16:
            v = (f32) *(u16*) pNum2;
            break;
        case DB_NUM_S32:
            v = (f32) *(u32*) pNum2;
            break;
        case DB_NUM_U32:
            v = (f32) *(u32*) pNum2;
            break;
        case DB_NUM_F32:
            v = *(f32*) pNum2;
            break;
        default:
            v = 0.0f;
            break;
        }
    } else {
        switch (numType) {
        case DB_NUM_S8:
            v = (f32) *(s8*) pNum2;
            break;
        case DB_NUM_U8:
            v = (f32) *(u8*) pNum2;
            break;
        case DB_NUM_S16:
            v = (f32) *(s16*) pNum2;
            break;
        case DB_NUM_U16:
            v = (f32) *(u16*) pNum2;
            break;
        case DB_NUM_S32:
            v = (f32) *(s32*) pNum2;
            break;
        case DB_NUM_U32:
            v = (f32) *(u32*) pNum2;
            break;
        case DB_NUM_F32:
            v = *(f32*) pNum2;
            break;
        default:
            v = 0.0f;
            break;
        }
    }
    return v;
}

void DB_NUMERIC2::OnCalcMsg(int msg)
{
    f32 v;
    f32 k10, k100, k1000, k01;

    if (numFlg & DB_NUM_FLAG_LOCK) {
        return;
    }
    if (numFlg & DB_NUM_FLAG_HEX) {
        k10 = 16.0f;
        k100 = 256.0f;
        k1000 = 4096.0f;
        k01 = 0.0625f;
    } else {
        k10 = 10.0f;
        k100 = 100.0f;
        k1000 = 1000.0f;
        k01 = 0.1f;
    }
    v = GetNumFloat();
    switch (msg) {
    case DB_CALC_MIN:
        // `v = 0.0f` BEFORE the call in the three call arms: the arm blocks then end in the CALL_INSN, the
        // fall-through arm (DEFAULT) gets flow.c's `(use (const_int 0))` nop and cannot be a cross-jump target;
        // MIN merges its 3-insn tail into MAX's (see docs/matching.md COMPILER-DIFF #6 resolved)
        v = 0.0f;
        SetNumFloat(min);
        break;
    case DB_CALC_SUB_X1000:
        v -= unit * k1000;
        break;
    case DB_CALC_SUB_X100:
        v -= unit * k100;
        break;
    case DB_CALC_SUB_X10:
        v -= unit * k10;
        break;
    case DB_CALC_SUB:
        v -= unit;
        break;
    case DB_CALC_SUB_X01:
        v -= unit * k01;
        break;
    case DB_CALC_ADD_X01:
        v += unit * k01;
        break;
    case DB_CALC_ADD:
        v += unit;
        break;
    case DB_CALC_ADD_X10:
        v += unit * k10;
        break;
    case DB_CALC_ADD_X100:
        v += unit * k100;
        break;
    case DB_CALC_ADD_X1000:
        v += unit * k1000;
        break;
    case DB_CALC_MAX:
        v = 0.0f;
        SetNumFloat(max);
        break;
    case DB_CALC_DEFAULT:
        v = 0.0f;
        SetNumFloat(def);
        break;
    }
    if (GetNumFloat2() == 0.0f) {
        SetNumFloat2(v - GetNumFloat());
    }
    lastMsg = msg;
}

void DB_NUMERIC2::OnCalcMsgFloat(f32 d)
{
    f32 v;

    if ((numFlg & DB_NUM_FLAG_LOCK) == 0) {
        if ((numFlg & DB_NUM_FLAG_NO_FLOAT_MSG) == 0) {
            v = GetNumFloat() + d * unit;
            if (GetNumFloat2() == 0.0f) {
                SetNumFloat2(v - GetNumFloat());
            }
        }
    }
}

// never called in this module (dead-stripped body, constant pool kept)
DB_SLIDEBAR::DB_SLIDEBAR()
{
    pNum = 0;
    rate = 0.0f;
    length = 64.0f;
    min = 0.0f;
    max = 255.0f;
    SetBase(10.0f, -5.0f);
    SetSize(16.0f, 16.0f);
}

// never called either (a second 16.0f pool entry follows the constructor's)
static void dbSlidebarSetDefaultLength(DB_SLIDEBAR* s)
{
    s->length = 16.0f;
}

void DB_SLIDEBAR::UpdateHoldNum()
{
    f32 v;

    if (pNum) {
        v = min + rate * (max - min);
        switch (numType) {
        case DB_NUM_S8:
            *(s8*) pNum = (s8) v;
            break;
        case DB_NUM_U8:
            *(u8*) pNum = (s8) v;
            break;
        case DB_NUM_S16:
            *(s16*) pNum = (s16) v;
            break;
        case DB_NUM_U16:
            *(u16*) pNum = (s16) v;
            break;
        case DB_NUM_S32:
            *(s32*) pNum = (int) v;
            break;
        case DB_NUM_U32:
            *(u32*) pNum = (int) v;
            break;
        case DB_NUM_F32:
            *(f32*) pNum = v;
            break;
        }
    }
}

void DB_SLIDEBAR::Update()
{
    f32 v;

    if (pNum) {
        switch (numType) {
        case DB_NUM_S8:
            v = (f32) *(s8*) pNum;
            break;
        case DB_NUM_U8:
            v = (f32) *(u8*) pNum;
            break;
        case DB_NUM_S16:
            v = (f32) *(s16*) pNum;
            break;
        case DB_NUM_U16:
            v = (f32) *(u16*) pNum;
            break;
        case DB_NUM_S32:
            v = (f32) *(s32*) pNum;
            break;
        case DB_NUM_U32:
            v = (f32) *(u32*) pNum;
            break;
        case DB_NUM_F32:
            v = *(f32*) pNum;
            break;
        default:
            v = 0.0f;
            break;
        }
        rate = (v - min) / (max - min);
        if (rate < 0.0f) rate = 0.0f;
        if (rate > 1.0f) rate = 1.0f;
    }
}

void DB_SLIDEBAR::OnMouseDrag(DB_POINT* p, int btn)
{
    if (btn == 0) {
        if (p->x != 0.0f) {
            rate += p->x / length;
            if (rate < 0.0f) rate = 0.0f;
            if (rate > 1.0f) rate = 1.0f;
        }
        UpdateHoldNum();
    }
}

void DB_SLIDEBAR::Draw()
{
    f32 kx;

    if (mouseOn) {
        DB_DrawBox(drawPos.x + base.x, drawPos.y + base.y + 2.0f, length, size.y - 3.0f, 0.9f, 0.9f, 0.0f, 1.0f);
    }
    if (select) {
        DB_DrawBox(drawPos.x + base.x, drawPos.y + base.y + 2.0f, length, size.y - 3.0f, 0.7f, 0.7f, 0.7f, 1.0f);
    }
    DB_DrawBox(drawPos.x, drawPos.y + 7.0f, length, 3.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    kx = rate * length;
    DB_DrawBox(drawPos.x + kx - 3.0f, drawPos.y + 8.0f - 6.0f, 6.0f, 14.0f, 0.5f, 0.5f, 0.5f, 1.0f);
    DB_DrawBoxFill(drawPos.x + kx - 3.0f, drawPos.y + 8.0f - 6.0f, 6.0f, 14.0f, 1.0f, 1.0f, 1.0f, 1.0f);
}

// Never called (dead-stripped body): keeps the range table alive.
static f32 dbNumRangeOf(DB_NUMERIC* n, int hi)
{
    return dbNumRange[n->numType][hi];
}

// Never called (the original linker dropped the body): deleting through each class marks the
// implicit destructors used, which our cc1plus does not do for the vtable entries alone.
static void dbWidgetDeleteAll(DB_WINDOW* a, DB_WINDOW_TITLE* b, DB_BUTTON_CLOSE* c, DB_BUTTON* d, DB_NUMERIC* e,
                              DB_NUMERIC2* f, DB_SLIDEBAR* g)
{
    delete a;
    delete b;
    delete c;
    delete d;
    delete e;
    delete f;
    delete g;
}
