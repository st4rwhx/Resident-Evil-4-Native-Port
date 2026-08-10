#include "esp.h"

class cEsp00 : public cEsp {
public:
    virtual void move();
};

cEsp* Esp00_Create()
{
    return new cEsp00;
}

void cEsp00::move()
{
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}
