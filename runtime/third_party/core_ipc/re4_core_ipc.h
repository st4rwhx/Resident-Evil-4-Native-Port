#ifndef RE4_CORE_IPC_H
#define RE4_CORE_IPC_H

// The game core in a process of its own (port/docs/32bit-kernprozess.md):
// what the app (core_mode.cpp through re4_core_client.h) and the core process
// (host/coreproc.cpp) share. A 32-bit device whose Java heap lies on MEM1
// (ART's "dalvik-main space" at 0x80000000, Android 14) cannot hold the core
// in the app's process; the core then runs in a program without Java, and the
// app's table of core calls (core_mode.cpp CoreApi) goes to it:
//
//   shared memory   input (controller and vsync queues, gyro), mirrors the
//                   core process writes every game frame (rumble, frames),
//                   the state frames (three buffers), the sound ring, and
//                   three GX slots with a memory piece of their own each
//   the socket      commands and replies (start, state, pause, memory card,
//                   log), new GX memory pieces (their file descriptor), the
//                   end of either side (the socket closes)
//
// Both processes are the same program width (the app's ABI), so the
// structures below have the same layout on both sides. Plain C++ and the
// core's interface headers only: the app is compiled separately.
#include "re4_card_files.h"
#include "re4_core.h"
#include "re4_gx_frame.h"

#include <atomic>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace re4ipc {

const uint32_t kMagic = 0x43344552;  // "RE4C"
const uint32_t kVersion = 3;
// the socket in the core process: file 3, inherited
const int kSocketFd = 3;

// --- messages ------------------------------------------------------------------
//
// A header, then `length` bytes. A file descriptor travels with the header
// (SCM_RIGHTS). Replies carry the request's id; requests without a reply have
// id 0. Reply payloads: `b` bytes of text (with its 0), then data.
enum MsgType : uint32_t {
    // core process -> app
    kHello = 1,    // a = pid, b = text length, c = control size; text: problem ("" = ready),
                   // then the address map; fd: the control block
    kSegment = 2,  // a = slot, b = generation, c = size; fd: the slot's new memory piece
    kReply = 3,    // a = result, b = text length; payload: text, data
    // app -> core process
    kStart = 10,       // payload: data folder, 0, module folder, 0 -> a = 1 running; text: message
    kState = 11,       // -> a = Re4CoreState; text: re4_core_message
    kPause = 12,       // a = on, b = wait ms -> a = stands
    kDebugMenu = 13,   // (no reply)
    kSetDof = 14,      // a = on (no reply)
    kLogTail = 15,     // a = size -> text
    kCardList = 16,    // a = max -> a = n or error; data: n Re4CardFile
    kCardFree = 17,    // -> a = blocks or error
    kCardName = 18,    // a = index, b = size -> a = result; text: name
    kCardExport = 19,  // a = index, b = capacity, c = note size -> a = bytes or error; text: note; data: .gci
    kCardImport = 20,  // a = replace, c = note size; payload: .gci -> a = result; text: note
    kHang = 21,        // a = seconds: where the game thread is, into the crash file (no reply)
    kCrashFile = 22,   // fd: the crash file (no reply)
    kSetLetterbox = 23,  // a = off (no reply)
    kTouch = 24,         // a, b = the tap's u, v as float bits (no reply)
    kSwipe = 25,         // a = 1 next tab, -1 the one before (no reply)
    kSlash = 26,         // a = the swipe's tilt as float bits (no reply)
};

struct MsgHeader {
    uint32_t type;
    uint32_t id;
    int32_t a, b, c;
    uint32_t length;
};

// --- shared memory ---------------------------------------------------------------

const int kGxSlots = 3;
const uint32_t kPadQueue = 64;    // a tap between two game frames is not lost (re4_pad_live)
const uint32_t kVsyncQueue = 16;
const uint32_t kAudioFrames = 4096;  // the sound ring (a power of two)
const uint32_t kAudioMaxChannels = 8;  // the ring holds 6: the stereo, then the four channels (DPL2)
const uint32_t kSnapshotNew = 4;     // Control::snapshotMiddle: the buffer is newer than the reader's

// The GX slot's states. The core process fills a FREE slot (FILLING) and
// publishes it (READY); the app takes it (READING) and gives it back
// (RELEASED); the core process then releases the core's chunk (FREE).
enum SlotState : uint32_t { kSlotFree, kSlotFilling, kSlotReady, kSlotReading, kSlotReleased };

// Re4GxChunk without its pointers (offsets into the slot's memory piece instead)
struct GxInfo {
    uint32_t version, sequence, flags;
    uint32_t opWords, vertexBytes, indexCount, blobBytes;
    uint32_t xfbAddress, xfbWidth, xfbHeight;
    uint32_t draws, vertexCount, textureBytes;
};

struct alignas(64) GxSlot {
    std::atomic<uint32_t> state;
    uint32_t epoch;       // the app's detach count when the core process took the chunk
    uint32_t order;       // publication order
    uint32_t generation;  // of the memory piece holding the data (kSegment)
    uint32_t opOffset, vertexOffset, indexOffset, blobOffset;
    GxInfo info;
    uint32_t hashLo, hashHi;  // RE4_CORE_IPC_CHECK=1: of the core's chunk (checks)
};

// A futex word: the app rings, a thread of the core process waits.
struct alignas(64) Bell {
    std::atomic<uint32_t> seq;
};

struct Control {
    uint32_t magic, version, size;
    uint32_t pid;
    uint32_t snapshotOffset;  // three Re4CoreSnapshot from here
    uint32_t audioOffset;     // kAudioFrames * audioChannels int16 from here
    // 6 per frame: the stereo (re4_core_audio), then front left, front right, rear left, rear
    // right (re4_core_audio4, Dolby Pro Logic II): one read of the core for both, the app plays one
    uint32_t audioChannels, audioRate;
    uint32_t check;           // RE4_CORE_IPC_CHECK in the core process: hashes filled in

    Bell inputBell, gxBell, audioBell;
    Bell audioDone;  // the core process wrote sound (the app waits for it when the ring is short)

    // app -> core process (one producer each, the app holds a lock)
    alignas(64) std::atomic<uint32_t> padHead, padTail;
    Re4CorePad pads[kPadQueue];
    std::atomic<uint32_t> vsyncHead, vsyncTail;
    int64_t vsyncs[kVsyncQueue];
    std::atomic<uint32_t> gyroSeq, gyroYaw, gyroPitch;  // a sequence lock; the floats as bits

    // core process -> app, written once per game frame (its frame hook)
    alignas(64) std::atomic<uint32_t> rumble, frames;
    std::atomic<int32_t> freeAt;
    std::atomic<uint32_t> audioStats[5];
    std::atomic<uint32_t> audioSurround;  // re4_core_audio_surround: the game plays in Dolby Pro Logic II
    // the state frames: the index of the middle buffer | kSnapshotNew
    std::atomic<uint32_t> snapshotMiddle;
    uint32_t snapshotHashLo[3], snapshotHashHi[3];  // RE4_CORE_IPC_CHECK=1

    // GX: the app's detach count, and the count and time (ms) of its last poll
    alignas(64) std::atomic<uint32_t> appEpoch, pollEpoch, pollMs;
    GxSlot slots[kGxSlots];

    // the sound ring: the core process writes, the app reads. audioWant: the ring position the
    // app needs now (its read before last plus the frames of this read): an AudioTrack takes
    // its frames in bursts, several reads at once, more than the ring keeps ahead
    alignas(64) std::atomic<uint32_t> audioWrite, audioRead, audioWant;

    // what the core process passed on (the report, the checks)
    alignas(64) std::atomic<uint32_t> padsForwarded, vsyncsForwarded, gyroForwarded, snapshotsCopied;
    std::atomic<uint32_t> chunksCopied, chunksDropped, segmentsMade, audioForwarded;
    std::atomic<uint32_t> audioCoreSilence;  // frames the core filled with silence (its ring ran dry)
};

static_assert(std::atomic<uint32_t>::is_always_lock_free, "shared atomics must be lock-free");
static_assert(std::atomic<int32_t>::is_always_lock_free, "shared atomics must be lock-free");

// The bytes of the control block (header, three state frames, the sound ring).
size_t controlBytes();
void controlInit(Control* c, uint32_t pid);

// A shared memory piece of `size` bytes (memfd_create, on Android else
// ASharedMemory); -1 when there is none. Its pages are taken when touched.
int shmCreate(const char* name, size_t size);
void* shmMap(int fd, size_t size);

// The bell: ring wakes the waiter; wait returns when the bell rang since
// `seen` (the value read before looking for work) or after timeoutMs.
void ring(Bell& bell);
void waitBell(Bell& bell, uint32_t seen, int timeoutMs);

// Messages on a stream socket; `fd` >= 0 travels with the header. The caller
// holds the socket's write lock. False when the other side is gone.
bool sendMsg(int sock, const MsgHeader& header, const void* payload, int fd = -1);
// One message; *fd gets a file descriptor that came with it (else -1).
bool receiveMsg(int sock, MsgHeader& header, std::vector<char>& payload, int* fd);

// FNV-1a over a GX chunk's data and its fields (RE4_CORE_IPC_CHECK)
uint64_t hashChunk(const GxInfo& info, const void* ops, const void* vertices, const void* indices,
                   const void* blob);
// over the parts of a state frame the app reads (its counts)
uint64_t hashSnapshot(const Re4CoreSnapshot& s);
// copies those parts only (a state frame is 1.3 MB, a game frame uses a fraction)
void copySnapshot(Re4CoreSnapshot& to, const Re4CoreSnapshot& from);

void fillInfo(GxInfo& info, const Re4GxChunk& c);
uint32_t nowMs();

}  // namespace re4ipc

#endif
