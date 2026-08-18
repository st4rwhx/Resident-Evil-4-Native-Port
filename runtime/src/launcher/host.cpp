#include "host.h"

#include <SDL.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <shellapi.h>
#elif defined(__ANDROID__)
#include <jni.h>
#endif

namespace nj::launcher::host {

namespace {
std::string g_picked;
int g_pick_state = 0;  // 0 en cours ou rien, 1 choisi, -1 annule
}

#if defined(_WIN32)

namespace {
std::wstring widen(const std::string &s) {
    std::wstring w(MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), (int)w.size());
    if (!w.empty()) w.pop_back();
    return w;
}
std::string narrow(const wchar_t *w) {
    std::string s(WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), (int)s.size(), nullptr, nullptr);
    if (!s.empty()) s.pop_back();
    return s;
}
}  // namespace

bool https_request(const std::string &method, const std::string &url, const std::string &headers,
                   const std::string &body, int &status, std::string &out) {
    const std::wstring wurl = widen(url);
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256], path[2048];
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS) return false;

    bool ok = false;
    HINTERNET s = WinHttpOpen(L"Nightjar", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET c = s ? WinHttpConnect(s, host, uc.nPort, 0) : nullptr;
    HINTERNET r = c ? WinHttpOpenRequest(c, widen(method).c_str(), path, nullptr, WINHTTP_NO_REFERER,
                                         WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                    : nullptr;
    if (r) {
        WinHttpSetTimeouts(r, 8000, 8000, 8000, 8000);
        const std::wstring hdr = widen(headers);
        if (WinHttpSendRequest(r, hdr.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : hdr.c_str(), (DWORD)-1L,
                               body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(), (DWORD)body.size(),
                               (DWORD)body.size(), 0) &&
            WinHttpReceiveResponse(r, nullptr)) {
            DWORD code = 0, len = sizeof(code);
            WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &code, &len, WINHTTP_NO_HEADER_INDEX);
            status = (int)code;
            out.clear();
            for (;;) {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(r, &avail) || !avail) break;
                std::string chunk(avail, '\0');
                DWORD got = 0;
                if (!WinHttpReadData(r, chunk.data(), avail, &got) || !got) break;
                out.append(chunk.data(), got);
                if (out.size() > (1u << 20)) break;
            }
            ok = true;
        }
        WinHttpCloseHandle(r);
    }
    if (c) WinHttpCloseHandle(c);
    if (s) WinHttpCloseHandle(s);
    return ok;
}

bool https_get(const std::string &url, int &status, std::string &body) {
    return https_request("GET", url, "Accept: application/vnd.github+json\r\n", "", status, body);
}

// Secret chiffre par DPAPI : lisible seulement par cet utilisateur Windows, sur cette machine.
bool secret_save(const std::string &value) {
    const std::string file = data_dir() + "compte.bin";
    if (value.empty()) {
        std::remove(file.c_str());
        return true;
    }
    DATA_BLOB in{(DWORD)value.size(), (BYTE *)value.data()}, enc{};
    if (!CryptProtectData(&in, L"Nightjar", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &enc))
        return false;
    std::ofstream o(file, std::ios::binary | std::ios::trunc);
    o.write((const char *)enc.pbData, enc.cbData);
    LocalFree(enc.pbData);
    return (bool)o;
}

std::string secret_load() {
    std::ifstream f(data_dir() + "compte.bin", std::ios::binary);
    const std::string blob((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (blob.empty()) return {};
    DATA_BLOB in{(DWORD)blob.size(), (BYTE *)blob.data()}, dec{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &dec)) return {};
    std::string v((const char *)dec.pbData, dec.cbData);
    LocalFree(dec.pbData);
    return v;
}

std::string data_dir() {
    char *p = SDL_GetPrefPath("Nightjar", "Nightjar");
    std::string d = p ? p : "";
    SDL_free(p);
    return d;
}

void begin_pick(int disc) {
    wchar_t file[MAX_PATH * 4] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"GameCube disc image (*.iso;*.gcm;*.rvz)\0*.iso;*.gcm;*.rvz\0All files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH * 4;
    ofn.lpstrTitle = disc == 2 ? L"Resident Evil 4 - Disc 2" : L"Resident Evil 4 - Disc 1";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) {
        g_picked = narrow(file);
        g_pick_state = 1;
    } else {
        g_pick_state = -1;
    }
}

namespace {
int g_import_state = 0;
std::string g_import_path;
}  // namespace

void import_begin() {
    wchar_t file[MAX_PATH * 4] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"GameCube save (*.gci;*.raw)\0*.gci;*.raw\0All files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH * 4;
    ofn.lpstrTitle = L"Import a Resident Evil 4 save";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) {
        g_import_path = narrow(file);
        g_import_state = 1;
    } else {
        g_import_state = -1;
    }
}

int import_poll(std::string &path) {
    const int s = g_import_state;
    g_import_state = 0;
    if (s == 1) path = g_import_path;
    return s;
}

void export_begin(const std::string &, const std::string &) {}
int export_poll() { return -1; }
bool share(const std::string &, const std::vector<std::string> &) { return false; }

bool restart_app() {
    wchar_t exe[MAX_PATH * 2];
    const DWORD n = GetModuleFileNameW(nullptr, exe, (DWORD)(sizeof exe / sizeof exe[0]));
    if (!n || n >= sizeof exe / sizeof exe[0]) return false;
    std::wstring cmd = std::wstring(L"\"") + exe + L"\" --restarted";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    ExitProcess(0);
    return true;
}

void open_folder(const std::string &dir) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::u8path(dir), ec);
    const std::wstring w = std::filesystem::u8path(dir).wstring();
    ShellExecuteW(nullptr, L"open", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

std::string saved_disc(int disc) {
    std::ifstream f(std::filesystem::path(data_dir()) / (disc == 2 ? "disc2.txt" : "disc.txt"));
    std::string p;
    std::getline(f, p);
    return !p.empty() && std::filesystem::exists(std::filesystem::u8path(p)) ? p : std::string();
}

void save_disc(const std::string &path, int disc) {
    std::ofstream(std::filesystem::path(data_dir()) / (disc == 2 ? "disc2.txt" : "disc.txt"), std::ios::trunc)
        << path << "\n";
}

#elif defined(__ANDROID__)

// Tout passe par des methodes statiques de RE4Activity. La classe se retrouve depuis l'objet
// activite et non par FindClass, qui depuis un fil natif ne voit pas les classes de l'application.
namespace {
struct Jni {
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    jobject act = (jobject)SDL_AndroidGetActivity();
    jclass cls = act ? env->GetObjectClass(act) : nullptr;
    ~Jni() {
        if (cls) env->DeleteLocalRef(cls);
        if (act) env->DeleteLocalRef(act);
    }
    jmethodID m(const char *name, const char *sig) {
        jmethodID id = cls ? env->GetStaticMethodID(cls, name, sig) : nullptr;
        if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
        return id;
    }
};

std::string fd_path(int fd) { return "/proc/self/fd/" + std::to_string(fd); }
}  // namespace

bool auth_open(const std::string &app_url, const std::string &web_url, bool prefer_app) {
    Jni j;
    jmethodID id = j.m("openDiscordAuth", "(Ljava/lang/String;Ljava/lang/String;Z)Z");
    if (!id) return false;
    jstring ja = j.env->NewStringUTF(app_url.c_str()), jw = j.env->NewStringUTF(web_url.c_str());
    const jboolean ok = j.env->CallStaticBooleanMethod(j.cls, id, ja, jw, (jboolean)prefer_app);
    j.env->DeleteLocalRef(ja);
    j.env->DeleteLocalRef(jw);
    if (j.env->ExceptionCheck()) {
        j.env->ExceptionClear();
        return false;
    }
    return ok;
}

bool auth_poll(std::string &uri) {
    Jni j;
    jmethodID id = j.m("pollDiscordAuth", "()Ljava/lang/String;");
    if (!id) return false;
    jstring res = (jstring)j.env->CallStaticObjectMethod(j.cls, id);
    if (j.env->ExceptionCheck()) {
        j.env->ExceptionClear();
        return false;
    }
    if (!res) return false;
    const char *s = j.env->GetStringUTFChars(res, nullptr);
    uri = s ? s : "";
    j.env->ReleaseStringUTFChars(res, s);
    j.env->DeleteLocalRef(res);
    return !uri.empty();
}

void import_begin() {
    Jni j;
    if (jmethodID id = j.m("pickImportFile", "()V")) j.env->CallStaticVoidMethod(j.cls, id);
    if (j.env->ExceptionCheck()) j.env->ExceptionClear();
}

int import_poll(std::string &path) {
    Jni j;
    jmethodID id = j.m("pollImportFile", "()Ljava/lang/String;");
    if (!id) return -1;
    jstring res = (jstring)j.env->CallStaticObjectMethod(j.cls, id);
    if (j.env->ExceptionCheck()) {
        j.env->ExceptionClear();
        return -1;
    }
    if (!res) return 0;   // en cours
    const char *s = j.env->GetStringUTFChars(res, nullptr);
    path = s ? s : "";
    j.env->ReleaseStringUTFChars(res, s);
    j.env->DeleteLocalRef(res);
    return path.empty() ? -1 : 1;
}

void export_begin(const std::string &src, const std::string &name) {
    Jni j;
    jmethodID id = j.m("exportFile", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (!id) return;
    jstring a = j.env->NewStringUTF(src.c_str()), b = j.env->NewStringUTF(name.c_str());
    j.env->CallStaticVoidMethod(j.cls, id, a, b);
    j.env->DeleteLocalRef(a);
    j.env->DeleteLocalRef(b);
    if (j.env->ExceptionCheck()) j.env->ExceptionClear();
}

int export_poll() {
    Jni j;
    jmethodID id = j.m("pollExport", "()I");
    const int r = id ? j.env->CallStaticIntMethod(j.cls, id) : -1;
    if (j.env->ExceptionCheck()) {
        j.env->ExceptionClear();
        return -1;
    }
    return r;
}

bool restart_app() {
    Jni j;
    jmethodID id = j.m("restartApp", "()V");
    if (!id) return false;
    j.env->CallStaticVoidMethod(j.cls, id);
    if (j.env->ExceptionCheck()) {
        j.env->ExceptionClear();
        return false;
    }
    return true;
}

bool share(const std::string &text, const std::vector<std::string> &files) {
    Jni j;
    jmethodID id = j.m("shareReport", "(Ljava/lang/String;[Ljava/lang/String;)Z");
    if (!id) return false;
    jclass str_cls = j.env->FindClass("java/lang/String");
    jobjectArray arr = j.env->NewObjectArray((jsize)files.size(), str_cls, nullptr);
    for (size_t i = 0; i < files.size(); ++i) {
        jstring s = j.env->NewStringUTF(files[i].c_str());
        j.env->SetObjectArrayElement(arr, (jsize)i, s);
        j.env->DeleteLocalRef(s);
    }
    jstring t = j.env->NewStringUTF(text.c_str());
    const jboolean ok = j.env->CallStaticBooleanMethod(j.cls, id, t, arr);
    j.env->DeleteLocalRef(t);
    j.env->DeleteLocalRef(arr);
    j.env->DeleteLocalRef(str_cls);
    if (j.env->ExceptionCheck()) {
        j.env->ExceptionClear();
        return false;
    }
    return ok;
}

void open_folder(const std::string &) {}

std::string device_id() {
    static const std::string id = [] {
        Jni j;
        if (!j.act) return std::string();
        JNIEnv *e = j.env;
        std::string r;
        jmethodID gcr = e->GetMethodID(j.cls, "getContentResolver", "()Landroid/content/ContentResolver;");
        jobject cr = gcr ? e->CallObjectMethod(j.act, gcr) : nullptr;
        jclass sec = e->FindClass("android/provider/Settings$Secure");   // classe du systeme : visible
        if (e->ExceptionCheck()) e->ExceptionClear();
        jmethodID gs = sec ? e->GetStaticMethodID(sec, "getString",
                                                  "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;")
                           : nullptr;
        if (e->ExceptionCheck()) e->ExceptionClear();
        if (cr && gs) {
            jstring key = e->NewStringUTF("android_id");
            jstring v = (jstring)e->CallStaticObjectMethod(sec, gs, cr, key);
            if (e->ExceptionCheck()) {
                e->ExceptionClear();
                v = nullptr;
            }
            if (v) {
                const char *s = e->GetStringUTFChars(v, nullptr);
                if (s) r = s;
                e->ReleaseStringUTFChars(v, s);
                e->DeleteLocalRef(v);
            }
            e->DeleteLocalRef(key);
        }
        if (sec) e->DeleteLocalRef(sec);
        if (cr) e->DeleteLocalRef(cr);
        return r;
    }();
    return id;
}

bool https_request(const std::string &method, const std::string &url, const std::string &headers,
                   const std::string &body, int &status, std::string &out) {
    Jni j;
    jmethodID id = j.m("httpsRequest",
                       "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    if (!id) return false;
    jstring jm = j.env->NewStringUTF(method.c_str()), ju = j.env->NewStringUTF(url.c_str());
    jstring jh = j.env->NewStringUTF(headers.c_str()), jb = j.env->NewStringUTF(body.c_str());
    jstring res = (jstring)j.env->CallStaticObjectMethod(j.cls, id, jm, ju, jh, jb);
    for (jstring x : {jm, ju, jh, jb}) j.env->DeleteLocalRef(x);
    if (j.env->ExceptionCheck()) { j.env->ExceptionClear(); return false; }
    if (!res) return false;
    const char *s = j.env->GetStringUTFChars(res, nullptr);
    std::string all = s ? s : "";
    j.env->ReleaseStringUTFChars(res, s);
    j.env->DeleteLocalRef(res);
    // Forme rendue par Java : "<code>\n<corps>"
    const size_t nl = all.find('\n');
    if (nl == std::string::npos) return false;
    status = std::atoi(all.substr(0, nl).c_str());
    out = all.substr(nl + 1);
    return true;
}

bool https_get(const std::string &url, int &status, std::string &body) {
    return https_request("GET", url, "Accept: application/vnd.github+json\r\n", "", status, body);
}

namespace {
std::string secret_file() {
    const char *d = SDL_AndroidGetInternalStoragePath();   // prive : aucune autre application n'y lit
    return d ? std::string(d) + "/compte.bin" : std::string();
}
}  // namespace

bool secret_save(const std::string &value) {
    const std::string file = secret_file();
    if (file.empty()) return false;
    if (value.empty()) {
        std::remove(file.c_str());
        return true;
    }
    std::ofstream o(file, std::ios::binary | std::ios::trunc);
    o << value;
    return (bool)o;
}

std::string secret_load() {
    std::ifstream f(secret_file(), std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

std::string data_dir() {
    const char *d = SDL_AndroidGetExternalStoragePath();
    return d ? std::string(d) + "/" : std::string();
}

void begin_pick(int disc) {
    g_pick_state = 0;
    Jni j;
    if (jmethodID id = j.m("pickDisc", "(I)V")) j.env->CallStaticVoidMethod(j.cls, id, (jint)disc);
}

std::string saved_disc(int disc) {
    Jni j;
    jmethodID id = j.m("openSavedDisc", "(I)I");
    const int fd = id ? j.env->CallStaticIntMethod(j.cls, id, (jint)disc) : -1;
    return fd >= 0 ? fd_path(fd) : std::string();
}

void save_disc(const std::string &, int) {}  // Java garde l'URI et son autorisation au moment du choix

int poll_pick(std::string &path) {
    Jni j;
    jmethodID id = j.m("pollPickedDisc", "()I");
    const int r = id ? j.env->CallStaticIntMethod(j.cls, id) : -2;
    if (r >= 0) { path = fd_path(r); return 1; }
    return r == -2 ? -1 : 0;  // -1 : en cours, -2 : annule
}

#elif defined(__APPLE__)
// iOS : host_ios.mm (14/10)
#else

bool https_get(const std::string &, int &, std::string &) { return false; }
bool https_request(const std::string &, const std::string &, const std::string &, const std::string &, int &,
                   std::string &) { return false; }
bool secret_save(const std::string &) { return false; }
std::string secret_load() { return {}; }
std::string data_dir() { return "./"; }
void begin_pick(int) { g_pick_state = -1; }
std::string saved_disc(int) { return {}; }
void save_disc(const std::string &, int) {}
void import_begin() {}
int import_poll(std::string &) { return -1; }
void export_begin(const std::string &, const std::string &) {}
int export_poll() { return -1; }
bool share(const std::string &, const std::vector<std::string> &) { return false; }
bool restart_app() { return false; }
void open_folder(const std::string &) {}

#endif

#if !defined(__ANDROID__) && !defined(__APPLE__)
std::string device_id() { return {}; }
bool auth_open(const std::string &, const std::string &, bool) { return false; }
bool auth_poll(std::string &) { return false; }

int poll_pick(std::string &path) {
    const int s = g_pick_state;
    g_pick_state = 0;
    if (s == 1) path = g_picked;
    return s;
}
#endif

}  // namespace nj::launcher::host
