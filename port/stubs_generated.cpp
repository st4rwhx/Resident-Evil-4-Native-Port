// -*- coding: utf-8 -*-
// GENERE par port/gen_stubs.py - ne pas editer a la main.
//
// Une definition par fonction du SDK Nintendo que le jeu appelle reellement. Chacune
// journalise son premier appel et rend une valeur neutre. Cela ne fait rien tourner,
// mais cela **lie** : a partir de la, on remplace les fonctions une par une en voyant
// a chaque execution ce que le jeu reclame ensuite, au lieu de tout implementer a
// l aveugle puis d esperer.
//
// Les prototypes viennent de include/dolphin/, donc ils sont exacts par construction.
#include "port_log.h"
#include <dolphin.h>
// dolphin.h n inclut ni AX ni AXFX ni AXART.
#include <dolphin/ax.h>
#include <dolphin/axfx.h>
#include <dolphin/axart.h>
#include <dolphin/demo.h>

__attribute__((weak)) void AXARTInit() { PORT_TODO("AXARTInit"); }   // include\dolphin\axart.h
__attribute__((weak)) void AXARTQuit() { PORT_TODO("AXARTQuit"); }   // include\dolphin\axart.h
__attribute__((weak)) void AXARTServiceSounds() { PORT_TODO("AXARTServiceSounds"); }   // include\dolphin\axart.h
__attribute__((weak)) AXVPB* AXAcquireVoice(u32 priority, void (*callback)(void *), u32 userContext) { PORT_TODO("AXAcquireVoice"); return nullptr; }   // include\dolphin\ax.h
__attribute__((weak)) int AXFXChorusInit(AXFX_CHORUS* c) { PORT_TODO("AXFXChorusInit"); return 0; }   // include\dolphin\axfx.h
__attribute__((weak)) int AXFXChorusSettings(AXFX_CHORUS* c) { PORT_TODO("AXFXChorusSettings"); return 0; }   // include\dolphin\axfx.h
__attribute__((weak)) int AXFXChorusShutdown(AXFX_CHORUS* c) { PORT_TODO("AXFXChorusShutdown"); return 0; }   // include\dolphin\axfx.h
__attribute__((weak)) int AXFXDelayInit(AXFX_DELAY* delay) { PORT_TODO("AXFXDelayInit"); return 0; }   // include\dolphin\axfx.h
__attribute__((weak)) int AXFXDelaySettings(AXFX_DELAY* delay) { PORT_TODO("AXFXDelaySettings"); return 0; }   // include\dolphin\axfx.h
__attribute__((weak)) int AXFXDelayShutdown(AXFX_DELAY* delay) { PORT_TODO("AXFXDelayShutdown"); return 0; }   // include\dolphin\axfx.h
__attribute__((weak)) int AXFXReverbHiInit(AXFX_REVERBHI* rev) { PORT_TODO("AXFXReverbHiInit"); return 0; }   // include\dolphin\axfx.h
__attribute__((weak)) int AXFXReverbHiInitDpl2(AXFX_REVERBHI_DPL2* reverb) { PORT_TODO("AXFXReverbHiInitDpl2"); return 0; }   // include\dolphin\axfx.h
__attribute__((weak)) int AXFXReverbHiSettings(AXFX_REVERBHI* rev) { PORT_TODO("AXFXReverbHiSettings"); return 0; }   // include\dolphin\axfx.h
__attribute__((weak)) int AXFXReverbHiSettingsDpl2(AXFX_REVERBHI_DPL2* rev) { PORT_TODO("AXFXReverbHiSettingsDpl2"); return 0; }   // include\dolphin\axfx.h
__attribute__((weak)) int AXFXReverbHiShutdown(AXFX_REVERBHI* rev) { PORT_TODO("AXFXReverbHiShutdown"); return 0; }   // include\dolphin\axfx.h
__attribute__((weak)) int AXFXReverbHiShutdownDpl2(AXFX_REVERBHI_DPL2* reverb) { PORT_TODO("AXFXReverbHiShutdownDpl2"); return 0; }   // include\dolphin\axfx.h
__attribute__((weak)) int AXFXReverbStdInit(AXFX_REVERBSTD* rev) { PORT_TODO("AXFXReverbStdInit"); return 0; }   // include\dolphin\axfx.h
__attribute__((weak)) int AXFXReverbStdSettings(AXFX_REVERBSTD* rev) { PORT_TODO("AXFXReverbStdSettings"); return 0; }   // include\dolphin\axfx.h
__attribute__((weak)) int AXFXReverbStdShutdown(AXFX_REVERBSTD* rev) { PORT_TODO("AXFXReverbStdShutdown"); return 0; }   // include\dolphin\axfx.h
__attribute__((weak)) void AXFreeVoice(AXVPB* p) { PORT_TODO("AXFreeVoice"); }   // include\dolphin\ax.h
__attribute__((weak)) u32 AXGetDspCycles() { PORT_TODO("AXGetDspCycles"); return 0; }   // include\dolphin\ax.h
__attribute__((weak)) u32 AXGetMaxDspCycles() { PORT_TODO("AXGetMaxDspCycles"); return 0; }   // include\dolphin\ax.h
__attribute__((weak)) void AXInitEx(u32 outputBufferMode) { PORT_TODO("AXInitEx"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXQuit() { PORT_TODO("AXQuit"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXRegisterAuxACallback(void (*callback)(void*, void*), void* context) { PORT_TODO("AXRegisterAuxACallback"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXRegisterAuxBCallback(void (*callback)(void*, void*), void* context) { PORT_TODO("AXRegisterAuxBCallback"); }   // include\dolphin\ax.h
__attribute__((weak)) AXCallback AXRegisterCallback(AXCallback callback) { PORT_TODO("AXRegisterCallback"); return {}; }   // include\dolphin\ax.h
__attribute__((weak)) void AXSetMode(u32 mode) { PORT_TODO("AXSetMode"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXSetVoiceAddr(AXVPB* p, AXPBADDR* addr) { PORT_TODO("AXSetVoiceAddr"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXSetVoiceAdpcm(AXVPB* p, AXPBADPCM* adpcm) { PORT_TODO("AXSetVoiceAdpcm"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXSetVoiceAdpcmLoop(AXVPB* p, AXPBADPCMLOOP* adpcmloop) { PORT_TODO("AXSetVoiceAdpcmLoop"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXSetVoiceEndAddr(AXVPB* p, u32 addr) { PORT_TODO("AXSetVoiceEndAddr"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXSetVoiceLoop(AXVPB* p, u16 loop) { PORT_TODO("AXSetVoiceLoop"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXSetVoiceLoopAddr(AXVPB* p, u32 addr) { PORT_TODO("AXSetVoiceLoopAddr"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXSetVoiceLpf(AXVPB* p, AXPBLPF* lpf) { PORT_TODO("AXSetVoiceLpf"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXSetVoiceLpfCoefs(AXVPB* p, u16 a0, u16 b0) { PORT_TODO("AXSetVoiceLpfCoefs"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXSetVoicePriority(AXVPB* p, u32 priority) { PORT_TODO("AXSetVoicePriority"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXSetVoiceSrc(AXVPB* p, AXPBSRC* src_) { PORT_TODO("AXSetVoiceSrc"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXSetVoiceSrcRatio(AXVPB* p, f32 ratio) { PORT_TODO("AXSetVoiceSrcRatio"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXSetVoiceSrcType(AXVPB* p, u32 type) { PORT_TODO("AXSetVoiceSrcType"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXSetVoiceState(AXVPB* p, u16 state) { PORT_TODO("AXSetVoiceState"); }   // include\dolphin\ax.h
__attribute__((weak)) void AXSetVoiceType(AXVPB* p, u16 type) { PORT_TODO("AXSetVoiceType"); }   // include\dolphin\ax.h
__attribute__((weak)) int DVDCancelAsync(DVDCommandBlock* block, DVDCBCallback callback) { PORT_TODO("DVDCancelAsync"); return 0; }   // include\dolphin\dvd.h
__attribute__((weak)) int DVDChangeDiskAsync(DVDCommandBlock* block, DVDDiskID* id, DVDCBCallback callback) { PORT_TODO("DVDChangeDiskAsync"); return 0; }   // include\dolphin\dvd.h
__attribute__((weak)) int DVDCloseDir(DVDDir* dir) { PORT_TODO("DVDCloseDir"); return 0; }   // include\dolphin\dvd.h
__attribute__((weak)) int DVDCompareDiskID(const DVDDiskID* id1, const DVDDiskID* id2) { PORT_TODO("DVDCompareDiskID"); return 0; }   // include\dolphin\dvd.h
__attribute__((weak)) DVDDiskID* DVDGenerateDiskID(DVDDiskID* id, const char* game, const char* company, u8 diskNum, u8 version) { PORT_TODO("DVDGenerateDiskID"); return nullptr; }   // include\dolphin\dvd.h
__attribute__((weak)) s32 DVDGetCommandBlockStatus(const DVDCommandBlock* block) { PORT_TODO("DVDGetCommandBlockStatus"); return 0; }   // include\dolphin\dvd.h
__attribute__((weak)) int DVDOpenDir(const char* dirName, DVDDir* dir) { PORT_TODO("DVDOpenDir"); return 0; }   // include\dolphin\dvd.h
__attribute__((weak)) int DVDReadDir(DVDDir* dir, DVDDirEntry* dirent) { PORT_TODO("DVDReadDir"); return 0; }   // include\dolphin\dvd.h
__attribute__((weak)) void GXBeginDisplayList(void* list, u32 size) { PORT_TODO("GXBeginDisplayList"); }   // include\dolphin\gx\GXDispList.h
__attribute__((weak)) void GXClearBoundingBox() { PORT_TODO("GXClearBoundingBox"); }   // include\dolphin\gx\GXFrameBuffer.h
__attribute__((weak)) void GXDrawTorus(f32 rc, u8 numc, u8 numt) { PORT_TODO("GXDrawTorus"); }   // include\dolphin\gx\GXDraw.h
__attribute__((weak)) void GXEnableTexOffsets(GXTexCoordID coord, u8 line_enable, u8 point_enable) { PORT_TODO("GXEnableTexOffsets"); }   // include\dolphin\gx\GXGeometry.h
__attribute__((weak)) u32 GXEndDisplayList() { PORT_TODO("GXEndDisplayList"); return 0; }   // include\dolphin\gx\GXDispList.h
__attribute__((weak)) void GXGetProjectionv(f32* ptr) { PORT_TODO("GXGetProjectionv"); }   // include\dolphin\gx\GXGet.h
__attribute__((weak)) void* GXGetTexObjData(const GXTexObj* to) { PORT_TODO("GXGetTexObjData"); return nullptr; }   // include\dolphin\gx\GXGet.h
__attribute__((weak)) void GXGetViewportv(f32* vp) { PORT_TODO("GXGetViewportv"); }   // include\dolphin\gx\GXGet.h
__attribute__((weak)) void GXInitLightAttnK(GXLightObj* lt_obj, f32 k0, f32 k1, f32 k2) { PORT_TODO("GXInitLightAttnK"); }   // include\dolphin\gx\GXLighting.h
__attribute__((weak)) void GXLoadNrmMtxImm(const f32 mtx[3][4], u32 id) { PORT_TODO("GXLoadNrmMtxImm"); }   // include\dolphin\gx\GXTransform.h
__attribute__((weak)) void GXPeekZ(u16 x, u16 y, u32* z) { PORT_TODO("GXPeekZ"); }   // include\dolphin\gx\GXCpu2Efb.h
__attribute__((weak)) void GXProject(f32 x, f32 y, f32 z, const f32 mtx[3][4], const f32* pm, const f32* vp, f32* sx, f32* sy, f32* sz) { PORT_TODO("GXProject"); }   // include\dolphin\gx\GXTransform.h
__attribute__((weak)) void GXSetCopyFilter(GXBool aa, const u8 sample_pattern[12][2], GXBool vf, const u8 vfilter[7]) { PORT_TODO("GXSetCopyFilter"); }   // include\dolphin\gx\GXFrameBuffer.h
__attribute__((weak)) OSThread* GXSetCurrentGXThread() { PORT_TODO("GXSetCurrentGXThread"); return nullptr; }   // include\dolphin\gx\GXFifo.h
__attribute__((weak)) void GXSetCurrentMtx(u32 id) { PORT_TODO("GXSetCurrentMtx"); }   // include\dolphin\gx\GXTransform.h
__attribute__((weak)) void GXSetDispCopyDst(u16 wd, u16 ht) { PORT_TODO("GXSetDispCopyDst"); }   // include\dolphin\gx\GXFrameBuffer.h
__attribute__((weak)) void GXSetDispCopyGamma(GXGamma gamma) { PORT_TODO("GXSetDispCopyGamma"); }   // include\dolphin\gx\GXFrameBuffer.h
__attribute__((weak)) void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht) { PORT_TODO("GXSetDispCopySrc"); }   // include\dolphin\gx\GXFrameBuffer.h
__attribute__((weak)) u32 GXSetDispCopyYScale(f32 vscale) { PORT_TODO("GXSetDispCopyYScale"); return 0; }   // include\dolphin\gx\GXFrameBuffer.h
__attribute__((weak)) void GXSetDither(GXBool dither) { PORT_TODO("GXSetDither"); }   // include\dolphin\gx\GXPixel.h
__attribute__((weak)) void GXSetDstAlpha(GXBool enable, u8 alpha) { PORT_TODO("GXSetDstAlpha"); }   // include\dolphin\gx\GXPixel.h
__attribute__((weak)) void GXSetFog(GXFogType type, f32 startz, f32 endz, f32 nearz, f32 farz, GXColor color) { PORT_TODO("GXSetFog"); }   // include\dolphin\gx\GXPixel.h
__attribute__((weak)) void GXSetIndTexCoordScale(GXIndTexStageID ind_state, GXIndTexScale scale_s, GXIndTexScale scale_t) { PORT_TODO("GXSetIndTexCoordScale"); }   // include\dolphin\gx\GXBump.h
__attribute__((weak)) void GXSetIndTexMtx(GXIndTexMtxID mtx_id, const f32 offset[2][3], s8 scale_exp) { PORT_TODO("GXSetIndTexMtx"); }   // include\dolphin\gx\GXBump.h
__attribute__((weak)) void GXSetIndTexOrder(GXIndTexStageID ind_stage, GXTexCoordID tex_coord, GXTexMapID tex_map) { PORT_TODO("GXSetIndTexOrder"); }   // include\dolphin\gx\GXBump.h
__attribute__((weak)) void GXSetLineWidth(u8 width, GXTexOffset texOffsets) { PORT_TODO("GXSetLineWidth"); }   // include\dolphin\gx\GXGeometry.h
__attribute__((weak)) void GXSetPixelFmt(GXPixelFmt pix_fmt, GXZFmt16 z_fmt) { PORT_TODO("GXSetPixelFmt"); }   // include\dolphin\gx\GXPixel.h
__attribute__((weak)) void GXSetTevIndBumpXYZ(GXTevStageID tev_stage, GXIndTexStageID ind_stage, GXIndTexMtxID matrix_sel) { PORT_TODO("GXSetTevIndBumpXYZ"); }   // include\dolphin\gx\GXBump.h
__attribute__((weak)) void GXSetTevIndWarp(GXTevStageID tev_stage, GXIndTexStageID ind_stage, u8 signed_offset, u8 replace_mode, GXIndTexMtxID matrix_sel) { PORT_TODO("GXSetTevIndWarp"); }   // include\dolphin\gx\GXBump.h
__attribute__((weak)) void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz, u32 field) { PORT_TODO("GXSetViewportJitter"); }   // include\dolphin\gx\GXTransform.h
__attribute__((weak)) void GXSetZCompLoc(GXBool before_tex) { PORT_TODO("GXSetZCompLoc"); }   // include\dolphin\gx\GXPixel.h
__attribute__((weak)) OSTime OSCalendarTimeToTicks(OSCalendarTime* td) { PORT_TODO("OSCalendarTimeToTicks"); return {}; }   // include\dolphin\os.h
__attribute__((weak)) s32 OSEnableScheduler() { PORT_TODO("OSEnableScheduler"); return 0; }   // include\dolphin\os\OSThread.h
__attribute__((weak)) u16 OSGetFontEncode() { PORT_TODO("OSGetFontEncode"); return 0; }   // include\dolphin\os\OSFont.h
__attribute__((weak)) char* OSGetFontTexture(const char* string, void** image, s32* x, s32* y, s32* width) { PORT_TODO("OSGetFontTexture"); return nullptr; }   // include\dolphin\os\OSFont.h
__attribute__((weak)) u32 OSGetProgressiveMode() { PORT_TODO("OSGetProgressiveMode"); return 0; }   // include\dolphin\os\OSRtc.h
__attribute__((weak)) u32 OSGetSoundMode() { PORT_TODO("OSGetSoundMode"); return 0; }   // include\dolphin\os.h
__attribute__((weak)) void OSInitAlarm() { PORT_TODO("OSInitAlarm"); }   // include\dolphin\os\OSAlarm.h
__attribute__((weak)) BOOL OSInitFont(OSFontHeader* fontData) { PORT_TODO("OSInitFont"); return 0; }   // include\dolphin\os\OSFont.h
__attribute__((weak)) void OSInitStopwatch(OSStopwatch* sw, char* name) { PORT_TODO("OSInitStopwatch"); }   // include\dolphin\os.h
__attribute__((weak)) void* OSPhysicalToCached(u32 paddr) { PORT_TODO("OSPhysicalToCached"); return nullptr; }   // include\dolphin\os.h
__attribute__((weak)) void OSResetStopwatch(OSStopwatch* sw) { PORT_TODO("OSResetStopwatch"); }   // include\dolphin\os.h
__attribute__((weak)) void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu) { PORT_TODO("OSResetSystem"); }   // include\dolphin\os\OSReset.h
__attribute__((weak)) OSErrorHandler OSSetErrorHandler(OSError error, OSErrorHandler handler) { PORT_TODO("OSSetErrorHandler"); return {}; }   // include\dolphin\os\OSError.h
__attribute__((weak)) void OSSetProgressiveMode(u32 on) { PORT_TODO("OSSetProgressiveMode"); }   // include\dolphin\os\OSRtc.h
__attribute__((weak)) void OSSetSaveRegion(void* start, void* end) { PORT_TODO("OSSetSaveRegion"); }   // include\dolphin\os\OSReboot.h
__attribute__((weak)) void OSSetSoundMode(u32 mode) { PORT_TODO("OSSetSoundMode"); }   // include\dolphin\os.h
__attribute__((weak)) void OSStartStopwatch(OSStopwatch* sw) { PORT_TODO("OSStartStopwatch"); }   // include\dolphin\os.h
__attribute__((weak)) void OSStopStopwatch(OSStopwatch* sw) { PORT_TODO("OSStopStopwatch"); }   // include\dolphin\os.h
__attribute__((weak)) void OSTicksToCalendarTime(OSTime ticks, OSCalendarTime* td) { PORT_TODO("OSTicksToCalendarTime"); }   // include\dolphin\os.h
