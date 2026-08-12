#include <libc/string.h>

#include <dolphin/gx.h>
#include <dolphin/os.h>

#include "__gx.h"
#ifdef RE4_PORT
#include <dlfcn.h>
#endif

static __GXFifoObj DisplayListFifo;
static volatile __GXFifoObj* OldCPUFifo;
static GXData __savedGXdata;

void GXBeginDisplayList(void* list, u32 size) {
    __GXFifoObj* CPUFifo = (__GXFifoObj*)GXGetCPUFifo();

    CHECK_GXBEGIN(137, "GXBeginDisplayList");
    ASSERTMSGLINE(138, !__GXData->inDispList, "GXBeginDisplayList: display list already in progress");
    ASSERTMSGLINE(139, (size & 0x1F) == 0, "GXBeginDisplayList: size is not 32 byte aligned");
    ASSERTMSGLINE(140, ((u32)list & 0x1F) == 0, "GXBeginDisplayList: list is not 32 byte aligned");

    if (__GXData->dirtyState != 0) {
        __GXSetDirtyState();
    }

    if (__GXData->dlSaveContext != 0) {
        memcpy(&__savedGXdata, __GXData, sizeof(__savedGXdata));
    }

    DisplayListFifo.base = (u8*)list;
    DisplayListFifo.top = (u8*)list + size - 4;
    DisplayListFifo.size = size;
    DisplayListFifo.count = 0;
    DisplayListFifo.rdPtr = list;
    DisplayListFifo.wrPtr = list;
    __GXData->inDispList = 1;
    GXSaveCPUFifo((GXFifoObj*)CPUFifo);
    OldCPUFifo = CPUFifo;
    GXSetCPUFifo((GXFifoObj*)&DisplayListFifo);
    GXResetWriteGatherPipe();
#ifdef RE4_PORT   // portage : le tuyau ecrit dans la liste, plus dans le flux de l'image (port/gx_sdk.cpp)
    {
        extern void port_wg_begin_list(void* list, u32 size);
        port_wg_begin_list(list, size);
    }
#endif
}

u32 GXEndDisplayList(void) {
    u32 ov;
#ifdef DEBUG
    u32 reg;
#endif
    BOOL enabled;
    u32 cpenable;

    CHECK_GXBEGIN(195, "GXEndDisplayList");
    ASSERTMSGLINE(196, __GXData->inDispList == TRUE, "GXEndDisplayList: no display list in progress");
    GXFlush();
#ifdef RE4_PORT   // portage : octets ecrits dans la liste, debordement si elle n'a pas suffi
    {
        extern u32 port_wg_end_list(int* overflow);
        int over = 0;
        DisplayListFifo.count = port_wg_end_list(&over);
        ov = (u32) over;
    }
#elif defined(DEBUG)
    reg = GX_GET_PI_REG(5);
    ov = (reg >> 26) & 1;
#else
    ov = (GX_GET_PI_REG(5) >> 26) & 1;
#endif
#ifndef RE4_PORT
    __GXSaveCPUFifoAux(&DisplayListFifo);
#endif
    ASSERTMSGLINE(213, !ov, "GXEndDisplayList: display list commands overflowed buffer");
    GXSetCPUFifo((GXFifoObj*)OldCPUFifo);

    if (__GXData->dlSaveContext != 0) {
        enabled = OSDisableInterrupts();
        cpenable = __GXData->cpEnable;
        memcpy(__GXData, &__savedGXdata, sizeof(*__GXData));
        __GXData->cpEnable = cpenable;
        OSRestoreInterrupts(enabled);
    }

    __GXData->inDispList = 0;
    if (!ov) {
        return DisplayListFifo.count;
    }

    return 0;
}

void GXCallDisplayList(void* list, u32 nbytes) {
    CHECK_GXBEGIN(254, "GXCallDisplayList");
    ASSERTMSGLINE(255, !__GXData->inDispList, "GXCallDisplayList: display list already in progress");
    ASSERTMSGLINE(256, (nbytes & 0x1F) == 0, "GXCallDisplayList: nbytes is not 32 byte aligned");
    ASSERTMSGLINE(257, ((u32)list & 0x1F) == 0, "GXCallDisplayList: list is not 32 byte aligned");

    if (__GXData->dirtyState != 0) {
        __GXSetDirtyState();
    }

#if DEBUG
    __GXShadowDispList(list, nbytes);
#endif

    if (*(u32*)&__GXData->vNumNot == 0) {  // checks both vNum and bpSent
        __GXSendFlushPrim();
    }

#ifdef RE4_PORT
    {   /* diagnostic N3 : liste hors de MEM1 (le GPU ne la lirait pas) : appelant releve */
        extern void OSReport(const char* fmt, ...);
        static void* seen[16];
        static int nseen;
        u32 a = (u32) list;
        if (a < 0x80000000u || a >= 0x82000000u) {
            void* ra = __builtin_return_address(0);
            int i, dup = 0;
            for (i = 0; i < nseen; ++i) dup |= seen[i] == ra;
            if (!dup && nseen < 16) {
                seen[nseen++] = ra;
                {
                    Dl_info di, dl;
                    int ok1 = dladdr(ra, &di), ok2 = dladdr(list, &dl);
                    OSReport("[DLHORS] liste %08x taille %x appelant %p (%s +%x, base %p) ; liste dans %s +%x\n", a, nbytes, ra,
                             ok1 && di.dli_sname ? di.dli_sname : "?", ok1 && di.dli_saddr ? (u32) ((char*) ra - (char*) di.dli_saddr) : 0,
                             ok1 ? di.dli_fbase : 0, ok2 && dl.dli_sname ? dl.dli_sname : "?",
                             ok2 && dl.dli_saddr ? (u32) ((char*) list - (char*) dl.dli_saddr) : 0);
                }
            }
        }
    }
#endif
    GX_WRITE_U8(GX_CMD_CALL_DL);
    GX_WRITE_U32(list);
    GX_WRITE_U32(nbytes);
#ifdef RE4_PORT
    {   /* decodage a la soumission (port/gx_sdk.cpp) : les sommets lus avant que le jeu ne reecrive son tampon */
        extern void port_gx_submit(void);
        port_gx_submit();
    }
#endif
}
