#ifndef CSTRING_H
#define CSTRING_H

#include "types.h"

// game/cString.cpp: minimal heap string. `str` points at the shared "" literal when empty.
class cString {
public:
    char* str;  // 0x00

    cString();
    cString(const char* s);
    // declared only: a user copy constructor makes the class BLKmode, so a `cString("...")` temporary
    // shares a freed aggregate slot (t_movie movie_test_main: the temp sits in the file entry's slot)
    cString(const cString& o);
    ~cString();
    cString& operator=(const cString& o);
    cString& operator+=(const cString& o);
    cString& operator+=(const char* s);
    char* c_str();
    u32 size();
    void clear();
    void copy(const char* s);
};

#endif
