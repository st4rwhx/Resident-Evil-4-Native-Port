#include <dolphin/dvd.h>
#include <dolphin/gx.h>
#include <dolphin/os.h>
#include <charPipeline/fileCache.h>
#include <charPipeline/texPalette.h>

static void LoadTexPalette(TEXPalettePtr* pal, char* name);
static void UnpackTexPalette(TEXPalettePtr pal);
static void TexFreeFunc(TEXPalettePtr* pal);

void TEXGetPalette(TEXPalettePtr* pal, char* name) {
    void* p = TexFreeFunc;

    if (DOCacheInitialized) {
        *pal = (TEXPalettePtr)DSGetCacheObj(&DODisplayCache, name);
    }

    if (!*pal) {
        LoadTexPalette(pal, name);
        if (DOCacheInitialized) {
            DSAddCacheNode(&DODisplayCache, name, (Ptr)*pal, p);
            DSGetCacheObj(&DODisplayCache, name);
        }
    }
}

static void LoadTexPalette(TEXPalettePtr* pal, char* name) {
    DVDFileInfo dfi;

    DVDOpen(name, &dfi);
    *pal = OSAlloc(OSRoundUp32B(dfi.length));
    DVDReadPrio(&dfi, *pal, OSRoundUp32B(dfi.length), 0, 2);
    DVDClose(&dfi);
    UnpackTexPalette(*pal);
}

#define PALETTE_VERSION 0x20AF30

static void UnpackTexPalette(TEXPalettePtr pal) {
    u16 i;

    if (pal->versionNumber != PALETTE_VERSION) {
        OSPanic(__FILE__, 86, "invalid version number for texture palette");
    }

    // N7 : champs de quatre octets (DISC_PTR) ; decalage + adresse du fichier, en u32 comme sur la console.
    DISC_SET(pal->descriptorArray, (TEXDescriptorPtr) port_ptr64((u32) pal->descriptorArray + port_ptr32(pal)));
    for (i = 0; i < pal->numDescriptors; i++) {
        TEXDescriptorPtr d = &DISC_GET(TEXDescriptor, pal->descriptorArray)[i];
        if (d->textureHeader) {
            TEXHeaderPtr h;
            DISC_SET(d->textureHeader, (TEXHeaderPtr) port_ptr64(port_ptr32(pal) + (u32) d->textureHeader));
            h = DISC_GET(TEXHeader, d->textureHeader);
            if (!h->unpacked) {
                DISC_SET(h->data, (unsigned char*) port_ptr64(port_ptr32(pal) + (u32) h->data));
                h->unpacked = TRUE;
            }
        }

        if (d->CLUTHeader) {
            CLUTHeaderPtr c;
            DISC_SET(d->CLUTHeader, (CLUTHeaderPtr) port_ptr64(port_ptr32(pal) + (u32) d->CLUTHeader));
            c = DISC_GET(CLUTHeader, d->CLUTHeader);
            if (!c->unpacked) {
                DISC_SET(c->data, (unsigned char*) port_ptr64(port_ptr32(pal) + (u32) c->data));
                c->unpacked = TRUE;
            }
        }
    }
}

TEXDescriptorPtr TEXGet(TEXPalettePtr pal, u32 id) {
    ASSERTMSGLINE(147, id < pal->numDescriptors, "GetTexture():  Texture Not Found ");
    return &DISC_GET(TEXDescriptor, pal->descriptorArray)[id];
}

static void TexFreeFunc(TEXPalettePtr* pal) {
    OSFree(*pal);
    *pal = NULL;
}

void TEXReleasePalette(TEXPalettePtr* pal) {
    if (DOCacheInitialized) {
        DSReleaseCacheObj(&DODisplayCache, (Ptr)*pal);
    } else {
        OSFree(*pal);
        *pal = NULL;
    }
}

void TEXGetGXTexObjFromPalette(TEXPalettePtr pal, GXTexObj* to, u32 id) {
    TEXDescriptorPtr tdp;
    GXBool mipMap;

    tdp = TEXGet(pal, id);
    if (DISC_GET(TEXHeader, tdp->textureHeader)->minLOD == DISC_GET(TEXHeader, tdp->textureHeader)->maxLOD) {
        mipMap = GX_FALSE;
    } else {
        mipMap = GX_TRUE;
    }
    GXInitTexObj(to, DISC_GET(void, DISC_GET(TEXHeader, tdp->textureHeader)->data), DISC_GET(TEXHeader, tdp->textureHeader)->width, DISC_GET(TEXHeader, tdp->textureHeader)->height, DISC_GET(TEXHeader, tdp->textureHeader)->format, DISC_GET(TEXHeader, tdp->textureHeader)->wrapS, DISC_GET(TEXHeader, tdp->textureHeader)->wrapT, mipMap);
    GXInitTexObjLOD(to, DISC_GET(TEXHeader, tdp->textureHeader)->minFilter, DISC_GET(TEXHeader, tdp->textureHeader)->magFilter, DISC_GET(TEXHeader, tdp->textureHeader)->minLOD, DISC_GET(TEXHeader, tdp->textureHeader)->maxLOD, DISC_GET(TEXHeader, tdp->textureHeader)->LODBias, GX_DISABLE, DISC_GET(TEXHeader, tdp->textureHeader)->edgeLODEnable, GX_ANISO_1);
}

void TEXGetGXTexObjFromPaletteCI(TEXPalettePtr pal, GXTexObj* to, GXTlutObj* tlo, GXTlut tluts, u32 id) {
    GXBool mipMap;
    TEXDescriptorPtr tdp;

    tdp = TEXGet(pal, id);
    if (DISC_GET(TEXHeader, tdp->textureHeader)->minLOD == DISC_GET(TEXHeader, tdp->textureHeader)->maxLOD) {
        mipMap = GX_FALSE;
    } else {
        mipMap = GX_TRUE;
    }
    GXInitTlutObj(tlo, DISC_GET(void, DISC_GET(CLUTHeader, tdp->CLUTHeader)->data), DISC_GET(CLUTHeader, tdp->CLUTHeader)->format, DISC_GET(CLUTHeader, tdp->CLUTHeader)->numEntries);
    GXInitTexObjCI(to, DISC_GET(void, DISC_GET(TEXHeader, tdp->textureHeader)->data), DISC_GET(TEXHeader, tdp->textureHeader)->width, DISC_GET(TEXHeader, tdp->textureHeader)->height, DISC_GET(TEXHeader, tdp->textureHeader)->format, DISC_GET(TEXHeader, tdp->textureHeader)->wrapS, DISC_GET(TEXHeader, tdp->textureHeader)->wrapT, mipMap, tluts);
    GXInitTexObjLOD(to, DISC_GET(TEXHeader, tdp->textureHeader)->minFilter, DISC_GET(TEXHeader, tdp->textureHeader)->magFilter, DISC_GET(TEXHeader, tdp->textureHeader)->minLOD, DISC_GET(TEXHeader, tdp->textureHeader)->maxLOD, DISC_GET(TEXHeader, tdp->textureHeader)->LODBias, GX_DISABLE, DISC_GET(TEXHeader, tdp->textureHeader)->edgeLODEnable, GX_ANISO_1);
}
