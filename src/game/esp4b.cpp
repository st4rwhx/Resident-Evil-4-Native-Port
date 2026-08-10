#include "atari.h"
#include "light.h"
#include "rnd.h"
#include "esp.h"

// Static sprite showing one selected (or random) animation pattern.
class cEsp4b : public cEsp {
public:
    u32 xF8;

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp4b_Create()
{
    return new cEsp4b;
}

void cEsp4b::move()
{
    CommonMove();
}

int cEsp4b::SetFreeWork(EspGenWork* gen, u32* seed)
{
    EspAnmData* anm;
    u32 ptn;

    if (!EspGetAnmAddr(anmNo, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", anmNo);
        return 0;
    }
    ptn = gen->xC8;
    if (ptn == 0xff) {
        anmPtn = (Rnd() + Rnd()) % anm->nPtn;
    } else {
        if (ptn >= anm->nPtn) {
            pLog->err(0, 0, "ESP4B : PtnNo[%d >= %d] over", ptn, anm->nPtn);
            return 0;
        }
        anmPtn = ptn;
    }
    return 1;
}
