#include "cObjWep.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80044f2c-80044f2f, 4 bytes / 1 instruction: blr
// Empty function - likely a base-class virtual/overridable hook with no
// work needed for this weapon type, or a stub not yet implemented at the
// point this debug build was compiled.
void cObjWep::moveAll()
{
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80044f40-80044f43, 4 bytes / 1 instruction: blr
void cObjWep::moveReload()
{
}
