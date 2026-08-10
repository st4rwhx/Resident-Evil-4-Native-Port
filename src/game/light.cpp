#include "light.h"
#include "ctrl.h"
#include "atari.h"
#include "global.h"
#include "model.h"
#include "em.h"
#include "obj.h"
#include "scroll.h"
#include "etc_model.h"
#include "view.h"
#include "filter.h"
#include "math_sub.h"
#include "cam_ctrl.h"
#include "pendulum.h"

// game/trans.cpp texture LOD / TEV scale settings
extern u8 gxCsScale[];
extern u8 min_lod;
extern u8 max_lod;
extern f32 lod_bias;
extern u32 aniso;

// pointer to game memory (0x80000000 .. 0x82FFFFFF)
#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)
#define IN_RANGE(p) ((u32) (p) - 0x80000000 <= 0x02FFFFFF)
#define IS_ALIVE(p) (((p)->be_flag & 0x201) == 1)

void funcDelCtrl(cCtrl* c);

// Light control work (cCtrl::work): the electric power path
struct LightCtrlWork {
    cLightPathData* pPath;   // 0x00
    cLightPathData* pPath2;  // 0x04
    u8 idx;                  // 0x08
};

static LightFog fogNew;

// value unknown: the linker dropped the object, only the `_GLOBAL_.I.FarDistance__9cLightMgr` name survives
const f32 cLightMgr::FarDistance = 100000.0f;

cLightMgr::cLightMgr() : cManager<cLight>(sizeof(cLight), 0)
{
    setName("cLightMgr");
}

void cLightMgr::log(const char* fmt, ...)
{
    va_list ap;

    if (logOn) {
        va_start(ap, fmt);
        pLog->vwarn(0, 0, fmt, ap);
    }
}

void cLightMgr::init(void (**funcTbl)(cLight*))
{
    cManager<cLight>::init(funcTbl);
    pLit = 0;
    memclr_asm(&env, sizeof(env));
    elecPower = 1.0f;
    colBrendRate = 0.0f;
    pArray = 0;
    nArray = 0;
    pPath = 0;
    memset_asm(kindFlags, 0xFF, sizeof(kindFlags));
    x204 = 0;
    x1AC = 0;
    x1B0 = 0;
}

int cLightMgr::roomInit(cLit* core, cLit* room, cLit* third)
{
    cManager<cLight>::roomInit();
    if (!IN_RANGE(core)) {
        pLog->err(0, 0, "cLightMgr::roomInit() CORE INVALID POINTER %08x", core);
        return 0;
    }
    if (!IN_RANGE(room)) {
        pLog->err(0, 0, "cLightMgr::roomInit() ROOM INVALID POINTER %08x", room);
        return 0;
    }
    if (!IN_RANGE(third)) {
        pLog->err(0, 0, "cLightMgr::roomInit() ROOM INVALID POINTER %08x", third);
        return 0;
    }
    x180 = core;
    core->versionUp();
    x184 = room;
    room->versionUp();
    x188 = third;
    pLit = room;
    elecPower = 1.0f;
    colBrendRate = 0.0f;
    pArray = 0;
    nArray = 0;
    tune[0].r = 0xC8;
    tune[0].g = 0xC0;
    tune[0].b = 0xF0;
    tune[0].a = 0x80;
    tune[1].r = 0;
    tune[1].g = 0;
    tune[1].b = 0;
    tune[1].a = 0x80;
    tune[2].r = 0xC8;
    tune[2].g = 0xC0;
    tune[2].b = 0xF0;
    tune[2].a = 0x80;
    x204 = 0;
    x1AC = 0;
    x1B0 = 0;
    memset_asm(kindFlags, 0xFF, sizeof(kindFlags));
    return 1;
}

int cLightMgr::construct(cLight* p, u32 id)
{
    switch (id) {
    default:
        new (p) cLight();
        break;
    case 1:
        new (p) cLight01();
        break;
    case 2:
        new (p) cLight02();
        break;
    case 6:
        new (p) cLight06();
        break;
    case 7:
        new (p) cLight07();
        break;
    case 8:
        new (p) cLight08();
        break;
    }
    return 1;
}

cLight* cLightMgr::create(cLightWork* w)
{
    cLight* l = cManager<cLight>::create(w->type);
    if (l == 0) {
        return 0;
    }
    if (!IS_ALIVE(l)) {
        return 0;
    }
    *l = *w;
    l->move();
    return l;
}

cLight* cLightMgr::createBack(cLightWork* w)
{
    cLight* l = cManager<cLight>::createBack(w->type);
    if (l == 0) {
        return 0;
    }
    *l = *w;
    l->move();
    return l;
}

cLight* cLightMgr::create(cLit* lit, int cutNo, int lightNo, int flag)
{
    cLightEnv* cut;
    cLightWork* w;
    cLight* l;

    if (!IN_RANGE(lit)) {
        pLog->err(0, 0, "Light create failed PTR ERR 0x%08x", lit);
        return 0;
    }
    cut = lit->getCut(cutNo);
    if (cut == 0) {
        return 0;
    }
    if (lightNo >= 0) {
        if ((u32) lightNo >= cut->nLight) {
            return 0;
        }
        w = cut->getLightWork(lightNo);
        if (w == 0) {
            return 0;
        }
        l = create(w);
        if (l == 0) {
            if (0) {
                log("cLightMgr::create() WORK ALLOC FAILED", 0);
            }
            return 0;
        }
        l->be_flag = flag | 3;
    } else {
        if (lightNo == -1 || lightNo == -3) {
            loadLit(cut->getLightWork(lightNo), cut->nLight);
        }
        l = 0;
    }
    if (lightNo == -3 || lightNo == -2) {
        setEnv(cut, -1);
    }
    return l;
}

cLight* cLightMgr::createBack(cLit* lit, int cutNo, int lightNo, int flag)
{
    cLightEnv* cut;
    cLightWork* w;
    cLight* l;

    cut = lit->getCut(cutNo);
    if (cut == 0) {
        return 0;
    }
    if (lightNo >= 0) {
        w = cut->getLightWork(lightNo);
        if (!VALID_PTR(w)) {
            return 0;
        }
        l = createBack(w);
        if (!VALID_PTR(l)) {
            if (0) {
                log("cLightMgr::createBack() WORK ALLOC FAILED", 0);
            }
            return 0;
        }
        l->be_flag = flag | 3;
    } else {
        if (lightNo == -1 || lightNo == -3) {
            loadLit(cut->getLightWork(lightNo), cut->nLight);
        }
        l = 0;
    }
    if (lightNo == -3 || lightNo == -2) {
        setEnv(cut, -1);
    }
    return l;
}

cLight* cLightMgr::create(int litNo, int cutNo, int lightNo, int flag)
{
    cLit* lit;

    switch (litNo) {
    case 0:
    default:
        lit = x180;
        break;
    case 1:
        lit = x184;
        break;
    case 2:
        lit = x188;
        break;
    }
    return create(lit, cutNo, lightNo, flag);
}

cLight* cLightMgr::createBack(int litNo, int cutNo, int lightNo, int flag)
{
    cLit* lit;

    switch (litNo) {
    case 0:
    default:
        lit = x180;
        break;
    case 1:
        lit = x184;
        break;
    case 2:
        lit = x188;
        break;
    }
    return createBack(lit, cutNo, lightNo, flag);
}

f32 cLightMgr::setElecPower(f32 d)
{
    elecPower += d;
    if (elecPower < 0.0f) {
        elecPower = 0.0f;
    } else if (elecPower > 1.0f) {
        elecPower = 1.0f;
    }
    return elecPower;
}

int cLightMgr::setElecPower2(u8 pathNo, u8 idx)
{
    cCtrl* c;
    cCtrl* n;
    LightCtrlWork* w;
    cLightPathData* path;
    void (*func)(cCtrl*) = funcDelCtrl;

    c = CtrlMgr.pAlive;
    while (c) {
        n = c;
        c = (cCtrl*) c->next;
        func(n);
    }
    c = CtrlMgr.createBack(0);
    if (c == 0) {
        return 0;
    }
    c->id = 1;
    w = (LightCtrlWork*) c->work;
    path = getPathPtr(pathNo);
    if (!VALID_PTR(path)) {
        pLog->err(0, 0, "cLightMgr::setElecPower2() NO PATH DATA %d", pathNo);
        CtrlMgr.destroy(c);
        return 0;
    }
    w->pPath = w->pPath2 = path;
    w->idx = idx;
    return 1;
}

void funcDelCtrl(cCtrl* c)
{
    if (c->id == 1) {
        CtrlMgr.destroy(c);
    }
}

int cLightMgr::onKind(u8 kind)
{
    kindFlags[kind >> 5] |= 1 << (kind & 31);
    return 1;
}

int cLightMgr::offKind(u8 kind)
{
    kindFlags[kind >> 5] &= ~(1 << (kind & 31));
    return 1;
}

int cLightMgr::checkKind(u8 kind)
{
    if (kindFlags[kind >> 5] & (1 << (kind & 31))) {
        return 1;
    }
    return 0;
}

cLight* cLightMgr::getKindLight(u8 kind)
{
    cLight* l;

    for (l = LightMgr.pAlive; l; l = (cLight*) l->next) {
        if (l->kind == kind) {
            return l;
        }
    }
    return 0;
}

int cLightMgr::roomLitSet(cLit* lit)
{
    if (lit == 0) {
        lit = x184;
    }
    if (!IN_RANGE(lit)) {
        pLog->err(0, 0, "cLightMgr::roomLitSet() INVALID POINTER %08x", lit);
        lit = 0;
    }
    pLit = lit;
    return 1;
}

int cLightMgr::roomLitCheck()
{
    return pLit == x184;
}

int cLightMgr::move()
{
    cLight* l;
    cLight* n;
    void (*func)(cLight*);

    if (elecPower != 1.0f) {
        pLog->err(0, 0, "cLightMgr ElecPower != 1.0f");
    }
    if (colBrendRate != 0.0f) {
        pLog->err(0, 0, "cLightMgr m_ColBrendRate != 0.0f");
    }
    hokanMove();
    dieCheck();
    pG->flags_5010 |= 0x200;
    func = lightMove;
    l = pAlive;
    while (l) {
        n = l;
        l = (cLight*) l->next;
        func(n);
    }
    return 1;
}

void cLightMgr::hokanMove()
{
    if (hokanCnt != 0) {
        LightFog* fog = &env.fog;

        hokanCnt--;
        fog->start = (fog->start * hokanCnt + fogNew.start) / (hokanCnt + 1);
        fog->end = (fog->end * hokanCnt + fogNew.end) / (hokanCnt + 1);
        fog->color.r = (u8) (((f32) fog->color.r * hokanCnt + (f32) fogNew.color.r) / (hokanCnt + 1));
        fog->color.g = (u8) (((f32) fog->color.g * hokanCnt + (f32) fogNew.color.g) / (hokanCnt + 1));
        fog->color.b = (u8) (((f32) fog->color.b * hokanCnt + (f32) fogNew.color.b) / (hokanCnt + 1));
        fog->color.a = (u8) (((f32) fog->color.a * hokanCnt + (f32) fogNew.color.a) / (hokanCnt + 1));
        GXSetFog(fog->type, fog->start, fog->end, ZNEAR, ZFAR, fog->color);
        if (fog->type == 0) {
            View.setFarPlane(ZFAR);
        } else {
            View.setFarPlane(fog->end * (1.0f - env.farRate) + 1.0f);
        }
    }
}

void lightMove(cLight* l)
{
    cModel* p = l->pParent;
    cModel* em;

    if (p != 0 && !IS_ALIVE(p) && !(pG->flags_60 & 0x80000000)) {
        if (IS_ALIVE(l)) {
            LightMgr.destroy(l);
        }
        return;
    }
    if (l->be_flag & 2) {
        l->calcPos(&l->pos, &l->curPos);
    }
    l->hitAdjust();
    l->move();
    if (l->parentType == 3) {
        if (getRoomEtcOnLight(l->parentId, &em, 0) == 1) {
            if (((cEm*) em)->hp <= 0) {
                l->be_flag &= ~2;
            }
        }
    }
}

cLightEnv* cLightMgr::getEnvPtr()
{
    return &env;
}

void Light00_Move(cLight* l)
{
    l->curColor = l->color;
}

void cLightMgr::setModel2(cModel* m)
{
    cLight* l;
    cModel* parent;
    u32 i;
    int n = 0;
    int hit;
    f32 pri[8];

    memclr_asm(m->lightInfo.pLight, sizeof(m->lightInfo.pLight));
    for (i = 0; i < nArray; i++) {
        l = (cLight*) ((u8*) pArray + size * i);
        if ((l->be_flag & 3) != 3) {
            continue;
        }
        if (l->xF & m->lightInfo.x50) {
            hit = 1;
        } else if (!(m->lightInfo.x50 & 0x41) && l->isParent(m)) {
            hit = 1;
        } else {
            hit = 0;
        }
        if (hit == 0) {
            continue;
        }
        if (l->type == 4) {
            continue;
        }
        if (!checkKind(l->kind)) {
            continue;
        }
        if (m->id == 2 && (((cObj*) m)->x3D0 & 1) && (l->attr & 4)) {
            continue;
        }
        if (i <= 31 && !((1 << i) & m->lightInfo.x54) && !(pG->flags_5010 & 0x01000000)) {
            if (!(pG->flags_5010 & 0x04000000)) {
                continue;
            }
        }
        if ((*(u32*) &l->curColor & 0xFFFFFF00) == 0) {
            continue;
        }
        if (!lightHitCheck(m, l)) {
            continue;
        }
        parent = l->pParent;
        if ((pG->flags_5010 & 0x10000000) && parent != 0 && !(parent->be_flag & 0x800)) {
            continue;
        }
        if (n > 7) {
            if (pG->flags_68 & 0x100) {
                pLog->warn(6, 3, "MODEL'S LIGHT OVER 8 !! [%08X]", m);
            }
            m->error();
            return;
        }
        m->lightInfo.pLight[n] = l;
        pri[n] = (f32) l->x2B;
        n++;
    }
}

void cLightMgr::setCloth(cModel* m)
{
    cLight* l;
    u32 i;
    int n;

    if (!VALID_PTR(m)) {
        pLog->err(0, 0, "cLightMgr::setCloth() INVALID PTR %08x", m);
        return;
    }
    n = 0;
    for (i = 0; i < nArray; i++) {
        l = (cLight*) ((u8*) pArray + size * i);
        if ((l->be_flag & 3) != 3) {
            continue;
        }
        if (l->type == 4) {
            continue;
        }
        if (!(l->xF & 0x10)) {
            continue;
        }
        if ((*(u32*) &l->curColor & 0xFFFFFF00) == 0) {
            continue;
        }
        if (!lightHitCheck(m, l)) {
            continue;
        }
        if (n > 7) {
            if (pG->flags_68 & 0x100) {
                pLog->warn(6, 3, "MODEL'S LIGHT OVER 8 !! [%08X]", m);
            }
            m->error();
            return;
        }
        m->lightInfo.pLight[n] = l;
        n++;
    }
}

void cLightMgr::setEsp(EspLightList* list, u8 mask)
{
    cLight* l;
    u32 i;

    list->num = 0;
    for (i = 0; i < nArray; i++) {
        l = (cLight*) ((u8*) pArray + size * i);
        if ((l->be_flag & 3) != 3) {
            continue;
        }
        if (!(l->xF & mask)) {
            continue;
        }
        if (list->num == 8) {
            pLog->warn(0, 0, "cLightMgr::setEsp():ESP LIGHT MAX(%d)", 8);
            return;
        }
        list->p[list->num] = l;
        list->num++;
    }
}

int lightHitCheck(cModel* m, cLight* l)
{
    static int (*funcTbl[4])(cModel*, cLight*) = {
        lightHitCheckCylinder,
        lightHitCheckSphere,
        lightHitCheckBBox,
        lightHitCheckSphere,
    };
    return funcTbl[m->lightInfo.x51 & 3](m, l);
}

int lightHitCheckSphere(cModel* m, cLight* l)
{
    Vec pos;
    Vec lpos;
    cLightInfo* li = &m->lightInfo;

    li->getPos(m, &pos);
    l->getPos(&lpos);
    if (GetDistance3(&pos, &lpos) < li->size.x + l->x1C || l->x1C == 0.0f) {
        return 1;
    }
    return 0;
}

int lightHitCheckCylinder(cModel* m, cLight* l)
{
    static const Vec vech = { 0.0f, 1.0f, 0.0f };
    Vec pos;
    Vec tmp;
    Vec lpos;
    cLightInfo* li = &m->lightInfo;
    cModel* c;
    f32 r;

    c = li->getPos(m, &pos);
    l->getPos(&lpos);
    r = l->x1C;
    if (r == 0.0f) {
        return 1;
    }
    PSVECScale(&vech, &tmp, -li->size.y);
    PSMTXMultVecSR(c->mat, &tmp, &tmp);
    PSVECAdd(&pos, &tmp, &tmp);
    if (GetDistance3(&tmp, &lpos) < li->size.x + r) {
        return 1;
    }
    PSVECScale(&vech, &tmp, li->size.y);
    PSMTXMultVecSR(c->mat, &tmp, &tmp);
    PSVECAdd(&pos, &tmp, &tmp);
    return GetDistance3(&tmp, &lpos) < li->size.x + r;
}

int lightHitCheckBBox(cModel* m, cLight* l)
{
    Vec p;
    Vec* size;
    f32 sx;
    f32 sz;
    f32 sy;

    if (l->x1C == 0.0f) {
        return 1;
    }
    p = l->curPos;
    PSMTXMultVec(m->lightInfo.mat, &p, &p);
    size = &m->lightInfo.size;
    sx = size->x * m->scale.x;
    sy = size->y * m->scale.y;
    sz = size->z * m->scale.z;
    if (p.x - l->x1C > sx || p.x + l->x1C < -sx || p.z - l->x1C > sz || p.z + l->x1C < -sz || p.y - l->x1C > sy || p.y + l->x1C < -sy) {
        return 0;
    }
    return 1;
}

int cLightMgr::update(int cut_no, int hokan)
{
    cLightEnv* cut;
    cLight* l;
    u32 n;
    u32 i;

    if (pG->flags_5014 & 0x00400000) {
        cut_no = cutNo;
    } else {
        cutNo = cut_no;
    }
    if (pG->flags_5010 & 0x04000000) {
        return 0;
    }
    if (!VALID_PTR(pLit)) {
        pLog->err(0, 0, "cLightMgr::update() NO LIGHT DATA");
        return 0;
    }
    if (cut_no == -1) {
        pLog->warn(0, 0, "cLightMgr::update() Cut No is [-1]");
        cut_no = 0;
    }
    deleteScr();
    cut_no = pLit->getSafeCutNo(cut_no);
    cut = pLit->getCut(cut_no);
    registCut(cut, hokan);
    if (pLit->nMaxLight >= nArray) {
        int max;
        pLog->err(0, 0, "cLightMgr::update() nMaxLight Over %d/%d", pLit->nMaxLight, nArray);
        max = nArray - 1;
        pLit->nMaxLight = max;
    }
    n = pLit->nMaxLight - cut->nLight;
    if (n < 10) {
        n = 10;
    }
    for (i = 0; i < n; i++) {
        l = cManager<cLight>::create();
        l->be_flag |= 4;
        l->be_flag &= ~2;
    }
    return 1;
}

int cLightMgr::setThermo()
{
    cLightEnv* cut;
    cLight* l;
    u32 i;

    deleteScr();
    cut = getCutAddr(0, 11);
    registCut(cut, 0);
    if (pLit->nMaxLight >= nArray) {
        int max;
        pLog->err(0, 0, "cLightMgr::update() nMaxLight Over %d/%d", pLit->nMaxLight, nArray);
        max = nArray - 1;
        pLit->nMaxLight = max;
    }
    for (i = 0; i < pLit->nMaxLight - cut->nLight; i++) {
        l = cManager<cLight>::create();
        l->be_flag |= 4;
        l->be_flag &= ~2;
    }
    return 1;
}

int cLightMgr::registCut(cLightEnv* cut, int hokan)
{
    setEnv(cut, hokan);
    if (cut->nLight > nArray) {
        pLog->err(0, 0, "LightRegistCut() LIGHT NUM OVER %d", nArray);
        cut->nLight = nArray;
        return 0;
    }
    loadLit(cut->getLightWork(0), cut->nLight);
    return 1;
}

cLightWork* cLightEnv::getLightWork(int no)
{
    cLightWork* w;

    if (nLight == 0) {
        w = 0;
    } else {
        w = (cLightWork*) ((u8*) this + sizeof(cLightEnv) + no * sizeof(cLightWork));
    }
    return w;
}

u32 cLightEnv::getSize()
{
    return sizeof(cLightEnv) + nLight * sizeof(cLightWork);
}

cLightEnv* cLightMgr::getCutAddr(int litNo, int cutNo)
{
    cLit* lit;

    switch (litNo) {
    case 0:
    default:
        lit = x180;
        break;
    case 1:
        lit = x184;
        break;
    case 2:
        lit = x188;
        break;
    }
    return lit->getCut(cutNo);
}

int cLit::getSafeCutNo(int no)
{
    if (no < 0 || no >= nCut || !VALID_PTR(getCut(no))) {
        no = 0;
    }
    return no;
}

void cLightMgr::setFogStart(f32 v)
{
    env.fogStart = v;
}

void cLightMgr::setFogEnd(f32 v)
{
    env.fogEnd = v;
}

f32 cLightMgr::getFogStart()
{
    return env.fogStart;
}

f32 cLightMgr::getFogEnd()
{
    return env.fogEnd;
}

void cLightMgr::setFog()
{
    LightFog* fog = &env.fog;
    u8 c = 0;

    if ((pG->flags_58 & 0x4000) || (pG->flags_5010 & 0x04000000)) {
        GXColor black;
        black.r = black.g = black.b = black.a = c;
        GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, black);
    } else {
        GXSetFog(fog->type, fog->start, fog->end, ZNEAR, ZFAR, fog->color);
        if (fog->type == 0) {
            View.setFarPlane(ZFAR);
        } else {
            View.setFarPlane(fog->end * (1.0f - env.farRate) + 1.0f);
        }
    }
}

void cLightMgr::setBlur()
{
    s8 power = env.blurPower;
    s8 r = env.contrast[0];
    s8 g = env.contrast[1];
    s8 b = env.contrast[2];
    u32 type = env.blurType;

    Filter00SetAlpha(env.blurAlpha);
    Filter00SetPower(power);
    Filter00SetType(type);
    Filter00SetContrast(r, g, b);
}

void cLightMgr::deleteScr()
{
    cLight* l;
    u32 i;

    for (i = 0; i < nArray; i++) {
        l = (cLight*) ((u8*) pArray + size * i);
        if (l->checkScr()) {
            destroy(l);
        }
    }
}

void cLightMgr::offScr(u8 mask)
{
    cLight* l;
    u32 i;

    for (i = 0; i < nArray; i++) {
        l = (cLight*) ((u8*) pArray + size * i);
        if (l->checkScr()) {
            l->xF &= ~mask;
        }
    }
}

int cLightMgr::countScr()
{
    u32 i;
    int n = 0;

    for (i = 0; i < nArray; i++) {
        if (((cLight*) ((u8*) pArray + size * i))->checkScr()) {
            n++;
        }
    }
    return n;
}

int cLightMgr::setEnv(cLightEnv* cut, int hokan)
{
    fogNew = cut->fog;
    if (hokan < 0) {
        hokanCnt = cut->hokan;
    } else {
        hokanCnt = hokan;
    }
    if (hokanCnt != 0) {
        f32 fs = env.fogStart;
        f32 fe = env.fogEnd;
        GXColor fc = env.bgColor;
        env = *cut;
        env.fogStart = fs;
        env.fogEnd = fe;
        env.bgColor = fc;
    } else {
        env = *cut;
    }
    setFog();
    setBlur();
    setMipmap(cut);
    setTune(cut);
    cut->wind.set();
    if (cut->tevScale[0] > 2) {
        pLog->err(0, 0, "setEnv() TEV_SCALE ERROR %d", cut->tevScale[0]);
        return 0;
    }
    if (cut->tevScale[1] > 2) {
        pLog->err(0, 0, "setEnv() TEV_SCALE ERROR %d", cut->tevScale[1]);
        return 0;
    }
    gxCsScale[0] = cut->tevScale[0];
    gxCsScale[1] = cut->tevScale[1];
    return 1;
}

void cLightMgr::setTune(cLightEnv* cut)
{
    if (cut->tuneOn & 1) {
        tune[0] = cut->tune[0];
        tune[1] = cut->tune[1];
        tune[2] = cut->tune[2];
    } else {
        tune[0].r = 0xC8;
        tune[0].g = 0xC0;
        tune[0].b = 0xF0;
        tune[0].a = 0x80;
        tune[1].r = 0;
        tune[1].g = 0;
        tune[1].b = 0;
        tune[1].a = 0x80;
        tune[2].r = 0xC8;
        tune[2].g = 0xC0;
        tune[2].b = 0xF0;
        tune[2].a = 0x80;
    }
}

int cLightMgr::setMipmap(cLightEnv* cut)
{
    if (!VALID_PTR(cut)) {
        pLog->err(0, 0, "cLightMgr::setMipmap() INVALIED PTR %08X", cut);
        return 0;
    }
    if (cut->minLod > 9) {
        pLog->err(0, 0, "cLightMgr::setMipmap() INVALIED MIN_LOD %d", cut->minLod);
        cut->minLod = 0;
    }
    if (cut->maxLod > 9) {
        pLog->err(0, 0, "cLightMgr::setMipmap() INVALIED MAX_LOD %d", cut->maxLod);
        cut->maxLod = 5;
    }
    if (cut->maxLod < cut->minLod) {
        pLog->err(0, 0, "cLightMgr::setMipmap() INVALIED MIN > MAX %d %d", cut->minLod, cut->maxLod);
        max_lod = min_lod;
    }
    min_lod = cut->minLod;
    max_lod = cut->maxLod;
    switch (cut->aniso) {
    default:
        pLog->err(0, 0, "cLightMgr::setMipmap() INVALIED ANISO %d", cut->aniso);
        cut->aniso = 0;
        aniso = 0;
        break;
    case 0:
        aniso = 0;
        break;
    case 1:
        aniso = 1;
        break;
    case 2:
        aniso = 2;
        break;
    }
    if (lod_bias < -5.0f) {
        lod_bias = -5.0f;
    }
    if (lod_bias > 10.0f) {
        lod_bias = 10.0f;
    }
    lod_bias = cut->lodBias;
    return 0;
}

int cLightMgr::loadLit(cLightWork* w, u32 n)
{
    cLight* l;
    u32 i;

    for (i = 0; i < n; i++, w++) {
        l = create(w);
        if (l->be_flag & 2) {
            l->calcParent();
            l->calcPos(&l->pos, &l->curPos);
        }
        l->x140 = i;
    }
    return 1;
}

int cLightMgr::saveLit(cLightWork* w)
{
    cLight* l;
    u32 i;

    for (i = 0; i < nArray; i++) {
        l = (cLight*) ((u8*) pArray + size * i);
        if (l->checkScr()) {
            *w = *l;
            w++;
        }
    }
    return 1;
}

cLit** cLightMgr::getLitPPtr()
{
    return &pLit;
}

int cLightMgr::initPath(LightPathHeader* p)
{
    if (!IN_RANGE(p)) {
        pLog->err(0, 0, "cLightMgr::initPath() INVALID PTR %08X", p);
        return 0;
    }
    pPath = p;
    return 1;
}

cLightPathData* cLightMgr::getPathPtr(u8 no)
{
    u32 ofs;

    if (!VALID_PTR(pPath)) {
        pLog->err(0, 0, "cLightMgr::getPathPtr() PATH NOT INITIALIZED.");
        return 0;
    }
    if (no >= pPath->num) {
        pLog->err(0, 0, "cLightMgr::getPathPtr() INVALID ID %d.", no);
        return 0;
    }
    ofs = *(u32*) (no * 4 + (u32) pPath + 4);
    if (ofs == 0) {
        return 0;
    }
    return (cLightPathData*) ((u8*) pPath + ofs);
}

LightPathHeader* cLightMgr::getPathHeader()
{
    return pPath;
}

cLight::cLight()
{
    be_flag = 3;
    x140 = -1;
}

void cLight::move()
{
    LightMgr.funcTbl[type](this);
}

cLight& cLight::operator=(cLightWork& w)
{
    be_flag = w.flag;
    xD = w.xD;
    type = w.type;
    xF = w.xF;
    pos = w.pos;
    x1C = w.x1C;
    color = w.color;
    power = w.power;
    parentType = w.parentType;
    kind = w.kind;
    attr = w.attr;
    x2B = w.x2B;
    parentId = w.parentId;
    x30 = w.x30;
    x32 = w.x32;
    x34 = w.x34;
    setParent(w.parentType, w.parentId);
    spot = w.spot;
    sub = w.sub;
    path = w.path;
    return *this;
}

cLightWork& cLightWork::operator=(cLight& l)
{
    flag = l.be_flag;
    xD = l.xD;
    type = l.type;
    xF = l.xF;
    pos = l.pos;
    x1C = l.x1C;
    color = l.color;
    power = l.power;
    parentType = l.parentType;
    kind = l.kind;
    attr = l.attr;
    x2B = l.x2B;
    parentId = l.parentId;
    x30 = l.x30;
    x32 = l.x32;
    x34 = l.x34;
    spot = l.spot;
    sub = l.sub;
    path = l.path;
    return *this;
}

int cLight::checkScr()
{
    if (IS_ALIVE(this)) {
        if (be_flag & 4) {
            return 1;
        }
        return 0;
    }
    return 0;
}

void cLight::setPartsNo(int no)
{
    parentId = (no << 16) | parent.no;
}

int cLight::setParent(u8 type, u32 id)
{
    parentType = type;
    parentId = id;
    calcParent();
    return 1;
}

int cLight::setParent(cModel* m)
{
    u32 i;
    u32 n;

    if (!VALID_PTR(m)) {
        pLog->err(0, 0, "cLight::setParent() INVALID PTR %08x", m);
        return 0;
    }
    n = EmMgr.nArray;
    for (i = 0; i < n; i++) {
        if ((cModel*) ((u8*) EmMgr.pArray + EmMgr.size * i) == m) {
            setParent(1, (parentId & 0xFFFF0000) | m->id);
            return 1;
        }
    }
    for (i = 0; i < 250; i++) {
        if (SmdGetGroupObjPtr2(i) == m) {
            setParent(2, (parentId & 0xFFFF0000) | i);
            return 1;
        }
    }
    n = ObjMgr.nArray;
    for (i = 0; i < n; i++) {
        if ((cModel*) ((u8*) ObjMgr.pArray + ObjMgr.size * i) == m) {
            setParent(4, (parentId & 0xFFFF0000) | i);
            return 1;
        }
    }
    return 0;
}

cModel* cLight::calcParent()
{
    switch (parentType) {
    default:
        pLog->err(0, 0, "cLight::calcPos() INVALID PARENT TYPE %d    NO:%d", parentType, parentId);
        parentType = 0;
        parentId = 1;
    case 0:
        pParent = 0;
        break;
    case 1:
        pParent = EmMgr.getEmPtr((u8) parent.no, 0);
        break;
    case 2:
        pParent = SmdGetGroupObjPtr(parent.no);
        break;
    case 3:
        if (getRoomEtcOnLight(parent.no, &pParent, 0) == 0) {
            pParent = 0;
        }
        break;
    case 4:
        pParent = (cModel*) ((u8*) ObjMgr.pArray + ObjMgr.size * parent.no);
        break;
    }
    return pParent;
}

cModel* cLight::getCoord()
{
    cModel* p = pParent;
    int partsNo = parent.partsNo;

    if (p != 0 && IS_ALIVE(p) && partsNo < p->nParts) {
        return p->getPartsPtr(partsNo);
    }
    return 0;
}

int cLight::isParent(cModel* m)
{
    return m == pParent;
}

int cLight::getPos2(Vec* src, Vec* dst)
{
    return calcPos(src, dst);
}

int cLight::calcPos(Vec* src, Vec* dst)
{
    cModel* p;
    cModel* c;
    int partsNo;
    int no;

    if (!VALID_PTR(dst)) {
        pLog->err(0, 0, "cLight::getPos() INVALED PTR %08x", dst);
        return 0;
    }
    switch (parentType) {
    default:
        pLog->err(0, 0, "Lit:calcPos() %d-%d INVALID PARENT TYPE", parentType, parentId);
        setParent(0, 1);
    case 0:
        *dst = *src;
        break;
    case 1:
    case 2:
    case 4:
        c = getCoord();
        if (c != 0) {
            PSMTXMultVec(c->mat, src, dst);
        } else if (!(parentType == 1 && parent.no == 3)) {
            if (G_ROOM_ID != 0x320) {
                pLog->err(0, 0, "Lit:calcPos() MODEL PARENT NOT FOUND");
            }
            setTrans(0);
        }
        break;
    case 3:
        partsNo = parentId >> 16;
        no = parentId & 0xFFFF;
        if (getRoomEtcOnLight(parentId, &p, 0) == 0) {
            p = 0;
            if (!(pG->flags_60 & 0x80000000)) {
                pLog->err(0, 0, "Lit:calcPos() %d-%d ETCMODEL PARENT NOT FOUND", no, partsNo);
            }
        } else if (p != 0 && IS_ALIVE(p) && partsNo < p->nParts) {
            PSMTXMultVec(p->getPartsPtr(partsNo)->mat, src, dst);
        }
        break;
    }
    return 1;
}

int cLight::getNormal(Vec* src, Vec* dst)
{
    cModel* p;
    int partsNo;
    int no;

    if (!VALID_PTR(dst)) {
        pLog->err(0, 0, "cLight::getNormal() INVALED PTR %08x", dst);
        return 0;
    }
    switch (parentType) {
    default:
        pLog->err(0, 0, "cLight::getPos() INVALID PARENT TYPE %d", parentType);
    case 0:
        *dst = *src;
        break;
    case 1: {
        u32 pid = parentId;
        partsNo = pid >> 16;
        p = EmMgr.getEmPtr((u8) pid, 0);
        if (!(VALID_PTR(p) && IS_ALIVE(p) && partsNo < p->nParts)) {
            if (!(parentType == 1 && parent.no == 3)) {
                if (!(pG->flags_60 & 0x80000000)) {
                    pLog->err(0, 0, "cLight::getNormal() FAILED.");
                    setTrans(0);
                }
            }
            *dst = *src;
            return 0;
        }
        PSMTXMultVecSR(p->getPartsPtr(partsNo)->mat, src, dst);
        break;
    }
    case 2: {
        u32 pid = parentId;
        no = pid & 0xFFFF;
        partsNo = pid >> 16;
        p = SmdGetGroupObjPtr(no);
        if (!VALID_PTR(p)) {
            pLog->err(0, 0, "cLight::getNormal() SCROLL No Error %d", no);
            *dst = *src;
            return 0;
        }
        if (IS_ALIVE(p) && partsNo < p->nParts) {
            PSMTXMultVecSR(p->getPartsPtr(partsNo)->mat, src, dst);
            if (dst->x == 0.0f && dst->y == 0.0f && dst->z == 0.0f) {
                dst->x = 0.001f;
            }
            break;
        }
        pLog->err(0, 0, "cLight::getNormal() FAILED.");
        *dst = *src;
        return 0;
    }
    case 3:
        partsNo = parentId >> 16;
        no = parentId & 0xFFFF;
        if (getRoomEtcOnLight(parentId, &p, 0) == 0) {
            p = 0;
            if (!(pG->flags_60 & 0x80000000)) {
                pLog->err(0, 0, "cLight::getNormal() ETCMODEL PARENT NOT FOUND %d %d", no, partsNo);
            }
        } else if ((p->be_flag & 1) && partsNo < p->nParts) {
            PSMTXMultVecSR(p->getPartsPtr(partsNo)->mat, src, dst);
        }
        break;
    case 4: {
        u32 pid = parentId;
        no = pid & 0xFFFF;
        partsNo = pid >> 16;
        p = ObjMgrWork(no);
        if (!(VALID_PTR(p) && IS_ALIVE(p) && partsNo < p->nParts)) {
            if (!(pG->flags_60 & 0x80000000)) {
                pLog->err(0, 0, "cLight::getNormal() FAILED.");
            }
            *dst = *src;
            return 0;
        }
        PSMTXMultVecSR(p->getPartsPtr(partsNo)->mat, src, dst);
        break;
    }
    }
    return 1;
}

void cLight::setTrans(int on)
{
    if (on) {
        be_flag |= 2;
    } else {
        be_flag &= ~2;
    }
}

void cLight::hitAdjust()
{
    Vec pos;
    Vec top;

    if (x30 == 0.0f) {
        return;
    }
    pos = curPos;
    if (pParent != 0) {
        top.x = pParent->pos.x;
        top.y = pos.y;
        top.z = pParent->pos.z;
    } else {
        top = pos;
    }
    if (SatMgr.polySphereCk(&top, &pos, (f32) x30, 0x80, 0, 0x8C2800) == 1) {
        curPos = pos;
    }
}

void cLight::setSpotNormal(Vec* n)
{
    if (xD != 3 && xD != 6) {
        pLog->err(0, 0, "lit.setSpot() TYPE ERROR");
        return;
    }
    normal = *n;
}

void cLight::setSpotTarget(Vec* target)
{
    Vec n;

    PSVECSubtract(target, &pos, &n);
#line 2629 "D:/Bio4/Prog/light.cpp"
    VECNormalize(&n, &n);
    setSpotNormal(&n);
}

void cLightMgr::setItemLight()
{
    cLight* l = create(0, 10, 0, 0);
    l->kind = 0x7F;
}

void cLightMgr::beginEvent()
{
    offKind(0x7F);
}

void cLightMgr::endEvent()
{
    onKind(0x7F);
}

void cLightMgr::dbSetRoomLit(cLit* lit)
{
    pLit = lit;
    x184 = lit;
}

cLightEnv* cLit::getCut(u16 no)
{
    cLightEnv* cut;

    u32* ofs = (u32*) (this + 1);

    if (no >= nCut || ofs[no] == 0) {
        return 0;
    }
    cut = (cLightEnv*) ((u8*) this + ofs[no]);
    if (!VALID_PTR(cut)) {
        return 0;
    }
    return cut;
}

int cLit::versionUp()
{
    cLightEnv* cut;
    cLightWork* w;
    u32 i;
    u32 j;
    u32 max;
    int changed = 0;

    if (version <= 0x20) {
        for (i = 0; i < nCut; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                cut->minLod = 0;
                cut->maxLod = 5;
                cut->aniso = 0;
                cut->lodBias = 0.0f;
            }
        }
    }
    if (version <= 0x22) {
        for (i = 0; i < nCut; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                for (j = 0; j < cut->nLight; j++) {
                    w = cut->getLightWork(j);
                    if (w->kind != 0) {
                        pLog->warn(0, 0, "LitVer CAUTION %d", w->kind);
                        changed = 1;
                        w->kind = 0;
                    }
                }
            }
        }
    }
    if (version <= 0x23) {
        for (i = 0; i < nCut; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                changed = 1;
                cut->x100 = cut->x0;
                cut->xFC = cut->x0;
            }
        }
    }
    if (version <= 0x24) {
        for (i = 0; i < nCut; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                for (j = 0; j < cut->nLight; j++) {
                    w = cut->getLightWork(j);
                    if (w->type == 1) {
                        changed = 1;
                        w->color = w->sub.color;
                    }
                }
            }
        }
    }
    if (version <= 0x25) {
        for (i = 0; i < nCut; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                changed = 1;
                cut->tevScale[1] = 2;
                cut->tevScale[0] = 2;
            }
        }
    }
    if (version <= 0x26) {
        for (i = 0; i < nCut; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                changed = 1;
                cut->tuneOn = 0;
                cut->tune[0].r = 0;
                cut->tune[0].g = 0;
                cut->tune[0].b = 0;
                cut->tune[0].a = 0;
                cut->tune[1].r = 0;
                cut->tune[1].g = 0;
                cut->tune[1].b = 0;
                cut->tune[1].a = 0;
                cut->tune[2].r = 0;
                cut->tune[2].g = 0;
                cut->tune[2].b = 0;
                cut->tune[2].a = 0;
            }
        }
    }
    if (version <= 0x27) {
        for (i = 0; i < nCut; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                changed = 1;
                cut->contrast[0] = 0;
                cut->contrast[1] = 0;
                cut->contrast[2] = 0;
            }
        }
    }
    if (version <= 0x28) {
        for (i = 0; i < nCut; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                cut->hokan = 0;
                changed = 1;
            }
        }
    }
    if (version <= 0x29) {
        for (i = 0; i < nCut; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                for (j = 0; j < cut->nLight; j++) {
                    w = cut->getLightWork(j);
                    if (w->xF & 1) {
                        w->xF |= 0x40;
                        changed = 1;
                    }
                }
            }
        }
    }
    if (version <= 0x2A) {
        for (i = 0; i < nCut; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                for (j = 0; j < cut->nLight; j++) {
                    w = cut->getLightWork(j);
                    w->x2B = 3;
                }
            }
        }
    }
    if (version <= 0x2B) {
        for (i = 0; i < nCut; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                cut->wind.dir = 0;
                cut->wind.power = 0;
                cut->wind.x2 = 0;
            }
        }
    }
    max = getMaxLight();
    if (nMaxLight != max) {
        pLog->warn(0, 0, "cLit::versionUp() nMaxLight %d -> %d", nMaxLight, max);
        nMaxLight = max;
    }
    if (changed == 1) {
        pLog->warn(0, 0, "cLit::versionUp() VERSION UP");
    }
    version = 0x2C;
    for (i = 0; i < nCut; i++) {
        if (VALID_PTR(cut = getCut(i))) {
            if (cut->tevScale[0] > 2) {
                cut->tevScale[0] = 2;
            }
            if (cut->tevScale[1] > 2) {
                cut->tevScale[1] = 2;
            }
        }
    }
    return 1;
}

u32 cLit::getMaxLight()
{
    cLightEnv* cut;
    u32 max = 0;
    u32 i;

    for (i = 0; i < nCut; i++) {
        cut = getCut(i);
        if (cut != 0 && cut->nLight > max) {
            max = cut->nLight;
        }
    }
    return max;
}

// Sub screen (inventory) in: keep only the first 10 works alive for the item lights.
u32 nArrayBak;
cLight* pAliveBak;

// Scalar (reference) accesses: a struct-member access through `this` and a global scalar are
// assumed independent, and the scheduler would hoist the load above the store.
static inline cLight* PGet(cLight*& p) { return p; }
static inline void PSet(cLight*& d, cLight* v) { d = v; }

void cLightMgr::inSscrn()
{
    deleteScr();
    nArrayBak = nArray;
    nArray = 10;
    pAliveBak = PGet(pAlive);
    offKind(0x7F);
}

void cLightMgr::outSscrn(u32 mode)
{
    BitSet(nArray, nArrayBak);
    PSet(pAlive, pAliveBak);
    switch (mode) {
    case 0:
    default:
        LightMgr.update(0, 0);
        break;
    case 1:
        LightMgr.update(CamCtrl.area_no, 0);
        break;
    case 2:
        pG->flags_5010 |= 0x04000000;
        LightMgr.setThermo();
        break;
    }
    LightMgr.onKind(0x7F);
}

void cPenWind::set()
{
    PenWindSet((f32) dir * 3.1415927f / 127.0f, (f32) power * 0.01f, (f32) x2 * 0.01f);
}

cLightMgr LightMgr;
