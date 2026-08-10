#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "obj.h"
#include "global.h"
#include "math_sub.h"

extern "C" void EmAtCheck(cModel* m);

// Obstacle model (Oba): an invisible collision model attached to a parent object (type 0: to
// one of its parts, type 1: to the object itself) or standing alone.
class cObjObaModel : public cObj {
public:
    virtual void move();
};

extern "C" cObj* SetObaModel(cObj* parent, int partsNo, Vec* ofs, f32 rad, u8 type, f32 h)
{
    cObj* obj;
    ObaModelWork* w;

    obj = ObjMgr.create(0x20);
    if (obj == 0) {
        return 0;
    }
    w = &obj->obaModel;
    if (obj->modelInit((void*) (pG->pArc->ofs_20 + (u32) pG->pArc),
                       (void*) (pG->pArc->ofs_24 + (u32) pG->pArc)) == 0) {
        pLog->err(0, 0, "SetObaModel() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 1000.0f, 1000.0f, 0.0f };

    obj->type = type;
    obj->lightInfo.init2(0, 1, &p0, &p1, 0x10);
    obj->sub2B4.atari.init(0, 0x2000, 10, 0.0f, 0.0f, 0.0f, rad, rad, rad, h);
    obj->sub2B4.atari.clrFlag100();
    obj->sub2B4.atari.setPriority(1);
    obj->be_flag &= ~2;
    w->parent = parent;
    w->ofs = *ofs;
    w->partsNo = partsNo;
    obj->move();
    return obj;
}

void cObjObaModel::move()
{
    ObaModelWork* w = &obaModel;

    if (w->parent) {
        if ((w->parent->be_flag & 0x201) != 1) {
            ObjMgr.destroy(this);
            return;
        }
        if (type == 0) {
            cModel* parts = w->parent->getPartsPtr(w->partsNo);
            PSMTXMultVec(parts->mat, &w->ofs, &pos);
            if (pos.y < w->parent->pos.y + 2000.0f) {
                pos.y = w->parent->pos.y;
            }
        }
        if (type == 1) {
            PSMTXMultVec(w->parent->mat, &w->ofs, &pos);
        }
        if (w->parent->sub2B4.atari.flags & 0x200) {
            sub2B4.atari.flags |= 0x200;
        } else {
            sub2B4.atari.flags &= ~0x200;
        }
    }
    RotMatrix(mat, &rot);
    TransMatrix(mat, &pos);
    ScaleMatrix(mat, &scale);
    matUpdate();
    EmAtCheck(this);
    sub2B4.atari.move();
}
