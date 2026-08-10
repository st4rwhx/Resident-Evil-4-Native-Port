#ifndef ST_MGR_EVENT_H
#define ST_MGR_EVENT_H

#include "types.h"
#include "cManager.h"

// cManager<T>::beginEvent / endEvent bodies for the stage rooms that call them on a DOL manager
// (`DmgMgr.beginEvent(0)` after EffectEventDelete): the DOL defines them only in sce_com.cpp, so
// every room object carries its own linkonce copy. cUnit::beginEvent takes an `int` in the original
// (see sce_com.cpp); the shared cManager.h declaration is still `()`, hence the view class.
class cUnitEventView {
public:
    u32 be_flag;
    cUnit* next;
    virtual ~cUnitEventView();
    virtual void beginEvent(int mode);
    virtual void endEvent(int mode);
};

template <class T>
void cManager<T>::beginEvent(int mode)
{
    u32 i;

    for (i = 0; i < nArray; i++) {
        T* p = (T*) ((u8*) pArray + size * i);
        if (p->isAlive()) {
            ((cUnitEventView*) p)->beginEvent(mode);
        }
    }
}

template <class T>
void cManager<T>::endEvent(int mode)
{
    u32 i;

    for (i = 0; i < nArray; i++) {
        T* p = (T*) ((u8*) pArray + size * i);
        if (p->isAlive()) {
            ((cUnitEventView*) p)->endEvent(mode);
        }
    }
}

#endif
