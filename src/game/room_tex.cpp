#include "types.h"
#include "global.h"
#include "main_mem.h"
#include "texture.h"
#include "room_tex.h"

#line 20 "D:/Bio4/Prog/room_tex.cpp"

cTexSys* g_pRoomTexSys;

void RoomTexInit()
{
    g_pRoomTexSys = NULL;
}

void RoomTexRoomInit()
{
#line 41
    g_pRoomTexSys = (cTexSys*) MEM_ALLOC(sizeof(cTexSys), 1, 0xD);
    g_pRoomTexSys->Init("RoomTex", 256);
    RoomTexDataLoad((TexData*) (pG->pArc->ofs_18 + (u32) pG->pArc), 1);
}

int RoomTexDataLoad(TexData* data, u32 owner)
{
    return g_pRoomTexSys->DataLoad(data, owner, 1);
}

int RoomGetTplAddr(u32 id, TEXPalette** out)
{
    return g_pRoomTexSys->GetTplAddr(id, out);
}

int RoomGetTexObj(u32 id, u32 no, GXTexObj** out)
{
    return g_pRoomTexSys->GetTexObj(id, no, out);
}

int RoomGetTlutObj(u32 id, GXTlutObj** out)
{
    return g_pRoomTexSys->GetTlutObj(id, out);
}
