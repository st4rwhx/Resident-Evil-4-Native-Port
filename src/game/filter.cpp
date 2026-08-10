#include "filter.h"
#include "main_sub.h"

int FilterInit()
{
    Filter00Init();
    Filter01Init();
    Filter02Init();
    Filter03Init();
    Filter04Init();
    Filter05Init();
    Filter06Init();
    Filter07Init();
    Filter08Init();
    Filter09Init();
    Filter0aInit();
    Filter0bInit();
    return 1;
}

int FilterRoomInit()
{
    Filter00RoomInit();
    Filter01RoomInit();
    Filter02RoomInit();
    Filter03RoomInit();
    Filter04RoomInit();
    Filter05RoomInit();
    Filter06RoomInit();
    Filter07RoomInit();
    Filter08RoomInit();
    Filter09RoomInit();
    Filter0aRoomInit();
    Filter0bRoomInit();
    return 1;
}

// Full-screen filters only run on the full-size frame buffer.
void FilterTrans()
{
    if (Render_checkBlurPermission()) {
        if (Screen.width == 512.0f && Screen.height == 448.0f && !Filter09GetbUse()) {
            Filter00Trans();
            Filter01Trans();
            Filter08Trans();
            Filter02Trans();
            Filter03Trans();
            Filter04Trans();
            Filter05Trans();
            Filter06Trans();
            Filter07Trans();
            Filter0aTrans();
            Filter0bTrans();
        }
    }
}
