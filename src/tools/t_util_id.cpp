// t_id's t_util.cpp: the same source without the unreferenced zero word (.data: old_menu, cursor_s,
// flicker; the linker then dead-stripped every function).
#define T_UTIL_NO_NUM
#include "t_util.cpp"
