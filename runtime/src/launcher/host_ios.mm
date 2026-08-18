// Hote iOS du lanceur et du menu (14/10) : ce que host.cpp fait avec WinHTTP / JNI sur les autres
// plateformes. Reseau (NSURLSession), connexion Discord dans une fenetre du systeme
// (ASWebAuthenticationSession, retour par discord-<id>:/authorize/callback comme sur Android), choix des
// disques et des sauvegardes (selecteur de fichiers d'iOS, copie dans Documents/), export, partage du
// rapport, identifiant de l'appareil, laissez-passer dans le trousseau.
// Les fichiers du jeu vivent dans Documents/ (visible dans l'app Fichiers : UIFileSharingEnabled).
#include "host.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <atomic>
#include <mutex>
#include <string>
#include <unistd.h>

namespace {

constexpr const char *kCallbackScheme = "discord-1559162514154201128";

std::string documents() {
    NSArray<NSString *> *d = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    return d.count ? std::string(d.firstObject.UTF8String) : std::string(".");
}

// Le moteur ecrit ses fichiers en chemins relatifs (config.toml, memcard_a.raw, journal/...) : le dossier
// courant devient Documents/ avant main().
__attribute__((constructor)) void enter_documents() { chdir(documents().c_str()); }

UIViewController *top_controller() {
    UIWindow *win = nil;
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:UIWindowScene.class]) continue;
        for (UIWindow *w in ((UIWindowScene *)scene).windows)
            if (w.isKeyWindow) win = w;
        if (!win) win = ((UIWindowScene *)scene).windows.firstObject;
    }
    UIViewController *vc = win.rootViewController;
    while (vc.presentedViewController) vc = vc.presentedViewController;
    return vc;
}

void on_main(void (^block)(void)) {
    if (NSThread.isMainThread) block();
    else dispatch_async(dispatch_get_main_queue(), block);
}

std::mutex g_m;
std::atomic<int> g_pick{0}, g_import{0}, g_export{0};
std::string g_picked, g_imported;
std::atomic<bool> g_auth_ready{false}, g_auth_running{false};
std::string g_auth_uri;

}  // namespace

// ---- selecteurs de fichiers ----------------------------------------------------------------------
@interface NJPicker : NSObject <UIDocumentPickerDelegate>
@property(nonatomic) int mode;   // 0 disque, 1 import, 2 export
@end

@implementation NJPicker
- (void)documentPicker:(UIDocumentPickerViewController *)picker didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    if (self.mode == 2) {
        g_export = 1;
        return;
    }
    NSURL *src = urls.firstObject;
    if (!src) return [self documentPickerWasCancelled:picker];
    // copie deja faite par iOS (asCopy) : rangee dans Documents/discs ou Documents/import
    NSString *sub = self.mode == 0 ? @"discs" : @"import";
    NSString *dir = [@(documents().c_str()) stringByAppendingPathComponent:sub];
    [NSFileManager.defaultManager createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    NSString *dst = [dir stringByAppendingPathComponent:src.lastPathComponent];
    [NSFileManager.defaultManager removeItemAtPath:dst error:nil];
    NSError *err = nil;
    if (![NSFileManager.defaultManager moveItemAtURL:src toURL:[NSURL fileURLWithPath:dst] error:&err])
        [NSFileManager.defaultManager copyItemAtURL:src toURL:[NSURL fileURLWithPath:dst] error:&err];
    const bool ok = err == nil;
    std::lock_guard<std::mutex> l(g_m);
    if (self.mode == 0) {
        g_picked = dst.UTF8String;
        g_pick = ok ? 1 : -1;
    } else {
        g_imported = dst.UTF8String;
        g_import = ok ? 1 : -1;
    }
}
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)picker {
    if (self.mode == 0) g_pick = -1;
    else if (self.mode == 1) g_import = -1;
    else g_export = -1;
}
@end

// ---- connexion Discord ---------------------------------------------------------------------------
@interface NJAuthAnchor : NSObject <ASWebAuthenticationPresentationContextProviding>
@end
@implementation NJAuthAnchor
- (ASPresentationAnchor)presentationAnchorForWebAuthenticationSession:(ASWebAuthenticationSession *)session {
    return top_controller().view.window;
}
@end

namespace {
NJPicker *g_picker_delegate = nil;
NJAuthAnchor *g_anchor = nil;
ASWebAuthenticationSession *g_session = nil;

void present_picker(int mode, NSArray<NSURL *> *export_urls) {
    on_main(^{
        UIDocumentPickerViewController *p =
            export_urls ? [[UIDocumentPickerViewController alloc] initForExportingURLs:export_urls asCopy:YES]
                        : [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[ UTTypeData ]
                                                                                       asCopy:YES];
        if (!g_picker_delegate) g_picker_delegate = [NJPicker new];
        g_picker_delegate.mode = mode;
        p.delegate = g_picker_delegate;
        p.modalPresentationStyle = UIModalPresentationFormSheet;
        UIViewController *top = top_controller();
        if (!top) return [g_picker_delegate documentPickerWasCancelled:p];
        [top presentViewController:p animated:YES completion:nil];
    });
}

bool http(const std::string &method, const std::string &url, const std::string &headers, const std::string &body,
          int &status, std::string &out) {
    NSURL *u = [NSURL URLWithString:@(url.c_str())];
    if (!u) return false;
    NSMutableURLRequest *rq = [NSMutableURLRequest requestWithURL:u
                                                      cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                                                  timeoutInterval:20];
    rq.HTTPMethod = @(method.c_str());
    // en-tetes « Nom: valeur\r\n... » (meme forme que sous Windows)
    size_t p = 0;
    while (p < headers.size()) {
        size_t e = headers.find("\r\n", p);
        if (e == std::string::npos) e = headers.size();
        const std::string line = headers.substr(p, e - p);
        const size_t c = line.find(':');
        if (c != std::string::npos) {
            size_t v = c + 1;
            while (v < line.size() && line[v] == ' ') ++v;
            [rq setValue:@(line.substr(v).c_str()) forHTTPHeaderField:@(line.substr(0, c).c_str())];
        }
        p = e + 2;
    }
    if (!body.empty()) rq.HTTPBody = [NSData dataWithBytes:body.data() length:body.size()];
    __block bool ok = false;
    __block int st = 0;
    __block std::string res;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    NSURLSessionDataTask *task = [NSURLSession.sharedSession
        dataTaskWithRequest:rq
          completionHandler:^(NSData *data, NSURLResponse *resp, NSError *err) {
            if (!err && [resp isKindOfClass:NSHTTPURLResponse.class]) {
                ok = true;
                st = (int)((NSHTTPURLResponse *)resp).statusCode;
                if (data) res.assign((const char *)data.bytes, data.length);
            }
            dispatch_semaphore_signal(sem);
          }];
    [task resume];
    if (dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 25 * NSEC_PER_SEC)) != 0) {
        [task cancel];
        return false;
    }
    status = st;
    out = res;
    return ok;
}

NSDictionary *keychain_query() {
    return @{
        (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService : @"com.ayano.nightjar",
        (__bridge id)kSecAttrAccount : @"pass",
    };
}
}  // namespace

namespace nj::launcher::host {

bool https_get(const std::string &url, int &status, std::string &body) {
    return http("GET", url, "User-Agent: Nightjar\r\n", "", status, body);
}

bool https_request(const std::string &method, const std::string &url, const std::string &headers,
                   const std::string &body, int &status, std::string &out) {
    return http(method, url, headers, body, status, out);
}

bool secret_save(const std::string &value) {
    SecItemDelete((__bridge CFDictionaryRef)keychain_query());
    if (value.empty()) return true;
    NSMutableDictionary *q = [keychain_query() mutableCopy];
    q[(__bridge id)kSecValueData] = [NSData dataWithBytes:value.data() length:value.size()];
    q[(__bridge id)kSecAttrAccessible] = (__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;
    return SecItemAdd((__bridge CFDictionaryRef)q, nullptr) == errSecSuccess;
}

std::string secret_load() {
    NSMutableDictionary *q = [keychain_query() mutableCopy];
    q[(__bridge id)kSecReturnData] = @YES;
    q[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
    CFTypeRef out = nullptr;
    if (SecItemCopyMatching((__bridge CFDictionaryRef)q, &out) != errSecSuccess || !out) return {};
    NSData *d = (__bridge_transfer NSData *)out;
    return std::string((const char *)d.bytes, d.length);
}

std::string device_id() {
    NSUUID *u = UIDevice.currentDevice.identifierForVendor;
    return u ? std::string(u.UUIDString.UTF8String) : std::string();
}

bool auth_open(const std::string &, const std::string &web_url, bool) {
    if (g_auth_running.exchange(true)) return true;   // une seule fenetre a la fois
    g_auth_ready = false;
    NSURL *u = [NSURL URLWithString:@(web_url.c_str())];
    if (!u) {
        g_auth_running = false;
        return false;
    }
    on_main(^{
        if (!g_anchor) g_anchor = [NJAuthAnchor new];
        g_session = [[ASWebAuthenticationSession alloc]
                  initWithURL:u
            callbackURLScheme:@(kCallbackScheme)
            completionHandler:^(NSURL *cb, NSError *err) {
              {
                  std::lock_guard<std::mutex> l(g_m);
                  // fenetre fermee par le joueur : meme reponse qu'un refus dans Discord
                  g_auth_uri = cb ? std::string(cb.absoluteString.UTF8String)
                                  : std::string(kCallbackScheme) + ":/authorize/callback?error=access_denied";
              }
              g_auth_ready = true;
              g_auth_running = false;
              g_session = nil;
            }];
        g_session.presentationContextProvider = g_anchor;
        g_session.prefersEphemeralWebBrowserSession = NO;   // compte Discord deja connecte dans Safari
        if (![g_session start]) {
            std::lock_guard<std::mutex> l(g_m);
            g_auth_uri = std::string(kCallbackScheme) + ":/authorize/callback?error=start_failed";
            g_auth_ready = true;
            g_auth_running = false;
        }
    });
    return true;
}

bool auth_poll(std::string &uri) {
    if (!g_auth_ready.exchange(false)) return false;
    std::lock_guard<std::mutex> l(g_m);
    uri = g_auth_uri;
    return true;
}

std::string data_dir() { return documents() + "/"; }

void begin_pick(int) {
    g_pick = 0;
    present_picker(0, nil);
}

int poll_pick(std::string &path) {
    const int s = g_pick.exchange(0);
    if (s == 1) {
        std::lock_guard<std::mutex> l(g_m);
        path = g_picked;
    }
    return s;
}

std::string saved_disc(int disc) {
    NSString *v = [NSUserDefaults.standardUserDefaults stringForKey:[NSString stringWithFormat:@"disc%d", disc]];
    return v ? std::string(v.UTF8String) : std::string();
}

void save_disc(const std::string &path, int disc) {
    [NSUserDefaults.standardUserDefaults setObject:@(path.c_str()) forKey:[NSString stringWithFormat:@"disc%d", disc]];
}

void import_begin() {
    g_import = 0;
    present_picker(1, nil);
}

int import_poll(std::string &path) {
    const int s = g_import.exchange(0);
    if (s == 1) {
        std::lock_guard<std::mutex> l(g_m);
        path = g_imported;
    }
    return s;
}

void export_begin(const std::string &src, const std::string &name) {
    g_export = 0;
    NSString *tmp = [NSTemporaryDirectory() stringByAppendingPathComponent:@(name.c_str())];
    [NSFileManager.defaultManager removeItemAtPath:tmp error:nil];
    if (![NSFileManager.defaultManager copyItemAtPath:@(src.c_str()) toPath:tmp error:nil]) {
        g_export = -1;
        return;
    }
    present_picker(2, @[ [NSURL fileURLWithPath:tmp] ]);
}

int export_poll() { return g_export.exchange(0); }

bool restart_app() { return false; }

bool share(const std::string &text, const std::vector<std::string> &files) {
    NSMutableArray *items = [NSMutableArray arrayWithObject:@(text.c_str())];
    for (const std::string &f : files) [items addObject:[NSURL fileURLWithPath:@(f.c_str())]];
    on_main(^{
        UIActivityViewController *a = [[UIActivityViewController alloc] initWithActivityItems:items
                                                                         applicationActivities:nil];
        UIViewController *top = top_controller();
        a.popoverPresentationController.sourceView = top.view;   // iPad
        [top presentViewController:a animated:YES completion:nil];
    });
    return true;
}

// Pas de « dossier » a ouvrir sur iOS : les fichiers sont dans l'app Fichiers > Nightjar.
void open_folder(const std::string &) {}

}  // namespace nj::launcher::host
