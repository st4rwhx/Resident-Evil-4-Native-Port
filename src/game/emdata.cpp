// game/emdata.cpp: enemy effect data swap (per enemy id effect data slot).

#include "atari.h"
#include "light.h"
#include "em.h"
#include "db_log.h"

// read.cpp enemy module entry (SearchEmModule); only the archive pointer is used here.
struct EmModule {
    u8 pad_0[0x84];
    void* pArc;   // 0x84
};

EmModule* SearchEmModule(int id);              // game/read.cpp (C++ linkage: SearchEmModule__Fi)
extern "C" {
void EspDataRelease(int a, int b, int c);      // game/eff_sys.cpp
void EspDataLoad(void* data, int a, int b);    // game/eff_sys.cpp
}
void* GetDataExt(void* arc, const char* tag, int no);   // game/read.cpp

int GetEmEffId(int id)
{
    int ret = -1;

    switch (id) {
    case 0x11 ... 0x20:
        ret = 0x10;
        break;
    case 0x2B:
        ret = 0x23;
        break;
    case 0x2F:
        ret = 0x27;
        break;
    case 3:
        ret = 4;
        break;
    case 4:
        ret = 7;
        break;
    case 0x22:
        ret = 0x1A;
        break;
    case 0x2C:
        ret = 0x24;
        break;
    case 0x2D:
        ret = 0x25;
        break;
    case 0x36:
        ret = 0x2D;
        break;
    case 0x30:
        ret = 0x28;
        break;
    case 0x31:
        ret = 0x29;
        break;
    case 0x32:
        ret = 0x2A;
        break;
    case 0x39:
        ret = 0x2F;
        break;
    default:
        pLog->err(0, 0, "GetEmEffId(): ID[%x] invalid.", id);
        break;
    }
    return ret;
}

void EspEmDataSwapPush(int id)
{
    int eff = GetEmEffId(id);

    if (eff == -1) {
        pLog->err(0, 0, "EspEmDataSwapPush(): ID[%x] invalid.", id);
    } else {
        EspDataRelease(eff, 1, 0);
    }
}

void EspEmDataSwapPop(int id)
{
    int eff = GetEmEffId(id);
    void* data;

    if (eff == -1) {
        pLog->err(0, 0, "EspEmDataSwapPop(): ID[%x] invalid.", id);
        return;
    }
    data = GetDataExt(SearchEmModule(id)->pArc, "EFF", 0);
    if (data == 0) {
        pLog->err(0, 0, "EspEmDataSwapPop(): ID[%x] '.EFF' not found.", id);
        return;
    }
    EspDataLoad(data, eff, 0);
}
