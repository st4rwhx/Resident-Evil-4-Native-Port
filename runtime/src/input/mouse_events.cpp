#include "input/mouse_events.h"

#include <SDL.h>

#include <atomic>

namespace nj::runtime::input {

namespace {

// Alimente par la boucle hote, lu par les sources d'entree : atomique plutot
// que verrouille, l'etat tenant dans des entiers simples.
std::atomic<unsigned> g_buttons{0};
std::atomic<int> g_dx{0}, g_dy{0};
std::atomic<int> g_x{0}, g_y{0};
// Combien d'evenements souris la fenetre a recus, tous types confondus.
// Sans ce compteur, "aucun bouton" ne distingue pas un correctif qui echoue
// d'un clic de test qui n'est jamais arrive - et j'ai deja tire des
// conclusions d'un test qui n'avait pas eu lieu.
std::atomic<unsigned> g_seen{0};
std::atomic<unsigned> g_btn_events{0};

} // namespace

void mouse_event_feed(const SDL_Event& e) {
    if (e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN ||
        e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEWHEEL)
        g_seen.fetch_add(1, std::memory_order_relaxed);

    switch (e.type) {
    case SDL_MOUSEBUTTONDOWN:
        g_btn_events.fetch_add(1, std::memory_order_relaxed);
        g_buttons.fetch_or(SDL_BUTTON(e.button.button), std::memory_order_relaxed);
        break;
    case SDL_MOUSEBUTTONUP:
        g_buttons.fetch_and(~SDL_BUTTON(e.button.button), std::memory_order_relaxed);
        break;
    case SDL_MOUSEMOTION:
        // On accumule : la boucle hote tourne plus vite que le jeu ne lit sa
        // manette, et plusieurs evenements peuvent tomber entre deux lectures.
        g_dx.fetch_add(e.motion.xrel, std::memory_order_relaxed);
        g_dy.fetch_add(e.motion.yrel, std::memory_order_relaxed);
        g_x.store(e.motion.x, std::memory_order_relaxed);
        g_y.store(e.motion.y, std::memory_order_relaxed);
        break;
    case SDL_WINDOWEVENT:
        // Perdre le focus relache tout : sinon un bouton enfonce au moment ou
        // l'on bascule vers une autre fenetre reste colle, et le jeu le voit
        // maintenu indefiniment.
        if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
            g_buttons.store(0, std::memory_order_relaxed);
        break;
    default:
        break;
    }
}

void mouse_event_stats(unsigned* seen, unsigned* btn) {
    if (seen)
        *seen = g_seen.load(std::memory_order_relaxed);
    if (btn)
        *btn = g_btn_events.load(std::memory_order_relaxed);
}

unsigned mouse_buttons() { return g_buttons.load(std::memory_order_relaxed); }

void mouse_take_motion(int* dx, int* dy) {
    if (dx)
        *dx = g_dx.exchange(0, std::memory_order_relaxed);
    if (dy)
        *dy = g_dy.exchange(0, std::memory_order_relaxed);
}

void mouse_position(int* x, int* y) {
    if (x)
        *x = g_x.load(std::memory_order_relaxed);
    if (y)
        *y = g_y.load(std::memory_order_relaxed);
}

} // namespace nj::runtime::input
