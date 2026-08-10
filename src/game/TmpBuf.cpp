#include "main_mem.h"

// Shared scratch buffer for the draw pipeline.
void* g_pDrawTmpBuffer;
int g_Type;

void SetDrawTmpBufType(int type);

#line 43 "D:/Bio4/Prog/TmpBuf.cpp"
void AllocDrawTmpBuf()
{
    g_pDrawTmpBuffer = MEM_ALLOC(0x38000, 1, 13);
    SetDrawTmpBufType(0);
}

void* GetDrawTmpBufAddr(int type)
{
    SetDrawTmpBufType(type);
    return g_pDrawTmpBuffer;
}

void SetDrawTmpBufType(int type)
{
    g_Type = type;
}

int GetDrawTmpBufType()
{
    return g_Type;
}
