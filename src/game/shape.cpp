#include "types.h"
#include "vec.h"
#include "db_log.h"
#include "main_mem.h"
#include "model.h"

// game/motion.cpp
struct HermiteParam {
    f32 t;           // 0x00  current frame
    f32 frames;      // 0x04  total frames
    s32 x8;          // 0x08  = 2
    u8 type;         // 0x0C  interpolation type
    void* table;     // 0x10  key table
};
extern "C" void HermiteInterpolation(HermiteParam* p, void* result, void* work);

extern "C" {
void* memcpy(void* dst, const void* src, unsigned int n);
int ShapeMove(cModelInfo* info);
void SetOriginalShape(cModelInfo* info);
void ClrShape(cModel* m);
int SetShape(cModelInfo* info, ShapeData* data, f32 rate);
void ResetShape(cModelInfo* info, void* dst);
void CalculateShape_new(cModelInfo* info, ShapeData* data, f32 rate, u8* dst);
}

void ShapeEnd(void* work);

// Start a shape animation on a model part. Returns 0 when the data has no frames.
int ShapeSet(void* work, int frame, void* data, int flags)
{
    cModelInfo* info = (cModelInfo*) work;
    ShapeData* sd = (ShapeData*) data;

    info->shapeFrame = frame;
    info->pShape = sd;
    info->shapeFlags = flags;
    if (flags & 4) {
        info->shapeFrame = sd->nFrame - 1;
    }
    if (sd->nFrame == 0) {
        pLog->err(0, 0, "ShapeSet : frame 0 data");
        ShapeEnd(work);
        return 0;
    }
    return 0;
}

// Advance the shape animations of a parts list.
int ShapeMove(cModelInfo* info)
{
    cModelInfo* p;

    if (info == NULL) {
        return 0;
    }
    for (p = info; p != NULL; p = p->pNext) {
        if (p->pShape != NULL) {
            u32 flags;
            ShapeData* sd;

            SetOriginalShape(p);
            SetShape(p, p->pShape, (f32) p->shapeFrame);
            flags = p->shapeFlags;
            sd = p->pShape;
            if (flags & 4) {
                p->shapeFrame--;
                if (p->shapeFrame < 0) {
                    if (flags & 1) {
                        p->shapeFrame = 0;
                        p->shapeFrame = sd->nFrame - 1;
                    } else if (flags & 2) {
                        p->shapeFrame++;
                    } else {
                        ShapeEnd(p);
                    }
                }
            } else {
                p->shapeFrame++;
                if (p->shapeFrame >= sd->nFrame - 1) {
                    if (flags & 1) {
                        p->shapeFrame = 0;
                    } else if (flags & 2) {
                        p->shapeFrame--;
                    } else {
                        ShapeEnd(p);
                    }
                }
            }
        }
    }
    return 1;
}

void ShapeEnd(void* work)
{
    cModelInfo* info = (cModelInfo*) work;

    info->shapeFrame = 0;
    info->pShape = NULL;
    SetOriginalShape(info);
}

void SetOriginalShape(cModelInfo* info)
{
    int i;

    for (i = 0; i < 5; i++) {
        memclr_asm(&info->shape[i], sizeof(ShapeKey));
    }
}

// Stop the shape animations of every part of a model.
void ClrShape(cModel* m)
{
    cModelInfo* info;

    for (info = m->pInfo; info != NULL; info = info->pNext) {
        if (info->be_flag & 2) {
            info->shapeFrame = 0;
            info->pShape = NULL;
            info->shapeFlags = 0;
            SetOriginalShape(info);
        }
    }
}

// Register a shape at `rate` in the first free channel. Returns 1 when rate == frame count.
int SetShape(cModelInfo* info, ShapeData* data, f32 rate)
{
    int i = 0;
    int n;

    if (info->shape[i].data == NULL) {
        info->shape[i].rate = rate;
        info->shape[i].data = data;
    } else {
    retry:
        i++;
        if (i <= 4) {
            if (info->shape[i].data != NULL) {
                goto retry;
            }
            info->shape[i].rate = rate;
            info->shape[i].data = data;
        }
    }
    n = data->nFrame;
    return (f32) n < rate;
}

// Restore the original vertex positions.
void ResetShape(cModelInfo* info, void* dst)
{
    memcpy(dst, info->pData->vtxOrig, info->pData->nVtx * 8);
}

// Shape evaluation work (0xD0 bytes, cleared).
struct ShapeWork {
    ShapeData* data;   // 0x00
    s32* table;        // 0x04  per channel key tables (relocated)
    u8 pad_8[0x18];
    f32 frames;        // 0x20  frame count + 1
    f32 rate;          // 0x24
    u8 pad_28[8];
    u8 num;            // 0x30
    u8 pad_31[3];
    u8* idx;           // 0x34  shape table index per channel
    u16* flags;        // 0x38  per channel flags
    u8 pad_3C[4];
    u16 x40;           // 0x40
    u8 pad_42[2];
    u32 x44;           // 0x44
    u8 pad_48[0x7C];
    u8 xC4;            // 0xC4
    u8 pad_C5[0xB];
};

struct ShapeEntry {
    u32 ofs;     // 0x00  offset of the delta list from the shape table
    s32 num;     // 0x04  entries: vertex index, dx, dy, dz (s16 each)
};

// Apply the shape `data` at `rate` to the vertex buffer `dst` (8-byte vertices, s16 xyz).
#define PSQ_L_S16(p) ({ f32 f_; asm volatile("psq_l %0,0(%1),1,5" : "=f"(f_) : "b"(p)); f_; })
#define PSQ_ST_S16(f, p) asm volatile("psq_st %0,0(%1),1,5" : : "f"(f), "b"(p) : "memory")

void CalculateShape_new(cModelInfo* info, ShapeData* data, f32 rate, u8* dst)
{
    ShapeWork work;
    ShapeWork* w = &work;
    HermiteParam prm;
    HermiteParam* pp = &prm;
    f32 result[4];
    s16 tmp[1];
    u8 out[8];
    u32 i;
    s32* p;

    memclr_asm(w, sizeof(ShapeWork));
    w->data = data;
    w->frames = (f32) (data->nFrame & 0x3FFF) + 1.0f;
    w->num = w->data->num;
    w->flags = (u16*) ((u8*) w->data + 3);
    w->idx = (u8*) w->data + (w->num * 2 + 3);
    p = (s32*) (w->idx + w->num);
    p = (s32*) (((u32) p + 3) & ~3);
    if (*++p >= 0) {
        for (i = 0; i < w->num; i++) {
            p[i] += (u32) w->data;
        }
    }
    w->table = p;
    w->x44 = 0;
    w->x40 = 0;
    w->xC4 = 0;
    w->rate = rate;

    if (dst != NULL) {
        ShapeEntry* tbl = (ShapeEntry*) (info->pData->shapeOfs + (u32) info->pData + 4);

        pp->t = rate;
        pp->frames = w->frames;
        pp->x8 = 2;
        for (i = 0; i < w->num; i++) {
            if (w->flags[i] & 4) {
                f32 v;
                pp->type = w->flags[i] >> 12;
                pp->table = (void*) w->table[i];
                memclr_asm(out, 6);
                HermiteInterpolation(pp, result, out);
                v = result[1] / 100.0f;
                if (info->shapeFlags & 8) {
                    v *= 1.37f;
                }
                if (v != 0.0f) {
                    ShapeEntry* e = (ShapeEntry*) (w->idx[i] * 8 + (u32) tbl);
                    s16* src = (s16*) (e->ofs + (u32) tbl);
                    s32 num = e->num;
                    s32 n;

                    for (n = 0; n < num; n++) {
                        s16* vtx = (s16*) (dst + *src++ * 8);
                        PSQ_ST_S16(PSQ_L_S16(src++) * v, tmp);
                        *vtx++ += tmp[0];
                        PSQ_ST_S16(PSQ_L_S16(src++) * v, tmp);
                        *vtx++ += tmp[0];
                        PSQ_ST_S16(PSQ_L_S16(src++) * v, tmp);
                        *vtx += tmp[0];
                    }
                }
            }
        }
    }
}

// Never called (superseded by CalculateShape_new): the original linker dropped the body but kept
// its constant pool (the double 0.0 at the end of .rodata).
static void CalculateShape(cModelInfo* info, ShapeData* data, f32 rate, u8* dst)
{
    if (rate != 0.0) {
        CalculateShape_new(info, data, rate, dst);
    }
}
