#include "atari.h"
#include "light.h"
#include "global.h"
#include "main_mem.h"
#include "eprintf.h"
#include "math_sub.h"
#include "esp.h"
#include "espgen.h"

// number of live effects per owner id (debug display)
u16 esp_num_list[0xD3];
EspTransFunc EspTransTbl[0xFF];
EspCreateFunc EspCreateTbl[0xFF];

extern "C" {
void EspDummyTrans(cEsp* esp);
void EspFuncTblInit();
int ESP_IsActive(cEsp* esp);
int EspMove();
f32 EspGetCameraPan();
f32 EspGetCameraPan2();
int EspTrans();
int EspDispInfo();
int EspArrayAlloc(u32 n);
int EspArrayFree();
int EspArrayPush(u32 n);
int EspArrayPop();
}

void EspDummyTrans(cEsp* esp)
{
    pLog->err(0, 0, "ESP_TRANS : ESP_ID[%x] invalid.", esp->id);
    PushEsp(esp);
}

void EspFuncTblInit()
{
    int i;

    for (i = 0; i < 0xFF; i++) {
        EspTransTbl[i] = EspDummyTrans;
        EspCreateTbl[i] = NULL;
    }
}

void EspFuncTblSet(int id, EspCreateFunc create, EspTransFunc trans)
{
    EspCreateTbl[id] = create;
    EspTransTbl[id] = trans;
}

int ESP_IsActive(cEsp* esp)
{
    if (!(esp->flag & 1)) {
        return 0;
    }
    if (pG->flags_5010 & 0x10000000) {
        if (!(esp->info.x0 & 1)) {
            return 0;
        }
        if (esp->parent != pEffParentWorld) {
            cModel* m = esp->pModel;
            if (m != NULL) {
                int off = !(m->be_flag & 0x800);
                if (off) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

int PullEsp(cEsp** out, int id)
{
    cEspSystem* sys = g_pEspSys;
    EspCreateFunc create = EspCreateTbl[id];
    cEsp* esp;
    int ret = 0;

    if (create == NULL) {
        pLog->err(0, 0, "ESP : EspID[%x] is invalid.", id);
        *out = sys->pDmy;
        return 0;
    }
    esp = create();
    *out = esp;
    if (esp != sys->pDmy) {
        esp->flag |= 1;
        ret = 1;
        sys->xC548++;
        (*out)->id = id;
    } else {
        pLog->warn(6, 0, "ESP : ESP work full!!");
    }
    return ret;
}

// The target's third loop has `mr r9,r10` (a copy of the sys+0x10000 base) before the loop and in
// its latch, with the pEspBuf load reading r9. That is cse_around_loop (cse.c): it only runs on a
// loop with LOOP_BEG/END notes whose latch jumps straight back to the header, and it rewrites the
// header's `sys+0x10000` into the latch's REG_LOOP_TEST_P copy. So loop3 is a real `for` loop;
// loop.c (find_and_verify_loops) would then move the `PushEsp; goto found` block behind the found:
// block (a guarded block ending in a jump out of the loop) and, with no call left in the loop,
// hoist the pEspBuf load. The `do { PushEsp(esp); goto found; } while (0)` wrapper stops that: the
// backward scan from the `goto` stops at the inner NOTE_INSN_LOOP_BEG instead of the guard jump.
// Loops 1/2/4 as for/while loops get strength-reduced `&esp->flag` givs the target lacks, so they
// stay goto loops.
void* cEsp::operator new(unsigned int size)
{
    static u32 old_hit = 0;
    cEspSystem* sys = g_pEspSys;
    u32 i;
    cEsp* esp;
    cEsp* ret;
    u32 start;
    u32 ofs;

    if (old_hit >= sys->xC554) {
        BitSet(old_hit, 0);
    }
    i = old_hit;
    start = i;
    ret = sys->pDmy;
    if (i < sys->xC554) {
        ofs = i * 0x150;
loop1:
        esp = (cEsp*) (sys->pEspBuf + ofs);
        if (!(esp->flag & 1)) {
            goto found;
        }
        i++;
        ofs += 0x150;
        if (i < sys->xC554) {
            goto loop1;
        }
    }
    i = 0;
    if (i < start) {
        ofs = 0;
loop2:
        esp = (cEsp*) (sys->pEspBuf + ofs);
        if (!(esp->flag & 1)) {
            goto found;
        }
        i++;
        ofs += 0x150;
        if (i < start) {
            goto loop2;
        }
    }
    if (ret == sys->pDmy) {
        for (i = start; i < sys->xC554; i++) {
            esp = (cEsp*) (sys->pEspBuf + i * 0x150);
            if ((esp->flag & 1) && (esp->flags & 0x40000)) {
                do { // keeps loop.c from moving this block out of the loop (see above)
                    PushEsp(esp);
                    goto found;
                } while (0);
            }
        }
        i = 0;
        if (i < start) {
            ofs = 0;
loop4:
            esp = (cEsp*) (sys->pEspBuf + ofs);
            if ((esp->flag & 1) && (esp->flags & 0x40000)) {
found:
                memclr_asm(esp, 0x150);
                old_hit = i + 1;
                return esp;
            }
            i++;
            ofs += 0x150;
            if (i < start) {
                goto loop4;
            }
        }
    }
    return ret;
}

u32 tubo_amb = 0;

void PushEsp(cEsp* esp)
{
    if (esp->flag & 1) {
        esp->flag &= ~3;
        g_pEspSys->xC548--;
        esp->Destruct();
    } else {
        pLog->warn(0, 0, "PushEsp() : No alive work is pushed.");
    }
}

int EspMove()
{
    cEspSystem* sys = g_pEspSys;
    cEsp* esp;
    u32 cnt;
    u32 i;
    int pause;
    int color;
    int y;

    for (i = 0; i < 0xD3; i++) {
        esp_num_list[i] = 0;
    }
    pause = 0;
    if (pG->flags_5010 & 2) {
        pause = 1;
    }
    cnt = 0;
    for (i = 0; i < sys->xC554; i++) {
        esp = (cEsp*) (sys->pEspBuf + i * 0x150);
        if (!ESP_IsActive(esp)) {
            continue;
        }
        if (esp->parent != pEffParentWorld) {
            cModel* m = esp->pModel;
            if (m != NULL) {
                if ((m->be_flag & 0x201) != 1 || m->serial != esp->x20) {
                    PushEsp(esp);
                    continue;
                }
            }
        }
        if (pause) {
            if (!(esp->info.x0 & 0x8000)) {
                continue;
            }
        }
        esp->move();
        if (esp->flag & 1) {
            cnt++;
            if (pG->debug_mode == 0xE) {
                esp_num_list[esp->info.x3]++;
            }
        }
    }
    color = 0;
    if ((f32) cnt > (f32) sys->xC554 * 0.7f) {
        color = 0x16;
    }
    if ((f32) cnt > (f32) sys->xC554 * 0.9f) {
        color = 2;
    }
    if (pG->flags_6C & 0x8000) {
        eprintf(0x1B0, 0xC8, color, 0, "%d/%d", sys->xC548, cnt);
    } else {
        eprintf(0x1D8, 0xC8, color, 0xE, "%d", cnt);
    }
    if ((s32) pG->flags_60 >= 0 && pG->debug_mode == 0xE) {
        eprintf(0x20, 0x60, 0, 0xE, "TOTAL:%d", cnt);
        // y counts printed rows; `0x70 + y * 0x10` is a strength-reduced giv whose `li 0x70` init
        // is emitted by loop.c after the hoisted `lis`/`addi`s (a plain `y = 0x70; y += 0x10`
        // schedules the li before the call).
        y = 0;
        for (i = 0; i < 0xD3; i++) {
            if (esp_num_list[i] != 0) {
                eprintf(0x20, 0x70 + y * 0x10, 0, 0xE, "%s:%d", owner_name_tbl[i], esp_num_list[i]);
                y++;
            }
        }
    }
    return 1;
}

f32 EspGetCameraPan()
{
    return g_pEspSys->camPan;
}

f32 EspGetCameraPan2()
{
    return g_pEspSys->camPan2;
}

int EspTrans()
{
    cEspSystem* sys = g_pEspSys;
    Vec dir;
    Vec wpos;
    Vec* wp;
    cEsp* esp;
    EspTransFunc trans;
    Camera* cam;
    u32 i;
    u16 prio;
    int ot;
    f32 zlimit;

    if (sys == NULL) {
        return 0;
    }
    LightMgr.setEsp(&sys->lightList, 8);
    cam = &pG->Cam;
    dir.x = cam->param.at.x - cam->param.pos.x;
    dir.y = cam->param.at.y - cam->param.pos.y;
    dir.z = cam->param.at.z - cam->param.pos.z;
    if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f) {
        dir.y = 1.0f;
    }
#line 584 "D:/Bio4/Prog/esp.cpp"
    VECNormalize(&dir, &dir);
    if (dir.x == 0.0f && dir.z == 0.0f) {
        sys->camPan = 0.0f;
    } else {
        sys->camPan = atan2f(dir.x, dir.z) * 57.295776f;
    }
    if (dir.y == 0.0f) {
        sys->camPan2 = 0.0f;
    } else {
        sys->camPan2 = -atan2f(dir.y, SQRTF(dir.x * dir.x + dir.z * dir.z)) * 57.295776f;
    }
    for (i = 0; i < sys->xC554; i++) {
        esp = (cEsp*) (sys->pEspBuf + i * 0x150);
        if (!ESP_IsActive(esp)) {
            continue;
        }
        if (esp->pModel != NULL && !(esp->pModel->be_flag & 2) && esp->parentCnt == 0xFF) {
            continue;
        }
        trans = EspTransTbl[esp->id];
        if (trans == NULL) {
            continue;
        }
        if (esp->info.x0 & 0x400) {
            if (pG->flags_5010 & 0x04000000) {
                continue;
            }
        }
        if (pG->flags_500C & 0x8000) {
            if (esp->flags & 0x100) {
                continue;
            }
        } else {
            if (esp->flags & 0x200) {
                continue;
            }
        }
        prio = 0x10;
        if (trans == EspCommonTrans && esp->pad_EC[0] == 0 && !(esp->flags & 0x6000)) {
            prio = 8;
        }
        if (pG->flags_5010 & 2) {
            if (!(esp->info.x0 & 0x8000)) {
                continue;
            }
            AddOtDirect(0x14, esp, (void (*)()) trans, 0, prio, NULL, 0.0f);
            continue;
        }
        if (esp->flags & 0x10000) {
            BitOn(pG->flags_5010, 0x08000000);
            ot = 0;
            if (!(esp->info.x0 & 8)) {
                if (esp->info.x0 & 0x10) {
                    ot = 1;
                } else if (esp->info.x0 & 0x20) {
                    ot = 2;
                } else if (esp->info.x0 & 0x40) {
                    ot = 3;
                } else if (esp->info.x0 & 0x80) {
                    ot = 4;
                } else if (esp->info.x0 & 0x100) {
                    ot = 5;
                } else if (esp->info.x0 & 0x200) {
                    ot = 6;
                } else if ((s32) pG->flags_60 >= 0) {
                    pLog->err(6, 0, "ESP : FLG_TEX_RENDER but no set tex_no");
                }
            }
            if ((u8) (esp->partsNo + 8) <= 5) {
                switch (esp->partsNo) {
                case 0xFD:
                    AddOtDirect(ot, esp, (void (*)()) trans, 3, prio, NULL, 0.0f);
                    break;
                case 0xFC:
                    AddOtDirect(ot, esp, (void (*)()) trans, 2, prio, NULL, 0.0f);
                    break;
                case 0xFB:
                    AddOtDirect(ot, esp, (void (*)()) trans, 1, prio, NULL, 0.0f);
                    break;
                case 0xFA:
                    AddOtDirect(ot, esp, (void (*)()) trans, 5, prio, NULL, 0.0f);
                    break;
                case 0xF9:
                    AddOtDirect(ot, esp, (void (*)()) trans, 4, prio, NULL, 0.0f);
                    break;
                case 0xF8:
                    AddOtDirect(ot, esp, (void (*)()) trans, 6, prio, NULL, 0.0f);
                    break;
                default:
                    pLog->err(0, 0, "ESP : FLG_TEX_RENDER but no screen");
                    break;
                }
            } else {
                pLog->err(0, 0, "ESP : FLG_TEX_RENDER but no screen");
            }
            continue;
        }
        if (esp->flags & 0x1000) {
            if ((u8) (esp->partsNo + 8) <= 5) {
                AddOtDirect(0x12, esp, (void (*)()) trans, 8, prio, NULL, 0.0f);
            } else {
                AddOtDirect(0x12, esp, (void (*)()) trans, 9, prio, NULL, 0.0f);
            }
            continue;
        }
        if (esp->dispFlag & 8) {
            AddOtDirect(0x12, esp, (void (*)()) trans, 6, prio, NULL, 0.0f);
            continue;
        }
        if (esp->flags & 0x400000) {
            if ((esp->flags & 0xC00) == 0xC00) {
                AddOtDirect(0xB, esp, (void (*)()) trans, 3, prio, NULL, 0.0f);
            } else {
                AddOtDirect(0x10, esp, (void (*)()) trans, 4, prio, NULL, 0.0f);
            }
            continue;
        }
        if (esp->flags & 0x400) {
            if (esp->flags & 0x800) {
                AddOtDirect(0x10, esp, (void (*)()) trans, 3, prio, NULL, 0.0f);
            } else {
                AddOtDirect(0x10, esp, (void (*)()) trans, 2, prio, NULL, 0.0f);
            }
            continue;
        }
        if (esp->flags & 0x800) {
            AddOtDirect(0x10, esp, (void (*)()) trans, 0, prio, NULL, 0.0f);
            continue;
        }
        if ((u8) (esp->partsNo + 8) <= 5) {
            switch (esp->partsNo) {
            case 0xFD:
                AddOtDirect(0x12, esp, (void (*)()) trans, 3, prio, NULL, 0.0f);
                break;
            case 0xFC:
                AddOtDirect(0x12, esp, (void (*)()) trans, 2, prio, NULL, 0.0f);
                break;
            case 0xFB:
                AddOtDirect(0x12, esp, (void (*)()) trans, 1, prio, NULL, 0.0f);
                break;
            case 0xFA:
                AddOtDirect(0x12, esp, (void (*)()) trans, 5, prio, NULL, 0.0f);
                break;
            case 0xF9:
                AddOtDirect(0x12, esp, (void (*)()) trans, 4, prio, NULL, 0.0f);
                break;
            case 0xF8:
                AddOtDirect(0x12, esp, (void (*)()) trans, 6, prio, NULL, 0.0f);
                break;
            }
            continue;
        }
        if (esp->parent != pEffParentWorld) {
            PSMTXMultVec(esp->parent->mat, &esp->pos, &wpos);
        } else {
            wp = &wpos;
            *wp = esp->pos;
        }
        wp = &wpos;
        zlimit = 0.0f;
        if (esp->xB8 == zlimit) {
            AddOtWorldPos(esp, (void (*)(void*)) trans, wp, prio, 200.0f);
        } else {
            if ((esp->dispFlag & 2) == 0 && (esp->flags & 1) == 0) {
                zlimit = 200.0f;
            }
            AddOtWorldPosRadius(esp, (void (*)(void*)) trans, wp, esp->xB8, prio, zlimit);
        }
    }
    return 1;
}

int EspDispInfo()
{
    static u32 max = 0;
    cEspSystem* sys = g_pEspSys;
    u8* p = sys->pEspBuf;
    u32 cnt;
    u32 i;

    if (p == NULL) {
        return 0;
    }
    cnt = 0;
    for (i = 0; i < sys->xC554; i++) {
        if (((cEsp*) (p + i * 0x150))->flag & 1) {
            cnt++;
        }
    }
    eprintf(0x1A0, 0x38, 0, 0xC, "%3d/%3d/%4d", cnt, max, sys->xC554);
    if (cnt > max) {
        max = cnt;
    }
    return 1;
}

u32 esp_dmy_amb = 0;

int EspArrayAlloc(u32 n)
{
    cEspSystem* sys = g_pEspSys;
    u32 size;
    u8* p;

    EspArrayFree();
    if (n == 0) {
        return 0;
    }
    size = n * 0x150;
#line 879 "D:/Bio4/Prog/esp.cpp"
    p = (u8*) MEM_ALLOC(size, 1, 0xD);
    sys->pEspBuf = p;
    if (p == NULL) {
        return 0;
    }
    sys->xC554 = n;
    memclr_asm(p, size);
    return 1;
}

int EspArrayFree()
{
    cEspSystem* sys = g_pEspSys;

    if (sys->pEspBuf == NULL) {
        return 0;
    }
    Mem_free(sys->pEspBuf);
    sys->pEspBuf = NULL;
    return 1;
}

int EspArrayPush(u32 n)
{
    cEspSystem* sys = g_pEspSys;

    if (sys->pEspBufSave != NULL) {
        return 0;
    }
    sys->pEspBufSave = sys->pEspBuf;
    sys->pEspBuf = (u8*) Debug_alloc(n * 0x150, 1);
    sys->numSave = sys->xC554;
    sys->xC554 = n;
    return 1;
}

int EspArrayPop()
{
    cEspSystem* sys = g_pEspSys;

    if (sys->pEspBufSave == NULL) {
        return 0;
    }
    Debug_free(sys->pEspBuf);
    sys->pEspBuf = sys->pEspBufSave;
    sys->pEspBufSave = NULL;
    sys->xC554 = sys->numSave;
    return 1;
}

void EspArrayClear()
{
    cEspSystem* sys = g_pEspSys;
    cEsp* esp;
    u32 i;

    for (i = 0; i < sys->xC554; i++) {
        esp = (cEsp*) (sys->pEspBuf + i * 0x150);
        if (esp->flag & 1) {
            PushEsp(esp);
        }
    }
}

cEsp* EspGetDmyPtr()
{
    return g_pEspSys->pDmy;
}

void EspAddOtAfterRender(cEsp* esp, void (*func)(cEsp*))
{
    if (EspGenGetMoveLoop() == 0) {
        AddOtDirect(0x16, esp, (void (*)()) func, 0, 0x20, NULL, 0.0f);
    }
}
