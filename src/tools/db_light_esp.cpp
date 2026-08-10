// t_esp's db_light.cpp: Tools' build plus cLightTool::setLogMode.
#define DB_LIGHT_SET_TOOL_LIGHT
#define DB_LIGHT_SET_LOG_MODE
#include "db_light.cpp"
// db_port.cpp's LightToolStart calls SetToolLight: global in the original t_esp REL although the .sym
// marks it local (the Tools wrapper has the same export).
asm(".globl SetToolLight__Fi");
