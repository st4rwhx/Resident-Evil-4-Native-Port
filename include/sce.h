#ifndef SCE_H
#define SCE_H

#include "types.h"
#include "vec.h"

class cEm;

// Scenario helpers (game/sce_com.cpp / sce_sys.cpp), C linkage.
extern "C" {
void SceEventStart(int mode);
void SceEventEnd(int mode);
void SceSleep(int frames);
void SceUpCutStart();
void SceUpCutEnd();
int SceCheckEventStart();
void SceSetRoomExitFunc(int a, int b);
void SetFree(int no, u32 v);
u32 GetFree(int no);
void SceMesSet(int no, u32 flags, int sel, int x, int y);
void SceMesCamSndSet(int no, int cut, int se);
void SceUpCut(int a, int b, int c, int flags);
int SceMesGetSelection();
void SceMesWait();
void SceSndCallThunder();
int SceCheckEmAlive(cEm* em);
int SceCountEmAlive(int lo, int hi);
void SceDestroyEm(int lo, int hi);
void SceInitItemEvent();
void SceSetItemEvent(int atNo, int itemNo, int flagNo, int cut, void (*func)(int), void (*doneFunc)(), int arg, int enable);
void getChapterSection(int chapter, int* chap, int* sec);
void SceChapterEnd();
void SceSetChapterEnd(int chapter, int doorAt);
void SceCamMove(Vec* pos, Vec* at, f32 fovy);
void OpenBoxMain(int type, int mode, int se, u32 id1, u32 id2, int itemNo);
void SceDebugDisp(const char* fmt, ...);
}

// sce_com.cpp: debug trigger check, always 0 (title's mercenaries unlock-all). C++ linkage.
int DebugTrg(int no);

#endif
