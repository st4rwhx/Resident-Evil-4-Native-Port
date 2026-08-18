// The app's side of the core in a process of its own (re4_core_client.h,
// port/docs/32bit-kernprozess.md). The calls of the core's interface go to
// the core process: the fast ones through the shared memory (re4_core_ipc.h),
// the rare ones as commands on the socket, whose replies a reader thread
// hands to the caller waiting for them. Compiled into the app (core mode) and
// into the Linux check re4_core_remote_test.
#include "re4_core_client.h"
#include "re4_core_ipc.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <errno.h>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <set>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#if defined(__ANDROID__)
#include <android/log.h>
#endif

extern char** environ;

namespace re4ipc {

namespace {

void note(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void note(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#if defined(__ANDROID__)
    __android_log_vprint(ANDROID_LOG_INFO, "RE4Core", fmt, ap);
#else
    fprintf(stderr, "[re4 app] ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
#endif
    va_end(ap);
}

struct Reply {
    MsgHeader header;
    std::vector<char> payload;
    const char* text() const { return header.b > 0 && payload.size() >= (size_t) header.b ? payload.data() : ""; }
    const char* data() const { return payload.data() + (header.b > 0 ? header.b : 0); }
    size_t dataBytes() const { return payload.size() - (size_t) (header.b > 0 ? header.b : 0); }
};

struct Pending {
    uint32_t generation;
    int fd;
    uint32_t size;
};

struct Client {
    Spawn spawn;
    std::mutex lock;  // all below, except where noted
    std::condition_variable changed;
    bool tried;       // the start was tried
    bool alive;       // the core process runs (its hello came, the socket is open)
    bool hello;
    int sock = -1;
    pid_t child = -1;
    std::string problem;  // why it did not start
    std::string endReason;
    std::string map;
    Control* control;
    std::map<uint32_t, Reply> replies;
    std::set<uint32_t> waiting;  // the commands whose callers wait for the reply
    uint32_t nextId = 1;
    std::vector<Pending> segments[kGxSlots];
    int crashFd = -1;
    std::string dataDir, moduleDir;
    std::mutex sendLock;  // the socket's writes
} g;

// the GL thread's (re4ipc_gx_*): the slots' memory pieces as mapped here
struct Mapped {
    uint32_t generation = 0;
    char* base = nullptr;
    size_t size = 0;
};
std::mutex gGxLock;
Mapped gMapped[kGxSlots];
Re4GxChunk gViews[kGxSlots];
bool gWantReset = true;  // after a detach: chunks up to the next RESET are left out
std::atomic<unsigned> gChunksChecked{0}, gChunkMismatches{0}, gSnapshotsChecked{0}, gSnapshotMismatches{0};
std::atomic<bool> gEnded{false};  // the core process ended: nobody to wait for

// the inputs' producers (any app thread)
std::mutex gPadLock, gVsyncLock, gGyroLock;
// the state frames' reader
std::mutex gSnapLock;
uint32_t gFront = 2;
bool gHaveFront, gHeld;

Control* control()
{
    std::lock_guard<std::mutex> l(g.lock);
    return g.hello ? g.control : nullptr;
}

std::string describeStatus(int status)
{
    char text[160];
    if (WIFSIGNALED(status)) {
        snprintf(text, sizeof(text), "signal %d (%s)", WTERMSIG(status), strsignal(WTERMSIG(status)));
    } else if (WIFEXITED(status)) {
        snprintf(text, sizeof(text), "exit code %d", WEXITSTATUS(status));
    } else {
        snprintf(text, sizeof(text), "status 0x%x", status);
    }
    return text;
}

// The reader thread: hello, replies and new memory pieces; at the end the
// core process's exit status.
void readerMain(int sock, pid_t child)
{
    for (;;) {
        MsgHeader h;
        std::vector<char> payload;
        int fd = -1;
        if (!receiveMsg(sock, h, payload, &fd)) {
            break;
        }
        std::unique_lock<std::mutex> l(g.lock);
        if (h.type == kHello) {
            const char* text = h.b > 0 && payload.size() >= (size_t) h.b ? payload.data() : "";
            g.problem = text;
            g.map.assign(payload.data() + (h.b > 0 ? h.b : 0), payload.size() - (size_t) (h.b > 0 ? h.b : 0));
            if (g.problem.empty() && fd >= 0) {
                Control* c = (Control*) shmMap(fd, (size_t) h.c);
                if (!c || c->magic != kMagic || c->version != kVersion || c->size != controlBytes() ||
                    (size_t) h.c != controlBytes()) {
                    g.problem = "the core process's shared memory does not fit this app (another version?)";
                    if (c) {
                        munmap(c, (size_t) h.c);
                    }
                } else {
                    g.control = c;
                    g.hello = true;
                }
            } else if (g.problem.empty()) {
                g.problem = "the core process sent no shared memory";
            }
        } else if (h.type == kSegment && h.a >= 0 && h.a < kGxSlots && fd >= 0) {
            g.segments[h.a].push_back({(uint32_t) h.b, fd, (uint32_t) h.c});
            fd = -1;
        } else if (h.type == kReply && g.waiting.count(h.id)) {
            Reply& r = g.replies[h.id];
            r.header = h;
            r.payload.swap(payload);
        }
        if (fd >= 0) {
            close(fd);
        }
        g.changed.notify_all();
    }
    // the socket closed: the core process ended (or the app shut it down)
    int status = 0;
    pid_t got;
    do {
        got = waitpid(child, &status, 0);
    } while (got < 0 && errno == EINTR);
    std::string reason = got == child ? describeStatus(status) : std::string("status unknown");
    gEnded.store(true);
    {
        std::lock_guard<std::mutex> l(g.lock);
        g.alive = false;
        g.endReason = "the game core process ended: " + reason;
        note("%s", g.endReason.c_str());
        g.changed.notify_all();
    }
}

// Starts the core process and waits for its hello (g.lock held).
bool startLocked(std::unique_lock<std::mutex>& l)
{
    g.tried = true;
    if (g.spawn.program.empty()) {
        g.problem = "no core process program configured";
        return false;
    }
    // the program itself (also when a wrapper runs it): a clear reason when it is missing
    if (access(g.spawn.program.c_str(), g.spawn.wrapper.empty() ? X_OK : R_OK) != 0) {
        g.problem = std::string("the core process does not start (") + g.spawn.program + "): " + strerror(errno);
        note("%s", g.problem.c_str());
        return false;
    }
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0) {
        g.problem = std::string("no socket for the core process: ") + strerror(errno);
        return false;
    }
    // argv: [wrapper ...] program --parent <pid>
    std::vector<std::string> args;
    for (const std::string& w : g.spawn.wrapper) {
        args.push_back(w);
    }
    args.push_back(g.spawn.program);
    args.push_back("--parent");
    args.push_back(std::to_string((long) getpid()));
    const std::string path = args[0];
    if (g.spawn.wrapper.empty() && !g.spawn.name.empty()) {
        args[0] = g.spawn.name;
    }
    // the app's environment (the RE4_* settings included) with the library folder
    std::vector<std::string> env;
    for (char** e = environ; e && *e; e++) {
        if (strncmp(*e, "LD_LIBRARY_PATH=", 16) != 0 && strncmp(*e, "RE4_MEM1_PLACEHOLDER=", 21) != 0) {
            env.push_back(*e);
        }
    }
    if (!g.spawn.libraryDir.empty()) {
        env.push_back("LD_LIBRARY_PATH=" + g.spawn.libraryDir);
    }
    std::vector<char*> argv, envp;
    for (std::string& a : args) {
        argv.push_back(&a[0]);
    }
    argv.push_back(nullptr);
    for (std::string& e : env) {
        envp.push_back(&e[0]);
    }
    envp.push_back(nullptr);
    // vfork: no copy of the Java process; the child only places the socket and
    // runs the program (the fd table is its own, the memory the parent's)
    volatile int execError = 0;
    const int childEnd = sv[1];
    pid_t pid = vfork();
    if (pid == 0) {
        if (childEnd == kSocketFd) {
            fcntl(kSocketFd, F_SETFD, 0);
        } else {
            dup2(childEnd, kSocketFd);  // without close-on-exec
        }
        execve(path.c_str(), argv.data(), envp.data());
        execError = errno ? errno : ENOEXEC;
        _exit(127);
    }
    close(childEnd);
    if (pid < 0 || execError) {
        const int e = pid < 0 ? errno : execError;
        if (pid > 0) {
            waitpid(pid, nullptr, 0);
        }
        close(sv[0]);
        g.problem = std::string("the core process does not start (") + path + "): " + strerror(e);
        return false;
    }
    g.sock = sv[0];
    g.child = pid;
    g.alive = true;
    note("core process started: pid %d, %s", (int) pid, path.c_str());
    std::thread(readerMain, sv[0], pid).detach();
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(g.spawn.helloSeconds);
    g.changed.wait_until(l, until, [] { return g.hello || !g.problem.empty() || !g.alive; });
    if (!g.hello) {
        if (g.problem.empty()) {
            g.problem = g.alive ? "the core process did not answer" : g.endReason;
        }
        note("core process: %s", g.problem.c_str());
        if (g.alive) {
            ::shutdown(g.sock, SHUT_RDWR);  // it ends with the socket
        }
        return false;
    }
    if (g.crashFd >= 0) {
        std::lock_guard<std::mutex> s(g.sendLock);
        MsgHeader h = {kCrashFile, 0, 0, 0, 0, 0};
        sendMsg(g.sock, h, nullptr, g.crashFd);
    }
    note("core process ready (pid %d)", (int) pid);
    return true;
}

// A command and its reply; false when the core process is gone or did not answer in time.
bool call(MsgType type, int a, int b, int c, const void* payload, uint32_t length, int timeoutMs, Reply* out)
{
    uint32_t id;
    int sock;
    {
        std::lock_guard<std::mutex> l(g.lock);
        if (!g.hello || !g.alive) {
            return false;
        }
        id = g.nextId++;
        if (g.nextId == 0) {
            g.nextId = 1;
        }
        sock = g.sock;
        g.waiting.insert(id);
    }
    bool sent;
    {
        std::lock_guard<std::mutex> s(g.sendLock);
        MsgHeader h = {(uint32_t) type, id, a, b, c, length};
        sent = sendMsg(sock, h, payload);
    }
    std::unique_lock<std::mutex> l(g.lock);
    struct Done {
        uint32_t id;
        ~Done() { g.waiting.erase(id); }
    } done = {id};
    if (!sent) {
        return false;
    }
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    const bool got = g.changed.wait_until(l, until, [&] { return g.replies.count(id) || !g.alive; });
    auto it = g.replies.find(id);
    if (!got || it == g.replies.end()) {
        if (g.alive) {
            note("core process: no reply to command %u in %d ms", (unsigned) type, timeoutMs);
        }
        return false;
    }
    if (out) {
        *out = std::move(it->second);
    }
    g.replies.erase(it);
    return true;
}

// A command without a reply.
void post(MsgType type, int a, int b, int c, int fd = -1)
{
    int sock;
    {
        std::lock_guard<std::mutex> l(g.lock);
        if (!g.hello || !g.alive) {
            return;
        }
        sock = g.sock;
    }
    std::lock_guard<std::mutex> s(g.sendLock);
    MsgHeader h = {(uint32_t) type, 0, a, b, c, 0};
    sendMsg(sock, h, nullptr, fd);
}

void copyText(char* out, int size, const char* text)
{
    if (out && size > 0) {
        snprintf(out, (size_t) size, "%s", text ? text : "");
    }
}

// The slot's memory piece of that generation, mapped here (GL thread).
char* segment(int slot, uint32_t generation)
{
    Mapped& m = gMapped[slot];
    if (m.base && m.generation == generation) {
        return m.base;
    }
    Pending p = {0, -1, 0};
    {
        std::unique_lock<std::mutex> l(g.lock);
        // the core process sends the piece before it marks the slot ready; the
        // reader thread may still be on its way
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        g.changed.wait_until(l, until, [&] {
            for (const Pending& q : g.segments[slot]) {
                if (q.generation == generation) {
                    return true;
                }
            }
            return !g.alive;
        });
        std::vector<Pending>& list = g.segments[slot];
        for (size_t i = 0; i < list.size();) {
            if (list[i].generation == generation) {
                p = list[i];
            }
            if ((int32_t) (list[i].generation - generation) <= 0) {
                if (list[i].generation != generation) {
                    close(list[i].fd);
                }
                list.erase(list.begin() + (long) i);
            } else {
                i++;
            }
        }
    }
    if (p.fd < 0) {
        note("core process: GX memory piece %u of slot %d did not come", (unsigned) generation, slot);
        return nullptr;
    }
    char* base = (char*) shmMap(p.fd, p.size);
    close(p.fd);
    if (!base) {
        note("core process: GX memory piece of %u bytes cannot be mapped", (unsigned) p.size);
        return nullptr;
    }
    if (m.base) {
        munmap(m.base, m.size);
    }
    m.base = base;
    m.size = p.size;
    m.generation = generation;
    return base;
}

}  // namespace

void configure(const Spawn& spawn)
{
    std::lock_guard<std::mutex> l(g.lock);
    g.spawn = spawn;
}

void setCrashFd(int fd)
{
    {
        std::lock_guard<std::mutex> l(g.lock);
        g.crashFd = fd;
    }
    if (fd >= 0) {
        post(kCrashFile, 0, 0, 0, fd);
    }
}

void reportHang(int seconds)
{
    post(kHang, seconds, 0, 0);
}

std::string ended()
{
    std::lock_guard<std::mutex> l(g.lock);
    return g.tried && !g.alive && g.hello ? g.endReason : std::string();
}

std::string describe()
{
    std::lock_guard<std::mutex> l(g.lock);
    if (!g.tried) {
        return "";
    }
    char text[96];
    snprintf(text, sizeof(text), "pid %d, %s", (int) g.child,
             g.alive ? "running" : g.hello ? "ended" : "did not start");
    std::string s = text;
    if (!g.alive && !g.endReason.empty()) {
        s += " (" + g.endReason + ")";
    }
    if (!g.hello && !g.problem.empty()) {
        s += " (" + g.problem + ")";
    }
    return s;
}

std::string addressMap()
{
    std::lock_guard<std::mutex> l(g.lock);
    return g.map;
}

int pid()
{
    std::lock_guard<std::mutex> l(g.lock);
    return (int) g.child;
}

bool shutdown(int waitMs)
{
    std::unique_lock<std::mutex> l(g.lock);
    if (!g.alive) {
        return true;
    }
    ::shutdown(g.sock, SHUT_RDWR);
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMs);
    return g.changed.wait_until(l, until, [] { return !g.alive; });
}

void checkCounts(unsigned* chunks, unsigned* chunkMismatches, unsigned* snapshots, unsigned* snapshotMismatches)
{
    *chunks = gChunksChecked.load();
    *chunkMismatches = gChunkMismatches.load();
    *snapshots = gSnapshotsChecked.load();
    *snapshotMismatches = gSnapshotMismatches.load();
}

void forwarded(unsigned out[8])
{
    Control* c = control();
    if (!c) {
        memset(out, 0, 8 * sizeof(unsigned));
        return;
    }
    out[0] = c->padsForwarded.load();
    out[1] = c->vsyncsForwarded.load();
    out[2] = c->gyroForwarded.load();
    out[3] = c->snapshotsCopied.load();
    out[4] = c->chunksCopied.load();
    out[5] = c->segmentsMade.load();
    out[6] = c->audioForwarded.load();
    out[7] = c->audioCoreSilence.load();
}

}  // namespace re4ipc

using namespace re4ipc;

int re4ipc_mem1_init(void)
{
    return 1;  // the core process reserves MEM1 itself
}

int re4ipc_reserve(const char* dataDir, const char* moduleDir)
{
    std::unique_lock<std::mutex> l(g.lock);
    g.dataDir = dataDir ? dataDir : "";
    g.moduleDir = moduleDir ? moduleDir : "";
    if (!g.tried) {
        startLocked(l);
    }
    return g.hello && g.alive ? 1 : 0;
}

int re4ipc_start(void)
{
    std::string payload;
    {
        std::lock_guard<std::mutex> l(g.lock);
        payload = g.dataDir;
        payload.push_back(0);
        payload += g.moduleDir;
        payload.push_back(0);
    }
    Reply r;
    if (!call(kStart, 0, 0, 0, payload.data(), (uint32_t) payload.size(), 60000, &r)) {
        return 0;
    }
    return r.header.a;
}

int re4ipc_state(void)
{
    {
        std::lock_guard<std::mutex> l(g.lock);
        if (!g.tried) {
            return RE4_CORE_IDLE;
        }
        if (!g.hello || !g.alive) {
            return RE4_CORE_FAILED;
        }
    }
    Reply r;
    return call(kState, 0, 0, 0, nullptr, 0, 3000, &r) ? r.header.a : RE4_CORE_FAILED;
}

const char* re4ipc_message(void)
{
    static char copy[512];
    std::string text;
    {
        std::lock_guard<std::mutex> l(g.lock);
        if (!g.tried) {
            text = "not started";
        } else if (!g.hello) {
            text = g.problem;
        } else if (!g.alive) {
            text = g.endReason;
        }
    }
    if (text.empty()) {
        Reply r;
        text = call(kState, 0, 0, 0, nullptr, 0, 3000, &r) ? r.text() : "the game core process does not answer";
    }
    static std::mutex copyLock;
    std::lock_guard<std::mutex> l(copyLock);
    snprintf(copy, sizeof(copy), "%s", text.c_str());
    return copy;
}

void re4ipc_pad(const Re4CorePad* pad)
{
    Control* c = control();
    if (!c || !pad) {
        return;
    }
    {
        std::lock_guard<std::mutex> l(gPadLock);
        const uint32_t head = c->padHead.load(std::memory_order_relaxed);
        if (head - c->padTail.load(std::memory_order_acquire) >= kPadQueue) {
            return;  // the core process stands: this change is lost
        }
        c->pads[head % kPadQueue] = *pad;
        c->padHead.store(head + 1, std::memory_order_release);
    }
    ring(c->inputBell);
}

unsigned re4ipc_rumble(void)
{
    Control* c = control();
    return c ? c->rumble.load() : 0;
}

void re4ipc_vsync(int64_t ns)
{
    Control* c = control();
    if (!c) {
        return;
    }
    {
        std::lock_guard<std::mutex> l(gVsyncLock);
        const uint32_t head = c->vsyncHead.load(std::memory_order_relaxed);
        if (head - c->vsyncTail.load(std::memory_order_acquire) >= kVsyncQueue) {
            return;
        }
        c->vsyncs[head % kVsyncQueue] = ns;
        c->vsyncHead.store(head + 1, std::memory_order_release);
    }
    ring(c->inputBell);
}

void re4ipc_gyro(float yaw, float pitch)
{
    Control* c = control();
    if (!c) {
        return;
    }
    {
        std::lock_guard<std::mutex> l(gGyroLock);
        uint32_t y, p;
        memcpy(&y, &yaw, 4);
        memcpy(&p, &pitch, 4);
        const uint32_t seq = c->gyroSeq.load(std::memory_order_relaxed);
        c->gyroSeq.store(seq + 1, std::memory_order_release);  // odd: being written
        c->gyroYaw.store(y, std::memory_order_release);
        c->gyroPitch.store(p, std::memory_order_release);
        c->gyroSeq.store(seq + 2, std::memory_order_release);
    }
    ring(c->inputBell);
}

// the sound's way through the core process, for the log (every 2000 reads)
unsigned gAudioReads, gAudioWaits, gAudioShort, gAudioWaitMax;

namespace {

// `frames` frames of `count` values from value `first` of each ring frame (0, 2: the stereo; 2, 4:
// the four channels); silence for what does not come
int readSound(int16_t* out, int frames, uint32_t first, uint32_t count)
{
    Control* c = control();
    if (frames <= 0) {
        return 0;
    }
    int got = 0;
    if (c) {
        const uint32_t channels = c->audioChannels;
        const int16_t* ring16 = (const int16_t*) ((const char*) c + c->audioOffset);
        const uint32_t r = c->audioRead.load(std::memory_order_relaxed);
        uint32_t w = c->audioWrite.load(std::memory_order_acquire);
        if (w - r < (uint32_t) frames && !gEnded.load()) {
            // more than the core process kept ahead (the AudioTrack takes several reads at
            // once): it reads them from the core now, the app waits that long (at most 20 ms;
            // in the app's own process the core's ring answers at once)
            c->audioWant.store(r + (uint32_t) frames, std::memory_order_release);
            ring(c->audioBell);
            const uint32_t from = nowMs();
            for (;;) {
                const uint32_t seen = c->audioDone.seq.load(std::memory_order_acquire);
                w = c->audioWrite.load(std::memory_order_acquire);
                const uint32_t waited = nowMs() - from;
                if (w - r >= (uint32_t) frames || waited >= 20) {
                    break;
                }
                waitBell(c->audioDone, seen, (int) (20 - waited));
            }
            const uint32_t waited = nowMs() - from;
            gAudioWaits++;
            gAudioWaitMax = waited > gAudioWaitMax ? waited : gAudioWaitMax;
            if (w - r < (uint32_t) frames) {
                gAudioShort++;
            }
        }
        if (++gAudioReads == 2000) {
            note("core process sound: 2000 reads, %u waited for the core process (at most %u ms), %u short",
                 gAudioWaits, gAudioWaitMax, gAudioShort);
            gAudioReads = gAudioWaits = gAudioShort = gAudioWaitMax = 0;
        }
        const uint32_t avail = w - r;
        got = (int) (avail < (uint32_t) frames ? avail : (uint32_t) frames);
        if (first + count <= channels) {
            for (int i = 0; i < got; i++) {
                const int16_t* f = ring16 + ((r + (uint32_t) i) & (kAudioFrames - 1)) * channels + first;
                for (uint32_t k = 0; k < count; k++) {
                    out[i * (int) count + (int) k] = f[k];
                }
            }
        } else {
            got = 0;  // a ring without these channels (cannot happen with this program's core process)
        }
        c->audioRead.store(r + (uint32_t) got, std::memory_order_release);
        ring(c->audioBell);
    }
    memset(out + got * (int) count, 0, (size_t) (frames - got) * count * sizeof(int16_t));
    return got;
}

}  // namespace

int re4ipc_audio(int16_t* out, int frames)
{
    return readSound(out, frames, 0, 2);
}

int re4ipc_audio4(int16_t* out, int frames)
{
    return readSound(out, frames, 2, 4);
}

int re4ipc_audio_surround(void)
{
    Control* c = control();
    return c ? (int) c->audioSurround.load() : 0;
}

void re4ipc_audio_stats(unsigned* counters)
{
    Control* c = control();
    for (int i = 0; i < 5; i++) {
        counters[i] = c ? c->audioStats[i].load() : 0;
    }
}

const Re4CoreSnapshot* re4ipc_acquire(void)
{
    Control* c = control();
    if (!c) {
        return nullptr;
    }
    std::lock_guard<std::mutex> l(gSnapLock);
    if (gHeld) {
        return nullptr;  // one reader at a time
    }
    Re4CoreSnapshot* buffers = (Re4CoreSnapshot*) ((char*) c + c->snapshotOffset);
    if (c->snapshotMiddle.load(std::memory_order_acquire) & kSnapshotNew) {
        gFront = c->snapshotMiddle.exchange(gFront, std::memory_order_acq_rel) & 3;
        gHaveFront = true;
        if (c->check) {
            const uint64_t h = hashSnapshot(buffers[gFront]);
            gSnapshotsChecked++;
            if ((uint32_t) h != c->snapshotHashLo[gFront] || (uint32_t) (h >> 32) != c->snapshotHashHi[gFront]) {
                gSnapshotMismatches++;
            }
        }
    }
    if (!gHaveFront) {
        return nullptr;
    }
    gHeld = true;
    return &buffers[gFront];
}

void re4ipc_release(const Re4CoreSnapshot*)
{
    std::lock_guard<std::mutex> l(gSnapLock);
    gHeld = false;
}

int re4ipc_log_tail(char* out, int size)
{
    if (!out || size <= 0) {
        return 0;
    }
    Reply r;
    if (!call(kLogTail, size, 0, 0, nullptr, 0, 3000, &r)) {
        copyText(out, size, re4ipc_message());
        return (int) strlen(out);
    }
    copyText(out, size, r.text());
    return (int) strlen(out);
}

const Re4GxChunk* re4ipc_gx_acquire(void)
{
    Control* c = control();
    if (!c) {
        return nullptr;
    }
    std::lock_guard<std::mutex> l(gGxLock);
    const uint32_t epoch = c->appEpoch.load();
    c->pollEpoch.store(epoch);
    c->pollMs.store(nowMs());
    ring(c->gxBell);
    for (;;) {
        int best = -1;
        for (int i = 0; i < kGxSlots; i++) {
            if (c->slots[i].state.load(std::memory_order_acquire) == kSlotReady &&
                (best < 0 || (int32_t) (c->slots[i].order - c->slots[best].order) < 0)) {
                best = i;
            }
        }
        if (best < 0) {
            return nullptr;
        }
        GxSlot& s = c->slots[best];
        uint32_t expected = kSlotReady;
        if (!s.state.compare_exchange_strong(expected, kSlotReading, std::memory_order_acq_rel)) {
            continue;  // the core process took it back (a detach)
        }
        // a chunk from before the last detach, or one before the RESET a new renderer needs
        const bool stale = s.epoch != epoch || (gWantReset && !(s.info.flags & RE4_GX_CHUNK_RESET));
        char* base = stale ? nullptr : segment(best, s.generation);
        if (!base) {
            s.state.store(kSlotReleased, std::memory_order_release);
            ring(c->gxBell);
            if (!stale) {
                // its data cannot be read here: start over
                c->appEpoch.fetch_add(1);
                gWantReset = true;
                return nullptr;
            }
            continue;
        }
        gWantReset = false;
        Re4GxChunk& v = gViews[best];
        const GxInfo& i = s.info;
        v.version = i.version;
        v.sequence = i.sequence;
        v.flags = i.flags;
        v.ops = (const uint32_t*) (base + s.opOffset);
        v.opWords = i.opWords;
        v.vertices = (const uint8_t*) (base + s.vertexOffset);
        v.vertexBytes = i.vertexBytes;
        v.indices = (const uint16_t*) (base + s.indexOffset);
        v.indexCount = i.indexCount;
        v.blob = (const uint8_t*) (base + s.blobOffset);
        v.blobBytes = i.blobBytes;
        v.xfbAddress = i.xfbAddress;
        v.xfbWidth = i.xfbWidth;
        v.xfbHeight = i.xfbHeight;
        v.draws = i.draws;
        v.vertexCount = i.vertexCount;
        v.textureBytes = i.textureBytes;
        if (c->check) {
            const uint64_t h = hashChunk(i, v.ops, v.vertices, v.indices, v.blob);
            gChunksChecked++;
            if ((uint32_t) h != s.hashLo || (uint32_t) (h >> 32) != s.hashHi) {
                gChunkMismatches++;
            }
        }
        return &v;
    }
}

void re4ipc_gx_release(const Re4GxChunk* chunk)
{
    Control* c = control();
    if (!c || !chunk) {
        return;
    }
    for (int i = 0; i < kGxSlots; i++) {
        if (chunk == &gViews[i] && c->slots[i].state.load() == kSlotReading) {
            c->slots[i].state.store(kSlotReleased, std::memory_order_release);
        }
    }
    ring(c->gxBell);
}

void re4ipc_gx_detach(void)
{
    Control* c = control();
    if (!c) {
        return;
    }
    {
        std::lock_guard<std::mutex> l(gGxLock);
        c->appEpoch.fetch_add(1);
        gWantReset = true;
    }
    ring(c->gxBell);
}

int re4ipc_pause(int on, int waitMs)
{
    Reply r;
    return call(kPause, on, waitMs, 0, nullptr, 0, (waitMs > 0 ? waitMs : 0) + 5000, &r) ? r.header.a : 0;
}

void re4ipc_debug_menu(void)
{
    post(kDebugMenu, 0, 0, 0);
}

void re4ipc_set_dof(int on)
{
    post(kSetDof, on, 0, 0);
}

void re4ipc_set_letterbox(int off)
{
    post(kSetLetterbox, off, 0, 0);
}

void re4ipc_swipe(int dir)
{
    post(kSwipe, dir, 0, 0);
}

void re4ipc_slash(float tilt)
{
    int32_t a;
    memcpy(&a, &tilt, 4);
    post(kSlash, a, 0, 0);
}

void re4ipc_touch(float u, float v)
{
    int32_t a, b;
    memcpy(&a, &u, 4);
    memcpy(&b, &v, 4);
    post(kTouch, a, b, 0);
}

int re4ipc_card_list(Re4CardFile* out, int max)
{
    Reply r;
    if (!call(kCardList, max, 0, 0, nullptr, 0, 15000, &r)) {
        return RE4_CARD_NOCARD;
    }
    const int n = r.header.a;
    if (n > 0 && out) {
        const size_t bytes = (size_t) (n < max ? n : max) * sizeof(Re4CardFile);
        memcpy(out, r.data(), bytes < r.dataBytes() ? bytes : r.dataBytes());
    }
    return n;
}

int re4ipc_card_free_blocks(void)
{
    Reply r;
    return call(kCardFree, 0, 0, 0, nullptr, 0, 15000, &r) ? r.header.a : RE4_CARD_NOCARD;
}

int re4ipc_card_file_name(int index, char* out, int size)
{
    Reply r;
    if (!call(kCardName, index, size, 0, nullptr, 0, 15000, &r)) {
        return RE4_CARD_NOCARD;
    }
    copyText(out, size, r.text());
    return r.header.a;
}

int re4ipc_card_export(int index, unsigned char* out, unsigned capacity, char* note, int noteSize)
{
    Reply r;
    if (!call(kCardExport, index, (int) capacity, noteSize, nullptr, 0, 30000, &r)) {
        copyText(note, noteSize, "the game core process does not answer");
        return RE4_CARD_NOCARD;
    }
    copyText(note, noteSize, r.text());
    const int bytes = r.header.a;
    if (bytes > 0 && out) {
        const size_t n = (size_t) bytes < capacity ? (size_t) bytes : capacity;
        memcpy(out, r.data(), n < r.dataBytes() ? n : r.dataBytes());
    }
    return bytes;
}

int re4ipc_card_import(const unsigned char* gci, unsigned size, int replace, char* note, int noteSize)
{
    Reply r;
    if (!call(kCardImport, replace, 0, noteSize, gci, size, 30000, &r)) {
        copyText(note, noteSize, "the game core process does not answer");
        return RE4_CARD_NOCARD;
    }
    copyText(note, noteSize, r.text());
    return r.header.a;
}

unsigned re4ipc_frame_count(void)
{
    Control* c = control();
    return c ? c->frames.load() : 0;
}

int re4ipc_game_thread(void)
{
    return 0;  // in the core process: reportHang
}

long re4ipc_free_at(void)
{
    Control* c = control();
    return c ? (long) c->freeAt.load() : -1;
}
