#ifndef SIMPLECLASSES1_H
#define SIMPLECLASSES1_H

#include "types.h"

class CardID {
public:
    void setAction(u32 action);
private:
    u8 unk_00[0x1c];
    u32 mAction;  // 0x1c
};

class cUnit {
public:
    void beginEvent();
    void endEvent();
};

class Message {
public:
    void WaitEnd();
private:
    u8 unk_00[0x8e];
    u16 mWaitFlag;  // 0x8e - set to 1 by WaitEnd()
};

class CountDown {
public:
    void initTimeFrame(u32 frame);
    u32 getFrame();
private:
    u8 unk_00[0x8];
    u32 mFrame;  // 0x8 - shared by both methods
};

class cString {
public:
    char* c_str();
private:
    char* mData;  // 0x0
};

class cLog {
public:
    bool on(u8 v);
private:
    u8 unk_00[0x6];
    u8 mFlag;  // 0x6
};

class cDvdQueue {
public:
    u32 fileGetLength();
private:
    u8 unk_00[0x3c];
    u32 mFileLength;  // 0x3c
};

class cDataUnit {
public:
    u32 getCommand();
    void setCondition(u32 cond);
    u32 getCondition();
private:
    u32 mCondition;  // 0x0 - both set/get use this offset
    u32 mCommand;     // 0x4
};

class cSatFile {
public:
    void* getVertexPtr();
private:
    u8 unk_00[0x14];
};

#endif
