#pragma once
// Ce que le lanceur demande au systeme, une implementation par plateforme (host.cpp) :
// HTTPS (WinHTTP sous Windows, HttpsURLConnection par JNI sous Android), le selecteur de fichier
// natif, et un repertoire ou garder ses reglages.
#include <string>
#include <vector>

namespace nj::launcher::host {

// GET HTTPS avec verification du certificat par le systeme. false si le reseau ne repond pas.
bool https_get(const std::string &url, int &status, std::string &body);
// Requete HTTPS quelconque (connexion Discord) : headers = lignes « Nom: valeur » separees par CRLF,
// body envoye tel quel (vide pour GET).
bool https_request(const std::string &method, const std::string &url, const std::string &headers,
                   const std::string &body, int &status, std::string &out);

// Petit secret de l'application (jeton de rafraichissement Discord) : chiffre pour l'utilisateur
// courant (DPAPI) sous Windows, dans le stockage prive de l'application sous Android. "" si absent.
bool secret_save(const std::string &value);
std::string secret_load();

// Identifiant de l'appareil qui lie le laissez-passer Discord a ce telephone (Android : ANDROID_ID,
// propre a l'appareil et a la cle de signature). "" ailleurs (Windows : DPAPI lie deja a la machine).
std::string device_id();

// Connexion Discord sur mobile (Android, 13/10) : ouvre la demande d'autorisation dans l'appli
// Discord (app_url, discord://action/oauth2/authorize) si prefer_app et qu'elle est installee, sinon
// web_url dans un onglet du navigateur. auth_poll rend l'adresse de retour
// (discord-<id>:/authorize/callback?...) une fois recue. Ailleurs : false.
bool auth_open(const std::string &app_url, const std::string &web_url, bool prefer_app);
bool auth_poll(std::string &uri);

// Menu F10 (13/10). Import : choisir un fichier (.gci, .raw) ; import_poll rend 1 et son chemin local,
// -1 annule, 0 en cours (Windows : fenetre du systeme, reponse tout de suite).
void import_begin();
int import_poll(std::string &path);
// Android : enregistrer une copie de src ou le joueur le choisit ; export_poll 1 fait, -1 echec, 0 en cours.
void export_begin(const std::string &src, const std::string &name);
int export_poll();
// Android : menu Partager (texte + fichiers joints). Windows : non (dossier ouvert a la place).
bool share(const std::string &text, const std::vector<std::string> &files);
// Relance l'application dans un processus neuf (apres la preparation du jeu : la 1re session gardait un rendu fautif).
// Windows : nouveau processus puis sortie ; Android : activite relancee, processus tue. Faux : relance impossible.
bool restart_app();
// Ouvre un dossier dans l'explorateur (Windows).
void open_folder(const std::string &dir);

// Repertoire de donnees de l'application (reglages du lanceur, sys/ extrait du disque).
std::string data_dir();

// Selecteur de disque (disc : 1 ou 2). begin_pick() ouvre le selecteur du systeme ; poll_pick()
// rend 1 et le chemin quand l'utilisateur a choisi, -1 s'il a annule, 0 tant qu'il choisit.
void begin_pick(int disc);
int poll_pick(std::string &path);

// Le disque choisi a un lancement precedent, s'il est toujours accessible ("" sinon).
std::string saved_disc(int disc);
void save_disc(const std::string &path, int disc);

}  // namespace nj::launcher::host
