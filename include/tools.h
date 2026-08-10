#ifndef TOOLS_H
#define TOOLS_H

#include "types.h"

// Exports of the tool module entry object (tools/tools.cpp). The work-array helpers exist only in the
// modules built with TOOLS_ARRAY (t_id, t_esp, Tools; config/G4BE08/modules.py CFLAGS): they park the
// room's manager arrays and give the tool fresh Debug-heap ones; a set bit EXCLUDES a pool (1 parts,
// 2 em, 4 obj, 8 esp, 0x10 espgen, 0x20 ctrl, 0x80 event, 0x100 light).
void ToolsTask();
void ToolArrayPush(int flags);
void ToolWorkPop(int flags);
void ToolEmArraySet(int on);  // t_esp only (TOOLS_EM_ARRAY): 10 enemy works on / off

#endif
