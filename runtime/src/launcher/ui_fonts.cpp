#include "ui_fonts.h"

#include <cstring>
#include <filesystem>
#include <string>

#include "imgui.h"
#include "i18n.h"

namespace nj::runtime::ui {
namespace {

namespace fs = std::filesystem;
using nj::runtime::i18n::Script;

// Latin etendu (polonais, turc), ponctuation generale, euro et marque : la plage par defaut d'ImGui s'arrete a U+00FF.
const ImWchar kLatinExt[] = {0x0020, 0x00FF, 0x0100, 0x017F, 0x2000, 0x206F, 0x20AC, 0x20AC, 0x2122, 0x2122, 0};

struct Cjk {
    const char *path;
    int index;   // numero de police dans un .ttc
};

const Cjk *cjk_candidates(Script s, int &n) {
#if defined(_WIN32)
    static const Cjk ja[] = {{"C:/Windows/Fonts/YuGothM.ttc", 0}, {"C:/Windows/Fonts/meiryo.ttc", 0}, {"C:/Windows/Fonts/msgothic.ttc", 0}};
    static const Cjk ko[] = {{"C:/Windows/Fonts/malgun.ttf", 0}, {"C:/Windows/Fonts/gulim.ttc", 0}};
    static const Cjk zh[] = {{"C:/Windows/Fonts/msyh.ttc", 0}, {"C:/Windows/Fonts/simsun.ttc", 0}};
#elif defined(__ANDROID__)
    // NotoSansCJK-Regular.ttc : JP, KR, SC, TC, HK dans cet ordre.
    static const Cjk ja[] = {{"/system/fonts/NotoSansCJK-Regular.ttc", 0}, {"/system/fonts/NotoSansJP-Regular.otf", 0}, {"/system/fonts/DroidSansFallback.ttf", 0}};
    static const Cjk ko[] = {{"/system/fonts/NotoSansCJK-Regular.ttc", 1}, {"/system/fonts/NotoSansKR-Regular.otf", 0}, {"/system/fonts/DroidSansFallback.ttf", 0}};
    static const Cjk zh[] = {{"/system/fonts/NotoSansCJK-Regular.ttc", 2}, {"/system/fonts/NotoSansSC-Regular.otf", 0}, {"/system/fonts/DroidSansFallback.ttf", 0}};
#else
    static const Cjk ja[] = {{nullptr, 0}}, ko[] = {{nullptr, 0}}, zh[] = {{nullptr, 0}};
#endif
    const Cjk *t = s == Script::kJapanese ? ja : s == Script::kKorean ? ko : zh;
    n = s == Script::kJapanese ? (int)(sizeof ja / sizeof ja[0]) : s == Script::kKorean ? (int)(sizeof ko / sizeof ko[0]) : (int)(sizeof zh / sizeof zh[0]);
    return t;
}

const Cjk *find_cjk(Script s) {
    int n = 0;
    const Cjk *c = cjk_candidates(s, n);
    std::error_code ec;
    for (int i = 0; i < n; ++i)
        if (c[i].path && fs::exists(c[i].path, ec)) return &c[i];
    return nullptr;
}

// Glyphes des noms de langues (selecteur du menu : « 日本語 », « 한국어 », « 简体中文 », « Русский », « Türkçe »...) : presents quelle que soit la langue
// active, sinon ils sortent en « ? ». Les plages vivent jusqu'a la construction de l'atlas (premiere image) : statiques.
ImVector<ImWchar> g_names_all, g_names_cjk[3];   // tous les noms (police latine) ; noms ja / ko / zh (police CJK de chacun)
bool g_names_built = false;

void build_name_ranges() {
    if (g_names_built) return;
    g_names_built = true;
    ImFontGlyphRangesBuilder all;
    all.AddRanges(kLatinExt);
    const int n = nj::runtime::i18n::count();
    for (int i = 0; i < n; ++i) all.AddText(nj::runtime::i18n::native_name_at(i));
    all.BuildRanges(&g_names_all);
    for (int i = 0; i < n; ++i) {
        const char *code = nj::runtime::i18n::code_at(i);
        const int k = !std::strcmp(code, "ja") ? 0 : !std::strcmp(code, "ko") ? 1 : !std::strcmp(code, "zh") ? 2 : -1;
        if (k < 0) continue;
        ImFontGlyphRangesBuilder b;
        b.AddText(nj::runtime::i18n::native_name_at(i));
        b.BuildRanges(&g_names_cjk[k]);
    }
}

}  // namespace

bool language_font_available() {
    const Script s = nj::runtime::i18n::script();
    if (s == Script::kLatin || s == Script::kCyrillic) return true;
    return find_cjk(s) != nullptr;
}

ImFont *add_font(ImGuiIO &io, const char *latin_path, float px) {
    std::error_code ec;
    if (!latin_path || !fs::exists(latin_path, ec)) return nullptr;
    const Script s = nj::runtime::i18n::script();
    build_name_ranges();
    const ImWchar *ranges = s == Script::kCyrillic ? io.Fonts->GetGlyphRangesCyrillic() : g_names_all.Data;
    ImFont *f = io.Fonts->AddFontFromFileTTF(latin_path, px, nullptr, ranges);
    if (!f) return nullptr;
    if (s == Script::kJapanese || s == Script::kKorean || s == Script::kChinese) {
        if (const Cjk *c = find_cjk(s)) {
            ImFontConfig cfg;
            cfg.MergeMode = true;
            cfg.FontNo = c->index;
            const ImWchar *r = s == Script::kJapanese ? io.Fonts->GetGlyphRangesJapanese()
                               : s == Script::kKorean ? io.Fonts->GetGlyphRangesKorean()
                                                      : io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
            io.Fonts->AddFontFromFileTTF(c->path, px, &cfg, r);
        }
    }
    // les noms des autres langues asiatiques, avec la police de chacune (seulement ces quelques glyphes)
    const Script cs[3] = {Script::kJapanese, Script::kKorean, Script::kChinese};
    for (int k = 0; k < 3; ++k) {
        if (cs[k] == s || g_names_cjk[k].Size == 0) continue;
        if (const Cjk *c2 = find_cjk(cs[k])) {
            ImFontConfig cfg;
            cfg.MergeMode = true;
            cfg.FontNo = c2->index;
            io.Fonts->AddFontFromFileTTF(c2->path, px, &cfg, g_names_cjk[k].Data);
        }
    }
    return f;
}

}  // namespace nj::runtime::ui
