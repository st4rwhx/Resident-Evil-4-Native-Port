#ifndef READ_H
#define READ_H

#include "types.h"
#include "main_sub.h"

// A linked DLL with its data archive (game/read.cpp). The first 0x80 bytes are the module's bss area.
class ReadModule {
public:
    u8 bss[0x80];               // 0x00
    u16 id;                     // 0x80
    u16 flag;                   // 0x82  bit0 dll copied to its own block, bit1 linked,
                                //       bit2 data allocated by DvdReadN, bit3 debug heap
    void* pArc;                 // 0x84
    OSModuleHeader* pModule;    // 0x88
    u32 size;                   // 0x8C  data size
    u32 bssSize;                // 0x90  size of the part after the data (dll + bss)
    void* pInitFunc;            // 0x94  EmInitFunc set by the dll prolog

    ReadModule() { flag = 0; }
};

extern ReadModule EmReadModule[4];
extern ReadModule PlReadModule;
extern ReadModule WepReadModule;

extern "C" {
void CoreDataRead();
void OptionDataRead();
// Loads enemy module `id` (the rooms preload the enemies of their events); the read address
void* EmReadSearch(int id, void* addr, u32 size);
// Runs the module's prolog (the rooms re-link an enemy module after swapping event data into it).
void InitModule(ReadModule* m);
}

// Clears the enemy module list (r106 before the chapter-end event reloads them).
extern "C" void EmReadInit();

// The enemy module entry of enemy `id` (the rooms swap event data into the boss module's block).
ReadModule* SearchEmModule(int id);

#endif
