// P3 (docs/P3_TRADUCTEUR.md) : compiler en memoire le C++ du traducteur, charger le code produit
// et brancher ses fonctions dans la table des cases. Build NJ_P3_JIT : LLVM + Clang lies au
// moteur, symboles du moteur donnes par la table p3_table.cpp, seule chose que le code charge voit.
//
// Deux voies :
//  - jit_compile : C++ -> IR en memoire -> JIT (essai du jalon 1) ;
//  - jit_compile_object + jit_load_object : C++ -> fichier objet (Clang fait toute la chaine), puis
//    chargement dans le JIT ; le fichier objet sert de cache (p3_host.cpp).
//
// Essai du jalon 1 : NJ_P3_JIT=<output_N.cpp> [NJ_P3_JIT_ARGS="-I... -D..."] compile un fichier
// genere pendant la partie et le branche (build avec main_output.cpp).
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Driver/CreateInvocationFromArgs.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/FrontendActions.h>
#include <llvm/ExecutionEngine/JITLink/JITLink.h>
#include <llvm/ExecutionEngine/Orc/AbsoluteSymbols.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/SelfExecutorProcessControl.h>
#include <llvm/ExecutionEngine/Orc/TaskDispatch.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/TargetParser/Host.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "runtime/cpu_context.h"
#include "runtime/env.h"
#include "p3_jit.h"
#include "p3_table.h"
#include "runtime/crash_report.h"
#include <llvm/Support/ErrorHandling.h>
#include "runtime/obf.h"
#include "runtime/diag.h"
using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire

#include "ios_jit_memory.inc"   // iPhone : memoire executable fournie par StikDebug (vide ailleurs)

namespace nj::runtime::p3 {
namespace {

using clk = std::chrono::steady_clock;

double ms_since(clk::time_point t) {
    return std::chrono::duration<double, std::milli>(clk::now() - t).count();
}

// _ZN1g<n><nom>ERN2nj7runtime8GuestCpuE -> <nom>
std::string game_name(llvm::StringRef mangled) {
    if (!mangled.starts_with("_ZN1g"_g)) return {};
    size_t i = 5, n = 0;
    while (i < mangled.size() && mangled[i] >= '0' && mangled[i] <= '9') n = n * 10 + (mangled[i++] - '0');
    if (!n || i + n > mangled.size()) return {};
    return mangled.substr(i, n).str();
}

// JIT partage : un seul espace de code pour toute la partie (les definitions faibles des en-tetes,
// communes a tous les lots, n'y sont gardees qu'une fois).
std::mutex g_jit_mu;
std::unique_ptr<llvm::orc::LLJIT> g_jit;

// Symboles du moteur fournis a la demande : le nom cherche par le code charge est reconnu par son
// empreinte dans la table (p3_table.h), qui ne contient pas les noms.
class TableGenerator : public llvm::orc::DefinitionGenerator {
public:
    llvm::Error tryToGenerate(llvm::orc::LookupState&, llvm::orc::LookupKind, llvm::orc::JITDylib& jd,
                              llvm::orc::JITDylibLookupFlags, const llvm::orc::SymbolLookupSet& wanted) override {
        llvm::orc::SymbolMap found;
        for (const auto& [name, flags] : wanted) {
            const std::string n = (*name).str();
            const uint64_t h = name_hash(n.c_str());
            for (unsigned i = 0; i < kTableSize; ++i) {
                if (kTable[i].hash != h) continue;
                // la table des cases est celle des tables enregistrees (generee ou allouee par le moteur)
                void* addr = h == kSlotTableHash ? (void*)dispatch_tables().slots : kTable[i].addr;
                found[name] = {llvm::orc::ExecutorAddr::fromPtr(addr), llvm::JITSymbolFlags::Exported};
                break;
            }
        }
        if (found.empty()) return llvm::Error::success();
        // deux recherches simultanees du meme symbole : la seconde trouve la definition de la premiere
        if (auto e = jd.define(llvm::orc::absoluteSymbols(std::move(found))))
            llvm::consumeError(std::move(e));
        return llvm::Error::success();
    }
};

llvm::orc::LLJIT* shared_jit(std::string& err) {
    std::lock_guard<std::mutex> lk(g_jit_mu);
    if (g_jit) return g_jit.get();
    // erreur fatale de LLVM (le processus s'arrete ensuite) : sa raison va au rapport de plantage
    static const bool fatal_hooked = [] {
        llvm::install_fatal_error_handler([](void*, const char* reason, bool) { nj::crash::note("LLVM"_g, reason); });
        return true;
    }();
    (void)fatal_hooked;
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    // plusieurs fils compilent et cherchent des symboles en meme temps : un TargetMachine par
    // compilation (ConcurrentIRCompiler), sinon le tas se corrompt (06/10, 3 fils)
    // Edition de liens sur le fil qui la demande (InPlaceTaskDispatcher), pas sur le groupe de fils
    // d'ORC : le chargement du cache se figeait au hasard (16, 183, 291 tranches sur 326, 12/10),
    // nos fils bloques dans lookup en attendant ce groupe, notre generateur de symboles definissant
    // pendant la recherche. Les liaisons passent deja une a une (link_mu) : rien de parallele n'est perdu.
    auto epc = llvm::orc::SelfExecutorProcessControl::Create(nullptr,
                                                             std::make_unique<llvm::orc::InPlaceTaskDispatcher>());
    if (!epc) {
        err = "LLJIT : "_g + llvm::toString(epc.takeError());
        return nullptr;
    }
    llvm::orc::LLJITBuilder jit_builder;
    jit_builder.setExecutorProcessControl(std::move(*epc)).setSupportConcurrentCompilation(true);
#if defined(__APPLE__) && TARGET_OS_IPHONE
    // iPhone : le code s'ecrit par un alias et s'execute par l'autre (regions de StikDebug), ios_jit_memory.inc.
    if (!debugger_attached()) {
        err = "iOS : lance Nightjar avec StikDebug (script JIT) attache, puis relance le jeu"_g;
        return nullptr;
    }
    jit_builder.setMemoryManagerCreator(
        [](llvm::orc::ExecutionSession&) -> llvm::Expected<std::unique_ptr<llvm::jitlink::JITLinkMemoryManager>> {
            return std::make_unique<StikMemoryManager>();
        });
#endif
    auto jit = jit_builder.create();
    if (!jit) {
        err = "LLJIT : "_g + llvm::toString(jit.takeError());
        return nullptr;
    }
    auto& J = **jit;
    J.getMainJITDylib().addGenerator(std::make_unique<TableGenerator>());
    g_jit = std::move(*jit);
    return g_jit.get();
}

llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> make_vfs(const VirtualFiles& files) {
    auto overlay = llvm::makeIntrusiveRefCnt<llvm::vfs::OverlayFileSystem>(llvm::vfs::getRealFileSystem());
    auto mem = llvm::makeIntrusiveRefCnt<llvm::vfs::InMemoryFileSystem>();
#ifndef _WIN32
    // Les fichiers virtuels s'appellent « C:/nj_p3_virtuel/... » (chemin inscrit dans les en-tetes
    // precompiles fabriques sous Windows). Hors Windows ce chemin est relatif : Clang le rend
    // absolu avec le dossier courant reel avant de chercher ; la couche en memoire doit partir du
    // meme dossier, sinon rien n'est trouve (Android, MuMu 07/10 et 12/10 : « unable to read PCH
    // file .../files/C:/nj_p3_virtuel/prelude-O2.pch »).
    if (auto cwd = llvm::vfs::getRealFileSystem()->getCurrentWorkingDirectory())
        mem->setCurrentWorkingDirectory(*cwd);
#endif
    for (const VirtualFile& f : files)
        mem->addFile(f.path, 0,
                     f.ref ? llvm::MemoryBuffer::getMemBuffer(*f.ref, f.path, /*RequiresNullTerminator=*/false)
                           : llvm::MemoryBuffer::getMemBufferCopy(f.text, f.path));
    // Pas de setCurrentWorkingDirectory sur l'overlay : il le transmet au systeme de fichiers reel,
    // qui change alors le dossier courant de tout le processus (cache P3 ecrit dans /nj_p3_cache,
    // refuse ; textures_hd perdues). La couche en memoire suffit (elle resout ses chemins seule).
    overlay->pushOverlay(mem);
    return overlay;
}

bool fma_flags_impl(const std::vector<std::string>& extra) {
    std::string tgt = llvm::sys::getProcessTriple();
    for (const auto& a : extra)
        if (a.compare(0, 9, "--target="_g) == 0) tgt = a.substr(9);
    if (tgt.compare(0, 6, "x86_64"_g) != 0) return false;
    if (const char* o = std::getenv("NJ_P3_NOFMA"_g)) { (void)o; return false; }   // retour a l'ancien code (comparaison)
    static const bool host = [] {
        const auto f = llvm::sys::getHostCPUFeatures();
        const auto it = f.find("fma"_g);
        return it != f.end() && it->second;
    }();
    return host;
}

thread_local std::string t_diag;   // messages de la derniere compilation de ce fil
thread_local llvm::raw_string_ostream t_diag_os(t_diag);
std::string diag_head() {
    t_diag_os.flush();
    const size_t e = t_diag.find("error: "_g);   // sans le chemin du fichier virtuel
    return t_diag.substr(e == std::string::npos ? 0 : e, 400);
}

// Instance de Clang pour `input` ; `mode` = options propres a l'action (-x c++-header, -c...).
std::unique_ptr<clang::CompilerInstance> make_ci(const VirtualFiles& files, const std::string& input,
                                                 const std::vector<std::string>& mode,
                                                 const std::vector<std::string>& extra, const std::string& opt,
                                                 const std::string& out_path, std::string& err) {
    std::vector<std::string> args = {"clang++"_g, "--target="_g + llvm::sys::getProcessTriple(), "-std=gnu++20"_g, opt,
                                     "-fno-rtti"_g, "-DNDEBUG"_g};
    args.insert(args.end(), mode.begin(), mode.end());
    args.insert(args.end(), extra.begin(), extra.end());
    if (fma_flags_impl(extra)) {   // std::fma -> vfmadd (meme resultat arrondi une fois) ; -ffp-contract=off : rien d'autre n'est fusionne
        args.push_back("-mfma"_g);
        args.push_back("-ffp-contract=off"_g);
    }
    args.push_back(input);
    std::vector<const char*> cargs;
    for (auto& a : args) cargs.push_back(a.c_str());
    // CodeGenOpts.Argv0 garde un POINTEUR vers cargs[0] (CompilerInvocation.cpp : Res.getCodeGenOpts().Argv0
    // = Argv0) et Clang le relit a la generation du code (initTargetOptions : strlen). `args` meurt au
    // retour de make_ci : le pointeur pendait, d'ou des plantages rares de l'installation (memoire
    // liberee puis reutilisee ; 16/10 : violation d'acces dans initTargetOptions). Chaine statique.
    cargs[0] = "clang++"_g;
    auto vfs = make_vfs(files);
    clang::DiagnosticOptions dopts;
    auto diags = clang::CompilerInstance::createDiagnostics(*vfs, dopts);
    clang::CreateInvocationOptions copts;
    copts.Diags = diags;
    copts.VFS = vfs;
    std::shared_ptr<clang::CompilerInvocation> inv = clang::createInvocation(cargs, copts);
    if (!inv) {
        err = "ligne de commande refusee"_g;
        return nullptr;
    }
    if (!out_path.empty()) inv->getFrontendOpts().OutputFile = out_path;   // le pilote ne retient pas toujours -o
    // Le pilote ajoute -disable-free (clang.exe quitte sans liberer, pour aller vite) : dans notre
    // processus, chaque tranche laissait son AST et son module derriere elle (16/10, tour : +2,7 Go
    // pendant une preparation de 365 tranches, pour 116 Mo de code produit).
    inv->getFrontendOpts().DisableFree = false;
    inv->getCodeGenOpts().DisableFree = false;
    auto ci = std::make_unique<clang::CompilerInstance>(inv);
    ci->createVirtualFileSystem(vfs);
    // 21/10 : messages de Clang gardes pour le journal (la sortie d'erreur est coupee en diffusion : 381 lots en echec
    // sans un mot sur MuMu). Une chaine par fil, videe a chaque tranche.
    t_diag.clear();
    ci->createDiagnostics(new clang::TextDiagnosticPrinter(t_diag_os, inv->getDiagnosticOpts()), true);
    return ci;
}

// Charge un module objet ou IR deja ajoute : cherche ses fonctions du jeu.
bool lookup_all(llvm::orc::LLJIT* J, llvm::orc::JITDylib& JD, const std::vector<std::string>& defined,
                std::vector<Compiled>& out, std::string& err) {
    for (const std::string& m : defined) {
        auto sym = J->lookup(JD, m);
        if (!sym) {
            err = m + " : "_g + llvm::toString(sym.takeError());
            return false;
        }
        out.push_back({game_name(m), sym->toPtr<GameFn>()});
    }
    return true;
}

}  // namespace

bool x86_fma_flags(const std::vector<std::string>& extra) { return fma_flags_impl(extra); }

// Niveau d'optimisation (NJ_P3_OPT, -O2 par defaut).
const char* opt_level() {
    const char* o = NJ_ENV("NJ_P3_OPT"_g);
    return o ? o : "-O2"_g;
}

bool jit_make_pch(const VirtualFiles& files, const std::string& header, const std::vector<std::string>& extra,
                  const std::string& opt, const std::string& out_path, std::string& err) {
    if (!shared_jit(err)) return false;   // initialise aussi les cibles de LLVM
    // Contenu des en-tetes copie dans le PCH : sinon Clang retourne lire les fichiers d'origine
    // (C:/re4n/nj-p3/runtime/include/...) quand il en a besoin, -fno-validate-pch ou non ; absents
    // chez le joueur (Android, MuMu 12/10 : « could not find file ... referenced by AST file »).
    auto ci = make_ci(files, header, {"-x"_g, "c++-header"_g, "-Xclang"_g, "-fmodules-embed-all-files"_g}, extra, opt,
                      out_path, err);
    if (!ci) return false;
    clang::GeneratePCHAction act;
    if (!ci->ExecuteAction(act)) {
        err = "en-tete precompile : echec"_g;
        return false;
    }
    return true;
}

bool jit_compile_object(const VirtualFiles& files, const std::string& main, const std::vector<std::string>& extra,
                        const std::string& opt, const std::string& obj_path, std::string& err) {
    if (!shared_jit(err)) return false;   // initialise aussi les cibles de LLVM
    auto ci = make_ci(files, main, {"-c"_g}, extra, opt, obj_path, err);
    if (!ci) return false;
    clang::EmitObjAction act;
    if (!ci->ExecuteAction(act)) {
        err = "echec de la compilation : "_g + diag_head() + " ("_g + main + ")"_g;
        return false;
    }
    return true;
}

bool jit_load_object(const std::string& obj_path, std::vector<Compiled>& out, std::string& err) {
    llvm::orc::LLJIT* J = shared_jit(err);
    if (!J) return false;
    // lu en memoire (pas de projection) : les objets temporaires sont effaces apres chargement
    auto buf = llvm::MemoryBuffer::getFile(obj_path, /*IsText=*/false, /*RequiresNullTerminator=*/false,
                                           /*IsVolatile=*/true);
    if (!buf) {
        err = "objet illisible : "_g + obj_path;
        return false;
    }
    std::vector<std::string> defined;
    {
        auto obj = llvm::object::ObjectFile::createObjectFile((*buf)->getMemBufferRef());
        if (!obj) {
            err = "objet invalide : "_g + llvm::toString(obj.takeError());
            return false;
        }
        for (const llvm::object::SymbolRef& s : (*obj)->symbols()) {
            auto flags = s.getFlags();
            if (!flags || (*flags & llvm::object::SymbolRef::SF_Undefined) || !(*flags & llvm::object::SymbolRef::SF_Global))
                continue;
            auto name = s.getName();
            if (!name) continue;
            if (!game_name(*name).empty()) defined.push_back(name->str());
        }
    }
    // un espace de symboles par objet : une fonction peut etre chargee en -O0 puis en -O2 (la case
    // pointe vers la derniere) ; il voit la table du moteur (espace principal).
    // Edition de liens a un fil a la fois : des recherches simultanees de plusieurs fils se
    // bloquaient dans ExecutionSession::lookup (5 chargements du cache sur 6, 06/10). La
    // compilation (Clang), elle, reste parallele.
    static std::mutex link_mu;
    std::lock_guard<std::mutex> link_lk(link_mu);
    static std::atomic<unsigned> no{0};
    auto jd = J->createJITDylib("o"_g + std::to_string(no++));
    if (!jd) {
        err = "espace : "_g + llvm::toString(jd.takeError());
        return false;
    }
    jd->addToLinkOrder(J->getMainJITDylib());
    if (auto e = J->addObjectFile(*jd, std::move(*buf))) {
        err = "objet : "_g + llvm::toString(std::move(e));
        return false;
    }
    return lookup_all(J, *jd, defined, out, err);
}

bool jit_compile(const VirtualFiles& files, const std::string& main, const std::vector<std::string>& extra,
                 std::vector<Compiled>& out, std::string& err, double* ms_front) {
    const auto t0 = clk::now();
    llvm::orc::LLJIT* J = shared_jit(err);
    if (!J) return false;
    auto ci = make_ci(files, main, {"-c"_g, "-o"_g, "nul"_g}, extra, opt_level(), ""_g, err);
    if (!ci) return false;
    auto ctx = std::make_unique<llvm::LLVMContext>();
    clang::EmitLLVMOnlyAction act(ctx.get());
    if (!ci->ExecuteAction(act)) {
        err = "echec de la compilation : "_g + diag_head() + " ("_g + main + ")"_g;
        return false;
    }
    std::unique_ptr<llvm::Module> mod = act.takeModule();
    if (ms_front) *ms_front = ms_since(t0);

    std::vector<std::string> defined;
    for (llvm::Function& f : *mod)
        if (!f.isDeclaration() && f.hasExternalLinkage() && !game_name(f.getName()).empty())
            defined.push_back(f.getName().str());

    if (auto e = J->addIRModule(llvm::orc::ThreadSafeModule(std::move(mod), std::move(ctx)))) {
        err = "module : "_g + llvm::toString(std::move(e));
        return false;
    }
    return lookup_all(J, J->getMainJITDylib(), defined, out, err);
}

unsigned patch_slots(const std::vector<Compiled>& fns) {
    static std::mutex mu;
    static std::unordered_map<std::string, unsigned> index;
    std::lock_guard<std::mutex> lk(mu);
    const DispatchTables& t = dispatch_tables();
    if (index.empty() && t.slot_names)
        for (unsigned i = 0; i < t.slot_count; ++i) index[t.slot_names[i]] = i;
    unsigned patched = 0;
    for (const Compiled& c : fns) {
        auto it = index.find(c.name);
        if (it == index.end()) continue;   // fonction interne au lot (enveloppee, __orig...)
        __atomic_store_n(&t.slots[it->second], c.fn, __ATOMIC_RELEASE);
        __atomic_store_n(&t.native[it->second], true, __ATOMIC_RELEASE);
        ++patched;
        // NJ_P3_MAP=<fichier> (developpement, mort en diffusion) : adresse hote -> nom (adresse du jeu),
        // pour nommer les echantillons de NJ_THREADPROF dans le code du jeu.
        static std::FILE* map = [] {
            const char* f = NJ_ENV("NJ_P3_MAP"_g);
            return f ? std::fopen(f, "a") : nullptr;
        }();
        if (map) std::fprintf(map, "%llx %s\n", (unsigned long long)(uintptr_t)c.fn, c.name.c_str());
    }
    return patched;
}

}  // namespace nj::runtime::p3

// Essai du jalon 1 (build avec main_output.cpp) : un fichier genere compile pendant la partie.
extern "C" void nj_p3_jit_start() {
    const char* file = NJ_ENV("NJ_P3_JIT"_g);
    if (!file) return;
    const char* extra = NJ_ENV("NJ_P3_JIT_ARGS"_g);
    std::thread([f = std::string(file), x = std::string(extra ? extra : ""_g)] {
        using namespace nj::runtime::p3;
        const auto t0 = clk::now();
        std::vector<std::string> args;
        std::istringstream ex(x);
        for (std::string a; ex >> a;) args.push_back(a);
        std::vector<Compiled> fns;
        std::string err;
        double front = 0;
        if (!jit_compile({}, f, args, fns, err, &front)) {
            NJ_DIAG("[P3 JIT] %s\n", err.c_str());
            return;
        }
        const unsigned n = patch_slots(fns);
        NJ_DIAG("[P3 JIT] %s : %u fonctions branchees (%zu definies), C++ -> IR %.0f ms, total %.0f ms\n",
                     f.c_str(), n, fns.size(), front, ms_since(t0));
    }).detach();
}
