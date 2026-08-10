// t_sce / t_movie's db_light.cpp: Tools' build (SetToolLight in front) after the original REL link
// dead-stripped everything the module never calls at function level — the cLightTool / cLitPathTool
// constructors, destructors, move, lightAnalysis, expand, getCutNo and the cVarRange<u8> /
// cVarLoop<u8> members only those used (their strings and pools stay). modules.py lists the two
// units in STRIP_UNUSED; .rodata/.data/.bss are byte-identical to t_camera's object.
#define DB_LIGHT_SET_TOOL_LIGHT
#include "db_light.cpp"
// t_sce_at.cpp / t_se_at.cpp call SetToolLight: global in both RELs (the .sym marks it local).
asm(".globl SetToolLight__Fi");
