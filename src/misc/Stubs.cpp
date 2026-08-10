#include "Stubs.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80041c0c-80041c0f, 4 bytes / 1 instruction: blr
void cObjEgg::waterExplode()
{
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800d2f24-800d2f27, 4 bytes / 1 instruction: blr
void CameraLookDownEm::move()
{
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800d2f98-800d2f9b, 4 bytes / 1 instruction: blr
void IDApplication::init()
{
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800d2f9c-800d2f9f, 4 bytes / 1 instruction: blr
void IDApplication::move()
{
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800d2fa0-800d2fa3, 4 bytes / 1 instruction: blr
void IDApplication::quit()
{
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800d2fc8-800d2fcb, 4 bytes / 1 instruction: blr
void IDApplication::operator_delete()
{
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800d2fc4-800d2fc7, 4 bytes / 1 instruction: blr
void cCamera::operator_delete()
{
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800da294-800da297, 4 bytes / 1 instruction: blr
void cCtrl::move()
{
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800da298-800da29b, 4 bytes / 1 instruction: blr
void cCtrl::trans()
{
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800edd28-800edd2b, 4 bytes / 1 instruction: blr
void cEm::move()
{
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800edb40-800edb47, 8 bytes / 2 instructions: li r3,0; blr
bool cEm::checkThrow()
{
    return false;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8010ac1c-8010ac1f, 4 bytes / 1 instruction: blr
void cEmRock::beginEvent()
{
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80113abc-80113abf, 4 bytes / 1 instruction: blr
void cEmTree::beginEvent()
{
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80115378-80115383, 12 bytes / 3 instructions: li r0,1; stb r0,0x455(r3); blr
void cEmTree::setCatch()
{
    mCatch = 1;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8011e75c-8011e763, 8 bytes / 2 instructions: li r3,1; blr
bool cEsp::SetFreeWork()
{
    return true;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8011f008-8011f00b, 4 bytes / 1 instruction: blr
void cEsp::Destruct()
{
}
