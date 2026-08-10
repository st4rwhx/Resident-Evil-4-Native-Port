#include "types.h"
#include "room_data.h"

// Stage 2 room module entry (D:/Bio4/Prog/st2.cpp; the same object ends every st2_* REL): registers the room
// Init/Main pairs of every stage-2 room in the DOL's St2_data_tbl, then the SN REL entry points.

extern "C" void OSReport(const char* fmt, ...);

#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

extern RoomTblEntry St2_data_tbl[46];

// _ctors/_dtors: the linker script's labels on the .ctors/.dtors lists (null terminated)
extern void (*_ctors[])(void);
extern void (*_dtors[])(void);

void R200Init();
void R200Main();
void R201Init();
void R201Main();
void R202Init();
void R202Main();
void R203Init();
void R203Main();
void R204Init();
void R204Main();
void R205Init();
void R205Main();
void R206Init();
void R206Main();
void R207Init();
void R207Main();
void R208Init();
void R208Main();
void R209Init();
void R209Main();
void R20aInit();
void R20aMain();
void R20bInit();
void R20bMain();
void R20cInit();
void R20cMain();
void R20dInit();
void R20dMain();
void R20eInit();
void R20eMain();
void R20fInit();
void R20fMain();
void R210Init();
void R210Main();
void R211Init();
void R211Main();
void R212Init();
void R212Main();
void R213Init();
void R213Main();
void R214Init();
void R214Main();
void R215Init();
void R215Main();
void R216Init();
void R216Main();
void R217Init();
void R217Main();
void R218Init();
void R218Main();
void R219Init();
void R219Main();
void R21aInit();
void R21aMain();
void R21bInit();
void R21bMain();
void R21dInit();
void R21dMain();
void R220Init();
void R220Main();
void R221Init();
void R221Main();
void R222Init();
void R222Main();
void R223Init();
void R223Main();
void R224Init();
void R224Main();
void R225Init();
void R225Main();
void R226Init();
void R226Main();
void R227Init();
void R227Main();
void R228Init();
void R228Main();
void R229Init();
void R229Main();
void R22aInit();
void R22aMain();
void R22bInit();
void R22bMain();
void R22cInit();
void R22cMain();

void set(int no, void (*init)(), void (*main)())
{
    St2_data_tbl[no].init = init;
    St2_data_tbl[no].main = main;
}

void setTbl()
{
    set(0, R200Init, R200Main);
    set(1, R201Init, R201Main);
    set(2, R202Init, R202Main);
    set(3, R203Init, R203Main);
    set(4, R204Init, R204Main);
    set(5, R205Init, R205Main);
    set(6, R206Init, R206Main);
    set(7, R207Init, R207Main);
    set(8, R208Init, R208Main);
    set(9, R209Init, R209Main);
    set(10, R20aInit, R20aMain);
    set(11, R20bInit, R20bMain);
    set(12, R20cInit, R20cMain);
    set(13, R20dInit, R20dMain);
    set(14, R20eInit, R20eMain);
    set(15, R20fInit, R20fMain);
    set(16, R210Init, R210Main);
    set(17, R211Init, R211Main);
    set(18, R212Init, R212Main);
    set(19, R213Init, R213Main);
    set(20, R214Init, R214Main);
    set(21, R215Init, R215Main);
    set(22, R216Init, R216Main);
    set(23, R217Init, R217Main);
    set(24, R218Init, R218Main);
    set(25, R219Init, R219Main);
    set(26, R21aInit, R21aMain);
    set(27, R21bInit, R21bMain);
    set(29, R21dInit, R21dMain);
    set(32, R220Init, R220Main);
    set(33, R221Init, R221Main);
    set(34, R222Init, R222Main);
    set(35, R223Init, R223Main);
    set(36, R224Init, R224Main);
    set(37, R225Init, R225Main);
    set(38, R226Init, R226Main);
    set(39, R227Init, R227Main);
    set(40, R228Init, R228Main);
    set(41, R229Init, R229Main);
    set(42, R22aInit, R22aMain);
    set(43, R22bInit, R22bMain);
    set(44, R22cInit, R22cMain);
}

extern "C" void _prolog()
{
    void (**p)(void);

    for (p = _ctors; *p; p++) {
        (*p)();
    }
    setTbl();
    OSReport("prolog...\n");
}

extern "C" void _epilog()
{
    void (**p)(void);

    for (p = _dtors; *p; p++) {
        (*p)();
    }
    OSReport("epilog...\n");
}

extern "C" void _unresolved()
{
    OSReport("unresolved...\n");
#line 169 "D:/Bio4/Prog/st2.cpp"
    HALT();
}
