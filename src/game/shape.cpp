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
    P32<void> table; // 0x10  key table
};
extern "C" void HermiteInterpolation(HermiteParam* p, void* result, void* work);

extern "C" {
#ifndef RE4_PORT   // portage : port/port_prelude.h le declare deja, avec la bonne taille
void* memcpy(void* dst, const void* src, unsigned int n);
#endif
int ShapeMove(cModelInfo* info);
void SetOriginalShape(cModelInfo* info);
void ClrShape(cModel* m);
int SetShape(cModelInfo* info, ShapeData* data, f32 rate);
void ResetShape(cModelInfo* info, void* dst);
void CalculateShape_new(cModelInfo* info, ShapeData* data, f32 rate, u8* dst);
}

void ShapeEnd(void* work);

#ifdef RE4_PORT
// Portage : sonde du Studio pour les visages qui parlent (cinematiques, radio). Signale chaque animation de
// formes lancee, puis (reglage sonde_formes 1) une fois par seconde environ, ce que CalculateShape_new applique vraiment : canaux actifs,
// canaux de poids non nul, poids extreme, sommets touches et plus grand deplacement. Sans effet sur le jeu.
extern "C" void port_studio_event(const char* fmt, ...);
extern "C" int port_knob(const char* name, int def);
static u32 s_shpCalls, s_shpActive, s_shpNonZero, s_shpVtx;
static f32 s_shpMaxV, s_shpMaxD;
#endif

// Start a shape animation on a model part. Returns 0 when the data has no frames.
int ShapeSet(void* work, int frame, void* data, int flags)
{
    cModelInfo* info = (cModelInfo*) work;
    ShapeData* sd = (ShapeData*) data;

    info->shapeFrame = frame;
    info->pShape = sd;
    info->shapeFlags = flags;
#ifdef RE4_PORT
    port_studio_event("forme_lancee info=%p data=%p images=%u canaux=%u image=%d drapeaux=%x sommets=%u table=%x",
                      (void*) info, (void*) sd, (unsigned) (sd->nFrame & 0x3FFF), (unsigned) sd->num, frame, flags,
                      info->pData ? (unsigned) info->pData->nVtx : 0u, info->pData ? (unsigned) info->pData->shapeOfs : 0u);
#endif
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
    P32<ShapeData> data; // 0x00
    P32<s32> table;    // 0x04  per channel key tables (relocated)
    u8 pad_8[0x18];
    f32 frames;        // 0x20  frame count + 1
    f32 rate;          // 0x24
    u8 pad_28[8];
    u8 num;            // 0x30
    u8 pad_31[3];
    P32<u8> idx;       // 0x34  shape table index per channel
    P32<u16> flags;    // 0x38  per channel flags
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
#ifdef RE4_PORT
// Portage : lecture dequantifiee par un GQR. main.cpp programme GQR2 = u8 et
// GQR5 = s16, tous deux avec un exposant d echelle nul : ces instructions se
// reduisent donc a une conversion de type, ecrite ici explicitement.
#define PSQ_L_S16(p) ((f32) *(const s16*) (p))
#else
#define PSQ_L_S16(p) ({ f32 f_; asm volatile("psq_l %0,0(%1),1,5" : "=f"(f_) : "b"(p)); f_; })
#endif
#ifdef RE4_PORT
// Portage : ecriture quantifiee vers s16 par GQR5 (echelle nulle). Le materiel sature
// aux bornes du type et tronque vers zero ; les deux sont reproduits ici, car un
// debordement silencieux se lirait comme un sommet parti a l autre bout de la salle.
// **Une fonction, pas une macro** : l'appelant passe `PSQ_L_S16(src++) * v`, et la macro d'origine evaluait son
// argument jusqu'a trois fois, donc avancait `src` de deux a trois cases au lieu d'une. La liste des deformations
// (indice de sommet, dx, dy, dz) etait lue de travers : des ecarts pris pour des indices projetaient des sommets du
// haut de la tete au loin (pointes autour des yeux et des cheveux en cinematique) et la bouche ne recevait jamais
// ses deformations (visages muets). Trouve le 15/11 avec le Studio (reglage formes_off, sonde peau_avant).
static inline void port_psq_st_s16(f32 f, void* p)
{
    *(s16*) p = (s16) (f > 32767.0f ? 32767.0f : (f < -32768.0f ? -32768.0f : f));
}
#define PSQ_ST_S16(f, p) port_psq_st_s16((f), (p))
#else
#define PSQ_ST_S16(f, p) asm volatile("psq_st %0,0(%1),1,5" : : "f"(f), "b"(p) : "memory")
#endif

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
    p = (s32*)port_ptr64((((u32)(uintptr_t)(p) + 3) & ~3));
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
        ShapeEntry* tbl = (ShapeEntry*)port_ptr64((info->pData->shapeOfs + (u32) info->pData + 4));

        pp->t = rate;
        pp->frames = w->frames;
        pp->x8 = 2;
#ifdef RE4_PORT
        const int sonde = port_knob("sonde_formes", 0);
        if (sonde && ++s_shpCalls % 60 == 0) {
            port_studio_event("forme_appliquee appels=%u canaux_actifs=%u non_nuls=%u poids_max=%.3f sommets=%u depl_max=%.1f image=%.1f/%.0f",
                              (unsigned) s_shpCalls, (unsigned) s_shpActive, (unsigned) s_shpNonZero, (double) s_shpMaxV,
                              (unsigned) s_shpVtx, (double) s_shpMaxD, (double) rate, (double) w->frames);
            s_shpActive = s_shpNonZero = s_shpVtx = 0;
            s_shpMaxV = s_shpMaxD = 0.0f;
        }
#endif
        for (i = 0; i < w->num; i++) {
            if (w->flags[i] & 4) {
                f32 v;
                pp->type = w->flags[i] >> 12;
                pp->table = (void*)port_ptr64(w->table[i]);
                memclr_asm(out, 6);
                HermiteInterpolation(pp, result, out);
                v = result[1] / 100.0f;
                if (info->shapeFlags & 8) {
                    v *= 1.37f;
                }
#ifdef RE4_PORT
                if (sonde)
                    ++s_shpActive;
                if (sonde && v != 0.0f) {
                    ++s_shpNonZero;
                    if ((v < 0 ? -v : v) > s_shpMaxV)
                        s_shpMaxV = v < 0 ? -v : v;
                    const ShapeEntry* pe = (const ShapeEntry*)port_ptr64((w->idx[i] * 8 + (u32)(uintptr_t)(tbl)));
                    s_shpVtx += (u32) pe->num;
                    const s16* ps = (const s16*)port_ptr64((pe->ofs + (u32)(uintptr_t)(tbl)));
                    for (s32 k = 0; k < pe->num; ++k) {
                        for (int c = 1; c < 4; ++c) {
                            const f32 d = (f32) ps[k * 4 + c] * v;
                            if ((d < 0 ? -d : d) > s_shpMaxD)
                                s_shpMaxD = d < 0 ? -d : d;
                        }
                    }
                }
#endif
                if (v != 0.0f) {
                    ShapeEntry* e = (ShapeEntry*)port_ptr64((w->idx[i] * 8 + (u32)(uintptr_t)(tbl)));
                    s16* src = (s16*)port_ptr64((e->ofs + (u32)(uintptr_t)(tbl)));
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
