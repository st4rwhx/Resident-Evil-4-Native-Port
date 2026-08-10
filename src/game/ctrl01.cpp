#include "types.h"
#include "cManager.h"
#include "ctrl.h"
#include "light.h"
#include "db_log.h"

// Dead-stripped by the original linker (STRIP_UNUSED): only its strings, the strings of the
// inlined CtrlMgr.destroy() (which instantiates cManager<cCtrl>::log) and its constant pool remain.
static void Ctrl01_Move(cCtrl* c)
{
    s32* w = (s32*) c->work;

    if (w[0] == 0) {
        pLog->err(0, 0, "Ctrl01_Move() PATH PTR ERR %08X", w[0]);
        CtrlMgr.destroy(c);
        return;
    }
    w[1] = (s32) ((f32) w[2] * 0.005f);
}
