#ifndef ETC_MODEL_H
#define ETC_MODEL_H

#include "types.h"
#include "vec.h"

// Room item (game/EtcModel.cpp). Only the flag word is known.
struct EtcItem {
    u32 flags;   // 0x00  0x02: taken
    u8 pad_4[0x70 - 0x04];
    Vec pos;     // 0x70
};

// One room etc model record (0x28 bytes, EtcModelListSet steps through them) handed to the
// Et*_init functions (EtcModel.cpp, et00.cpp).
struct EtcSetData {
    u16 id;          // 0x00  etc model id (EtcModelSet switch, 0x00..0x67)
    union {
        u16 no;      // 0x02  g_EtcTbl slot (< 0x40)
        struct {
            u8 pad_2;
            u8 type; // 0x03  low byte of `no`: the etc number the Set* functions take (WindowData row)
        };
    };
    u8 pad_4[0x10 - 0x4];
    Vec rot;         // 0x10
    Vec pos;         // 0x1C
};

// EtcModel.cpp is C++ but exports its functions with C linkage (unmangled names in the DOL).
// C++ linkage (sym_map: GetEtcFlgPtr__Fii, getRoomEtcItem__FiPP7EtcItemi)
u16* GetEtcFlgPtr(int no, int room);   // etc flag word of etc model `no` in `room` (stage << 8 | room), 0 when none
int getRoomEtcItem(int room, EtcItem** out, int a);

extern "C" {
void* GetEtcAddr(void* arc, const char* name);   // file `name` inside the room etc archive
// Model a light of parent type 3 (room etc model) hangs on; 1 = found (light.cpp)
int getRoomEtcOnLight(u32 id, class cModel** out, int flag);
}

// Room etc enemies by etc number (the stage rooms delete / hide them); 1 = found.
class cEm;
extern "C" {
int getRoomEtcBreak(int no, cEm** out, int flag);
int setRoomEtcDisp(int no, int on, int flag);
int getRoomEtcWindow(int no, cEm** out, int flag);
int getRoomEtcBox(int no, cEm** out, int flag);
int getRoomEtcDoor(int no, cEm** out, int flag);
int getRoomEtcRack(int no, cEm** out, int flag);
int getRoomEtcLadder(int no, cEm** out, int flag);
int getRoomEtcTorch(int no, cEm** out, int flag);
int getRoomEtcSwitch(int no, cEm** out, int flag);
int getRoomEtcBarred(int no, cEm** out, int flag);
int getRoomEtcDram(int no, cEm** out, int flag);
int EtcGetDasAddr(int id, void** out);   // archive of etc model `id` (r400 setLadderMotion)
// Generic lookup by etc type (getRoomEtc* call it; r20d counts the torches / lamps with it).
int getRoomEtc(int no, int type, cEm** out, int flag);
}

#endif
