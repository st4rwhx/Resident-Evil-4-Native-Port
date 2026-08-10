#include "types.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "global.h"
#include "main.h"
#include "main_mem.h"
#include "joy.h"
#include "item.h"
#include "model.h"
#include "math_sub.h"
#include "db_log.h"
#include "eprintf.h"
#include "puzzle.h"

// Attache case packing puzzle: pieces (items) placed on the case board or the spare board.

extern "C" {
void* __builtin_new(unsigned int size);
void __builtin_delete(void* p);
void* __builtin_vec_new(unsigned int size);
void __builtin_vec_delete(void* p);
float sinf(float);
float cosf(float);
}

static u8 piece_max = 0x79;
static u8 space_w = 6;
static u8 space_h = 0xC;

PieceInfo piece_info[] = {
    { 0x0003, 0, 5, 2, { 0, 0 }, 2.0f, 0.5f, "1111111111", { 0 } },
    { 0x0021, 0, 3, 2, { 0, 0 }, 1.0f, 0.5f, "111111", { 0 } },
    { 0x0040, 0, 3, 2, { 0, 0 }, 1.0f, 0.5f, "111111", { 0 } },
    { 0x0023, 0, 3, 2, { 0, 0 }, 1.0f, 0.5f, "111111", { 0 } },
    { 0x0025, 0, 4, 2, { 0, 0 }, 1.5f, 0.5f, "11111111", { 0 } },
    { 0x0027, 0, 3, 2, { 0, 0 }, 1.0f, 0.5f, "111111", { 0 } },
    { 0x0029, 0, 4, 2, { 0, 0 }, 1.5f, 0.5f, "11111111", { 0 } },
    { 0x002A, 0, 4, 2, { 0, 0 }, 1.5f, 0.5f, "11111111", { 0 } },
    { 0x002C, 0, 8, 2, { 0, 0 }, 3.5f, 0.5f, "1111111111111111", { 0 } },
    { 0x002D, 0, 5, 2, { 0, 0 }, 2.0f, 0.5f, "1111111111", { 0 } },
    { 0x002E, 0, 9, 1, { 0, 0 }, 4.0f, 0.0f, "111111111", { 0 } },
    { 0x002F, 0, 7, 2, { 0, 0 }, 3.0f, 0.5f, "11111111111111", { 0 } },
    { 0x0030, 0, 3, 2, { 0, 0 }, 1.0f, 0.5f, "111111", { 0 } },
    { 0x0034, 0, 7, 3, { 0, 0 }, 3.0f, 1.0f, "111111111111111111111", { 0 } },
    { 0x0035, 0, 8, 2, { 0, 0 }, 3.5f, 0.5f, "1111111111111111", { 0 } },
    { 0x0036, 0, 5, 2, { 0, 0 }, 2.0f, 0.5f, "1111111111", { 0 } },
    { 0x0037, 0, 4, 2, { 0, 0 }, 1.5f, 0.5f, "11111111", { 0 } },
    { 0x0038, 0, 1, 3, { 0, 0 }, 0.0f, 1.0f, "111", { 0 } },
    { 0x0094, 0, 8, 2, { 0, 0 }, 3.5f, 0.5f, "1111111111111111", { 0 } },
    { 0x0017, 0, 8, 2, { 0, 0 }, 3.5f, 0.5f, "1111111111111111", { 0 } },
    { 0x006D, 0, 8, 2, { 0, 0 }, 3.5f, 0.5f, "1111111111111111", { 0 } },
    { 0x003E, 0, 4, 2, { 0, 0 }, 1.5f, 0.5f, "11111111", { 0 } },
    { 0x0052, 0, 7, 3, { 0, 0 }, 3.0f, 1.0f, "111111111111111111111", { 0 } },
    { 0x0004, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x0020, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x0018, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x0007, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x006A, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x001A, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x0000, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x0046, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x0072, 0, 4, 1, { 0, 0 }, 1.5f, 0.0f, "1111", { 0 } },
    { 0x003F, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x0042, 0, 3, 1, { 0, 0 }, 1.0f, 0.0f, "111", { 0 } },
    { 0x0043, 0, 2, 2, { 0, 0 }, 0.5f, 0.5f, "1111", { 0 } },
    { 0x0044, 0, 3, 1, { 0, 0 }, 1.0f, 0.0f, "111", { 0 } },
    { 0x0045, 0, 3, 1, { 0, 0 }, 1.0f, 0.0f, "111", { 0 } },
    { 0x00AA, 0, 2, 2, { 0, 0 }, 0.5f, 0.5f, "1111", { 0 } },
    { 0x00C5, 0, 3, 1, { 0, 0 }, 1.0f, 0.0f, "111", { 0 } },
    { 0x0001, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0002, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x000E, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0005, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0008, 0, 1, 1, { 0, 0 }, 0.0f, 0.0f, "1", { 0 } },
    { 0x0009, 0, 1, 1, { 0, 0 }, 0.0f, 0.0f, "1", { 0 } },
    { 0x000A, 0, 1, 1, { 0, 0 }, 0.0f, 0.0f, "1", { 0 } },
    { 0x0095, 0, 1, 3, { 0, 0 }, 0.0f, 1.0f, "111", { 0 } },
    { 0x0097, 0, 2, 6, { 0, 0 }, 0.5f, 2.5f, "111111111111", { 0 } },
    { 0x0006, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0019, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x001C, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0014, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0016, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x00A8, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0012, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0013, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0015, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x000C, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 }, 0.0f, 0.0f, "", { 0 } },
};

PieceData* searchItemPieceData(int id, PieceInfo* tbl)
{
    int i;

    for (i = 0;; i++) {
        if (tbl[i].id == id) {
            return &tbl[i].data;
        }
        if (tbl[i].id == 0xFFFF) {
            return 0;
        }
    }
}

u8* searchItemModelData(int id, PieceInfo* tbl)
{
    int i;

    for (i = 0;; i++) {
        if (tbl[i].id == id) {
            return tbl[i].model;
        }
        if (tbl[i].id == 0xFFFF) {
            return 0;
        }
    }
}

// The second loop counts from 0 to `o - 4` (not from 4 to `o`): the runtime bound is computed
// before the entry test (`addic. r0,r29,-4; ble`), check_dbra_loop reverses the compare-only biv
// with that count, and the count temp takes the ctrsi pattern's CTR preference (`mtctr r0; mfctr
// r31`; the loop body's call keeps the biv itself in r31).
void pzlPiece::orientation(int o)
{
    int i;

    cx = data->cx;
    cy = data->cy;
    orient = 0;
    if (o <= 3) {
        for (i = 0; i < o; i++) {
            rotate(0);
        }
    } else {
        mirror(0);
        for (i = 0; i < o - 4; i++) {
            rotate(0);
        }
    }
    if (o != orient) {
        pLog->err(0, 0, "pzlPiece::orientation() failed.");
    }
}

void pzlPiece::rotate(int dir)
{
    switch (dir) {
    case 0: {
        if (orient >= 0) {
            if (orient <= 3) {
                orient++;
                if (orient > 3) {
                    orient -= 4;
                }
            } else if (orient <= 7) {
                orient++;
                if (orient > 7) {
                    orient -= 4;
                }
            }
        }
        f32 t = cx;
        cx = -cy;
        cy = t;
        break;
    }
    case 1: {
        if (orient >= 0) {
            if (orient <= 3) {
                orient--;
                if (orient < 0) {
                    orient += 4;
                }
            } else if (orient <= 7) {
                orient--;
                if (orient < 4) {
                    orient += 4;
                }
            }
        }
        f32 t = cx;
        cx = cy;
        cy = -t;
        break;
    }
    }
}

void pzlPiece::mirror(int axis)
{
    switch (axis) {
    case 0:
        switch (orient) {
        case 0:
            orient = 4;
            break;
        case 1:
            orient = 7;
            break;
        case 2:
            orient = 6;
            break;
        case 3:
            orient = 5;
            break;
        case 4:
            orient = 0;
            break;
        case 5:
            orient = 3;
            break;
        case 6:
            orient = 2;
            break;
        case 7:
            orient = 1;
            break;
        }
        cx = -cx;
        break;
    case 1:
        switch (orient) {
        case 0:
            orient = 6;
            break;
        case 1:
            orient = 5;
            break;
        case 2:
            orient = 4;
            break;
        case 3:
            orient = 7;
            break;
        case 4:
            orient = 2;
            break;
        case 5:
            orient = 1;
            break;
        case 6:
            orient = 0;
            break;
        case 7:
            orient = 3;
            break;
        }
        cy = -cy;
        break;
    }
}

void pzlPiece::init(PieceData* d)
{
    data = d;
    flags |= 1;
    cx = d->cx;
    cy = d->cy;
    state = 0;
    orient = 0;
}

f32 pzlPiece::ver0_x()
{
    return x - cx;
}

f32 pzlPiece::ver0_y()
{
    return y - cy;
}

int pzlPiece::size_x()
{
    s8 size;

    if (data == 0) {
        return 0;
    }
    switch (orient) {
    case 0:
    case 6:
        size = data->w;
        break;
    case 1:
    case 5:
        size = -data->h;
        break;
    case 2:
    case 4:
        size = -data->w;
        break;
    case 3:
    case 7:
        size = data->h;
        break;
    default:
        goto none;
    }
    return size;
none:
    return 0;
}

// The last arm's `neg; extsb; blr` tail is cross-jumped into the previous arm in the original:
// jump2 only pairs RETURN insns, so every arm returns on its own (the `break` form's last arm falls
// into the shared return and is never a candidate). With per-arm returns the byte value prefers r3
// (global.c's sign_extend preference); the original keeps it in r0.
int pzlPiece::size_y()
{
    register s8 size asm("r0");  // COMPILER-DIFF: #17 (value pin)

    if (data == 0) {
        return 0;
    }
    switch (orient) {
    case 0:
    case 4:
        size = data->h;
        return size;
    case 1:
    case 7:
        size = data->w;
        return size;
    case 2:
    case 6:
        size = -data->h;
        return size;
    case 3:
    case 5:
        size = -data->w;
        return size;
    }
    return 0;
}

void pzlPiece::snap()
{
    s8 vx;
    s8 vy;

    // The `+=` in both arms: jump2 merges the tails from the conversion on and each arm keeps
    // its own `mr r3,this` for the third call.
    if (ver0_x() >= 0.0f) {
        vx = ver0_x() + 0.5f;
        x += ver0_x() - (f32) vx;
    } else {
        vx = ver0_x() - 0.5f;
        x += ver0_x() - (f32) vx;
    }
    if (ver0_y() >= 0.0f) {
        vy = ver0_y() + 0.5f;
        y += ver0_y() - (f32) vy;
    } else {
        vy = ver0_y() - 0.5f;
        y += ver0_y() - (f32) vy;
    }
}

// s8 rotation matrix kept in one word (rlwimi inserts); the products are (s8)px * s8 entries so
// convert_to_integer narrows the multiplies to QImode (the low byte of the word is used raw,
// byte 2 comes out as `extsh; srawi 8`); the negated sine goes through an s8 local (its
// `extsb; neg`); the mirror test is a `case 4..7` range (`cmpwi 7; bgt` then `cmpwi 4; blt`).
int pzlPiece::shape(int px, int py)
{
    s8 rot[2][2];
    f32 ang;
    int oi;
    s8 o;
    s8 s;
    int rx;
    int ry;
    // COMPILER-DIFF: px as a one-byte struct. A RECORD_TYPE local is not PROMOTE_MODEd, so `w` is
    // an unpromoted QImode pseudo and `w.v` expands to that REG (extract_bit_field's lsb-subreg
    // path returns op0 itself); `(s8) px` expands to `(subreg:QI px)`, and expand_binop swaps a
    // commutative multiply whenever op1 is a REG and op0 is not -- byte 2 of `rot` is such a REG
    // (the HImode extraction is copied into a fresh QI pseudo), which put `px` second in that one
    // `mullw`. cse folds the paradoxical `(subreg:SI w)` back to px (like the force_reg temps of
    // the other operands), so the copy is dead and no instruction is added (pass 5).
    struct { s8 v; } w;

    // The join `extsb r0,r0` is the promoted store of `o` from the int `oi` on both paths: the
    // compare's extension IS `oi` (r0), the else arm copies it into `o` (extsb of r0 -> r0), so the
    // `ble` lands on that extsb (pass 4).
    oi = orient;
    if (oi > 3) {
        o = orient - 4;
    } else {
        o = oi;
    }
    ang = (f32) o * -3.1415927f * 0.5f;
    rot[0][0] = cosf(ang);
    s = sinf(ang);
    rot[0][1] = -s;
    rot[1][0] = sinf(ang);
    rot[1][1] = cosf(ang);
    w.v = px;
    rx = (s8) (w.v * rot[0][0] + (s8) py * rot[0][1]);
    ry = (s8) (w.v * rot[1][0] + (s8) py * rot[1][1]);
    switch (orient) {
    case 4:
    case 5:
    case 6:
    case 7:
        rx = (s8) -rx;
        break;
    }
    if (rx < 0 || rx >= data->w || ry < 0 || ry >= data->h) {
        return 0;
    }
    return data->shape[rx + data->w * ry] == '1';
}

// Debug shape display. Dead: the original linker dropped the body (STRIP_UNUSED); its strings
// ("#", "") and constant pool (the int->f32 double trick, 8.0f, 14.0f) stay in .rodata between
// shape's pool and pzlBoard::init's strings.
static void dispShape(pzlPiece* p, int px, int py)
{
    int i;
    int j;

    for (j = 0; j < p->data->h; j++) {
        for (i = 0; i < p->data->w; i++) {
            f32 x = (f32) (px + i) * 8.0f + 14.0f;
            eprintf((int) x, py + j, 0, 0, p->shape(i, j) ? "#" : "");
        }
    }
}

#line 540 "D:/Bio4/Prog/puzzle.cpp"
int pzlBoard::init(int w_, int h_, int pieceMax_)
{
    int i;

#line 543 "D:/Bio4/Prog/puzzle.cpp"
    cells = (u8*) MEM_ALLOC(w_ * h_, 1, 13);
    if (cells == 0) {
        w = 0;
        h = 0;
        return 0;
    }
    w = w_;
    h = h_;
    clearState(0xFF);
#line 560 "D:/Bio4/Prog/puzzle.cpp"
    pieces = (pzlPiece**) MEM_ALLOC(pieceMax_ * 4, 1, 13);
    if (pieces == 0) {
        Mem_free(cells);
        return 0;
    }
    // Guarded count-down (`cmpwi n,0; beq` + `mtctr n` after the guard): the counter is a local set
    // inside the guard, so its CTR copy is initialised after the branch, not before it.
    if (pieceMax_ != 0) {
        int n = pieceMax_;
        i = 0;
        do {
            pieces[i] = 0;
            i++;
            n--;
        } while (n != 0);
    }
    pieceMax = pieceMax_;
    return 1;
}

void pzlBoard::quit()
{
    if (cells) {
        Mem_free(cells);
    }
    if (pieces) {
        Mem_free(pieces);
    }
}

int pzlBoard::getPieceNum()
{
    u8 n = 0;
    int i;

    for (i = 0; i < pieceMax; i++) {
        if (pieces[i]) {
            n++;
        }
    }
    return n;
}

int pzlBoard::search(pzlPiece* p)
{
    int i;

    for (i = 0; i < pieceMax; i++) {
        if (pieces[i] && p == pieces[i]) {
            return 1;
        }
    }
    return 0;
}

int pzlBoard::ckInsideWall(pzlPiece* p)
{
    int sx;
    int sy;
    s8 vx;
    s8 vy;
    int i;
    int j;

    wallDir = 0;
    sx = p->size_x();
    sy = p->size_y();
    vx = p->ver0_x();
    vy = p->ver0_y();
    wallDir = 0;
    for (i = 0; i != sx; sx > 0 ? i++ : i--) {
        for (j = 0; j != sy; sy > 0 ? j++ : j--) {
            if (p->shape((s8) i, (s8) j)) {
                int cx_ = vx + i;
                int cy_ = vy + j;
                if ((cellState((s8) cx_, (s8) cy_) & 2) && !(cellState((s8) cx_, (s8) cy_) & 0x40)) {
                    if (cx_ < -1) {
                        wallDir = 1;
                    }
                    if (cx_ > w) {
                        wallDir = 2;
                    }
                    if (cy_ < -1) {
                        wallDir = 3;
                    }
                    if (cy_ > h) {
                        wallDir = 4;
                    }
                    return 0;
                }
            }
        }
    }
    return 1;
}

int pzlBoard::outPiece(pzlPiece* p)
{
    int sx;
    int sy;
    s8 vx;
    s8 vy;
    int i;
    int j;
    int left = 1;
    int right = 1;
    int up = 1;
    int down = 1;

    outDir = 0;
    sx = p->size_x();
    sy = p->size_y();
    vx = p->ver0_x();
    vy = p->ver0_y();
    for (i = 0; i != sx; sx > 0 ? i++ : i--) {
        for (j = 0; j != sy; sy > 0 ? j++ : j--) {
            if (p->shape((s8) i, (s8) j)) {
                int cx_ = vx + i;
                int cy_ = vy + j;
                if (!(cellState((s8) cx_, (s8) cy_) & 2)) {
                    return 0;
                }
                if (cx_ >= 0) {
                    left = 0;
                }
                if (cx_ <= w - 1) {
                    right = 0;
                }
                if (cy_ >= 0) {
                    up = 0;
                }
                if (cy_ <= h - 1) {
                    down = 0;
                }
            }
        }
    }
    if (left) {
        outDir = 1;
    }
    if (right) {
        outDir = 2;
    }
    if (up) {
        outDir = 3;
    }
    if (down) {
        outDir = 4;
    }
    return 1;
}

int pzlBoard::putPiece(pzlPiece* p)
{
    int sx;
    int sy;
    s8 vx;
    s8 vy;
    int i;
    int j;

    p->snap();
    sx = p->size_x();
    sy = p->size_y();
    vx = p->ver0_x();
    vy = p->ver0_y();
    for (i = 0; i != sx; sx > 0 ? i++ : i--) {
        for (j = 0; j != sy; sy > 0 ? j++ : j--) {
            if (p->shape((s8) i, (s8) j)) {
                if (cellState((s8) (vx + i), (s8) (vy + j)) & 1) {
                    return 0;
                }
            }
        }
    }
    // The slot search has its own counter (r10: no call crossed); the marking nest reuses i/j,
    // which then conflict with the cx_/cy_ temps (r31/r30) and take r28/r29 in both nests.
    for (int n = 0; n < pieceMax; n++) {
        if (pieces[n] == 0) {
            pieces[n] = p;
            for (i = 0; i != sx; sx > 0 ? i++ : i--) {
                for (j = 0; j != sy; sy > 0 ? j++ : j--) {
                    if (p->shape((s8) i, (s8) j)) {
                        s8 cx_ = vx + i;
                        s8 cy_ = vy + j;
                        if (!(cellState(cx_, cy_) & 1)) {
                            *cell(cx_, cy_) |= 1;
                        }
                    }
                }
            }
            p->state = 1;
            return 1;
        }
    }
    return 0;
}

pzlPiece* pzlBoard::lapPiece(pzlPiece* p)
{
    pzlPiece* hit = 0;
    int sx;
    int sy;
    s8 vx;
    s8 vy;
    int i;
    int j;

    p->snap();
    sx = p->size_x();
    sy = p->size_y();
    vx = p->ver0_x();
    vy = p->ver0_y();
    for (i = 0; i != sx; sx > 0 ? i++ : i--) {
        for (j = 0; j != sy; sy > 0 ? j++ : j--) {
            if (p->shape((s8) i, (s8) j)) {
                s8 cx_ = vx + i;
                s8 cy_ = vy + j;
                pzlPiece* q;
                if (cellState(cx_, cy_) & 2) {
                    return 0;
                }
                q = getPiece(cx_, cy_);
                if (q) {
                    if (hit == 0) {
                        hit = q;
                    } else if (hit != q) {
                        return 0;
                    }
                }
            }
        }
    }
    return hit;
}

pzlPiece* pzlBoard::getPiece(int x, int y)
{
    pzlPiece* p = 0;
    int i;

    if (!(cellState(x, y) & 1)) {
        return 0;
    }
    for (i = 0; i < pieceMax; i++) {
        if (pieces[i]) {
            s8 vx = pieces[i]->ver0_x();
            s8 vy = pieces[i]->ver0_y();
            if (pieces[i]->shape((s8) (x - vx), (s8) (y - vy))) {
                p = pieces[i];
                break;
            }
        }
    }
    return p;
}

int pzlBoard::rmPiece(pzlPiece* p)
{
    s8 vx;
    s8 vy;
    int sx;
    int sy;
    int i;
    int j;

    vx = p->ver0_x();
    vy = p->ver0_y();
    sx = p->size_x();
    sy = p->size_y();
    for (i = 0; i != sx; sx > 0 ? i++ : i--) {
        for (j = 0; j != sy; sy > 0 ? j++ : j--) {
            if (p->shape((s8) i, (s8) j)) {
                *cell((s8) (vx + i), (s8) (vy + j)) &= ~1;
            }
        }
    }
    // Own counter for the slot search (r11, no call crossed: sy then takes r30 and i r29).
    for (int n = 0; n < pieceMax; n++) {
        if (p == pieces[n]) {
            pieces[n] = 0;
            break;
        }
    }
    p->state = 0;
    return 1;
}

pzlPiece* pzlBoard::rmPiece(int x, int y)
{
    pzlPiece* p = getPiece(x, y);

    if (p) {
        rmPiece(p);
    }
    return p;
}

u8* pzlBoard::cell(int x, int y)
{
    return cells + (x + y * w);
}

int pzlBoard::cellState(int x, int y)
{
    if (x >= w || x < 0 || y >= h || y < 0) {
        if ((x == -1 || x == w) && y >= -1 && y <= h) {
            return 0x43;
        }
        if ((y == -1 || y == h) && x >= -1 && x <= w) {
            return 0x43;
        }
        return 3;
    }
    return *cell(x, y);
}

void pzlBoard::clearState(u8 mask)
{
    int i;
    int j;

    for (i = 0; i < w; i++) {
        for (j = 0; j < h; j++) {
            *cell((s8) i, (s8) j) &= ~mask;
        }
    }
}

// Debug cell display. Dead (STRIP_UNUSED); its "%c" sits between pzlBoard::init's file string
// and pzlPlayer::init's strings.
static void dispCell(pzlBoard* b, int x, int y)
{
    eprintf(x, y, 0, 0, "%c", (b->cellState(x, y) & 1) ? '1' : '0');
}

// Case sizes: the switch is on an unsigned index with a `case 0` sharing the default label
// (balanced tree root 1, `cmplwi/blt` to default for 0, case bodies laid out 3, 2, 1, default).
// One `p` for both piece loops (its priority then beats `item`'s: p r31, item r30); the flag
// clear loop has its own counter (r10, no call crossed); item positions are stored halved
// (save() doubles them back).
int pzlPlayer::init(int type)
{
    int w;
    int h;
    int i;
    int extraGame;
    pzlPiece* p;

    extraGame = pG->x4FB8 == 1;
    switch ((u32) type) {
    case 3:
        w = 0xF;
        h = 8;
        break;
    case 2:
        w = 0xC;
        h = 8;
        break;
    case 1:
        w = 0xB;
        h = 7;
        break;
    case 0:
    default:
        w = 0xA;
        h = 6;
        break;
    }
    caseBoard = (pzlBoard*) __builtin_new(sizeof(pzlBoard));
    if (caseBoard == 0) {
        pLog->err(0, 0, "Can't create pzlPlayer()");
        return 0;
    }
    if (caseBoard->init(w, h, piece_max) == 0) {
        pLog->err(0, 0, "Can't create pzlPlayer()");
        __builtin_delete(caseBoard);
        return 0;
    }
    spaceBoard = (pzlBoard*) __builtin_new(sizeof(pzlBoard));
    if (spaceBoard == 0) {
        pLog->err(0, 0, "Can't create pzlPlayer()");
        caseBoard->quit();
        __builtin_delete(caseBoard);
        return 0;
    }
    if (spaceBoard->init(space_w, space_h, piece_max) == 0) {
        pLog->err(0, 0, "Can't create pzlPlayer()");
        caseBoard->quit();
        __builtin_delete(caseBoard);
        __builtin_delete(spaceBoard);
        return 0;
    }
    pieces = (pzlPiece*) __builtin_vec_new(piece_max * sizeof(pzlPiece));
    if (pieces == 0) {
        pLog->err(0, 0, "Can't create pzlPlayer()");
        caseBoard->quit();
        __builtin_delete(caseBoard);
        spaceBoard->quit();
        __builtin_delete(spaceBoard);
        return 0;
    }
    pieceNum_ = piece_max;
    {
        int j;
        for (j = 0; j < pieceNum_; j++) {
            pieces[j].flags = 0;
        }
    }
    {
        int k = 0;
        for (i = 0; i < ItemMgr.nItems; i++) {
            ItemWork* item = ItemMgr.at(i);
            int ok;
            if (item->flags & 1) {
                ok = item->type == (u8) extraGame;
            } else {
                ok = 0;
            }
            if (ok) {
                p = &pieces[k];
                PieceData* d = searchItemPieceData(item->id, piece_info);
                if (d) {
                    k++;
                    p->init(d);
                    p->x = (f32) item->x * 0.5f;
                    p->y = (f32) item->y * 0.5f;
                    p->orientation((s8) item->orient);
                    p->item = item;
                }
            }
        }
    }
    for (i = 0; i < pieceNum_; i++) {
        p = &pieces[i];
        // `!(bool)`: the flag test is `xori; andi.; bne` (the negated bool materialised).
        if (!((bool) (p->flags & 1))) {
            continue;
        }
        {
            pzlBoard* b = p->item->board ? caseBoard : spaceBoard;
            if (b->putPiece(p) == 0) {
                p->item->board = 0;
                pLog->err(0, 0, "pzlPlayer::pzlPlayer() Can't locate piece.");
            }
        }
    }
    cur = caseBoard;
    return 1;
}

void pzlPlayer::quit()
{
    if (caseBoard) {
        caseBoard->quit();
        __builtin_delete(caseBoard);
    }
    if (spaceBoard) {
        spaceBoard->quit();
        __builtin_delete(spaceBoard);
    }
    if (pieces) {
        __builtin_vec_delete(pieces);
    }
}

int pzlPlayer::pieceNum()
{
    int n = 0;
    int i;

    for (i = 0; i < pieceNum_; i++) {
        if (!(pieces[i].flags & 1)) {
            continue;
        }
        n++;
    }
    return n;
}

pzlPiece* pzlPlayer::piecePtr(int no)
{
    int n = 0;
    int i;

    for (i = 0; i < pieceNum_; i++) {
        if (!(pieces[i].flags & 1)) {
            continue;
        }
        if (n == no) {
            return &pieces[i];
        }
        n++;
    }
    return 0;
}

pzlPiece* pzlPlayer::piecePtr(ItemWork* item)
{
    int i;

    for (i = 0; i < pieceNum_; i++) {
        if (!(pieces[i].flags & 1)) {
            continue;
        }
        if (item == pieces[i].item) {
            return &pieces[i];
        }
    }
    return 0;
}

void pzlPlayer::save()
{
    int i;

    for (i = 0; i < pieceNum_; i++) {
        pzlPiece* p = &pieces[i];
        ItemWork* item;
        if (!(p->flags & 1)) {
            continue;
        }
        item = p->item;
        item->x = (s8) (p->x + p->x);
        item->y = (s8) (p->y + p->y);
        item->orient = p->orient;
        if (caseBoard->search(p)) {
            item->board = 1;
        } else if (spaceBoard->search(p)) {
            item->board = 0;
        }
    }
}

int pzlPlayer::appendExtraPiece(ItemWork* item)
{
    pzlPiece* p = 0;
    PieceData* d;
    int i;

    if (item == 0) {
        return 0;
    }
    d = searchItemPieceData(item->id, piece_info);
    if (d == 0) {
        return 0;
    }
    for (i = 0; i < pieceNum_; i++) {
        pzlPiece* q = &pieces[i];
        if (!(q->flags & 1)) {
            p = q;
            break;
        }
    }
    if (p == 0) {
        return 0;
    }
    p->init(d);
    p->item = item;
    p->x = 0.0f;
    p->y = 0.0f;
    extra = p;
    return 1;
}

int pzlPlayer::removeExtraPiece()
{
    if (extra->state & 1) {
        pzlBoard* b;
        if (caseBoard->search(extra)) {
            b = caseBoard;
        } else if (spaceBoard->search(extra)) {
            b = spaceBoard;
        } else {
            pLog->err(0, 0, "pzlPlayer::removeExtraPiece(): Piece not found.");
            return 0;
        }
        if (b->rmPiece(extra) == 0) {
            pLog->err(0, 0, "pzlPlayer::removeExtraPiece(): Can't remove piece.");
            return 0;
        }
    }
    ItemMgr.erase(extra->item);
    extra->model->push();
    extra->flags = 0;
    hand = 0;
    extra = 0;
    return 1;
}

void pzlPlayer::inHandExtraPiece()
{
    extra->state = 2;
    hand = extra;
}

void pzlPlayer::giveupExtraPiece()
{
    extra = 0;
}

// Cursor clamp after a move: written twice per axis in the original (a macro): once in the loop's
// `q == 0` else arm and once on the `p == 0` break path INSIDE the loop; the `cur < 0` compare is
// shared between the outer `||` and the inner `if` (cr7). The clamp on the break path makes the
// rotated loop's exit code (`cur += d; if (p == 0) { clamp; break; }`) longer than 20 insns, so
// jump1's duplicate_loop_exit_test never peels it and the loop is entered with a plain `b INC`
// (docs/matching.md COMPILER-DIFF #9, closed: a source form, not a compiler difference).
#define SEL_CHECK(cur, size, lo, hi, done)                                                          \
    if (b->cur < 0 || b->cur > b->size - 1) {                                                        \
        if (b->cur < 0) {                                                                            \
            ret = lo;                                                                                \
        }                                                                                            \
        if (b->cur > b->size - 1) {                                                                  \
            ret = hi;                                                                                \
        }                                                                                            \
        b->cur = save;                                                                               \
    } else {                                                                                         \
        goto done;                                                                                   \
    }

// Per-axis block locals: `d` and `save` are one pseudo per axis (half the live length), so `ret`
// (r29) is allocated before the saves (both r28) and the saves before the two `d`s (both r27).
// `d` is s8: the QImode step is a REG operand of the byte add, so expand_binop keeps it first
// (`add r0,r27,r0`); the `d != 0` test drops the extension (combine's simplify_comparison).
int pzlPlayer::selPiece(pzlBoard* b)
{
    int ret = 0;
    pzlPiece* p;

    if (Key.rep & 0x0F000000) {
        ret = 5;
    }
    {
        s8 d = 0;
        s8 save;
        if (Key.rep & 0x08000000) {
            d = -1;
        }
        if (Key.rep & 0x04000000) {
            d = 1;
        }
        if (d != 0) {
            save = b->curX;
            p = b->getPiece(save, b->curY);
            for (;;) {
                b->curX += d;
                if (p == 0) {
                    SEL_CHECK(curX, w, 3, 4, doneX);
                    break;
                }
                {
                    pzlPiece* q = b->getPiece(b->curX, b->curY);
                    if (q) {
                        if (q->item != p->item) {
                            goto doneX;
                        }
                    } else {
                        SEL_CHECK(curX, w, 3, 4, doneX);
                        goto doneX;
                    }
                }
            }
        }
    }
doneX:
    {
        s8 d = 0;
        s8 save;
        if (Key.rep & 0x01000000) {
            d = -1;
        }
        if (Key.rep & 0x02000000) {
            d = 1;
        }
        if (d != 0) {
            save = b->curY;
            p = b->getPiece(b->curX, save);
            for (;;) {
                b->curY += d;
                if (p == 0) {
                    SEL_CHECK(curY, h, 1, 2, doneY);
                    break;
                }
                {
                    pzlPiece* q = b->getPiece(b->curX, b->curY);
                    if (q) {
                        if (q->item != p->item) {
                            goto doneY;
                        }
                    } else {
                        SEL_CHECK(curY, h, 1, 2, doneY);
                        goto doneY;
                    }
                }
            }
        }
    }
doneY:
    return ret;
}

pzlPiece* pzlPlayer::ptrPiece(pzlBoard* b)
{
    return b->getPiece(b->curX, b->curY);
}

void pzlPlayer::getPiece(pzlBoard* b)
{
    pzlPiece* p = b->rmPiece(b->curX, b->curY);

    if (p) {
        p->state = 2;
        handX = p->x;
        handY = p->y;
        handOrient = p->orient;
        handBoard = cur;
    }
    hand = p;
}

int pzlPlayer::putPiece(pzlBoard* b)
{
    if (hand == 0) {
        return 0;
    }
    if (b->putPiece(hand)) {
        b->curX = (s8) hand->x;
        b->curY = (s8) hand->y;
        hand = 0;
        return 1;
    } else {
        return 0;
    }
}

int pzlPlayer::relPiece(pzlBoard* b)
{
    if (hand) {
        hand->x = handX;
        hand->y = handY;
        hand->orientation(handOrient);
        if (handBoard) {
            if (putPiece(handBoard)) {
                cur = handBoard;
                return 1;
            } else {
                cur = handBoard;
                return 0;
            }
        }
    }
    return 0;
}

int pzlPlayer::chgPiece(pzlBoard* b)
{
    pzlPiece* p;

    if (hand == 0) {
        return 0;
    }
    p = b->lapPiece(hand);
    if (p != 0) {
        b->rmPiece(p);
        b->putPiece(hand);
        p->state = 2;
        hand = p;
        handX = p->x;
        handY = p->y;
        handOrient = p->orient;
        handBoard = cur;
        return 1;
    }
    return 0;
}

// `ex = 0` after the lapPiece check (its `li` follows the call); the success path is the then-arm
// of `if (combine())` so the failing `return 0` is laid out last; `if (!used) {...} else hand = 0`.
pzlPiece* pzlPlayer::cmbPiece(pzlBoard* b)
{
    pzlPiece* p;
    pzlPiece* h;
    ItemWork* ex;
    ItemInfo info;
    int rel = 0;
    int used;

    if (hand == 0) {
        return 0;
    }
    p = b->lapPiece(hand);
    if (p == 0) {
        return 0;
    }
    ex = 0;
    h = hand;
    if (extra == p || extra == h) {
        ex = extra->item;
        itemInfo(ex->id, &info);
        if (info.type != 2 && info.type != 6) {
            return 0;
        }
    }
    itemInfo(p->item->id, &info);
    if (info.type == 9) {
        rel = 1;
    } else {
        itemInfo(h->item->id, &info);
        if (info.type == 9) {
            rel = 1;
        }
    }
    if (ItemMgr.combine(p->item, h->item, 0)) {
        if (ex) {
            giveupExtraPiece();
        }
        used = 0;
        if (!(h->item->flags & 1)) {
            used = 1;
        }
        if (!used) {
            if (rel) {
                relPiece(cur);
            }
        } else {
            hand = 0;
        }
        return p;
    }
    return 0;
}

// Structure notes (bytes): the Joy arms set `ret = 2` and `goto cursor` past the wall block (a
// `do {} while (0)` would be a loop: its invariants get hoisted), so they skip the `Key.rep & 0x0F000000` test and fall into the cursor
// update; `Joy` is read through a pointer (`&Joy` materialised in block 0); `out` is `== 1`
// (`xori; subfic; adde`); the board swap writes `ny` (0.0f on the impossible third path, the step
// is the -2.0f constant); `edge` and `step` are ints converted with the double trick; the
// `size_y < 0` clamp adds `cur->h` implicitly (int -> float, magic) where the compare casts (psq_l);
// the `dir` shuffle is a two-case switch. Pass 2: both dir switches have `case 0:` (their lower
// halves cross-jump), the compares convert the member `cur->h` directly (raw byte to psq_l) while
// the stores use the int, `caseBoard` goes through a local before the swap (load order). Pass 4
// (matched): the y clamp uses two ints. `ch` (compare arm only, single set -> r9) and `h`, declared
// with `edge` and set in BOTH y arms (`h = (int)(fabsf(...) - 1.0f) - 1` and `h = edge - sy`): a
// multi-set pseudo cannot be tied to the fix result (so the fix ties to the fctiwz temp, r9, and
// the `- 1` lands in the global r0), and its `subf` gets no r3 suggestion (`subf r0,r3,r30`). Arm 2
// re-extends and increments `edge` itself (`extsb r30,r30; addi r30,r30,1`); the volatile launder
// after the size_y call keeps those two below the call (edge crosses calls, so the scheduler has no
// anti-dependence to hold them there) and keeps the (s8) from folding away.
int pzlPlayer::movePiece()
{
    pzlPiece* p = hand;
    int ret = 0;
    JOY* joy = Joy;
    int dir;

    {
        if (Key.rep & 0x08000000) {
            if ((f32) (int) p->ver0_y() != p->ver0_y()) {
                p->y -= 0.5f;
            }
            if ((f32) (int) p->ver0_x() != p->ver0_x()) {
                p->x -= 0.5f;
            } else {
                p->x -= 1.0f;
            }
            ret = 1;
        } else if (Key.rep & 0x04000000) {
            if ((f32) (int) p->ver0_y() != p->ver0_y()) {
                p->y += 0.5f;
            }
            if ((f32) (int) p->ver0_x() != p->ver0_x()) {
                p->x += 0.5f;
            } else {
                p->x += 1.0f;
            }
            ret = 1;
        } else if (Key.rep & 0x01000000) {
            if ((f32) (int) p->ver0_x() != p->ver0_x()) {
                p->x -= 0.5f;
            }
            if ((f32) (int) p->ver0_y() != p->ver0_y()) {
                p->y -= 0.5f;
            } else {
                p->y -= 1.0f;
            }
            ret = 1;
        } else if (Key.rep & 0x02000000) {
            if ((f32) (int) p->ver0_x() != p->ver0_x()) {
                p->x += 0.5f;
            }
            if ((f32) (int) p->ver0_y() != p->ver0_y()) {
                p->y += 0.5f;
            } else {
                p->y += 1.0f;
            }
            ret = 1;
        } else if (joy->trg & 0x20) {
            p->rotate(0);
            if (fabsf((f32) (s8) p->size_y()) > (f32) (cur->h + 2)) {
                p->rotate(0);
            }
            ret = 2;
            goto cursor;
        } else if (joy->trg & 0x40) {
            p->rotate(1);
            if (fabsf((f32) (s8) p->size_y()) > (f32) (cur->h + 2)) {
                p->rotate(1);
            }
            ret = 2;
            goto cursor;
        } else if (joy->trg & 0x00C00000) {
            p->mirror(1);
            ret = 2;
            goto cursor;
        } else if (joy->trg & 0x00300000) {
            p->mirror(0);
            ret = 2;
            goto cursor;
        }
        if (Key.rep & 0x0F000000) {
            int out = cur->outPiece(p) == 1;
            int wall = cur->ckInsideWall(p) == 0;
            if (out || wall) {
                dir = 0;
                if (wall) {
                    switch (cur->wallDir) {
                    case 0:
                        dir = 0;
                        break;
                    case 1:
                        dir = 1;
                        break;
                    case 2:
                        dir = 2;
                        break;
                    case 3:
                        dir = 3;
                        break;
                    case 4:
                        dir = 4;
                        break;
                    }
                } else if (out) {
                    switch (cur->outDir) {
                    case 0:
                        break;
                    case 1:
                        dir = 1;
                        break;
                    case 2:
                        dir = 2;
                        break;
                    case 3:
                        dir = 3;
                        break;
                    case 4:
                        dir = 4;
                        break;
                    }
                }
                if (dir == 1 || dir == 2) {
                    int edge;
                    int h;
                    f32 fy;
                    pzlBoard* cb = caseBoard;
                    f32 ny = 0.0f;
                    if (cur == cb) {
                        cur = spaceBoard;
                        ny = p->y - -2.0f;
                    } else if (cur == spaceBoard) {
                        cur = cb;
                        ny = p->y + -2.0f;
                    }
                    p->y = ny;
                    if (fabsf((f32) (s8) p->size_y()) > (f32) (cur->h + 2)) {
                        p->rotate(1);
                        p->snap();
                    }
                    if (dir == 1) {
                        edge = cur->w;
                        if (p->size_x() > 0) {
                            edge -= (int) (fabsf((f32) (s8) p->size_x()) - 1.0f);
                        }
                    } else {
                        edge = -1;
                        if (p->size_x() < 0) {
                            edge = (int) (fabsf((f32) (s8) p->size_x()) - 1.0f) - 1;
                        }
                    }
                    p->x = (f32) edge + p->cx;
                    if (p->size_y() < 0) {
                        f32 vy = p->ver0_y();
                        int ch = cur->h;
                        if (vy > (f32) cur->h) {
                            p->y = (f32) ch + p->cy;
                        }
                        fy = p->ver0_y() + (f32) (p->size_y() + 1);
                        if (fy < -1.0f) {
                            h = (int) (fabsf((f32) (s8) p->size_y()) - 1.0f) - 1;
                            p->y = (f32) h + p->cy;
                        }
                    } else {
                        fy = p->ver0_y() + (f32) (p->size_y() - 1);
                        edge = cur->h;
                        if (fy > (f32) cur->h) {
                            int sy = p->size_y();
                            asm volatile("" : "+r"(edge)); // COMPILER-DIFF: the target re-extends edge after the size_y call (extsb r30,r30; addi r30,r30,1); ours proves it sign-extended and, edge crossing calls, would hoist the two above the call
                            edge = (s8) edge;
                            edge += 1;
                            h = edge - sy;
                            p->y = (f32) h + p->cy;
                        }
                        if (p->ver0_y() < -1.0f) {
                            p->y = p->cy + -1.0f;
                        }
                    }
                    if (cur->outPiece(p) == 1) {
                        if (fabsf((f32) (s8) p->size_x()) == 1.0f && fabsf((f32) (s8) p->size_y()) == 1.0f) {
                            switch (dir) {
                            case 1:
                                p->x -= 1.0f;
                                break;
                            case 2:
                                p->x += 1.0f;
                                break;
                            }
                        }
                        if (fabsf((f32) (s8) p->size_y()) == 1.0f && cur->outPiece(p) == 1) {
                            int step = -1;
                            if (p->y < (f32) (s8) (cur->h / 2)) {
                                step = 1;
                            }
                            do {
                                p->y += (f32) step;
                            } while (cur->outPiece(p) == 1);
                        }
                        if (fabsf((f32) (s8) p->size_x()) == 1.0f && cur->outPiece(p) == 1) {
                            int step = -1;
                            if (p->x < (f32) (s8) (cur->w / 2)) {
                                step = 1;
                            }
                            do {
                                p->x += (f32) step;
                            } while (cur->outPiece(p) == 1);
                        }
                    }
                } else {
                    if (fabsf((f32) (s8) p->size_y()) == 1.0f) {
                        if (dir == 3) {
                            do {
                                p->y += 1.0f;
                            } while (cur->outPiece(p) == 0);
                            p->y -= 1.0f;
                        } else {
                            do {
                                p->y -= 1.0f;
                            } while (cur->outPiece(p) == 0);
                            p->y += 1.0f;
                        }
                    } else {
                        if (dir == 3) {
                            do {
                                p->y += 1.0f;
                            } while (cur->ckInsideWall(p) == 1);
                            p->y -= 1.0f;
                        } else {
                            do {
                                p->y -= 1.0f;
                            } while (cur->ckInsideWall(p) == 1);
                            p->y += 1.0f;
                        }
                    }
                }
            }
        }
    }
cursor:
    cur->curX = (s8) (p->x + 0.5f);
    cur->curY = (s8) (p->y + 0.5f);
    if (cur->curX & 0x80) {
        cur->curX = 0;
    }
    {
        int wm = cur->w - 1;
        if (cur->curX > wm) {
            cur->curX = wm;
        }
    }
    if (cur->curY & 0x80) {
        cur->curY = 0;
    }
    {
        int hm = cur->h - 1;
        if (cur->curY > hm) {
            cur->curY = hm;
        }
    }
    return ret;
}

void pzlPlayer::rehash()
{
    int i;

    for (i = 0; i < pieceNum_; i++) {
        pzlPiece* p = &pieces[i];
        if (!(p->flags & 1)) {
            continue;
        }
        if (p->item->flags == 0) {
            if (p->state & 1) {
                if (caseBoard->search(p)) {
                    caseBoard->rmPiece(p);
                } else if (spaceBoard->search(p)) {
                    spaceBoard->rmPiece(p);
                }
            }
            p->model->push();
            p->flags = 0;
        }
    }
}

void pzlPlayer::saveCursor()
{
    saveBoard = cur;
    saveX = cur->curX;
    saveY = cur->curY;
}

void pzlPlayer::loadCursor()
{
    cur = saveBoard;
    cur->curX = saveX;
    cur->curY = saveY;
}

void pzlPlayer::salvCursor()
{
    if (spaceBoard == cur) {
        cur = caseBoard;
        cur->curX = 0;
        cur->curY = 0;
    }
}

// `num` is u16 (its copies into `rest` are plain moves and cse propagates `num` into the peeled
// first order entry, so jump2 cannot cross-jump the peeled head), `max` u16 (shorten_compare gives the
// unsigned compares); `item` is declared before `info`; each loop has its own counter (the two
// placement nests share i/j); the fill-up loop is a guarded do-while (`cmpwi nOrder,0; ble`).
// `last` is r31 in the target = an allocno that crosses a call; the three COMPILER-DIFF lines below
// give it a codeless def before get() (see docs/research/ "DOL puzzle final closer").
int PutInCase(u16 id, u16 num, int type)
{
    ItemWork item;
    ItemInfo info;
    pzlPlayer* pl;
    u16 max;
    int total;
    int i;
    int j;
    int ok = 0;
    pzlPiece* p;
    ItemWork* last;

    itemInfo(id, &info);
    if (info.type == 1 || info.type == 9) {
        if (num == 0) {
            num = 1;
        }
        max = 1;
    } else {
        if (num == 0) {
            num = info.x3;
        }
        max = info.x4;
    }
    if (num > max) {
        num = max;
        pLog->err(0, 0, "PutInCase(): Volume of ITEM(0x%02x) is OOL.", id);
    }
    ItemMgr.ordering(id);
    total = 0;
    for (int i = 0; i < ItemMgr.nOrder; i++) {
        total += max - ItemMgr.pOrder[i].item->num;
    }
    if (total >= num) {
        u16 rest = num;
        for (int i = 0; i < ItemMgr.nOrder; i++) {
            ItemWork* w = ItemMgr.pOrder[i].item;
            u16 room = max - w->num;
            if (room >= rest) {
                w->num = rest + w->num;
                break;
            }
            w->num = max;
            rest -= room;
        }
        return 1;
    }
    pl = (pzlPlayer*) __builtin_new(sizeof(pzlPlayer));
    if (pl == 0) {
        return 0;
    }
    if (pl->init(type) == 0) {
        __builtin_delete(pl);
        return 0;
    }
    ItemMgr.construct(&item, id);
    item.num = num;
    item.flags |= 1;
    pl->appendExtraPiece(&item);
    pl->inHandExtraPiece();
    {
        p = pl->extra;
        s8 bh = pl->caseBoard->h;
        s8 bw = pl->caseBoard->w;
        for (i = 0; i < bh; i++) {
            for (j = 0; j < bw; j++) {
                p->x = (f32) j + p->cx;
                p->y = (f32) i + p->cy;
                if (pl->putPiece(pl->caseBoard)) {
                    ok = 1;
                    goto placed;
                }
            }
        }
        p->orientation(1);
        for (i = 0; i < bh; i++) {
            for (j = 0; j < bw; j++) {
                p->x = (f32) j + p->cx;
                p->y = (f32) i + p->cy;
                if (pl->putPiece(pl->caseBoard)) {
                    ok = 1;
                    goto placed;
                }
            }
        }
    }
placed:
    pl->save();
    last = (ItemWork*) p; // COMPILER-DIFF: codeless copy of the dead r31 value: `last` then crosses the get() call and takes the first callee-saved reg, r31
    asm("" : "+r"(p)); // COMPILER-DIFF: makes the copy unavailable to gcse's copy propagation; flow deletes it (p is dead)
    if (ok) {
        u16 rest;
        int n;
        ItemMgr.ordering(id);
        rest = num;
        n = 0;
        if (ItemMgr.nOrder > 0) {
            do {
                ItemWork* w = ItemMgr.pOrder[n].item;
                u16 room = max - w->num;
                w->num = max;
                rest -= room;
                n++;
            } while (n < ItemMgr.nOrder);
        }
        ItemMgr.get(id, rest);
        asm("" : "=m"(item.x) : "r"(last)); // COMPILER-DIFF: keeps the copy live across the call (an output-less asm is volatile and flushes cse's ItemMgr high)
        last = ItemMgr.pLast;
        if (last) {
            last->x = item.x;
            last->y = item.y;
            last->orient = item.orient;
            last->board = item.board;
        }
    }
    pl->quit();
    __builtin_delete(pl);
    return ok;
}

asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
