// -*- coding: utf-8 -*-
// GENERE par port/gen_link_stubs.py -- ne pas editer a la main.
//
// Les symboles que le lien reclamait et qu'aucune couche du portage ne fournit encore :
// le middleware audio et video de CRI, le synthetiseur, le mixeur, les compteurs de
// performance du PowerPC. Chacun journalise son premier appel, pour que ce soit
// **l'execution** qui dise lesquels comptent vraiment, et dans quel ordre les ecrire.
#include <dolphin.h>
#include <dolphin/axfx.h>
#include <dolphin/mix.h>
#include <dolphin/syn.h>
#include <dolphin/seq.h>

// include/card.h et include/dolphin/card.h declarent les memes fonctions avec des
// typedefs differents et se contredisent : on ne peut inclure que le second, et le
// type manquant est redeclare ici a l identique.
typedef void (*CardCallback)(s32 chan, s32 result);
#include <charPipeline/texPalette.h>

#include "port_log.h"

extern "C" {

__attribute__((weak)) int ADXGC_SetupDvdFs() { PORT_TODO("ADXGC_SetupDvdFs"); return 0; }   // prototype inconnu
__attribute__((weak)) int ADXM_ExecMain() { PORT_TODO("ADXM_ExecMain"); return 0; }   // prototype inconnu
__attribute__((weak)) int ADXM_SetCbErr() { PORT_TODO("ADXM_SetCbErr"); return 0; }   // prototype inconnu
__attribute__((weak)) int ADXT_SetOutputMono() { PORT_TODO("ADXT_SetOutputMono"); return 0; }   // prototype inconnu
__attribute__((weak)) void AIInit(u8* stack) { PORT_TODO("AIInit"); }   // include/dolphin/ai.h
__attribute__((weak)) void AIReset(void) { PORT_TODO("AIReset"); }   // include/dolphin/ai.h
__attribute__((weak)) void AXFXChorusCallback(AXFX_BUFFERUPDATE* bufferUpdate, AXFX_CHORUS* chorus) { PORT_TODO("AXFXChorusCallback"); }   // include/dolphin/axfx.h
__attribute__((weak)) void AXFXDelayCallback(AXFX_BUFFERUPDATE* bufferUpdate, AXFX_DELAY* delay) { PORT_TODO("AXFXDelayCallback"); }   // include/dolphin/axfx.h
__attribute__((weak)) void AXFXReverbHiCallback(AXFX_BUFFERUPDATE* bufferUpdate, AXFX_REVERBHI* reverb) { PORT_TODO("AXFXReverbHiCallback"); }   // include/dolphin/axfx.h
__attribute__((weak)) void AXFXReverbHiCallbackDpl2(AXFX_BUFFERUPDATE_DPL2* bufferUpdate, AXFX_REVERBHI_DPL2* reverb) { PORT_TODO("AXFXReverbHiCallbackDpl2"); }   // include/dolphin/axfx.h
__attribute__((weak)) void AXFXReverbStdCallback(AXFX_BUFFERUPDATE* bufferUpdate, AXFX_REVERBSTD* reverb) { PORT_TODO("AXFXReverbStdCallback"); }   // include/dolphin/axfx.h
__attribute__((weak)) int AXFXSetHooks() { PORT_TODO("AXFXSetHooks"); return 0; }   // prototype inconnu
__attribute__((weak)) int AXSetCompressor() { PORT_TODO("AXSetCompressor"); return 0; }   // prototype inconnu
__attribute__((weak)) int CARDFormatAsync() { PORT_TODO("CARDFormatAsync"); return 0; }   // prototype inconnu
__attribute__((weak)) void C_MTXFrustum(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f) { PORT_TODO("C_MTXFrustum"); }   // include/dolphin/mtx.h
__attribute__((weak)) void C_QUATMtx(Quaternion* r, const Mtx m) { PORT_TODO("C_QUATMtx"); }   // include/dolphin/mtx.h
__attribute__((weak)) void C_QUATSlerp(const Quaternion* p, const Quaternion* q, Quaternion* r, f32 t) { PORT_TODO("C_QUATSlerp"); }   // include/dolphin/mtx.h
__attribute__((weak)) void MIXInit(void) { PORT_TODO("MIXInit"); }   // include/dolphin/mix.h
__attribute__((weak)) void MIXQuit(void) { PORT_TODO("MIXQuit"); }   // include/dolphin/mix.h
__attribute__((weak)) void MIXSetAuxA(AXVPB* p, int dB) { PORT_TODO("MIXSetAuxA"); }   // include/dolphin/mix.h
__attribute__((weak)) void MIXSetAuxB(AXVPB* p, int dB) { PORT_TODO("MIXSetAuxB"); }   // include/dolphin/mix.h
__attribute__((weak)) void MIXSetSoundMode(u32 mode) { PORT_TODO("MIXSetSoundMode"); }   // include/dolphin/mix.h
__attribute__((weak)) void MIXUpdateSettings(void) { PORT_TODO("MIXUpdateSettings"); }   // include/dolphin/mix.h
__attribute__((weak)) int PCclose() { PORT_TODO("PCclose"); return 0; }   // prototype inconnu
__attribute__((weak)) int PCcreat() { PORT_TODO("PCcreat"); return 0; }   // prototype inconnu
__attribute__((weak)) int PCinit() { PORT_TODO("PCinit"); return 0; }   // prototype inconnu
__attribute__((weak)) int PClseek() { PORT_TODO("PClseek"); return 0; }   // prototype inconnu
__attribute__((weak)) int PCopen() { PORT_TODO("PCopen"); return 0; }   // prototype inconnu
__attribute__((weak)) int PCread() { PORT_TODO("PCread"); return 0; }   // prototype inconnu
__attribute__((weak)) int PCwrite() { PORT_TODO("PCwrite"); return 0; }   // prototype inconnu
__attribute__((weak)) u32 PPCMfpmc1() { PORT_TODO("PPCMfpmc1"); return (u32) 0; }   // include/dolphin/base/PPCArch.h
__attribute__((weak)) void PPCMtmmcr0(u32 newMmcr0) { PORT_TODO("PPCMtmmcr0"); }   // include/dolphin/base/PPCArch.h
__attribute__((weak)) void PPCMtmmcr1(u32 newMmcr0) { PORT_TODO("PPCMtmmcr1"); }   // include/dolphin/base/PPCArch.h
__attribute__((weak)) void PPCMtpmc1(u32 newPmc1) { PORT_TODO("PPCMtpmc1"); }   // include/dolphin/base/PPCArch.h
__attribute__((weak)) void PPCMtpmc2(u32 newPmc1) { PORT_TODO("PPCMtpmc2"); }   // include/dolphin/base/PPCArch.h
__attribute__((weak)) void PPCMtpmc3(u32 newPmc1) { PORT_TODO("PPCMtpmc3"); }   // include/dolphin/base/PPCArch.h
__attribute__((weak)) void PPCMtpmc4(u32 newPmc1) { PORT_TODO("PPCMtpmc4"); }   // include/dolphin/base/PPCArch.h
__attribute__((weak)) void SEQInit(void) { PORT_TODO("SEQInit"); }   // include/dolphin/seq.h
__attribute__((weak)) void SEQQuit(void) { PORT_TODO("SEQQuit"); }   // include/dolphin/seq.h
__attribute__((weak)) void SEQRunAudioFrame(void) { PORT_TODO("SEQRunAudioFrame"); }   // include/dolphin/seq.h
__attribute__((weak)) u32 SYNGetActiveNotes(SYNSYNTH* synth) { PORT_TODO("SYNGetActiveNotes"); return (u32) 0; }   // include/dolphin/syn.h
__attribute__((weak)) void SYNInit(void) { PORT_TODO("SYNInit"); }   // include/dolphin/syn.h
__attribute__((weak)) void SYNInitSynth(SYNSYNTH* synth, void* wavetable, u32 aramBase, u32 zeroBase, u32 priorityVoiceAlloc, u32 priorityNoteOn, u32 priorityNoteRelease) { PORT_TODO("SYNInitSynth"); }   // include/dolphin/syn.h
__attribute__((weak)) void SYNMidiInput(SYNSYNTH* synth, u8* input) { PORT_TODO("SYNMidiInput"); }   // include/dolphin/syn.h
__attribute__((weak)) void SYNQuit(void) { PORT_TODO("SYNQuit"); }   // include/dolphin/syn.h
__attribute__((weak)) void SYNQuitSynth(SYNSYNTH* synth) { PORT_TODO("SYNQuitSynth"); }   // include/dolphin/syn.h
__attribute__((weak)) void SYNRunAudioFrame(void) { PORT_TODO("SYNRunAudioFrame"); }   // include/dolphin/syn.h
__attribute__((weak)) void SYNSetMasterVolume(SYNSYNTH* synth, s32 dB) { PORT_TODO("SYNSetMasterVolume"); }   // include/dolphin/syn.h
// TEXGet : implemente dans port/texobj.cpp.
__attribute__((weak)) int mwPlyCalcWorkCprmSfd() { PORT_TODO("mwPlyCalcWorkCprmSfd"); return 0; }   // prototype inconnu
__attribute__((weak)) int mwPlyCreateSofdec() { PORT_TODO("mwPlyCreateSofdec"); return 0; }   // prototype inconnu
__attribute__((weak)) int mwPlyFxCnvFrmARGB8888() { PORT_TODO("mwPlyFxCnvFrmARGB8888"); return 0; }   // prototype inconnu
__attribute__((weak)) int mwPlyFxCnvFrmY84C44() { PORT_TODO("mwPlyFxCnvFrmY84C44"); return 0; }   // prototype inconnu
__attribute__((weak)) int mwPlyFxSetOutBufPitchHeight() { PORT_TODO("mwPlyFxSetOutBufPitchHeight"); return 0; }   // prototype inconnu
__attribute__((weak)) int mwPlyFxSetOutBufSize() { PORT_TODO("mwPlyFxSetOutBufSize"); return 0; }   // prototype inconnu
__attribute__((weak)) int mwPlyGetCurFrm() { PORT_TODO("mwPlyGetCurFrm"); return 0; }   // prototype inconnu
__attribute__((weak)) int mwPlyGetHdrInf() { PORT_TODO("mwPlyGetHdrInf"); return 0; }   // prototype inconnu
__attribute__((weak)) int mwPlyGetNumSkipDec() { PORT_TODO("mwPlyGetNumSkipDec"); return 0; }   // prototype inconnu
__attribute__((weak)) int mwPlyGetNumSkipDisp() { PORT_TODO("mwPlyGetNumSkipDisp"); return 0; }   // prototype inconnu
__attribute__((weak)) int mwPlyInitSfdFx() { PORT_TODO("mwPlyInitSfdFx"); return 0; }   // prototype inconnu
__attribute__((weak)) int mwPlyRelCurFrm() { PORT_TODO("mwPlyRelCurFrm"); return 0; }   // prototype inconnu

// Ajoutees a la main (phase N1) : materiel audio appele par le son logiciel de la decompilation (syn, mix).
__attribute__((weak)) void AXSetVoiceItdOn(AXVPB* p) { PORT_TODO("AXSetVoiceItdOn"); }   // include/dolphin/ax.h
__attribute__((weak)) void AXSetVoiceItdTarget(AXVPB* p, u16 lShift, u16 rShift) { PORT_TODO("AXSetVoiceItdTarget"); }   // include/dolphin/ax.h
__attribute__((weak)) void AISetStreamVolLeft(u8 vol) { PORT_TODO("AISetStreamVolLeft"); }   // include/dolphin/ai.h
__attribute__((weak)) void AISetStreamVolRight(u8 vol) { PORT_TODO("AISetStreamVolRight"); }   // include/dolphin/ai.h

}
