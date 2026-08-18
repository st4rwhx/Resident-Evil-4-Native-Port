#ifndef RE4_CORE_CLIENT_H
#define RE4_CORE_CLIENT_H

// The app's side of the core in a process of its own (re4_core_ipc.h,
// port/docs/32bit-kernprozess.md): functions with the signatures of the core's
// interface (re4_core.h, re4_gx_frame.h), which core_mode.cpp puts into its
// table of core calls instead of the core's own. The first re4ipc_reserve
// starts the core process.
#include "re4_card_files.h"
#include "re4_core.h"
#include "re4_gx_frame.h"

#include <stdint.h>
#include <string>
#include <vector>

namespace re4ipc {

struct Spawn {
    std::string program;     // the core process's program (libre4_coreproc.so, re4_coreproc)
    std::string libraryDir;  // LD_LIBRARY_PATH for it ("" none)
    std::string name;        // its argv[0] ("" the program)
    std::vector<std::string> wrapper;  // run through this program first (qemu-arm in the Linux check)
    int helloSeconds = 30;   // how long to wait for its hello
};

// Before the first re4ipc_reserve: how to start the core process.
void configure(const Spawn& spawn);
// The crash file (CoreBridge.setCrashFile): the core process writes its crashes there too.
void setCrashFd(int fd);
// Where its game thread is, into the crash file (CoreBridge.reportHang).
void reportHang(int seconds);
// "" while it runs or before it was started; else why it ended (a crash, its exit code).
std::string ended();
// "pid N" etc. for the report; "" before the start.
std::string describe();
// The core process's address map (its hello), for the report's address-map.txt.
std::string addressMap();
int pid();
// The app goes (checks): the socket closes, the core process ends; true when
// it ended within `waitMs`.
bool shutdown(int waitMs);

// Checks (RE4_CORE_IPC_CHECK=1 in the core process): chunks and state frames
// whose hash did not match what the core process copied, and how many were checked.
void checkCounts(unsigned* chunks, unsigned* chunkMismatches, unsigned* snapshots, unsigned* snapshotMismatches);
// What the core process passed on: pads, vsyncs, gyro values, state frames, chunks, memory pieces,
// sound frames, and of these the frames the core filled with silence.
void forwarded(unsigned out[8]);

}  // namespace re4ipc

// the table's functions (the same meaning as re4_core.h)
int re4ipc_mem1_init(void);
int re4ipc_reserve(const char* dataDir, const char* moduleDir);
int re4ipc_start(void);
int re4ipc_state(void);
const char* re4ipc_message(void);
void re4ipc_pad(const Re4CorePad* pad);
unsigned re4ipc_rumble(void);
int re4ipc_audio(int16_t* out, int frames);
int re4ipc_audio4(int16_t* out, int frames);
int re4ipc_audio_surround(void);
void re4ipc_audio_stats(unsigned* counters);
const Re4CoreSnapshot* re4ipc_acquire(void);
void re4ipc_release(const Re4CoreSnapshot* snapshot);
int re4ipc_log_tail(char* out, int size);
const Re4GxChunk* re4ipc_gx_acquire(void);
void re4ipc_gx_release(const Re4GxChunk* chunk);
void re4ipc_gx_detach(void);
void re4ipc_vsync(int64_t ns);
int re4ipc_pause(int on, int waitMs);
void re4ipc_debug_menu(void);
void re4ipc_set_dof(int on);
void re4ipc_set_letterbox(int off);
void re4ipc_touch(float u, float v);
void re4ipc_swipe(int dir);
void re4ipc_slash(float tilt);
void re4ipc_gyro(float yaw, float pitch);
int re4ipc_card_list(Re4CardFile* out, int max);
int re4ipc_card_free_blocks(void);
int re4ipc_card_file_name(int index, char* out, int size);
int re4ipc_card_export(int index, unsigned char* out, unsigned capacity, char* note, int noteSize);
int re4ipc_card_import(const unsigned char* gci, unsigned size, int replace, char* note, int noteSize);
unsigned re4ipc_frame_count(void);
int re4ipc_game_thread(void);
long re4ipc_free_at(void);

#endif
