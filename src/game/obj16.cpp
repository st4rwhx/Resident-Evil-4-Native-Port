#include "atari.h"
#include "light.h"
#include "ctrl.h"
#include "obj.h"
#include "em.h"
#include "emhit.h"
#include "esp.h"
#include "est.h"
#include "global.h"
#include "main.h"
#include "math_sub.h"
#include "snd.h"
#include "rnd.h"
#include "pad.h"
#include "quake.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"

// Enemy head (obj 0x16): the head / mouth model of the plaga-carrying enemies, hung on a parts of
// its body (`o16.body`). It turns toward the player (obj16NeckMove), bites (R1_Atk, R1_Critical),
// takes damage motions (R1_Damage) and fades out once its enemies are dead.
class cObj16 : public cObj {
public:
    virtual void move();
    virtual ~cObj16() {}

    void setScale(Vec* s);
    void setDieEff();
    void clearLostWait();
    void setLostWait(int n);
    void setMotData(void* m0, void* m1, void* m2, void* m3, void* m4, void* m5, void* m6, void* m7, void* m8,
                    void* m9, void* m10);
    void setPlDmgMot(void* mot, int a);
    void setAtk(u8 flag);
    void setCritical();
    void setDamage();
    int ckAtkEnable();
    void setBurn();
    int ckAtkHit();
};

// Model part as obj16NeckMove writes it: the parts rotation and the override flag.
struct Obj16Parts {
    u8 pad_0[0x128];
    Vec rot;        // 0x128
    u8 pad_134[0x1C0 - 0x134];
    u32 flags;      // 0x1C0  bit30: rotation override
};

extern "C" {
int MotionMove(cModel* m, int a);
int EmAtkHitCk(void* atk, Vec* pos, Vec* oldPos, int a);
void LifeDownSet(cEm* em, int dmg, int a);
cObj* SetObj16(void* bin, void* tpl, cModel* target, cModel* body, int partsNo, u8 type, Vec* pos, Vec* rot);
void obj16_R1_Set(cObj16* obj);
void obj16_R1_CoreMove(cObj16* obj);
void obj16_R1_Atk(cObj16* obj);
void obj16_R1_Critical(cObj16* obj);
void obj16_R1_Damage(cObj16* obj);
void MotSetObj16(cObj* obj, void* mot, int a, int b);
void obj16MatCalc(cObj16* obj);
int obj16AtkCk(cObj16* obj, u32 kind, int partsNo);
void obj16PlHeadLost(cObj16* obj);
static void obj16NeckMove(cObj16* obj);
void plemDmMStar(cPlayer* pl);
}
void MotionSetCore(cModel* m, void* work, void* mot, int a, int b, int c, int d);

void (*Obj16_R1_move_tbl[5])(cObj16*) = {
    obj16_R1_Set, obj16_R1_CoreMove, obj16_R1_Atk, obj16_R1_Critical, obj16_R1_Damage,
};

// Attack parameters per obj16AtkCk kind (EmAtkHitCk).
EmAtkInfo obj16_atk_info[4] = {
    { 500.0f, 8, 0x320, 0, 0xA, 0 },
    { 500.0f, 8, 0x320, 0, 0xA, 0 },
    { 500.0f, 8, 0x1F4, 0, 0xA, 0 },
    { 800.0f, 8, 0x270F, 0, 0xA, 0 },
};

cObj* SetObj16(void* bin, void* tpl, cModel* target, cModel* body, int partsNo, u8 type, Vec* pos, Vec* rot)
{
    cObj* obj;
    Obj16Work* w;

    if (target == 0) {
        return 0;
    }
    if (target->pParts == 0) {
        return 0;
    }
    if (body == 0) {
        return 0;
    }
    if (body->pParts == 0) {
        return 0;
    }
    obj = ObjMgr.createBack(0x16);
    if (obj == 0) {
        return 0;
    }
    w = &obj->o16;
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetObj16() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 2000.0f, 2000.0f, 2000.0f };

    obj->sub2B4.atari.throughOn();
    obj->lightInfo.init2(0, 1, &p0, &p1, 2);
    obj->type = type;
    if (pos) {
        obj->pos = *pos;
    } else {
        obj->pos.x = 0.0f;
        obj->pos.y = 0.0f;
        obj->pos.z = 0.0f;
    }
    if (rot) {
        obj->rot = *rot;
    } else {
        obj->rot.x = 0.0f;
        obj->rot.y = 0.0f;
        obj->rot.z = 0.0f;
    }
    obj->oldPos = obj->pos;
    obj->scale.x = 0.0f;
    obj->scale.y = 0.0f;
    obj->scale.z = 0.0f;
    w->scale.x = 1.0f;
    w->scale.y = 1.0f;
    w->scale.z = 1.0f;
    w->ctrl12 = GetCtrlCtrl12();
    w->body = body;
    w->partsNo = partsNo;
    w->target = target;
    w->seTimer = 60;
    w->seHandle = 0;
    w->dieEffTimer = 0;
    w->espKind = 0x3D;
    w->espKind2 = 0x3E;
    w->lostWait = 150;
    w->estTimer = (Rnd() & 3) + 9;
    w->neckAng = 0.0f;
    w->x76 = 60;
    w->x72 = 0;
    w->mot[0] = 0;
    w->mot[1] = 0;
    w->mot[2] = 0;
    w->mot[4] = 0;
    w->mot[5] = 0;
    w->mot[6] = 0;
    w->mot[7] = 0;
    w->mot[8] = 0;
    w->mot[9] = 0;
    w->mot[10] = 0;
    w->plMot = 0;
    w->plMotA = 0;
    w->mot[3] = 0;
    w->x6C = 0;
    w->x73 = 0;
    w->x74 = 0;
    w->active = 0;
    w->atkHit = 0;
    obj16MatCalc((cObj16*) obj);
    w->x28 = 0;
    obj->xFC = 1;
    obj->xFD = 0;
    obj->xFE = 0;
    obj->xFF = 0;
    if (target->be_flag & 0x800) {
        obj->setNoSuspend(1);
    } else {
        obj->setNoSuspend(0);
    }
    return obj;
}

// Delete the effects the head owns and the head itself.
#define OBJ16_LOST(obj, w)                                    \
    EffectEspDelete(0, (w)->espKind, (u32) (obj), 0);         \
    EffectEspgenDelete(0, (w)->espKind, (int) (obj));         \
    EffectEfmDelete(0, (w)->espKind, (int) (obj));            \
    EffectEspDelete(0, (w)->espKind2, (u32) (obj), 0);        \
    EffectEspgenDelete(0, (w)->espKind2, (int) (obj));        \
    EffectEfmDelete(0, (w)->espKind2, (int) (obj));           \
    ObjMgr.destroy(obj)

void cObj16::move()
{
    Obj16Work* w = &o16;
    int alive;
    const f32 decRate = 0.9f;
    const f32 addRate = 0.1f;

    if (w->target && (w->target->be_flag & 0x201) != 1) {
        OBJ16_LOST(this, w);
        return;
    }
    if (w->body && (w->body->be_flag & 0x201) != 1) {
        OBJ16_LOST(this, w);
        return;
    }
    if (w->atkWait) {
        w->atkWait--;
    }
    switch (type) {
    case 8:
    case 9:
    case 0xA:
    case 0x11:
        break;
    default:
        if (w->flags & 1) {
            if (w->lostWait) {
                w->lostWait--;
            }
        }
        break;
    }
    if (w->target) {
        switch (type) {
        case 6:
        case 8:
        case 9:
        case 0xA:
        case 0x11:
            break;
        default:
            if (!(w->flags & 1)) {
                if (((cEm*) w->target)->hp > 0) {
                    w->lostWait = 90;
                }
            }
            break;
        }
        alive = 0;
        if (w->lostWait) {
            alive = 1;
        }
        switch (type) {
        case 6:
        case 8:
        case 9:
        case 0xA:
        case 0x11:
            break;
        default:
            if (!(w->flags & 1)) {
                if (((cEm*) w->target)->hp > 0) {
                    alive = 1;
                }
            }
            break;
        }
        if (alive == 0) {
            scale.y = scale.z = scale.x = scale.x * decRate;
            alpha *= 0.85f;
            if (alpha <= 0.01f) {
                alpha = 0.0f;
                OBJ16_LOST(this, w);
                return;
            }
        } else {
            scale.x = scale.x * decRate + w->scale.x * addRate;
            scale.y = scale.y * decRate + w->scale.y * addRate;
            scale.z = scale.z * decRate + w->scale.z * addRate;
        }
    }
    if (w->x76) {
        w->x76--;
    }
    if (w->body && type == 1) {
        if (((cEm*) w->body)->x39D) {
            w->scale.x = 0.5f;
            w->scale.y = 0.5f;
            w->scale.z = 0.5f;
        } else {
            w->scale.x = 1.0f;
            w->scale.y = 1.0f;
            w->scale.z = 1.0f;
        }
    }
    w->atkEnable = 0;
    Obj16_R1_move_tbl[xFD](this);
    if (w->target) {
        switch (type) {
        case 2:
        case 3:
            if (((cEm*) w->target)->hp > 0) {
                if (w->seTimer) {
                    w->seTimer--;
                } else {
                    w->seTimer = 29;
                    w->seHandle = SndCall(8, 0x13, &w->target->pos, w->target->id, 0, 0);
                }
            } else {
                SndStop(w->seHandle, 0);
            }
            break;
        }
        if (w->target) {
            switch (type) {
            case 2:
            case 3:
                if (w->dieEffTimer) {
                    if (--w->dieEffTimer == 0) {
                        w->dieEffTimer = 2;
                        if (be_flag & 0x800) {
                            EstSet((int) this, -1, 0, 0, 0x10, 0x2E, 1, 0, (u32) this, 0);
                        } else {
                            EstSet((int) this, -1, 0, 0, 0x10, 0x2E, 0, 0, (u32) this, 0);
                        }
                    }
                }
                break;
            case 0xB:
            case 0xD:
                if (w->dieEffTimer) {
                    if (--w->dieEffTimer == 0) {
                        w->dieEffTimer = 2;
                        if (be_flag & 0x800) {
                            EstSet((int) this, -1, 0, 0, 0x31, 0x16, 1, 0, (u32) this, 0);
                        } else {
                            EstSet((int) this, -1, 0, 0, 0x31, 0x16, 0, 0, (u32) this, 0);
                        }
                    }
                }
                break;
            }
        }
    }
    switch (type) {
    case 1:
        if (w->estTimer) {
            if (--w->estTimer == 0) {
                w->estTimer = (Rnd() & 3) + 15;
                if (be_flag & 0x800) {
                    EstSet((int) this, -1, 0, 0, 0x10, 4, 1, 0, (u32) this, 0);
                } else {
                    EstSet((int) this, -1, 0, 0, 0x10, 4, 0, 0, (u32) this, 0);
                }
            }
        }
        break;
    case 0xC:
        if (w->estTimer) {
            if (--w->estTimer == 0) {
                w->estTimer = (Rnd() & 3) + 15;
                if (be_flag & 0x800) {
                    EstSet((int) this, -1, 0, 0, 0x31, 0x10, 1, 0, (u32) this, 0);
                } else {
                    EstSet((int) this, -1, 0, 0, 0x31, 0x10, 0, 0, (u32) this, 0);
                }
            }
        }
        break;
    case 2:
        if (w->active) {
            if (w->estTimer) {
                if (--w->estTimer == 0) {
                    w->estTimer = 3;
                    EstSet((int) this, -1, 0, 0, 0x10, 0x52, 0, 0, (u32) this, 0);
                }
            }
        }
        break;
    case 0xD:
        if (w->active) {
            if (w->estTimer) {
                if (--w->estTimer == 0) {
                    w->estTimer = 3;
                    EstSet((int) this, -1, 0, 0, 0x31, 0x18, 0, 0, (u32) this, 0);
                }
            }
        }
        break;
    case 5:
        if (w->estTimer) {
            if (--w->estTimer == 0) {
                w->estTimer = (Rnd() & 3) + 15;
                if (be_flag & 0x800) {
                    EstSet((int) this, -1, 0, 0, 0x1A, 0xE, 1, 0, (u32) this, 0);
                } else {
                    EstSet((int) this, -1, 0, 0, 0x1A, 0xE, 0, 0, (u32) this, 0);
                }
            }
        }
        break;
    case 6:
        if (w->estTimer) {
            if (--w->estTimer == 0) {
                w->estTimer = (Rnd() & 3) + 9;
                if (be_flag & 0x800) {
                    EstSet((int) this, -1, 0, 0, 0x1A, 0xF, 1, 0, (u32) this, 0);
                } else {
                    EstSet((int) this, -1, 0, 0, 0x1A, 0xF, 0, 0, (u32) this, 0);
                }
            }
        }
        break;
    }
    if (w->target) {
        if (w->target->be_flag & 0x800) {
            setNoSuspend(1);
        } else {
            setNoSuspend(0);
        }
    }
    if (pG->flags_5010 & 0x04000000) {
        lightInfo.x50 = 4;
    } else {
        lightInfo.x50 = 2;
    }
}

void obj16_R1_Set(cObj16* obj)
{
    if (obj->pMotion) {
        MotionMove(obj, 0);
    }
    obj16MatCalc(obj);
}

void obj16_R1_CoreMove(cObj16* obj)
{
    Obj16Work* w = &obj->o16;
    int atk;
    f32 dist;

    w->atkEnable = 1;
    w->atkHit = 0;
    atk = 0;
    switch (obj->xFE) {
    case 0:
        if (w->active) {
            MotionSetCore(obj, &obj->pMotion, w->mot[2], 0, 3, 4, (u8) ((u32) Rnd() % 15));
        } else if (Rnd() & 1) {
            MotionSetCore(obj, &obj->pMotion, w->mot[0], 0, 3, 4, 0);
        } else {
            MotionSetCore(obj, &obj->pMotion, w->mot[1], 0, 3, 4, 0);
        }
        w->timer = (u8) ((u32) Rnd() % 5) + 15;
        obj->xFE++;
    case 1:
        if (MotionMove(obj, 0)) {
            if (obj->type == 4) {
                break;
            }
            if (obj->type == 3 || obj->type == 0xD) {
                obj->xFE = 0;
                break;
            }
            if (obj->type == 2 || obj->type == 0xB || obj->type == 0xE) {
                cModel* p = obj->getPartsPtr(0);
                dist = (p->worldPos.x - pPL->pos.x) * (p->worldPos.x - pPL->pos.x) +
                       (p->worldPos.y - pPL->pos.y) * (p->worldPos.y - pPL->pos.y) +
                       (p->worldPos.z - pPL->pos.z) * (p->worldPos.z - pPL->pos.z);
                if (w->active) {
                    if ((Rnd() & 3) == 0 && dist > 49000000.0f) {
                        obj->xFE = 2;
                        break;
                    }
                } else {
                    if ((Rnd() & 3) == 0 || dist < 25000000.0f) {
                        obj->xFE = 4;
                        break;
                    }
                }
            } else if ((Rnd() & 3) == 0) {
                obj->xFE = 0;
                break;
            }
        }
        if (obj->type == 2 && w->active) {
            atk = 1;
            if (w->timer) {
                w->timer--;
            } else {
                w->timer = (u8) ((u32) Rnd() % 5) + 15;
                SndCall(8, 0x10, &w->target->pos, w->target->id, 0, 0);
            }
        }
        if (obj->type == 0xB && w->active) {
            atk = 1;
            if (w->timer) {
                w->timer--;
            } else {
                w->timer = (u8) ((u32) Rnd() % 5) + 15;
                SndCall(8, 0x21, &w->target->pos, w->target->id, 0, 0);
            }
        }
        break;
    case 2:
        MotionSetCore(obj, &obj->pMotion, w->mot[10], 0, 3, 0, 0);
        if (obj->type == 2) {
            EstSet((int) obj, -1, 0, 0, 0x10, 0x53, 0, 0, (u32) obj, 0);
            EffectEspDelete(0, w->espKind2, (u32) obj, 0);
            EffectEspgenDelete(0, w->espKind2, (int) obj);
            EffectEfmDelete(0, w->espKind2, (int) obj);
        }
        if (obj->type == 0xB) {
            EstSet((int) obj, -1, 0, 0, 0x31, 0x19, 0, 0, (u32) obj, 0);
            EffectEspDelete(0, w->espKind2, (u32) obj, 0);
            EffectEspgenDelete(0, w->espKind2, (int) obj);
            EffectEfmDelete(0, w->espKind2, (int) obj);
        }
        w->timer = 36;
        obj->xFE++;
    case 3:
        if (w->timer) {
            if (--w->timer == 0) {
                w->active = 0;
            }
        }
        if (MotionMove(obj, 0)) {
            w->active = 0;
            obj->xFE = 0;
        }
        break;
    case 4:
        MotionSetCore(obj, &obj->pMotion, w->mot[9], 0, 3, 0, 0);
        if (obj->type == 2) {
            EstSet((int) obj, -1, 0, 0, 0x10, 0xA, 0, 0, (u32) obj, 0);
            EstSet((int) obj, -1, 0, 0, 0x10, 0x51, 0, w->espKind2, (u32) obj, 0);
            w->estTimer = 3;
        }
        if (obj->type == 0xB) {
            EstSet((int) obj, -1, 0, 0, 0x31, 0xC, 0, 0, (u32) obj, 0);
            EstSet((int) obj, -1, 0, 0, 0x31, 0x17, 0, w->espKind2, (u32) obj, 0);
            w->estTimer = 3;
        }
        w->timer = 20;
        obj->xFE++;
    case 5:
        if (w->timer) {
            if (--w->timer == 0) {
                w->active = 1;
            }
        }
        if (MotionMove(obj, 0)) {
            obj->xFE = 0;
        }
        break;
    }
    obj16MatCalc(obj);
    if (atk) {
        obj16AtkCk(obj, 2, 0x10);
        obj16AtkCk(obj, 2, 0x11);
        obj16AtkCk(obj, 2, 0x12);
        obj16AtkCk(obj, 2, 0x13);
        obj16AtkCk(obj, 2, 0x14);
        obj16AtkCk(obj, 2, 0x15);
    }
}

void obj16_R1_Atk(cObj16* obj)
{
    Obj16Work* w = &obj->o16;
    int atk;
    int flag;

    if (obj->xFE == 0 && w->active) {
        obj->xFE = 2;
    }
    w->atkHit = 0;
    atk = 0;
    switch (obj->xFE) {
    case 0:
        MotionSetCore(obj, &obj->pMotion, w->mot[9], 0, 0xA, 0, 0);
        if (obj->type == 2) {
            EstSet((int) obj, -1, 0, 0, 0x10, 0xA, 0, 0, (u32) obj, 0);
            EstSet((int) obj, -1, 0, 0, 0x10, 0x51, 0, w->espKind2, (u32) obj, 0);
            w->estTimer = 3;
        }
        if (obj->type == 0xB) {
            EstSet((int) obj, -1, 0, 0, 0x31, 0xC, 0, 0, (u32) obj, 0);
            EstSet((int) obj, -1, 0, 0, 0x31, 0x17, 0, w->espKind2, (u32) obj, 0);
            w->estTimer = 3;
        }
        obj->xFE++;
    case 1:
        if (MotionMove(obj, 0)) {
            w->active = 1;
            obj->xFE++;
        }
        break;
    case 2:
        if ((Rnd() & 1) || obj->xFF) {
            MotionSetCore(obj, &obj->pMotion, w->mot[3], 0, 0xA, 0, 0);
            w->timer = 34;
            w->atkTimer = 8;
            obj->xFF = 0;
            if (obj->type == 2) {
                EstSet((int) obj, -1, 0, 0, 0x10, 0x6C, 0, 0, (u32) obj, 0);
            }
            if (obj->type == 0xB) {
                EstSet((int) obj, -1, 0, 0, 0x31, 0x1C, 0, 0, (u32) obj, 0);
            }
        } else {
            MotionSetCore(obj, &obj->pMotion, w->mot[4], 0, 3, 0, 0);
            w->timer = 28;
            w->atkTimer = 6;
            obj->xFF = 1;
            if (obj->type == 2) {
                EstSet((int) obj, -1, 0, 0, 0x10, 0x6F, 0, 0, (u32) obj, 0);
            }
            if (obj->type == 0xB) {
                EstSet((int) obj, -1, 0, 0, 0x31, 0x1F, 0, 0, (u32) obj, 0);
            }
        }
        if (obj->type == 2) {
            SndCall(8, 9, &w->target->pos, w->target->id, 0, 0);
        }
        if (obj->type == 0xB) {
            SndCall(8, 0x1E, &w->target->pos, w->target->id, 0, 0);
        }
        obj->xFE++;
    case 3:
        if (MotionMove(obj, 0)) {
            obj->xFC = 1;
            obj->xFD = 1;
            obj->xFE = 0;
            obj->xFF = 0;
        } else {
            if (w->timer) {
                if (--w->timer == 0) {
                    if (obj->type == 2) {
                        SndCall(8, 0xA, &w->target->pos, w->target->id, 0, 0);
                    }
                    if (obj->type == 0xB) {
                        SndCall(8, 0x1F, &w->target->pos, w->target->id, 0, 0);
                    }
                }
            } else {
                if (w->atkTimer) {
                    w->atkTimer--;
                    atk = 1;
                }
            }
            if (obj->motFrame > 44.7f && obj->motFrame < 45.3f) {
                if (obj->type == 2) {
                    SndCall(8, 0xF, &w->target->pos, w->target->id, 0, 0);
                }
                if (obj->type == 0xB) {
                    SndCall(8, 0x20, &w->target->pos, w->target->id, 0, 0);
                }
            }
        }
        break;
    }
    obj16MatCalc(obj);
    if (atk) {
        flag = 0;
        if (obj->xFF) {
            flag = 1;
        }
        obj16AtkCk(obj, flag, 0x10);
        obj16AtkCk(obj, flag, 0x11);
        obj16AtkCk(obj, flag, 0x12);
        obj16AtkCk(obj, flag, 0x13);
        obj16AtkCk(obj, flag, 0x14);
        obj16AtkCk(obj, flag, 0x15);
    }
}

void obj16_R1_Critical(cObj16* obj)
{
    Obj16Work* w = &obj->o16;
    int atk;
    Vec head;
    Vec tgt;
    f32 d;
    cModel* p;

    w->atkHit = 0;
    atk = 0;
    switch (obj->xFE) {
    case 0:
        MotionSetCore(obj, &obj->pMotion, w->mot[9], 0, 3, 0, 0);
        if (obj->type == 3) {
            EstSet((int) obj, -1, 0, 0, 0x10, 0x5F, 0, 0, (u32) obj, 0);
            SndCall(8, 9, &w->target->pos, w->target->id, 0, 0);
        }
        if (obj->type == 0xD) {
            EstSet((int) obj, -1, 0, 0, 0x31, 0x1B, 0, 0, (u32) obj, 0);
            SndCall(8, 0x28, &w->target->pos, w->target->id, 0, 0);
        }
        w->timer = 0;
        obj->xFE++;
    case 1:
        if (MotionMove(obj, 0)) {
            obj->xFE++;
        }
        break;
    case 2:
        head.x = obj->mat[0][3];
        head.y = obj->mat[1][3];
        head.z = obj->mat[2][3];
        // one `p` for both parts lookups: a pointer local assigned in two blocks is not
        // local-allocated, so the worldPos copy's address register is not tied to r3
        p = pPL->getPartsPtr(3);
        tgt = p->worldPos;
        if (pSUB && w->body) {
            d = SQRTF((w->body->pos.x - pPL->pos.x) * (w->body->pos.x - pPL->pos.x) +
                      (w->body->pos.y - pPL->pos.y) * (w->body->pos.y - pPL->pos.y) +
                      (w->body->pos.z - pPL->pos.z) * (w->body->pos.z - pPL->pos.z));
            if (d > SQRTF((w->body->pos.x - pSUB->pos.x) * (w->body->pos.x - pSUB->pos.x) +
                          (w->body->pos.y - pSUB->pos.y) * (w->body->pos.y - pSUB->pos.y) +
                          (w->body->pos.z - pSUB->pos.z) * (w->body->pos.z - pSUB->pos.z)) +
                        3000.0f) {
                p = pSUB->getPartsPtr(3);
                tgt = p->worldPos;
            }
        }
        // `w->timer = 14` repeated in every arm: the last arm's block then does not end in a
        // call (no flow nop), so all four tails cross-jump into one MotionSetCore
        if (head.y - tgt.y > 500.0f) {
            MotionSetCore(obj, &obj->pMotion, w->mot[6], 0, 0, 0, 0);
            w->timer = 14;
        } else {
            f32 d2 = (head.x - tgt.x) * (head.x - tgt.x) + (head.z - tgt.z) * (head.z - tgt.z);
            if (d2 < 1440000.0f) {
                MotionSetCore(obj, &obj->pMotion, w->mot[3], 0, 0, 0, 0);
                w->timer = 14;
            } else if (d2 < 3240000.0f) {
                MotionSetCore(obj, &obj->pMotion, w->mot[4], 0, 0, 0, 0);
                w->timer = 14;
            } else {
                MotionSetCore(obj, &obj->pMotion, w->mot[5], 0, 0, 0, 0);
                w->timer = 14;
            }
        }
        if (obj->type == 3) {
            EstSet((int) obj, -1, 0, 0, 0x10, 0x5D, 0, 0, (u32) obj, 0);
            SndCall(8, 0xA, &w->target->pos, w->target->id, 0, 0);
        }
        if (obj->type == 0xD) {
            EstSet((int) obj, -1, 0, 0, 0x31, 0xB, 0, 0, (u32) obj, 0);
            SndCall(8, 0x29, &w->target->pos, w->target->id, 0, 0);
        }
        obj->xFE++;
    case 3:
        if (MotionMove(obj, 0)) {
            obj->xFC = 1;
            obj->xFD = 1;
            obj->xFE = 0;
            obj->xFF = 0;
        } else {
            if (w->timer) {
                if (--w->timer == 0) {
                    atk = 1;
                }
            }
            if (obj->motFrame > 9.7f && obj->motFrame < 10.3f) {
                if (obj->type == 3) {
                    SndCall(8, 0xAB, &w->target->pos, w->target->id, 0, 0);
                }
                if (obj->type == 0xD) {
                    SndCall(8, 0x2B, &w->target->pos, w->target->id, 0, 0);
                }
            }
            if (obj->motFrame > 41.7f && obj->motFrame < 42.3f) {
                if (obj->type == 3) {
                    SndCall(8, 0xF, &w->target->pos, w->target->id, 0, 0);
                }
                if (obj->type == 0xD) {
                    SndCall(8, 0x2A, &w->target->pos, w->target->id, 0, 0);
                }
            }
        }
        break;
    }
    obj16MatCalc(obj);
    if (atk) {
        if (obj16AtkCk(obj, 3, 9)) {
            if (obj->type == 3) {
                EstSet((int) obj, -1, 0, 0, 0x10, 0x5E, 0, 0, (u32) obj, 0);
            }
            if (obj->type == 0xD) {
                EstSet((int) obj, -1, 0, 0, 0x31, 0xE, 0, 0, (u32) obj, 0);
            }
        }
    }
}

void obj16_R1_Damage(cObj16* obj)
{
    Obj16Work* w = &obj->o16;

    switch (obj->xFE) {
    case 0:
        if (w->active) {
            MotionSetCore(obj, &obj->pMotion, w->mot[8], 0, 0, 0, 0);
        } else {
            MotionSetCore(obj, &obj->pMotion, w->mot[7], 0, 0, 0, 0);
        }
        if (obj->type == 3 || obj->type == 0xD) {
            w->scale.x = 0.3f;
            w->scale.y = 0.3f;
            w->scale.z = 0.3f;
        }
        if (w->target && (obj->type == 2 || obj->type == 3)) {
            SndCall(8, 0x88, &w->target->pos, w->target->id, 0, 0);
        }
        if (obj->type == 3) {
            if (w->body) {
                EstSet((int) w->body, -1, 0, 0, 0x10, 0x86, 0, 0, (u32) w->body, 0);
            }
        }
        w->timer = 30;
        w->atkTimer = 0;
        obj->xFE++;
    case 1:
        if (obj->type == 3 || obj->type == 0xD) {
            if (w->timer) {
                if (--w->timer == 0) {
                    w->scale.x = 1.0f;
                    w->scale.y = 1.0f;
                    w->scale.z = 1.0f;
                }
            }
        }
        if (obj->type == 4) {
            if (w->target) {
                if (w->atkTimer) {
                    w->atkTimer--;
                } else {
                    w->atkTimer = 29;
                    w->seHandle = SndCall(8, 0x13, &w->target->pos, w->target->id, 0, 0);
                }
            }
        }
        if (MotionMove(obj, 0)) {
            if (obj->type == 4) {
                if (w->target) {
                    SndStop(w->seHandle, 0);
                }
            }
            if (w->body && ((cEm*) w->body)->hp > 0 && (u8) ((u32) Rnd() % 10) > 5 && obj->type == 2) {
                obj->xFC = 1;
                obj->xFD = 2;
                obj->xFE = 0;
                obj->xFF = 0;
            } else {
                obj->xFC = 1;
                obj->xFD = 1;
                obj->xFE = 0;
                obj->xFF = 0;
            }
        }
        break;
    }
    obj16MatCalc(obj);
}

void MotSetObj16(cObj* obj, void* mot, int a, int b)
{
    if (obj == 0) {
        return;
    }
    MotionSetCore(obj, &obj->pMotion, mot, 0, 0xA, (u16) a, (u16) b);
}

void obj16MatCalc(cObj16* obj)
{
    Obj16Work* w = &obj->o16;
    cModel* p;

    if (w->body) {
        p = w->body->getPartsPtr(w->partsNo);
        RotMatrix(obj->mat, &obj->rot);
        TransMatrix(obj->mat, &obj->pos);
        ScaleMatrix(obj->mat, &obj->scale);
        PSMTXConcat(p->mat, obj->mat, obj->mat);
        obj->x21C |= 0x40000000;
    } else {
        RotMatrix(obj->worldMat, &obj->rot);
        TransMatrix(obj->worldMat, &obj->pos);
        ScaleMatrix(obj->worldMat, &obj->scale);
        PSMTXCopy(obj->worldMat, obj->mat);
    }
    if (obj->pMotion == 0) {
        obj->partsMatCalc();
    }
    obj16NeckMove(obj);
    obj->partsWorldCalc();
}

void cObj16::setScale(Vec* s)
{
    o16.scale = *s;
}

void cObj16::setDieEff()
{
    o16.dieEffTimer = 3;
}

void cObj16::clearLostWait()
{
    o16.lostWait = 0;
    o16.flags |= 1;
}

void cObj16::setLostWait(int n)
{
    o16.lostWait = n;
    o16.flags |= 1;
}

void cObj16::setMotData(void* m0, void* m1, void* m2, void* m3, void* m4, void* m5, void* m6, void* m7, void* m8,
                        void* m9, void* m10)
{
    Obj16Work* w = &o16;

    w->mot[0] = m0;
    w->mot[1] = m1;
    w->mot[2] = m2;
    w->mot[3] = m3;
    w->mot[4] = m4;
    w->mot[5] = m5;
    w->mot[6] = m6;
    w->mot[7] = m7;
    w->mot[8] = m8;
    w->mot[9] = m9;
    w->mot[10] = m10;
    if (type == 3 || type == 0xD) {
        MotionSetCore(this, &pMotion, m2, 0, 0, 4, 0);
    } else {
        MotionSetCore(this, &pMotion, m0, 0, 0, 4, 0);
    }
    w->x6C = 0;
    if (type == 3) {
        EstSet((int) this, -1, 0, 0, 0x10, 0x5C, 0, w->espKind, (u32) this, 0);
        EstSet((int) this, -1, 0, 0, 0x10, 0x70, 0, w->espKind2, (u32) this, 0);
    }
    if (type == 0xD) {
        EstSet((int) this, -1, 0, 0, 0x31, 0x1A, 0, w->espKind, (u32) this, 0);
        EstSet((int) this, -1, 0, 0, 0x31, 0xF, 0, w->espKind2, (u32) this, 0);
    }
    xFC = 1;
    xFD = 1;
    xFE = 1;
    xFF = 0;
}

void cObj16::setPlDmgMot(void* mot, int a)
{
    Obj16Work* w = &o16;

    w->plMot = mot;
    w->plMotA = a;
}

void cObj16::setAtk(u8 flag)
{
    xFC = 1;
    xFD = 2;
    o16.atkHit = 0;
    xFE = 0;
    xFF = flag;
}

void cObj16::setCritical()
{
    xFC = 1;
    xFD = 3;
    o16.atkHit = 0;
    xFE = 0;
    xFF = 0;
}

void cObj16::setDamage()
{
    xFC = 1;
    xFD = 4;
    xFE = 0;
    xFF = 0;
}

int cObj16::ckAtkEnable()
{
    if (o16.atkEnable == 0) {
        return 0;
    }
    return 1;
}

// The player is dead (the upper half of the damage word).
static inline int PlIsDead()
{
    return (pPL->flags_324 & 0xFFFF0000) ? 1 : 0;
}

int obj16AtkCk(cObj16* obj, u32 kind, int partsNo)
{
    Obj16Work* w = &obj->o16;
    cModel* body = w->body;
    cModel* p;
    Vec* pp;
    Vec plPos;
    EmAtkInfo info;
    Vec pos;
    int hit;
    f32 ang;

    if (body == 0) {
        return 0;
    }
    if (((cEm*) body)->hp <= 0) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (PlIsDead()) {
        return 0;
    }
    if (w->atkWait != 0) {
        if (kind == 2) {
            return 0;
        }
    }
    p = obj->getPartsPtr(partsNo);
    pp = &p->worldPos;
    pos = *pp;
    if (kind == 3) {
        pos.y -= 500.0f;
    }
    plPos = pPL->pos;
    plPos.y += 1600.0f;
    if (pos.y > body->pos.y + 2500.0f) {
        return 0;
    }
    info = obj16_atk_info[kind];
    hit = EmAtkHitCk(&info, pp, pp, 0);
    if (hit & 1) {
        switch (kind) {
        default:
        case 0:
            if (obj->type == 2) {
                EmPlBloodSet2(obj, pp, 1, 0x10, 0x6D);
            }
            Ctrl12Set(w->ctrl12, 9, 0x1E);
            if (obj->type == 2 && w->target) {
                SndCall(8, 0x3E, &w->target->pos, w->target->id, 0, 0);
            }
            if (obj->type == 0xB && w->target) {
                SndCall(8, 0x12, &w->target->pos, w->target->id, 0, 0);
            }
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            QuakeExec(0, 0, 5, 22.0f, 2);
            if (w->plMot && (s16) pG->pl_life > 0 && w->body) {
                if (obj->type == 2) {
                    EstSet((int) pPL, -1, 0, 0, 0x10, 0x74, 0, 0, (u32) pPL, 0);
                }
                if (obj->type == 0xB) {
                    EstSet((int) pPL, -1, 0, 0, 0x31, 9, 0, 0, (u32) pPL, 0);
                }
                SetPlDamage((int) obj, plemDmMStar);
                if (fabsf(Muku(&pPL->pos, &w->body->pos, pPL->rot.y, PI)) < PI / 2) {
                    ang = Muku(&pPL->pos, &w->body->pos, pPL->rot.y, PI);
                    FSet(pPL->rot.y, pPL->rot.y + ang);
                    pPL->xFF = 0;
                } else {
                    ang = Muku(&w->body->pos, &pPL->pos, pPL->rot.y, PI);
                    FSet(pPL->rot.y, pPL->rot.y + ang);
                    pPL->xFF = 1;
                }
            } else {
                if (obj->type == 2) {
                    EstSet((int) obj, -1, 0, 0, 0x10, 0x73, 0, 0, (u32) obj, 0);
                }
                if (obj->type == 0xB) {
                    EstSet((int) obj, -1, 0, 0, 0x31, 8, 0, 0, (u32) obj, 0);
                }
            }
            break;
        case 1:
            if (obj->type == 2) {
                EmPlBloodSet2(obj, pp, 1, 0x10, 0x6E);
            }
            Ctrl12Set(w->ctrl12, 9, 0x1E);
            if (obj->type == 2 && w->target) {
                SndCall(8, 0x3E, &w->target->pos, w->target->id, 0, 0);
            }
            if (obj->type == 0xB && w->target) {
                SndCall(8, 0x12, &w->target->pos, w->target->id, 0, 0);
            }
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            QuakeExec(0, 0, 5, 22.0f, 2);
            if (obj->type == 2) {
                EstSet((int) obj, -1, 0, 0, 0x10, 0x73, 0, 0, (u32) obj, 0);
            }
            break;
        case 2:
            w->atkWait = 90;
            if (obj->type == 2) {
                EmPlBloodSet2(obj, pp, 1, 0x10, 0x72);
            }
            if (obj->type == 0xB) {
                EmPlBloodSet2(obj, pp, 1, 0x31, 7);
            }
            Ctrl12Set(w->ctrl12, 9, 0x1E);
            if (obj->type == 2 && w->target) {
                SndCall(8, 0x3E, &w->target->pos, w->target->id, 0, 0);
            }
            if (obj->type == 0xB && w->target) {
                SndCall(8, 0x12, &w->target->pos, w->target->id, 0, 0);
            }
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            QuakeExec(0, 0, 5, 22.0f, 2);
            if (obj->type == 2) {
                EstSet((int) obj, -1, 0, 0, 0x10, 0x73, 0, 0, (u32) obj, 0);
            }
            break;
        case 3:
            obj16PlHeadLost(obj);
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            QuakeExec(0, 0, 5, 22.0f, 2);
            break;
        }
        w->atkHit = 1;
    }
    if (hit & 2) {
        if (kind != 3) {
            Ctrl12Set(w->ctrl12, 9, 0x1E);
        } else {
            if (pSUB->id & 3) {
                LifeDownSet(pSUB, 9999, 0);
            }
        }
    }
    return 1;
}

// The head bit the player's head off: game over.
void obj16PlHeadLost(cObj16* obj)
{
    Obj16Work* w = &obj->o16;
    u8 region;

    pG->pl_life = 0;
    PlSetDamage(6, 0, 0);
    region = pSys->region;
    if (region == 0) {
        PlSetDamageSe(0xD);
        if (w->body) {
            switch (w->body->id) {
            case 0x10 ... 0x17:
            case 0x19 ... 0x20:
                EstSet((int) pPL, -1, 0, 0, 0x10, 0x57, 0, 0, (u32) pPL, 0);
                break;
            case 0x3C:
                EstSet((int) pPL, -1, 0, 0, 0x31, 0x28, 0, 0, (u32) pPL, 0);
                break;
            }
        }
    } else {
        pPL->setHead(0);
        if (w->body) {
            switch (w->body->id) {
            case 0x10 ... 0x17:
            case 0x19 ... 0x20:
                EstSet((int) pPL, -1, 0, 0, 0x10, 0x45, 0, 0, (u32) pPL, 0);
                break;
            case 0x3C:
                EstSet((int) pPL, -1, 0, 0, 0x31, 0x27, 0, 0, (u32) pPL, 0);
                break;
            }
        }
        SndCall(1, 0x3E, &pPL->pos, 0, 0, 0);
    }
}

// Turn the neck parts (1, 2) toward the player and tilt the head (parts 0) along the body parts.
static void obj16NeckMove(cObj16* obj)
{
    Obj16Work* w = &obj->o16;
    cModel* body = w->body;
    Vec tgt;
    Vec dir;
    f32 ang;
    Obj16Parts* p;

    if (body == 0) {
        return;
    }
    switch (obj->type) {
    case 2:
    case 3:
    case 0xB:
    case 0xD:
    case 0xE:
        break;
    default:
        return;
    }
    tgt = pPL->pos;
    if (pSUB) {
        f32 d = SQRTF((body->pos.x - pPL->pos.x) * (body->pos.x - pPL->pos.x) +
                      (body->pos.y - pPL->pos.y) * (body->pos.y - pPL->pos.y) +
                      (body->pos.z - pPL->pos.z) * (body->pos.z - pPL->pos.z));
        if (d > SQRTF((body->pos.x - pSUB->pos.x) * (body->pos.x - pSUB->pos.x) +
                      (body->pos.y - pSUB->pos.y) * (body->pos.y - pSUB->pos.y) +
                      (body->pos.z - pSUB->pos.z) * (body->pos.z - pSUB->pos.z)) +
                    3000.0f) {
            tgt = pSUB->pos;
        }
    }
    tgt.y += 1600.0f;
    w->neckAng = w->neckAng * 0.9f + Muku(&body->pos, &tgt, body->rot.y, PI / 2) * 0.1f;
    switch (obj->type) {
    case 2:
    case 0xE:
        ang = w->neckAng * 0.5f;
        p = (Obj16Parts*) obj->getPartsPtr(1);
        p->rot.x = 0.0f;
        p->rot.y = ang;
        p->flags |= 0x40000000;
        p->rot.z = 0.0f;
        p = (Obj16Parts*) obj->getPartsPtr(2);
        p->rot.x = 0.0f;
        p->rot.y = ang;
        p->flags |= 0x40000000;
        p->rot.z = 0.0f;
        if (w->body) {
            cModel* bp = w->body->getPartsPtr(w->partsNo);
            dir.x = 0.0f;
            dir.y = 0.0f;
            dir.z = 1.0f;
            PSMTXMultVecSR(bp->mat, &dir, &dir);
            if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f) {
                dir.z = 1.0f;
            }
#line 1850 "D:/Bio4/Prog/obj16.cpp"
            VECNormalize(&dir, &dir);
            ang = asinf(dir.y);
            p = (Obj16Parts*) obj->getPartsPtr(0);
            p->rot.x = ang;
            p->rot.y = 0.0f;
            p->flags |= 0x40000000;
            p->rot.z = 0.0f;
        }
        break;
    case 3:
    case 0xB:
    case 0xD:
        ang = w->neckAng * 0.5f;
        p = (Obj16Parts*) obj->getPartsPtr(1);
        p->rot.x = 0.0f;
        p->rot.y = ang;
        p->flags |= 0x40000000;
        p->rot.z = 0.0f;
        p = (Obj16Parts*) obj->getPartsPtr(2);
        p->rot.x = 0.0f;
        p->rot.y = ang;
        p->flags |= 0x40000000;
        p->rot.z = 0.0f;
        break;
    }
}

void cObj16::setBurn()
{
    cModelInfo* info;

    for (info = pInfo; info; info = info->pNext) {
        info->color[0] = 0x20;
        info->color[1] = 0x20;
        info->color[2] = 0x20;
    }
}

int cObj16::ckAtkHit()
{
    return o16.atkHit ? 1 : 0;
}

// Player damage routine while the head holds him (SetPlDamage callback).
void plemDmMStar(cPlayer* pl)
{
    Obj16Work* w = &((cObj*) pPL->dmgType)->o16;
    int hokan;

    if (pl->xFF == 0) {
        pl->dmg.set(0, 2);
    }
    switch (pl->xFE) {
    case 0:
        if (pl->xFF) {
            hokan = 0x41;
        } else {
            hokan = 1;
        }
        MotionSetCore(pl, &pl->pMotion, w->plMot, w->plMotA, 3, hokan, 0);
        PlSetDamageSe(0);
        if (pl->xFF) {
            pl->dmg.set(0, 0xF);
        }
        pl->xFE++;
    case 1:
        if (MotionMove(pl, 0)) {
            EndPlDamage();
            if (pl->xFF == 0) {
                pl->dmg.set(0, 0xF);
            }
        }
        break;
    }
}
