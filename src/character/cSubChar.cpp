#include "cSubChar.h"

// --- Matching status: Equivalent (bit-extraction recipe confirmed correct -
// mask 0x8 verified right - only r0-vs-r3 scratch register differs) ---
// 80053a98-80053aa3, 12 bytes / 3 instructions:
//   lwz r3,0x434(r3); rlwinm r3,r3,0x1d,0x1f,0x1f; blr
// Confirms the "(flags & N) != 0" -> single rlwinm/extrwi bit-extraction
// recipe (discovered while matching CameraControl::IsChangeCamera's
// sibling attempts) generalizes to other functions - this compiled to the
// same 2 real instructions + blr, extracting the same bit (0x8). Only
// difference: this compile loads into r0 then extracts to r3; the target
// loads directly into r3 and extracts in place (r3 -> r3) - the same
// register-reuse-vs-fresh-register class of near-miss documented in
// docs/matching/CameraControl_pointers.md, just the opposite direction.
bool cSubChar::ckPlRun()
{
    return (mRunFlags & 0x8) != 0;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80056604-80056607, 4 bytes / 1 instruction: blr
void cSubChar::setFace()
{
}
