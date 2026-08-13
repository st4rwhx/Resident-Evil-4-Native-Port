# Inventaire GX pour le GX natif

Fonctions definies dans src/lib/GX*.c : 306 ; appelees par le jeu : 97 ; noms GX appeles hors bibliotheque : 14 ; acces directs a GXWGFifo : 11 dans 2 fichiers.

## GXTev : 15 fonctions, 872 appels

| fonction | appels | fichiers | ecrit | appelle |
|---|---|---|---|---|
| GXSetTevOrder | 141 | 45 | BP | - |
| GXSetTevColorOp | 129 | 29 | BP | - |
| GXSetTevAlphaOp | 118 | 28 | BP | - |
| GXSetTevColorIn | 117 | 29 | BP | - |
| GXSetTevAlphaIn | 114 | 28 | BP | - |
| GXSetNumTevStages | 93 | 45 | etat | - |
| GXSetAlphaCompare | 39 | 23 | BP | - |
| GXSetTevOp | 39 | 22 | BP | - |
| GXSetTevKColorSel | 17 | 3 | BP | - |
| GXSetTevSwapModeTable | 17 | 2 | BP | - |
| GXSetTevKAlphaSel | 15 | 3 | BP | - |
| GXSetTevKColor | 13 | 3 | BP | - |
| GXSetTevSwapMode | 13 | 2 | BP | - |
| GXSetTevColor | 6 | 3 | BP | - |
| GXSetTevColorS10 | 1 | 1 | BP | - |

## GXAttr : 7 fonctions, 639 appels

| fonction | appels | fichiers | ecrit | appelle |
|---|---|---|---|---|
| GXSetVtxAttrFmt | 199 | 49 | - | - |
| GXSetVtxDesc | 197 | 49 | etat | - |
| GXSetNumTexGens | 88 | 45 | XF | - |
| GXClearVtxDesc | 78 | 49 | etat | - |
| GXSetTexCoordGen2 | 40 | 8 | XF | __GXSetMatrixIndex |
| GXSetArray | 35 | 6 | BP | - |
| GXInvalidateVtxCache | 2 | 1 | FIFO | - |

## GXPixel : 9 fonctions, 314 appels

| fonction | appels | fichiers | ecrit | appelle |
|---|---|---|---|---|
| GXSetBlendMode | 103 | 49 | BP | - |
| GXSetZMode | 91 | 42 | BP | - |
| GXSetAlphaUpdate | 49 | 21 | BP | - |
| GXSetColorUpdate | 28 | 20 | BP | - |
| GXSetFog | 26 | 21 | BP | - |
| GXSetDstAlpha | 7 | 3 | BP | - |
| GXSetPixelFmt | 4 | 1 | BP | - |
| GXSetZCompLoc | 4 | 3 | BP | - |
| GXSetDither | 2 | 1 | BP | - |

## GXTransform : 11 fonctions, 299 appels

| fonction | appels | fichiers | ecrit | appelle |
|---|---|---|---|---|
| GXLoadPosMtxImm | 75 | 49 | XF FIFO | - |
| GXSetCurrentMtx | 70 | 47 | etat | __GXSetMatrixIndex |
| GXLoadTexMtxImm | 51 | 20 | XF FIFO | - |
| GXSetProjection | 48 | 33 | - | __GXSetProjection |
| GXLoadNrmMtxImm | 22 | 16 | XF FIFO | - |
| GXSetScissor | 10 | 5 | BP | - |
| GXSetViewport | 9 | 4 | - | GXSetViewportJitter |
| GXGetProjectionv | 4 | 2 | - | - |
| GXGetViewportv | 4 | 2 | - | - |
| GXProject | 4 | 2 | - | - |
| GXSetViewportJitter | 2 | 1 | - | __GXSetViewport |

## GXLight : 12 fonctions, 281 appels

| fonction | appels | fichiers | ecrit | appelle |
|---|---|---|---|---|
| GXSetChanCtrl | 87 | 35 | XF | - |
| GXSetNumChans | 75 | 38 | XF | - |
| GXSetChanMatColor | 37 | 20 | XF | - |
| GXSetChanAmbColor | 29 | 20 | XF | - |
| GXLoadLightObjImm | 12 | 3 | XF FIFO | - |
| GXInitLightDir | 11 | 3 | - | - |
| GXInitLightPos | 9 | 3 | - | - |
| GXInitLightAttn | 8 | 2 | - | - |
| GXInitLightColor | 5 | 3 | - | - |
| GXInitLightSpot | 5 | 3 | - | - |
| GXInitLightDistAttn | 2 | 2 | - | - |
| GXInitLightAttnK | 1 | 1 | - | - |

## GXTexture : 9 fonctions, 233 appels

| fonction | appels | fichiers | ecrit | appelle |
|---|---|---|---|---|
| GXLoadTexObj | 66 | 31 | - | GXLoadTexObjPreLoaded |
| GXInitTexObj | 55 | 30 | etat | - |
| GXInvalidateTexAll | 26 | 22 | BP | __GXFlushTextureState |
| GXLoadTlut | 23 | 19 | BP | __GXFlushTextureState |
| GXInitTexObjLOD | 21 | 18 | etat | - |
| GXInitTexObjCI | 19 | 16 | - | GXInitTexObj |
| GXInitTlutObj | 18 | 16 | etat | - |
| GXGetTexBufferSize | 3 | 1 | - | __GXGetTexTileShift |
| GXGetTexObjData | 2 | 2 | - | - |

## GXGeometry : 4 fonctions, 162 appels

| fonction | appels | fichiers | ecrit | appelle |
|---|---|---|---|---|
| GXBegin | 85 | 44 | FIFO | __GXSendFlushPrim __GXSetDirtyState |
| GXSetCullMode | 64 | 41 | etat | - |
| GXSetLineWidth | 11 | 7 | BP | - |
| GXEnableTexOffsets | 2 | 1 | BP | - |

## GXFrameBuf : 11 fonctions, 135 appels

| fonction | appels | fichiers | ecrit | appelle |
|---|---|---|---|---|
| GXSetCopyFilter | 31 | 14 | BP | - |
| GXSetTexCopyDst | 28 | 21 | etat | - |
| GXCopyTex | 26 | 21 | BP | __GXVerifCopy |
| GXSetTexCopySrc | 26 | 21 | etat | - |
| GXCopyDisp | 8 | 3 | BP | __GXVerifCopy |
| GXSetCopyClear | 4 | 3 | BP | - |
| GXSetDispCopyDst | 3 | 1 | etat | - |
| GXSetDispCopySrc | 3 | 1 | etat | - |
| GXSetDispCopyYScale | 3 | 1 | BP | __GXGetNumXfbLines |
| GXSetDispCopyGamma | 2 | 1 | etat | - |
| GXClearBoundingBox | 1 | 1 | BP | - |

## GXBump : 7 fonctions, 105 appels

| fonction | appels | fichiers | ecrit | appelle |
|---|---|---|---|---|
| GXSetTevDirect | 45 | 9 | - | GXSetTevIndirect |
| GXSetNumIndStages | 21 | 9 | etat | - |
| GXSetIndTexCoordScale | 10 | 7 | BP | - |
| GXSetIndTexOrder | 10 | 7 | BP | - |
| GXSetIndTexMtx | 9 | 6 | BP | - |
| GXSetTevIndWarp | 8 | 6 | - | GXSetTevIndirect |
| GXSetTevIndBumpXYZ | 2 | 1 | - | GXSetTevIndirect |

## GXMisc : 6 fonctions, 47 appels

| fonction | appels | fichiers | ecrit | appelle |
|---|---|---|---|---|
| GXPixModeSync | 29 | 25 | BP | - |
| GXDrawDone | 9 | 7 | - | GXSetDrawDone GXWaitDrawDone |
| GXPeekZ | 4 | 4 | etat | - |
| GXSetDrawSync | 2 | 1 | BP | GXFlush |
| GXSetDrawSyncCallback | 2 | 1 | - | - |
| GXAbortFrame | 1 | 1 | - | GXFlush GXGetGPFifo __GXAbort __GXCleanGPFifo __GXInitRevisionBits |

## GXDisplayList : 3 fonctions, 23 appels

| fonction | appels | fichiers | ecrit | appelle |
|---|---|---|---|---|
| GXCallDisplayList | 13 | 8 | FIFO | __GXSendFlushPrim __GXSetDirtyState |
| GXBeginDisplayList | 5 | 1 | - | GXGetCPUFifo GXResetWriteGatherPipe GXSaveCPUFifo GXSetCPUFifo __GXSetDirtyState |
| GXEndDisplayList | 5 | 1 | HW | GXFlush GXSetCPUFifo __GXSaveCPUFifoAux |

## GXFifo : 1 fonctions, 9 appels

| fonction | appels | fichiers | ecrit | appelle |
|---|---|---|---|---|
| GXSetCurrentGXThread | 9 | 2 | - | - |

## GXInit : 1 fonctions, 2 appels

| fonction | appels | fichiers | ecrit | appelle |
|---|---|---|---|---|
| GXInit | 2 | 1 | BP XF FIFO HW | GXInitFifoBase GXInitTexCacheRegion GXInitTlutRegion GXSetCPUFifo GXSetGPFifo GXSetMisc __ |

## GXDraw : 1 fonctions, 1 appels

| fonction | appels | fichiers | ecrit | appelle |
|---|---|---|---|---|
| GXDrawTorus | 1 | 1 | - | GXBegin GXGetVtxDesc GXSetVtxAttrFmt GXSetVtxDesc |

## Noms GX appeles par le jeu mais pas definis dans src/lib/GX*.c

GXPosition3f32 (228), GXTexCoord2f32 (187), GXColor4u8 (168), GXPosition3s16 (68), GXMatrixIndex1u8 (55), GXSetTexCoordGen (54), GXNormal3s8 (44), GXPosition2u16 (16), GXTexCoord2s16 (8), GXWORK (7), GXColor (2), GXColorW (2), GXNormal3f32 (2), GXVerifyCallback (1)

## Fichiers qui ecrivent directement dans GXWGFifo

src/game/dbmodule.cpp, src/game/mes.cpp

## Totaux (fonctions appelees par le jeu, par type d ecriture)

BP 41, etat 15, XF 11, FIFO 8, HW 2
