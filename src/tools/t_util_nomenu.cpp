// t_light's t_util.cpp: the same source without the menu statics (its object has no .data; the linker
// then dead-stripped everything but TutilInitDefault/TutilQuitDefault).
#define T_UTIL_NO_MENU_DATA
#include "t_util.cpp"
