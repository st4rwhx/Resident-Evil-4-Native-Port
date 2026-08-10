#ifndef DBG_VAR_H
#define DBG_VAR_H

#include "types.h"

// Range-limited variable of the debug tools (db_light.cpp instantiates cVarLoop<u8>). The value and
// its bounds come first, the vptr after them (GCC 2.95 layout). The members are defined out of class:
// the original calls init/limitUpper/limitLower out of line from the constructor (implicitly inline
// in-class bodies would be inlined there).
template <class T>
class cVarRange {
public:
    T val;    // 0x00
    T lower;  // 0x01
    T upper;  // 0x02
    // 0x04 vptr

    void init(const T& lo, const T& hi);
    // the value after adding `d`, limited at the upper / lower bound
    virtual int limitUpper(int d) = 0;
    virtual int limitLower(int d) = 0;
    int operator==(int x);
    T operator--(int);
    T operator++(int);
    operator int();
};

template <class T>
void cVarRange<T>::init(const T& lo, const T& hi)
{
    lower = lo;
    upper = hi;
}

template <class T>
int cVarRange<T>::operator==(int x)
{
    return val == x;
}

template <class T>
T cVarRange<T>::operator--(int)
{
    T old = val;
    val = limitLower(-1);
    return old;
}

template <class T>
T cVarRange<T>::operator++(int)
{
    T old = val;
    val = limitUpper(1);
    return old;
}

template <class T>
cVarRange<T>::operator int()
{
    return val;
}

// Wrapping variant: stepping past a bound continues from the other one.
template <class T>
class cVarLoop : public cVarRange<T> {
public:
    cVarLoop(const T& lo, const T& hi, const T& v);
    virtual int limitUpper(int d);
    virtual int limitLower(int d);
};

template <class T>
cVarLoop<T>::cVarLoop(const T& lo, const T& hi, const T& v)
{
    init(lo, hi);
    val = v;
    val = limitUpper(0);
    val = limitLower(0);
}

template <class T>
int cVarLoop<T>::limitUpper(int d)
{
    // `range` before `v` (limitLower declares them the other way round): the declaration order
    // decides the load / compare schedule of the entry block.
    int range = upper - lower + 1;
    int v = val + d;
    while (v > upper) {
        v -= range;
    }
    return v;
}

template <class T>
int cVarLoop<T>::limitLower(int d)
{
    int v = val + d;
    int range = upper - lower + 1;
    while (v < lower) {
        v += range;
    }
    return v;
}

#endif
