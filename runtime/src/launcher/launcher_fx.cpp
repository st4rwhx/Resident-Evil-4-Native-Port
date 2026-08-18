#include "launcher_fx.h"

#include <algorithm>
#include <cmath>

namespace nj::launcher::fx {
namespace {

constexpr float kTau = 6.2831853f;

ImU32 rgba(int r, int g, int b, float a) { return IM_COL32(r, g, b, (int)(std::clamp(a, 0.0f, 1.0f) * 255.0f)); }

// Tache douce : cercles concentriques de plus en plus transparents (ImGui n'a pas de degrade radial).
void soft_blob(ImDrawList *dl, ImVec2 c, float r, int cr, int cg, int cb, float a) {
    const int n = 14;
    for (int i = 0; i < n; ++i) {
        const float k = (float)(i + 1) / n;
        dl->AddCircleFilled(c, r * (1.0f - 0.9f * (float)i / n), rgba(cr, cg, cb, a / n * (0.5f + k)), 40);
    }
}

}  // namespace

void background(ImDrawList *dl, ImVec2 size, float t) {
    (void)t;
    const ImVec2 a(0, 0), b = size;
    dl->AddRectFilledMultiColor(a, b, IM_COL32(26, 6, 6, 255), IM_COL32(26, 6, 6, 255), IM_COL32(6, 3, 3, 255), IM_COL32(6, 3, 3, 255));
    // vignette : bords sombres, comme derriere les menus du jeu
    const float h = size.y;
    dl->AddRectFilledMultiColor(a, ImVec2(b.x, h * 0.22f), IM_COL32(0, 0, 0, 200), IM_COL32(0, 0, 0, 200), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
    dl->AddRectFilledMultiColor(ImVec2(0, h * 0.78f), b, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 210), IM_COL32(0, 0, 0, 210));
    const float w = size.x * 0.12f;
    dl->AddRectFilledMultiColor(a, ImVec2(w, h), IM_COL32(0, 0, 0, 150), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 150));
    dl->AddRectFilledMultiColor(ImVec2(b.x - w, 0), b, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 150), IM_COL32(0, 0, 0, 150), IM_COL32(0, 0, 0, 0));
}

void title_glow(ImDrawList *dl, ImVec2 center, float radius, float t) {
    const float breath = 0.75f + 0.25f * std::sin(t * 1.3f);
    soft_blob(dl, center, radius * (0.9f + 0.1f * breath), 170, 22, 18, 0.32f * breath);
}

bool fade_in(ImDrawList *dl, ImVec2 size, float t, float duration) {
    if (t >= duration) return false;
    const float k = 1.0f - t / duration;
    dl->AddRectFilled(ImVec2(0, 0), size, rgba(0, 0, 0, k * k));
    return true;
}

bool button(const char *label, ImU32 accent, float width, float height) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - width) * 0.5f);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(label, ImVec2(width, height));
    const bool hov = ImGui::IsItemHovered() || ImGui::IsItemActive();
    // survol lisse : memorise dans le stockage de la fenetre, une valeur par bouton
    ImGuiStorage *ss = ImGui::GetStateStorage();
    const ImGuiID id = ImGui::GetItemID();
    float v = ss->GetFloat(id, 0.0f);
    v += ((hov ? 1.0f : 0.0f) - v) * std::min(1.0f, ImGui::GetIO().DeltaTime * 12.0f);
    if (ImGui::GetIO().DeltaTime <= 0.0f) v = hov ? 1.0f : 0.0f;
    ss->SetFloat(id, v);

    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 q(p.x + width, p.y + height);
    dl->AddRectFilled(p, q, rgba(14, 8, 7, 0.82f), 3.0f);
    dl->AddRectFilledMultiColor(p, q, rgba(150, 24, 20, 0.0f), rgba(150, 24, 20, 0.55f * v), rgba(150, 24, 20, 0.55f * v), rgba(150, 24, 20, 0.0f));   // surbrillance rouge qui s'etale
    dl->AddRect(p, q, accent, 3.0f, 0, 1.5f);
    // curseur rouge (petit triangle) qui glisse vers le texte au survol
    const float cy = p.y + height * 0.5f, ch = height * 0.20f;
    const ImVec2 ts = ImGui::CalcTextSize(label);
    const float tx = p.x + (width - ts.x) * 0.5f;
    const float cx = tx - ch * 2.4f - (1.0f - v) * ch * 1.6f;
    dl->AddTriangleFilled(ImVec2(cx, cy - ch), ImVec2(cx, cy + ch), ImVec2(cx + ch * 1.6f, cy), rgba(196, 48, 38, v));
    const ImU32 txt = rgba(235 + (int)(20 * v), 225 + (int)(27 * v), 205 + (int)(35 * v), 0.78f + 0.22f * v);
    dl->AddText(ImVec2(tx + 1.5f, cy - ts.y * 0.5f + 1.5f), IM_COL32(0, 0, 0, 200), label);
    dl->AddText(ImVec2(tx, cy - ts.y * 0.5f), txt, label);
    return pressed;
}

}  // namespace nj::launcher::fx
