#pragma once
// Traduction de l'interface de Nightjar (lanceur et menu F10), 19/10/2026.
//
// Les textes du code sont l'anglais ; TXT("English text") rend la traduction dans la langue de l'appareil (ou celle choisie dans le menu
// F10), ou l'anglais quand elle manque. Les tables viennent de tools/i18n/ (un fichier JSON par langue, relu par un humain) via
// tools/i18n/gen_i18n.py -> i18n_data.inc. Les textes du jeu (menus, dialogues) ne passent pas ici : ils viennent du disque du joueur.
//
// Langue : choix du menu (« auto » par defaut), sinon SDL_GetPreferredLocales (liste de langues de l'appareil, Android / iOS / PC),
// sinon la langue de la session Windows. Les polices sont celles du systeme (Segoe UI, Roboto) ; japonais, coreen et chinois y
// ajoutent la police CJK du systeme (aucune police n'est livree).
#include <cstddef>

namespace nj::runtime::i18n {

enum Script { kLatin = 0, kCyrillic, kJapanese, kKorean, kChinese };

// Choisit la langue : « auto » (appareil) ou un code (en, fr, pt, es, de, it, ru, tr, pl, ja, ko, zh). A appeler avant de creer les polices.
void init(const char *choice);
// Traduction de `en` (adresse stable ; l'anglais lui-meme si la langue active est l'anglais ou si la chaine manque).
const char *tr(const char *en);
// Langue active.
const char *code();
Script script();
// Pour le selecteur du menu : nombre de langues, code et nom dans la langue elle-meme.
int count();
const char *code_at(int i);
const char *native_name_at(int i);
// Un changement de langue en cours de partie : nouvelles polices a construire (le menu les recree).
bool changed_since_last_check();

}  // namespace nj::runtime::i18n

#define TXT(s) ::nj::runtime::i18n::tr(s)
// Marque un texte a traduire sans le traduire sur place (tableaux, arguments d'aides qui traduisent a l'affichage).
#define N_(s) s
