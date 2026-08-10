#include "Light00.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8014efd4-8014efdf, 12 bytes / 3 instructions:
//   lwz r0,0x20(r3); stw r0,0x13c(r3); blr
void Light00_Move(Light00Obj* obj)
{
    obj->field_0x13c = obj->field_0x20;
}
