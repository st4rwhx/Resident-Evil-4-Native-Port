#ifndef _CHARPIPELINE_TEXPALETTE_H_
#define _CHARPIPELINE_TEXPALETTE_H_

#include <dolphin/gx/GXStruct.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    u16 numEntries;
    u8 unpacked;
    u8 pad8;
    GXTlutFmt format;
    // Portage : quatre octets, comme sur le disque. Voir port/disc_ptr.h.
    DISC_PTR(unsigned char) data;
} CLUTHeader, *CLUTHeaderPtr;

typedef struct {
    u16 height;
    u16 width;
    u32 format;
    // Portage : quatre octets, comme sur le disque. Voir port/disc_ptr.h.
    DISC_PTR(unsigned char) data;
    GXTexWrapMode wrapS; 
    GXTexWrapMode wrapT;
    GXTexFilter minFilter;
    GXTexFilter magFilter;
    f32 LODBias;
    u8 edgeLODEnable;
    u8 minLOD;
    u8 maxLOD;
    u8 unpacked;
} TEXHeader, *TEXHeaderPtr;

typedef struct {
    // Portage : quatre octets, comme sur le disque. Voir port/disc_ptr.h.
    DISC_PTR(TEXHeader) textureHeader;
    DISC_PTR(CLUTHeader) CLUTHeader;
} TEXDescriptor, *TEXDescriptorPtr;

typedef struct {
    u32 versionNumber;
    u32 numDescriptors;
    // Portage : quatre octets, comme sur le disque. Voir port/disc_ptr.h.
    DISC_PTR(TEXDescriptor) descriptorArray;
} TEXPalette, *TEXPalettePtr;

void TEXGetPalette(TEXPalettePtr* pal, char* name);
TEXDescriptorPtr TEXGet(TEXPalettePtr pal, u32 id);
void TEXReleasePalette(TEXPalettePtr* pal);
void TEXGetGXTexObjFromPalette(TEXPalettePtr pal, GXTexObj* to, u32 id);
void TEXGetGXTexObjFromPaletteCI(TEXPalettePtr pal, GXTexObj* to, GXTlutObj* tlo, GXTlut tluts, u32 id);

#ifdef __cplusplus
}
#endif

#endif
