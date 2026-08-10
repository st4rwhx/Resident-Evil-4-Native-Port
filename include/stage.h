#ifndef STAGE_H
#define STAGE_H

#include "types.h"

// game/stage.cpp: enemy list (ESL) selection and the sub-missions (C linkage).
extern "C" {
int checkEmListNo(u16 room);
const char* getEmListName(u32 no);
const char* getEmListDbgName(int no);
int getEmListNum();
void StageSet();
void readEmList(int mode);
int checkSubMissionTarget(int stage, int no);
void SubMissionCheck();
}

#endif
