#include "i18n.h"

#include <SDL.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace nj::runtime::i18n {
namespace {

struct Lang {
    const char *code;
    const char *native;                 // nom dans la langue elle-meme
    const char *const *table;           // une traduction par cle de kKeys (nullptr : manque), nullptr pour l'anglais
    Script script;
};

#include "i18n_data.inc"                // kKeys[], tables par langue, kLangs[], kLangCount

int g_active = 0;                       // indice dans kLangs
int g_epoch = 0, g_seen = 0;
std::unordered_map<std::string_view, int> g_index;   // cle anglaise -> rang dans kKeys

int find_lang(const char *c) {
    if (!c) return -1;
    for (int i = 0; i < kLangCount; ++i)
        if (!std::strcmp(kLangs[i].code, c)) return i;
    return -1;
}

// Code de langue (2 lettres) d'une etiquette comme « pt_BR », « fr-CA », « zh_Hans_CN ».
int lang_of_tag(const char *tag) {
    if (!tag || !tag[0]) return -1;
    char two[3] = {(char)std::tolower((unsigned char)tag[0]), (char)std::tolower((unsigned char)tag[1]), 0};
    return find_lang(two);
}

int detect() {
    int n = 0;
    if (SDL_Locale *l = SDL_GetPreferredLocales()) {   // liste ordonnee des langues de l'appareil
        for (; l[n].language; ++n) {
            const int k = find_lang(l[n].language);
            if (k >= 0) {
                SDL_free(l);
                return k;
            }
        }
        SDL_free(l);
    }
#if defined(_WIN32)
    wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
    if (GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH)) {
        char a[16] = {};
        for (int i = 0; i < 15 && name[i]; ++i) a[i] = (char)name[i];
        if (const int k = lang_of_tag(a); k >= 0) return k;
    }
#endif
    for (const char *e : {"LC_ALL", "LC_MESSAGES", "LANG"})
        if (const char *v = std::getenv(e))
            if (const int k = lang_of_tag(v); k >= 0) return k;
    return 0;   // anglais
}

}  // namespace

void init(const char *choice) {
    if (g_index.empty())
        for (int i = 0; kKeys[i]; ++i) g_index.emplace(kKeys[i], i);
    int k = (!choice || !std::strcmp(choice, "auto") || !choice[0]) ? detect() : find_lang(choice);
    if (k < 0) k = 0;
    if (k != g_active) ++g_epoch;
    g_active = k;
}

const char *tr(const char *en) {
    if (!en || !*en || g_active == 0) return en;
    const auto it = g_index.find(std::string_view(en));
    if (it == g_index.end()) return en;
    const char *t = kLangs[g_active].table[it->second];
    return t && *t ? t : en;
}

const char *code() { return kLangs[g_active].code; }
Script script() { return kLangs[g_active].script; }
int count() { return kLangCount; }
const char *code_at(int i) { return i >= 0 && i < kLangCount ? kLangs[i].code : "en"; }
const char *native_name_at(int i) { return i >= 0 && i < kLangCount ? kLangs[i].native : ""; }

bool changed_since_last_check() {
    const bool c = g_epoch != g_seen;
    g_seen = g_epoch;
    return c;
}

}  // namespace nj::runtime::i18n
