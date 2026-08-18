#include "script_source.h"
#include "runtime/log.h"

#include "runtime/env.h"
#include "input/re4_context.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>

// Compteurs du runtime, hors namespace (src/hle/gx.cpp).
extern uint64_t g_st_frames;
extern uint64_t g_stat_draws;
extern std::atomic<bool> g_gx_framelog_request;   // src/hle/gx/renderer_gl.cpp ('F')

namespace nj::runtime::input {

// Capture demandee par 'P' : dixiemes de seconde apres l'arrivee en scene, plus 1 (0 = rien).
// Lue et remise a zero par la boucle hote (main.cpp).
std::atomic<int> g_shot_request{0};

namespace {
// Bits de la manette GameCube, comme dans bindings.cpp.
constexpr uint16_t B_LEFT = 1 << 0, B_RIGHT = 1 << 1, B_DOWN = 1 << 2, B_UP = 1 << 3,
                   B_Z = 1 << 4, B_R = 1 << 5, B_L = 1 << 6, B_A = 1 << 8, B_B = 1 << 9,
                   B_X = 1 << 10, B_Y = 1 << 11, B_START = 1 << 12;

const char* script() { return NJ_ENV("NJ_KEYSCRIPT"); }
} // namespace

bool ScriptSource::enabled() {
    const char* s = script();
    return s != nullptr && *s != '\0';
}

ScriptSource::ScriptSource() {
    const char* p = script();
    while (p && *p) {
        Step st{0.0, 150.0, 0, 0, 0, false, false};
        st.at_s = std::atof(p);
        while (*p && *p != ':' && *p != ',') ++p;
        if (*p == ':') {
            ++p;
            for (; *p && *p != ',' && *p != '*'; ++p) {
                switch (*p) {
                case 'A': st.buttons |= B_A; break;
                case 'B': st.buttons |= B_B; break;
                case 'X': st.buttons |= B_X; break;
                case 'Y': st.buttons |= B_Y; break;
                case 'Z': st.buttons |= B_Z; break;
                case 'S': st.buttons |= B_START; break;
                case 'L': st.buttons |= B_L; break;
                case 'R': st.buttons |= B_R; break;
                case 'U': st.buttons |= B_UP; break;
                case 'D': st.buttons |= B_DOWN; break;
                case '<': st.buttons |= B_LEFT; break;
                case '>': st.buttons |= B_RIGHT; break;
                case 'w': st.stick_y = 100; break;
                case 's': st.stick_y = -100; break;
                case 'a': st.stick_x = -100; break;
                case 'd': st.stick_x = 100; break;
                case 'P': st.shot = true; break;
                case 'F': st.framelog = true; break;
                default: break;
                }
            }
            if (*p == '*') {
                st.hold_ms = std::atof(p + 1);
                while (*p && *p != ',') ++p;
            }
        }
        if (st.buttons || st.stick_x || st.stick_y || st.shot || st.framelog)
            steps_.push_back(st);
        if (*p == ',') ++p;
    }
    NJ_LOG << "[Script] " << steps_.size() << " etapes, horloge lancee a l'arrivee en scene\n";
}

void ScriptSource::poll(PadState pads[4]) {

    const auto now = std::chrono::steady_clock::now();
    if (!started_) {
        // Meme critere qu'AutoStartSource : Leon jouable (etat du jeu), sinon une vraie scene de
        // plusieurs milliers de tirages (le seuil seul ratait les petites salles).
        {
            // 2 s d'affilee (meme raison que l'AutoStart : salle qui ouvre sur une video).
            static std::chrono::steady_clock::time_point since{};
            const Re4Context ctx = re4_context_read();
            if (ctx.valid && ctx.mode == Re4Mode::Gameplay && ctx.player) {
                if (since == std::chrono::steady_clock::time_point{})
                    since = now;
                if (now - since > std::chrono::seconds(2)) {
                    started_ = true;
                    origin_ = now;
                    NJ_LOG << "[Script] scene atteinte, t=0\n";
                    return;
                }
            } else {
                since = {};
            }
        }
        const uint64_t f = ::g_st_frames, d = ::g_stat_draws;
        if (f > last_frames_ + 30) {
            const uint64_t per = (d - last_draws_) / (f - last_frames_);
            last_frames_ = f;
            last_draws_ = d;
            if (per > 3000) {
                started_ = true;
                origin_ = now;
                NJ_LOG << "[Script] scene atteinte, t=0\n";
            }
        }
        return;
    }
    const double t = std::chrono::duration<double>(now - origin_).count();
    for (Step& st : steps_) {
        const double dt = (t - st.at_s) * 1000.0;
        if (dt < 0.0 || dt > st.hold_ms) continue;
        if (!st.fired) {
            st.fired = true;
            if (st.shot)
                g_shot_request.store((int) (st.at_s * 10.0 + 0.5) + 1);
            if (st.framelog) {
                ::g_gx_framelog_request = true;
            }
            NJ_LOG << "[Script] t=" << st.at_s << "s boutons=0x" << std::hex << st.buttons
                      << std::dec << " stick=" << (int) st.stick_x << "," << (int) st.stick_y
                      << " pendant " << st.hold_ms << " ms\n";
        }
        pads[0].buttons |= st.buttons;
        // Le jeu fabrique L et R depuis les gachettes analogiques (pad.cpp) : le bit seul ne vise pas.
        if (st.buttons & B_R) pads[0].trigger_r = 255;
        if (st.buttons & B_L) pads[0].trigger_l = 255;
        if (st.stick_x) pads[0].stick_x = st.stick_x;
        if (st.stick_y) pads[0].stick_y = st.stick_y;
        pads[0].err = 0;
    }
}

}
