#include "SystemInit.h"

// --- Matching status: in progress (first attempt) ---
// systemStartInit, 801533F4-80153554, 0x164 bytes / ~89 instructions.
// The top-level engine bootstrap: clears the global system work buffer,
// brings up every subsystem in a fixed order (OS/exception/video/DVD/
// memory/render/sound/FMV/message), then returns. Almost entirely a
// straight-line sequence of bl's with two small conditionals and one block
// of GQR (paired-single quantization register) setup that needs inline
// asm - not expressible in portable C++.
//
// Two globals whose layout isn't recovered beyond what this function
// touches:
//   g_systemWork (lbl_80314BC8, .sdata, 4-byte POINTER variable) - cleared
//     (0x8680 bytes) via memclr_asm, then ->field0 read/written as a flag
//     (0 = normal, 1 = "debug/dev mode", set when OSGetConsoleType() has
//     the 0xf0000000 bits set, i.e. any devkit console type)
//   g_dvd (lbl_802DA1CC, .bss, cDvd instance - only 4 bytes confirmed so
//     far, real size unknown), g_idSystem (lbl_802E020C, .bss, IDSystem,
//     0x50 bytes), g_messageControl (lbl_80271F34, .bss, MessageControl,
//     0x1E bytes) - all called as member functions on global instances,
//     consistent with the project's existing MessageControl::init /
//     IDSystem::gameInit callers elsewhere.
//
// SystemWork::field0 == 1 also gates a debug-only block: InitFile(), then
// try file_path("d:\bio4") and fall back to file_path("c:/") if that
// fails - the SN Systems dev-drive redirect paths seen in the debug build
// ("HDread: d:\bio4/room/sysdata.dat not found!!!" in a real boot log is
// this exact fallback failing under Dolphin, since there's no dev PC HDD).

struct SystemWork {
    s32 field0;      // +0x0 - 1 when running under a devkit console type
    u8 unk_04[0x54 - 0x4];
    u32 field_0x54;  // +0x54 - bit 0x40000 set/cleared by systemVISetBlack;
                      //   bit 0x20000000 tested (not this bit) by
                      //   postVSyncCallback before calling haltExecCheck();
                      //   bit 0x1000 tested by SetScissorState (not matched)
};

// Real size unconfirmed (matches project convention: only the touched
// portion is claimed). All three called only as `obj->Method()` here.
// Declarations only, deliberately - these are the CALLEES from
// systemStartInit, not themselves matched yet. IMPORTANT: do not give them
// bodies here even for the native build - with -inline auto, mwcc inlines
// trivial empty bodies and *removes the `bl` instructions from
// systemStartInit's own compiled output*, silently wrecking its verified
// match (confirmed: dropped systemStartInit from 80.7% to 62.8% when this
// was tried). Native-only definitions live in native/boot/sdk_stubs.cpp
// under RE4_NATIVE_BUILD, kept fully separate from anything mwcc compiles.
class cDvd {
public:
    void Init();
    void SizeTableRead();
};

class IDSystem {
public:
    void gameInit(s32 unk);
};

class MessageControl {
public:
    void init();
};

#ifdef RE4_NATIVE_BUILD
void cDvd::Init() {}
void cDvd::SizeTableRead() {}
void IDSystem::gameInit(s32) {}
void MessageControl::init() {}
#endif

extern "C" {
void memclr_asm(void* dst, u32 size);
void OSInit();
void setLanguage();
void RomFontSetting();
u32 OSGetConsoleType();
void InitFile();
s32 file_path(const char* path);
void ExceptionInit();
void OSInitAlarm();
void VIInit();
void VISetBlack(s32 black);
u32 OSDisableInterrupts();
void OSRestoreInterrupts(u32 enabled);
void __LCEnable();
void VIWaitForRetrace();
typedef void (*VIRetraceCallback)();
void VISetPostRetraceCallback(VIRetraceCallback cb);
s32 GetSystemVcnt();
void iTaskSuspend();
void haltExecCheck();
void CardInit();
void SystemMemInit();
void CardDbgCacheSet();
void TaskSchedulerInit();
void systemScreenInit();
void Render_init();
void LightSetInit();
void InitOt();
void SndInit();
void SofdecInit();
void IdTexGameInit();
void EprintfInit();
void LogInit();
void init_dbmodule();
}

static SystemWork* g_systemWork;
static cDvd g_dvd;
static IDSystem g_idSystem;
static MessageControl g_messageControl;
static u16 g_rndSeed;      // lbl_80315B44
static s32 g_vsyncCount;   // lbl_80314BD0

// --- Matching status: MATCHED (byte-identical, verified via objdiff-cli,
// 100.0%) ---
// memclr_asm, 80158E58-80158EAC, 0x58 bytes. Hand-written PPC assembly in
// the original (loop-unrolled-by-4 word clear with a byte-at-a-time tail) -
// not something a C++ compiler would ever emit from a portable memset-style
// loop, so reproduced as literal inline asm (CodeWarrior "asm function")
// rather than "matched" from C++. First attempt got 81.8% - CodeWarrior's
// asm-function feature auto-generates a stack frame + parameter spill
// (stwu/stw r3/stw r4 ... addi r1) around the body by default, which the
// real leaf routine doesn't have. Fix: the `nofralloc` directive as the
// first line of the asm body suppresses that - jumped straight to 100%.
asm void memclr_asm(void*, u32)
{
    nofralloc
    li r11, 0
    cmplwi r4, 0xf
    ble zeroSmall
    andi. r0, r3, 0x3
    bne zeroSmall
    subi r3, r3, 4
    srwi r12, r4, 4
    andi. r4, r4, 0xf
    mtctr r12
wordLoop:
    stwu r11, 4(r3)
    stwu r11, 4(r3)
    stwu r11, 4(r3)
    stwu r11, 4(r3)
    bdnz wordLoop
    addi r3, r3, 4
zeroSmall:
    cmpwi r4, 0
    beqlr
    subi r3, r3, 1
    mtctr r4
byteLoop:
    stbu r11, 1(r3)
    bdnz byteLoop
    blr
}

// --- Matching status: MATCHED (byte-identical modulo global naming,
// verified via objdiff-cli, 97.5% raw / 100% real - the only reported diff
// is "lbl_80315B44" vs "g_rndSeed", the same symbol, different name;
// resolves identically once linked) ---
// RndInit, 8017FE70-8017FE74, 8 bytes / 2 instructions.
void RndInit(s32 seed)
{
    g_rndSeed = (u16)seed;
}

// --- Matching status: equivalent (71.4% via objdiff-cli) ---
// LCEnable, 80073EB8-80073EEC, 0x38 bytes. Standard Nintendo SDK
// disable-interrupts/do-the-thing/restore idiom. Logic/call sequence is
// exactly right; only difference is prologue/epilogue instruction order -
// target does mflr+save-LR *before* growing the stack (stwu), ours grows
// the stack first then saves LR (the far more common order) - same
// register set, same net effect, just reordered. Compiler-internal
// scheduling choice, same class as CameraControl::Disable's register-
// numbering-only difference. Not chased further.
void LCEnable()
{
    u32 enabled = OSDisableInterrupts();
    __LCEnable();
    OSRestoreInterrupts(enabled);
}

// --- Matching status: equivalent (83.9% via objdiff-cli) ---
// systemVISetBlack, 8015618C-801561DC, 0x54 bytes. Logic exactly right
// (VISetBlack(1)/set bit 0x40000 vs VISetBlack(0)/clear bit 0x40000 in
// g_systemWork->field_0x54). Target shares one `stw` at the branch merge
// point since both arms only differ in the computed value; our compile
// keeps a separate store in each arm despite restructuring the source to
// compute a shared local first - didn't change the codegen, so this is
// scheduler-internal, not source-controllable from here. Not chased
// further right now.
void systemVISetBlack(s32 black)
{
    u32 flags;
    if (black == 1) {
        VISetBlack(1);
        flags = g_systemWork->field_0x54 | 0x40000;
    } else {
        VISetBlack(0);
        flags = g_systemWork->field_0x54 & ~0x40000;
    }
    g_systemWork->field_0x54 = flags;
}

// --- Matching status: equivalent (94.3% via objdiff-cli) ---
// postVSyncCallback, 80153340-8015338C, 0x4C bytes. Installed via
// VISetPostRetraceCallback in systemStartInit - runs once per VBlank.
// First attempt (94.05%) had the vsync-count comparison INVERTED
// (`g_vsyncCount < GetSystemVcnt()`) - objdiff's
// DIFF_OP_MISMATCH on the branch instruction (target `blt`, ours `bge`,
// same operands) caught a genuine logic bug, not just a cosmetic
// difference; fixed to `>=`, matching the target exactly. Remaining gaps
// are the established categories: prologue/epilogue size (same class as
// LCEnable above) and `andis.`-vs-`rlwinm.` single-bit-test instruction
// selection for the `field_0x54 & 0x20000000` check (same class as
// systemStartInit's `OSGetConsoleType() & 0xf0000000` finding).
void postVSyncCallback()
{
    g_vsyncCount++;
    if (g_vsyncCount >= GetSystemVcnt()) {
        iTaskSuspend();
    }
    if (!(g_systemWork->field_0x54 & 0x20000000)) {
        haltExecCheck();
    }
}

void systemStartInit()
{
    memclr_asm(g_systemWork, 0x8680);
    OSInit();
    setLanguage();
    RomFontSetting();
    if (OSGetConsoleType() & 0xf0000000) {
        g_systemWork->field0 = 1;
    }
    if (g_systemWork->field0 == 1) {
        InitFile();
        if (!file_path("d:\\bio4")) {
            file_path("c:/");
        }
    }
    ExceptionInit();
    OSInitAlarm();
    VIInit();
    systemVISetBlack(1);
    LCEnable();
    VIWaitForRetrace();
    VISetPostRetraceCallback(postVSyncCallback);
    cDvd* dvd = &g_dvd;
    dvd->Init();
    CardInit();

    asm {
        li r3, 0x0004
        oris r3, r3, 0x0004
        mtspr 914, r3  // GQR2
        li r3, 0x0005
        oris r3, r3, 0x0005
        mtspr 915, r3  // GQR3
        li r3, 0x0006
        oris r3, r3, 0x0006
        mtspr 916, r3  // GQR4
        li r3, 0x0007
        oris r3, r3, 0x0007
        mtspr 917, r3  // GQR5
    }

    SystemMemInit();
    if (g_systemWork->field0 == 1) {
        CardDbgCacheSet();
    }
    TaskSchedulerInit();
    systemScreenInit();
    Render_init();
    LightSetInit();
    RndInit(0xd37);
    InitOt();
    SndInit();
    SofdecInit();
    g_idSystem.gameInit(0x80);
    IdTexGameInit();
    EprintfInit();
    LogInit();
    init_dbmodule();
    dvd->SizeTableRead();
    g_messageControl.init();
}
