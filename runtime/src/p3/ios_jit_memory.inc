// Memoire executable de P3 sur iPhone (inclus par p3_jit.cpp, seulement pour iOS).
//
// iOS interdit PROT_EXEC a une application ordinaire (W^X strict). StikDebug, attache a notre processus avec son
// script JIT universel, sait pourtant rendre une region executable : le protocole (lu dans le code public de
// StikDebug, universal.js / JIT26PrepareRegion, et dans la documentation de nos essais iPS4) est une instruction
//
//     brk #0xf00d     avec x16 = 1, x0 = 0 (« region neuve »), x1 = taille en octets
//
// que le debogueur intercepte : il alloue la region, ecrit un octet par page de 16 Ko pour l'activer, et rend son
// adresse dans x0 (nul en cas d'echec). Cette adresse est EXECUTABLE SEULEMENT (une ecriture plante) : on en prend
// un second alias, inscriptible, des memes pages physiques par vm_remap. Le code se produit par l'alias en
// ecriture et s'execute par l'autre adresse. Sans debogueur attache, le brk tue le processus : on le verifie
// d'abord (P_TRACED) et on repond par un message clair.
//
// Ecrit pour Nightjar a partir de ce protocole ; aucune ligne de iPS4 / shadPS4 (GPL) n'y est reprise.
#if defined(__APPLE__) && defined(__aarch64__)
#include <TargetConditionals.h>
#endif
#if defined(__APPLE__) && TARGET_OS_IPHONE

#include <libkern/OSCacheControl.h>
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include <llvm/ExecutionEngine/JITLink/JITLinkMemoryManager.h>
#include <llvm/ExecutionEngine/Orc/Shared/AllocationActions.h>

namespace nj::runtime::p3 {
namespace {

// Un debogueur est-il attache a ce processus ? (kinfo_proc.p_flag & P_TRACED)
bool debugger_attached() {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, (int)getpid()};
    struct kinfo_proc info;
    std::memset(&info, 0, sizeof info);
    size_t sz = sizeof info;
    if (sysctl(mib, 4, &info, &sz, nullptr, 0) != 0) return false;
    return (info.kp_proc.p_flag & P_TRACED) != 0;
}

// Demande une region executable neuve de `len` octets a StikDebug (nullptr : refus). Les registres sont poses a la
// main (x0 = 0, x1 = taille, x16 = 1) : un « register ... asm("x0") » est refuse par le C++17.
void* stik_prepare_region(size_t len) {
    void* r = nullptr;
    __asm__ volatile(
        "mov x1, %1\n"
        "mov x0, xzr\n"
        "mov x16, #1\n"
        "brk #0xf00d\n"
        "mov %0, x0\n"
        : "=r"(r)
        : "r"(len)
        : "x0", "x1", "x16", "memory");
    return r;
}

// Une grande region executable (rx) et son alias en ecriture (rw), decoupee a la demande : peu d'allers-retours
// vers le debogueur (chacun active la region page par page).
struct Slab {
    uint8_t* rx = nullptr;
    uint8_t* rw = nullptr;
    size_t size = 0, used = 0;
};

class StikMemoryManager final : public llvm::jitlink::JITLinkMemoryManager {
public:
    using JITLinkMemoryManager::allocate;
    using JITLinkMemoryManager::deallocate;

    StikMemoryManager() : page_((size_t)vm_page_size) {}

    void allocate(const llvm::jitlink::JITLinkDylib*, llvm::jitlink::LinkGraph& G,
                  OnAllocatedFunction OnAllocated) override {
        using namespace llvm;
        jitlink::BasicLayout BL(G);
        size_t exec_bytes = 0, data_bytes = 0;
        for (auto& KV : BL.segments()) {
            auto& Seg = KV.second;
            if (Seg.Alignment.value() > page_) {
                OnAllocated(make_error<jitlink::JITLinkError>("StikDebug : alignement de segment superieur a une page"));
                return;
            }
            const size_t sz = alignTo(Seg.ContentSize + Seg.ZeroFillSize, page_);
            (is_exec(KV.first) ? exec_bytes : data_bytes) += sz;
        }
        uint8_t *exec_rx = nullptr, *exec_rw = nullptr;
        if (exec_bytes) {
            std::string err;
            if (!take_exec(exec_bytes, exec_rx, exec_rw, err)) {
                OnAllocated(make_error<jitlink::JITLinkError>(err));
                return;
            }
        }
        uint8_t* data = nullptr;
        if (data_bytes) {
            void* p = mmap(nullptr, data_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
            if (p == MAP_FAILED) {
                OnAllocated(make_error<jitlink::JITLinkError>("StikDebug : mmap des donnees refuse"));
                return;
            }
            data = (uint8_t*)p;
        }
        std::vector<Range> execs;
        size_t eo = 0, dof = 0;
        for (auto& KV : BL.segments()) {
            auto& Seg = KV.second;
            const size_t sz = alignTo(Seg.ContentSize + Seg.ZeroFillSize, page_);
            if (is_exec(KV.first)) {
                Seg.Addr = orc::ExecutorAddr::fromPtr(exec_rx + eo);
                Seg.WorkingMem = (char*)(exec_rw + eo);
                execs.push_back({exec_rx + eo, exec_rw + eo, sz});
                eo += sz;
            } else {
                Seg.Addr = orc::ExecutorAddr::fromPtr(data + dof);
                Seg.WorkingMem = (char*)(data + dof);
                dof += sz;
            }
        }
        if (auto Err = BL.apply()) {
            if (data) munmap(data, data_bytes);
            OnAllocated(std::move(Err));
            return;
        }
        OnAllocated(std::make_unique<InFlight>(std::move(BL), std::move(execs), data, data_bytes));
    }

    void deallocate(std::vector<FinalizedAlloc> Allocs, OnDeallocatedFunction OnDeallocated) override {
        llvm::Error Err = llvm::Error::success();
        for (auto& A : Allocs) {
            auto* fi = A.release().toPtr<Final*>();
            while (!fi->actions.empty()) {
                if (auto E = fi->actions.back().runWithSPSRetErrorMerged()) Err = llvm::joinErrors(std::move(Err), std::move(E));
                fi->actions.pop_back();
            }
            if (fi->data) munmap(fi->data, fi->data_bytes);
            delete fi;   // le code reste en place : une region obtenue du debogueur ne se rend pas (session courte)
        }
        OnDeallocated(std::move(Err));
    }

private:
    struct Range {
        uint8_t *rx, *rw;
        size_t size;
    };
    struct Final {
        uint8_t* data;
        size_t data_bytes;
        std::vector<llvm::orc::shared::WrapperFunctionCall> actions;
    };

    static bool is_exec(const llvm::orc::AllocGroup& AG) {
        return (static_cast<unsigned>(AG.getMemProt()) & static_cast<unsigned>(llvm::orc::MemProt::Exec)) != 0;
    }

    class InFlight final : public InFlightAlloc {
    public:
        InFlight(llvm::jitlink::BasicLayout BL, std::vector<Range> execs, uint8_t* data, size_t data_bytes)
            : BL_(std::move(BL)), execs_(std::move(execs)), data_(data), data_bytes_(data_bytes) {}

        void finalize(OnFinalizedFunction OnFinalized) override {
            // ce qui a ete ecrit par l'alias rw doit etre visible a l'execution par l'alias rx
            for (const Range& r : execs_) {
                sys_dcache_flush(r.rw, r.size);
                sys_icache_invalidate(r.rx, r.size);
            }
            auto Dealloc = llvm::orc::shared::runFinalizeActions(BL_.graphAllocActions());
            if (!Dealloc) {
                OnFinalized(Dealloc.takeError());
                return;
            }
            auto* fi = new Final{data_, data_bytes_, std::move(*Dealloc)};
            data_ = nullptr;
            OnFinalized(FinalizedAlloc(llvm::orc::ExecutorAddr::fromPtr(fi)));
        }
        void abandon(OnAbandonedFunction OnAbandoned) override {
            if (data_) munmap(data_, data_bytes_);
            data_ = nullptr;
            OnAbandoned(llvm::Error::success());
        }

    private:
        llvm::jitlink::BasicLayout BL_;
        std::vector<Range> execs_;
        uint8_t* data_;
        size_t data_bytes_;
    };

    // Morceau executable de `n` octets (multiple d'une page) pris dans la region courante, ou une neuve.
    bool take_exec(size_t n, uint8_t*& rx, uint8_t*& rw, std::string& err) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!slab_.rx || slab_.size - slab_.used < n) {
            const size_t want = std::max<size_t>(alignTo_(n), kSlab);
            if (!debugger_attached()) {
                err = "iOS : lance Nightjar avec StikDebug (script JIT) attache, puis relance le jeu";
                return false;
            }
            void* r = stik_prepare_region(want);
            if (!r) {
                err = "iOS : StikDebug a refuse la region executable (script JIT universel actif ?)";
                return false;
            }
            vm_address_t alias = 0;
            vm_prot_t cur = VM_PROT_NONE, max = VM_PROT_NONE;
            kern_return_t kr = vm_remap(mach_task_self(), &alias, (vm_size_t)want, 0, VM_FLAGS_ANYWHERE, mach_task_self(),
                                        (vm_address_t)r, FALSE, &cur, &max, VM_INHERIT_NONE);
            if (kr == KERN_SUCCESS)
                kr = vm_protect(mach_task_self(), alias, (vm_size_t)want, FALSE, VM_PROT_READ | VM_PROT_WRITE);
            if (kr != KERN_SUCCESS) {
                err = std::string("iOS : alias en ecriture refuse (vm_remap/vm_protect ") + std::to_string((int)kr) + ")";
                return false;
            }
            slab_ = {(uint8_t*)r, (uint8_t*)alias, want, 0};   // l'ancienne region reste en place, partiellement libre
        }
        rx = slab_.rx + slab_.used;
        rw = slab_.rw + slab_.used;
        slab_.used += n;
        return true;
    }
    size_t alignTo_(size_t n) const { return (n + page_ - 1) / page_ * page_; }

    static constexpr size_t kSlab = 48u << 20;   // 48 Mo par region (le code traduit du jeu en fait quelques dizaines)
    size_t page_;
    std::mutex mu_;
    Slab slab_;
};

}  // namespace
}  // namespace nj::runtime::p3

#endif  // iOS
