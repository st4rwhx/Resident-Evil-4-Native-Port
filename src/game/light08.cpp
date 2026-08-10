#include "light.h"
#include "em.h"

struct Light08Work {
    u8 type;     // 0x00
    u8 emId;     // 0x01
    u8 partsNo;  // 0x02
};

cLight08::cLight08()
{
}

// Spot light that tracks an enemy model part.
void Light08_Move(cLight* l)
{
    Light08Work* w = (Light08Work*)l->work;

    if (w->type == 0) {
        cEm* em = EmMgr.getEmPtr(w->emId, 0);
        if (em) {
            cModel* parts = em->getPartsPtr(w->partsNo);
            if (parts) {
                l->setSpotTarget(&parts->worldPos);
            }
        }
    }
}
