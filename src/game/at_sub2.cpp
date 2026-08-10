#include "atari.h"
#include "at_sub2.h"

int AtEffInfo::getWepEff(int wepId, u32* eff1, u32* eff2)
{
    int ret = 1;

    switch (wepId) {
    case 1:  // handguns
    case 2:
    case 3:
    case 4:
        *eff1 = effGun[0];
        *eff2 = effGun[1];
        break;
    case 5:  // shotguns
    case 6:
    case 7:
    case 8:
        *eff1 = effGun[0];
        *eff2 = effGun[1];
        break;
    case 9:  // rifles
    case 10:
    case 11:
    case 12:
        *eff1 = effGun[0];
        *eff2 = effGun[1];
        break;
    case 0x0F:
    case 0x11:
    case 0x21:
        *eff1 = effGun[0];
        *eff2 = effGun[1];
        break;
    case 0x13:
        *eff1 = eff13[0];
        *eff2 = eff13[1];
        break;
    case 0x16:
        *eff1 = eff16[0];
        *eff2 = eff16[1];
        break;
    case 0x17:
        *eff1 = eff17[0];
        *eff2 = eff17[1];
        break;
    case 0x0D:
        *eff1 = eff0D[0];
        *eff2 = eff0D[1];
        break;
    default:
        ret = 0;
        pLog->err(0, 0, "getWepEff() INVALID ARG %d %d %d", wepId, *eff1, *eff2);
        break;
    }
    return ret;
}
