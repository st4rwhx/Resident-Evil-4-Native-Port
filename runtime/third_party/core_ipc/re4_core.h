#ifndef RE4_CORE_H
#define RE4_CORE_H

/* Embedding interface of the game core (libre4_main): what the Android app
 * (RE4-Android-Decomp, core mode) calls through dlsym, and what the Linux
 * checks (re4_host --core-test) call directly. Plain C and fixed-width types
 * only: the app is compiled separately and must not see game headers.
 *
 * Startup order (the app's JNI_OnLoad, before any GL work):
 *   re4_core_reserve(data, modules)   MEM1 at 0x80000000, folders, address check
 *   re4_core_start()                  the game in its own thread
 * Every frame (any thread):
 *   re4_core_pad(&pad)                controller channel 0
 *   re4_core_rumble()                 its rumble motor
 * The app's audio thread:
 *   re4_core_audio(out, frames)       the sound, 32 kHz stereo
 *   re4_core_audio4(out, frames)      or as four channels (Dolby Pro Logic II)
 *   s = re4_core_acquire() ... re4_core_release(s)
 *                                     the newest published game frame
 * The game publishes one snapshot per game frame (PADRead), so a reader never
 * sees a half-written frame; the matrices are the parts' world matrices
 * (cParts::mat after partsWorldCalc), the camera is pG->Camera.param. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RE4_CORE_API_VERSION 10

enum Re4CoreState {
    RE4_CORE_IDLE = 0,     /* not started */
    RE4_CORE_RUNNING = 1,  /* the game thread runs */
    RE4_CORE_ENDED = 2,    /* re4_game_main returned (never on the GameCube) */
    RE4_CORE_FAILED = 3    /* could not start; re4_core_message says why */
};

/* GameCube controller state (PADStatus without the error byte); buttons as
 * the SDK defines them: LEFT 0x0001 RIGHT 0x0002 DOWN 0x0004 UP 0x0008 Z 0x0010
 * R 0x0020 L 0x0040 A 0x0100 B 0x0200 X 0x0400 Y 0x0800 START 0x1000.
 * Raw values like the hardware delivers them: the game's PadRead applies the
 * SDK's PADClamp (src/lib/Padclamp.c) itself, dead zones included (sticks 15,
 * triggers 30; main stick at most 72, C stick 59, triggers 150 afterwards). */
typedef struct Re4CorePad {
    uint16_t buttons;
    int8_t stickX, stickY;        /* main stick, about -100..100 like a real pad */
    int8_t substickX, substickY;  /* C stick, about -90..90 */
    uint8_t triggerL, triggerR;   /* analog triggers 0..255 */
} Re4CorePad;

/* Where a model's data lies on the disc: `innerOffset` bytes into archive
 * entry `entry` of the archive that starts `fileOffset` bytes into
 * files/<file> (a DVD container part or a plain archive file) and is
 * `archiveSize` bytes long. innerOffset > 0: nested data (a BIN inside an SMD
 * entry, a model inside a room block). fileOffset 0xFFFFFFFF: the archive is
 * Yz2-packed on the disc (room archives; the app reads its decoded copy).
 * file[0] == 0: unknown (not loaded from an archive the converter saw). */
typedef struct Re4CoreSource {
    char file[52];
    uint32_t fileOffset;
    uint32_t archiveSize;
    int32_t entry;
    uint32_t innerOffset;
} Re4CoreSource;

/* One cModelInfo of a model: its BIN (cModelData) and TPL. The addresses
 * identify the loaded data across frames. How the game places it
 * (trans.cpp commonModelTrans): drawn only with flags & 8; a rigid model
 * (one part, no weights) hangs on parts[rigidPart] with its vertices in that
 * part's space; a skinned one is posed by all parts; both times the info's
 * own matrix comes on top (identity, unless the game moves or hides it, like
 * the knife: cPlBody::setKnife scales it to 0). */
typedef struct Re4CoreModelInfo {
    uint32_t binAddress, tplAddress;
    Re4CoreSource bin, tpl;
    uint32_t flags;      /* cModelInfo::be_flag */
    int16_t rigidPart;   /* >= 0 rigid on that part, -1 skinned, -2 placed by the model matrix */
    uint16_t nMorph;     /* > 0: the vertex positions are Re4CoreSnapshot::morph[firstMorph ..] */
    float matrix[12];    /* cModelInfo::mat, row-major 3x4 */
    uint32_t firstMorph;
    /* How the game blends it (trans.cpp commonModelTrans): blendMode =
     * cModelInfo::blend_mode, the row of GXSetBlendMode it uses (0 alpha
     * blend, 1 add with source alpha, 2 add, 3 add times the destination
     * colour, 4 none: glass, light glows and beams use 1..3); color =
     * cModelInfo::color, whose alpha a fade scales; opacity =
     * cModelInfo::invisible_factor, the info's share of the fade. */
    uint8_t color[4];
    float opacity;
    uint8_t blendMode;
    uint8_t pad[3];
} Re4CoreModelInfo;

/* Shape (morph) animation: faces talk and blink by moving their vertices
 * (shape.cpp, applied in trans.cpp before skinning). For an info with a
 * running shape the bridge runs the game's ResetShape / CalculateShape_new and
 * passes the resulting positions: per vertex x y z and a pad, int16 in the
 * BIN's fixed point (cModelData::shift), host order, BIN vertex order. */
#define RE4_CORE_MAX_MORPH 32768

/* All infos of all models, in the game's list order (cModel::pModelInfo):
 * Leon alone has nine (body, costume, knife, face, head, hair, hands ...). */
#define RE4_CORE_MAX_INFOS 2048

/* A drawable model of the game (cModel): every live work of the enemy and
 * object managers, the player included. */
typedef struct Re4CoreModel {
    uint32_t address;      /* the cModel, stable while the work lives */
    uint32_t beFlag;       /* cUnit::be_flag; drawn when (beFlag & 6) == 6, unless HIDDEN (flags) */
    uint8_t manager;       /* 0 EmMgr (player, partner, enemies), 1 ObjMgr */
    uint8_t id, type;      /* cModel::id / type */
    uint8_t nInfos;        /* Re4CoreSnapshot::infos[firstInfo ..] */
    uint16_t firstMatrix;  /* into Re4CoreSnapshot::parts */
    uint16_t nParts;
    uint16_t firstInfo;    /* into Re4CoreSnapshot::infos */
    /* Scenery (ObjMgr id 2, cObjScr) made from an SMD work: which one,
     * scrollIndex = the work's index in the SMD, scrollBlock = -1 the room's
     * SMD, 0.. that scroll block's (cObj::blk). RE4_CORE_NO_SCROLL: not an SMD
     * work (an other model, or scenery a room script made, like r100's police
     * car: SetObjSmd). */
    uint16_t scrollIndex;
    int16_t scrollBlock;
    uint16_t flags;        /* RE4_CORE_MODEL_* */
    float matrix[12];      /* cModel::mat, row-major 3x4 (world placement) */
    /* How the game draws it (trans.cpp ModelRender / commonModelTrans):
     * opacity = invisible_factor * invisible_factor2 (0: not drawn at all,
     * HIDDEN; below 1: faded, each info's colour alpha times this); zMode =
     * cModel::z_mode (0 depth test and write, 1 test only, 2 neither);
     * cullMode = cModel::CullMode (0 the usual single side, 1 the other side,
     * 2 both sides); otType = cModel::ot_type (its ordering-table class). */
    float opacity;
    uint8_t zMode, cullMode, otType, pad;
} Re4CoreModel;
#define RE4_CORE_NO_SCROLL 0xFFFF
/* Re4CoreModel::flags. HIDDEN: the game does not draw the model this frame
 * although its be_flag would allow it (trans.cpp emTrans / objTrans /
 * ModelTrans): hidden by a display switch (pG->Disp_flg: DPF_PL the player,
 * DPF_SUBCHAR the partner, DPF_EM the other enemies, DPF_SCR scenery, DPF_OBJ
 * the other objects), held while an event or close-up runs (STA_SUSPEND)
 * without be_flag 0x800 (setNoSuspend), or faded out completely (opacity 0). In r100's opening event that is the
 * player, the parked police car and the two officers of the room: the event
 * shows its own models instead. Drawn = (beFlag & 6) == 6 and not HIDDEN. */
#define RE4_CORE_MODEL_HIDDEN 1

/* Pose of one cParts: world matrix and bind position (the translation that
 * cParts::lt_inv_mat removes: palette = mat * lt_inv_mat). */
typedef struct Re4CorePart {
    float matrix[12];  /* row-major 3x4 */
    float bind[3];
} Re4CorePart;

#define RE4_CORE_MAX_MODELS 1024

/* Re4CoreSnapshot::screen */
#define RE4_CORE_SCREEN_SUB 1       /* a sub screen is open (inventory, map, radio ...): drawn by
                                       the game in 2D; the camera fields hold the world camera
                                       (CamCtrl) instead of the sub screen's own */
#define RE4_CORE_SCREEN_EVENT 2     /* an event (cutscene) runs */
#define RE4_CORE_SCREEN_DEBUGCAM 4  /* the debug camera (pad 2) is on */
#define RE4_CORE_MAX_PARTS 8192

typedef struct Re4CoreSnapshot {
    uint32_t apiVersion;   /* RE4_CORE_API_VERSION */
    uint32_t serial;       /* +1 per published frame */
    uint32_t frame;        /* PADRead calls so far */
    uint8_t rno[4];        /* pG->Rno0..3: game task step (2 room init, 3 main loop) */
    uint16_t room;         /* pG->room_id (stage << 8 | room) */
    int8_t disc;           /* inserted disc 0/1, -1 none */
    uint8_t cameraValid;
    float cameraEye[3];    /* pG->Camera.param.Campos */
    float cameraTarget[3]; /* .Target */
    float cameraRoll;      /* .Roll, radians */
    float cameraFovy;      /* .Fovy, degrees (C_MTXPerspective) */
    uint16_t nModels;
    uint16_t nParts;
    uint32_t droppedModels;  /* models beyond the budget */
    uint32_t screen;         /* RE4_CORE_SCREEN_* */
    uint16_t nInfos;
    uint16_t reserved;
    uint32_t droppedInfos;   /* infos beyond the budget (or beyond 255 of one model) */
    uint32_t nMorph;
    uint32_t droppedMorph;   /* morphed infos beyond the budget (drawn unmorphed) */
    Re4CoreModel models[RE4_CORE_MAX_MODELS];
    Re4CoreModelInfo infos[RE4_CORE_MAX_INFOS];
    Re4CorePart parts[RE4_CORE_MAX_PARTS];
    int16_t morph[RE4_CORE_MAX_MORPH][4];
} Re4CoreSnapshot;

/* Reserves MEM1 (call as early as possible), sets the disc folder (layout:
 * runtime/dvd.cpp) and the module library folder ("" = load by library name,
 * as an APK with uncompressed libraries needs) and checks that the game
 * library lies above 0x80000000. 1 = ready, 0 = not (see re4_core_message). */
int re4_core_reserve(const char* dataDir, const char* moduleDir);
/* Starts re4_game_main in a thread of its own (8 MB stack). 1 = started. */
int re4_core_start(void);
int re4_core_state(void);
/* Last start problem or state, for the screen. */
const char* re4_core_message(void);
/* Controller channel 0 from now on (replaces the test script). */
void re4_core_pad(const Re4CorePad* pad);
/* The rumble motor of channel 0 as the game drives it: bits 0-3 = motor on in
 * the last four frames (bit 0 the newest), bits 8-31 = frames so far (the app
 * stops vibrating when they stand still). */
unsigned re4_core_rumble(void);
/* A vsync of the device's display at `ns` (CLOCK_MONOTONIC, e.g. the
 * Choreographer's frame time), any thread, every vsync while the game is
 * shown: the game's retraces follow a display near 60 Hz (runtime/vi.cpp).
 * Optional for the app (an older core does not have it). */
void re4_core_vsync(int64_t ns);
/* The game's sound for the device: `frames` stereo frames (left, right) at
 * 32 kHz, 16 bit; always fills `out` (silence while the game has none) and
 * returns how many came from the game. Any thread (the app's audio thread);
 * the core keeps about 60 ms ahead and matches the device's clock. */
int re4_core_audio(int16_t* out, int frames);
/* The same sound as four channels: front left, front right, rear left, rear
 * right per frame. In the game's Dolby Pro Logic II mode (AUDIO SETUP) the rear
 * is the game's own; otherwise it is silent and the front is the stereo. The
 * same ring position and clock matching as re4_core_audio: use one of the two.
 * Optional for the app (cores since 20.10.2026). */
int re4_core_audio4(int16_t* out, int frames);
/* Both in one read, 6 values per frame: the stereo of re4_core_audio, then the
 * four channels of re4_core_audio4 (the core in a process of its own hands
 * both to the app, which plays one of them; host/coreproc.cpp). */
int re4_core_audio6(int16_t* out, int frames);
/* 1 while the game plays in Dolby Pro Logic II (the four channels carry a
 * rear), else 0. */
int re4_core_audio_surround(void);
/* Sound counters: [0] AI blocks so far (5 ms each), [1] frames waiting,
 * [2] blocks lost while the app did not read, [3] reads that ran dry,
 * [4] skips after a pause of the app. */
void re4_core_audio_stats(unsigned counters[5]);
/* The newest snapshot (0 before the first frame); stays valid and unchanged
 * until re4_core_release. One reader at a time. */
const Re4CoreSnapshot* re4_core_acquire(void);
void re4_core_release(const Re4CoreSnapshot* snapshot);
/* The last log lines, newest last, '\n'-separated (diagnostic overlay). */
int re4_core_log_tail(char* out, int size);
/* How often the player lost life so far (runtime/state_trace.cpp): the app
 * records the pictures after a hit for the analysis (RE4_GX_DUMP_ON_HIT). */
unsigned re4_core_player_hits(void);

/* The app's menu (M11). on = 1: the game stops at its next frame (it waits
 * in PADRead until on = 0); waits up to waitMs for that. 1 when the game
 * stands. Optional for the app (cores since M11), like those below. */
int re4_core_pause(int on, int waitMs);
/* The game's debug menu at its next frame (SELECT held, patch 0061). */
void re4_core_debug_menu(void);
/* The depth of field (filter01.cpp, patch 0065): on = 0 draws no blur in
 * front of / behind the focus, as the app's menu switch asks; on by
 * default. With it on, frames without a focus of the game get the app's
 * gameplay autofocus while the player is free (runtime/dof.cpp, BP 0x5A in
 * the GX stream). re4_dof_enabled is what the patched game code reads. */
void re4_core_set_dof(int on);
int re4_dof_enabled(void);
/* The picture format "4:3 without bars" (cores since 22.10.2026): off = 1
 * leaves out the game's letterbox (the scissor of Render to rows 56..392 and the
 * bars of cinesco.cpp Draw_cinesco, patch 0105) while the player plays: not in
 * events or sub screens. re4_letterbox_off is the switch, re4_letterbox_open
 * (runtime/picture.cpp) what the patched game code reads. */
void re4_core_set_letterbox(int off);
int re4_letterbox_off(void);
/* A tap on the picture (cores since 22.10.2026; runtime/menu_touch.cpp): u
 * across the game's 512 x 448 picture, v down it, both 0..1. Used at the next
 * game frame while a sub screen is open (tabs, the attache case and its
 * command menu), ignored elsewhere. Any thread. */
void re4_core_touch(float u, float v);
/* A swipe across the picture (cores since 22.10.2026): dir 1 the next tab of
 * the sub screen (a swipe to the left), -1 the one before, nothing beyond the
 * first and the last; ignored outside the normal inventory. Any thread. */
void re4_core_swipe(int dir);
/* A swipe while playing (cores since 22.10.2026, knife_swipe.cpp): Leon draws
 * the knife and slashes; tilt -1..1 how steep the swipe went (1 straight up,
 * -1 straight down) moves the slash up or down. Ignored in sub screens, events
 * and where L would not draw the knife. Any thread. */
void re4_core_slash(float tilt);
/* Game frames so far (one per PADRead) and the game's host thread id: the
 * app notices a game that stands without a pause and samples where it is. */
unsigned re4_core_frame_count(void);
int re4_core_game_thread(void);
/* The memory card for the menu (runtime/re4_card_files.h, Re4CardFile):
 * only while the game stands and does not use the card, RE4_CARD_BUSY
 * otherwise. List, free blocks, the .gci name Dolphin gives a file, export
 * (into `out`, its size) and import (replace: over a file of that name). */
struct Re4CardFile;
int re4_core_card_list(struct Re4CardFile* out, int max);
int re4_core_card_free_blocks(void);
int re4_core_card_file_name(int index, char* out, int size);
int re4_core_card_export(int index, unsigned char* out, unsigned capacity, char* note, int noteSize);
int re4_core_card_import(const unsigned char* gci, unsigned size, int replace, char* note, int noteSize);

/* Tests: like re4_core_start with another boot thread entry. */
int re4_core_start_entry(int (*entry)(void));

/* Internal: fills `out` from the game state (runtime/core_bridge.cpp, game
 * flags); called once per game frame by the core's frame hook. */
void re4_bridge_capture(Re4CoreSnapshot* out);
/* Internal: the faces' shapes in the snapshot (morph) on or off; off unless RE4_VIEW=bridge or
 * RE4_BRIDGE_SHAPES=1 (only the bridge view draws them). */
void re4_bridge_shapes(int on);
/* Internal (formats.cpp): the disc source of a converted archive entry that
 * starts at `addr`; 0 when no archive the converter saw holds it. */
int re4_formats_locate(const void* addr, Re4CoreSource* out);

#ifdef __cplusplus
}
#endif

#endif
