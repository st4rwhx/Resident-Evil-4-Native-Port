#include "obj.h"
#include "math_sub.h"

// Plain box object: rebuilds its matrix from pos/rot/scale every frame.
class cObjBox : public cObj {
public:
    cObjBox();
    virtual void move();
};

cObjBox::cObjBox()
{
}

void cObjBox::move()
{
    RotMatrix(mat, &rot);
    TransMatrix(mat, &pos);
    ScaleMatrix(mat, &scale);
    partsMatCalc();
    partsWorldCalc();
}
