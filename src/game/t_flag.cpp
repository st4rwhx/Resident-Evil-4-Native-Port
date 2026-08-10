#include "types.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "room_data.h"
#include "t_util.h"

extern "C" int strcmp(const char* a, const char* b);

// Flag editor tool: pages of bit flags with names.

struct FE_WORK {
    s16 page;      // 0x00  fe_data index
    s16 cursor;    // 0x02  bit number in the page
    u16 mode;      // 0x04  func_tbl index (0 = move, 1 = die)
    u32 stop_bak;  // 0x08  saved pG stop flags
};

struct FE_DATA {
    const char* name;    // 0x00
    u32* flags;          // 0x04
    s16 bits;            // 0x08  flag word bits on this page
    s16 base;            // 0x0A  added to the bit number for display
    const char** names;  // 0x0C
    u32 num_names;       // 0x10
};

static void init(FE_WORK* t);
static void move(FE_WORK* t);
static void die(FE_WORK* t);
int CkBit(u32* flags, u32 bit);

static const char* dbg_s[125] = {
    "DBG_TEST_MODE", "DBG_SCR_TEST", "DBG_BACK_CLIP", "DBG_DBG_CAM", "DBG_SAT_DISP", "DBG_EAT_DISP",
    "DBG_EVENT_TOOL", "DBG_SLOW_ON", "DBG_SHADOW_POLYGON", "DBG_SCE_AT_DISP", "DBG_SCR2_TEST",
    "DBG_SHADOW_FRAME", "DBG_MIRROR_POLYGON", "DBG_GROUND_DISP", "DBG_SKELETON_DISP",
    "DBG_ESPTOOL_ONSCR", "DBG_CINESCO_OFF", "DBG_RTP_DISP", "DBG_ROOM_WIRE_DISP",
    "DBG_EM_YARARE_DISP", "DBG_CAM_AREA_OFF", "DBG_CLOTH_AT_DISP", "DBG_WIND_ON",
    "DBG_ESPTOOL_MEM_USE", "DBG_TEX_RENDER_ALL", "DBG_ESPTOOL_ONEM", "DBG_EMINFO_DISP",
    "DBG_LIGHT_TOOL", "", "", "", "", "DBG_COCKPIT_TOOL", "DBG_BOUNDING_DISP", "DBG_ADJUST_CAM",
    "DBG_FLAT_FLOOR", "DBG_OBJ_SKELETON_", "DBG_DRAW_SH_TEX", "DBG_EM_NO_ATK", "DBG_NO_EST_CALL",
    "DBG_IN_ESP_TOOL", "DBG_TERM_TOOL", "DBG_WARN_LEVEL_LOW", "", "", "", "", "DGG_TIMER_STOP", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "DBG_ROOMJMP", "DBG_PROC_BAR",
    "DBG_SCA_VIEW", "DBG_OBA_VIEW", "DBG_SLOW_MODE", "DBG_NO_SCE_EXE", "DBG_SINGLE_DISK",
    "DBG_BUGCHECK_MODE", "DBG_NO_DEATH", "DBG_INF_BULLET", "DBG_NO_ENEMY", "DBG_BGM_STOP",
    "DBG_SE_STOP", "DBG_PL_LOCK_FOLLOW", "DBG_EM_NO_DEATH", "DBG_KAIOUKEN", "DBG_PAD_INFO",
    "DBG_UNDER_CONST", "DBG_EM_WEAK", "DBG_EM_LIFE_DISP", "DBG_SHADOW_LIGHT", "DBG_CAPTION_OFF",
    "DBG_TEST_MODE_CK", "DBG_LIGHT_ERR_CHECK", "DBG_EST_CALL_CHK", "DBG_GX_WARN_ALL",
    "DBG_GX_WARN_MIDIUM", "DBG_GX_WARN_SEVERE", "DBG_PL_NOHIT", "DBG_SE_ERR_ALL", "DBG_5e",
    "DBG_AV_TEST", "DBG_INF_BULLET2", "DBG_BATTLE_CAM", "DBG_62", "DBG_63", "DBG_SCISSOR_OFF",
    "DBG_LOG_OFF", "DBG_SCR_CHECK", "DBG_OBJ_SERVER", "DBG_START_ST2", "DBG_ERRORL_CK",
    "DBG_APP_USE_DBMEM", "DBG_REFRACT_CK", "DBG_EM_NO_DIE_FLAG", "DBG_START_ST3", "DBG_6e",
    "DBG_DOOR_SET_MODE", "DBG_EFF_NUM_DISP", "DBG_SET_HITMARK_ALL", "DBG_FOG_FAR_GREEN", "DBG_73",
    "DBG_NO_ETC_SET", "DBG_NO_DEATH2", "DBG_NO_PARASITE", "DBG_ESP_CHK", "DBG_NO_EVENT",
    "DBG_NO_LASER_LINE", "", "DBG_SHOP_FULL", "DBG_ADA_OMAKE_EV",
};

const char* spf_s[27] = {
    "SPF_KEY", "SPF_CAMERA", "SPF_EM", "SPF_PL", "SPF_ESP", "SPF_OBJ", "SPF_CTRL", "SPF_LIGHT",
    "SPF_SCE", "SPF_SCE_AT", "SPF_CCHG", "SPF_PL_CCHG", "SPF_NOTSUBSCR", "SPF_WATER",
    "SPF_SPECULAR", "SPF_EARTHQUAKE", "SPF_VIBRATION", "SPF_CINESCO", "SPF_MIST", "SPF_SUBCHAR",
    "SPF_SE", "SPF_EVT", "SPF_BLOCK", "SPF_ACTBTN", "SPF_DATAREAD_AT", "SPF_ID_SYSTEM",
    "SPF_ESP_AREA",
};

const char* sta_s[105] = {
    "STA_BG_OFF", "STA_PL_CHECK", "STA_PL_CHECK2", "STA_MOVIE_ON", "STA_CUTCHG", "STA_MOVIE2_ON",
    "STA_SSCRN_ENABLE", "STA_CINESCO", "STA_PL_FIRE", "STA_09", "STA_ACT_DONT_FIRE", "STA_DIEDEMO",
    "STA_BLUR", "STA_SUB_SCRN", "STA_CARD_ACCESS", "STA_PAD_SENSITIVE", "STA_10", "STA_PL_ACTION",
    "STA_PL_INVISIBLE", "STA_EVENT", "STA_ASHLEY_HIDE", "STA_CAM_SHOULDER", "STA_WATER_ALIVE",
    "STA_CAMERA", "STA_BLACKOUT", "STA_SCOPE_CAMERA", "STA_RIDE_GONDOLA", "STA_1b",
    "STA_PL_JUMP_OFF", "STA_MIRROR", "STA_SAND_ALIVE", "STA_SELF_SHADOW", "STA_PL_SE_FOOT",
    "STA_PL_SE_WHISTLE", "STA_SE_BURST", "STA_SUSPEND", "STA_TEX_RENDER", "STA_THERMO_GRAPH",
    "STA_CAMERA_IN_ROOM", "STA_NO_LIGHTMASK", "STA_PL_SPEAR_SET", "STA_PL_SWIM", "STA_PL_BOAT",
    "STA_WATER_CAMERA", "STA_PL_SWIM_CAMERA", "STA_PL_LADDER", "STA_CRITICAL", "STA_TAKEAWAY",
    "STA_PL_CATCHED", "STA_SHADOW_EQCOL", "STA_PL_CATCHHOLD", "STA_NEARCLIP_TOUCH",
    "STA_CAMERA_SET_ROOM", "STA_ROOM_RAIN", "STA_USE_CAST_SHADOW", "STA_PROC_SHD_TEX",
    "STA_ALPHA_DRAW2", "STA_SET_BG_COLOR", "STA_ESPGEN45_SET", "STA_EFFEM2D_TEXRND",
    "STA_SUB_LADDER", "STA_SUBCHAR_CTRL", "STA_ITEM_GET", "STA_LASERSITE_NOADD", "STA_PL_DONT_FIRE",
    "STA_PL_EM_ACTION", "STA_SUB_CATCHED", "STA_CUT_CHANGE", "STA_NO_FENCE", "STA_SSCRN_REQUEST",
    "STA_ESP_COMPULSION_NOSUSPEND", "STA_PL_MISS_SHOT", "STA_SUB_BULLDOZER", "STA_LIT_NO_UPDATE",
    "STA_MAP_DISABLE", "STA_USE_SHADOW_LIGHT", "STA_EVENT_SYSYTEM", "STA_INTO_SHOP",
    "STA_TIMER_NO_PAUSE", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "STA_SAVEDATA_NO_UPDATE", "STA_BEHIND_CAM", "STA_62", "STA_SCISSOR", "STA_SLOW",
    "STA_SUB_ASHLEY", "STA_BIG_MARKER", "", "STA_KLAUSER_TRANSFORM",
};

const char* cfg_s[6] = {
    "CFG_AIM_REVERSE", "CFG_WIDE_MODE", "CFG_LOCK_ON", "CFG_BONUS_GET", "CFG_VIBRATION",
    "CFG_KNIFE_MODE",
};

// The EXTRA page lists cfg_s names (num_names 2), yet the original .rodata carries these two
// strings between the CFG_ and SCF_ names with no table referencing them. An unused inline
// keeps them in that spot (GCC 2.95 emits string literals at parse time).
static inline const char* ext_name(int no)
{
    return no == 0 ? "EXT_COSTUME" : "EXT_HARD_MODE";
}

const char* scf_s[64] = {
    "SCF_KEY_ID_A_GET", "SCF_KEY_ID_B_GET", "SCF_KEY_ID_C_GET", "SCF_HOOK_STALKING_R10A",
    "SCF_R10E_BATTLE_END", "SCF_R104_ACT_STATUE", "SCF_R01E_TEST", "SCF_R100_TEST00",
    "SCF_R100_TEST01", "SCF_R100_TEST02", "SCF_R106_EVENT", "SCF_R117_ASHLEY_FIND",
    "SCF_R100_DOG_RUN", "SCF_ST1_SUB_MISSION", "SCF_R11C_BESIEGED_EVENT", "SCF_R201_EVENT00",
    "SCF_R108_PUZZLE_CLEAR", "SCF_R100_KILL_GANADE_1ST", "SCF_R101_ENTER", "SCF_R103_ENTER",
    "SCF_R106_ENTER", "SCF_R106_CONFINEED_WITH_LUIS", "SCF_R108_CHECK_DOOR", "SCF_R10C_GET_CREST",
    "SCF_NO_ASHLEY_DIST_CK", "SCF_R11C_BESIEGED_END_EVENT", "SCF_R103_MANURE_RECEPTACLE",
    "SCF_R103_ITEM_IN_MANURE_RECEPTACLE", "SCF_R11B_END_SALAMANDER", "SCF_ST1_MAP_DAY",
    "SCF_ST1_MAP_NIGHT", "SCF_ST2_MAP", "SCF_ST3_MAP", "SCF_R217_PUZZLE_CLEAR", "",
    "SCF_R206_ASHLEY_RESCUE", "SCF_R101_IMPRISON", "SCF_R103_OPEN_COVER",
    "SCF_R20D_END_OF_ASHLEY_PLAY", "SCF_ST1_NIGHT", "SCF_ST2_IN", "SCF_STOCK_ST1_DAY",
    "SCF_STOCK_ST1_NIGHT", "SCF_STOCK_ST2", "SCF_R108_OPERATOR", "SCF_R204_ASHLEY_SPLIT",
    "SCF_R11C_OPERATOR", "SCF_ST3_IN", "SCF_R119_DOOR_CLOSE", "SCF_R10C_TO_R10E",
    "SCF_ST1_NIGHT_LV_ADD", "SCF_R307_REGENERATER_APPEAR", "SCF_R316_TO_R30A_CUTBACK_EVENT",
    "SCF_R30D_ENTER", "", "", "", "", "SCF_R317_LEON_WOUND", "", "SCF_R22C_BONUS_1",
    "SCF_R22C_BONUS_2", "SCF_R22C_BONUS_3", "SCF_R22C_BONUS_4",
};

const char* itf_s[16] = {
    "ITF_DUMMY", "", "", "", "", "", "", "", "", "", "", "", "", "ITF_R101_IDCARD_A",
    "ITF_R10D_IDCARD_B", "ITF_R10B_IDCARD_C",
};

static const char* dpf_s[21] = {
    "DPF_EM", "DPF_PL", "DPF_SUBCHAR", "DPF_OBJ", "DPF_SCR", "DPF_ESP", "DPF_SHADOW", "DPF_WATER",
    "DPF_MIRROR", "DPF_CTRL", "DPF_CINESCO", "DPF_FILTER\t", "DPF_GLB_ILM", "DPF_CAST_SHADOW",
    "DPF_CLOTH", "DPF_COCKPIT", "DPF_SELF_SHADOW", "DPF_FOG", "DPF_ID_SYSTEM", "DPF_ACTBTN",
    "DPF_MESSAGE",
};

static const char* sys_s[29] = {
    "SYS_OMAKE_ADA_GAME", "SYS_OMAKE_ETC_GAME", "SYS_EXCEPTION", "SYS_RENDER_END", "SYS_SP_USED",
    "SYS_SOFT_RESET", "SYS_DATA_READ", "SYS_ROOMJUMP", "SYS_INVISIBLE", "SYS_DOOR_AFTER",
    "SYS_DOORDEMO", "SYS_TRANS_STOP", "SYS_CONTINUE", "SYS_SET_BLACK", "SYS_SN_PC_READ",
    "SYS_SN_PC_READ_TOOL", "SYS_HARD_RESET", "SYS_SCREEN_SHOT", "SYS_NEW_GAME", "SYS_TYPEWRITER",
    "SYS_SCISSOR_ON", "SYS_SCREEN_STOP", "SYS_CARD_ACCESS", "SYS_LOAD_GAME", "SYS_CONTINUE_AFTER",
    "SYS_START_EVT_SKIP", "SYS_HARD_MODE", "SYS_MESSAGE_INIT", "SYS_PUBLICITY_VER",
};
const char* kyf_s[1] = {""};

static FE_DATA fe_data[12] = {
    {"DEBUG", (u32*) ((u8*) &Global + OFS_DEBUG_FLG), 128, 0, dbg_s, 125},
    {"STOP", (u32*) ((u8*) &Global + OFS_STOP_FLG), 32, 0, spf_s, 27},
    {"STATUS", (u32*) ((u8*) &Global + OFS_STATUS_FLG), 128, 0, sta_s, 105},
    {"SYSTEM", (u32*) ((u8*) &Global + OFS_SYSTEM_FLG), 32, 0, sys_s, 29},
    {"ITEM_SET", (u32*) ((u8*) &Global + 0x519C), 128, 0, itf_s, 16},
    {"SCENARIO", (u32*) ((u8*) &Global + 0x51BC), 256, 0, scf_s, 64},
    {"KEY_LOCK", (u32*) ((u8*) &Global + 0x51DC), 64, 0, kyf_s, 1},
    {"ROOM", (u32*) ((u8*) &Global + 0x174), 128, 0, NULL, 0},
    {"ROOM_SAVE", NULL, 32, 0, NULL, 0},
    {"EXTRA", &SystemSave.extra_flg, 32, 0, cfg_s, 2},
    {"CONFIG", &SystemSave.config_flg, 32, 0, cfg_s, 6},
    {"DISP", (u32*) ((u8*) &Global + OFS_DISP_FLG), 32, 0, dpf_s, 21},
};

FE_WORK Test;

void FlagEdit()
{
    static void (*func_tbl[2])(FE_WORK*) = {move, die};

    Test.stop_bak = TOOL_FLAG(OFS_STOP_FLG);
    BitOn(TOOL_FLAG(OFS_STOP_FLG), ~0x4000);
    init(&Test);
    TaskSleep(1);
    while (1) {
        TOOL_FLAG(OFS_STOP_FLG) = Test.stop_bak;
        func_tbl[Test.mode](&Test);
        Test.stop_bak = TOOL_FLAG(OFS_STOP_FLG);
        BitOn(TOOL_FLAG(OFS_STOP_FLG), ~0x4000);
        TaskSleep(1);
    }
}

static void init(FE_WORK* t)
{
    Test.mode = 0;
    t->cursor = 0;
    t->page = 0;
    TOOL_FLAG(OFS_DEBUG_FLG) |= 0x80000000;
}

static void move(FE_WORK* t)
{
    JOY* joy = GetBugCheckController();
    FE_DATA* p;
    int i;
    int line;
    int a, b, c, d;
    u16 w;
    int bit = 0;
    u32 cur;
    s16 sh;

    if (joy->rep & JOY_RIGHT) {
        t->cursor++;
    }
    if (joy->rep & JOY_LEFT) {
        t->cursor--;
    }
    if (joy->rep & JOY_UP) {
        t->cursor -= 16;
    }
    if (joy->rep & JOY_DOWN) {
        t->cursor += 16;
    }
    if (joy->rep & 0x20000) {
        t->cursor++;
    } else if (joy->rep & 0x10000) {
        t->cursor--;
    } else if (joy->rep & 0x80000) {
        t->cursor -= 16;
    } else if (joy->rep & 0x40000) {
        t->cursor += 16;
    }
    if (t->cursor >= fe_data[t->page].bits) {
        t->cursor -= fe_data[t->page].bits;
        t->page++;
        if (t->page > 11) {
            t->page = 0;
        }
    }
    if (t->cursor < 0) {
        t->page--;
        if (t->page < 0) {
            t->page = 11;
        }
        t->cursor += fe_data[t->page].bits;
    }
    if (joy->rep & JOY_R) {
        t->cursor = 0;
        t->page++;
        if (t->page > 11) {
            t->page = 0;
        }
    }
    if (joy->rep & JOY_L) {
        t->cursor = 0;
        t->page--;
        if (t->page < 0) {
            t->page = 11;
        }
    }
    if (strcmp(fe_data[t->page].name, "ROOM_SAVE") == 0) {
        if (RoomData.getRoomSavePtr(G_ROOM_ID) != NULL) {
            fe_data[t->page].flags = (u32*) (RoomData.getRoomSavePtr(G_ROOM_ID) + 4);
        } else {
            fe_data[t->page].flags = NULL;
        }
    }
    p = &fe_data[t->page];
    if (fe_data[t->page].flags != NULL) {
        for (i = 0; i < p->bits / 16; i++) {
            w = ((u16*) p->flags)[i];
            a = BtoX(w >> 12);
            b = BtoX((w >> 8) & 0xF);
            c = BtoX((w >> 4) & 0xF);
            d = BtoX(w & 0xF);
            line = i / 4 + 6;
            eprintf(184, (i + line) * 14, 0, 0, "%04x %04x %04x %04x", a, b, c, d);
        }
        bit = t->cursor;
        cur = bit;
        {
            // COMPILER-DIFF: #17. The original keeps `cur & 0xF` in scratch r11, untied from x's
            // chain (`add r29,r11,r9`), so y is allocated first (r30) and x second (r29); ours ties
            // the mask into x (r30) and gives y r29. With the mask pinned, `cur >> 6` still loses r0
            // to the mask's dependents in sched1 (m has two consumers, u one) unless it is pinned
            // too. Both pins are codeless.
            register u32 m asm("r11");
            register u32 u asm("r0");
            u = cur >> 6;
            int y = ((cur >> 4) + u + 6) * 14;
            m = cur & 0xF;
            int x = (m + (m >> 2) + 23) * 8;
            eprintf(x, y, 2, 0, "%01x", CkBit(p->flags, cur));
        }
        bit = t->cursor;
        if (joy->trg & JOY_A) {
            sh = bit % 32;
            p->flags[bit / 32] ^= 0x80000000 >> sh;
        }
    }
    bit += p->base;
    eprintf(184, 56, 6, 0, "[%s]", p->name);
    eprintf(184, 70, 0, 0, " (0x%02x)", bit, bit);
    if ((u32) bit < p->num_names) {
        eprintf(264, 70, 4, 0, "%s", p->names[bit]);
    }
    if (joy->trg & JOY_B) {
        t->mode++;
    }
}

static void die(FE_WORK* t)
{
    TOOL_FLAG(OFS_DEBUG_FLG) &= 0x7FFFFFFF;
    TOOL_FLAG(OFS_STOP_FLG) = Test.stop_bak;
    TaskSignal(0);
    TaskExit();
}

int CkBit(u32* flags, u32 bit)
{
    flags += bit / 32;
    if (*flags & (0x80000000 >> (bit % 32))) {
        return 1;
    }
    return 0;
}

// The split object carries the 8-byte .sdata alignment of the following unit.
asm(".section .sdata; .balign 8");
