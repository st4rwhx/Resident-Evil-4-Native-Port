#pragma once
// Connexion Discord du lanceur (0.1.1) : le joueur doit etre membre du serveur Nightjar et avoir le
// role exige (Member). Application publique Discord (« Public Client ») : code d'autorisation avec
// PKCE (S256), sans secret dans l'executable ; reponse recue sur 127.0.0.1 ; appartenance lue chez
// Discord (GET /users/@me/guilds/{serveur}/member, scope guilds.members.read). Le jeton de
// rafraichissement est garde chiffre (host::secret_save) pour ne pas redemander la connexion.
#include <atomic>
#include <mutex>
#include <string>

namespace nj::launcher::discord {

// true : Discord verifie A CHAQUE LANCEMENT, en ligne obligatoire (pre-release 0.1.1 a 0.1.5).
// false (0.1.6, plan docs/PLAN_ACCES_2026-10-16.md) : laissez-passer a BAIL. Une connexion par version,
// puis le jeu demarre sans reseau tant que le bail dure ; chaque contact reussi avec Discord (en
// silence) le renouvelle. Duree par defaut kLeaseDays, modifiable par access.json (« lease_days »,
// en texte, 0 = sans limite) dans la limite de kLeaseMaxDays ; une version invérifiable ne bloque plus.
inline constexpr bool kOnlineEveryLaunch = false;
inline constexpr int kLeaseDays = 60;
inline constexpr int kLeaseDaysNightly = 7;   // canal « nightly » (version « x.y.z-nightly ») : bail court, access.json « lease_days_nightly »
inline constexpr int kLeaseMaxDays = 365;

enum class Access {
    Idle,        // rien de lance
    Checking,    // verification du compte enregistre
    Waiting,     // navigateur ouvert, en attente de l'autorisation
    Granted,     // membre avec le role : le jeu peut demarrer
    NeedLogin,   // aucun compte enregistre (ou plus valable)
    NotMember,   // le compte n'est pas sur le serveur
    NoRole,      // sur le serveur, sans le role exige
    Offline,     // Discord injoignable
    Failed,      // autorisation refusee ou erreur
};

struct State {
    std::atomic<Access> access{Access::Idle};
    std::atomic<bool> cancel{false};
    std::atomic<bool> use_browser{false};   // mobile : rouvrir la demande dans le navigateur
    std::atomic<bool> guest{false};         // essai invite en cours (pas de compte Discord)
    std::mutex m;
    std::string user;     // nom d'affichage Discord
    std::string detail;   // cause d'un echec, pour l'ecran
};

// A lancer dans un fil (bloquant). check_saved : compte enregistre -> Granted / NotMember / NoRole /
// NeedLogin. login : ouvre le navigateur et attend l'autorisation (annulable par State::cancel).
void check_saved(State *st);
void login(State *st);
// Oublie le compte enregistre (changer de compte).
void forget();
// Une image du lanceur affichee (a chaque tour de sa boucle) : l'application est au premier plan.
// Android coupe le reseau d'une application en arriere-plan (navigateur devant pendant la connexion,
// 13/10) : les echanges avec Discord attendent son retour.
void frame_tick();
// Essai invite (0.1.7, plan d'acces) : jours proposes a un appareil qui n'a jamais ouvert de session (0 = rien : valeur de
// « guest_days » d'access.json, texte, plafond 14 ; connue apres le premier contact avec le depot public) ; start_guest demarre
// l'essai (une fois par installation) et rend Granted, la fin de l'essai ramene a NeedLogin.
// Canal nightly : la version cachee de l'exe se termine par « -nightly » (tools/version_cachee.py 0.1.7-nightly). Il lit
// version-nightly.txt au lieu de version.txt et n'a qu'un bail de kLeaseDaysNightly jours.
bool is_nightly();
int guest_offer_days();
void start_guest(State *st);
// Ligne d'information sur le bail (« valable jusqu'au ... », ou avertissement a moins de 7 jours) ; vide s'il n'y en a pas.
std::string lease_line();

}  // namespace nj::launcher::discord
