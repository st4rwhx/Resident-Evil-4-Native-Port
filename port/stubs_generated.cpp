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

u32 ARAlloc(u32 length) { PORT_TODO("ARAlloc"); return 0; }   // include\dolphin\ar.h
u32 ARGetBaseAddress() { PORT_TODO("ARGetBaseAddress"); return 0; }   // include\dolphin\ar.h
u32 ARInit(u32* stack_index_addr, u32 num_entries) { PORT_TODO("ARInit"); return 0; }   // include\dolphin\ar.h
void ARQFlushQueue() { PORT_TODO("ARQFlushQueue"); }   // include\dolphin\ar.h
void ARQInit() { PORT_TODO("ARQInit"); }   // include\dolphin\ar.h
void ARQPostRequest(ARQRequest* request, u32 owner, u32 type, u32 priority, u32 source, u32 dest, u32 length, ARQCallback callback) { PORT_TODO("ARQPostRequest"); }   // include\dolphin\ar.h
void AXARTInit() { PORT_TODO("AXARTInit"); }   // include\dolphin\axart.h
void AXARTQuit() { PORT_TODO("AXARTQuit"); }   // include\dolphin\axart.h
void AXARTServiceSounds() { PORT_TODO("AXARTServiceSounds"); }   // include\dolphin\axart.h
AXVPB* AXAcquireVoice(u32 priority, void (*callback)(void *), u32 userContext) { PORT_TODO("AXAcquireVoice"); return nullptr; }   // include\dolphin\ax.h
int AXFXChorusInit(AXFX_CHORUS* c) { PORT_TODO("AXFXChorusInit"); return 0; }   // include\dolphin\axfx.h
int AXFXChorusSettings(AXFX_CHORUS* c) { PORT_TODO("AXFXChorusSettings"); return 0; }   // include\dolphin\axfx.h
int AXFXChorusShutdown(AXFX_CHORUS* c) { PORT_TODO("AXFXChorusShutdown"); return 0; }   // include\dolphin\axfx.h
int AXFXDelayInit(AXFX_DELAY* delay) { PORT_TODO("AXFXDelayInit"); return 0; }   // include\dolphin\axfx.h
int AXFXDelaySettings(AXFX_DELAY* delay) { PORT_TODO("AXFXDelaySettings"); return 0; }   // include\dolphin\axfx.h
int AXFXDelayShutdown(AXFX_DELAY* delay) { PORT_TODO("AXFXDelayShutdown"); return 0; }   // include\dolphin\axfx.h
int AXFXReverbHiInit(AXFX_REVERBHI* rev) { PORT_TODO("AXFXReverbHiInit"); return 0; }   // include\dolphin\axfx.h
int AXFXReverbHiInitDpl2(AXFX_REVERBHI_DPL2* reverb) { PORT_TODO("AXFXReverbHiInitDpl2"); return 0; }   // include\dolphin\axfx.h
int AXFXReverbHiSettings(AXFX_REVERBHI* rev) { PORT_TODO("AXFXReverbHiSettings"); return 0; }   // include\dolphin\axfx.h
int AXFXReverbHiSettingsDpl2(AXFX_REVERBHI_DPL2* rev) { PORT_TODO("AXFXReverbHiSettingsDpl2"); return 0; }   // include\dolphin\axfx.h
int AXFXReverbHiShutdown(AXFX_REVERBHI* rev) { PORT_TODO("AXFXReverbHiShutdown"); return 0; }   // include\dolphin\axfx.h
int AXFXReverbHiShutdownDpl2(AXFX_REVERBHI_DPL2* reverb) { PORT_TODO("AXFXReverbHiShutdownDpl2"); return 0; }   // include\dolphin\axfx.h
int AXFXReverbStdInit(AXFX_REVERBSTD* rev) { PORT_TODO("AXFXReverbStdInit"); return 0; }   // include\dolphin\axfx.h
int AXFXReverbStdSettings(AXFX_REVERBSTD* rev) { PORT_TODO("AXFXReverbStdSettings"); return 0; }   // include\dolphin\axfx.h
int AXFXReverbStdShutdown(AXFX_REVERBSTD* rev) { PORT_TODO("AXFXReverbStdShutdown"); return 0; }   // include\dolphin\axfx.h
void AXFreeVoice(AXVPB* p) { PORT_TODO("AXFreeVoice"); }   // include\dolphin\ax.h
u32 AXGetDspCycles() { PORT_TODO("AXGetDspCycles"); return 0; }   // include\dolphin\ax.h
u32 AXGetMaxDspCycles() { PORT_TODO("AXGetMaxDspCycles"); return 0; }   // include\dolphin\ax.h
void AXInitEx(u32 outputBufferMode) { PORT_TODO("AXInitEx"); }   // include\dolphin\ax.h
void AXQuit() { PORT_TODO("AXQuit"); }   // include\dolphin\ax.h
void AXRegisterAuxACallback(void (*callback)(void*, void*), void* context) { PORT_TODO("AXRegisterAuxACallback"); }   // include\dolphin\ax.h
void AXRegisterAuxBCallback(void (*callback)(void*, void*), void* context) { PORT_TODO("AXRegisterAuxBCallback"); }   // include\dolphin\ax.h
AXCallback AXRegisterCallback(AXCallback callback) { PORT_TODO("AXRegisterCallback"); return {}; }   // include\dolphin\ax.h
void AXSetMode(u32 mode) { PORT_TODO("AXSetMode"); }   // include\dolphin\ax.h
void AXSetVoiceAddr(AXVPB* p, AXPBADDR* addr) { PORT_TODO("AXSetVoiceAddr"); }   // include\dolphin\ax.h
void AXSetVoiceAdpcm(AXVPB* p, AXPBADPCM* adpcm) { PORT_TODO("AXSetVoiceAdpcm"); }   // include\dolphin\ax.h
void AXSetVoiceAdpcmLoop(AXVPB* p, AXPBADPCMLOOP* adpcmloop) { PORT_TODO("AXSetVoiceAdpcmLoop"); }   // include\dolphin\ax.h
void AXSetVoiceEndAddr(AXVPB* p, u32 addr) { PORT_TODO("AXSetVoiceEndAddr"); }   // include\dolphin\ax.h
void AXSetVoiceLoop(AXVPB* p, u16 loop) { PORT_TODO("AXSetVoiceLoop"); }   // include\dolphin\ax.h
void AXSetVoiceLoopAddr(AXVPB* p, u32 addr) { PORT_TODO("AXSetVoiceLoopAddr"); }   // include\dolphin\ax.h
void AXSetVoiceLpf(AXVPB* p, AXPBLPF* lpf) { PORT_TODO("AXSetVoiceLpf"); }   // include\dolphin\ax.h
void AXSetVoiceLpfCoefs(AXVPB* p, u16 a0, u16 b0) { PORT_TODO("AXSetVoiceLpfCoefs"); }   // include\dolphin\ax.h
void AXSetVoicePriority(AXVPB* p, u32 priority) { PORT_TODO("AXSetVoicePriority"); }   // include\dolphin\ax.h
void AXSetVoiceSrc(AXVPB* p, AXPBSRC* src_) { PORT_TODO("AXSetVoiceSrc"); }   // include\dolphin\ax.h
void AXSetVoiceSrcRatio(AXVPB* p, f32 ratio) { PORT_TODO("AXSetVoiceSrcRatio"); }   // include\dolphin\ax.h
void AXSetVoiceSrcType(AXVPB* p, u32 type) { PORT_TODO("AXSetVoiceSrcType"); }   // include\dolphin\ax.h
void AXSetVoiceState(AXVPB* p, u16 state) { PORT_TODO("AXSetVoiceState"); }   // include\dolphin\ax.h
void AXSetVoiceType(AXVPB* p, u16 type) { PORT_TODO("AXSetVoiceType"); }   // include\dolphin\ax.h
s32 CARDCheckAsync(s32 chan, CARDCallback callback) { PORT_TODO("CARDCheckAsync"); return 0; }   // include\dolphin\card.h
s32 CARDClose(CARDFileInfo* fileInfo) { PORT_TODO("CARDClose"); return 0; }   // include\dolphin\card.h
s32 CARDCreateAsync(s32 chan, const char* fileName, u32 size, CARDFileInfo* fileInfo, CARDCallback callback) { PORT_TODO("CARDCreateAsync"); return 0; }   // include\dolphin\card.h
s32 CARDDeleteAsync(s32 chan, const char* fileName, CARDCallback callback) { PORT_TODO("CARDDeleteAsync"); return 0; }   // include\dolphin\card.h
s32 CARDFreeBlocks(s32 chan, s32* byteNotUsed, s32* filesNotUsed) { PORT_TODO("CARDFreeBlocks"); return 0; }   // include\dolphin\card.h
s32 CARDGetResultCode(s32 chan) { PORT_TODO("CARDGetResultCode"); return 0; }   // include\dolphin\card.h
s32 CARDGetSerialNo(s32 chan, u64* serialNo) { PORT_TODO("CARDGetSerialNo"); return 0; }   // include\dolphin\card.h
s32 CARDGetStatus(s32 chan, s32 fileNo, CARDStat* stat) { PORT_TODO("CARDGetStatus"); return 0; }   // include\dolphin\card.h
void CARDInit() { PORT_TODO("CARDInit"); }   // include\dolphin\card.h
s32 CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback, CARDCallback attachCallback) { PORT_TODO("CARDMountAsync"); return 0; }   // include\dolphin\card.h
s32 CARDOpen(s32 chan, const char* fileName, CARDFileInfo* fileInfo) { PORT_TODO("CARDOpen"); return 0; }   // include\dolphin\card.h
s32 CARDProbeEx(s32 chan, s32* memSize, s32* sectorSize) { PORT_TODO("CARDProbeEx"); return 0; }   // include\dolphin\card.h
s32 CARDReadAsync(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset, CARDCallback callback) { PORT_TODO("CARDReadAsync"); return 0; }   // include\dolphin\card.h
s32 CARDSetStatusAsync(s32 chan, s32 fileNo, CARDStat* stat, CARDCallback callback) { PORT_TODO("CARDSetStatusAsync"); return 0; }   // include\dolphin\card.h
s32 CARDUnmount(s32 chan) { PORT_TODO("CARDUnmount"); return 0; }   // include\dolphin\card.h
s32 CARDWriteAsync(CARDFileInfo*  fileInfo, void* buf, s32 length, s32 offset, CARDCallback callback) { PORT_TODO("CARDWriteAsync"); return 0; }   // include\dolphin\card.h
int DVDCancelAsync(DVDCommandBlock* block, DVDCBCallback callback) { PORT_TODO("DVDCancelAsync"); return 0; }   // include\dolphin\dvd.h
int DVDChangeDiskAsync(DVDCommandBlock* block, DVDDiskID* id, DVDCBCallback callback) { PORT_TODO("DVDChangeDiskAsync"); return 0; }   // include\dolphin\dvd.h
int DVDCloseDir(DVDDir* dir) { PORT_TODO("DVDCloseDir"); return 0; }   // include\dolphin\dvd.h
int DVDCompareDiskID(const DVDDiskID* id1, const DVDDiskID* id2) { PORT_TODO("DVDCompareDiskID"); return 0; }   // include\dolphin\dvd.h
DVDDiskID* DVDGenerateDiskID(DVDDiskID* id, const char* game, const char* company, u8 diskNum, u8 version) { PORT_TODO("DVDGenerateDiskID"); return nullptr; }   // include\dolphin\dvd.h
s32 DVDGetCommandBlockStatus(const DVDCommandBlock* block) { PORT_TODO("DVDGetCommandBlockStatus"); return 0; }   // include\dolphin\dvd.h
DVDDiskID* DVDGetCurrentDiskID() { PORT_TODO("DVDGetCurrentDiskID"); return nullptr; }   // include\dolphin\dvd.h
void* DVDGetFSTLocation() { PORT_TODO("DVDGetFSTLocation"); return nullptr; }   // include\dolphin\dvd.h
int DVDOpenDir(const char* dirName, DVDDir* dir) { PORT_TODO("DVDOpenDir"); return 0; }   // include\dolphin\dvd.h
int DVDReadDir(DVDDir* dir, DVDDirEntry* dirent) { PORT_TODO("DVDReadDir"); return 0; }   // include\dolphin\dvd.h
void GXBeginDisplayList(void* list, u32 size) { PORT_TODO("GXBeginDisplayList"); }   // include\dolphin\gx\GXDispList.h
void GXCallDisplayList(void* list, u32 nbytes) { PORT_TODO("GXCallDisplayList"); }   // include\dolphin\gx\GXDispList.h
void GXClearBoundingBox() { PORT_TODO("GXClearBoundingBox"); }   // include\dolphin\gx\GXFrameBuffer.h
void GXCopyTex(void* dest, GXBool clear) { PORT_TODO("GXCopyTex"); }   // include\dolphin\gx\GXFrameBuffer.h
void GXDrawDone() { PORT_TODO("GXDrawDone"); }   // include\dolphin\gx\GXManage.h
void GXDrawTorus(f32 rc, u8 numc, u8 numt) { PORT_TODO("GXDrawTorus"); }   // include\dolphin\gx\GXDraw.h
void GXEnableTexOffsets(GXTexCoordID coord, u8 line_enable, u8 point_enable) { PORT_TODO("GXEnableTexOffsets"); }   // include\dolphin\gx\GXGeometry.h
u32 GXEndDisplayList() { PORT_TODO("GXEndDisplayList"); return 0; }   // include\dolphin\gx\GXDispList.h
void GXGetProjectionv(f32* ptr) { PORT_TODO("GXGetProjectionv"); }   // include\dolphin\gx\GXGet.h
void* GXGetTexObjData(const GXTexObj* to) { PORT_TODO("GXGetTexObjData"); return nullptr; }   // include\dolphin\gx\GXGet.h
void GXGetViewportv(f32* vp) { PORT_TODO("GXGetViewportv"); }   // include\dolphin\gx\GXGet.h
void GXInitLightAttn(GXLightObj* lt_obj, f32 a0, f32 a1, f32 a2, f32 k0, f32 k1, f32 k2) { PORT_TODO("GXInitLightAttn"); }   // include\dolphin\gx\GXLighting.h
void GXInitLightAttnK(GXLightObj* lt_obj, f32 k0, f32 k1, f32 k2) { PORT_TODO("GXInitLightAttnK"); }   // include\dolphin\gx\GXLighting.h
void GXInitLightColor(GXLightObj* lt_obj, GXColor color) { PORT_TODO("GXInitLightColor"); }   // include\dolphin\gx\GXLighting.h
void GXInitLightDir(GXLightObj* lt_obj, f32 nx, f32 ny, f32 nz) { PORT_TODO("GXInitLightDir"); }   // include\dolphin\gx\GXLighting.h
void GXInitLightDistAttn(GXLightObj* lt_obj, f32 ref_dist, f32 ref_br, GXDistAttnFn dist_func) { PORT_TODO("GXInitLightDistAttn"); }   // include\dolphin\gx\GXLighting.h
void GXInitLightPos(GXLightObj* lt_obj, f32 x, f32 y, f32 z) { PORT_TODO("GXInitLightPos"); }   // include\dolphin\gx\GXLighting.h
void GXInitLightSpot(GXLightObj* lt_obj, f32 cutoff, GXSpotFn spot_func) { PORT_TODO("GXInitLightSpot"); }   // include\dolphin\gx\GXLighting.h
void GXInvalidateVtxCache() { PORT_TODO("GXInvalidateVtxCache"); }   // include\dolphin\gx\GXGeometry.h
void GXLoadLightObjImm(const GXLightObj* lt_obj, GXLightID light) { PORT_TODO("GXLoadLightObjImm"); }   // include\dolphin\gx\GXLighting.h
void GXLoadNrmMtxImm(const f32 mtx[3][4], u32 id) { PORT_TODO("GXLoadNrmMtxImm"); }   // include\dolphin\gx\GXTransform.h
void GXLoadTexMtxImm(const f32 mtx[][4], u32 id, GXTexMtxType type) { PORT_TODO("GXLoadTexMtxImm"); }   // include\dolphin\gx\GXTransform.h
void GXPeekZ(u16 x, u16 y, u32* z) { PORT_TODO("GXPeekZ"); }   // include\dolphin\gx\GXCpu2Efb.h
void GXPixModeSync() { PORT_TODO("GXPixModeSync"); }   // include\dolphin\gx\GXManage.h
void GXProject(f32 x, f32 y, f32 z, const f32 mtx[3][4], const f32* pm, const f32* vp, f32* sx, f32* sy, f32* sz) { PORT_TODO("GXProject"); }   // include\dolphin\gx\GXTransform.h
void GXSetAlphaCompare(GXCompare comp0, u8 ref0, GXAlphaOp op, GXCompare comp1, u8 ref1) { PORT_TODO("GXSetAlphaCompare"); }   // include\dolphin\gx\GXTev.h
void GXSetAlphaUpdate(GXBool update_enable) { PORT_TODO("GXSetAlphaUpdate"); }   // include\dolphin\gx\GXPixel.h
void GXSetArray(GXAttr attr, void* base_ptr, u8 stride) { PORT_TODO("GXSetArray"); }   // include\dolphin\gx\GXGeometry.h
void GXSetChanAmbColor(GXChannelID chan, GXColor amb_color) { PORT_TODO("GXSetChanAmbColor"); }   // include\dolphin\gx\GXLighting.h
void GXSetChanCtrl(GXChannelID chan, GXBool enable, GXColorSrc amb_src, GXColorSrc mat_src, u32 light_mask, GXDiffuseFn diff_fn, GXAttnFn attn_fn) { PORT_TODO("GXSetChanCtrl"); }   // include\dolphin\gx\GXLighting.h
void GXSetChanMatColor(GXChannelID chan, GXColor mat_color) { PORT_TODO("GXSetChanMatColor"); }   // include\dolphin\gx\GXLighting.h
void GXSetColorUpdate(GXBool update_enable) { PORT_TODO("GXSetColorUpdate"); }   // include\dolphin\gx\GXPixel.h
void GXSetCopyFilter(GXBool aa, const u8 sample_pattern[12][2], GXBool vf, const u8 vfilter[7]) { PORT_TODO("GXSetCopyFilter"); }   // include\dolphin\gx\GXFrameBuffer.h
OSThread* GXSetCurrentGXThread() { PORT_TODO("GXSetCurrentGXThread"); return nullptr; }   // include\dolphin\gx\GXFifo.h
void GXSetCurrentMtx(u32 id) { PORT_TODO("GXSetCurrentMtx"); }   // include\dolphin\gx\GXTransform.h
void GXSetDispCopyDst(u16 wd, u16 ht) { PORT_TODO("GXSetDispCopyDst"); }   // include\dolphin\gx\GXFrameBuffer.h
void GXSetDispCopyGamma(GXGamma gamma) { PORT_TODO("GXSetDispCopyGamma"); }   // include\dolphin\gx\GXFrameBuffer.h
void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht) { PORT_TODO("GXSetDispCopySrc"); }   // include\dolphin\gx\GXFrameBuffer.h
u32 GXSetDispCopyYScale(f32 vscale) { PORT_TODO("GXSetDispCopyYScale"); return 0; }   // include\dolphin\gx\GXFrameBuffer.h
void GXSetDither(GXBool dither) { PORT_TODO("GXSetDither"); }   // include\dolphin\gx\GXPixel.h
void GXSetDrawSync(u16 token) { PORT_TODO("GXSetDrawSync"); }   // include\dolphin\gx.h
GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb) { PORT_TODO("GXSetDrawSyncCallback"); return {}; }   // include\dolphin\gx\GXManage.h
void GXSetDstAlpha(GXBool enable, u8 alpha) { PORT_TODO("GXSetDstAlpha"); }   // include\dolphin\gx\GXPixel.h
void GXSetFog(GXFogType type, f32 startz, f32 endz, f32 nearz, f32 farz, GXColor color) { PORT_TODO("GXSetFog"); }   // include\dolphin\gx\GXPixel.h
void GXSetIndTexCoordScale(GXIndTexStageID ind_state, GXIndTexScale scale_s, GXIndTexScale scale_t) { PORT_TODO("GXSetIndTexCoordScale"); }   // include\dolphin\gx\GXBump.h
void GXSetIndTexMtx(GXIndTexMtxID mtx_id, const f32 offset[2][3], s8 scale_exp) { PORT_TODO("GXSetIndTexMtx"); }   // include\dolphin\gx\GXBump.h
void GXSetIndTexOrder(GXIndTexStageID ind_stage, GXTexCoordID tex_coord, GXTexMapID tex_map) { PORT_TODO("GXSetIndTexOrder"); }   // include\dolphin\gx\GXBump.h
void GXSetLineWidth(u8 width, GXTexOffset texOffsets) { PORT_TODO("GXSetLineWidth"); }   // include\dolphin\gx\GXGeometry.h
void GXSetNumIndStages(u8 nIndStages) { PORT_TODO("GXSetNumIndStages"); }   // include\dolphin\gx\GXBump.h
void GXSetPixelFmt(GXPixelFmt pix_fmt, GXZFmt16 z_fmt) { PORT_TODO("GXSetPixelFmt"); }   // include\dolphin\gx\GXPixel.h
void GXSetTevAlphaIn(GXTevStageID stage, GXTevAlphaArg a, GXTevAlphaArg b, GXTevAlphaArg c, GXTevAlphaArg d) { PORT_TODO("GXSetTevAlphaIn"); }   // include\dolphin\gx\GXTev.h
void GXSetTevAlphaOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out_reg) { PORT_TODO("GXSetTevAlphaOp"); }   // include\dolphin\gx\GXTev.h
void GXSetTevColor(GXTevRegID id, GXColor color) { PORT_TODO("GXSetTevColor"); }   // include\dolphin\gx\GXTev.h
void GXSetTevColorIn(GXTevStageID stage, GXTevColorArg a, GXTevColorArg b, GXTevColorArg c, GXTevColorArg d) { PORT_TODO("GXSetTevColorIn"); }   // include\dolphin\gx\GXTev.h
void GXSetTevColorOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out_reg) { PORT_TODO("GXSetTevColorOp"); }   // include\dolphin\gx\GXTev.h
void GXSetTevColorS10(GXTevRegID id, GXColorS10 color) { PORT_TODO("GXSetTevColorS10"); }   // include\dolphin\gx\GXTev.h
void GXSetTevDirect(GXTevStageID tev_stage) { PORT_TODO("GXSetTevDirect"); }   // include\dolphin\gx\GXBump.h
void GXSetTevIndBumpXYZ(GXTevStageID tev_stage, GXIndTexStageID ind_stage, GXIndTexMtxID matrix_sel) { PORT_TODO("GXSetTevIndBumpXYZ"); }   // include\dolphin\gx\GXBump.h
void GXSetTevIndWarp(GXTevStageID tev_stage, GXIndTexStageID ind_stage, u8 signed_offset, u8 replace_mode, GXIndTexMtxID matrix_sel) { PORT_TODO("GXSetTevIndWarp"); }   // include\dolphin\gx\GXBump.h
void GXSetTevKAlphaSel(GXTevStageID stage, GXTevKAlphaSel sel) { PORT_TODO("GXSetTevKAlphaSel"); }   // include\dolphin\gx\GXTev.h
void GXSetTevKColor(GXTevKColorID id, GXColor color) { PORT_TODO("GXSetTevKColor"); }   // include\dolphin\gx\GXTev.h
void GXSetTevKColorSel(GXTevStageID stage, GXTevKColorSel sel) { PORT_TODO("GXSetTevKColorSel"); }   // include\dolphin\gx\GXTev.h
void GXSetTevOp(GXTevStageID id, GXTevMode mode) { PORT_TODO("GXSetTevOp"); }   // include\dolphin\gx\GXTev.h
void GXSetTevSwapMode(GXTevStageID stage, GXTevSwapSel ras_sel, GXTevSwapSel tex_sel) { PORT_TODO("GXSetTevSwapMode"); }   // include\dolphin\gx\GXTev.h
void GXSetTevSwapModeTable(GXTevSwapSel table, GXTevColorChan red, GXTevColorChan green, GXTevColorChan blue, GXTevColorChan alpha) { PORT_TODO("GXSetTevSwapModeTable"); }   // include\dolphin\gx\GXTev.h
void GXSetTexCoordGen2(GXTexCoordID dst_coord, GXTexGenType func, GXTexGenSrc src_param, u32 mtx, GXBool normalize, u32 pt_texmtx) { PORT_TODO("GXSetTexCoordGen2"); }   // include\dolphin\gx\GXGeometry.h
void GXSetTexCopyDst(u16 wd, u16 ht, GXTexFmt fmt, GXBool mipmap) { PORT_TODO("GXSetTexCopyDst"); }   // include\dolphin\gx\GXFrameBuffer.h
void GXSetTexCopySrc(u16 left, u16 top, u16 wd, u16 ht) { PORT_TODO("GXSetTexCopySrc"); }   // include\dolphin\gx\GXFrameBuffer.h
void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz, u32 field) { PORT_TODO("GXSetViewportJitter"); }   // include\dolphin\gx\GXTransform.h
void GXSetZCompLoc(GXBool before_tex) { PORT_TODO("GXSetZCompLoc"); }   // include\dolphin\gx\GXPixel.h
OSTime OSCalendarTimeToTicks(OSCalendarTime* td) { PORT_TODO("OSCalendarTimeToTicks"); return {}; }   // include\dolphin\os.h
void OSCancelThread(OSThread* thread) { PORT_TODO("OSCancelThread"); }   // include\dolphin\os\OSThread.h
int OSCreateThread(OSThread* thread, void* (*func)(void*), void* param, void* stack, u32 stackSize, OSPriority priority, u16 attr) { PORT_TODO("OSCreateThread"); return 0; }   // include\dolphin\os\OSThread.h
s32 OSEnableScheduler() { PORT_TODO("OSEnableScheduler"); return 0; }   // include\dolphin\os\OSThread.h
void OSExitThread(void* val) { PORT_TODO("OSExitThread"); }   // include\dolphin\os\OSThread.h
OSThread* OSGetCurrentThread() { PORT_TODO("OSGetCurrentThread"); return nullptr; }   // include\dolphin\os\OSThread.h
u16 OSGetFontEncode() { PORT_TODO("OSGetFontEncode"); return 0; }   // include\dolphin\os\OSFont.h
char* OSGetFontTexture(const char* string, void** image, s32* x, s32* y, s32* width) { PORT_TODO("OSGetFontTexture"); return nullptr; }   // include\dolphin\os\OSFont.h
u32 OSGetProgressiveMode() { PORT_TODO("OSGetProgressiveMode"); return 0; }   // include\dolphin\os\OSRtc.h
u32 OSGetSoundMode() { PORT_TODO("OSGetSoundMode"); return 0; }   // include\dolphin\os.h
void OSInitAlarm() { PORT_TODO("OSInitAlarm"); }   // include\dolphin\os\OSAlarm.h
BOOL OSInitFont(OSFontHeader* fontData) { PORT_TODO("OSInitFont"); return 0; }   // include\dolphin\os\OSFont.h
void OSInitSemaphore(OSSemaphore* sem, s32 count) { PORT_TODO("OSInitSemaphore"); }   // include\dolphin\os\OSSemaphore.h
void OSInitStopwatch(OSStopwatch* sw, char* name) { PORT_TODO("OSInitStopwatch"); }   // include\dolphin\os.h
void OSInitThreadQueue(OSThreadQueue* queue) { PORT_TODO("OSInitThreadQueue"); }   // include\dolphin\os\OSThread.h
BOOL OSLink(OSModuleInfo* newModule, void* bss) { PORT_TODO("OSLink"); return 0; }   // include\dolphin\os\OSModule.h
void* OSPhysicalToCached(u32 paddr) { PORT_TODO("OSPhysicalToCached"); return nullptr; }   // include\dolphin\os.h
void OSResetStopwatch(OSStopwatch* sw) { PORT_TODO("OSResetStopwatch"); }   // include\dolphin\os.h
void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu) { PORT_TODO("OSResetSystem"); }   // include\dolphin\os\OSReset.h
s32 OSResumeThread(OSThread* thread) { PORT_TODO("OSResumeThread"); return 0; }   // include\dolphin\os\OSThread.h
OSErrorHandler OSSetErrorHandler(OSError error, OSErrorHandler handler) { PORT_TODO("OSSetErrorHandler"); return {}; }   // include\dolphin\os\OSError.h
void OSSetProgressiveMode(u32 on) { PORT_TODO("OSSetProgressiveMode"); }   // include\dolphin\os\OSRtc.h
void OSSetSaveRegion(void* start, void* end) { PORT_TODO("OSSetSaveRegion"); }   // include\dolphin\os\OSReboot.h
void OSSetSoundMode(u32 mode) { PORT_TODO("OSSetSoundMode"); }   // include\dolphin\os.h
s32 OSSignalSemaphore(OSSemaphore* sem) { PORT_TODO("OSSignalSemaphore"); return 0; }   // include\dolphin\os\OSSemaphore.h
void OSSleepThread(OSThreadQueue* queue) { PORT_TODO("OSSleepThread"); }   // include\dolphin\os\OSThread.h
void OSStartStopwatch(OSStopwatch* sw) { PORT_TODO("OSStartStopwatch"); }   // include\dolphin\os.h
void OSStopStopwatch(OSStopwatch* sw) { PORT_TODO("OSStopStopwatch"); }   // include\dolphin\os.h
s32 OSSuspendThread(OSThread* thread) { PORT_TODO("OSSuspendThread"); return 0; }   // include\dolphin\os\OSThread.h
void OSTicksToCalendarTime(OSTime ticks, OSCalendarTime* td) { PORT_TODO("OSTicksToCalendarTime"); }   // include\dolphin\os.h
BOOL OSUnlink(OSModuleInfo* oldModule) { PORT_TODO("OSUnlink"); return 0; }   // include\dolphin\os\OSModule.h
s32 OSWaitSemaphore(OSSemaphore* sem) { PORT_TODO("OSWaitSemaphore"); return 0; }   // include\dolphin\os\OSSemaphore.h
void OSWakeupThread(OSThreadQueue* queue) { PORT_TODO("OSWakeupThread"); }   // include\dolphin\os\OSThread.h
