#ifndef EST_H
#define EST_H

#include "types.h"
#include "vec.h"
#include "model.h"

// game/est.cpp: effect set table (est) helpers. EstSet itself is declared in esp.h.
extern "C" {
void AreaSstSet(int id);
int GetSstDispFlag(u32 id);
void SetSstDispFlag(u32 id, int on);
void SetSstAddAreaFlag(u32 flag);
void SstSet(u32 owner, int type, int no, int lo, int hi, int move);
void EffectEspDelete(int a, int b, u32 c, cModel* model);
void EffectEspgenDelete(int a, int b, int c);
void EffectEfmDelete(int a, int b, int c);
void EffectDeleteAll();
void EffectEventDelete();
void EspDelete(int a, int b, u32 c, cModel* model);
void EspDeleteEvent();
void EspSetWaterBomb(Vec* pos);
void EspSetWaterHitmark(Vec* pos);
int EspChkInPuddle(Vec* pos, Vec* nrm);
void EspSetEatEffect(Vec* pos, Vec* nrm, int type, int wep);
void EventCutEstSet(int owner, u32 no);
void EventCutEffDelete();
void EventAllEffDelete();
int ChkWaterEffectEnable(Vec* pos);
void EstSetEm10WaterFall(Vec* pos);
}

#endif
