#include "discord_auth.h"

#include "host.h"
#include "runtime/report.h"
#include "launcher/launcher.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using sock_t = SOCKET;
#define NJ_CLOSESOCK closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using sock_t = int;
#define INVALID_SOCKET (-1)
#define NJ_CLOSESOCK close
#endif

namespace nj::launcher::discord {

namespace {

// Application Discord « Nightjar » (Public Client) ; serveur et role par defaut, remplaces par
// access.json du depot public quand il existe (apres un raid, on change un fichier, pas l'exe).
constexpr const char *kClientId = "1559162514154201128";
constexpr const char *kDefaultGuild = "1455692745615085725";
constexpr const char *kDefaultRole = "1455704890063192064";   // Member
constexpr int kPort = 53134;
constexpr const char *kRedirect = "http://127.0.0.1:53134/callback";
constexpr const char *kApi = "https://discord.com/api/v10";

// Derniere image du lanceur (ms, horloge monotone) : premier plan si elle a moins de 300 ms.
std::atomic<int64_t> g_last_frame_ms{0};
int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Android : attendre le retour au premier plan avant de parler a Discord (reseau coupe en arriere-plan).
void wait_foreground(const std::atomic<bool> &cancel) {
#if defined(__ANDROID__)
    const int64_t limit = now_ms() + 15 * 60 * 1000;
    while (!cancel && now_ms() < limit && now_ms() - g_last_frame_ms.load() > 300)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#else
    (void)cancel;
#endif
}

// Requete HTTPS ; sur Android, jusqu'a 4 essais : le reseau revient une ou deux secondes apres le
// retour au premier plan.
bool request(const std::string &method, const std::string &url, const std::string &headers,
             const std::string &body, int &status, std::string &out) {
#if defined(__ANDROID__)
    for (int i = 0; i < 4; ++i) {
        if (host::https_request(method, url, headers, body, status, out)) return true;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return false;
#else
    return host::https_request(method, url, headers, body, status, out);
#endif
}

// ---- SHA-256 (FIPS 180-4) : empreinte du code PKCE --------------------------------------------

std::array<uint8_t, 32> sha256(const std::string &msg) {
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::vector<uint8_t> d(msg.begin(), msg.end());
    const uint64_t bits = (uint64_t)d.size() * 8;
    d.push_back(0x80);
    while (d.size() % 64 != 56) d.push_back(0);
    for (int i = 7; i >= 0; --i) d.push_back(uint8_t(bits >> (i * 8)));
    auto rotr = [](uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
    for (size_t off = 0; off < d.size(); off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = uint32_t(d[off + 4 * i]) << 24 | uint32_t(d[off + 4 * i + 1]) << 16 |
                   uint32_t(d[off + 4 * i + 2]) << 8 | d[off + 4 * i + 3];
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], e4 = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const uint32_t t1 = hh + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            const uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            hh = g;
            g = f;
            f = e;
            e = e4 + t1;
            e4 = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += e4; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    std::array<uint8_t, 32> out{};
    for (int i = 0; i < 8; ++i)
        for (int j = 0; j < 4; ++j) out[4 * i + j] = uint8_t(h[i] >> (24 - 8 * j));
    return out;
}

std::string base64url(const uint8_t *p, size_t n) {
    static const char *t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string o;
    for (size_t i = 0; i < n; i += 3) {
        const uint32_t v = uint32_t(p[i]) << 16 | (i + 1 < n ? uint32_t(p[i + 1]) << 8 : 0) | (i + 2 < n ? p[i + 2] : 0);
        o += t[(v >> 18) & 63];
        o += t[(v >> 12) & 63];
        if (i + 1 < n) o += t[(v >> 6) & 63];
        if (i + 2 < n) o += t[v & 63];
    }
    return o;
}

// Chaine aleatoire du jeu non reserve de RFC 7636 (code PKCE : 43 a 128 caracteres).
std::string random_token(size_t n) {
    static const char *alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    std::random_device rd;   // MinGW : rand_s ; Android : /dev/urandom
    std::string s;
    for (size_t i = 0; i < n; ++i) s += alpha[rd() % 66];
    return s;
}

std::string url_encode(const std::string &v) {
    static const char *hex = "0123456789ABCDEF";
    std::string o;
    for (unsigned char c : v) {
        if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') o += char(c);
        else { o += '%'; o += hex[c >> 4]; o += hex[c & 15]; }
    }
    return o;
}

// ---- JSON minimal (reponses de Discord) ------------------------------------------------------

std::string json_str(const std::string &body, const char *key) {
    const std::string k = std::string("\"") + key + "\"";
    size_t p = body.find(k);
    if (p == std::string::npos) return {};
    p = body.find_first_not_of(" \t\r\n", body.find(':', p + k.size()) + 1);
    if (p == std::string::npos || body[p] != '"') return {};
    const size_t e = body.find('"', p + 1);
    return e == std::string::npos ? std::string() : body.substr(p + 1, e - p - 1);
}

bool json_array_has(const std::string &body, const char *key, const std::string &value) {
    const std::string k = std::string("\"") + key + "\"";
    const size_t p = body.find(k);
    if (p == std::string::npos) return false;
    const size_t a = body.find('[', p), b = body.find(']', p);
    if (a == std::string::npos || b == std::string::npos || b < a) return false;
    return body.substr(a, b - a).find("\"" + value + "\"") != std::string::npos;
}

// ---- serveur et role exiges --------------------------------------------------------------------

struct Gate {
    std::string guild = kDefaultGuild, role = kDefaultRole;
};

// Duree du bail (jours), 0 = sans limite ; fin du bail du laissez-passer charge (pour l'affichage).
std::atomic<int> g_lease_days{kLeaseDays};
std::atomic<int> g_lease_days_nightly{kLeaseDaysNightly};
std::atomic<long long> g_lease_end{0};
std::atomic<int> g_guest_days{0};          // essai invite propose (access.json « guest_days »), 0 = aucun
std::atomic<bool> g_guest_used{false};     // un essai a deja ete pris sur cet appareil
std::atomic<long long> g_guest_end{0};     // fin de l'essai en cours (affichage)
constexpr int kGuestMaxDays = 14;
constexpr long long kNoExpiry = 1LL << 60;
constexpr long long kSkewTolerance = 2 * 86400;   // l'horloge peut avoir 2 jours de retard sur la derniere date vue

Gate read_gate() {
    Gate g;
    int status = 0;
    std::string body;
    if (host::https_get(std::string("https://raw.githubusercontent.com/") + kRepo + "/main/access.json", status,
                        body) &&
        status == 200) {
        const std::string guild = json_str(body, "guild"), role = json_str(body, "role");
        if (!guild.empty()) g.guild = guild;
        if (!role.empty()) g.role = role;
        const std::string ld = json_str(body, "lease_days");
        if (!ld.empty() && std::isdigit(static_cast<unsigned char>(ld[0]))) {
            const int d = std::atoi(ld.c_str());
            if (d >= 0 && d <= kLeaseMaxDays) g_lease_days = d;
        }
        const std::string ln = json_str(body, "lease_days_nightly");
        if (!ln.empty() && std::isdigit(static_cast<unsigned char>(ln[0]))) {
            const int d = std::atoi(ln.c_str());
            if (d >= 0 && d <= kLeaseMaxDays) g_lease_days_nightly = d;
        }
        const std::string gd = json_str(body, "guest_days");
        if (!gd.empty() && std::isdigit(static_cast<unsigned char>(gd[0]))) {
            const int d = std::atoi(gd.c_str());
            if (d >= 0 && d <= kGuestMaxDays) g_guest_days = d;
        }
    }
    return g;
}

// ---- jetons ----------------------------------------------------------------------------------

struct Tokens {
    std::string access, refresh;
};

enum class Net { Ok, Refused, Offline };

Net token_request(const std::string &form, Tokens &t) {
    int status = 0;
    std::string body;
    if (!request("POST", std::string(kApi) + "/oauth2/token",
                             "Content-Type: application/x-www-form-urlencoded\r\nAccept: application/json\r\n", form,
                             status, body))
        return Net::Offline;
    if (status != 200) return Net::Refused;
    t.access = json_str(body, "access_token");
    t.refresh = json_str(body, "refresh_token");
    return t.access.empty() ? Net::Refused : Net::Ok;
}

void set(State *st, Access a, const std::string &detail = {}) {
    {
        std::lock_guard<std::mutex> l(st->m);
        st->detail = detail;
    }
    st->access = a;
    // rapport (onglet Report) : verdicts seulement, jamais de nom ni de jeton (13/10)
    static const char *const names[] = {"idle", "checking", "waiting for Discord", "OK (Member)", "sign-in needed",
                                        "not on the server", "no Member role", "Discord unreachable", "failed"};
    const int i = int(a);
    if (a != Access::Idle && a != Access::Checking && i >= 0 && i < 9)
        nj::report::add(std::string("Discord: ") + names[i] + (detail.empty() ? "" : " (" + detail + ")"));
}

// Appartenance au serveur et role, avec le jeton d'acces. Seuls un 404 (pas membre) et un 200 sans
// le role sont des refus ; 401 = jeton perime ; tout le reste (403, 429, 5xx) reste sans verdict.
Access member_verdict(State *st, const Tokens &t) {
    const std::string auth = "Authorization: Bearer " + t.access + "\r\n";
    int status = 0;
    std::string body;
    if (request("GET", std::string(kApi) + "/users/@me", auth, "", status, body) && status == 200) {
        std::string name = json_str(body, "global_name");
        if (name.empty()) name = json_str(body, "username");
        std::lock_guard<std::mutex> l(st->m);
        st->user = name;
    }
    const Gate g = read_gate();
    if (!request("GET", std::string(kApi) + "/users/@me/guilds/" + g.guild + "/member", auth, "",
                             status, body))
        return Access::Offline;
    if (status == 401) return Access::NeedLogin;
    if (status == 404) return Access::NotMember;   // serveur inconnu pour ce compte
    if (status != 200) return Access::Offline;     // Discord en difficulte : pas un refus
    if (!g.role.empty() && !json_array_has(body, "roles", g.role)) return Access::NoRole;
    return Access::Granted;
}

// ---- laissez-passer (13/10) ------------------------------------------------------------------
// Une connexion par version : un compte reconnu membre avec le role recoit un laissez-passer pour
// cette version (kAppVersion), garde avec le jeton de rafraichissement (host::secret_save : DPAPI
// sous Windows, lie a l'utilisateur et a la machine ; stockage prive de l'application sous
// Android). Avec lui, le jeu demarre tout de suite, meme sans reseau, sans limite de temps ; la
// verification se refait en silence quand Discord repond, et seul un refus net (plus membre, plus
// le role) retire le laissez-passer, au lancement suivant. Une nouvelle version redemande Discord.

struct Pass {
    std::string version, user, refresh;
    bool granted = false;
    long long lease_end = 0;   // fin du bail (secondes Unix), 0 = pas de bail (ancien format)
    long long last_seen = 0;   // derniere date vue : l'horloge ne peut pas la faire reculer
    long long guest_end = 0;   // fin de l'essai invite (0 = jamais pris) ; reste inscrite apres la fin : un seul essai
};

// Cle du sceau : jamais en clair dans le binaire, reconstruite au moment du calcul.
std::string pass_key() {
    static const uint8_t k[] = {0x3d, 0x8a, 0x51, 0xe4, 0x72, 0x19, 0xc6, 0xab, 0x0f, 0x94, 0x5e, 0xd3,
                                0x27, 0xb8, 0x6c, 0x41, 0xfa, 0x13, 0x88, 0x5d, 0xe2, 0x37, 0x9c, 0x60};
    std::string s;
    for (size_t i = 0; i < sizeof(k); ++i) s.push_back(char(k[i] ^ uint8_t(0xa5 + 29 * i)));
    return s;
}

// HMAC-SHA-256 (RFC 2104), cle de moins de 64 octets
std::array<uint8_t, 32> hmac(const std::string &key, const std::string &msg) {
    std::string ip(64, '\x36'), op(64, '\x5c');
    for (size_t i = 0; i < key.size(); ++i) {
        ip[i] = char(ip[i] ^ key[i]);
        op[i] = char(op[i] ^ key[i]);
    }
    const auto in = sha256(ip + msg);
    return sha256(op + std::string(reinterpret_cast<const char *>(in.data()), in.size()));
}

std::string pass_tag(const Pass &p) {
    // lie a l'appareil (Android : ANDROID_ID ; Windows : deja lie par DPAPI, identifiant vide)
    const auto d = hmac(pass_key(), p.version + "|" + (p.granted ? "1" : "0") + "|" + p.user + "|" + p.refresh +
                                        "|" + std::to_string(p.lease_end) + "|" + std::to_string(p.last_seen) + "|" +
                                        host::device_id());
    return base64url(d.data(), 18);
}

// Sceau du format NJP4 (0.1.7) : comme celui du NJP3, avec la fin de l'essai invite.
std::string pass_tag4(const Pass &p) {
    const auto d = hmac(pass_key(), p.version + "|" + (p.granted ? "1" : "0") + "|" + p.user + "|" + p.refresh +
                                        "|" + std::to_string(p.lease_end) + "|" + std::to_string(p.last_seen) + "|g" +
                                        std::to_string(p.guest_end) + "|" + host::device_id());
    return base64url(d.data(), 18);
}

// Sceau du format NJP2 (0.1.1 a 0.1.5) : relu pour garder le compte enregistre apres la mise a jour.
std::string pass_tag_v2(const Pass &p) {
    const auto d = hmac(pass_key(), p.version + "|" + (p.granted ? "1" : "0") + "|" + p.user + "|" + p.refresh +
                                        "|" + host::device_id());
    return base64url(d.data(), 18);
}

std::vector<std::string> split_lines(const std::string &s, size_t a) {
    std::vector<std::string> f;
    for (size_t b; (b = s.find('\n', a)) != std::string::npos; a = b + 1) f.push_back(s.substr(a, b - a));
    f.push_back(s.substr(a));
    return f;
}

Pass load_pass() {
    Pass p;
    const std::string s = host::secret_load();
    if (s.rfind("NJP4\n", 0) == 0) {
        const auto f = split_lines(s, 5);
        if (f.size() != 8) return {};
        p.version = f[0];
        p.granted = f[1] == "1";
        p.user = f[2];
        p.refresh = f[3];
        p.lease_end = std::atoll(f[4].c_str());
        p.last_seen = std::atoll(f[5].c_str());
        p.guest_end = std::atoll(f[6].c_str());
        if (pass_tag4(p) != f[7]) return {};   // modifie a la main : plus rien
        return p;
    }
    if (s.rfind("NJP3\n", 0) == 0) {
        const auto f = split_lines(s, 5);
        if (f.size() != 7) return {};
        p.version = f[0];
        p.granted = f[1] == "1";
        p.user = f[2];
        p.refresh = f[3];
        p.lease_end = std::atoll(f[4].c_str());
        p.last_seen = std::atoll(f[5].c_str());
        if (pass_tag(p) != f[6]) return {};   // modifie a la main : plus rien
        return p;
    }
    if (s.rfind("NJP", 0) == 0 && s.rfind("NJP2\n", 0) != 0) return {};   // sceau d'avant : se reconnecter
    if (s.rfind("NJP2\n", 0) != 0) {   // ancien format : le jeton seul
        p.refresh = s;
        return p;
    }
    const auto f = split_lines(s, 5);
    if (f.size() != 5) return {};
    p.version = f[0];
    p.granted = f[1] == "1";
    p.user = f[2];
    p.refresh = f[3];
    if (pass_tag_v2(p) != f[4]) return {};   // modifie a la main : plus rien
    p.granted = false;   // pas de bail dans ce format : une verification en ligne en donnera un
    return p;
}

long long now_s() { return static_cast<long long>(std::time(nullptr)); }

// Le laissez-passer vaut-il encore sans reseau ? Version identique, bail en cours, et horloge
// coherente avec la derniere date vue (reculer l'horloge ne prolonge rien).
bool lease_ok(const Pass &p) {
    if (!p.granted || p.version != app_version() || p.lease_end == 0) return false;
    const long long now = now_s();
    if (p.last_seen - now > kSkewTolerance) return false;
    return std::max(now, p.last_seen) < p.lease_end;
}

void save_pass(Pass p) {
    const long long now = now_s();
    if (p.last_seen - now <= kSkewTolerance) p.last_seen = std::max(p.last_seen, now);
    g_lease_end = p.granted ? p.lease_end : 0;
    host::secret_save(std::string("NJP4\n") + p.version + "\n" + (p.granted ? "1" : "0") + "\n" + p.user + "\n" +
                      p.refresh + "\n" + std::to_string(p.lease_end) + "\n" + std::to_string(p.last_seen) + "\n" +
                      std::to_string(p.guest_end) + "\n" + pass_tag4(p));
}

// Verdict de Discord applique au laissez-passer.
void apply_verdict(Pass &p, Access a, const std::string &user) {
    if (a == Access::Granted) {
        p.granted = true;
        p.version = app_version();
        if (!user.empty()) p.user = user;
        // Discord vient de repondre : le bail repart (le cadran vient d'access.json, 0 = sans limite)
        const long long now = now_s();
        const int days = is_nightly() ? g_lease_days_nightly.load() : g_lease_days.load();
        p.lease_end = days <= 0 ? kNoExpiry : now + static_cast<long long>(days) * 86400;
        p.last_seen = now;
    } else if (a == Access::NotMember || a == Access::NoRole) {
        p.granted = false;
    } else if (a == Access::NeedLogin) {
        p.refresh.clear();
    }
    if (a != Access::Offline) save_pass(p);
}

// Verification silencieuse d'un laissez-passer (fil a part, le jeu demarre sans l'attendre).
void recheck_pass(Pass p) {
    if (p.refresh.empty()) return;
    Tokens t;
    const Net r = token_request(std::string("client_id=") + kClientId +
                                    "&grant_type=refresh_token&refresh_token=" + url_encode(p.refresh),
                                t);
    if (r == Net::Offline) return;
    if (r == Net::Refused) {   // jeton refuse : le laissez-passer reste, la connexion sera redemandee a la version suivante
        p.refresh.clear();
        save_pass(p);
        return;
    }
    if (!t.refresh.empty()) p.refresh = t.refresh;
    State tmp;
    const Access a = member_verdict(&tmp, t);
    apply_verdict(p, a, tmp.user);
    if (a == Access::Offline) save_pass(p);   // garder le jeton tourne
}

// Verdict de Discord : laissez-passer mis a jour, ecran du lanceur.
void finish(State *st, Pass &p, const Tokens &t) {
    if (!t.refresh.empty()) p.refresh = t.refresh;
    const Access a = member_verdict(st, t);
    std::string user;
    {
        std::lock_guard<std::mutex> l(st->m);
        user = st->user;
    }
    apply_verdict(p, a, user);
    set(st, a);
}

// ---- reponse du navigateur sur 127.0.0.1 -----------------------------------------------------

std::string query_value(const std::string &request_line, const char *key) {
    const std::string k = std::string(key) + "=";
    size_t p = request_line.find("?" + k);
    if (p == std::string::npos) p = request_line.find("&" + k);
    if (p == std::string::npos) return {};
    p += k.size() + 1;
    const size_t e = request_line.find_first_of("& ", p);
    return request_line.substr(p, e == std::string::npos ? std::string::npos : e - p);
}

void reply(sock_t c, const char *message) {
    const std::string html = std::string("<!doctype html><meta charset=utf-8><title>Nightjar</title>"
                                         "<body style=\"background:#0b0b0d;color:#ddd;font-family:sans-serif;"
                                         "text-align:center;padding-top:20vh\"><h1 style=\"color:#b0141e\">"
                                         "NIGHTJAR</h1><p>") +
                             message + "</p></body>";
    const std::string r = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " +
                          std::to_string(html.size()) + "\r\nConnection: close\r\n\r\n" + html;
    send(c, r.data(), (int)r.size(), 0);
}

// Attend « GET /callback?code=...&state=... » ; rend le code, ou "" (annule, refus, delai).
std::string wait_for_code(State *st, sock_t srv, const std::string &state, std::string &error) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    while (!st->cancel && std::chrono::steady_clock::now() < deadline) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(srv, &fds);
        timeval tv{0, 250000};
        if (select((int)srv + 1, &fds, nullptr, nullptr, &tv) <= 0) continue;
        sock_t c = accept(srv, nullptr, nullptr);
        if (c == INVALID_SOCKET) continue;
        char buf[4096];
        const int n = recv(c, buf, sizeof(buf) - 1, 0);
        const std::string req(buf, n > 0 ? (size_t)n : 0);
        const std::string line = req.substr(0, req.find("\r\n"));
        if (line.rfind("GET /callback", 0) != 0) {   // favicon et autres : ignores
            NJ_CLOSESOCK(c);
            continue;
        }
        const std::string code = query_value(line, "code");
        if (code.empty() || query_value(line, "state") != state) {
            error = query_value(line, "error");
            if (error.empty()) error = "invalid response";
            reply(c, "Sign-in was cancelled. You can close this page and try again in Nightjar.");
            NJ_CLOSESOCK(c);
            return {};
        }
#if defined(__ANDROID__)
        // retour direct dans l'application (activite singleTask : la meme, pas une seconde)
        reply(c, "You are signed in.</p><p><a href=\"intent:#Intent;action=android.intent.action.MAIN;"
                 "category=android.intent.category.LAUNCHER;package=com.ayano.nightjar;end\" "
                 "style=\"display:inline-block;margin-top:2em;padding:.8em 2em;background:#b0141e;"
                 "color:#fff;border-radius:8px;text-decoration:none;font-size:1.2em\">Return to Nightjar</a>");
#else
        reply(c, "You are signed in. You can close this page and go back to Nightjar.");
#endif
        NJ_CLOSESOCK(c);
        return code;
    }
    error = st->cancel ? "cancelled" : "timeout";
    return {};
}

#if defined(__ANDROID__) || defined(__APPLE__)
// Connexion mobile (13/10 ; iOS le 14/10 : fenetre ASWebAuthenticationSession, host_ios.mm) : l'ecran « Authorize » de l'appli Discord, sur le compte deja connecte
// (route discord://action/oauth2/authorize de l'appli, essayee sur la 348.10), sinon un onglet du
// navigateur. Retour par discord-<id>:/authorize/callback (AuthCallback.java), sans serveur local.
constexpr const char *kRedirectApp = "discord-1559162514154201128:/authorize/callback";

void login_mobile(State *st) {
    const std::string verifier = random_token(64), state = random_token(24);
    const auto digest = sha256(verifier);
    const std::string params = std::string("response_type=code&client_id=") + kClientId +
                               "&scope=" + url_encode("identify guilds.members.read") +
                               "&redirect_uri=" + url_encode(kRedirectApp) +
                               "&code_challenge=" + base64url(digest.data(), digest.size()) +
                               "&code_challenge_method=S256&state=" + state;
    const std::string app = "discord://action/oauth2/authorize?" + params;
    const std::string web = "https://discord.com/oauth2/authorize?" + params;
    st->use_browser = false;
    if (!host::auth_open(app, web, true)) return set(st, Access::Failed, "couldn't open Discord");

    std::string uri;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(10);
    for (;;) {
        if (st->cancel) return set(st, Access::NeedLogin);
        if (std::chrono::steady_clock::now() > deadline) return set(st, Access::Failed, "timeout");
        if (st->use_browser.exchange(false)) host::auth_open(app, web, false);
        if (host::auth_poll(uri)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
    const std::string error = query_value(uri, "error");
    if (!error.empty()) return set(st, error == "access_denied" ? Access::NeedLogin : Access::Failed, error);
    const std::string code = query_value(uri, "code");
    if (code.empty() || query_value(uri, "state") != state) return set(st, Access::Failed, "invalid response");
    wait_foreground(st->cancel);

    Tokens t;
    const Net r = token_request(std::string("client_id=") + kClientId + "&grant_type=authorization_code&code=" +
                                    url_encode(code) + "&redirect_uri=" + url_encode(kRedirectApp) +
                                    "&code_verifier=" + verifier,
                                t);
    if (r == Net::Offline) return set(st, Access::Offline);
    if (r == Net::Refused) return set(st, Access::Failed, "Discord refused the sign-in");
    Pass p;
    finish(st, p, t);
}
#endif

}  // namespace

void check_saved(State *st) {
    set(st, Access::Checking);
    Pass p = load_pass();
    g_guest_used = p.guest_end != 0;
    if (!p.granted) std::thread([] { read_gate(); }).detach();   // apprend « guest_days » avant l'ecran de connexion
    if (!kOnlineEveryLaunch && lease_ok(p)) {
        // laissez-passer de cette version, bail en cours : on joue tout de suite, Discord est revu en silence
        {
            std::lock_guard<std::mutex> l(st->m);
            st->user = p.user;
        }
        save_pass(p);   // derniere date vue
        set(st, Access::Granted);
        std::thread(recheck_pass, p).detach();
        return;
    }
    if (!kOnlineEveryLaunch && p.guest_end != 0 && p.refresh.empty()) {   // essai invite en cours ?
        const long long now = now_s();
        if (p.last_seen - now <= kSkewTolerance && std::max(now, p.last_seen) < p.guest_end) {
            {
                std::lock_guard<std::mutex> l(st->m);
                st->user = "Guest";
            }
            st->guest = true;
            g_guest_end = p.guest_end;
            save_pass(p);   // derniere date vue
            return set(st, Access::Granted);
        }
    }
    if (p.refresh.empty()) return set(st, Access::NeedLogin);
    Tokens t;
    const Net r = token_request(std::string("client_id=") + kClientId + "&grant_type=refresh_token&refresh_token=" +
                                    url_encode(p.refresh),
                                t);
    if (r == Net::Offline)
        return set(st, Access::Offline, p.granted && p.version == app_version() ? "lease" : std::string());
    if (r == Net::Refused) {   // compte deconnecte, ou rafraichissement refuse sans secret
        host::secret_save("");
        return set(st, Access::NeedLogin);
    }
    finish(st, p, t);
}

void login(State *st) {
    st->cancel = false;
    set(st, Access::Waiting);
#if defined(__ANDROID__) || defined(__APPLE__)
    login_mobile(st);
    return;
#endif
#if defined(_WIN32)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    sock_t srv = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (srv == INVALID_SOCKET || bind(srv, (sockaddr *)&addr, sizeof(addr)) != 0 || listen(srv, 4) != 0) {
        if (srv != INVALID_SOCKET) NJ_CLOSESOCK(srv);
        return set(st, Access::Failed, "local port 53134 is busy");
    }

    const std::string verifier = random_token(64), state = random_token(24);
    const auto digest = sha256(verifier);
    const std::string challenge = base64url(digest.data(), digest.size());
    const std::string url = std::string("https://discord.com/oauth2/authorize?response_type=code&client_id=") +
                            kClientId + "&scope=" + url_encode("identify guilds.members.read") +
                            "&redirect_uri=" + url_encode(kRedirect) + "&code_challenge=" + challenge +
                            "&code_challenge_method=S256&state=" + state;
    SDL_OpenURL(url.c_str());

    std::string error;
    const std::string code = wait_for_code(st, srv, state, error);
    NJ_CLOSESOCK(srv);
    if (code.empty()) return set(st, error == "cancelled" ? Access::NeedLogin : Access::Failed, error);
    wait_foreground(st->cancel);

    Tokens t;
    const Net r = token_request(std::string("client_id=") + kClientId + "&grant_type=authorization_code&code=" +
                                    url_encode(code) + "&redirect_uri=" + url_encode(kRedirect) +
                                    "&code_verifier=" + verifier,
                                t);
    if (r == Net::Offline) return set(st, Access::Offline);
    if (r == Net::Refused) return set(st, Access::Failed, "Discord refused the sign-in");
    Pass p;
    finish(st, p, t);
}

void forget() { host::secret_save(""); }

void frame_tick() { g_last_frame_ms = now_ms(); }

bool is_nightly() { return std::strstr(app_version(), "nightly") != nullptr; }

int guest_offer_days() { return g_guest_used.load() ? 0 : g_guest_days.load(); }

void start_guest(State *st) {
    const int days = g_guest_days.load();
    Pass p = load_pass();
    if (days <= 0 || p.guest_end != 0) return set(st, Access::NeedLogin);   // pas d'essai propose, ou deja pris
    const long long now = now_s();
    p.guest_end = now + static_cast<long long>(days) * 86400;
    p.last_seen = now;
    save_pass(p);
    g_guest_used = true;
    g_guest_end = p.guest_end;
    {
        std::lock_guard<std::mutex> l(st->m);
        st->user = "Guest";
    }
    st->guest = true;
    set(st, Access::Granted);
}

std::string lease_line() {
    if (const long long ge = g_guest_end.load(); ge != 0) {   // essai invite en cours
        const long long gl = std::max(0LL, (ge - now_s() + 86399) / 86400);
        char gb[200];
        std::snprintf(gb, sizeof gb, "Guest access: %lld day%s left. Join the Discord server to keep playing.", gl,
                      gl == 1 ? "" : "s");
        return gb;
    }
    const long long end = g_lease_end.load();
    if (end == 0 || end >= kNoExpiry) return {};
    const long long left = end - now_s();
    char buf[200];
    if (left < 7 * 86400) {
        const long long days = std::max(0LL, (left + 86399) / 86400);
        std::snprintf(buf, sizeof buf, "Offline access ends in %lld day%s. Connect to the internet once to renew it.", days,
                      days == 1 ? "" : "s");
    } else {
        const std::time_t t = static_cast<std::time_t>(end);
        char date[40] = "";
        if (const std::tm *tmv = std::localtime(&t)) std::strftime(date, sizeof date, "%d/%m/%Y", tmv);
        std::snprintf(buf, sizeof buf, "Offline access valid until %s. Playing online renews it.", date);
    }
    return buf;
}

}  // namespace nj::launcher::discord

namespace nj::launcher {

const char *app_version() {
    // VERSION_CACHEE_DEBUT (tools/version_cachee.py)
    static const uint8_t v[] = {0x6b, 0xaa, 0x9c, 0xf8, 0xc7};
    // VERSION_CACHEE_FIN
    static char s[sizeof(v) + 1];
    if (!s[0]) {
        for (size_t i = 0; i < sizeof(v); ++i) s[i] = char(v[i] ^ uint8_t(0x5b + 41 * i));
        s[sizeof(v)] = 0;
    }
    return s;
}

}  // namespace nj::launcher
