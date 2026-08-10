// Tools' db_light.cpp: t_camera's object with SetToolLight in front of it.
#define DB_LIGHT_SET_TOOL_LIGHT
#include "db_light.cpp"
// t_mv calls SetToolLight: the .sym scope is wrong, the symbol is global in the original Tools REL. The
// definition in db_light.cpp is `static` for the other builds, so the wrapper exports it here.
asm(".globl SetToolLight__Fi");
