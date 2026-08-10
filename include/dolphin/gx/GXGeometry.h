#ifndef _DOLPHIN_GX_GXGEOMETRY_H_
#define _DOLPHIN_GX_GXGEOMETRY_H_

#include <dolphin/gx/GXEnum.h>
#include <dolphin/gx/GXStruct.h>

#ifdef __cplusplus
extern "C" {
#endif

void __GXCalculateVLim();
void GXSetVtxDesc(GXAttr attr, GXAttrType type);
void GXSetVtxDescv(const GXVtxDescList* attrPtr);
void GXClearVtxDesc(void);
void GXSetVtxAttrFmt(GXVtxFmt vtxfmt, GXAttr attr, GXCompCnt cnt, GXCompType type, u8 frac);
void GXSetVtxAttrFmtv(GXVtxFmt vtxfmt, const GXVtxAttrFmtList* list);
void GXSetArray(GXAttr attr, void* base_ptr, u8 stride);
void GXInvalidateVtxCache(void);
void GXSetTexCoordGen2(GXTexCoordID dst_coord, GXTexGenType func, GXTexGenSrc src_param, u32 mtx, GXBool normalize, u32 pt_texmtx);
void GXSetNumTexGens(u8 nTexGens);

static inline void GXSetTexCoordGen(GXTexCoordID dst_coord, GXTexGenType func, GXTexGenSrc src_param, u32 mtx) {
    GXSetTexCoordGen2(dst_coord, func, src_param, mtx, GX_FALSE, GX_PTIDENTITY);
}

void GXBegin(GXPrimitive type, GXVtxFmt vtxfmt, u16 nverts);

#ifdef RE4_PORT
// Portage : sur le materiel, GXEnd ne soumet rien - la geometrie part par le write-gather pipe
// au fil des GXPosition/GXColor, et cette fonction n'est qu'une assertion de debug. Un portage
// doit au contraire savoir **quand la primitive est finie** pour emettre son appel de dessin.
// C'est donc ici le point d'accroche, au meme endroit et de la meme facon que GXVert.h bascule
// deja ses macros en fonctions externes sous `#if DEBUG`.
//
// (L'assertion d'origine est abandonnee : elle redeclare OSPanic sans `const`, en conflit avec
// os.h pour un compilateur moderne.)
void port_gx_end(void);
static inline void GXEnd(void) { port_gx_end(); }
#else
static inline void GXEnd(void) {
#if DEBUG
    extern GXBool __GXinBegin;
    extern void OSPanic(char* file, int line, char* msg, ...);
    if (!__GXinBegin) {
        OSPanic(__FILE__, 118, "GXEnd: called without a GXBegin");
    }
    __GXinBegin = GX_FALSE;
#endif
}
#endif

void GXSetLineWidth(u8 width, GXTexOffset texOffsets);
void GXSetPointSize(u8 pointSize, GXTexOffset texOffsets);
void GXEnableTexOffsets(GXTexCoordID coord, u8 line_enable, u8 point_enable);

#ifdef __cplusplus
}
#endif

#endif
