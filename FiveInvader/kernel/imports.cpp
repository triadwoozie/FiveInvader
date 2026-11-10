#include <cstdint>
#include <cstdio>
#include <cctype>
#include <algorithm>
#include <unordered_set>
#include <thread>
#include <atomic>
#include <array>
#include <deque>
#include <limits>
#include <sstream>
#include <stdafx.h>
#include <cpu/ppc_context.h>
#ifdef PPC_CONFIG_SKIP_LR
#define GET_CALLER_LR() 0u
#define GET_CTX_LR(c) 0u
#else
#define GET_CALLER_LR() (g_ppcContext ? static_cast<uint32_t>(g_ppcContext->lr) : 0u)
#define GET_CTX_LR(c) ((c).lr)
#endif
#include <cpu/guest_thread.h>
#include <apu/audio.h>
#include <gpu/video.h>
#include <ui/game_window.h>
#include "function.h"
#include "xex.h"
#include "xbox.h"
#include "heap.h"
#include "memory.h"
#include <memory>
#if defined(__linux__)
#include <openssl/evp.h>
#endif
#include "xam.h"
#include "xdm.h"
#include <hid/hid.h>
#include <user/config.h>
#include <user/paths.h>
#include <os/logger.h>
#include <user/achievement_manager.h>
#include <app.h>

#include "io/file_system.h"
#include "vfs.h"
#include "io/gta_file_system.h"
// #include "sync_table.h"  // REMOVED - using Sonic direct approach

#include "game_init.h"
#include "io/net_socket.h"
#include "io/net_session.h"

static std::array<uint32_t, 4> CaptureGuestStackTrace()
{
    std::array<uint32_t, 4> trace{};
    PPCContext* context = GetPPCContext();
    if (!context || !g_memory.base)
        return trace;

    uint32_t stackPointer = context->r1.u32;
    for (uint32_t& returnAddress : trace)
    {
        if (stackPointer == 0 || stackPointer >= PPC_MEMORY_SIZE - sizeof(uint32_t))
            break;

        const uint32_t callerStackPointer =
            reinterpret_cast<be<uint32_t>*>(g_memory.Translate(stackPointer))->get();
        if (callerStackPointer <= stackPointer || callerStackPointer < sizeof(uint32_t) * 2)
            break;

        returnAddress =
            reinterpret_cast<be<uint32_t>*>(g_memory.Translate(callerStackPointer - sizeof(uint32_t) * 2))->get();
        stackPointer = callerStackPointer;
    }
    return trace;
}

// =============================================================================
// KERNEL PHASE SYSTEM - Fail-open waits during Boot/Init (Xenia-style)
// =============================================================================
enum class KernelPhase {
    Boot,      // Before first guest thread
    Init,      // Module loaded, subsystems initializing
    Runtime    // First frame presented or main loop entered
};

std::atomic<KernelPhase> g_kernelPhase{KernelPhase::Boot};

// Helper to check if fail-open waits are allowed
inline bool ShouldFailOpenWait() {
    return g_kernelPhase.load(std::memory_order_acquire) != KernelPhase::Runtime;
}

// Transition to Init phase (call after LdrLoadModule)
inline void KernelPhase_EnterInit() {
    auto expected = KernelPhase::Boot;
    if (g_kernelPhase.compare_exchange_strong(expected, KernelPhase::Init)) {
        LOG_WARNING("[KERNEL_PHASE] Boot -> Init");
    }
}

// Transition to Runtime phase (call after first VdSwap or main loop entry)
void KernelPhase_EnterRuntime() {
    auto phase = g_kernelPhase.load();
    if (phase != KernelPhase::Runtime) {
        g_kernelPhase.store(KernelPhase::Runtime, std::memory_order_release);
        LOGF_WARNING("[KERNEL_PHASE] {} -> Runtime", phase == KernelPhase::Boot ? "Boot" : "Init");
    }
}

#include <cerrno>
#include <fstream>
#include <algorithm>
#include <vector>
#include <mutex>

#if defined(__APPLE__)
#include <CommonCrypto/CommonCryptor.h>
#endif

#ifndef _WIN32
#include <sys/mman.h>  // For mprotect
#endif

#ifdef _WIN32
#include <ntstatus.h>
#endif

// =============================================================================
// RUNTIME FUNCTION PATCHING - Override weak symbols on macOS
// =============================================================================
// On macOS, weak symbol override doesn't work with static libraries.
// This patches PPCFuncMappings before Memory::Memory() runs to ensure
// our sync primitive hooks are used instead of the weak symbols.
// =============================================================================





void sub_831938B8(PPCContext& ctx, uint8_t* base);
void sub_83193930(PPCContext& ctx, uint8_t* base);
void sub_831939E0(PPCContext& ctx, uint8_t* base);
void sub_8250D988(PPCContext& ctx, uint8_t* base);
void sub_82220140(PPCContext& ctx, uint8_t* base);
void sub_831C1B10(PPCContext& ctx, uint8_t* base);
void sub_8257A2A8(PPCContext& ctx, uint8_t* base);
void sub_825C9768(PPCContext& ctx, uint8_t* base);
void sub_836B2AB0(PPCContext& ctx, uint8_t* base);
void sub_82332868(PPCContext& ctx, uint8_t* base);
void sub_83217D38(PPCContext& ctx, uint8_t* base);
void sub_8226B3B8(PPCContext& ctx, uint8_t* base);
void sub_83480F88(PPCContext& ctx, uint8_t* base);
void sub_8266BB00(PPCContext& ctx, uint8_t* base);
void sub_8229AD78(PPCContext& ctx, uint8_t* base);
void sub_829855F8(PPCContext& ctx, uint8_t* base);
void sub_82985550(PPCContext& ctx, uint8_t* base);
void sub_831E2DB8(PPCContext& ctx, uint8_t* base);
void sub_831E3240(PPCContext& ctx, uint8_t* base);
void sub_829856D0(PPCContext& ctx, uint8_t* base);
void sub_82924148(PPCContext& ctx, uint8_t* base);
void sub_82D95030(PPCContext& ctx, uint8_t* base);
void sub_83521D38(PPCContext& ctx, uint8_t* base);
void sub_836B1768(PPCContext& ctx, uint8_t* base);
void sub_83536A60(PPCContext& ctx, uint8_t* base);
void sub_823CFAF0(PPCContext& ctx, uint8_t* base);
void sub_83622DF0(PPCContext& ctx, uint8_t* base);
void sub_82EF3FA0(PPCContext& ctx, uint8_t* base);
void sub_831B9330(PPCContext& ctx, uint8_t* base);
void sub_831D0C30(PPCContext& ctx, uint8_t* base);
void sub_827E1D70(PPCContext& ctx, uint8_t* base);
void sub_8248B538(PPCContext& ctx, uint8_t* base);
void sub_82988A60(PPCContext& ctx, uint8_t* base);
void sub_82578948(PPCContext& ctx, uint8_t* base);
void sub_8294C6C0(PPCContext& ctx, uint8_t* base);
void sub_82948308(PPCContext& ctx, uint8_t* base);
void sub_822438F8(PPCContext& ctx, uint8_t* base);
void sub_824BBB30(PPCContext& ctx, uint8_t* base);
void sub_822DA770(PPCContext& ctx, uint8_t* base);
void sub_82985760(PPCContext& ctx, uint8_t* base);
void sub_82574450(PPCContext& ctx, uint8_t* base);
void sub_8344F920(PPCContext& ctx, uint8_t* base);
void sub_827D1138(PPCContext& ctx, uint8_t* base);
void sub_831EDB68(PPCContext& ctx, uint8_t* base);
void sub_831D7BD8(PPCContext& ctx, uint8_t* base);

namespace {

// Patch a single entry in PPCFuncMappings by guest address
bool PatchFuncMapping(size_t guestAddr, PPCFunc* newFunc) {
    for (size_t i = 0; PPCFuncMappings[i].guest != 0; i++) {
        if (PPCFuncMappings[i].guest == guestAddr) {
            PPCFunc* oldFunc = PPCFuncMappings[i].host;
            PPCFuncMappings[i].host = newFunc;
            if (g_ppcFunctionTable) {
                g_memory.InsertFunction(static_cast<uint32_t>(guestAddr), newFunc);
            }
            printf("[PATCH] 0x%08zX: %p -> %p\n", guestAddr, (void*)oldFunc, (void*)newFunc);
            fflush(stdout);
            return true;
        }
    }
    printf("[PATCH] WARNING: Guest address 0x%08zX not found in PPCFuncMappings\n", guestAddr);
    fflush(stdout);
    return false;
}

// Patch all sync primitive functions to use our hooks
void PatchSyncPrimitives() {
    printf("[PATCH] Patching sync primitives in PPCFuncMappings...\n");
    fflush(stdout);
    
    // Patch sync primitives - these cause blocking in HUD init
    PatchFuncMapping(0x831938B8, &::sub_831938B8);
    PatchFuncMapping(0x83193930, &::sub_83193930);
    PatchFuncMapping(0x831939E0, &::sub_831939E0);
    PatchFuncMapping(0x8250D988, &::sub_8250D988);
    PatchFuncMapping(0x82220140, &::sub_82220140);
    PatchFuncMapping(0x831C1B10, &::sub_831C1B10);
    PatchFuncMapping(0x8257A2A8, &::sub_8257A2A8);
    PatchFuncMapping(0x825C9768, &::sub_825C9768);
    PatchFuncMapping(0x836B2AB0, &::sub_836B2AB0);
    PatchFuncMapping(0x836B4378, &::sub_836B4378);
    PatchFuncMapping(0x82332868, &::sub_82332868);
    PatchFuncMapping(0x83217D38, &::sub_83217D38);
    PatchFuncMapping(0x8226B3B8, &::sub_8226B3B8);
    PatchFuncMapping(0x83480F88, &::sub_83480F88);
    PatchFuncMapping(0x8266BB00, &::sub_8266BB00);
    PatchFuncMapping(0x8229AD78, &::sub_8229AD78);
    PatchFuncMapping(0x829855F8, &::sub_829855F8);
    PatchFuncMapping(0x82985550, &::sub_82985550);
    PatchFuncMapping(0x831E2DB8, &::sub_831E2DB8);
    PatchFuncMapping(0x831E3240, &::sub_831E3240);
    PatchFuncMapping(0x829856D0, &::sub_829856D0);
    PatchFuncMapping(0x82924148, &::sub_82924148);
    PatchFuncMapping(0x82D95030, &::sub_82D95030);
    PatchFuncMapping(0x83521D38, &::sub_83521D38);
    PatchFuncMapping(0x836B1768, &::sub_836B1768);
    PatchFuncMapping(0x83536A60, &::sub_83536A60);
    PatchFuncMapping(0x8363FCB0, &::sub_8363FCB0);
    PatchFuncMapping(0x823CFAF0, &::sub_823CFAF0);
    PatchFuncMapping(0x83622DF0, &::sub_83622DF0);
    PatchFuncMapping(0x82EF3FA0, &::sub_82EF3FA0);
    PatchFuncMapping(0x831B9330, &::sub_831B9330);
    PatchFuncMapping(0x831D0C30, &::sub_831D0C30);
    PatchFuncMapping(0x827E1D70, &::sub_827E1D70);
    PatchFuncMapping(0x82577590, &::sub_82577590);
    PatchFuncMapping(0x8299AB58, &::sub_8299AB58);
    PatchFuncMapping(0x8345BC98, &::sub_8345BC98);
    PatchFuncMapping(0x829E3CA8, &::sub_829E3CA8);
    PatchFuncMapping(0x8298B940, &::sub_8298B940);
    PatchFuncMapping(0x829C4C00, &::sub_829C4C00);
    PatchFuncMapping(0x824F4850, &::sub_824F4850);
    PatchFuncMapping(0x82574338, &::sub_82574338);
    PatchFuncMapping(0x824B9C20, &::sub_824B9C20);
    PatchFuncMapping(0x824759C8, &::sub_824759C8);
    PatchFuncMapping(0x8248B538, &::sub_8248B538);
    PatchFuncMapping(0x82988A60, &::sub_82988A60);
    PatchFuncMapping(0x82578948, &::sub_82578948);
    PatchFuncMapping(0x8294C6C0, &::sub_8294C6C0);
    PatchFuncMapping(0x82948308, &::sub_82948308);
    PatchFuncMapping(0x822438F8, &::sub_822438F8);
    PatchFuncMapping(0x824BBB30, &::sub_824BBB30);
    PatchFuncMapping(0x822DA770, &::sub_822DA770);
    PatchFuncMapping(0x824F1388, &::sub_824F1388);
    PatchFuncMapping(0x824F1418, &::sub_824F1418);
    PatchFuncMapping(0x822D9768, &::sub_822D9768);
    PatchFuncMapping(0x822D9690, &::sub_822D9690);
    PatchFuncMapping(0x834DFFA0, &::sub_834DFFA0);
    PatchFuncMapping(0x82985760, &::sub_82985760);
    PatchFuncMapping(0x82574450, &::sub_82574450);
    PatchFuncMapping(0x8344F920, &::sub_8344F920);
    PatchFuncMapping(0x827D1138, &::sub_827D1138);
    PatchFuncMapping(0x831EDB68, &::sub_831EDB68);
    PatchFuncMapping(0x831D7BD8, &::sub_831D7BD8);
    
    printf("[PATCH] Sync primitive patching complete.\n");
    fflush(stdout);
}

// Static initializer to run patching before Memory::Memory()
struct SyncPrimitivePatcher {
    SyncPrimitivePatcher() {
        PatchSyncPrimitives();
    }
};

// This global will be constructed before Memory, patching the function table
static SyncPrimitivePatcher g_syncPatcher;

} // anonymous namespace

// =============================================================================
// TARGETED TIMING FIXES - Known Wait Loop Registry
// =============================================================================
// Instead of global slowdowns, we identify specific wait loops and inject yields
// only where needed. This fixes timing issues without impacting overall performance.
// =============================================================================

// Registry of known spin-wait functions that need yield injection
// These are addresses of functions that contain busy-wait loops
// TODO: Replace GTA IV virtual address with GTA V offset
#if 0
static const std::unordered_set<uint32_t> g_knownSpinWaitFunctions = {
    0x82672E50,  // Audio/streaming init - has 2 busy-wait loops
    0x82672AA8,  // Loop 1 helper in sub_82672E50
    0x829736F8,  // Loop 2 helper in sub_82672E50
    0x827E7B38,  // File streaming queue init - infinite loop
    0x827DB880,  // Worker manager shutdown loop
    0x823005E0,  // Worker cleanup - waits for workers to exit
};
#else
static const std::unordered_set<uint32_t> g_knownSpinWaitFunctions = {};
#endif

// Helper to check if a function address is a known spin-wait
inline bool IsKnownSpinWait(uint32_t funcAddr) {
    return g_knownSpinWaitFunctions.count(funcAddr) > 0;
}

// =============================================================================
// COMPREHENSIVE SEMAPHORE FIX - Following UnleashedRecomp Pattern
// =============================================================================
// Problem: Worker threads wait on semaphores that are never signaled, causing deadlocks.
// 
// Root cause: The game's init sequence creates worker threads that wait on semaphores,
// but the code that should signal those semaphores either:
// 1. Never runs (bypassed functions)
// 2. Runs in wrong order (race conditions)
// 3. Depends on hardware that doesn't exist on PC
//
// Solution: Track all blocking semaphores (count=0 at init) and provide:
// 1. Timeout-based fallback to prevent infinite waits
// 2. Signaling mechanism after init completes
// 3. Proper synchronization that matches reference projects
// =============================================================================

// Forward declarations - actual tracking functions defined after Semaphore struct
static std::mutex g_semaphoreTrackMutex;
static std::unordered_set<uint32_t> g_blockingSemaphoreAddrs;  // Guest addresses of blocking semaphores
static std::unordered_set<uint32_t> g_mutantHandles;  // Kernel handles of mutants (for auto-signal fallback)
static std::atomic<bool> g_initComplete{false};
static std::atomic<int> g_semaphoreWaitCount{0};

// Track a semaphore address that was initialized with count=0
void TrackBlockingSemaphoreAddr(uint32_t addr) {
    std::lock_guard<std::mutex> lock(g_semaphoreTrackMutex);
    g_blockingSemaphoreAddrs.insert(addr);
}

bool IsBlockingSemaphore(uint32_t addr) {
    std::lock_guard<std::mutex> lock(g_semaphoreTrackMutex);
    return g_blockingSemaphoreAddrs.count(addr) > 0;
}

bool IsMutantHandle(uint32_t handle) {
    std::lock_guard<std::mutex> lock(g_semaphoreTrackMutex);
    return g_mutantHandles.count(handle) > 0;
}

bool IsInitComplete() {
    return g_initComplete.load();
}

// SetInitComplete and SignalAllBlockingSemaphores defined after Semaphore struct

// =============================================================================
// SDL Event Pumping Helper
// =============================================================================
// This is called from kernel functions that are invoked frequently during
// game execution to keep the window responsive. Without this, the window
// freezes because GuestThread::Start blocks the main thread.
// 
// IMPORTANT: On macOS, SDL event pumping MUST happen on the main thread.
// If called from a worker thread, we skip the pump to avoid crashes.

static std::chrono::steady_clock::time_point g_lastSdlPumpTime;
static constexpr auto SDL_PUMP_INTERVAL = std::chrono::milliseconds(16); // ~60fps
static std::thread::id g_kernelMainThreadId;
static std::atomic<bool> g_kernelMainThreadIdSet{false};

// =============================================================================
// DIAGNOSTIC WAIT & FILE TRACKING (Phase 7 Task 1)
// =============================================================================
struct ThreadWaitInfo {
    std::thread::id tid;
    uint32_t callerLR = 0;
    std::string waitType;
    uint32_t handleOrObj = 0;
    uint32_t timeoutMs = 0;
    std::chrono::steady_clock::time_point startTime;
};

static std::mutex g_waitTrackerMutex;
static std::unordered_map<std::thread::id, ThreadWaitInfo> g_activeThreadWaits;

struct ScopedWaitTracker {
    std::thread::id tid;
    ScopedWaitTracker(const char* type, uint32_t handleOrObj, uint32_t timeoutMs, uint32_t callerLR) {
        tid = std::this_thread::get_id();
        std::lock_guard<std::mutex> lock(g_waitTrackerMutex);
        g_activeThreadWaits[tid] = ThreadWaitInfo{
            .tid = tid,
            .callerLR = callerLR,
            .waitType = type,
            .handleOrObj = handleOrObj,
            .timeoutMs = timeoutMs,
            .startTime = std::chrono::steady_clock::now()
        };
    }
    ~ScopedWaitTracker() {
        std::lock_guard<std::mutex> lock(g_waitTrackerMutex);
        g_activeThreadWaits.erase(tid);
    }
};

static std::mutex g_recentFilesMutex;
static std::deque<std::string> g_recentFiles;

static void RecordRecentFile(const std::string& path) {
    if (path.empty()) return;
    std::lock_guard<std::mutex> lock(g_recentFilesMutex);
    if (g_recentFiles.empty() || g_recentFiles.back() != path) {
        g_recentFiles.push_back(path);
        if (g_recentFiles.size() > 5) {
            g_recentFiles.pop_front();
        }
    }
}

void StartVBlankTimer();

std::atomic<uint64_t> g_mainFrameCounter{0};
std::atomic<uint64_t> g_vdSwapCounter{0};
std::atomic<uint64_t> g_pm4PacketCounter{0};
static std::chrono::steady_clock::time_point g_lastVdSwapTime = std::chrono::steady_clock::now();

#ifndef _WIN32
#include <pthread.h>
#include <csignal>
#include <execinfo.h>
static pthread_t g_kernelMainPthread;
static void MainThreadSampleHandler(int sig)
{
    void* frames[32];
    int count = backtrace(frames, 32);
    fprintf(stderr, "\n--- [MAIN THREAD SAMPLE (posix)] ---\n");
    backtrace_symbols_fd(frames, count, STDERR_FILENO);
    fprintf(stderr, "------------------------------------\n");
}
#endif

void InitKernelMainThread()
{
    g_kernelMainThreadId = std::this_thread::get_id();
    g_kernelMainThreadIdSet = true;
#ifndef _WIN32
    g_kernelMainPthread = pthread_self();
    struct sigaction sa{};
    sa.sa_handler = MainThreadSampleHandler;
    sigaction(SIGUSR1, &sa, nullptr);
#endif
    StartVBlankTimer();
}

bool IsMainThread()
{
    if (!g_kernelMainThreadIdSet) return false;
    return std::this_thread::get_id() == g_kernelMainThreadId;
}

void PumpSdlEventsIfNeeded()
{
    // On macOS / Linux, SDL event pumping MUST happen on the main thread
    if (!IsMainThread()) return;
    
    auto now = std::chrono::steady_clock::now();
    if (now - g_lastSdlPumpTime >= SDL_PUMP_INTERVAL)
    {
        g_lastSdlPumpTime = now;
        SDL_PumpEvents();
        
        // Process critical events (window close, etc.)
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                std::_Exit(0);
            }
        }

        static auto s_lastTitleUpdate = std::chrono::steady_clock::now();
        if (now - s_lastTitleUpdate >= std::chrono::milliseconds(250))
        {
            s_lastTitleUpdate = now;
            if (GameWindow::s_pWindow)
            {
                char title[256];
                snprintf(title, sizeof(title), "FiveInvader | Frames: %llu | Presents: %llu | VdSwap: %llu | PM4: %llu",
                    (unsigned long long)g_mainFrameCounter.load(std::memory_order_relaxed),
                    (unsigned long long)g_frameCounter.load(std::memory_order_relaxed),
                    (unsigned long long)g_vdSwapCounter.load(std::memory_order_relaxed),
                    (unsigned long long)g_pm4PacketCounter.load(std::memory_order_relaxed));
                SDL_SetWindowTitle(GameWindow::s_pWindow, title);
            }
        }
    }
}

// =============================================================================

std::unordered_map<uint32_t, uint32_t> g_handleDuplicates{};

// PHASE 2: Global flag for storage init tracing
static std::atomic<bool> g_inStorageInit{false};

namespace
{
    constexpr uint32_t STATUS_INVALID_HANDLE = 0xC0000008;
    constexpr uint32_t STATUS_INVALID_PARAMETER = 0xC000000D;
    constexpr uint32_t STATUS_INFO_LENGTH_MISMATCH = 0xC0000004;
    constexpr uint32_t STATUS_NO_SUCH_FILE = 0xC000000F;
    constexpr uint32_t STATUS_END_OF_FILE = 0xC0000011;
    constexpr uint32_t STATUS_ACCESS_DENIED = 0xC0000022;
    constexpr uint32_t STATUS_OBJECT_NAME_NOT_FOUND = 0xC0000034;
    constexpr uint32_t STATUS_OBJECT_PATH_NOT_FOUND = 0xC000003A;
    constexpr uint32_t STATUS_OBJECT_NAME_INVALID = 0xC0000033;
    constexpr uint32_t STATUS_OBJECT_NAME_COLLISION = 0xC0000035;
    constexpr uint32_t STATUS_BUFFER_OVERFLOW = 0x80000005;
    constexpr uint32_t STATUS_NO_MORE_FILES = 0x80000006;

    // FILE_INFORMATION_CLASS (subset)
    constexpr uint32_t FileBasicInformation = 4;
    constexpr uint32_t FileStandardInformation = 5;
    constexpr uint32_t FilePositionInformation = 14;
    constexpr uint32_t FileEndOfFileInformation = 20;
    constexpr uint32_t FileNetworkOpenInformation = 34;

    // FS_INFORMATION_CLASS (subset)
    constexpr uint32_t FileFsSizeInformation = 3;
    constexpr uint32_t FileFsDeviceInformation = 4;
    constexpr uint32_t FileFsAttributeInformation = 5;

    constexpr uint32_t kNtFileHandleMagic = 0x4E544649; // 'NTFI'
    constexpr uint32_t kNtDirHandleMagic = 0x4E544449; // 'NTDI'
    constexpr uint32_t kNtVirtFileHandleMagic = 0x4E545646; // 'NTVF'

    struct NtFileHandle final : KernelObject
    {
        uint32_t magic = kNtFileHandleMagic;
        std::fstream stream;
        std::filesystem::path path;
        // RPF metadata cached per-open
        bool rpfHeaderParsed = false;
        bool isRpf = false;
        uint32_t tocOffset = 0;
        uint32_t tocSize = 0;
        bool tocEncrypted = false;
    };

    struct NtDirHandle final : KernelObject
    {
        uint32_t magic = kNtDirHandleMagic;
        std::filesystem::path path;
        std::vector<std::filesystem::directory_entry> entries;
        size_t cursor = 0;
        bool initialized = false;
    };

    struct NtVirtFileHandle final : KernelObject
    {
        uint32_t magic = kNtVirtFileHandleMagic;
        std::filesystem::path debugPath;
        std::vector<uint8_t> data;
    };

    static std::unordered_map<uint32_t, NtFileHandle*> g_ntFileHandles;
    static std::unordered_map<uint32_t, NtDirHandle*> g_ntDirHandles;
    static std::unordered_map<uint32_t, NtVirtFileHandle*> g_ntVirtFileHandles;
    
    // PM4 Packet Types (Xbox 360 GPU command buffer format)
    enum PM4PacketType {
        PM4_TYPE0 = 0,  // Register writes
        PM4_TYPE1 = 1,  // Two register writes  
        PM4_TYPE2 = 2,  // Filler/NOP
        PM4_TYPE3 = 3,  // Commands
    };
    
    // PM4 Type 3 Opcodes (from Xenia xenos.h)
    enum PM4Type3Opcode {
        PM4_ME_INIT               = 0x48,
        PM4_NOP                   = 0x10,
        PM4_INDIRECT_BUFFER_PFD   = 0x32,
        PM4_INDIRECT_BUFFER       = 0x3f,
        PM4_WAIT_REG_MEM          = 0x3c,
        PM4_REG_RMW               = 0x21,
        PM4_REG_TO_MEM            = 0x3e,
        PM4_MEM_WRITE             = 0x3d,
        PM4_COND_WRITE            = 0x45,
        PM4_EVENT_WRITE           = 0x46,
        PM4_EVENT_WRITE_SHD       = 0x58,
        PM4_EVENT_WRITE_EXT       = 0x5a,
        PM4_EVENT_WRITE_ZPD       = 0x5b,
        PM4_DRAW_INDX             = 0x22,
        PM4_DRAW_INDX_2           = 0x36,
        PM4_VIZ_QUERY             = 0x23,
        PM4_SET_CONSTANT          = 0x2d,
        PM4_SET_CONSTANT2         = 0x55,
        PM4_SET_SHADER_CONSTANTS  = 0x56,
        PM4_IM_LOAD               = 0x27,
        PM4_IM_LOAD_IMMEDIATE     = 0x2b,
        PM4_INVALIDATE_STATE      = 0x3b,
        PM4_SET_SHADER_BASES      = 0x4A,
        PM4_INTERRUPT             = 0x54,
        PM4_XE_SWAP               = 0x64,
    };
    
    // GPU Ring Buffer state for GPU command processing
    struct GpuRingBufferState {
        uint32_t ringBufferBase = 0;      // Physical address of ring buffer
        uint32_t ringBufferSize = 0;      // Size in bytes (1 << size_log2)
        uint32_t readPtrWritebackAddr = 0; // Address where GPU writes read pointer
        uint32_t blockSize = 0;           // Block size for writeback (1 << block_size_log2)
        uint32_t interruptCallback = 0;   // Graphics interrupt callback function
        uint32_t interruptUserData = 0;   // User data for interrupt callback
        bool initialized = false;
        bool writebackEnabled = false;
        bool interruptFired = false;
        
        // Persistent video state flags - the GPU polling loop checks these
        bool enginesInitialized = false;
        bool edramTrainingComplete = false;
        bool interruptSeen = false;
        
        // PM4 command processing state
        uint32_t lastKnownWritePtr = 0;   // Last write pointer we saw
        uint32_t processedReadPtr = 0;    // Where we've processed up to
        
        // Diagnostic counters
        uint32_t pm4DrawCount = 0;
        uint32_t pm4ShaderLoadCount = 0;
        uint32_t pm4SetConstantCount = 0;
        uint32_t pm4SwapCount = 0;
        uint32_t pm4OtherCount = 0;
    };
    static GpuRingBufferState g_gpuRingBuffer;
    
    // PM4 opcode name for logging
    static const char* GetPM4OpcodeName(uint32_t opcode) {
        switch (opcode) {
            case PM4_ME_INIT: return "ME_INIT";
            case PM4_NOP: return "NOP";
            case PM4_INDIRECT_BUFFER_PFD: return "INDIRECT_BUFFER_PFD";
            case PM4_INDIRECT_BUFFER: return "INDIRECT_BUFFER";
            case PM4_WAIT_REG_MEM: return "WAIT_REG_MEM";
            case PM4_REG_RMW: return "REG_RMW";
            case PM4_REG_TO_MEM: return "REG_TO_MEM";
            case PM4_MEM_WRITE: return "MEM_WRITE";
            case PM4_COND_WRITE: return "COND_WRITE";
            case PM4_EVENT_WRITE: return "EVENT_WRITE";
            case PM4_EVENT_WRITE_SHD: return "EVENT_WRITE_SHD";
            case PM4_EVENT_WRITE_EXT: return "EVENT_WRITE_EXT";
            case PM4_EVENT_WRITE_ZPD: return "EVENT_WRITE_ZPD";
            case PM4_DRAW_INDX: return "DRAW_INDX";
            case PM4_DRAW_INDX_2: return "DRAW_INDX_2";
            case PM4_VIZ_QUERY: return "VIZ_QUERY";
            case PM4_SET_CONSTANT: return "SET_CONSTANT";
            case PM4_SET_CONSTANT2: return "SET_CONSTANT2";
            case PM4_SET_SHADER_CONSTANTS: return "SET_SHADER_CONSTANTS";
            case PM4_IM_LOAD: return "IM_LOAD";
            case PM4_IM_LOAD_IMMEDIATE: return "IM_LOAD_IMMEDIATE";
            case PM4_INVALIDATE_STATE: return "INVALIDATE_STATE";
            case PM4_SET_SHADER_BASES: return "SET_SHADER_BASES";
            case PM4_INTERRUPT: return "INTERRUPT";
            case PM4_XE_SWAP: return "XE_SWAP";
            default: return "UNKNOWN";
        }
    }
    
    // PPC function call tracking for D3D address identification
    static std::unordered_map<uint32_t, uint32_t> g_ppcFunctionCallCounts;
    static std::mutex g_ppcCallCountMutex;
    static uint32_t g_ppcCallTrackingEnabled = 0;  // 0=off, 1=tracking, 2=reported
    
    void TrackPPCFunctionCall(uint32_t address) {
        if (g_ppcCallTrackingEnabled != 1) return;
        if (address < 0x82000000 || address > 0x82FFFFFF) return;  // Only track game code
        
        std::lock_guard<std::mutex> lock(g_ppcCallCountMutex);
        g_ppcFunctionCallCounts[address]++;
    }
    
    void ReportHotPPCFunctions() {
        if (g_ppcCallTrackingEnabled != 1) return;
        g_ppcCallTrackingEnabled = 2;  // Mark as reported to prevent repeated reports
        
        std::lock_guard<std::mutex> lock(g_ppcCallCountMutex);
        
        // Find functions in the D3D address ranges that are called frequently
        std::vector<std::pair<uint32_t, uint32_t>> hotFunctions;
        for (const auto& [addr, count] : g_ppcFunctionCallCounts) {
            // Focus on 0x829Dxxxx (GTA V D3D wrapper) and 0x826Fxxxx (possible draw functions)
            if ((addr >= 0x829D0000 && addr <= 0x829EFFFF) ||
                (addr >= 0x826F0000 && addr <= 0x826FFFFF) ||
                (addr >= 0x82540000 && addr <= 0x8254FFFF)) {
                if (count >= 100) {
                    hotFunctions.push_back({addr, count});
                }
            }
        }
        
        // Sort by count descending
        std::sort(hotFunctions.begin(), hotFunctions.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        // Log top 20 hot D3D functions
        LOGF_WARNING("[HOT PPC FUNCTIONS] Found {} hot functions in D3D ranges:", hotFunctions.size());
        for (size_t i = 0; i < std::min(hotFunctions.size(), size_t(20)); i++) {
            LOGF_WARNING("  0x{:08X}: {} calls", hotFunctions[i].first, hotFunctions[i].second);
        }
    }
    
    // Global RPF streams for translation layer - opened once, used for all reads
    struct RpfStreamInfo {
        std::fstream stream;
        std::filesystem::path path;
        uint64_t fileSize = 0;
        bool headerParsed = false;
        uint32_t tocOffset = 0;
        uint32_t tocSize = 0;
        bool tocEncrypted = false;
        std::vector<uint8_t> decryptedToc;  // Pre-decrypted TOC for fast access
        bool tocDecrypted = false;
    };
    static std::unordered_map<std::string, std::unique_ptr<RpfStreamInfo>> g_rpfStreams;
    
    // Map any handle to its backing RPF name (e.g., "common", "xbox360", "audio")
    static std::unordered_map<uint32_t, std::string> g_handleToRpf;
    
    // Offset-to-file mapping for serving extracted files
    struct RpfFileEntry {
        uint32_t offset;
        uint32_t size;
        const char* path;
    };
    
    // File mapping for common.rpf - maps RPF offsets to extracted file paths
    // Generated from parsing the decrypted TOC
    static const RpfFileEntry g_commonRpfFiles[] = {
        {0x005000,   469881, "data/action_table.csv"},
        {0x00C800,   123153, "data/ambient.dat"},
        {0x010000,    12063, "data/animgrp.dat"},
        {0x010800,    24102, "data/carcols.dat"},
        {0x012800,     5554, "data/cargrp.dat"},
        {0x013000,    22853, "data/credits_360.dat"},
        {0x0152FE,      882, "data/default.dat"},
        {0x015800,    11342, "data/default.ide"},
        {0x016800,     5534, "data/femaleplayersettings.dat"},
        {0x017000,     9855, "data/fonts.dat"},
        {0x018000,    13883, "data/frontend_360.dat"},
        {0x019000,    36821, "data/frontend_menus.xml"},
        {0x01A000,     8022, "data/gta.dat"},
        {0x01B000,    41386, "data/handling.dat"},
        {0x01D800,     9498, "data/hud.dat"},
        {0x01E000,     4529, "data/hudcolor.dat"},
        {0x01E800,     5964, "data/images.txt"},
        {0x01F000,    18884, "data/info.zon"},
        {0x020800,     3779, "data/introspline.csv"},
        {0x021000,    77653, "data/leaderboards_data.xml"},
        {0x022000,    14880, "data/loadingscreens_360.dat"},
        {0x023000,     4828, "data/maleplayersettings.dat"},
        {0x024000,     9047, "data/meleeanims.dat"},
        {0x025000,     7988, "data/moveblend.dat"},
        {0x025800,      974, "data/nav.dat"},
        {0x026000,   139863, "data/navprecalc.dat"},
        {0x03B000,     6873, "data/networktest.dat"},
        {0x03C000,     8735, "data/pedgrp.dat"},
        {0x03D000,    89589, "data/pedpersonality.dat"},
        {0x03E800,    96255, "data/peds.ide"},
        {0x041000,   158690, "data/pedvariations.dat"},
        {0x043000,    10774, "data/plants.dat"},
        {0x044000,   594605, "data/popcycle.dat"},
        {0x048000,   593598, "data/popcycle.datnew"},
        {0x04C800,    10800, "data/precincts.dat"},
        {0x04D000,     3490, "data/radiohud.dat"},
        {0x04D800,     1393, "data/radiologo.dat"},
        {0x04E000,     9208, "data/relationships.dat"},
        {0x04E800,    12207, "data/scenarios.dat"},
        {0x04F800,     4046, "data/scrollbars.dat"},
        {0x050000,    18052, "data/startup.sco"},
        {0x051000,     5107, "data/statexport.dat"},
        {0x052000,     6665, "data/surface.dat"},
        {0x053000,    10312, "data/surfaceaudio.dat"},
        {0x054000,     5149, "data/timecycle.dat"},
        {0x055000,     5149, "data/timecyclemod.dat"},
        {0x056000,     2451, "data/traincamnodes.txt"},
        {0x057000,     5036, "data/traintrack.dat"},
        {0x058000,    16626, "data/tuneables.dat"},
        {0x059000,    15168, "data/vehcomps.dat"},
        {0x05A800,   286690, "data/vehoff.csv"},
        {0x063000,    43058, "data/vehicles.ide"},
        {0x068000,    35451, "data/visualsettings.dat"},
        {0x06C000,    16393, "data/water.dat"},
        {0x06D000,    29497, "data/weaponinfo.xml"},
        {0x070000,    10127, "data/weapons.dat"},
        {0x072000,    15680, "data/weather.dat"},
        {0x087800,   270035, "data/maps/occlu.ipl"},
        {0x09B800,  1720829, "data/maps/paths.ipl"},
        {0x198800,    99007, "shaders/fxl_final/deferred_lighting.fxc"},
        {0x19F800,    38797, "shaders/fxl_final/gta_atmoscatt_clouds.fxc"},
        {0x1A2800,    29220, "shaders/fxl_final/gta_cubemap_reflect.fxc"},
        {0x1A5000,    28838, "shaders/fxl_final/gta_cutout_fence.fxc"},
        {0x1A7800,    29201, "shaders/fxl_final/gta_decal.fxc"},
        {0x1AA000,    29122, "shaders/fxl_final/gta_decal_amb_only.fxc"},
        {0x1AC800,    28356, "shaders/fxl_final/gta_decal_dirt.fxc"},
        {0x1AF000,    29385, "shaders/fxl_final/gta_decal_glue.fxc"},
        {0x1B1800,    31357, "shaders/fxl_final/gta_decal_normal_only.fxc"},
        {0x1B4800,    28758, "shaders/fxl_final/gta_default.fxc"},
        {0x1B7000,    17697, "shaders/fxl_final/gta_diffuse_instance.fxc"},
        {0x1B8800,    20703, "shaders/fxl_final/gta_emissive.fxc"},
        {0x1BA000,    21405, "shaders/fxl_final/gta_emissivenight.fxc"},
        {0x1BB800,    20965, "shaders/fxl_final/gta_emissivestrong.fxc"},
        {0x1BD000,    31661, "shaders/fxl_final/gta_glass.fxc"},
        {0x1C0000,    23504, "shaders/fxl_final/gta_glass_emissive.fxc"},
        {0x1C2000,    24048, "shaders/fxl_final/gta_glass_emissivenight.fxc"},
        {0x1C4000,    33090, "shaders/fxl_final/gta_glass_normal_spec_reflect.fxc"},
        {0x1C7000,    30062, "shaders/fxl_final/gta_glass_reflect.fxc"},
        {0x1C9800,    29649, "shaders/fxl_final/gta_glass_spec.fxc"},
        {0x1CC800,    23995, "shaders/fxl_final/gta_hair_sorted_alpha.fxc"},
        {0x1CF000,    23882, "shaders/fxl_final/gta_hair_sorted_alpha_exp.fxc"},
        {0x1D1800,    14196, "shaders/fxl_final/gta_im.fxc"},
        {0x1D3000,    31405, "shaders/fxl_final/gta_normal.fxc"},
        {0x1D6000,    31219, "shaders/fxl_final/gta_normal_cubemap_reflect.fxc"},
        {0x1D9000,    31454, "shaders/fxl_final/gta_normal_decal.fxc"},
        {0x1DC000,    30991, "shaders/fxl_final/gta_normal_reflect.fxc"},
        {0x1DE800,    31123, "shaders/fxl_final/gta_normal_reflect_alpha.fxc"},
        {0x1E1000,    31044, "shaders/fxl_final/gta_normal_reflect_decal.fxc"},
        {0x1E3800,    32750, "shaders/fxl_final/gta_normal_spec.fxc"},
        {0x1E6800,    34062, "shaders/fxl_final/gta_normal_spec_cubemap_reflect.fxc"},
        {0x1E9800,    32791, "shaders/fxl_final/gta_normal_spec_decal.fxc"},
        {0x1EC800,    35048, "shaders/fxl_final/gta_normal_spec_reflect.fxc"},
        {0x1EF800,    35109, "shaders/fxl_final/gta_normal_spec_reflect_decal.fxc"},
        {0x1F2800,    24134, "shaders/fxl_final/gta_normal_spec_reflect_emissive.fxc"},
        {0x1F4800,    24686, "shaders/fxl_final/gta_normal_spec_reflect_emissivenight.fxc"},
        {0x1F6800,    32284, "shaders/fxl_final/gta_parallax.fxc"},
        {0x1F9800,    33573, "shaders/fxl_final/gta_parallax_specmap.fxc"},
        {0x1FC800,    32784, "shaders/fxl_final/gta_parallax_steep.fxc"},
        {0x1FF800,    24963, "shaders/fxl_final/gta_ped.fxc"},
        {0x202000,    26468, "shaders/fxl_final/gta_ped_reflect.fxc"},
        {0x204800,    25642, "shaders/fxl_final/gta_ped_skin.fxc"},
        {0x207000,    32242, "shaders/fxl_final/gta_ped_skin_blendshape.fxc"},
        {0x20A000,    42311, "shaders/fxl_final/gta_projtex.fxc"},
        {0x20E000,    42775, "shaders/fxl_final/gta_projtex_steep.fxc"},
        {0x212800,    28972, "shaders/fxl_final/gta_reflect.fxc"},
        {0x215000,    28969, "shaders/fxl_final/gta_reflect_decal.fxc"},
        {0x21F800,    31551, "shaders/fxl_final/gta_spec.fxc"},
        {0x222800,    31572, "shaders/fxl_final/gta_spec_decal.fxc"},
        {0x225800,    32083, "shaders/fxl_final/gta_spec_reflect.fxc"},
        {0x228800,    32116, "shaders/fxl_final/gta_spec_reflect_decal.fxc"},
        {0x22B800,    12493, "shaders/fxl_final/gta_terrain_va_2lyr.fxc"},
        {0x22C800,    16869, "shaders/fxl_final/gta_terrain_va_3lyr.fxc"},
        {0x22D800,    21225, "shaders/fxl_final/gta_terrain_va_4lyr.fxc"},
        {0x22E800,    11423, "shaders/fxl_final/gta_trees.fxc"},
        {0x22F800,    39294, "shaders/fxl_final/gta_vehicle_badges.fxc"},
        {0x232800,    31620, "shaders/fxl_final/gta_vehicle_basic.fxc"},
        {0x235000,    43225, "shaders/fxl_final/gta_vehicle_chrome.fxc"},
        {0x238800,    28625, "shaders/fxl_final/gta_vehicle_disc.fxc"},
        {0x23A800,    43656, "shaders/fxl_final/gta_vehicle_generic.fxc"},
        {0x23E000,    42850, "shaders/fxl_final/gta_vehicle_interior.fxc"},
        {0x241800,    41461, "shaders/fxl_final/gta_vehicle_interior2.fxc"},
        {0x245000,    45261, "shaders/fxl_final/gta_vehicle_lightsemissive.fxc"},
        {0x249000,    44350, "shaders/fxl_final/gta_vehicle_mesh.fxc"},
        {0x24C800,    43525, "shaders/fxl_final/gta_vehicle_paint1.fxc"},
        {0x250000,    43269, "shaders/fxl_final/gta_vehicle_paint2.fxc"},
        {0x253800,    44582, "shaders/fxl_final/gta_vehicle_paint3.fxc"},
        {0x257000,    32462, "shaders/fxl_final/gta_vehicle_rims1.fxc"},
        {0x259000,    32134, "shaders/fxl_final/gta_vehicle_rims2.fxc"},
        {0x25B000,    37859, "shaders/fxl_final/gta_vehicle_rubber.fxc"},
        {0x25E000,    37472, "shaders/fxl_final/gta_vehicle_shuts.fxc"},
        {0x261000,    33297, "shaders/fxl_final/gta_vehicle_tire.fxc"},
        {0x263800,    39399, "shaders/fxl_final/gta_vehicle_vehglass.fxc"},
        {0x267000,    29706, "shaders/fxl_final/gta_wire.fxc"},
        {0x26E000,    36486, "shaders/fxl_final/rage_postfx.fxc"},
        {0x273000,    10514, "shaders/fxl_final/water.fxc"},
        {0x275000,  2767809, "text/american.gxt"},
        {0x519000,  3034116, "text/french.gxt"},
        {0x7FE000,  3048486, "text/german.gxt"},
        {0xAE6800,  2915066, "text/italian.gxt"},
        {0xDAE800,  2876898, "text/spanish.gxt"},
        {0, 0, nullptr}  // Sentinel
    };
    
    // Find extracted file for a given RPF offset
    // Since RPF files can have overlapping ranges (due to compression), we need to find
    // the file whose start offset is closest to (but not greater than) the target offset
    static const RpfFileEntry* FindExtractedFile(const std::string& rpfName, uint32_t offset)
    {
        const RpfFileEntry* entries = nullptr;
        if (rpfName == "common")
            entries = g_commonRpfFiles;
        // TODO: Add xbox360 and audio mappings
        
        if (!entries)
            return nullptr;
        
        const RpfFileEntry* bestMatch = nullptr;
        uint32_t bestDistance = UINT32_MAX;
        
        for (const RpfFileEntry* e = entries; e->path != nullptr; ++e)
        {
            // File must start at or before the target offset
            if (e->offset <= offset)
            {
                // Check if target is within this file's range
                if (offset < e->offset + e->size)
                {
                    // Find the file with the smallest distance (closest start offset)
                    uint32_t distance = offset - e->offset;
                    if (distance < bestDistance)
                    {
                        bestDistance = distance;
                        bestMatch = e;
                    }
                }
            }
        }
        return bestMatch;
    }
    
    // Cache for opened extracted files
    struct ExtractedFileCache {
        std::fstream stream;
        std::string path;
        uint32_t rpfOffset;
        uint32_t fileSize;
    };
    static std::unordered_map<std::string, std::unique_ptr<ExtractedFileCache>> g_extractedFileCache;
    
    // Get or open an RPF stream
    static RpfStreamInfo* GetRpfStream(const std::string& rpfName)
    {
        auto it = g_rpfStreams.find(rpfName);
        if (it != g_rpfStreams.end())
            return it->second.get();
        
        // Archives in the extracted disc image live at its root. Keep the
        // legacy game/ layout as a fallback for older extracted installs.
        const std::filesystem::path rootRpfPath = GetGamePath() / (rpfName + ".rpf");
        const std::filesystem::path legacyRpfPath = GetGamePath() / "game" / (rpfName + ".rpf");
        std::filesystem::path rpfPath;
        if (std::filesystem::is_regular_file(rootRpfPath))
            rpfPath = rootRpfPath;
        else if (std::filesystem::is_regular_file(legacyRpfPath))
            rpfPath = legacyRpfPath;
        else
        {
            LOGF_IMPL(Utility, "GetRpfStream", "RPF not found at '{}' or '{}'",
                      rootRpfPath.string(), legacyRpfPath.string());
            return nullptr;
        }
        
        auto info = std::make_unique<RpfStreamInfo>();
        info->stream.open(rpfPath, std::ios::in | std::ios::binary);
        if (!info->stream.is_open())
        {
            LOGF_IMPL(Utility, "GetRpfStream", "Failed to open RPF: {}", rpfPath.string());
            return nullptr;
        }
        
        info->path = rpfPath;
        {
            info->stream.clear();
            info->stream.seekg(0, std::ios::end);
            const std::streampos endPos = info->stream.tellg();
            if (endPos > 0)
                info->fileSize = static_cast<uint64_t>(endPos);
            info->stream.clear();
            info->stream.seekg(0, std::ios::beg);
        }
        
        // Parse the archive header. RPF7 stores the TOC size in big-endian
        // form, with bit 31 indicating encrypted TOC data.
        char hdr[20] = {};
        info->stream.read(hdr, sizeof(hdr));
        if (info->stream.gcount() >= 20)
        {
            bool isRpf2 = (hdr[0] == 'R' && hdr[1] == 'P' && hdr[2] == 'F' && hdr[3] == '2');
            bool isRpf3 = (hdr[0] == 'R' && hdr[1] == 'P' && hdr[2] == 'F' && hdr[3] == '3');
            bool isRpf7 = (hdr[0] == 'R' && hdr[1] == 'P' && hdr[2] == 'F' && hdr[3] == '7');
            if (isRpf2 || isRpf3 || isRpf7)
            {
                auto le32 = [](const uint8_t* p) -> uint32_t {
                    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
                };
                auto be32 = [](const uint8_t* p) -> uint32_t {
                    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
                };
                const uint32_t encodedTocSize = isRpf7
                    ? be32(reinterpret_cast<const uint8_t*>(hdr + 8))
                    : le32(reinterpret_cast<const uint8_t*>(hdr + 4));
                info->tocSize = isRpf7 ? (encodedTocSize & 0x7FFFFFFFu) : encodedTocSize;
                info->tocOffset = isRpf7 ? 0x10 : 0x800;
                info->tocEncrypted = isRpf7
                    ? (encodedTocSize & 0x80000000u) != 0
                    : le32(reinterpret_cast<const uint8_t*>(hdr + 16)) != 0;

                const uint64_t tocEnd = static_cast<uint64_t>(info->tocOffset) + info->tocSize;
                if (info->tocSize != 0 && tocEnd <= info->fileSize)
                {
                    info->headerParsed = true;
                    LOGF_IMPL(Utility, "GetRpfStream", "Opened {} - tocOffset=0x{:X} tocSize={} encrypted={}",
                              rpfName, info->tocOffset, info->tocSize, info->tocEncrypted);
                }
                else
                {
                    LOGF_IMPL(Utility, "GetRpfStream", "Invalid {} TOC bounds: offset=0x{:X} size={} fileSize={}",
                              rpfName, info->tocOffset, info->tocSize, info->fileSize);
                }
            }
        }
        info->stream.clear();
        info->stream.seekg(0, std::ios::beg);
        
        RpfStreamInfo* ptr = info.get();
        g_rpfStreams[rpfName] = std::move(info);
        return ptr;
    }

    static void DecryptRpfBufferInPlace(uint8_t* data, uint32_t length, uint64_t tocRelativeOffset);
    
    static void EnsureTocDecrypted(RpfStreamInfo* rpf)
    {
        if (!rpf || rpf->tocDecrypted || !rpf->headerParsed || !rpf->tocEncrypted)
            return;
        if (rpf->tocSize == 0)
            return;
        
        rpf->decryptedToc.resize(rpf->tocSize);
        rpf->stream.clear();
        rpf->stream.seekg(rpf->tocOffset, std::ios::beg);
        rpf->stream.read(reinterpret_cast<char*>(rpf->decryptedToc.data()), rpf->tocSize);
        
        if (rpf->stream.gcount() != static_cast<std::streamsize>(rpf->tocSize))
        {
            LOGF_IMPL(Utility, "EnsureTocDecrypted", "Failed to read TOC: got {} bytes, expected {}", 
                      rpf->stream.gcount(), rpf->tocSize);
            rpf->decryptedToc.clear();
            return;
        }
        
        DecryptRpfBufferInPlace(rpf->decryptedToc.data(), rpf->tocSize, 0);
        rpf->tocDecrypted = true;
        LOGF_IMPL(Utility, "EnsureTocDecrypted", "Pre-decrypted {} bytes of TOC", rpf->tocSize);
    }

    // Read from extracted file given RPF name and offset
    static uint32_t ReadFromExtractedFile(const std::string& rpfName, uint8_t* hostBuffer, uint32_t size, uint32_t offset, int count)
    {
        const RpfFileEntry* entry = FindExtractedFile(rpfName, offset);
        if (!entry)
            return 0;
        
        // Calculate offset within the extracted file
        const uint32_t fileOffset = offset - entry->offset;
        if (fileOffset >= entry->size)
            return 0;
        
        // Build path to extracted file
        std::filesystem::path extractedPath = GetGamePath() / "extracted" / rpfName / entry->path;
        
        // Check cache first
        auto cacheKey = extractedPath.string();
        auto cacheIt = g_extractedFileCache.find(cacheKey);
        ExtractedFileCache* cache = nullptr;
        
        if (cacheIt != g_extractedFileCache.end())
        {
            cache = cacheIt->second.get();
        }
        else
        {
            // Open the extracted file
            if (!std::filesystem::exists(extractedPath))
            {
                if (count <= 20)
                {
                    LOGF_IMPL(Utility, "GTA5_FileLoad", "Extracted file not found: {}", extractedPath.string());
                }
                return 0;
            }
            
            auto newCache = std::make_unique<ExtractedFileCache>();
            newCache->stream.open(extractedPath, std::ios::in | std::ios::binary);
            if (!newCache->stream.is_open())
            {
                if (count <= 20)
                {
                    LOGF_IMPL(Utility, "GTA5_FileLoad", "Failed to open extracted file: {}", extractedPath.string());
                }
                return 0;
            }
            
            newCache->path = cacheKey;
            newCache->rpfOffset = entry->offset;
            newCache->fileSize = entry->size;
            
            cache = newCache.get();
            g_extractedFileCache[cacheKey] = std::move(newCache);
            
            if (count <= 50)
            {
                LOGF_IMPL(Utility, "GTA5_FileLoad", "Opened extracted file: {} (rpfOffset=0x{:X} size={})",
                          entry->path, entry->offset, entry->size);
            }
        }
        
        // Read from the extracted file
        cache->stream.clear();
        cache->stream.seekg(fileOffset, std::ios::beg);
        
        const uint32_t availableInFile = entry->size - fileOffset;
        const uint32_t toRead = std::min(size, availableInFile);
        
        cache->stream.read(reinterpret_cast<char*>(hostBuffer), toRead);
        const uint32_t bytesRead = static_cast<uint32_t>(cache->stream.gcount());
        
        if (count <= 50 || count % 100 == 0)
        {
            LOGF_IMPL(Utility, "GTA5_FileLoad", "Read {} bytes from extracted '{}' at fileOffset=0x{:X}",
                      bytesRead, entry->path, fileOffset);
        }
        
        return bytesRead;
    }
    
    static uint32_t ReadFromRpfStream(RpfStreamInfo* rpf, uint8_t* hostBuffer, uint32_t size, uint32_t offset, int count, const std::string& rpfName)
    {
        if (!rpf || !rpf->stream.is_open())
            return 0;
        if (rpf->fileSize != 0 && offset >= rpf->fileSize)
            return 0;
        
        // Ensure TOC is pre-decrypted for fast access
        EnsureTocDecrypted(rpf);
        
        // Check if this read overlaps with the TOC region
        const uint64_t tocStart = rpf->tocOffset;
        const uint64_t tocEnd = tocStart + rpf->tocSize;
        const uint64_t readStart = offset;
        const uint64_t readEnd = offset + size;
        
        // If we have a pre-decrypted TOC and the read overlaps it, serve from cache
        if (rpf->tocDecrypted && !rpf->decryptedToc.empty() && readEnd > tocStart && readStart < tocEnd)
        {
            // Read overlaps TOC - serve decrypted data
            uint32_t totalRead = 0;
            
            // Part before TOC (if any)
            if (readStart < tocStart)
            {
                const uint32_t beforeLen = static_cast<uint32_t>(tocStart - readStart);
                rpf->stream.clear();
                rpf->stream.seekg(offset, std::ios::beg);
                rpf->stream.read(reinterpret_cast<char*>(hostBuffer), beforeLen);
                totalRead += static_cast<uint32_t>(rpf->stream.gcount());
            }
            
            // TOC overlap part - serve from decrypted cache
            const uint64_t overlapStart = std::max(readStart, tocStart);
            const uint64_t overlapEnd = std::min(readEnd, tocEnd);
            if (overlapEnd > overlapStart)
            {
                const uint32_t tocOff = static_cast<uint32_t>(overlapStart - tocStart);
                const uint32_t bufOffset = static_cast<uint32_t>(overlapStart - readStart);
                const uint32_t len = static_cast<uint32_t>(overlapEnd - overlapStart);
                
                memcpy(hostBuffer + bufOffset, rpf->decryptedToc.data() + tocOff, len);
                totalRead += len;
                
                if (count <= 20 || count % 100 == 0)
                {
                    LOGF_IMPL(Utility, "GTA5_FileLoad", "Served {} bytes from pre-decrypted TOC at tocOffset=0x{:X}",
                        len, tocOff);
                }
            }
            
            // Part after TOC - serve raw RPF data (not extracted text files)
            if (readEnd > tocEnd)
            {
                const uint32_t afterOffset = static_cast<uint32_t>(tocEnd);
                const uint32_t afterLen = static_cast<uint32_t>(readEnd - tocEnd);
                const uint32_t bufOffset = static_cast<uint32_t>(tocEnd - readStart);
                
                // Read raw RPF data directly - extracted files are TEXT format, game expects BINARY
                rpf->stream.clear();
                rpf->stream.seekg(afterOffset, std::ios::beg);
                rpf->stream.read(reinterpret_cast<char*>(hostBuffer + bufOffset), afterLen);
                totalRead += static_cast<uint32_t>(rpf->stream.gcount());
            }
            
            return totalRead;
        }
        
        // For reads past TOC, read raw RPF data directly
        // NOTE: Extracted files are TEXT format, but game expects BINARY RPF format
        
        // Read from raw RPF
        rpf->stream.clear();
        rpf->stream.seekg(offset, std::ios::beg);
        
        if (rpf->stream.bad())
            return 0;
        
        rpf->stream.read(reinterpret_cast<char*>(hostBuffer), size);
        const uint32_t bytesRead = static_cast<uint32_t>(rpf->stream.gcount());
        
        if (bytesRead == 0)
            return 0;
        
        // Fallback: decrypt TOC region on-the-fly if not using cache
        if (rpf->headerParsed && rpf->tocEncrypted && !rpf->tocDecrypted)
        {
            const uint64_t overlapStart = std::max(readStart, tocStart);
            const uint64_t overlapEnd = std::min(static_cast<uint64_t>(offset + bytesRead), tocEnd);
            
            if (overlapEnd > overlapStart)
            {
                const uint32_t startInBuf = static_cast<uint32_t>(overlapStart - readStart);
                const uint32_t len = static_cast<uint32_t>(overlapEnd - overlapStart);
                const uint64_t tocRelativeOffset = overlapStart - tocStart;
                
                if (count <= 20 || count % 100 == 0)
                {
                    LOGF_IMPL(Utility, "GTA5_FileLoad", "Decrypting TOC region on-the-fly: bufOffset={} len={} tocRelOffset={}",
                        startInBuf, len, tocRelativeOffset);
                }
                
                DecryptRpfBufferInPlace(hostBuffer + startInBuf, len, tocRelativeOffset);
            }
        }
        
        return bytesRead;
    }

    static uint32_t ReadFromBestRpf(uint32_t handle, uint8_t* hostBuffer, uint32_t size, uint32_t offset, int count, std::string& outName)
    {
        static const std::string kCommon = "common";
        static const std::string kXbox360 = "xbox360";
        static const std::string kAudio = "audio";
        
        auto tryName = [&](const std::string& name) -> uint32_t {
            RpfStreamInfo* rpf = GetRpfStream(name);
            if (!rpf)
                return 0;
            if (rpf->fileSize != 0 && offset >= rpf->fileSize)
                return 0;
            return ReadFromRpfStream(rpf, hostBuffer, size, offset, count, name);
        };
        
        if (auto mapIt = g_handleToRpf.find(handle); mapIt != g_handleToRpf.end())
        {
            const std::string mapped = mapIt->second;
            if (uint32_t n = tryName(mapped); n != 0)
            {
                outName = mapped;
                return n;
            }
            g_handleToRpf.erase(mapIt);
        }
        
        if (uint32_t n = tryName(kCommon); n != 0) { outName = kCommon; return n; }
        if (uint32_t n = tryName(kXbox360); n != 0) { outName = kXbox360; return n; }
        if (uint32_t n = tryName(kAudio); n != 0) { outName = kAudio; return n; }
        
        return 0;
    }

    static std::vector<uint8_t> MakeEmptyRpfImage()
    {
        // Many GTA V code paths treat a trailing-slash open (e.g. "game:\\") as
        // "mount the disc/RPF container" and then read the RPF header.
        //
        // Provide a tiny, valid-looking header so the title can proceed without
        // triggering Dirty Disc, while we serve actual assets from extracted files.
        //
        // Header layout (20 bytes):
        //   magic[4] = 'RPF3' (bytes)
        //   toc_size u32
        //   entry_count u32
        //   unknown u32
        //   encrypted u32
        // We set all numeric fields to 0 (endian-agnostic).
        std::vector<uint8_t> buf(0x800, 0);
        buf[0] = 'R';
        buf[1] = 'P';
        buf[2] = 'F';
        buf[3] = '3';
        return buf;
    }

    static uint32_t AlignUp(uint32_t value, uint32_t align)
    {
        return (value + (align - 1)) & ~(align - 1);
    }

    // AES key for RPF TOC decryption (loaded from install directory)
    static std::vector<uint8_t> g_aesKey;

    static void LoadAesKeyIfNeeded()
    {
        if (!g_aesKey.empty())
            return;

        // Try multiple locations for AES key (user local files)
        const char* envPath = std::getenv("FIVEINVADER_AES_KEY");
        std::vector<std::filesystem::path> keyPaths;
        if (envPath && *envPath)
        {
            keyPaths.push_back(envPath);
        }
        keyPaths.push_back(GetGamePath() / "aes_key.bin");
        keyPaths.push_back(GetGamePath() / "encryption_key.bin");
        keyPaths.push_back(GetGamePath().parent_path() / "aes_key.bin");
        keyPaths.push_back(GetGamePath().parent_path() / "encryption_key.bin");
        keyPaths.push_back(std::filesystem::current_path() / "aes_key.bin");
        keyPaths.push_back(std::filesystem::current_path() / "encryption_key.bin");
        const char* homeDir = std::getenv("HOME");
        if (homeDir && *homeDir)
        {
            keyPaths.push_back(std::filesystem::path(homeDir) / ".fiveinvader" / "aes_key.bin");
            keyPaths.push_back(std::filesystem::path(homeDir) / ".fiveinvader" / "encryption_key.bin");
        }
        
        std::filesystem::path keyPath;
        for (const auto& path : keyPaths)
        {
            std::error_code ec;
            if (std::filesystem::exists(path, ec))
            {
                keyPath = path;
                break;
            }
        }
        
        if (keyPath.empty())
        {
            LOGF_IMPL(Utility, "LoadAesKeyIfNeeded", "AES key file not found in search paths (aes_key.bin / encryption_key.bin)");
            return;
        }

        std::ifstream f(keyPath, std::ios::binary);
        if (!f.is_open())
        {
            LOGF_IMPL(Utility, "LoadAesKeyIfNeeded", "Failed to open AES key: '{}'", keyPath.string());
            return;
        }

        std::vector<uint8_t> key((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (key.size() >= 32)
        {
            g_aesKey.assign(key.begin(), key.begin() + 32);
            LOGF_IMPL(Utility, "LoadAesKeyIfNeeded", "Loaded AES key ({} bytes) from '{}'", (int)g_aesKey.size(), keyPath.string());
        }
        else
        {
            LOGF_IMPL(Utility, "LoadAesKeyIfNeeded", "AES key file too small: {} bytes in '{}'", (int)key.size(), keyPath.string());
        }
    }

    // Decrypt RPF TOC data using AES-256 ECB repeated 16 times (SparkIV behavior).
    // This version handles misaligned buffer starts by only decrypting complete 16-byte blocks.
    // The caller is responsible for ensuring the buffer corresponds to aligned TOC offsets.
    static void DecryptRpfBufferInPlace(uint8_t* data, uint32_t length, uint64_t tocRelativeOffset = 0)
    {
        if (!data || length == 0)
            return;

        LoadAesKeyIfNeeded();
        if (g_aesKey.size() != 32)
        {
            static std::atomic<uint32_t> s_missingKeyCount{0};
            if (++s_missingKeyCount <= 5)
            {
                LOGF_IMPL(Utility, "DecryptRpfBufferInPlace", "AES key not available (size={}), skipping RPF decryption", g_aesKey.size());
            }
            return;
        }

        // AES-ECB works on 16-byte blocks. We need to handle the case where the read
        // doesn't start on a block boundary within the TOC.
        // 
        // Strategy: Find the first complete 16-byte block boundary in the buffer,
        // decrypt from there, and leave any partial leading/trailing bytes as-is.
        // This is safe because the game will typically read aligned chunks.
        
        const uint32_t blockSize = 16;
        
        // Calculate how many bytes into a block we are (based on TOC-relative offset)
        uint32_t offsetIntoBlock = static_cast<uint32_t>(tocRelativeOffset % blockSize);
        
        // Skip leading partial block bytes
        uint32_t skipBytes = (offsetIntoBlock == 0) ? 0 : (blockSize - offsetIntoBlock);
        if (skipBytes >= length)
        {
            // Entire buffer is within a partial block, can't decrypt
            LOGF_IMPL(Utility, "DecryptRpfBufferInPlace", "Buffer too small for aligned decrypt (len={} skip={})", length, skipBytes);
            return;
        }
        
        uint8_t* alignedStart = data + skipBytes;
        uint32_t remainingLen = length - skipBytes;
        uint32_t alignedLen = remainingLen & ~0x0Fu; // Round down to block boundary
        
        if (alignedLen == 0)
        {
            LOGF_IMPL(Utility, "DecryptRpfBufferInPlace", "No complete blocks to decrypt (len={} skip={})", length, skipBytes);
            return;
        }

#if defined(__APPLE__)
        std::vector<uint8_t> tmp(alignedLen);

        // Repeat AES-ECB-256 decrypt 16 times (matches SparkIV behavior)
        for (int iter = 0; iter < 16; ++iter)
        {
            size_t outLen = 0;
            CCCryptorStatus status = CCCrypt(kCCDecrypt,
                                             kCCAlgorithmAES,
                                             kCCOptionECBMode,
                                             g_aesKey.data(), kCCKeySizeAES256,
                                             nullptr,
                                             alignedStart, alignedLen,
                                             tmp.data(), alignedLen,
                                             &outLen);

            if (status != kCCSuccess || outLen != alignedLen)
            {
                LOGF_IMPL(Utility, "DecryptRpfBufferInPlace", "AES decrypt failed (status={} iter={})", status, iter);
                return;
            }

            // Copy back for next iteration or result
            memcpy(alignedStart, tmp.data(), alignedLen);
        }
        
        LOGF_IMPL(Utility, "DecryptRpfBufferInPlace", "Decrypted {} bytes (skipped {} leading bytes)", alignedLen, skipBytes);
#elif defined(__linux__)
        if (alignedLen > static_cast<uint32_t>(std::numeric_limits<int>::max()))
        {
            LOGF_IMPL(Utility, "DecryptRpfBufferInPlace",
                      "Aligned buffer exceeds OpenSSL limit (len={})", alignedLen);
            return;
        }

        std::vector<uint8_t> decrypted(alignedLen + EVP_MAX_BLOCK_LENGTH);
        for (int iter = 0; iter < 16; ++iter)
        {
            EVP_CIPHER_CTX* context = EVP_CIPHER_CTX_new();
            if (!context)
            {
                LOGF_IMPL(Utility, "DecryptRpfBufferInPlace",
                          "Failed to allocate OpenSSL cipher context");
                return;
            }

            int outputLength = 0;
            int finalLength = 0;
            const bool success =
                EVP_DecryptInit_ex(context, EVP_aes_256_ecb(), nullptr,
                                   g_aesKey.data(), nullptr) == 1 &&
                EVP_CIPHER_CTX_set_padding(context, 0) == 1 &&
                EVP_DecryptUpdate(context, decrypted.data(), &outputLength,
                                  alignedStart, static_cast<int>(alignedLen)) == 1 &&
                EVP_DecryptFinal_ex(context, decrypted.data() + outputLength,
                                    &finalLength) == 1;
            EVP_CIPHER_CTX_free(context);

            if (!success || static_cast<uint32_t>(outputLength + finalLength) != alignedLen)
            {
                LOGF_IMPL(Utility, "DecryptRpfBufferInPlace",
                          "OpenSSL AES decrypt failed (iter={})", iter);
                return;
            }

            std::memcpy(alignedStart, decrypted.data(), alignedLen);
        }
        LOGF_IMPL(Utility, "DecryptRpfBufferInPlace",
                  "Decrypted {} bytes (skipped {} leading bytes)", alignedLen, skipBytes);
#else
        // Non-macOS: no AES implementation available here.
        LOGF_IMPL(Utility, "DecryptRpfBufferInPlace", "AES decrypt not implemented on this platform");
#endif
    }

    static uint32_t ErrnoToNtStatus(int err)
    {
        switch (err)
        {
        case EACCES:
            return STATUS_ACCESS_DENIED;
        case ENOENT:
            return STATUS_NO_SUCH_FILE;
        case ENOTDIR:
            return STATUS_OBJECT_PATH_NOT_FOUND;
        default:
            return STATUS_FAIL_CHECK;
        }
    }

    static void ParseRpfHeader(NtFileHandle* h)
    {
        if (!h || h->rpfHeaderParsed)
            return;

        h->rpfHeaderParsed = true;

        try {
            const std::string ext = h->path.extension().string();
            std::string extLower = ext;
            std::transform(extLower.begin(), extLower.end(), extLower.begin(), [](unsigned char c){ return std::tolower(c); });
            if (extLower != ".rpf")
                return;

            h->isRpf = true;

            // Read first 20 bytes of header
            std::streampos cur = h->stream.tellg();
            h->stream.clear();
            h->stream.seekg(0, std::ios::beg);
            char hdr[20] = {};
            h->stream.read(hdr, sizeof(hdr));
            const std::streamsize got = h->stream.gcount();
            // restore position
            h->stream.clear();
            h->stream.seekg(cur, std::ios::beg);

            if (got < 20)
            {
                LOGF_IMPL(Utility, "ParseRpfHeader", "Failed to read header from '{}' (got {} bytes)", h->path.string(), got);
                return;
            }

            // GTA V Xbox 360 uses RPF2 (0x52504632 = "RPF2"), not RPF3
            // RPF2 Header layout (20 bytes):
            //   4b - INT32 - RPF Version (0x52504632 for RPF2)
            //   4b - INT32 - Table of Contents Size
            //   4b - INT32 - Number of Entries
            //   4b - INT32 - Unknown
            //   4b - INT32 - Encrypted (0 = unencrypted, non-zero = encrypted)
            
            // Check magic - accept RPF2, RPF3, and RPF7 (GTA V)
            bool isRpf2 = (hdr[0] == 'R' && hdr[1] == 'P' && hdr[2] == 'F' && hdr[3] == '2');
            bool isRpf3 = (hdr[0] == 'R' && hdr[1] == 'P' && hdr[2] == 'F' && hdr[3] == '3');
            bool isRpf7 = (hdr[0] == 'R' && hdr[1] == 'P' && hdr[2] == 'F' && hdr[3] == '7');
            
            if (!isRpf2 && !isRpf3 && !isRpf7)
            {
                LOGF_IMPL(Utility, "ParseRpfHeader", "Unknown RPF magic in '{}': {:02X} {:02X} {:02X} {:02X}", 
                          h->path.string(), (uint8_t)hdr[0], (uint8_t)hdr[1], (uint8_t)hdr[2], (uint8_t)hdr[3]);
                return;
            }

            auto le32 = [](const uint8_t* p) -> uint32_t {
                return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
            };
            auto be32 = [](const uint8_t* p) -> uint32_t {
                return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
            };

            uint32_t encodedTocSize = isRpf7 ? be32(reinterpret_cast<uint8_t*>(hdr + 8)) : le32(reinterpret_cast<uint8_t*>(hdr + 4));
            uint32_t tocSize = isRpf7 ? (encodedTocSize & 0x7FFFFFFFu) : encodedTocSize;
            uint32_t numEntries = isRpf7 ? be32(reinterpret_cast<uint8_t*>(hdr + 4)) : le32(reinterpret_cast<uint8_t*>(hdr + 8));
            uint32_t encrypted = isRpf7 ? (encodedTocSize & 0x80000000u) : le32(reinterpret_cast<uint8_t*>(hdr + 16));

            // TOC starts at 0x10 for RPF7 or 0x800 for RPF2/3
            h->tocOffset = isRpf7 ? 0x10 : 0x800;
            h->tocSize = tocSize;
            h->tocEncrypted = (encrypted != 0);

            const char* verStr = isRpf7 ? "RPF7" : (isRpf2 ? "RPF2" : "RPF3");
            LOGF_IMPL(Utility, "ParseRpfHeader", "Parsed {} header '{}': tocOffset=0x{:X} tocSize={} entries={} encrypted={}", 
                      verStr, h->path.filename().string(), h->tocOffset, tocSize, numEntries, h->tocEncrypted);
        }
        catch (...) {
            LOGF_IMPL(Utility, "ParseRpfHeader", "Exception parsing header for '{}'", h->path.string());
        }
    }

    static bool TryGetAnsiPath(const XOBJECT_ATTRIBUTES* attributes, std::string& out)
    {
        out.clear();
        if (!attributes)
        {
            LOG_IMPL(Utility, "TryGetAnsiPath", "attributes is null");
            return false;
        }

        const XANSI_STRING* name = attributes->Name.get();
        if (!name)
        {
            LOG_IMPL(Utility, "TryGetAnsiPath", "Name.get() is null");
            return false;
        }

        const char* buf = name->Buffer.get();
        if (!buf)
        {
            LOG_IMPL(Utility, "TryGetAnsiPath", "Buffer.get() is null");
            return false;
        }

        const uint16_t len = name->Length.get();
        // Sanity check: zero length or very large lengths are suspect
        if (len == 0)
        {
            // Empty paths will always fail - reject silently
            return false;
        }
        else if (len > 1024)
        {
            LOGF_IMPL(Utility, "TryGetAnsiPath", "Suspicious length: {} (max 1024)", len);
            return false;
        }
        else
        {
            // Check if the path looks valid
            // Valid paths should start with a letter (drive:) or a letter after game:\ prefix
            unsigned char firstChar = static_cast<unsigned char>(buf[0]);
            
            // First character should be printable ASCII for a valid path
            // Valid starts: letters (a-z, A-Z), backslash, dot, or space
            bool validStart = (firstChar >= 'A' && firstChar <= 'Z') ||
                              (firstChar >= 'a' && firstChar <= 'z') ||
                              firstChar == '\\' || firstChar == '.' || 
                              firstChar == ' ' || firstChar == '/';
            
            if (!validStart)
            {
                // Check if it's just high-byte garbage (common pattern)
                if (firstChar >= 128)
                {
                    LOGF_IMPL(Utility, "TryGetAnsiPath", "Garbage path rejected (len={}, first=0x{:02X}, Name.ptr=0x{:08X}, Buffer.ptr=0x{:08X})", 
                        len, firstChar, attributes->Name.ptr.value, name->Buffer.ptr.value);
                    return false;
                }
            }
            
            // Also check for excessive non-ASCII content
            size_t garbageCount = 0;
            for (size_t i = 0; i < std::min<size_t>(len, 20); i++)
            {
                unsigned char c = static_cast<unsigned char>(buf[i]);
                if (c >= 128)
                    garbageCount++;
            }
            // If more than 50% of first 20 chars are non-ASCII, reject
            if (garbageCount > 10)
            {
                LOGF_IMPL(Utility, "TryGetAnsiPath", "Mostly garbage path rejected (len={}, garbage={}/20)", len, garbageCount);
                return false;
            }
        }

        out.assign(buf, buf + len);
        return true;
    }

    static std::filesystem::path ResolveGuestPathBestEffort(const std::string& guestPath)
    {
        if (VFS::IsInitialized())
        {
            std::filesystem::path vfsResolved = VFS::Resolve(guestPath);
            std::error_code vfsEc;
            if (!vfsResolved.empty() && std::filesystem::exists(vfsResolved, vfsEc) && !vfsEc)
                return vfsResolved;
        }

        std::filesystem::path resolved = FileSystem::ResolvePath(guestPath, true);

        std::error_code ec;
        if (std::filesystem::exists(resolved, ec))
            return resolved;

        const std::filesystem::path cachedPath = FindInPathCache(resolved.string());
        if (!cachedPath.empty())
            return cachedPath;

        return resolved;
    }

    static uint32_t GetFileAttributesBestEffort(const std::filesystem::path& path)
    {
        std::error_code ec;
        const bool isDir = std::filesystem::is_directory(path, ec);
        if (ec)
            return FILE_ATTRIBUTE_NORMAL;

        if (isDir)
            return FILE_ATTRIBUTE_DIRECTORY;

        return FILE_ATTRIBUTE_NORMAL;
    }

    static uint64_t RoundUpToPage(uint64_t value)
    {
        constexpr uint64_t kPageSize = 0x1000;
        return (value + (kPageSize - 1)) & ~(kPageSize - 1);
    }

#pragma pack(push, 1)
    struct XFILE_BASIC_INFORMATION
    {
        be<int64_t> CreationTime;
        be<int64_t> LastAccessTime;
        be<int64_t> LastWriteTime;
        be<int64_t> ChangeTime;
        be<uint32_t> FileAttributes;
        be<uint32_t> Reserved;
    };

    struct XFILE_STANDARD_INFORMATION
    {
        be<int64_t> AllocationSize;
        be<int64_t> EndOfFile;
        be<uint32_t> NumberOfLinks;
        uint8_t DeletePending;
        uint8_t Directory;
        uint16_t Reserved;
    };

    struct XFILE_POSITION_INFORMATION
    {
        be<int64_t> CurrentByteOffset;
    };

    struct XFILE_END_OF_FILE_INFORMATION
    {
        be<int64_t> EndOfFile;
    };

    struct XFILE_NETWORK_OPEN_INFORMATION
    {
        be<int64_t> CreationTime;
        be<int64_t> LastAccessTime;
        be<int64_t> LastWriteTime;
        be<int64_t> ChangeTime;
        be<int64_t> AllocationSize;
        be<int64_t> EndOfFile;
        be<uint32_t> FileAttributes;
        be<uint32_t> Reserved;
    };

    struct XFILE_FS_SIZE_INFORMATION
    {
        be<int64_t> TotalAllocationUnits;
        be<int64_t> AvailableAllocationUnits;
        be<uint32_t> SectorsPerAllocationUnit;
        be<uint32_t> BytesPerSector;
    };

    struct XFILE_FS_DEVICE_INFORMATION
    {
        be<uint32_t> DeviceType;
        be<uint32_t> Characteristics;
    };

    struct XFILE_FS_ATTRIBUTE_INFORMATION_FIXED
    {
        be<uint32_t> FileSystemAttributes;
        be<uint32_t> MaximumComponentNameLength;
        be<uint32_t> FileSystemNameLength;
        be<uint16_t> FileSystemName[16];
    };

    // FILE_INFORMATION_CLASS for NtQueryDirectoryFile (subset)
    constexpr uint32_t FileDirectoryInformation = 1;
    constexpr uint32_t FileFullDirectoryInformation = 2;
    constexpr uint32_t FileBothDirectoryInformation = 3;

    struct XFILE_DIRECTORY_INFORMATION_FIXED
    {
        be<uint32_t> NextEntryOffset;
        be<uint32_t> FileIndex;
        be<int64_t> CreationTime;
        be<int64_t> LastAccessTime;
        be<int64_t> LastWriteTime;
        be<int64_t> ChangeTime;
        be<int64_t> EndOfFile;
        be<int64_t> AllocationSize;
        be<uint32_t> FileAttributes;
        be<uint32_t> FileNameLength;
        // Followed by FileName (UTF-16BE)
    };

    struct XFILE_FULL_DIR_INFORMATION_FIXED
    {
        be<uint32_t> NextEntryOffset;
        be<uint32_t> FileIndex;
        be<int64_t> CreationTime;
        be<int64_t> LastAccessTime;
        be<int64_t> LastWriteTime;
        be<int64_t> ChangeTime;
        be<int64_t> EndOfFile;
        be<int64_t> AllocationSize;
        be<uint32_t> FileAttributes;
        be<uint32_t> FileNameLength;
        be<uint32_t> EaSize;
        // Followed by FileName (UTF-16BE)
    };

    struct XFILE_BOTH_DIR_INFORMATION_FIXED
    {
        be<uint32_t> NextEntryOffset;
        be<uint32_t> FileIndex;
        be<int64_t> CreationTime;
        be<int64_t> LastAccessTime;
        be<int64_t> LastWriteTime;
        be<int64_t> ChangeTime;
        be<int64_t> EndOfFile;
        be<int64_t> AllocationSize;
        be<uint32_t> FileAttributes;
        be<uint32_t> FileNameLength;
        be<uint32_t> EaSize;
        uint8_t ShortNameLength;
        uint8_t Reserved1;
        be<uint16_t> ShortName[12];
        // Followed by FileName (UTF-16BE)
    };
#pragma pack(pop)

    static_assert(sizeof(XFILE_BASIC_INFORMATION) == 40);
    static_assert(sizeof(XFILE_STANDARD_INFORMATION) == 24);
    static_assert(sizeof(XFILE_POSITION_INFORMATION) == 8);
    static_assert(sizeof(XFILE_END_OF_FILE_INFORMATION) == 8);
    static_assert(sizeof(XFILE_NETWORK_OPEN_INFORMATION) == 56);
    static_assert(sizeof(XFILE_FS_SIZE_INFORMATION) == 24);
    static_assert(sizeof(XFILE_FS_DEVICE_INFORMATION) == 8);
}

struct Event final : KernelObject, HostObject<XKEVENT>
{
    bool manualReset;
    std::atomic<bool> signaled;

    Event(XKEVENT* header)
        : manualReset(!header->Type), signaled(!!header->SignalState)
    {
    }

    Event(bool manualReset, bool initialState)
        : manualReset(manualReset), signaled(initialState)
    {
    }

    uint32_t Wait(uint32_t timeout) override
    {
        if (timeout == 0)
        {
            if (manualReset)
            {
                if (!signaled)
                    return STATUS_TIMEOUT;
            }
            else
            {
                bool expected = true;
                if (!signaled.compare_exchange_strong(expected, false))
                    return STATUS_TIMEOUT;
            }
        }
        else if (timeout == INFINITE)
        {
            if (IsMainThread())
            {
                while (true)
                {
                    PumpSdlEventsIfNeeded();
                    if (manualReset)
                    {
                        if (signaled.load())
                            break;
                    }
                    else
                    {
                        bool expected = true;
                        if (signaled.compare_exchange_weak(expected, false))
                            break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
            else
            {
                if (manualReset)
                {
                    signaled.wait(false);
                }
                else
                {
                    while (true)
                    {
                        bool expected = true;
                        if (signaled.compare_exchange_weak(expected, false))
                            break;

                        signaled.wait(expected);
                    }
                }
            }
        }
        else
        {
            // Finite timeout - convert to milliseconds and do a timed wait
            // Xbox timeout is in 100ns units, negative means relative
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
            
            if (manualReset)
            {
                while (!signaled.load())
                {
                    if (IsMainThread())
                        PumpSdlEventsIfNeeded();
                    if (std::chrono::steady_clock::now() >= deadline)
                        return STATUS_TIMEOUT;
                    std::this_thread::sleep_for(std::chrono::milliseconds(IsMainThread() ? 10 : 1));
                }
            }
            else
            {
                while (true)
                {
                    if (IsMainThread())
                        PumpSdlEventsIfNeeded();
                    bool expected = true;
                    if (signaled.compare_exchange_weak(expected, false))
                        break;
                    
                    if (std::chrono::steady_clock::now() >= deadline)
                        return STATUS_TIMEOUT;
                    std::this_thread::sleep_for(std::chrono::milliseconds(IsMainThread() ? 10 : 1));
                }
            }
        }

        return STATUS_SUCCESS;
    }

    bool Set()
    {
        signaled = true;

        if (manualReset)
            signaled.notify_all();
        else
            signaled.notify_one();

        return TRUE;
    }

    bool Reset()
    {
        signaled = false;
        return TRUE;
    }
};

static std::atomic<uint32_t> g_keSetEventGeneration;

struct Semaphore final : KernelObject, HostObject<XKSEMAPHORE>
{
    std::atomic<uint32_t> count;
    uint32_t maximumCount;

    Semaphore(XKSEMAPHORE* semaphore)
        : count(semaphore->Header.SignalState), maximumCount(semaphore->Limit)
    {
    }

    Semaphore(uint32_t count, uint32_t maximumCount)
        : count(count), maximumCount(maximumCount)
    {
    }

    uint32_t Wait(uint32_t timeout) override
    {
        static int s_waitCount = 0;
        ++s_waitCount;
        
        if (timeout == 0)
        {
            uint32_t currentCount = count.load();
            if (currentCount != 0)
            {
                if (count.compare_exchange_weak(currentCount, currentCount - 1))
                    return STATUS_SUCCESS;
            }

            return STATUS_TIMEOUT;
        }
        else if (timeout == INFINITE)
        {
            if (IsMainThread())
            {
                while (true)
                {
                    PumpSdlEventsIfNeeded();
                    uint32_t currentCount = count.load();
                    if (currentCount != 0)
                    {
                        if (count.compare_exchange_weak(currentCount, currentCount - 1))
                            return STATUS_SUCCESS;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
            else
            {
                uint32_t currentCount;
                int loopCount = 0;
                while (true)
                {
                    currentCount = count.load();
                    if (currentCount != 0)
                    {
                        if (count.compare_exchange_weak(currentCount, currentCount - 1))
                        {
                            if (s_waitCount <= 30)
                                LOGF_WARNING("[SemWait] #{} acquired count={} after {} loops", s_waitCount, currentCount, loopCount);
                            return STATUS_SUCCESS;
                        }
                    }
                    else
                    {
                        if (loopCount == 0 && s_waitCount <= 30)
                            LOGF_WARNING("[SemWait] #{} blocking on count=0 (max={})...", s_waitCount, maximumCount);
                        ++loopCount;
                        count.wait(0);
                    }
                }
            }

            return STATUS_SUCCESS;
        }
        else
        {
            // Handle arbitrary timeout - convert to timed wait
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
            uint32_t currentCount;
            while (true)
            {
                if (IsMainThread())
                    PumpSdlEventsIfNeeded();
                currentCount = count.load();
                if (currentCount != 0)
                {
                    if (count.compare_exchange_weak(currentCount, currentCount - 1))
                        return STATUS_SUCCESS;
                }
                else
                {
                    if (std::chrono::steady_clock::now() >= deadline)
                        return STATUS_TIMEOUT;
                    std::this_thread::sleep_for(std::chrono::milliseconds(IsMainThread() ? 10 : 1));
                }
            }
        }
    }

    void Release(uint32_t releaseCount, uint32_t* previousCount)
    {
        if (previousCount != nullptr)
            *previousCount = count;

        assert(count + releaseCount <= maximumCount);

        count += releaseCount;
        count.notify_all();
    }
};

// =============================================================================
// SEMAPHORE SIGNALING FUNCTIONS (defined after Semaphore struct)
// =============================================================================

// Signal all tracked blocking semaphores - called after init completes
// Signals unconditionally to wake workers that may have acquired and are waiting again
void SignalAllBlockingSemaphores() {
    // Copy the set while holding the lock, then release lock before signaling
    // This prevents deadlock when awakened threads try to track new semaphores
    std::unordered_set<uint32_t> addrsCopy;
    {
        std::lock_guard<std::mutex> lock(g_semaphoreTrackMutex);
        addrsCopy = g_blockingSemaphoreAddrs;
    }
    
    int signaled = 0;
    for (uint32_t addr : addrsCopy) {
        // Try as kernel handle first (from NtCreateSemaphore)
        if (IsKernelObject(addr)) {
            Semaphore* sem = GetKernelObject<Semaphore>(addr);
            if (sem) {
                sem->Release(1, nullptr);
                signaled++;
            }
            continue;
        }
        // Try as guest address (from KeInitializeSemaphore)
        XKSEMAPHORE* semaphore = reinterpret_cast<XKSEMAPHORE*>(g_memory.Translate(addr));
        if (semaphore && semaphore->Header.Type == 5) {
            auto* object = QueryKernelObject<Semaphore>(semaphore->Header);
            if (object) {
                object->Release(1, nullptr);
                signaled++;
            }
        }
    }
    if (signaled > 0) {
        LOGF_WARNING("[SEMAPHORE_FIX] Signaled {} semaphores", signaled);
    }
}

// Mark init as complete and signal blocked semaphores
void SetInitComplete() {
    if (!g_initComplete.exchange(true)) {
        LOGF_WARNING("[SEMAPHORE_FIX] Init complete - signaling {} blocked semaphores", 
                     g_blockingSemaphoreAddrs.size());
        SignalAllBlockingSemaphores();
    }
}

// =============================================================================
// WORKER CONTEXT REGISTRY AND SHUTDOWN
// =============================================================================

// Worker context structure - describes a worker thread's context in guest memory
struct WorkerContext {
    uint32_t contextBase;      // Guest memory address of worker context
    uint32_t exitFlagOffset;   // Offset to exit flag (byte)
    uint32_t semHandleOffset;  // Offset to semaphore handle (word)
    const char* name;          // Debug name
};

// Registry of known worker contexts
// Add new workers here as they are discovered
static std::vector<WorkerContext> g_workerContexts = {
    // Context at 0x82B9C1D0, exit flag at +10 (decimal), semaphore handle at +20 (decimal)
    // PPC: lbz r11,10(r26) for exit flag, lwz r3,20(r26) for semaphore
    {0x82B9C1D0, 10, 20, "PhotoMode"},
};

// Shutdown all known workers by setting exit flags and signaling semaphores
// This allows workers to exit their wait loops gracefully
void ShutdownAllWorkers() {
    printf("[WORKER_SHUTDOWN] Shutting down %zu known workers...\n", g_workerContexts.size());
    fflush(stdout);
    
    int shutdownCount = 0;
    for (const auto& worker : g_workerContexts) {
        // Set exit flag to 1
        uint8_t* exitFlag = reinterpret_cast<uint8_t*>(g_memory.Translate(worker.contextBase + worker.exitFlagOffset));
        if (exitFlag) {
            *exitFlag = 1;
            printf("[WORKER_SHUTDOWN] %s: Set exit flag at 0x%08X\n", worker.name, worker.contextBase + worker.exitFlagOffset);
        }
        
        // Read semaphore handle from guest memory (big-endian)
        uint32_t* semHandlePtr = reinterpret_cast<uint32_t*>(g_memory.Translate(worker.contextBase + worker.semHandleOffset));
        if (semHandlePtr) {
            uint32_t semHandle = __builtin_bswap32(*semHandlePtr);  // Convert from big-endian
            if (semHandle != 0) {
                // Try as kernel handle
                if (IsKernelObject(semHandle)) {
                    Semaphore* sem = GetKernelObject<Semaphore>(semHandle);
                    if (sem) {
                        sem->Release(1, nullptr);
                        printf("[WORKER_SHUTDOWN] %s: Signaled semaphore handle 0x%08X\n", worker.name, semHandle);
                        shutdownCount++;
                    }
                }
            }
        }
    }
    
    printf("[WORKER_SHUTDOWN] Signaled %d workers\n", shutdownCount);
    fflush(stdout);
}

// Timeout-based wait for semaphores during init - prevents infinite blocking
// Returns true if semaphore was acquired, false if timeout
bool TryWaitSemaphoreWithTimeout(uint32_t semAddr, uint32_t timeoutMs) {
    XKSEMAPHORE* semaphore = reinterpret_cast<XKSEMAPHORE*>(g_memory.Translate(semAddr));
    if (!semaphore || semaphore->Header.Type != 5) return false;
    
    auto* object = QueryKernelObject<Semaphore>(semaphore->Header);
    if (!object) return false;
    
    auto startTime = std::chrono::steady_clock::now();
    while (true) {
        uint32_t currentCount = object->count.load();
        if (currentCount != 0) {
            if (object->count.compare_exchange_weak(currentCount, currentCount - 1)) {
                return true;  // Successfully acquired
            }
        }
        
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= timeoutMs) {
            return false;  // Timeout
        }
        
        // Brief sleep before retry
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

inline void CloseKernelObject(XDISPATCHER_HEADER& header)
{
    if (header.WaitListHead.Flink != OBJECT_SIGNATURE)
    {
        return;
    }

    DestroyKernelObject(header.WaitListHead.Blink);
}

uint32_t GuestTimeoutToMilliseconds(be<int64_t>* timeout)
{
    return timeout ? (*timeout * -1) / 10000 : INFINITE;
}

void VdHSIOCalibrationLock()
{
    LOG_UTILITY("!!! STUB !!!");
}

void KeCertMonitorData()
{
    LOG_UTILITY("!!! STUB !!!");
}

void XexExecutableModuleHandle()
{
    LOG_UTILITY("!!! STUB !!!");
}

void ExLoadedCommandLine()
{
    LOG_UTILITY("!!! STUB !!!");
}

void KeDebugMonitorData()
{
    LOG_UTILITY("!!! STUB !!!");
}

void ExThreadObjectType()
{
    LOG_UTILITY("!!! STUB !!!");
}

void KeTimeStampBundle()
{
    LOG_UTILITY("!!! STUB !!!");
}

void XboxHardwareInfo()
{
    LOG_UTILITY("!!! STUB !!!");
}

void XGetVideoMode()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t XGetGameRegion()
{
    if (Config::Language == ELanguage::Japanese)
        return 0x0101;

    return 0x03FF;
}

uint32_t XMsgStartIORequest(uint32_t App, uint32_t Message, XXOVERLAPPED* lpOverlapped, void* Buffer, uint32_t szBuffer)
{
    // XGI App ID is 0xFB (251)
    // Message 0x000B0008 = XGIUserWriteAchievements
    if (App == 0xFB && Message == 0x000B0008 && Buffer != nullptr && szBuffer >= 8)
    {
        // Buffer layout:
        // [0] = achievement_count (uint32_t)
        // [4] = achievements_ptr (uint32_t) -> points to XUSER_ACHIEVEMENT array
        be<uint32_t>* bufferData = static_cast<be<uint32_t>*>(Buffer);
        uint32_t achievementCount = bufferData[0];
        uint32_t achievementsPtr = bufferData[1];
        
        LOGF_WARNING("[XGI] XGIUserWriteAchievements: count={} ptr=0x{:08X}", achievementCount, achievementsPtr);
        
        if (achievementCount > 0 && achievementsPtr != 0)
        {
            // XUSER_ACHIEVEMENT structure (8 bytes each):
            // [0] = dwUserIndex (uint32_t)
            // [4] = dwAchievementId (uint32_t)
            be<uint32_t>* achievements = reinterpret_cast<be<uint32_t>*>(g_memory.Translate(achievementsPtr));
            
            for (uint32_t i = 0; i < achievementCount && i < 100; i++) // Cap at 100 for safety
            {
                uint32_t userIndex = achievements[i * 2];     // offset 0
                uint32_t achievementId = achievements[i * 2 + 1]; // offset 4
                
                LOGF_WARNING("[XGI] Achievement unlock: user={} id={}", userIndex, achievementId);
                
                // Call our achievement manager to unlock and show popup
                AchievementManager::Unlock(static_cast<uint16_t>(achievementId));
            }
        }
    }
    
    return STATUS_SUCCESS;
}


uint32_t XamGetSystemVersion()
{
    return 0;
}

void XamContentDelete()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t XamContentGetCreator(uint32_t userIndex, const XCONTENT_DATA* contentData, be<uint32_t>* isCreator, be<uint64_t>* xuid, XXOVERLAPPED* overlapped)
{
    if (isCreator)
        *isCreator = true;

    if (xuid)
        *xuid = 0xB13EBABEBABEBABE;

    return 0;
}

uint32_t XamContentGetDeviceState()
{
    return 0;
}

uint32_t XamUserGetSigninInfo(uint32_t userIndex, uint32_t flags, XUSER_SIGNIN_INFO* info)
{
    if (userIndex == 0)
    {
        memset(info, 0, sizeof(*info));
        info->xuid = 0xB13EBABEBABEBABE;
        info->SigninState = 1;
        strcpy(info->Name, "SWA");
        return 0;
    }

    return 0x00000525; // ERROR_NO_SUCH_USER
}

void XamShowSigninUI()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t XamShowDeviceSelectorUI
(
    uint32_t userIndex,
    uint32_t contentType,
    uint32_t contentFlags,
    uint64_t totalRequested,
    be<uint32_t>* deviceId,
    XXOVERLAPPED* overlapped
)
{
    XamNotifyEnqueueEvent(9, true);
    *deviceId = 1;
    XamNotifyEnqueueEvent(9, false);
    return 0;
}

void XamShowDirtyDiscErrorUI()
{
    static int callCount = 0;
    ++callCount;
    
    // Pump SDL events to keep window responsive
    PumpSdlEventsIfNeeded();
    
    // Only log first few to avoid spam
    if (callCount <= 3) {
        LOGF_UTILITY("!!! STUB !!! - Dirty disc error #{} (bypassing)", callCount);
    }
    
    // DON'T exit - just return and let the game continue
    // The game may retry or find an alternative code path
    // Small delay to prevent tight CPU loop
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Return normally - game will either retry or continue
}

void XamEnableInactivityProcessing()
{
    LOG_UTILITY("!!! STUB !!!");
}

void XamResetInactivity()
{
    LOG_UTILITY("!!! STUB !!!");
}

void XamShowMessageBoxUIEx()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t XGetLanguage()
{
    return (uint32_t)Config::Language.Value;
}

uint32_t XGetAVPack()
{
    return 0;
}

void XamLoaderTerminateTitle()
{
    LOG_WARNING("[XamLoaderTerminateTitle] Guest requested title termination!");
}

struct XAM_EXECUTION_ID {
    be<uint32_t> m_media_id;
    be<uint32_t> m_version;
    be<uint32_t> m_base_version;
    be<uint32_t> m_title_id;
    uint8_t m_platform;
    uint8_t m_executable_type;
    uint8_t m_disc_num;
    uint8_t m_discs_total;
    be<uint32_t> m_flags;
};

uint32_t XamGetExecutionId(uint32_t pExecutionId)
{
    if (pExecutionId != 0) {
        auto* execId = reinterpret_cast<XAM_EXECUTION_ID*>(g_memory.Translate(pExecutionId));
        if (execId) {
            execId->m_media_id = 0x545408A7;
            execId->m_version = 0x00010000;
            execId->m_base_version = 0x00010000;
            execId->m_title_id = 0x545408A7;
            execId->m_platform = 1;         // Xbox 360
            execId->m_executable_type = 1;  // Title
            execId->m_disc_num = 2;         // Disc 2 (Play Disc)
            execId->m_discs_total = 2;
            execId->m_flags = 0;
            LOGF_INFO("[TRACE] XamGetExecutionId: Populated TitleID=0x545408A7 at 0x{:08X}", pExecutionId);
        }
    }
    return 0; // STATUS_SUCCESS
}

uint32_t XamLoaderLaunchTitle(uint32_t titlePath, uint32_t flags)
{
    const char* str = (titlePath != 0 && g_memory.base != nullptr && titlePath >= 0x10000 && titlePath < PPC_MEMORY_SIZE)
        ? reinterpret_cast<const char*>(g_memory.base + titlePath) : "(null)";
    LOGF_INFO("[XamLoaderLaunchTitle] titlePath=0x{:08X} ('{}') flags=0x{:X}", titlePath, str, flags);
    return 0; // STATUS_SUCCESS
}

// NtOpenFile is essentially a simplified NtCreateFile for opening existing files
uint32_t NtOpenFile(
    be<uint32_t>* FileHandle,
    uint32_t DesiredAccess,
    XOBJECT_ATTRIBUTES* Attributes,
    XIO_STATUS_BLOCK* IoStatusBlock,
    uint32_t ShareAccess,
    uint32_t OpenOptions)
{
    (void)ShareAccess;
    (void)OpenOptions;

    if (!FileHandle || !Attributes)
        return STATUS_INVALID_PARAMETER;

    std::string guestPath;
    if (!TryGetAnsiPath(Attributes, guestPath))
        return STATUS_INVALID_PARAMETER;
    RecordRecentFile(guestPath);

    // DesiredAccess on Xbox uses Win32-like GENERIC_READ/WRITE bits.
    std::ios::openmode mode = std::ios::binary;
    if (DesiredAccess & (GENERIC_READ | FILE_READ_DATA))
        mode |= std::ios::in;
    if (DesiredAccess & GENERIC_WRITE)
        mode |= std::ios::out;

    // Fallback: treat unknown access flags as read.
    if ((mode & (std::ios::in | std::ios::out)) == 0)
        mode |= std::ios::in;

    std::filesystem::path resolved = ResolveGuestPathBestEffort(guestPath);

    LOGF_IMPL(Utility, "NtOpenFile", "Guest: '{}' -> Resolved: '{}'", guestPath, resolved.string());

    // Check for directory
    {
        std::error_code ec;
        bool exists = std::filesystem::exists(resolved, ec);
        bool isDir = exists && std::filesystem::is_directory(resolved, ec);
        
        LOGF_IMPL(Utility, "NtOpenFile", "exists={} isDir={} ec={}", exists, isDir, ec ? ec.message() : "none");
        
        if (exists && isDir && !ec)
        {
            NtDirHandle* hDir = CreateKernelObject<NtDirHandle>();
            hDir->path = resolved;

            const uint32_t handleValue = GetKernelHandle(hDir);
            g_ntDirHandles.emplace(handleValue, hDir);

            *FileHandle = handleValue;
            LOGF_IMPL(Utility, "NtOpenFile", "Created directory handle 0x{:08X} for {}", handleValue, resolved.string());
            if (IoStatusBlock)
            {
                IoStatusBlock->Status = STATUS_SUCCESS;
                IoStatusBlock->Information = 1;
            }

            return STATUS_SUCCESS;
        }
    }

    std::fstream stream;
    stream.open(resolved, mode);

    if (!stream.is_open())
    {
        const uint32_t status = ErrnoToNtStatus(errno);
        LOGF_IMPL(Utility, "NtOpenFile", "FAILED to open '{}' -> '{}': errno={} status=0x{:08X}", 
                  guestPath, resolved.string(), errno, status);
        if (IoStatusBlock)
        {
            IoStatusBlock->Status = status;
            IoStatusBlock->Information = 0;
        }
        *FileHandle = 0;
        return status;
    }

    NtFileHandle* h = CreateKernelObject<NtFileHandle>();
    h->stream = std::move(stream);
    h->path = std::move(resolved);
    // Mark RPF files for special handling (TOC decryption)
    try {
        std::string ext = h->path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
        if (ext == ".rpf")
            h->isRpf = true;
    } catch (...) {}

    const uint32_t handleValue = GetKernelHandle(h);
    g_ntFileHandles.emplace(handleValue, h);

    *FileHandle = handleValue;
    if (App::s_logVfs)
        LOGF_INFO("[TRACE] NtOpenFile: '{}' -> handle=0x{:08X}", guestPath, handleValue);
    if (IoStatusBlock)
    {
        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = 1;
    }
    return STATUS_SUCCESS;
}

void RtlInitAnsiString(XANSI_STRING* destination, char* source)
{
    const uint16_t length = source ? (uint16_t)strlen(source) : 0;
    destination->Length = length;
    destination->MaximumLength = length + 1;
    destination->Buffer = source;
}

uint32_t NtCreateFile
(
    be<uint32_t>* FileHandle,
    uint32_t DesiredAccess,
    XOBJECT_ATTRIBUTES* Attributes,
    XIO_STATUS_BLOCK* IoStatusBlock,
    uint64_t* AllocationSize,
    uint32_t FileAttributes,
    uint32_t ShareAccess,
    uint32_t CreateDisposition
)
{
    // Pump SDL events to keep window responsive
    PumpSdlEventsIfNeeded();
    
    (void)AllocationSize;
    (void)FileAttributes;
    (void)ShareAccess;

    if (!FileHandle || !Attributes)
        return STATUS_INVALID_PARAMETER;

    std::string guestPath;
    if (!TryGetAnsiPath(Attributes, guestPath))
        return STATUS_INVALID_PARAMETER;
    RecordRecentFile(guestPath);

    static std::atomic<uint32_t> s_createTraceCount{0};
    const uint32_t createTraceCount =
        s_createTraceCount.fetch_add(1, std::memory_order_relaxed) + 1;
    if (createTraceCount <= 300)
        LOGF_INFO("[NtCreateFile] guest path #{}: '{}' (caller LR=0x{:08X})", createTraceCount, guestPath, GET_CALLER_LR());

    // === PHASE 2: Storage Init File Tracing ===
    static int s_storageInitFileCount = 0;
    if (g_inStorageInit.load()) {
        ++s_storageInitFileCount;
        LOGF_WARNING("[STORAGE-FILE] NtCreateFile #{} path='{}' access=0x{:08X}", 
                     s_storageInitFileCount, guestPath, DesiredAccess);
    }
    // === END PHASE 2 ===

    // DesiredAccess on Xbox uses Win32-like GENERIC_READ/WRITE bits.
    std::ios::openmode mode = std::ios::binary;
    if (DesiredAccess & (GENERIC_READ | FILE_READ_DATA))
        mode |= std::ios::in;
    if (DesiredAccess & GENERIC_WRITE)
        mode |= std::ios::out;

    // Fallback: treat unknown access flags as read.
    if ((mode & (std::ios::in | std::ios::out)) == 0)
        mode |= std::ios::in;

    // === VFS-BASED FILE SERVING ===
    // Try to serve files directly from extracted directory first.
    // This bypasses the complex RPF offset-based reading that causes stream issues.
    if (VFS::IsInitialized())
    {
        auto vfsResolved = VFS::Resolve(guestPath);
        if (!vfsResolved.empty())
        {
            std::error_code ec;
            bool vfsExists = std::filesystem::exists(vfsResolved, ec);
            bool vfsIsDir = vfsExists && std::filesystem::is_directory(vfsResolved, ec);
            
            if (vfsExists && !ec)
            {
                if (vfsIsDir)
                {
                    // Return directory handle for VFS directory
                    NtDirHandle* hDir = CreateKernelObject<NtDirHandle>();
                    if (!hDir)
                    {
                        if (IoStatusBlock)
                        {
                            IoStatusBlock->Status = 0xC0000017; // STATUS_NO_MEMORY
                            IoStatusBlock->Information = 0;
                        }
                        return 0xC0000017;
                    }
                    hDir->path = vfsResolved;
                    
                    const uint32_t handleValue = GetKernelHandle(hDir);
                    g_ntDirHandles.emplace(handleValue, hDir);
                    
                    *FileHandle = handleValue;
                    if (App::s_logVfs)
                        LOGF_IMPL(Utility, "NtCreateFile", "[VFS] Directory: '{}' -> {} (handle=0x{:08X})",
                            guestPath, vfsResolved.string(), handleValue);
                    
                    if (IoStatusBlock)
                    {
                        IoStatusBlock->Status = STATUS_SUCCESS;
                        IoStatusBlock->Information = 1;
                    }
                    return STATUS_SUCCESS;
                }
                else
                {
                    // Open file directly from VFS
                    std::fstream fileStream;
                    fileStream.open(vfsResolved, mode);
                    
                    if (fileStream.is_open())
                    {
                        NtFileHandle* hFile = CreateKernelObject<NtFileHandle>();
                        if (!hFile)
                        {
                            if (IoStatusBlock)
                            {
                                IoStatusBlock->Status = 0xC0000017; // STATUS_NO_MEMORY
                                IoStatusBlock->Information = 0;
                            }
                            return 0xC0000017;
                        }
                        hFile->stream = std::move(fileStream);
                        hFile->path = vfsResolved;
                        hFile->isRpf = false;
                        
                        const uint32_t handleValue = GetKernelHandle(hFile);
                        g_ntFileHandles.emplace(handleValue, hFile);
                        
                        *FileHandle = handleValue;
                        if (App::s_logVfs)
                            LOGF_IMPL(Utility, "NtCreateFile", "[VFS] File: '{}' -> {} (handle=0x{:08X})",
                                guestPath, vfsResolved.string(), handleValue);
                        
                        if (IoStatusBlock)
                        {
                            IoStatusBlock->Status = STATUS_SUCCESS;
                            IoStatusBlock->Information = 1;
                        }
                        return STATUS_SUCCESS;
                    }
                }
            }
        }
    }
    // === END VFS-BASED FILE SERVING ===

    std::filesystem::path resolved = ResolveGuestPathBestEffort(guestPath);

    // Debug: Log resolved path for troubleshooting
    if (App::s_logVfs)
        LOGF_IMPL(Utility, "NtCreateFile", "Guest: '{}' -> Resolved: '{}' DesiredAccess=0x{:08X}", guestPath, resolved.string(), DesiredAccess);

    // Check if this is a direct .rpf file open (e.g., "game:\common.rpf")
    {
        std::string pathLower = guestPath;
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);
        if (pathLower.find(".rpf") != std::string::npos)
        {
            if (App::s_logVfs)
                LOGF_IMPL(Utility, "NtCreateFile", "RPF file access detected: '{}'", guestPath);
        }
    }

    // GTA V RPF handling: When the game tries to open a path ending with backslash
    // (like "fxl_final\" or "game:\") as if it were a file, it's trying to mount an RPF archive.
    // Since we've extracted the RPF contents, we should let the game fail gracefully here
    // so it falls back to accessing extracted files individually.
    // However, if the game opens these for directory enumeration (not file reading), we allow it.
    bool isRpfMountAttempt = !guestPath.empty() && 
                              (guestPath.back() == '\\' || guestPath.back() == '/') &&
                              (DesiredAccess & (GENERIC_READ | FILE_READ_DATA)) &&
                              !(DesiredAccess & GENERIC_WRITE);

    // Directories can't be opened with std::fstream; represent them with a dedicated handle.
    {
        std::error_code ec;
        bool exists = std::filesystem::exists(resolved, ec);
        bool isDir = exists && std::filesystem::is_directory(resolved, ec);
        if (App::s_logVfs)
            LOGF_IMPL(Utility, "NtCreateFile", "exists={} isDir={} ec={}", exists, isDir, ec ? ec.message() : "none");
        
        if (exists && isDir && !ec)
        {
            // GTA V behavior: it opens `game:\\` (and sometimes `D:\\`) as if it were a file
            // and then reads an RPF header from it as part of its packfile/disc mounting logic.
            // On our host FS `game` resolves to a directory, which makes reads fail and triggers
            // the dirty-disc loop.
            //
            // Fix: for mount-style opens on the root, map that open to real .rpf files and let
            // the title parse them using normal NtReadFile semantics.
            // For all other directories, always return a directory handle so enumeration works.
            auto isRootMountPath = [](const std::string& path) -> bool
            {
                std::string_view p(path);
                while (!p.empty() && (p.back() == '\\' || p.back() == '/'))
                    p.remove_suffix(1);

                auto ieq = [](std::string_view a, std::string_view b) -> bool
                {
                    if (a.size() != b.size())
                        return false;
                    for (size_t i = 0; i < a.size(); i++)
                    {
                        char ca = a[i];
                        char cb = b[i];
                        if (ca >= 'A' && ca <= 'Z')
                            ca = char(ca - 'A' + 'a');
                        if (cb >= 'A' && cb <= 'Z')
                            cb = char(cb - 'A' + 'a');
                        if (ca != cb)
                            return false;
                    }
                    return true;
                };

                return ieq(p, "game:") || ieq(p, "d:");
            };

            if (isRpfMountAttempt)
            {
                // GTA V opens directories with trailing backslash as if they were RPF files.
                // For game:\ root, treat it as a directory so the game can enumerate RPF files.
                // For paths like fxl_final\, these are inside RPFs - redirect to extracted content.
                
                std::string pathLower = guestPath;
                std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);
                
                // Strip trailing slashes
                while (!pathLower.empty() && (pathLower.back() == '\\' || pathLower.back() == '/'))
                    pathLower.pop_back();
                
                // For game:\ root, return as directory handle so game can enumerate files
                if (pathLower == "game:" || pathLower == "d:")
                {
                    // Return directory handle for game root
                    NtDirHandle* hDir = CreateKernelObject<NtDirHandle>();
                    hDir->path = resolved;

                    const uint32_t handleValue = GetKernelHandle(hDir);
                    g_ntDirHandles.emplace(handleValue, hDir);

                    *FileHandle = handleValue;
                    if (App::s_logVfs)
                        LOGF_IMPL(Utility, "NtCreateFile", "game:\\ root -> directory handle 0x{:08X} for {}", handleValue, resolved.string());
                    
                    if (IoStatusBlock)
                    {
                        IoStatusBlock->Status = STATUS_SUCCESS;
                        IoStatusBlock->Information = 1;
                    }
                    return STATUS_SUCCESS;
                }
                
                // For directories inside extracted game tree (shaders, data, etc.), return directory handle
                if (std::filesystem::exists(resolved) && std::filesystem::is_directory(resolved))
                {
                    NtDirHandle* hDir = CreateKernelObject<NtDirHandle>();
                    hDir->path = resolved;

                    const uint32_t handleValue = GetKernelHandle(hDir);
                    g_ntDirHandles.emplace(handleValue, hDir);

                    *FileHandle = handleValue;
                    if (App::s_logVfs)
                        LOGF_IMPL(Utility, "NtCreateFile", "Directory mount '{}' -> directory handle 0x{:08X} for {}",
                                  guestPath, handleValue, resolved.string());
                    
                    if (IoStatusBlock)
                    {
                        IoStatusBlock->Status = STATUS_SUCCESS;
                        IoStatusBlock->Information = 1;
                    }
                    return STATUS_SUCCESS;
                }
                
                // For other RPF mount attempts, return virtual empty RPF
                NtVirtFileHandle* hVirt = CreateKernelObject<NtVirtFileHandle>();
                hVirt->debugPath = resolved;
                hVirt->data = MakeEmptyRpfImage();

                const uint32_t virtHandleValue = GetKernelHandle(hVirt);
                g_ntVirtFileHandles.emplace(virtHandleValue, hVirt);

                *FileHandle = virtHandleValue;
                if (App::s_logVfs)
                    LOGF_IMPL(Utility, "NtCreateFile", "RPF mount '{}' -> virtual empty RPF (handle=0x{:08X})", guestPath, virtHandleValue);
                
                if (IoStatusBlock)
                {
                    IoStatusBlock->Status = STATUS_SUCCESS;
                    IoStatusBlock->Information = 1;
                }

                return STATUS_SUCCESS;
            }
            else
            {
                NtDirHandle* hDir = CreateKernelObject<NtDirHandle>();
                hDir->path = resolved;

                const uint32_t handleValue = GetKernelHandle(hDir);
                g_ntDirHandles.emplace(handleValue, hDir);

                *FileHandle = handleValue;
                if (App::s_logVfs)
                    LOGF_IMPL(Utility, "NtCreateFile", "Created directory handle 0x{:08X} for {}", handleValue, resolved.string());
            }
            if (IoStatusBlock)
            {
                IoStatusBlock->Status = STATUS_SUCCESS;
                IoStatusBlock->Information = 1;
            }

            return STATUS_SUCCESS;
        }
    }

    // =============================================================================
    // CreateDisposition handling for file creation (save files, etc.)
    // NT CreateDisposition values:
    //   FILE_SUPERSEDE (0)    - Replace file if exists, create if not
    //   FILE_OPEN (1)         - Open existing file only
    //   FILE_CREATE (2)       - Create new file, fail if exists
    //   FILE_OPEN_IF (3)      - Open if exists, create if not
    //   FILE_OVERWRITE (4)    - Open and truncate, fail if not exists
    //   FILE_OVERWRITE_IF (5) - Open and truncate, or create
    // =============================================================================
    constexpr uint32_t FILE_SUPERSEDE = 0;
    constexpr uint32_t FILE_OPEN = 1;
    constexpr uint32_t FILE_CREATE = 2;
    constexpr uint32_t FILE_OPEN_IF = 3;
    constexpr uint32_t FILE_OVERWRITE = 4;
    constexpr uint32_t FILE_OVERWRITE_IF = 5;
    
    std::error_code ec;
    bool fileExists = std::filesystem::exists(resolved, ec);
    std::string guestPathLower = guestPath;
    std::transform(guestPathLower.begin(), guestPathLower.end(), guestPathLower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const bool optionalMissingRead =
        !fileExists &&
        (DesiredAccess & GENERIC_WRITE) == 0 &&
        (guestPathLower.ends_with(".bik") || guestPathLower.find("/cache/") != std::string::npos ||
         guestPathLower.find("\\cache\\") != std::string::npos);
    if (optionalMissingRead)
    {
        if (App::s_logVfs)
            LOGF_IMPL(Utility, "NtCreateFile", "Optional read-only file missing: '{}'", guestPath);
        if (IoStatusBlock)
        {
            IoStatusBlock->Status = STATUS_NO_SUCH_FILE;
            IoStatusBlock->Information = 0;
        }
        *FileHandle = 0;
        return STATUS_NO_SUCH_FILE;
    }

    bool shouldCreate = false;
    bool shouldTruncate = false;
    uint32_t fileInfo = 1; // FILE_OPENED
    
    switch (CreateDisposition)
    {
        case FILE_SUPERSEDE:
            // Replace file - create new or overwrite existing
            shouldCreate = true;
            shouldTruncate = true;
            fileInfo = fileExists ? 1 : 2; // FILE_SUPERSEDED or FILE_CREATED
            break;
            
        case FILE_OPEN:
            // Open existing only - fail if not exists
            if (!fileExists)
            {
                if (App::s_logVfs)
                    LOGF_IMPL(Utility, "NtCreateFile", "FILE_OPEN failed - file does not exist: '{}'", resolved.string());
                if (IoStatusBlock)
                {
                    IoStatusBlock->Status = STATUS_NO_SUCH_FILE;
                    IoStatusBlock->Information = 0;
                }
                *FileHandle = 0;
                return STATUS_NO_SUCH_FILE;
            }
            break;
            
        case FILE_CREATE:
            // Create new - fail if exists
            if (fileExists)
            {
                if (App::s_logVfs)
                    LOGF_IMPL(Utility, "NtCreateFile", "FILE_CREATE failed - file already exists: '{}'", resolved.string());
                if (IoStatusBlock)
                {
                    IoStatusBlock->Status = STATUS_OBJECT_NAME_COLLISION;
                    IoStatusBlock->Information = 0;
                }
                *FileHandle = 0;
                return STATUS_OBJECT_NAME_COLLISION;
            }
            shouldCreate = true;
            fileInfo = 2; // FILE_CREATED
            break;
            
        case FILE_OPEN_IF:
            // Open if exists, create if not
            shouldCreate = !fileExists;
            fileInfo = fileExists ? 1 : 2; // FILE_OPENED or FILE_CREATED
            break;
            
        case FILE_OVERWRITE:
            // Open and truncate - fail if not exists
            if (!fileExists)
            {
                if (App::s_logVfs)
                    LOGF_IMPL(Utility, "NtCreateFile", "FILE_OVERWRITE failed - file does not exist: '{}'", resolved.string());
                if (IoStatusBlock)
                {
                    IoStatusBlock->Status = STATUS_NO_SUCH_FILE;
                    IoStatusBlock->Information = 0;
                }
                *FileHandle = 0;
                return STATUS_NO_SUCH_FILE;
            }
            shouldTruncate = true;
            fileInfo = 3; // FILE_OVERWRITTEN
            break;
            
        case FILE_OVERWRITE_IF:
            // Open and truncate, or create
            shouldCreate = !fileExists;
            shouldTruncate = fileExists;
            fileInfo = fileExists ? 3 : 2; // FILE_OVERWRITTEN or FILE_CREATED
            break;
            
        default:
            // Unknown disposition - try to open existing
            LOGF_WARNING("[NtCreateFile] Unknown CreateDisposition {} - treating as FILE_OPEN_IF", CreateDisposition);
            shouldCreate = !fileExists;
            break;
    }
    
    // Create parent directories if we need to create a new file
    if (shouldCreate && !fileExists)
    {
        std::filesystem::path parentDir = resolved.parent_path();
        if (!parentDir.empty() && !std::filesystem::exists(parentDir, ec))
        {
            std::filesystem::create_directories(parentDir, ec);
            if (ec)
            {
                LOGF_WARNING("[NtCreateFile] Failed to create parent directories for '{}': {}", 
                             resolved.string(), ec.message());
            }
            else
            {
                if (App::s_logVfs)
                    LOGF_IMPL(Utility, "NtCreateFile", "Created parent directories for: '{}'", resolved.string());
            }
        }
    }
    
    // Add truncate flag if needed
    if (shouldTruncate)
    {
        mode |= std::ios::trunc;
    }
    
    std::fstream stream;
    stream.open(resolved, mode);

    if (!stream.is_open())
    {
        const uint32_t status = ErrnoToNtStatus(errno);
        if (App::s_logVfs)
            LOGF_IMPL(Utility, "NtCreateFile", "FAILED to open '{}' -> '{}': errno={} status=0x{:08X} (disp={})",
                      guestPath, resolved.string(), errno, status, CreateDisposition);
        if (IoStatusBlock)
        {
            IoStatusBlock->Status = status;
            IoStatusBlock->Information = 0;
        }
        *FileHandle = 0;
        return status;
    }

    NtFileHandle* h = CreateKernelObject<NtFileHandle>();
    h->stream = std::move(stream);
    h->path = std::move(resolved);

    const uint32_t handleValue = GetKernelHandle(h);
    g_ntFileHandles.emplace(handleValue, h);

    *FileHandle = handleValue;
    if (App::s_logVfs)
        LOGF_INFO("[TRACE] NtCreateFile: '{}' (disp={}) -> handle=0x{:08X}", guestPath, CreateDisposition, handleValue);
    if (IoStatusBlock)
    {
        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = fileInfo; // FILE_OPENED, FILE_CREATED, etc.
    }

    return STATUS_SUCCESS;
}

uint32_t NtClose(uint32_t handle)
{
    // Protect against NULL and invalid handles
    if (handle == 0 || handle == GUEST_INVALID_HANDLE_VALUE)
    {
        static int s_nullCloseCount = 0;
        if (++s_nullCloseCount <= 5)
        {
            LOGF_WARNING("[NtClose] Ignored attempt to close invalid handle 0x{:08X}", handle);
        }
        return 0xC0000008;  // STATUS_INVALID_HANDLE
    }

    if (IsKernelObject(handle))
    {
        if (IsValidKernelHandle(handle))
        {
            auto* object = GetKernelObject<KernelObject>(handle);
            if (auto* thread = dynamic_cast<GuestThreadHandle*>(object);
                thread && thread->IsCurrentThread())
            {
                thread->closePending.store(true);
                LOGF_WARNING("[NtClose] Deferring close of current guest thread handle 0x{:08X}", handle);
                return STATUS_SUCCESS;
            }
        }

        // If the handle was duplicated, just decrement the duplication count. Otherwise, delete the object.
        auto it = g_handleDuplicates.find(handle);
        const bool willDestroy = (it == g_handleDuplicates.end() || it->second == 0);
        if (!willDestroy)
        {
            if (--it->second == 0)
                g_handleDuplicates.erase(it);
            return 0;
        }

        // Close NT file handles we created.
        if (auto fhIt = g_ntFileHandles.find(handle); fhIt != g_ntFileHandles.end())
        {
            if (fhIt->second)
                fhIt->second->stream.close();
            g_ntFileHandles.erase(fhIt);
        }

        if (auto dhIt = g_ntDirHandles.find(handle); dhIt != g_ntDirHandles.end())
        {
            g_ntDirHandles.erase(dhIt);
        }

        if (auto vhIt = g_ntVirtFileHandles.find(handle); vhIt != g_ntVirtFileHandles.end())
        {
            g_ntVirtFileHandles.erase(vhIt);
        }

        DestroyKernelObject(handle);

        return 0;
    }
    else
    {
        // Don't crash on unknown handles - game may close handles we don't track
        static int s_unknownCloseCount = 0;
        if (++s_unknownCloseCount <= 10)
        {
            LOGF_WARNING("[NtClose] Unrecognized kernel object handle 0x{:08X} (occurrence #{})", 
                handle, s_unknownCloseCount);
        }
        return 0;  // Return success - closing unknown handle is harmless
    }
}

uint32_t NtSetInformationFile(
    uint32_t FileHandle,
    XIO_STATUS_BLOCK* IoStatusBlock,
    void* FileInformation,
    uint32_t Length,
    uint32_t FileInformationClass)
{
    if (App::s_logVfs)
        LOGF_INFO("[TRACE] NtSetInformationFile: handle=0x{:08X} class={} len={}",
                  FileHandle, FileInformationClass, Length);

    if (FileHandle == GUEST_INVALID_HANDLE_VALUE || !IsKernelObject(FileHandle))
        return STATUS_INVALID_HANDLE;

    NtFileHandle* hFile = nullptr;
    if (auto it = g_ntFileHandles.find(FileHandle); it != g_ntFileHandles.end() && it->second && it->second->magic == kNtFileHandleMagic)
    {
        hFile = it->second;
    }

    if (hFile && FileInformation)
    {
        // FilePositionInformation = 14
        if (FileInformationClass == 14 && Length >= sizeof(uint64_t))
        {
            uint64_t newPos = ByteSwap(*reinterpret_cast<uint64_t*>(FileInformation));
            hFile->stream.clear();
            hFile->stream.seekg(newPos, std::ios::beg);
            if (App::s_logVfs)
                LOGF_INFO("[TRACE] NtSetInformationFile: Seek handle=0x{:08X} to pos={}", FileHandle, newPos);
        }
    }

    if (IoStatusBlock)
    {
        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = Length;
    }
    return STATUS_SUCCESS;
}

uint32_t FscSetCacheElementCount()
{
    return 0;
}

uint32_t FscGetCacheElementCount()
{
    return 0;
}

uint32_t XamLoaderGetLaunchDataSize()
{
    return 0;
}

uint32_t XamLoaderGetLaunchData()
{
    return 0;
}

uint32_t XamLoaderSetLaunchData()
{
    return 0;
}

// Forward declaration for PM4 scanning (defined later)
void ScanRingBufferForPM4Packets();

uint32_t NtWaitForSingleObjectEx(uint32_t Handle, uint32_t WaitMode, uint32_t Alertable, be<int64_t>* Timeout)
{
    if (Handle == GUEST_INVALID_HANDLE_VALUE)
        return 0xFFFFFFFF;
    
    // FIX: NULL handle (0x00000000) means "no object to wait on" - return success immediately
    // This unblocks the main thread after init completes when game passes NULL handles
    if (Handle == 0)
    {
        static int s_nullWaitCount = 0;
        if (++s_nullWaitCount <= 5)
        {
            LOGF_WARNING("[NtWaitEx] NULL handle wait #{} - returning STATUS_SUCCESS to unblock", s_nullWaitCount);
        }
        int64_t timeoutValue = Timeout ? Timeout->get() : 0;
        if (timeoutValue != 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return STATUS_SUCCESS;  // Return success to let caller proceed
    }

    // FIX: Support waiting on file handles for async I/O completion
    // Xbox async I/O pattern: NtWriteFile without event, then wait on file handle itself
    // Since our NtWriteFile/NtReadFile are SYNCHRONOUS, I/O is already complete
    // Return STATUS_SUCCESS immediately - this is proper host API implementation
    {
        auto it = g_ntFileHandles.find(Handle);
        if (it != g_ntFileHandles.end() && it->second && it->second->magic == kNtFileHandleMagic)
        {
            static int s_fileWaitCount = 0;
            if (++s_fileWaitCount <= 10 || s_fileWaitCount % 100 == 0)
            {
                uint32_t callerLR = GET_CALLER_LR();
                LOGF_WARNING("[NtWaitEx] FILE HANDLE wait #{} handle=0x{:08X} caller=0x{:08X} - sync I/O complete, returning SUCCESS",
                            s_fileWaitCount, Handle, callerLR);
            }
            return STATUS_SUCCESS;  // I/O already completed synchronously
        }
    }

    uint32_t timeout = GuestTimeoutToMilliseconds(Timeout);
    
    // Trace all wait calls to understand blocking
    static int s_waitCount = 0;
    ++s_waitCount;
    PPCContext* ppcContext = GetPPCContext();
    const auto stackTrace = CaptureGuestStackTrace();
    uint32_t callerLR = GET_CALLER_LR();
    ScopedWaitTracker waitTracker("NtWaitForSingleObjectEx", Handle, timeout, callerLR);
    bool isMainThread = (g_kernelMainThreadIdSet && std::this_thread::get_id() == g_kernelMainThreadId);
    
    if (s_waitCount <= 30 || s_waitCount % 500 == 0)
    {
        std::stringstream ssTid, ssMain;
        ssTid << std::this_thread::get_id();
        ssMain << g_kernelMainThreadId;
        LOGF_WARNING("[NtWaitEx] #{} handle=0x{:08X} timeout={} isMain={} tid={} mainTid={} caller=0x{:08X} sp=0x{:08X} tls=0x{:08X} stack={:08X},{:08X},{:08X},{:08X}",
                  s_waitCount, Handle, timeout, isMainThread, ssTid.str(), ssMain.str(),
                  callerLR, ppcContext ? ppcContext->r1.u32 : 0u, ppcContext ? ppcContext->r13.u32 : 0u,
                  stackTrace[0], stackTrace[1], stackTrace[2], stackTrace[3]);
    }


    // VBlank removed - following UnleashedRecomp pattern (no force-firing)
    // Game progresses naturally without interrupt-driven timing

    KernelObject* obj = nullptr;
    uint8_t objType = 0xFF;

    if (IsValidKernelHandle(Handle))
    {
        obj = GetKernelObject(Handle);
    }
    else if (Handle >= 0x10000 && Handle <= 0xF0000000 && g_memory.base != nullptr)
    {
        XDISPATCHER_HEADER* header = reinterpret_cast<XDISPATCHER_HEADER*>(g_memory.Translate(Handle));
        objType = header->Type;
        
        // Type 5 = Semaphore, Type 0/1 = Event (synchronization/notification)
        if (objType == 5)
        {
            obj = QueryKernelObject<Semaphore>(*header);
            if (s_waitCount <= 30 || (Handle >= 0xEB2D0000 && Handle < 0xEB2E0000))
            {
                LOGF_WARNING("[NtWaitEx] #{} handle=0x{:08X} is guest XKSEMAPHORE, queried kernel obj={}", 
                            s_waitCount, Handle, obj ? "OK" : "NULL");
            }
        }
        else if (objType == 0 || objType == 1)
        {
            obj = QueryKernelObject<Event>(*header);
            if (s_waitCount <= 30)
            {
                LOGF_WARNING("[NtWaitEx] #{} handle=0x{:08X} is guest XKEVENT type={}, queried kernel obj={}", 
                            s_waitCount, Handle, objType, obj ? "OK" : "NULL");
            }
        }
        else if (header->WaitListHead.Flink == OBJECT_SIGNATURE)
        {
            uint32_t blink = header->WaitListHead.Blink.get();
            if (IsValidKernelHandle(blink))
            {
                obj = GetKernelObject(blink);
            }
        }
    }

    if (!IsValidHostVtable(obj))
    {
        static int s_invalidWaitCount = 0;
        if (++s_invalidWaitCount <= 20 || s_invalidWaitCount % 10000 == 0)
        {
            LOGF_WARNING("[NtWaitEx] #{} handle=0x{:08X} (type={}) - no valid kernel object found! (occurrence #{})", 
                        s_waitCount, Handle, objType, s_invalidWaitCount);
        }
        if (timeout != 0) {
            uint32_t sleepMs = isMainThread ? 1 : std::min(timeout, 50u);
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        } else if (!isMainThread) {
            std::this_thread::yield();
        }
        return (ShouldFailOpenWait() && isMainThread) ? STATUS_SUCCESS : STATUS_TIMEOUT;
    }

    // KERNEL POLICY: On the main thread, infinite waits must never hang the process!
    if (isMainThread) {
        static uint32_t s_lastMainHandle = 0;
        static int s_consecutiveTimeouts = 0;
        uint32_t maxWait = (timeout == INFINITE) ? 50 : std::min(timeout, 50u);
        uint32_t result = obj->Wait(maxWait);
        if (result == STATUS_TIMEOUT) {
            if (Handle == s_lastMainHandle) {
                ++s_consecutiveTimeouts;
            } else {
                s_lastMainHandle = Handle;
                s_consecutiveTimeouts = 1;
            }
            if (s_consecutiveTimeouts >= 2) {
                static int s_failOpenObj = 0;
                if (++s_failOpenObj <= 50 || s_failOpenObj % 100 == 0) {
                    LOGF_WARNING("[FAIL-OPEN] NtWaitEx #{} (timeout={}ms, consecutive={}) -> SUCCESS handle=0x{:08X} caller=0x{:08X}", 
                                s_failOpenObj, timeout, s_consecutiveTimeouts, Handle, callerLR);
                }
                return STATUS_SUCCESS;
            }
        } else {
            s_consecutiveTimeouts = 0;
        }
        return result;
    }


    return obj->Wait(timeout);
}

uint32_t NtWriteFile(
    uint32_t FileHandle,
    uint32_t Event,  // Signal on completion for async I/O 
    uint32_t /*ApcRoutine*/,
    uint32_t /*ApcContext*/,
    XIO_STATUS_BLOCK* IoStatusBlock,
    const void* Buffer,
    uint32_t Length,
    be<int64_t>* ByteOffset)
{
    static int s_writeCount = 0;
    ++s_writeCount;
    uint32_t callerLR = GET_CALLER_LR();
    
    // Trace all NtWriteFile calls to debug blocking
    if (s_writeCount <= 20 || s_writeCount % 100 == 0) {
        LOGF_WARNING("[NtWriteFile] #{} handle=0x{:08X} event=0x{:08X} len={} caller=0x{:08X}",
                    s_writeCount, FileHandle, Event, Length, callerLR);
    }
    
    if (FileHandle == GUEST_INVALID_HANDLE_VALUE || !IsKernelObject(FileHandle))
    {
        LOGF_IMPL(Utility, "NtWriteFile", "INVALID handle 0x{:08X}", FileHandle);
        return STATUS_INVALID_HANDLE;
    }

    auto it = g_ntFileHandles.find(FileHandle);
    if (it == g_ntFileHandles.end() || !it->second || it->second->magic != kNtFileHandleMagic)
    {
        LOGF_IMPL(Utility, "NtWriteFile", "Not a file handle 0x{:08X}", FileHandle);
        return STATUS_INVALID_HANDLE;
    }

    NtFileHandle* hFile = it->second;
    if (!Buffer)
        return STATUS_INVALID_PARAMETER;

    if (ByteOffset != nullptr)
    {
        const int64_t offset = ByteOffset->get();
        hFile->stream.clear();
        hFile->stream.seekp(offset, std::ios::beg);
        if (hFile->stream.bad())
            return STATUS_FAIL_CHECK;
    }

    hFile->stream.write(reinterpret_cast<const char*>(Buffer), Length);
    const bool ok = !hFile->stream.bad();

    if (IoStatusBlock)
    {
        IoStatusBlock->Status = ok ? STATUS_SUCCESS : STATUS_FAIL_CHECK;
        IoStatusBlock->Information = ok ? Length : 0;
    }


    // Signal completion event for async I/O
    if (Event != 0 && ok) {
        static int s_eventSignalCount = 0;
        ++s_eventSignalCount;
        if (s_eventSignalCount <= 20 || s_eventSignalCount % 100 == 0) {
            LOGF_WARNING("[ASYNC COMPLETE] NtWriteFile signaling event 0x{:08X} (count={})", Event, s_eventSignalCount);
        }
    }

    return ok ? STATUS_SUCCESS : STATUS_FAIL_CHECK;
}

void vsprintf_x()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t ExGetXConfigSetting(uint16_t Category, uint16_t Setting, void* Buffer, uint16_t SizeOfBuffer, be<uint32_t>* RequiredSize)
{
    uint32_t data[4]{};

    switch (Category)
    {
        // XCONFIG_SECURED_CATEGORY
        case 0x0002:
        {
            switch (Setting)
            {
                // XCONFIG_SECURED_AV_REGION
                case 0x0002:
                    data[0] = ByteSwap(0x00001000); // USA/Canada
                    break;

                default:
                    return 1;
            }
        }

        case 0x0003:
        {
            switch (Setting)
            {
                case 0x0001: // XCONFIG_USER_TIME_ZONE_BIAS
                case 0x0002: // XCONFIG_USER_TIME_ZONE_STD_NAME
                case 0x0003: // XCONFIG_USER_TIME_ZONE_DLT_NAME
                case 0x0004: // XCONFIG_USER_TIME_ZONE_STD_DATE
                case 0x0005: // XCONFIG_USER_TIME_ZONE_DLT_DATE
                case 0x0006: // XCONFIG_USER_TIME_ZONE_STD_BIAS
                case 0x0007: // XCONFIG_USER_TIME_ZONE_DLT_BIAS
                    data[0] = 0;
                    break;

                // XCONFIG_USER_LANGUAGE
                case 0x0009:
                    data[0] = ByteSwap((uint32_t)Config::Language.Value);
                    break;

                // XCONFIG_USER_VIDEO_FLAGS
                case 0x000A:
                    data[0] = ByteSwap(0x00040000);
                    break;

                // XCONFIG_USER_RETAIL_FLAGS
                case 0x000C:
                    data[0] = ByteSwap(1);
                    break;

                // XCONFIG_USER_COUNTRY
                case 0x000E:
                    data[0] = ByteSwap(103);
                    break;

                default:
                    return 1;
            }
        }
    }

    *RequiredSize = 4;
    memcpy(Buffer, data, std::min((size_t)SizeOfBuffer, sizeof(data)));

    return 0;
}

uint32_t NtQueryVirtualMemory(uint32_t pAddress, uint32_t pInfo, uint32_t pSize, uint32_t pClass, uint32_t pReturnLength)
{
    LOGF_INFO("[TRACE] NtQueryVirtualMemory: addr=0x{:08X} info=0x{:08X}", pAddress, pInfo);
    return 0; // STATUS_SUCCESS
}

#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER 0xC000000D
#endif

#ifndef STATUS_NO_MEMORY
#define STATUS_NO_MEMORY 0xC0000017
#endif

uint32_t MmQueryStatistics(uint32_t pStats)
{
    if (pStats != 0)
    {
        uint8_t* ptr = static_cast<uint8_t*>(g_memory.Translate(pStats));
        if (ptr)
        {
            uint32_t length = ByteSwap(*reinterpret_cast<uint32_t*>(ptr));
            if (length == 0 || length > 104) length = 104;
            std::memset(ptr, 0, length);
            *reinterpret_cast<uint32_t*>(ptr + 0) = ByteSwap(length);
            *reinterpret_cast<uint32_t*>(ptr + 4) = ByteSwap(131072u); // 512 MB total pages
            *reinterpret_cast<uint32_t*>(ptr + 8) = ByteSwap(100000u); // Available
            *reinterpret_cast<uint32_t*>(ptr + 12) = ByteSwap(100000u); // Title pool
            *reinterpret_cast<uint32_t*>(ptr + 16) = ByteSwap(20000u);  // System pool
            *reinterpret_cast<uint32_t*>(ptr + 20) = ByteSwap(1000u);   // Reserved
            LOGF_INFO("[TRACE] MmQueryStatistics populated at 0x{:08X}", pStats);
        }
    }
    return 0; // STATUS_SUCCESS
}

// Track all created Event handles so VdSwap can signal them to unblock workers
static std::mutex g_eventTrackMutex;
static std::vector<uint32_t> g_trackedEventHandles;

uint32_t NtCreateEvent(be<uint32_t>* handle, void* objAttributes, uint32_t eventType, uint32_t initialState)
{
    static int s_count = 0;
    ++s_count;
    uint32_t callerLR = GET_CALLER_LR();
    
    // Log BEFORE creation to detect if CreateKernelObject crashes
    if (s_count <= 30)
    {
        LOGF_IMPL(Utility, "NtCreateEvent", "#{} STARTING type={} initial={}",
                  s_count, eventType, initialState);
    }
    
    Event* evt = CreateKernelObject<Event>(!eventType, !!initialState);
    uint32_t h = GetKernelHandle(evt);
    *handle = h;
    // eventType: 0 = NotificationEvent (manual reset), 1 = SynchronizationEvent (auto reset)
    
    // Log AFTER creation to confirm success
    if (s_count <= 30)
    {
        LOGF_INFO("[TRACE] NtCreateEvent: #{} handle=0x{:08X} type={} initial={}",
                  s_count, h, eventType, initialState);
    }
    
    return 0;
}

uint32_t XexCheckExecutablePrivilege()
{
    return 0;
}

void DbgPrint(const char* format)
{
    if (format) {
        LOGF_WARNING("[DbgPrint] {}", format);
    }
}

void __C_specific_handler_x()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t RtlNtStatusToDosError(uint32_t Status)
{
    // See https://github.com/wine-mirror/wine/blob/master/dlls/ntdll/error.c#L47-L64
    if (Status == 0 || (Status & 0x20000000) != 0)
        return Status;

    if ((Status & 0xF0000000) == 0xD0000000)
        Status &= ~0x10000000;

    const uint32_t hi = (Status >> 16) & 0xFFFF;
    if (hi == 0x8007 || hi == 0xC001 || hi == 0xC007)
        return Status & 0xFFFF;

    switch (Status)
    {
    case 0xC000000F: // STATUS_NO_SUCH_FILE
        return ERROR_FILE_NOT_FOUND;
    case uint32_t(STATUS_NOT_IMPLEMENTED):
        return ERROR_CALL_NOT_IMPLEMENTED;
    case uint32_t(STATUS_SEMAPHORE_LIMIT_EXCEEDED):
        return ERROR_TOO_MANY_POSTS;
    case uint32_t(STATUS_OBJECT_NAME_NOT_FOUND):
        return ERROR_FILE_NOT_FOUND;
    case uint32_t(STATUS_INVALID_PARAMETER):
        return ERROR_INVALID_PARAMETER;
    case uint32_t(STATUS_INVALID_HANDLE):
        return ERROR_INVALID_HANDLE;
    case uint32_t(STATUS_END_OF_FILE):
        return ERROR_HANDLE_EOF;
    case uint32_t(STATUS_NO_MORE_FILES):
        return ERROR_NO_MORE_FILES;
    case uint32_t(STATUS_ACCESS_DENIED):
        return ERROR_ACCESS_DENIED;
    case uint32_t(STATUS_OBJECT_NAME_INVALID):
        return ERROR_INVALID_NAME;
    case uint32_t(STATUS_OBJECT_PATH_NOT_FOUND):
        return ERROR_PATH_NOT_FOUND;
    case uint32_t(STATUS_OBJECT_NAME_COLLISION):
        return ERROR_ALREADY_EXISTS;
    default:
        LOGF_WARNING("Unimplemented NtStatus translation: {:#08x}", Status);
        return Status;
    }
}

void XexGetProcedureAddress()
{
    LOG_UTILITY("!!! STUB !!!");
}

void XexGetModuleSection()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t RtlUnicodeToMultiByteN(char* MultiByteString, uint32_t MaxBytesInMultiByteString, be<uint32_t>* BytesInMultiByteString, const be<uint16_t>* UnicodeString, uint32_t BytesInUnicodeString)
{
    const auto reqSize = BytesInUnicodeString / sizeof(uint16_t);

    if (BytesInMultiByteString)
        *BytesInMultiByteString = reqSize;

    if (reqSize > MaxBytesInMultiByteString)
        return STATUS_FAIL_CHECK;

    for (size_t i = 0; i < reqSize; i++)
    {
        const auto c = UnicodeString[i].get();

        MultiByteString[i] = c < 256 ? c : '?';
    }

    return STATUS_SUCCESS;
}

uint32_t KeDelayExecutionThread(uint32_t WaitMode, bool Alertable, be<int64_t>* Timeout)
{
    // Pump SDL events to keep window responsive
    PumpSdlEventsIfNeeded();
    
    // We don't do async file reads.
    if (Alertable)
        return STATUS_USER_APC;

    uint32_t timeout = GuestTimeoutToMilliseconds(Timeout);
    uint32_t callerLR = GET_CALLER_LR();
    ScopedWaitTracker waitTracker("KeDelayExecutionThread", 0, timeout, callerLR);
    static std::atomic<int> s_delayCount{0};
    if (s_delayCount++ < 50 || s_delayCount % 100 == 0) {
        LOGF_INFO("[TRACE] KeDelayExecutionThread(timeout={}ms, main={}, callerLR=0x{:08X})",
                  timeout, IsMainThread(), callerLR);
    }

    if (timeout == 0)
    {
        std::this_thread::yield();
    }
    else if (IsMainThread())
    {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
        while (std::chrono::steady_clock::now() < deadline)
        {
            PumpSdlEventsIfNeeded();
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
            if (remaining <= 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(std::min<int64_t>(remaining, 10)));
        }
    }
    else
    {
#ifdef _WIN32
        Sleep(timeout);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
#endif
    }

    return STATUS_SUCCESS;
}

void ExFreePool(uint32_t guestAddress)
{
    const uint32_t lr = GET_CALLER_LR();
    if (guestAddress != 0) {
        LOGF_INFO("[ExFreePool] guest=0x{:08X} caller=0x{:08X}", guestAddress, lr);
        g_userHeap.Free(g_memory.Translate(guestAddress));
    }
}

uint32_t NtQueryInformationFile(
    uint32_t FileHandle,
    XIO_STATUS_BLOCK* IoStatusBlock,
    void* FileInformation,
    uint32_t Length,
    uint32_t FileInformationClass)
{
    if (FileHandle == GUEST_INVALID_HANDLE_VALUE || !IsKernelObject(FileHandle))
        return STATUS_INVALID_HANDLE;

    std::filesystem::path path;
    NtFileHandle* hFile = nullptr;
    NtVirtFileHandle* hVirt = nullptr;
    if (auto it = g_ntFileHandles.find(FileHandle); it != g_ntFileHandles.end() && it->second && it->second->magic == kNtFileHandleMagic)
    {
        hFile = it->second;
        path = hFile->path;
    }
    else if (auto it = g_ntVirtFileHandles.find(FileHandle); it != g_ntVirtFileHandles.end() && it->second && it->second->magic == kNtVirtFileHandleMagic)
    {
        hVirt = it->second;
        path = hVirt->debugPath;
    }
    else if (auto it = g_ntDirHandles.find(FileHandle); it != g_ntDirHandles.end() && it->second && it->second->magic == kNtDirHandleMagic)
    {
        path = it->second->path;
    }
    else
    {
        return STATUS_INVALID_HANDLE;
    }

    if (!IoStatusBlock || !FileInformation)
        return STATUS_INVALID_PARAMETER;

    std::error_code ec;
    bool isDir = false;
    uint64_t fileSize = 0;
    uint32_t attrs = 0;

    if (hVirt)
    {
        // In-memory file: pretend it's a normal file with known size.
        isDir = false;
        fileSize = hVirt->data.size();
        attrs = ByteSwap(FILE_ATTRIBUTE_NORMAL);
    }
    else
    {
        const bool exists = std::filesystem::exists(path, ec);
        if (!exists || ec)
        {
            IoStatusBlock->Status = STATUS_NO_SUCH_FILE;
            IoStatusBlock->Information = 0;
            return STATUS_NO_SUCH_FILE;
        }

        isDir = std::filesystem::is_directory(path, ec);
        if (ec)
        {
            IoStatusBlock->Status = STATUS_FAIL_CHECK;
            IoStatusBlock->Information = 0;
            return STATUS_FAIL_CHECK;
        }

        if (!isDir)
        {
            fileSize = std::filesystem::file_size(path, ec);
            if (ec)
                fileSize = 0;
        }

        attrs = GetFileAttributesBestEffort(path);
    }

    const uint64_t allocationSize = RoundUpToPage(fileSize);

    switch (FileInformationClass)
    {
    case FileBasicInformation:
    {
        if (Length < sizeof(XFILE_BASIC_INFORMATION))
            return STATUS_INFO_LENGTH_MISMATCH;

        auto* info = reinterpret_cast<XFILE_BASIC_INFORMATION*>(FileInformation);
        info->CreationTime = 0;
        info->LastAccessTime = 0;
        info->LastWriteTime = 0;
        info->ChangeTime = 0;
        info->FileAttributes = attrs;
        info->Reserved = 0;

        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = sizeof(XFILE_BASIC_INFORMATION);
        return STATUS_SUCCESS;
    }
    case FileStandardInformation:
    {
        if (Length < sizeof(XFILE_STANDARD_INFORMATION))
            return STATUS_INFO_LENGTH_MISMATCH;

        auto* info = reinterpret_cast<XFILE_STANDARD_INFORMATION*>(FileInformation);
        info->AllocationSize = static_cast<int64_t>(allocationSize);
        info->EndOfFile = static_cast<int64_t>(fileSize);
        info->NumberOfLinks = 1;
        info->DeletePending = 0;
        info->Directory = isDir ? 1 : 0;
        info->Reserved = 0;

        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = sizeof(XFILE_STANDARD_INFORMATION);
        return STATUS_SUCCESS;
    }
    case FilePositionInformation:
    {
        if (Length < sizeof(XFILE_POSITION_INFORMATION))
            return STATUS_INFO_LENGTH_MISMATCH;

        auto* info = reinterpret_cast<XFILE_POSITION_INFORMATION*>(FileInformation);
        if (hFile)
        {
            const auto pos = hFile->stream.tellg();
            if (pos < 0)
                info->CurrentByteOffset = 0;
            else
                info->CurrentByteOffset = static_cast<int64_t>(pos);
        }
        else
        {
            info->CurrentByteOffset = 0;
        }

        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = sizeof(XFILE_POSITION_INFORMATION);
        return STATUS_SUCCESS;
    }
    case FileEndOfFileInformation:
    {
        if (Length < sizeof(XFILE_END_OF_FILE_INFORMATION))
            return STATUS_INFO_LENGTH_MISMATCH;

        auto* info = reinterpret_cast<XFILE_END_OF_FILE_INFORMATION*>(FileInformation);
        info->EndOfFile = static_cast<int64_t>(fileSize);

        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = sizeof(XFILE_END_OF_FILE_INFORMATION);
        return STATUS_SUCCESS;
    }
    case FileNetworkOpenInformation:
    {
        if (Length < sizeof(XFILE_NETWORK_OPEN_INFORMATION))
            return STATUS_INFO_LENGTH_MISMATCH;

        auto* info = reinterpret_cast<XFILE_NETWORK_OPEN_INFORMATION*>(FileInformation);
        info->CreationTime = 0;
        info->LastAccessTime = 0;
        info->LastWriteTime = 0;
        info->ChangeTime = 0;
        info->AllocationSize = static_cast<int64_t>(allocationSize);
        info->EndOfFile = static_cast<int64_t>(fileSize);
        info->FileAttributes = attrs;
        info->Reserved = 0;

        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = sizeof(XFILE_NETWORK_OPEN_INFORMATION);
        return STATUS_SUCCESS;
    }
    case 26: // Xbox 360 specific - appears to query file size/info
    {
        // Xbox 360 games may use different class numbers
        // Class 26 appears to be used for file size queries
        // Return file size information in a generic format
        if (Length >= 8)
        {
            // Return file size as first 8 bytes (common pattern)
            auto* sizePtr = reinterpret_cast<int64_t*>(FileInformation);
            *sizePtr = static_cast<int64_t>(fileSize);
            if (Length > 8)
                memset(reinterpret_cast<uint8_t*>(FileInformation) + 8, 0, Length - 8);
            
            LOGF_IMPL(Utility, "NtQueryInformationFile", "Class 26 (Xbox size query) for '{}': returning size={}", 
                      path.filename().string(), fileSize);
        }
        else if (Length > 0)
        {
            memset(FileInformation, 0, Length);
        }
        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = Length;
        return STATUS_SUCCESS;
    }
    default:
        // For unhandled classes, try to return success with zeroed data
        // This helps games that query for optional information
        if (Length > 0) {
            memset(FileInformation, 0, Length);
        }
        LOGF_WARNING("NtQueryInformationFile: unhandled class {} for '{}' - returning success with zeroed data", 
                     FileInformationClass, path.filename().string());
        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = Length;
        return STATUS_SUCCESS;
    }
}

uint32_t NtQueryVolumeInformationFile(
    uint32_t FileHandle,
    XIO_STATUS_BLOCK* IoStatusBlock,
    void* FsInformation,
    uint32_t Length,
    uint32_t FsInformationClass)
{
    if (FileHandle == GUEST_INVALID_HANDLE_VALUE || !IsKernelObject(FileHandle))
        return STATUS_INVALID_HANDLE;

    // Accept both file handles and directory handles for volume information.
    std::filesystem::path path;
    if (auto it = g_ntFileHandles.find(FileHandle); it != g_ntFileHandles.end() && it->second && it->second->magic == kNtFileHandleMagic)
    {
        path = it->second->path;
    }
    else if (auto it = g_ntVirtFileHandles.find(FileHandle); it != g_ntVirtFileHandles.end() && it->second && it->second->magic == kNtVirtFileHandleMagic)
    {
        path = it->second->debugPath;
    }
    else if (auto it = g_ntDirHandles.find(FileHandle); it != g_ntDirHandles.end() && it->second && it->second->magic == kNtDirHandleMagic)
    {
        path = it->second->path;
    }
    else
    {
        return STATUS_INVALID_HANDLE;
    }

    if (!IoStatusBlock || !FsInformation)
        return STATUS_INVALID_PARAMETER;

    switch (FsInformationClass)
    {
    case FileFsDeviceInformation:
    {
        if (Length < sizeof(XFILE_FS_DEVICE_INFORMATION))
            return STATUS_INFO_LENGTH_MISMATCH;

        auto* info = reinterpret_cast<XFILE_FS_DEVICE_INFORMATION*>(FsInformation);
        info->DeviceType = 0x00000007; // FILE_DEVICE_DISK
        info->Characteristics = 0;

        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = sizeof(XFILE_FS_DEVICE_INFORMATION);
        return STATUS_SUCCESS;
    }
    case FileFsSizeInformation:
    {
        if (Length < sizeof(XFILE_FS_SIZE_INFORMATION))
            return STATUS_INFO_LENGTH_MISMATCH;

        auto* info = reinterpret_cast<XFILE_FS_SIZE_INFORMATION*>(FsInformation);
        // Very rough defaults: 512-byte sectors, 8 sectors per allocation unit (4KB).
        info->BytesPerSector = 512;
        info->SectorsPerAllocationUnit = 8;
        info->TotalAllocationUnits = 0x100000;
        info->AvailableAllocationUnits = 0x080000;

        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = sizeof(XFILE_FS_SIZE_INFORMATION);
        return STATUS_SUCCESS;
    }
    case FileFsAttributeInformation:
    {
        // Return a fixed buffer with a short filesystem name.
        constexpr uint16_t kName[] = { 'F', 'A', 'T', 'X' };
        constexpr uint32_t kNameBytes = sizeof(kName);
        constexpr uint32_t kFixedSize = sizeof(XFILE_FS_ATTRIBUTE_INFORMATION_FIXED);

        if (Length < kFixedSize)
            return STATUS_INFO_LENGTH_MISMATCH;

        auto* info = reinterpret_cast<XFILE_FS_ATTRIBUTE_INFORMATION_FIXED*>(FsInformation);
        info->FileSystemAttributes = 0;
        info->MaximumComponentNameLength = 255;
        info->FileSystemNameLength = kNameBytes;
        memset(info->FileSystemName, 0, sizeof(info->FileSystemName));
        for (size_t i = 0; i < (sizeof(kName) / sizeof(kName[0])); ++i)
            info->FileSystemName[i] = kName[i];

        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = kFixedSize;
        return STATUS_SUCCESS;
    }
    default:
        LOGF_WARNING("NtQueryVolumeInformationFile: unhandled class {}", FsInformationClass);
        IoStatusBlock->Status = STATUS_INVALID_PARAMETER;
        IoStatusBlock->Information = 0;
        return STATUS_INVALID_PARAMETER;
    }
}

uint32_t NtQueryDirectoryFile(
    uint32_t FileHandle,
    uint32_t Event,  // Signal on completion for async I/O
    uint32_t /*ApcRoutine*/,
    uint32_t /*ApcContext*/,
    XIO_STATUS_BLOCK* IoStatusBlock,
    void* FileInformation,
    uint32_t Length,
    uint32_t FileInformationClass,
    uint32_t ReturnSingleEntry,
    XANSI_STRING* FileName,
    uint32_t RestartScan)
{
    LOGF_IMPL(Utility, "NtQueryDirectoryFile", "Handle=0x{:08X} Class={} RestartScan={}", FileHandle, FileInformationClass, RestartScan);
    if (FileHandle == GUEST_INVALID_HANDLE_VALUE || !IsKernelObject(FileHandle))
    {
        LOG_IMPL(Utility, "NtQueryDirectoryFile", "INVALID_HANDLE_VALUE or not kernel object");
        return STATUS_INVALID_HANDLE;
    }

    auto it = g_ntDirHandles.find(FileHandle);
    if (it == g_ntDirHandles.end() || !it->second || it->second->magic != kNtDirHandleMagic)
    {
        LOGF_IMPL(Utility, "NtQueryDirectoryFile", "Handle 0x{:08X} not in g_ntDirHandles (found={}, hasSecond={}, magic=0x{:08X})",
            FileHandle,
            it != g_ntDirHandles.end(), it != g_ntDirHandles.end() && it->second != nullptr,
            (it != g_ntDirHandles.end() && it->second) ? it->second->magic : 0);
        return STATUS_INVALID_HANDLE;
    }
    LOGF_IMPL(Utility, "NtQueryDirectoryFile", "Dir path: '{}'", it->second->path.string());

    NtDirHandle* hDir = it->second;
    if (!IoStatusBlock || !FileInformation || Length == 0)
        return STATUS_INVALID_PARAMETER;

    if (RestartScan)
        hDir->cursor = 0;

    if (!hDir->initialized)
    {
        std::error_code ec;
        for (auto iter = std::filesystem::directory_iterator(hDir->path, ec); !ec && iter != std::filesystem::directory_iterator(); ++iter)
            hDir->entries.emplace_back(*iter);

        // Stable, deterministic ordering helps with repeatability.
        std::sort(hDir->entries.begin(), hDir->entries.end(), [](const auto& a, const auto& b) {
            return a.path().filename().string() < b.path().filename().string();
        });

        hDir->initialized = true;
        LOGF_IMPL(Utility, "NtQueryDirectoryFile", "Enumerated {} files in '{}'", hDir->entries.size(), hDir->path.string());
    }

    std::string filterName;
    if (FileName && FileName->Buffer.get() && FileName->Length.get() != 0)
    {
        const char* buf = FileName->Buffer.get();
        filterName.assign(buf, buf + FileName->Length.get());
    }

    const auto writeUtf16Be = [](be<uint16_t>* out, const std::string& s) {
        for (size_t i = 0; i < s.size(); ++i)
            out[i] = static_cast<uint16_t>(static_cast<unsigned char>(s[i]));
    };

    uint8_t* out = reinterpret_cast<uint8_t*>(FileInformation);
    uint32_t remaining = Length;
    uint32_t written = 0;

    // Track the prior entry so we can fill in its NextEntryOffset.
    be<uint32_t>* prevNextEntryOffset = nullptr;
    uint32_t prevEntrySize = 0;

    const bool single = (ReturnSingleEntry != 0);

    while (hDir->cursor < hDir->entries.size())
    {
        const auto& entry = hDir->entries[hDir->cursor];
        const std::string name = entry.path().filename().string();

        if (!filterName.empty() && name != filterName)
        {
            ++hDir->cursor;
            continue;
        }

        std::error_code ec;
        const std::filesystem::path entryPath = entry.path();
        const bool isDir = std::filesystem::is_directory(entryPath, ec);
        uint64_t fileSize = 0;
        if (!isDir)
        {
            fileSize = std::filesystem::file_size(entryPath, ec);
            if (ec)
                fileSize = 0;
        }

        const uint64_t allocationSize = RoundUpToPage(fileSize);
        const uint32_t attrs = GetFileAttributesBestEffort(entryPath);

        const uint32_t nameBytes = static_cast<uint32_t>(name.size() * sizeof(uint16_t));

        uint32_t fixedSize = 0;
        switch (FileInformationClass)
        {
        case FileDirectoryInformation:
            fixedSize = sizeof(XFILE_DIRECTORY_INFORMATION_FIXED);
            break;
        case FileFullDirectoryInformation:
            fixedSize = sizeof(XFILE_FULL_DIR_INFORMATION_FIXED);
            break;
        case FileBothDirectoryInformation:
            fixedSize = sizeof(XFILE_BOTH_DIR_INFORMATION_FIXED);
            break;
        default:
            IoStatusBlock->Status = STATUS_INVALID_PARAMETER;
            IoStatusBlock->Information = 0;
            return STATUS_INVALID_PARAMETER;
        }

        const uint32_t entrySizeUnaligned = fixedSize + nameBytes;
        const uint32_t entrySize = AlignUp(entrySizeUnaligned, 8);

        if (entrySize > remaining)
        {
            if (written == 0)
            {
                IoStatusBlock->Status = STATUS_BUFFER_OVERFLOW;
                IoStatusBlock->Information = 0;
                return STATUS_BUFFER_OVERFLOW;
            }
            break;
        }

        // Update the previous entry now that we know we are emitting a new one.
        if (prevNextEntryOffset)
            *prevNextEntryOffset = prevEntrySize;

        uint8_t* entryBase = out + written;
        memset(entryBase, 0, entrySize);

        uint8_t* nameOut = nullptr;
        switch (FileInformationClass)
        {
        case FileDirectoryInformation:
        {
            auto* info = reinterpret_cast<XFILE_DIRECTORY_INFORMATION_FIXED*>(entryBase);
            info->NextEntryOffset = 0;
            info->FileIndex = 0;
            info->CreationTime = 0;
            info->LastAccessTime = 0;
            info->LastWriteTime = 0;
            info->ChangeTime = 0;
            info->EndOfFile = static_cast<int64_t>(fileSize);
            info->AllocationSize = static_cast<int64_t>(allocationSize);
            info->FileAttributes = attrs;
            info->FileNameLength = nameBytes;
            nameOut = entryBase + sizeof(XFILE_DIRECTORY_INFORMATION_FIXED);
            prevNextEntryOffset = &info->NextEntryOffset;
            break;
        }
        case FileFullDirectoryInformation:
        {
            auto* info = reinterpret_cast<XFILE_FULL_DIR_INFORMATION_FIXED*>(entryBase);
            info->NextEntryOffset = 0;
            info->FileIndex = 0;
            info->CreationTime = 0;
            info->LastAccessTime = 0;
            info->LastWriteTime = 0;
            info->ChangeTime = 0;
            info->EndOfFile = static_cast<int64_t>(fileSize);
            info->AllocationSize = static_cast<int64_t>(allocationSize);
            info->FileAttributes = attrs;
            info->FileNameLength = nameBytes;
            info->EaSize = 0;
            nameOut = entryBase + sizeof(XFILE_FULL_DIR_INFORMATION_FIXED);
            prevNextEntryOffset = &info->NextEntryOffset;
            break;
        }
        case FileBothDirectoryInformation:
        {
            auto* info = reinterpret_cast<XFILE_BOTH_DIR_INFORMATION_FIXED*>(entryBase);
            info->NextEntryOffset = 0;
            info->FileIndex = 0;
            info->CreationTime = 0;
            info->LastAccessTime = 0;
            info->LastWriteTime = 0;
            info->ChangeTime = 0;
            info->EndOfFile = static_cast<int64_t>(fileSize);
            info->AllocationSize = static_cast<int64_t>(allocationSize);
            info->FileAttributes = attrs;
            info->FileNameLength = nameBytes;
            info->EaSize = 0;
            info->ShortNameLength = 0;
            info->Reserved1 = 0;
            memset(info->ShortName, 0, sizeof(info->ShortName));
            nameOut = entryBase + sizeof(XFILE_BOTH_DIR_INFORMATION_FIXED);
            prevNextEntryOffset = &info->NextEntryOffset;
            break;
        }
        default:
            break;
        }

        if (nameOut)
            writeUtf16Be(reinterpret_cast<be<uint16_t>*>(nameOut), name);

        prevEntrySize = entrySize;
        written += entrySize;
        remaining -= entrySize;
        ++hDir->cursor;

        if (single)
            break;
    }

    if (written == 0)
    {
        IoStatusBlock->Status = STATUS_NO_MORE_FILES;
        IoStatusBlock->Information = 0;
        return STATUS_NO_MORE_FILES;
    }

    // Final entry in the buffer.
    if (prevNextEntryOffset)
        *prevNextEntryOffset = 0;

    IoStatusBlock->Status = STATUS_SUCCESS;
    IoStatusBlock->Information = written;
    return STATUS_SUCCESS;
}

void NtReadFileScatter()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t NtReadFile(
    uint32_t FileHandle,
    uint32_t Event,  // IMPORTANT: Signal this event on completion for async I/O
    uint32_t /*ApcRoutine*/,
    uint32_t /*ApcContext*/,
    XIO_STATUS_BLOCK* IoStatusBlock,
    void* Buffer,
    uint32_t Length,
    be<int64_t>* ByteOffset)
{
    static int s_readCount = 0;
    static int s_eventSignalCount = 0;
    ++s_readCount;
    
    const uint64_t offset = ByteOffset ? static_cast<uint64_t>(ByteOffset->get()) : 0ull;
    if (App::s_logVfs && (s_readCount <= 20 || s_readCount % 500 == 0))
    {
        LOGF_INFO("[TRACE] NtReadFile: #{} handle=0x{:08X} len=0x{:X} offset=0x{:X} event=0x{:08X}",
                  s_readCount, FileHandle, Length, offset, Event);
    }
    
    // Helper lambda to signal completion event if provided
    auto signalCompletionEvent = [&]() {
        if (Event != 0 && Event != GUEST_INVALID_HANDLE_VALUE && IsKernelObject(Event))
        {
            ++s_eventSignalCount;
            if (s_eventSignalCount <= 20 || s_eventSignalCount % 100 == 0)
            {
                LOGF_WARNING("[ASYNC COMPLETE] NtReadFile signaling event 0x{:08X} (count={})", 
                            Event, s_eventSignalCount);
            }
            
            // Signal the event to wake any waiters
            XDISPATCHER_HEADER* eventObj = reinterpret_cast<XDISPATCHER_HEADER*>(g_memory.Translate(Event));
            if (eventObj && (eventObj->Type == 0 || eventObj->Type == 1))
            {
                QueryKernelObject<::Event>(*eventObj)->Set();
            }
        }
    };
    
    // Pump SDL events to keep window responsive
    PumpSdlEventsIfNeeded();
    
    // Explicit check for NULL handle - return fatal error to break retry loops
    if (FileHandle == 0)
    {
        static int nullHandleCount = 0;
        ++nullHandleCount;
        if (nullHandleCount <= 3 || nullHandleCount % 1000 == 0) {
            LOGF_IMPL(Utility, "NtReadFile", "NULL handle 0x00000000 (count={})", nullHandleCount);
        }
        // A failed open can be followed by a read using the zero handle.
        if (IoStatusBlock) {
            IoStatusBlock->Status = STATUS_NO_SUCH_FILE;
            IoStatusBlock->Information = 0;
        }
        signalCompletionEvent();
        return STATUS_NO_SUCH_FILE;
    }
    
    if (FileHandle == GUEST_INVALID_HANDLE_VALUE || !IsKernelObject(FileHandle))
    {
        LOGF_IMPL(Utility, "NtReadFile", "INVALID handle 0x{:08X}", FileHandle);
        if (IoStatusBlock)
        {
            IoStatusBlock->Status = STATUS_INVALID_HANDLE;
            IoStatusBlock->Information = 0;
        }
        signalCompletionEvent();
        return STATUS_INVALID_HANDLE;
    }

    // Virtual (in-memory) file handles (used to satisfy RPF mount header reads).
    if (auto vit = g_ntVirtFileHandles.find(FileHandle);
        vit != g_ntVirtFileHandles.end() && vit->second && vit->second->magic == kNtVirtFileHandleMagic)
    {
        NtVirtFileHandle* hVirt = vit->second;
        if (!IoStatusBlock || !Buffer)
            return STATUS_INVALID_PARAMETER;

        const uint64_t offset = ByteOffset ? static_cast<uint64_t>(ByteOffset->get()) : 0ull;
        if (offset >= hVirt->data.size())
        {
            IoStatusBlock->Status = STATUS_END_OF_FILE;
            IoStatusBlock->Information = 0;
            return STATUS_END_OF_FILE;
        }

        const uint32_t available = static_cast<uint32_t>(hVirt->data.size() - offset);
        const uint32_t toCopy = std::min<uint32_t>(Length, available);
        memcpy(Buffer, hVirt->data.data() + offset, toCopy);

        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = toCopy;
        signalCompletionEvent();
        return STATUS_SUCCESS;
    }

    // Directories can't be read as files. Return EOF so callers terminate scan/read loops.
    if (auto dit = g_ntDirHandles.find(FileHandle); dit != g_ntDirHandles.end() && dit->second && dit->second->magic == kNtDirHandleMagic)
    {
        static int s_dirReadCount = 0;
        if (++s_dirReadCount <= 10 || s_dirReadCount % 1000 == 0) {
            uint32_t callerLR = GET_CALLER_LR();
            LOGF_WARNING("[NtReadFile] Reading directory handle 0x{:08X} ('{}') as file (count={}) caller=0x{:08X} -> returning STATUS_END_OF_FILE",
                         FileHandle, dit->second->path.string(), s_dirReadCount, callerLR);
        }
        if (IoStatusBlock)
        {
            IoStatusBlock->Status = STATUS_END_OF_FILE;
            IoStatusBlock->Information = 0;
        }
        signalCompletionEvent();
        if (s_dirReadCount > 5) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return STATUS_END_OF_FILE;
    }

    auto it = g_ntFileHandles.find(FileHandle);
    if (it == g_ntFileHandles.end() || !it->second || it->second->magic != kNtFileHandleMagic)
    {
        LOGF_IMPL(Utility, "NtReadFile", "Not a file handle 0x{:08X}", FileHandle);
        if (IoStatusBlock)
        {
            IoStatusBlock->Status = STATUS_INVALID_HANDLE;
            IoStatusBlock->Information = 0;
        }
        signalCompletionEvent();
        return STATUS_INVALID_HANDLE;
    }

    NtFileHandle* hFile = it->second;
    if (!IoStatusBlock || !Buffer)
        return STATUS_INVALID_PARAMETER;

    // Parse RPF header on first access so we can detect encrypted TOC ranges.
    if (!hFile->rpfHeaderParsed)
    {
        // If the path extension indicates .rpf, mark it as RPF
        if (!hFile->isRpf)
        {
            try {
                std::string ext = hFile->path.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
                if (ext == ".rpf")
                    hFile->isRpf = true;
            } catch (...) {}
        }
        
        // Parse header if this is an RPF file
        if (hFile->isRpf)
            ParseRpfHeader(hFile);
    }

    const uint64_t fileOffset = ByteOffset ? static_cast<uint64_t>(ByteOffset->get()) : 0ull;
    if (ByteOffset != nullptr)
    {
        hFile->stream.clear();
        hFile->stream.seekg(fileOffset, std::ios::beg);
        if (hFile->stream.bad())
        {
            IoStatusBlock->Status = STATUS_FAIL_CHECK;
            IoStatusBlock->Information = 0;
            return STATUS_FAIL_CHECK;
        }
    }

    hFile->stream.read(reinterpret_cast<char*>(Buffer), Length);
    const uint32_t bytesRead = static_cast<uint32_t>(hFile->stream.gcount());

    if (bytesRead == 0 && Length != 0 && hFile->stream.eof())
    {
        IoStatusBlock->Status = STATUS_END_OF_FILE;
        IoStatusBlock->Information = 0;
        signalCompletionEvent();
        return STATUS_END_OF_FILE;
    }

    // If this is an RPF file with encrypted TOC, decrypt any portion we returned that overlaps the TOC.
    if (hFile->isRpf && hFile->tocEncrypted && bytesRead > 0)
    {
        const uint64_t tocStart = hFile->tocOffset;
        const uint64_t tocEnd = tocStart + hFile->tocSize;
        const uint64_t readStart = fileOffset;
        const uint64_t readEnd = fileOffset + bytesRead;
        const uint64_t overlapStart = std::max(readStart, tocStart);
        const uint64_t overlapEnd = std::min(readEnd, tocEnd);
        if (overlapEnd > overlapStart)
        {
            const uint32_t startInBuf = static_cast<uint32_t>(overlapStart - readStart);
            const uint32_t len = static_cast<uint32_t>(overlapEnd - overlapStart);
            // Pass TOC-relative offset for proper AES block alignment
            const uint64_t tocRelativeOffset = overlapStart - tocStart;
            DecryptRpfBufferInPlace(reinterpret_cast<uint8_t*>(Buffer) + startInBuf, len, tocRelativeOffset);
        }
    }

    const bool ok = !hFile->stream.bad();
    IoStatusBlock->Status = ok ? STATUS_SUCCESS : STATUS_FAIL_CHECK;
    IoStatusBlock->Information = ok ? bytesRead : 0;
    
    // Signal completion event for async I/O
    signalCompletionEvent();
    
    return ok ? STATUS_SUCCESS : STATUS_FAIL_CHECK;
}

uint32_t NtDuplicateObject(uint32_t SourceHandle, be<uint32_t>* TargetHandle, uint32_t Options)
{
    if (SourceHandle == GUEST_INVALID_HANDLE_VALUE)
        return 0xFFFFFFFF;

    if (IsKernelObject(SourceHandle))
    {
        // Increment handle duplicate count.
        const auto& it = g_handleDuplicates.find(SourceHandle);
        if (it != g_handleDuplicates.end())
            ++it->second;
        else
            g_handleDuplicates[SourceHandle] = 1;

        *TargetHandle = SourceHandle;
        return 0;
    }
    else
    {
        // Don't crash on unknown handles - just return error
        static int s_unknownDupCount = 0;
        if (++s_unknownDupCount <= 10)
        {
            LOGF_WARNING("[NtDuplicateObject] Unrecognized handle 0x{:08X} (occurrence #{})", SourceHandle, s_unknownDupCount);
        }
        return 0xC0000008;  // STATUS_INVALID_HANDLE
    }
}

static std::unordered_map<uint32_t, uint32_t> g_ntVirtualMemoryAllocs;

// NT-style virtual memory APIs used by some titles (e.g., GTA V).
// Backed by the existing guest heap allocator since we map a full 4GB guest address space.
// This is intentionally minimal: it supports the common "BaseAddress == NULL" allocation path.
uint32_t NtAllocateVirtualMemory(
    be<uint32_t>* BaseAddress,
    be<uint32_t>* RegionSize,
    uint32_t AllocationType,
    uint32_t Protect,
    uint32_t ZeroBits)
{
    (void)AllocationType;
    (void)Protect;
    (void)ZeroBits;

    if (!BaseAddress || !RegionSize)
        return STATUS_INVALID_PARAMETER;

    uint32_t size = RegionSize->get();
    if (size == 0)
        return STATUS_INVALID_PARAMETER;

    // Round up to page size. X360 is page-based, callers often expect at least 4KB granularity.
    constexpr uint32_t kPageSize = 0x1000;
    size = (size + (kPageSize - 1)) & ~(kPageSize - 1);

    uint32_t requested = BaseAddress->get();
    if (requested != 0)
    {
        // We currently don't manage fixed-address reservations/commits.
        // The full guest address space is already mapped, so accept the request if it is in-range.
        if (requested >= PPC_MEMORY_SIZE)
            return STATUS_INVALID_PARAMETER;

        *RegionSize = size;
        return STATUS_SUCCESS;
    }

    if (size > 0x10000000 || (AllocationType & 0x00002000 /* MEM_RESERVE */))
    {
        // Virtual address space reservation (entire 4GB guest memory is already mapped in recomp).
        static uint32_t s_virtualReserveBase = 0x40000000;
        *BaseAddress = s_virtualReserveBase;
        *RegionSize = size;
        LOGF_INFO("[NtAllocateVirtualMemory] Virtual reservation size=0x{:08X} allocType=0x{:X} -> base=0x{:08X}", 
                  size, AllocationType, s_virtualReserveBase);
        return STATUS_SUCCESS;
    }

    void* ptr = g_userHeap.AllocPhysical(size, kPageSize);
    if (!ptr)
        return STATUS_NO_MEMORY;

    const uint32_t base = g_memory.MapVirtual(ptr);
    g_ntVirtualMemoryAllocs[base] = size;

    *BaseAddress = base;
    *RegionSize = size;
    return STATUS_SUCCESS;
}

uint32_t NtFreeVirtualMemory(
    be<uint32_t>* BaseAddress,
    be<uint32_t>* RegionSize,
    uint32_t FreeType,
    uint32_t /*Unknown*/)
{
    (void)FreeType;

    if (!BaseAddress)
        return STATUS_INVALID_PARAMETER;

    const uint32_t addr = BaseAddress->get();
    if (addr == 0)
        return STATUS_SUCCESS;

    // If MEM_RELEASE is specified, RegionSize must be 0 on Win32.
    // Some titles may pass non-zero; we ignore and free anyway.
    if (RegionSize)
        *RegionSize = 0;

    const auto it = g_ntVirtualMemoryAllocs.find(addr);
    if (it != g_ntVirtualMemoryAllocs.end())
    {
        g_userHeap.Free(g_memory.Translate(addr));
        g_ntVirtualMemoryAllocs.erase(it);
    }
    *BaseAddress = 0;
    return STATUS_SUCCESS;
}

void ObDereferenceObject(void* object)
{
    // Reference counting is not fully implemented.
    // Objects are managed by the kernel object system.
    // This is a no-op for now.
    (void)object;
}

void KeSetBasePriorityThread(uint32_t threadHandle, int priority)
{
    (void)threadHandle;
    (void)priority;
}

uint32_t ObReferenceObjectByHandle(uint32_t handle, uint32_t objectType, be<uint32_t>* object)
{
    *object = handle;
    return 0;
}

void KeQueryBasePriorityThread()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t NtSuspendThread(uint32_t threadHandle, be<uint32_t>* suspendCount)
{
    if (suspendCount != nullptr)
        *suspendCount = 0;

    if (IsKernelObject(threadHandle)) {
        GuestThreadHandle* hThread = GetKernelObject<GuestThreadHandle>(threadHandle);
        if (hThread) {
            LOGF_INFO("[TRACE] NtSuspendThread: handle=0x{:08X} (suspending)", threadHandle);
            hThread->suspended = true;
            return S_OK;
        }
    }
    return S_OK;
}

uint32_t KeSetAffinityThread(uint32_t Thread, uint32_t Affinity, be<uint32_t>* lpPreviousAffinity)
{
    if (lpPreviousAffinity)
        *lpPreviousAffinity = 2;

    return 0;
}

void RtlLeaveCriticalSection(XRTL_CRITICAL_SECTION* cs)
{
    if (!cs)
        return;

    uint32_t guestAddr = g_memory.MapVirtual(cs);
    if (guestAddr < 0x10000000)
        return;

    if (guestAddr == 0x83878764)
    {
        static uint32_t s_cs8387LeaveCount = 0;
        if (++s_cs8387LeaveCount <= 20 || s_cs8387LeaveCount % 100 == 0)
        {
            LOGF_INFO("[LOCK 0x83878764] LEAVE #{} by thread=0x{:08X} (newRecCount={})",
                      s_cs8387LeaveCount,
                      (g_ppcContext && g_ppcContext->r13.u32 != 0) ? g_ppcContext->r13.u32 : 1,
                      cs->RecursionCount.get() - 1);
        }
    }

    int32_t rec = cs->RecursionCount.get();
    if (rec <= 1)
    {
        cs->RecursionCount = 0;
        std::atomic_ref owningThread(cs->OwningThread);
        owningThread.store(0);
        owningThread.notify_all();
    }
    else
    {
        cs->RecursionCount = rec - 1;
    }
}

void RtlEnterCriticalSection(XRTL_CRITICAL_SECTION* cs)
{
    if (!cs)
        return;

    uint32_t guestAddr = g_memory.MapVirtual(cs);
    if (guestAddr < 0x10000000)
    {
        static uint32_t s_lowCount = 0;
        if (++s_lowCount <= 10)
        {
            LOGF_WARNING("[LOCK] RtlEnterCriticalSection ignored invalid low address 0x{:08X} (caller=0x{:08X})",
                         guestAddr, GET_CALLER_LR());
        }
        return;
    }

    static int s_count = 0;
    static int s_waitCount = 0;
    ++s_count;

    be<uint32_t> thisThread =
        (g_ppcContext && g_ppcContext->r13.u32 != 0) ? g_ppcContext->r13.u32 : 1;

    if (guestAddr == 0x83878764)
    {
        static uint32_t s_cs8387EnterCount = 0;
        if (++s_cs8387EnterCount <= 20 || s_cs8387EnterCount % 100 == 0)
        {
            LOGF_INFO("[LOCK 0x83878764] ENTER #{} by thread=0x{:08X} (recCount={})",
                      s_cs8387EnterCount, thisThread.get(), cs->RecursionCount.get());
        }
    }

    // If critical section has no recursion count (unlocked) but stale owner, clear stale owner
    if (cs->RecursionCount.get() == 0 && cs->OwningThread.get() != 0)
    {
        uint32_t owner = cs->OwningThread.get();
        if (owner < 0x80000000 || owner > 0xF0000000)
        {
            static uint32_t s_staleCount = 0;
            if (++s_staleCount <= 20)
            {
                LOGF_WARNING("[LOCK] Clearing stale owner 0x{:08X} (recCount=0) on cs=0x{:08X}",
                             owner, guestAddr);
            }
            cs->OwningThread = 0;
        }
    }

    std::atomic_ref owningThread(cs->OwningThread);

    int loopCount = 0;

    while (true)
    {
        be<uint32_t> previousOwner = 0;

        if (owningThread.compare_exchange_weak(previousOwner, thisThread) ||
            previousOwner == thisThread)
        {
            cs->RecursionCount = cs->RecursionCount.get() + 1;
            return;
        }

        if (loopCount == 0)
        {
            ++s_waitCount;
            if (s_waitCount <= 50)
            {
                LOGF_WARNING("[LOCK] RtlEnterCriticalSection #{} thisThread=0x{:08X} WAITING on owner=0x{:08X} cs=0x{:08X}",
                             s_count, thisThread.get(), previousOwner.get(),
                             guestAddr);
            }
        }
        ++loopCount;

        // Anti-deadlock guard: if spinning on an invalid thread ID or orphaned lock
        if (loopCount > 100 && (previousOwner.get() < 0x80000000 || previousOwner.get() > 0xF0000000))
        {
            static uint32_t s_breakCount = 0;
            if (++s_breakCount <= 20)
            {
                LOGF_WARNING("[LOCK] Breaking deadlock on cs=0x{:08X}: invalid owner=0x{:08X}, acquiring for thread=0x{:08X}",
                             guestAddr, previousOwner.get(), thisThread.get());
            }
            owningThread.store(thisThread);
            cs->RecursionCount = 1;
            return;
        }

        if (loopCount % 5000 == 0)
        {
            LOGF_WARNING("[LOCK] Long wait on cs=0x{:08X} ({} spins, owner=0x{:08X}, thisThread=0x{:08X})",
                         guestAddr, loopCount,
                         previousOwner.get(), thisThread.get());
        }

        if (loopCount < 50)
            std::this_thread::yield();
        else if (loopCount < 1000)
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void RtlImageXexHeaderField()
{
    LOG_UTILITY("!!! STUB !!!");
}

void HalReturnToFirmware()
{
    LOG_UTILITY("!!! STUB !!!");
}

void RtlFillMemoryUlong()
{
    LOG_UTILITY("!!! STUB !!!");
}

void KeBugCheckEx(uint32_t bugCheckCode, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4)
{
    LOGF_ERROR("[KeBugCheckEx] code=0x{:08X} ({}) p1=0x{:08X} p2=0x{:08X} p3=0x{:08X} p4=0x{:08X}", 
               bugCheckCode, bugCheckCode, p1, p2, p3, p4);
}

uint32_t KeGetCurrentProcessType()
{
    return 1;
}

void RtlCompareMemoryUlong()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t RtlInitializeCriticalSection(XRTL_CRITICAL_SECTION* cs)
{
    if (!cs)
        return 0xC0000008;

    uint32_t guestAddr = g_memory.MapVirtual(cs);
    if (guestAddr < 0x10000000)
        return 0xC0000008;

    // printf("RtlInitializeCriticalSection %x\n", cs);
    cs->Header.Absolute = 0;
    cs->LockCount = -1;
    cs->RecursionCount = 0;
    cs->OwningThread = 0;

    return 0;
}

void RtlRaiseException_x()
{
    LOG_UTILITY("!!! STUB !!!");
}

void KfReleaseSpinLock(uint32_t* spinLock)
{
    std::atomic_ref spinLockRef(*spinLock);
    spinLockRef = 0;
}

void KfAcquireSpinLock(uint32_t* spinLock)
{
    if (!spinLock) return;
    std::atomic_ref spinLockRef(*spinLock);
    uint32_t owner = (g_ppcContext && g_ppcContext->r13.u32 != 0) ? g_ppcContext->r13.u32 : 1;

    while (true)
    {
        uint32_t expected = 0;
        if (spinLockRef.compare_exchange_weak(expected, owner))
            break;

        std::this_thread::yield();
    }
}

uint64_t KeQueryPerformanceFrequency()
{
    return 49875000;
}

void MmFreePhysicalMemory(uint32_t type, uint32_t guestAddress)
{
    if (guestAddress != NULL)
        g_userHeap.Free(g_memory.Translate(guestAddress));
}

bool VdPersistDisplay(uint32_t a1, uint32_t* a2)
{
    *a2 = NULL;
    return false;
}

void FireVBlankCallback();
void VdCallGraphicsNotificationRoutines(uint32_t type = 0, uint32_t data = 0);

void VdSwap()
{
    KernelPhase_EnterRuntime();
    g_vdSwapCounter.fetch_add(1, std::memory_order_relaxed);
    g_lastVdSwapTime = std::chrono::steady_clock::now();

    static uint32_t s_swapCount = 0;
    ++s_swapCount;
    if (s_swapCount <= 10 || (s_swapCount % 60) == 0)
    {
        LOGF_INFO("[VdSwap] Frame swap #{}", s_swapCount);
    }

    // Call Video::Present() to clear/flip Vulkan swapchain
    Video::Present();

    // Fire VBlank interrupt callback so RAGE render thread advances its frame
    FireVBlankCallback();

    // Trigger graphics notification routines
    VdCallGraphicsNotificationRoutines(1, 0);
}

void VdGetSystemCommandBuffer()
{
    static std::unordered_set<uint32_t> s_seenCallers;
    const uint32_t lr = GET_CALLER_LR();
    if (lr != 0 && s_seenCallers.insert(lr).second)
        LOGF_UTILITY("!!! STUB !!! caller_lr=0x{:08X}", lr);
    else
        LOG_UTILITY("!!! STUB !!!");
}

void KeReleaseSpinLockFromRaisedIrql(uint32_t* spinLock)
{
    std::atomic_ref spinLockRef(*spinLock);
    spinLockRef = 0;
}

void KeAcquireSpinLockAtRaisedIrql(uint32_t* spinLock)
{
    if (!spinLock) return;
    std::atomic_ref spinLockRef(*spinLock);
    uint32_t owner = (g_ppcContext && g_ppcContext->r13.u32 != 0) ? g_ppcContext->r13.u32 : 1;

    while (true)
    {
        uint32_t expected = 0;
        if (spinLockRef.compare_exchange_weak(expected, owner))
            break;

        std::this_thread::yield();
    }
}

uint32_t KiApcNormalRoutineNop()
{
    return 0;
}

void VdEnableRingBufferRPtrWriteBack(uint32_t writebackAddr, uint32_t blockSizeLog2)
{
    // r3 = writeback address (physical)
    // r4 = log2(block size), typically 6
    g_gpuRingBuffer.readPtrWritebackAddr = writebackAddr;
    g_gpuRingBuffer.blockSize = 1u << blockSizeLog2;
    g_gpuRingBuffer.writebackEnabled = true;
    
    LOGF_UTILITY("writebackAddr=0x{:08X} blockSizeLog2={} blockSize={}", 
                 writebackAddr, blockSizeLog2, g_gpuRingBuffer.blockSize);
    
    // Immediately write 0 to the writeback address to indicate GPU is caught up
    if (writebackAddr != 0)
    {
        uint32_t* hostPtr = reinterpret_cast<uint32_t*>(
            g_memory.Translate(0xC0000000u | (writebackAddr & 0x1FFFFFFFu)));
        if (hostPtr)
        {
            *hostPtr = 0;  // GPU read pointer = 0 (caught up with write pointer initially)
            LOGF_UTILITY("Initialized writeback at 0x{:08X} to 0", writebackAddr);
        }
    }
}

void VdInitializeRingBuffer(uint32_t physAddr, uint32_t sizeLog2)
{
    // r3 = physical address of ring buffer (from MmGetPhysicalAddress)
    // r4 = log2(size)
    g_gpuRingBuffer.ringBufferBase = physAddr;
    g_gpuRingBuffer.ringBufferSize = 1u << sizeLog2;
    g_gpuRingBuffer.initialized = true;
    
    LOGF_INFO("[TRACE] VdInitializeRingBuffer: base=0x{:08X} sizeLog2={} size=0x{:X}", 
              physAddr, sizeLog2, g_gpuRingBuffer.ringBufferSize);
}

static uint32_t* TranslateGpuPhysical(uint32_t address)
{
    const uint32_t guestAlias = 0xC0000000u | (address & 0x1FFFFFFFu);
    if (guestAlias > PPC_MEMORY_SIZE - sizeof(uint32_t))
        return nullptr;

    return reinterpret_cast<uint32_t*>(g_memory.Translate(guestAlias));
}

static void DispatchGraphicsInterrupt(uint32_t source, uint32_t cpu)
{
    const uint32_t callback = g_gpuRingBuffer.interruptCallback;
    if (callback == 0)
        return;

    PPCContext* previousContext = GetPPCContext();
    PPCContext interruptContext = previousContext ? *previousContext : PPCContext{};
    interruptContext.r3.u32 = source;
    interruptContext.r4.u32 = g_gpuRingBuffer.interruptUserData;
    if (interruptContext.r13.u32 < PPC_MEMORY_SIZE - 269)
        *reinterpret_cast<uint8_t*>(
            g_memory.Translate(interruptContext.r13.u32 + 268)) = static_cast<uint8_t>(cpu);
    static std::atomic<uint32_t> s_dispatchCount{0};
    const uint32_t dispatchCount = ++s_dispatchCount;
    if (dispatchCount <= 8)
        LOGF_INFO("[GPU IRQ] dispatch #{} source={} cpu={} callback=0x{:08X} userData=0x{:08X}",
                  dispatchCount, source, cpu, callback, g_gpuRingBuffer.interruptUserData);
    SetPPCContext(interruptContext);

    if (PPCFunc* callbackFunction = g_memory.FindFunction(callback))
    {
        callbackFunction(interruptContext, g_memory.base);
        if (dispatchCount <= 8)
            LOGF_INFO("[GPU IRQ] dispatch #{} completed return=0x{:08X}",
                      dispatchCount, static_cast<uint32_t>(interruptContext.r3.u32));
    }
    else
        LOGF_WARNING("[GPU] Graphics interrupt callback is unmapped: 0x{:08X}", callback);

    if (previousContext)
        SetPPCContext(*previousContext);
    else
        g_ppcContext = nullptr;
}

static bool StoreGpuWrite(uint32_t address, uint32_t value)
{
    uint8_t* destination = reinterpret_cast<uint8_t*>(
        TranslateGpuPhysical(address & ~0x3u));
    if (!destination)
        return false;

    const uint32_t endianness = address & 0x3u;
    switch (endianness)
    {
        case 1: value = static_cast<uint32_t>(
                    (value & 0x00FF00FFu) << 8 |
                    (value & 0xFF00FF00u) >> 8); break;
        case 2: value = ByteSwap(value); break;
        case 3: value = (value << 16) | (value >> 16); break;
        default: break;
    }

    std::memcpy(destination, &value, sizeof(value));
    return true;
}

static uint32_t GpuSwapPM4(uint32_t value, uint32_t endianness)
{
    switch (endianness & 0x3u)
    {
        case 1: return (value & 0x00FF00FFu) << 8 |
                       (value & 0xFF00FF00u) >> 8;
        case 2: return ByteSwap(value);
        case 3: return (value << 16) | (value >> 16);
        default: return value;
    }
}

static uint32_t ExecutePM4Buffer(uint32_t* buffer, uint32_t dwordCount,
                                uint32_t depth = 0,
                                uint32_t* consumedDwords = nullptr)
{
    if (!buffer || depth > 8)
        return 0;

    static std::array<uint32_t, 0x10000> s_registerFile{};
    uint32_t executed = 0;
    uint32_t pos = 0;
    const uint32_t limit = std::min(dwordCount, 4096u);

    for (; pos < limit;)
    {
        const uint32_t packet = ByteSwap(buffer[pos]);
        if (packet == 0)
            break;

        const uint32_t type = packet >> 30;
        if (type == PM4_TYPE2)
        {
            ++pos;
            continue;
        }

        if (type == PM4_TYPE0)
        {
            const uint32_t count = ((packet >> 16) & 0x3FFFu) + 1;
            if (count > limit - pos - 1)
                break;
            uint32_t reg = packet & 0x7FFFu;
            for (uint32_t i = 0; i < count; ++i)
                s_registerFile[(reg + i) & 0xFFFFu] = ByteSwap(buffer[pos + 1 + i]);
            pos += count + 1;
            ++executed;
            continue;
        }

        if (type != PM4_TYPE3)
            break;

        const uint32_t opcode = (packet >> 8) & 0x7Fu;
        const uint32_t count = ((packet >> 16) & 0x3FFFu) + 1;
        if (count > limit - pos - 1)
            break;
        auto payload = [&](uint32_t index) { return ByteSwap(buffer[pos + 1 + index]); };
        bool waitConditionMet = true;

        switch (opcode)
        {
            case PM4_INDIRECT_BUFFER:
            case PM4_INDIRECT_BUFFER_PFD:
                if (count >= 2)
                {
                    const uint32_t address = payload(0);
                    const uint32_t indirectDwords = payload(1) & 0xFFFFFu;
                    if (indirectDwords != 0)
                    {
                        uint32_t* indirect = TranslateGpuPhysical(address);
                        if (indirect)
                            executed += ExecutePM4Buffer(indirect, indirectDwords, depth + 1);
                    }
                }
                break;
            case PM4_MEM_WRITE:
                if (count >= 2)
                {
                    uint32_t address = payload(0);
                    for (uint32_t i = 1; i < count; ++i, address += 4)
                    {
                        if (!StoreGpuWrite(address, payload(i)))
                            LOGF_WARNING("[PM4] MEM_WRITE target unmapped: 0x{:08X}", address);
                    }
                }
                break;
            case PM4_EVENT_WRITE_SHD:
                if (count >= 3)
                {
                    const uint32_t initiator = payload(0);
                    const uint32_t address = payload(1);
                    const uint32_t value = payload(2);
                    if (!StoreGpuWrite(address, value))
                        LOGF_WARNING("[PM4] EVENT_WRITE_SHD target unmapped: 0x{:08X}", address);
                    else
                    {
                        LOGF_INFO("[PM4] EVENT_WRITE_SHD executed initiator=0x{:08X} target=0x{:08X} value=0x{:08X}",
                                  initiator, address, value);
                    }
                }
                break;
            case PM4_EVENT_WRITE_EXT:
                if (count >= 2)
                {
                    const uint32_t address = payload(1);
                    uint8_t* destination = reinterpret_cast<uint8_t*>(
                        TranslateGpuPhysical(address & ~0x3u));
                    if (destination)
                    {
                        static constexpr uint16_t extents[] = {0, 0x03FF, 0, 0x03FF, 0, 1};
                        for (size_t i = 0; i < std::size(extents); ++i)
                        {
                            const uint16_t value = ByteSwap(extents[i]);
                            std::memcpy(destination + i * sizeof(value), &value, sizeof(value));
                        }
                    }
                }
                break;
            case PM4_SET_CONSTANT:
            case PM4_SET_CONSTANT2:
            case PM4_SET_SHADER_CONSTANTS:
                if (count >= 2)
                {
                    const uint32_t reg = payload(0) & 0x7FFFu;
                    for (uint32_t i = 1; i < count; ++i)
                        s_registerFile[(reg + i - 1) & 0xFFFFu] = payload(i);
                }
                break;
            case PM4_WAIT_REG_MEM:
                if (count >= 5)
                {
                    const uint32_t waitInfo = payload(0);
                    const uint32_t target = payload(1);
                    const uint32_t reference = payload(2);
                    const uint32_t mask = payload(3);
                    uint32_t current = s_registerFile[target & 0xFFFFu];
                    if (waitInfo & 0x10u)
                    {
                        uint32_t* targetPointer = TranslateGpuPhysical(target & ~0x3u);
                        if (targetPointer)
                        {
                            std::memcpy(&current, targetPointer, sizeof(current));
                            current = GpuSwapPM4(current, target & 0x3u);
                            // Satisfy the memory fence so synchronous PM4 command processing is never deadlocked
                            StoreGpuWrite(target, reference);
                        }
                    }
                    else
                    {
                        s_registerFile[target & 0xFFFFu] = reference;
                    }
                    waitConditionMet = true;
                    LOGF_INFO("[PM4] WAIT_REG_MEM memory={} target=0x{:08X} current=0x{:08X} ref=0x{:08X} mask=0x{:08X} satisfied=true",
                              (waitInfo & 0x10u) != 0, target, current, reference, mask);
                }
                break;
            case PM4_EVENT_WRITE:
                break;
            case PM4_INTERRUPT:
                if (count >= 1)
                {
                    const uint32_t cpuMask = payload(0);
                    for (uint32_t cpu = 0; cpu < 6; ++cpu)
                    {
                        if (cpuMask & (1u << cpu))
                            DispatchGraphicsInterrupt(1, cpu);
                    }
                }
                break;
            case PM4_DRAW_INDX:
            case PM4_DRAW_INDX_2:
                if ((opcode == PM4_DRAW_INDX_2 && count >= 1) ||
                    (opcode == PM4_DRAW_INDX && count >= 2))
                {
                    const uint32_t initiator = payload(opcode == PM4_DRAW_INDX_2 ? 0 : 1);
                    const uint32_t primitiveType = initiator & 0x3Fu;
                    const uint32_t sourceSelect = (initiator >> 6) & 0x3u;
                    const uint32_t indexCount = (initiator >> 16) & 0x3FFFu;
                    if (sourceSelect == 2)
                    {
                        const bool queued = Video::ExecutePM4Draw(primitiveType, indexCount);
                        g_gpuRingBuffer.pm4DrawCount++;
                        g_pm4PacketCounter.fetch_add(1, std::memory_order_relaxed);
                        LOGF_INFO("[PM4] Auto-index draw executed #{} primitive={} indices={} queued={}",
                                  g_gpuRingBuffer.pm4DrawCount, primitiveType, indexCount, queued);
                    }
                    else
                    {
                        LOGF_WARNING("[PM4] Draw packet source {} is not implemented", sourceSelect);
                    }
                }
                break;
            default:
                break;
        }

        if (!waitConditionMet)
            break;
        pos += count + 1;
        ++executed;
    }
    if (consumedDwords)
        *consumedDwords = pos;
    return executed;
}

// Scan a command buffer for PM4 packets (helper function)
// Returns number of packets found
static uint32_t ScanPM4Buffer(uint32_t* buffer, uint32_t bufferDwords, bool isIndirect, uint32_t scanCount, uint32_t depth = 0) {
    static uint32_t s_loggedWaitCount = 0;
    const bool accountPackets = false;
    uint32_t packetsFound = 0;
    uint32_t pos = 0;
    uint32_t maxScan = std::min(bufferDwords, 1024u);
    
    while (pos < maxScan && packetsFound < 200) {
        uint32_t packet = ByteSwap(buffer[pos]);
        uint32_t type = (packet >> 30) & 0x3;
        
        if (type == PM4_TYPE3) {
            uint32_t opcode = (packet >> 8) & 0x7F;
            uint32_t count = ((packet >> 16) & 0x3FFF) + 1;
            
            // Log ALL packets from indirect buffers for first few scans
            if (isIndirect && scanCount == 0) {
                LOGF_WARNING("[PM4-IB] opcode=0x{:02X}({}) count={} pos={}",
                    opcode, GetPM4OpcodeName(opcode), count, pos);
            }
            
            // Count by type & process fences
            switch (opcode) {
                case PM4_INDIRECT_BUFFER_PFD:
                case PM4_INDIRECT_BUFFER: {
                    if (count >= 2 && pos + 2 < maxScan && depth < 8) {
                        const uint32_t indirectAddress = ByteSwap(buffer[pos + 1]);
                        const uint32_t indirectSize = ByteSwap(buffer[pos + 2]) & 0xFFFFFu;
                        uint32_t* indirect = TranslateGpuPhysical(indirectAddress);
                        if (isIndirect && scanCount == 0) {
                            LOGF_WARNING(
                                "[PM4-IB] nested addr=0x{:08X} size={} dwords depth={}",
                                indirectAddress, indirectSize, depth + 1);
                        }
                        if (indirect != nullptr && indirectSize != 0) {
                            packetsFound += ScanPM4Buffer(
                                indirect, indirectSize, true, scanCount, depth + 1);
                        }
                    }
                    if (accountPackets) g_gpuRingBuffer.pm4OtherCount++;
                    break;
                }
                case PM4_WAIT_REG_MEM: {
                    if (s_loggedWaitCount < 16 && count >= 5 && pos + 5 < maxScan) {
                        const uint32_t waitInfo = ByteSwap(buffer[pos + 1]);
                        const uint32_t pollAddress = ByteSwap(buffer[pos + 2]);
                        const uint32_t reference = ByteSwap(buffer[pos + 3]);
                        const uint32_t mask = ByteSwap(buffer[pos + 4]);
                        const uint32_t waitValue = ByteSwap(buffer[pos + 5]);
                        const bool isMemory = (waitInfo & 0x10) != 0;
                        const uint32_t* poll = isMemory
                            ? TranslateGpuPhysical(pollAddress & ~0x3u)
                            : nullptr;
                        const uint32_t currentValue = poll ? ByteSwap(*poll) : 0;
                        LOGF_WARNING(
                            "[PM4-IB] WAIT_REG_MEM scan={} indirect={} info=0x{:08X} target={} address=0x{:08X} ref=0x{:08X} mask=0x{:08X} wait=0x{:08X} current=0x{:08X}",
                            scanCount, isIndirect,
                            waitInfo, isMemory ? "memory" : "register",
                            pollAddress, reference, mask, waitValue, currentValue);
                        ++s_loggedWaitCount;
                    }
                    g_gpuRingBuffer.pm4OtherCount++;
                    break;
                }
                case PM4_DRAW_INDX:
                case PM4_DRAW_INDX_2: {
                    static uint32_t s_loggedDrawCount = 0;
                    if (s_loggedDrawCount < 10 && count >= 1 && pos + 1 < maxScan) {
                        const uint32_t initiator = ByteSwap(buffer[pos + 1]);
                        LOGF_WARNING(
                            "[PM4-IB] draw initiator=0x{:08X} primitive={} source={} indexSize={} indices={}",
                            initiator, initiator & 0x3Fu, (initiator >> 6) & 0x3u,
                            (initiator >> 11) & 0x1u, initiator >> 16);
                        ++s_loggedDrawCount;
                    }
                    if (accountPackets) g_gpuRingBuffer.pm4DrawCount++;
                    break;
                }
                case PM4_IM_LOAD:
                case PM4_IM_LOAD_IMMEDIATE:
                    if (accountPackets) g_gpuRingBuffer.pm4ShaderLoadCount++;
                    break;
                case PM4_SET_CONSTANT:
                case PM4_SET_CONSTANT2:
                case PM4_SET_SHADER_CONSTANTS:
                    if (accountPackets) g_gpuRingBuffer.pm4SetConstantCount++;
                    break;
                case PM4_XE_SWAP:
                    if (accountPackets) g_gpuRingBuffer.pm4SwapCount++;
                    break;
                case PM4_EVENT_WRITE_SHD:
                case PM4_EVENT_WRITE_EXT: {
                    if (scanCount == 0 && count >= 3 && pos + 3 < maxScan) {
                        const uint32_t initiator = ByteSwap(buffer[pos + 1]);
                        const uint32_t address = ByteSwap(buffer[pos + 2]);
                        const uint32_t value = ByteSwap(buffer[pos + 3]);
                        LOGF_WARNING(
                            "[PM4-IB] event-write opcode=0x{:02X} initiator=0x{:08X} address=0x{:08X} value=0x{:08X} callback=0x{:08X}",
                            opcode, initiator, address, value,
                            g_gpuRingBuffer.interruptCallback);
                    }
                    if (accountPackets) g_gpuRingBuffer.pm4OtherCount++;
                    break;
                }
                case PM4_MEM_WRITE:
                    if (accountPackets) g_gpuRingBuffer.pm4OtherCount++;
                    break;
                default:
                    if (accountPackets) g_gpuRingBuffer.pm4OtherCount++;
                    break;
            }
            
            packetsFound++;
            pos += 1 + count;
        }
        else if (type == PM4_TYPE0) {
            // Register write: extract register index and count
            uint32_t regIndex = packet & 0x7FFF;
            uint32_t count = ((packet >> 16) & 0x3FFF) + 1;
            
            // Log first few TYPE0 packets from indirect buffers
            if (isIndirect && scanCount == 0) {
                LOGF_WARNING("[PM4-IB] TYPE0 reg=0x{:04X} count={} pos={}",
                    regIndex, count, pos);
            }
            
            pos += 1 + count;
            packetsFound++;
        }
        else if (type == PM4_TYPE2) {
            if (isIndirect && scanCount == 0) {
                LOGF_WARNING("[PM4-IB] TYPE2 pos={}", pos);
            }
            pos++;
        }
        else {
            if (isIndirect && scanCount == 0) {
                LOGF_WARNING("[PM4-IB] type={} raw=0x{:08X} pos={}", type, packet, pos);
            }
            pos++;
        }
    }
    
    return packetsFound;
}

// Scan the GPU ring buffer for PM4 packets and log what commands the game writes
// This is diagnostic - helps determine if GTA V uses GPU command buffer or D3D API
void ScanRingBufferForPM4Packets()
{
    if (!g_gpuRingBuffer.initialized || g_gpuRingBuffer.ringBufferBase == 0)
        return;
    
    // Get pointer to ring buffer in host memory
    uint32_t* ringBuffer = TranslateGpuPhysical(g_gpuRingBuffer.ringBufferBase);
    if (!ringBuffer)
        return;
    
    uint32_t ringBufferDwords = g_gpuRingBuffer.ringBufferSize / 4;
    if (ringBufferDwords == 0)
        return;
    
    static bool s_scannedOnce = false;
    static uint32_t s_scanCount = 0;
    static bool s_loggedIndirectBuffers = false;
    
    // First pass: scan primary ring buffer and collect INDIRECT_BUFFER addresses
    std::vector<std::pair<uint32_t, uint32_t>> indirectBuffers;  // address, size
    uint32_t packetsFound = 0;
    uint32_t pos = 0;
    uint32_t maxScan = std::min(ringBufferDwords, 512u);
    
    while (pos < maxScan && packetsFound < 50) {
        uint32_t packet = ByteSwap(ringBuffer[pos]);
        uint32_t type = (packet >> 30) & 0x3;
        
        if (type == PM4_TYPE3) {
            uint32_t opcode = (packet >> 8) & 0x7F;
            uint32_t count = ((packet >> 16) & 0x3FFF) + 1;
            
            // Log the first few packets we find
            if (!s_scannedOnce || s_scanCount < 5) {
                LOGF_WARNING("[PM4] pos={} type=3 opcode=0x{:02X}({}) count={}",
                    pos, opcode, GetPM4OpcodeName(opcode), count);
            }
            
            // Capture INDIRECT_BUFFER and INDIRECT_BUFFER_PFD addresses
            if ((opcode == PM4_INDIRECT_BUFFER || opcode == PM4_INDIRECT_BUFFER_PFD) && count >= 2) {
                uint32_t ibAddr = ByteSwap(ringBuffer[pos + 1]);
                uint32_t ibSize = ByteSwap(ringBuffer[pos + 2]) & 0xFFFFF;  // Size in dwords
                indirectBuffers.push_back({ibAddr, ibSize});
                
                if (!s_loggedIndirectBuffers && s_scanCount < 5) {
                    LOGF_WARNING("[PM4] INDIRECT_BUFFER addr=0x{:08X} size={} dwords",
                        ibAddr, ibSize);
                }
            }
            // Count by type (primary buffer)
            g_gpuRingBuffer.pm4OtherCount++;
            
            packetsFound++;
            pos += 1 + count;
        }
        else if (type == PM4_TYPE0) {
            uint32_t count = ((packet >> 16) & 0x3FFF) + 1;
            pos += 1 + count;
            packetsFound++;
        }
        else if (type == PM4_TYPE2) {
            pos++;
        }
        else {
            pos++;
        }
    }
    
    // Second pass: scan indirect buffers for actual draw/shader commands
    for (const auto& [ibAddr, ibSize] : indirectBuffers) {
        if (ibAddr == 0 || ibSize == 0) continue;
        
        uint32_t* ibBuffer = TranslateGpuPhysical(ibAddr);
        if (ibBuffer) {
            uint32_t ibPackets = ScanPM4Buffer(ibBuffer, ibSize, true, s_scanCount);
            packetsFound += ibPackets;
        }
    }
    
    if (!indirectBuffers.empty()) {
        s_loggedIndirectBuffers = true;
    }
    
    // Log summary periodically
    if (packetsFound > 0 && (s_scanCount % 50 == 0 || !s_scannedOnce)) {
        LOGF_WARNING("[PM4 SUMMARY] scan={} found={} draws={} shaders={} constants={} swaps={} other={} IB={}",
            s_scanCount, packetsFound,
            g_gpuRingBuffer.pm4DrawCount,
            g_gpuRingBuffer.pm4ShaderLoadCount,
            g_gpuRingBuffer.pm4SetConstantCount,
            g_gpuRingBuffer.pm4SwapCount,
            g_gpuRingBuffer.pm4OtherCount,
            indirectBuffers.size());
    }
    
    if (packetsFound > 0) {
        s_scannedOnce = true;
    }

    uint32_t consumedDwords = 0;
    const uint32_t executionStart = g_gpuRingBuffer.processedReadPtr % ringBufferDwords;
    const uint32_t executedPackets = ExecutePM4Buffer(
        ringBuffer + executionStart, ringBufferDwords - executionStart, 0, &consumedDwords);
    if (consumedDwords != 0)
    {
        g_gpuRingBuffer.processedReadPtr =
            (executionStart + consumedDwords) % ringBufferDwords;
        if (g_gpuRingBuffer.writebackEnabled && g_gpuRingBuffer.readPtrWritebackAddr != 0)
        {
            uint32_t* writeback = TranslateGpuPhysical(g_gpuRingBuffer.readPtrWritebackAddr);
            if (writeback)
            {
                const uint32_t byteOffset = g_gpuRingBuffer.processedReadPtr * sizeof(uint32_t);
                const uint32_t pointerValue = g_gpuRingBuffer.blockSize
                    ? byteOffset / g_gpuRingBuffer.blockSize : byteOffset;
                const uint32_t encodedPointer = ByteSwap(pointerValue);
                std::memcpy(writeback, &encodedPointer, sizeof(encodedPointer));
            }
        }
        LOGF_INFO("[PM4] Executor consumed {} packets / {} dwords; ring rptr={} dwords",
                  executedPackets, consumedDwords, g_gpuRingBuffer.processedReadPtr);
    }

    g_pm4PacketCounter.store(g_gpuRingBuffer.pm4DrawCount + g_gpuRingBuffer.pm4SwapCount, std::memory_order_relaxed);
    s_scanCount++;
}

uint32_t MmGetPhysicalAddress(uint32_t address)
{
    const uint32_t physicalAddress =
        address >= 0x80000000u ? address & 0x1FFFFFFFu : address;
    LOGF_UTILITY("virtual=0x{:08X} physical=0x{:08X}", address, physicalAddress);
    return physicalAddress;
}

void VdSetSystemCommandBufferGpuIdentifierAddress()
{
    LOG_UTILITY("!!! STUB !!!");
}

void _vsnprintf_x()
{
    LOG_UTILITY("!!! STUB !!!");
}

void sprintf_x()
{
    LOG_UTILITY("!!! STUB !!!");
}

int32_t ExRegisterTitleTerminateNotification(uint32_t* reg, uint32_t create)
{
    LOG_UTILITY("!!! STUB !!!");
    return 0;
}

void VdShutdownEngines()
{
    LOG_UTILITY("!!! STUB !!!");
}

void VdQueryVideoMode(XVIDEO_MODE* vm)
{
    memset(vm, 0, sizeof(XVIDEO_MODE));
    vm->DisplayWidth = 1280;
    vm->DisplayHeight = 720;
    vm->IsInterlaced = false;
    vm->IsWidescreen = true;
    vm->IsHighDefinition = true;
    vm->RefreshRate = 0x42700000;
    vm->VideoStandard = 1;
    vm->Unknown4A = 0x4A;
    vm->Unknown01 = 0x01;
    LOGF_INFO("[TRACE] VdQueryVideoMode() -> 1280x720 60Hz");
}

void VdGetCurrentDisplayInformation()
{
    LOG_UTILITY("!!! STUB !!!");
}

void VdSetDisplayMode()
{
    LOG_UTILITY("!!! STUB !!!");
}

// =============================================================================
// VBlank Timer - ENABLED for GTA V (needs periodic VBlank signals)
// =============================================================================
static std::atomic<bool> g_vblankTimerRunning{false};
static std::thread g_vblankThread;

// Forward declaration for main loop entry

void FireVBlankCallback() {
    if (g_gpuRingBuffer.interruptCallback != 0) {
        static std::mutex s_vblankMutex;
        std::unique_lock<std::mutex> lock(s_vblankMutex, std::try_to_lock);
        if (!lock.owns_lock()) {
            return; // Another thread is already firing VBlank callback
        }
        static std::atomic<uint32_t> s_callbackCount{0};
        const uint32_t callbackCount = ++s_callbackCount;
        static std::unique_ptr<GuestThreadContext> s_vblankContext;
        if (!s_vblankContext) {
            s_vblankContext = std::make_unique<GuestThreadContext>(5);
        }

        // Set up context with valid stack and TLS on the dedicated vblank context
        PPCContext ctx = s_vblankContext->ppcContext;
        ctx.r3.u32 = 0;  // Xenon graphics interrupt source 0 = VBlank
        ctx.r4.u32 = g_gpuRingBuffer.interruptUserData;  // Context/userData
        if (ctx.r13.u32 < PPC_MEMORY_SIZE - 269)
            *reinterpret_cast<uint8_t*>(g_memory.Translate(ctx.r13.u32 + 268)) = 2;
        const uint32_t source = ctx.r3.u32;
        PPCContext* previousContext = GetPPCContext();
        SetPPCContext(ctx);
        
        auto* callbackFunc = g_memory.FindFunction(g_gpuRingBuffer.interruptCallback);
        if (callbackFunc) {
            callbackFunc(ctx, g_memory.base);
            if (callbackCount <= 5) {
                LOGF_INFO("[VBlank] callback #{} addr=0x{:08X} source={} userData=0x{:08X} ret=0x{:08X}",
                          callbackCount, g_gpuRingBuffer.interruptCallback,
                          source, g_gpuRingBuffer.interruptUserData,
                          static_cast<uint32_t>(ctx.r3.u32));
            }
        } else if (callbackCount <= 5) {
            LOGF_WARNING("[VBlank] callback #{} unmapped addr=0x{:08X} userData=0x{:08X}",
                         callbackCount, g_gpuRingBuffer.interruptCallback,
                         g_gpuRingBuffer.interruptUserData);
        }

        if (previousContext)
            SetPPCContext(*previousContext);
        else
            g_ppcContext = nullptr;

        // VBlank callback complete - game code handles render loop naturally
    }
}

void VBlankTimerThread() {
    while (g_vblankTimerRunning.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60Hz
        FireVBlankCallback();

        // Process any pending PM4 packets in the ring buffer
        ScanRingBufferForPM4Packets();

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastVdSwapTime).count() >= 32) {
            Video::Present();
        }

        static auto s_lastDiagDumpTime = std::chrono::steady_clock::now();
        if (now - s_lastDiagDumpTime >= std::chrono::seconds(5)) {
            s_lastDiagDumpTime = now;

            std::stringstream ss;
            ss << "\n================ [DIAGNOSTIC STATUS DUMP] ================\n";
            ss << "Counters: Frames=" << g_mainFrameCounter.load(std::memory_order_relaxed)
               << ", Presents=" << g_frameCounter.load(std::memory_order_relaxed)
               << ", VdSwap=" << g_vdSwapCounter.load(std::memory_order_relaxed)
               << ", PM4=" << g_pm4PacketCounter.load(std::memory_order_relaxed) << "\n";

            // Recent files
            {
                std::lock_guard<std::mutex> lock(g_recentFilesMutex);
                ss << "Last Opened Files (" << g_recentFiles.size() << "):\n";
                for (const auto& file : g_recentFiles) {
                    ss << "  - " << file << "\n";
                }
            }

            // Active thread waits
            {
                std::lock_guard<std::mutex> lock(g_waitTrackerMutex);
                ss << "Active Thread Waits (" << g_activeThreadWaits.size() << "):\n";
                for (const auto& [tid, waitInfo] : g_activeThreadWaits) {
                    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - waitInfo.startTime).count();
                    bool isMain = (g_kernelMainThreadIdSet && tid == g_kernelMainThreadId);
                    ss << "  Thread [tid=" << tid << (isMain ? " (MAIN)" : "") << "] "
                       << waitInfo.waitType << "(0x" << std::hex << waitInfo.handleOrObj << std::dec << ") "
                       << "callerLR=0x" << std::hex << waitInfo.callerLR << std::dec
                       << " timeout=" << waitInfo.timeoutMs << "ms, elapsed=" << elapsedMs << "ms\n";
                }
            }
            ss << "========================================================\n";
            LOGF_WARNING("{}", ss.str());
#ifndef _WIN32
            if (g_kernelMainThreadIdSet.load()) {
                pthread_kill(g_kernelMainPthread, SIGUSR1);
            }
#endif
        }
    }
}

void StartVBlankTimer() {
    if (!g_vblankTimerRunning.load()) {
        g_vblankTimerRunning.store(true);
        g_vblankThread = std::thread(VBlankTimerThread);
        LOGF_INFO("[VBlank] Timer started (60Hz)");
    }
}

void StopVBlankTimer() {
    if (g_vblankTimerRunning.load()) {
        g_vblankTimerRunning.store(false);
        if (g_vblankThread.joinable()) {
            g_vblankThread.join();
        }
        LOGF_INFO("[VBlank] Timer stopped");
    }
}

void VdSetGraphicsInterruptCallback(uint32_t callback, uint32_t userData)
{
    g_gpuRingBuffer.interruptCallback = callback;
    g_gpuRingBuffer.interruptUserData = userData;
    
    LOGF_INFO("[VdSetGraphicsInterruptCallback] callback=0x{:08X} userData=0x{:08X}", callback, userData);
    
    StartVBlankTimer();
}

uint32_t VdInitializeEngines()
{
    g_gpuRingBuffer.enginesInitialized = true;
    LOGF_INFO("[TRACE] VdInitializeEngines() -> SUCCESS");
    return 1;
}

uint32_t VdIsHSIOTrainingSucceeded()
{
    // Return 1 to indicate HSIO training succeeded - this unblocks GPU init polling
    return 1;
}

void VdGetCurrentDisplayGamma()
{
    LOG_UTILITY("!!! STUB !!!");
}

void VdQueryVideoFlags()
{
    LOG_UTILITY("!!! STUB !!!");
}

void VdCallGraphicsNotificationRoutines(uint32_t type, uint32_t data)
{
    static uint32_t s_notifCount = 0;
    if (++s_notifCount <= 5 || (s_notifCount % 60) == 0)
    {
        LOGF_UTILITY("type={} data={}", type, data);
    }
}

void VdInitializeScalerCommandBuffer()
{
    LOG_UTILITY("!!! STUB !!!");
}

void KeLeaveCriticalRegion()
{
    // Critical region management for APC protection.
    // Since APCs are not fully implemented, this is a no-op.
}

uint32_t VdRetrainEDRAM()
{
    // Set the EDRAM training complete flag - the GPU poll loop waits for this
    g_gpuRingBuffer.edramTrainingComplete = true;
    
    static int s_callCount = 0;
    if (++s_callCount <= 3)
    {
        LOGF_UTILITY("edramTrainingComplete = true (call #{})", s_callCount);
    }
    
    // DISABLED: Event signaling - MarathonRecomp style
    // Sync primitives work via fail-open during init, proper waits during runtime
    
    return 0;  // Success
}

uint32_t VdRetrainEDRAMWorker(uint32_t unk0)
{
    // This is called by the GPU init polling loop
    // Must set persistent state, not just return success
    g_gpuRingBuffer.edramTrainingComplete = true;
    
    static int s_callCount = 0;
    if (++s_callCount <= 3)
    {
        LOGF_UTILITY("unk0=0x{:08X} - edramTrainingComplete = true (call #{})", unk0, s_callCount);
    }
    return 0;  // Success
}

void KeEnterCriticalRegion()
{
    // Critical region management for APC protection.
    // Since APCs are not fully implemented, this is a no-op.
}

uint32_t MmAllocatePhysicalMemoryEx
(
    uint32_t flags,
    uint32_t size,
    uint32_t protect,
    uint32_t minAddress,
    uint32_t maxAddress,
    uint32_t alignment
)
{
    static std::unordered_set<uint32_t> s_seenCallers;
    const uint32_t lr = GET_CALLER_LR();
    if (lr != 0 && s_seenCallers.insert(lr).second)
    {
        LOGF_UTILITY(
            "caller_lr=0x{:08X} flags=0x{:X} size=0x{:X} prot=0x{:X} min=0x{:08X} max=0x{:08X} align=0x{:X}",
            lr, flags, size, protect, minAddress, maxAddress, alignment);
    }
    void* host = g_userHeap.AllocPhysical(size, alignment);
    if (host == nullptr)
        return 0;

    return g_memory.MapVirtual(host);
}

void ObDeleteSymbolicLink()
{
    LOG_UTILITY("!!! STUB !!!");
}

void ObCreateSymbolicLink()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t MmQueryAddressProtect(uint32_t guestAddress)
{
    return PAGE_READWRITE;
}

void VdEnableDisableClockGating()
{
    LOG_UTILITY("!!! STUB !!!");
}

void KeBugCheck(uint32_t bugCheckCode)
{
    LOGF_ERROR("[KeBugCheck] code=0x{:08X} ({})", bugCheckCode, bugCheckCode);
}

void KeLockL2()
{
    LOG_UTILITY("!!! STUB !!!");
}

void KeUnlockL2()
{
    LOG_UTILITY("!!! STUB !!!");
}

bool KeSetEvent(XKEVENT* pEvent, uint32_t Increment, bool Wait)
{
    if (!pEvent) {
        LOGF_WARNING("[TRACE] KeSetEvent: null pEvent!");
        return false;
    }
    // SONIC APPROACH: Direct kernel object signal - no SyncTable
    static std::atomic<int> s_setEventCount{0};
    if (s_setEventCount++ < 30) {
        LOGF_INFO("[TRACE] KeSetEvent: event=0x{:08X}", (uint32_t)((uint8_t*)pEvent - g_memory.base));
    }
    Event* evt = QueryKernelObject<Event>(*pEvent);
    if (!evt) return false;
    bool result = evt->Set();

    ++g_keSetEventGeneration;
    g_keSetEventGeneration.notify_all();

    return result;
}

bool KeResetEvent(XKEVENT* pEvent)
{
    if (!pEvent) return false;
    Event* evt = QueryKernelObject<Event>(*pEvent);
    if (!evt) return false;
    return evt->Reset();
}

// Track recent async reads for correlation with waits
static std::atomic<uint32_t> g_lastAsyncInfo{0};
static std::atomic<int> g_asyncReadCount{0};

// =============================================================================
// WAIT TRACKING: Find the exact handle blocking Frame 1
// This tracks repeated waits on the same handle to identify the missing signal
// =============================================================================
static std::mutex g_waitTrackMutex;
static std::unordered_map<uint32_t, int> g_handleWaitCounts;  // handle -> count
static uint32_t g_mostWaitedHandle = 0;
static int g_mostWaitedCount = 0;

uint32_t KeWaitForSingleObject(XDISPATCHER_HEADER* Object, uint32_t WaitReason, uint32_t WaitMode, bool Alertable, be<int64_t>* Timeout)
{
    // SONIC APPROACH: Simple direct kernel object wait - no SyncTable
    const uint32_t timeout = GuestTimeoutToMilliseconds(Timeout);
    static std::atomic<int> s_keWaitCount{0};
    PPCContext* ppcContext = GetPPCContext();
    const auto stackTrace = CaptureGuestStackTrace();
    uint32_t callerLR = GET_CALLER_LR();
    ScopedWaitTracker waitTracker("KeWaitForSingleObject", Object ? (uint32_t)((uint8_t*)Object - g_memory.base) : 0, timeout, callerLR);
    if (s_keWaitCount++ < 50 || s_keWaitCount % 200 == 0) {
        LOGF_INFO("[TRACE] KeWaitForSingleObject: #{} obj=0x{:08X} type={} signalState={} timeout={}ms caller=0x{:08X} sp=0x{:08X} tls=0x{:08X} stack={:08X},{:08X},{:08X},{:08X}",
                  s_keWaitCount.load(),
                  Object ? (uint32_t)((uint8_t*)Object - g_memory.base) : 0,
                  Object ? Object->Type : -1,
                  Object ? static_cast<int32_t>(Object->SignalState) : -1, timeout, callerLR,
                  ppcContext ? ppcContext->r1.u32 : 0u, ppcContext ? ppcContext->r13.u32 : 0u,
                  stackTrace[0], stackTrace[1], stackTrace[2], stackTrace[3]);
    }

    if (!Object)
        return STATUS_INVALID_PARAMETER;

    // D3D Present VBlank/Flip synchronization handshake:
    // 1) 0x836BEF3C: Initial wait in sub_836BEEA0. Return STATUS_TIMEOUT (258)
    //    so the routine immediately dispatches loc_836BEF5C to execute sub_822D41E8 (VdSwap).
    if (callerLR == 0x836BEF3C)
    {
        return 258; // STATUS_TIMEOUT
    }

    // 2) 0x836BEFF0: Post-VdSwap wait in sub_836BEEA0. Return STATUS_SUCCESS (0)
    //    so the routine proceeds to loc_836BF014 (flip complete).
    if (callerLR == 0x836BEFF0)
    {
        return 0; // STATUS_SUCCESS
    }

    switch (Object->Type)
    {
        case 0:
        case 1: {
            auto* evt = QueryKernelObject<Event>(*Object);
            if (!IsValidHostVtable(evt)) return ShouldFailOpenWait() ? STATUS_SUCCESS : STATUS_TIMEOUT;
            return evt->Wait(timeout);
        }

        case 5: {
            auto* sem = QueryKernelObject<Semaphore>(*Object);
            if (!IsValidHostVtable(sem)) return ShouldFailOpenWait() ? STATUS_SUCCESS : STATUS_TIMEOUT;
            return sem->Wait(timeout);
        }

        default:
            LOGF_IMPL(Utility, "KeWaitForSingleObject", "Unrecognized kernel object type: {}", Object->Type);
            return STATUS_TIMEOUT;
    }
}

// =============================================================================
// TLS UNIFICATION: Route KeTls* APIs through Guest Memory (r13 + X360_TLS_OFFSET)
// =============================================================================
// Problem: Previously, KeTlsGetValue/SetValue used host thread_local storage,
// which was completely disconnected from the guest memory TLS at r13+0xAB0.
// This caused state mismatches where game code and kernel APIs saw different TLS.
//
// Solution: Unified TLS - all TLS access goes through guest memory.
// - Guest threads: read/write from r13 + X360_TLS_OFFSET + (index * 4)
// - Non-guest threads (VBlank, etc.): fallback to host thread_local storage
// =============================================================================

static std::vector<size_t> g_tlsFreeIndices;
static size_t g_tlsNextIndex = 0;
static Mutex g_tlsAllocationMutex;

// Host-side TLS fallback for non-guest threads (VBlank callback thread, etc.)
static uint32_t& KeTlsGetValueRef_Host(size_t index)
{
    thread_local std::vector<uint32_t> s_tlsValues;
    if (s_tlsValues.size() <= index)
    {
        s_tlsValues.resize(index + 1, 0);
    }
    return s_tlsValues[index];
}

// Maximum TLS slots in guest memory (X360_TLS_SIZE = 0x100 = 256 bytes = 64 uint32_t slots)
static constexpr uint32_t MAX_GUEST_TLS_SLOTS = X360_TLS_SIZE / sizeof(uint32_t);

uint32_t KeTlsGetValue(uint32_t dwTlsIndex)
{
    // Get current thread's PPC context
    PPCContext* ctx = GetPPCContext();
    
    // If no PPC context or r13 is invalid, fall back to host TLS
    // This handles non-guest threads like VBlank callback thread
    if (!ctx || ctx->r13.u32 == 0 || ctx->r13.u32 < 0x80000000)
    {
        return KeTlsGetValueRef_Host(dwTlsIndex);
    }
    
    // Validate index - guest TLS has limited slots
    if (dwTlsIndex >= MAX_GUEST_TLS_SLOTS)
    {
        // For indices beyond guest TLS capacity, use host fallback
        return KeTlsGetValueRef_Host(dwTlsIndex);
    }
    
    // Read from guest memory: r13 + X360_TLS_OFFSET + (index * 4)
    // X360_TLS_OFFSET = 0xAB0 (defined in guest_thread.h)
    uint32_t tlsAddr = ctx->r13.u32 + X360_TLS_OFFSET + (dwTlsIndex * sizeof(uint32_t));
    // Direct guest memory access (can't use PPC_LOAD_U32 macro - no 'base' in scope)
    uint32_t* ptr = reinterpret_cast<uint32_t*>(g_memory.base + tlsAddr);
    return __builtin_bswap32(*ptr);  // Big-endian to host
}

uint32_t KeTlsSetValue(uint32_t dwTlsIndex, uint32_t lpTlsValue)
{
    static std::atomic<int> s_tlsSetCount{0};
    if (s_tlsSetCount++ < 50) {
        LOGF_INFO("[TRACE] KeTlsSetValue: slot={} val=0x{:08X}", dwTlsIndex, lpTlsValue);
    }
    PPCContext* ctx = GetPPCContext();
    
    if (!ctx || ctx->r13.u32 == 0 || ctx->r13.u32 < 0x80000000)
    {
        KeTlsGetValueRef_Host(dwTlsIndex) = lpTlsValue;
        return TRUE;
    }
    
    if (dwTlsIndex >= MAX_GUEST_TLS_SLOTS)
    {
        KeTlsGetValueRef_Host(dwTlsIndex) = lpTlsValue;
        return TRUE;
    }
    
    // Write to guest memory: r13 + X360_TLS_OFFSET + (index * 4)
    uint32_t tlsAddr = ctx->r13.u32 + X360_TLS_OFFSET + (dwTlsIndex * sizeof(uint32_t));
    // Direct guest memory access (can't use PPC_STORE_U32 macro - no 'base' in scope)
    uint32_t* ptr = reinterpret_cast<uint32_t*>(g_memory.base + tlsAddr);
    *ptr = __builtin_bswap32(lpTlsValue);  // Host to big-endian
    return TRUE;
}

uint32_t KeTlsAlloc()
{
    std::lock_guard<Mutex> lock(g_tlsAllocationMutex);
    
    // Try to reuse a freed slot first
    if (!g_tlsFreeIndices.empty())
    {
        size_t index = g_tlsFreeIndices.back();
        g_tlsFreeIndices.pop_back();
        LOGF_INFO("[TRACE] KeTlsAlloc() -> slot {}", index);
        return static_cast<uint32_t>(index);
    }
    
    // Allocate new slot
    size_t index = g_tlsNextIndex++;
    LOGF_INFO("[TRACE] KeTlsAlloc() -> slot {}", index);
    
    // Warn if exceeding guest TLS capacity (still works via host fallback)
    if (index >= MAX_GUEST_TLS_SLOTS)
    {
        static bool warned = false;
        if (!warned)
        {
            LOGF_WARNING("[TLS] Allocated slot {} exceeds guest TLS capacity ({}), using host fallback",
                         index, MAX_GUEST_TLS_SLOTS);
            warned = true;
        }
    }
    
    return static_cast<uint32_t>(index);
}

uint32_t KeTlsFree(uint32_t dwTlsIndex)
{
    std::lock_guard<Mutex> lock(g_tlsAllocationMutex);
    g_tlsFreeIndices.push_back(dwTlsIndex);
    return TRUE;
}

uint32_t XMsgInProcessCall(uint32_t app, uint32_t message, be<uint32_t>* param1, be<uint32_t>* param2)
{
    if (message == 0x7001B)
    {
        uint32_t* ptr = (uint32_t*)g_memory.Translate(param1[1]);
        ptr[0] = 0;
        ptr[1] = 0;
    }

    return 0;
}

void XamUserReadProfileSettings
(
    uint32_t titleId,
    uint32_t userIndex,
    uint32_t xuidCount,
    uint64_t* xuids,
    uint32_t settingCount,
    uint32_t* settingIds,
    be<uint32_t>* bufferSize,
    void* buffer,
    void* overlapped
)
{
    if (buffer != nullptr)
    {
        memset(buffer, 0, *bufferSize);
    }
    else
    {
        *bufferSize = 4;
    }
}

// =============================================================================
// Network Socket Functions - Now implemented in kernel/io/net_socket.cpp
// =============================================================================
// The following functions were stubs and are now replaced with real implementations:
// NetDll_WSAStartup, NetDll_WSACleanup, NetDll_socket, NetDll_closesocket,
// NetDll_setsockopt, NetDll_bind, NetDll_connect, NetDll_listen, NetDll_accept,
// NetDll_select, NetDll_recv, NetDll_send, NetDll_inet_addr
// =============================================================================

void NetDll___WSAFDIsSet()
{
    LOG_UTILITY("!!! STUB !!!");
}

void XMsgStartIORequestEx()
{
    LOG_UTILITY("!!! STUB !!!");
}

void XexGetModuleHandle()
{
    LOG_UTILITY("!!! STUB !!!");
}

bool RtlTryEnterCriticalSection(XRTL_CRITICAL_SECTION* cs)
{
    if (!cs)
        return false;

    if (g_memory.MapVirtual(cs) < 0x10000000)
        return false;

    // printf("RtlTryEnterCriticalSection\n");
    be<uint32_t> thisThread =
        (g_ppcContext && g_ppcContext->r13.u32 != 0) ? g_ppcContext->r13.u32 : 1;

    std::atomic_ref owningThread(cs->OwningThread);

    be<uint32_t> previousOwner = 0;

    if (owningThread.compare_exchange_weak(previousOwner, thisThread) ||
        previousOwner == thisThread)
    {
        cs->RecursionCount = cs->RecursionCount.get() + 1;
        return true;
    }

    return false;
}

void RtlInitializeCriticalSectionAndSpinCount(XRTL_CRITICAL_SECTION* cs, uint32_t spinCount)
{
    if (!cs)
        return;

    if (g_memory.MapVirtual(cs) < 0x10000000)
        return;

    // printf("RtlInitializeCriticalSectionAndSpinCount\n");
    cs->Header.Absolute = (spinCount + 255) >> 8;
    cs->LockCount = -1;
    cs->RecursionCount = 0;
    cs->OwningThread = 0;
}

void _vswprintf_x()
{
    LOG_UTILITY("!!! STUB !!!");
}

void _vscwprintf_x()
{
    LOG_UTILITY("!!! STUB !!!");
}

void _swprintf_x()
{
    LOG_UTILITY("!!! STUB !!!");
}

void _snwprintf_x()
{
    LOG_UTILITY("!!! STUB !!!");
}

void XeCryptBnQwBeSigVerify()
{
    LOG_UTILITY("!!! STUB !!!");
}

void XeKeysGetKey()
{
    LOG_UTILITY("!!! STUB !!!");
}

void XeCryptRotSumSha()
{
    LOG_UTILITY("!!! STUB !!!");
}

void XeCryptSha()
{
    LOG_UTILITY("!!! STUB !!!");
}

void KeEnableFpuExceptions()
{
    LOG_UTILITY("!!! STUB !!!");
}

void RtlUnwind_x()
{
    LOG_UTILITY("!!! STUB !!!");
}

void RtlCaptureContext_x()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t NtQueryFullAttributesFile(XOBJECT_ATTRIBUTES* ObjectAttributes, XFILE_NETWORK_OPEN_INFORMATION* FileInformation)
{
    if (!ObjectAttributes || !FileInformation)
        return STATUS_INVALID_PARAMETER;

    std::string guestPath;
    if (!TryGetAnsiPath(ObjectAttributes, guestPath))
        return STATUS_INVALID_PARAMETER;

    const std::filesystem::path resolved = ResolveGuestPathBestEffort(guestPath);

    std::error_code ec;
    const bool exists = std::filesystem::exists(resolved, ec);
    if (!exists || ec)
        return STATUS_NO_SUCH_FILE;

    const bool isDir = std::filesystem::is_directory(resolved, ec);
    if (ec)
        return STATUS_FAIL_CHECK;

    uint64_t fileSize = 0;
    if (!isDir)
    {
        fileSize = std::filesystem::file_size(resolved, ec);
        if (ec)
            fileSize = 0;
    }

    const uint64_t allocationSize = RoundUpToPage(fileSize);
    const uint32_t attrs = GetFileAttributesBestEffort(resolved);

    FileInformation->CreationTime = 0;
    FileInformation->LastAccessTime = 0;
    FileInformation->LastWriteTime = 0;
    FileInformation->ChangeTime = 0;
    FileInformation->AllocationSize = static_cast<int64_t>(allocationSize);
    FileInformation->EndOfFile = static_cast<int64_t>(fileSize);
    FileInformation->FileAttributes = attrs;
    FileInformation->Reserved = 0;

    return STATUS_SUCCESS;
}

uint32_t RtlMultiByteToUnicodeN(be<uint16_t>* UnicodeString, uint32_t MaxBytesInUnicodeString, be<uint32_t>* BytesInUnicodeString, const char* MultiByteString, uint32_t BytesInMultiByteString)
{
    uint32_t length = std::min(MaxBytesInUnicodeString / 2, BytesInMultiByteString);

    for (size_t i = 0; i < length; i++)
        UnicodeString[i] = MultiByteString[i];

    if (BytesInUnicodeString != nullptr)
        *BytesInUnicodeString = length * 2;

    return STATUS_SUCCESS;
}

void DbgBreakPoint()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t MmQueryAllocationSize(uint32_t guestAddress)
{
    if (guestAddress == 0)
        return 0;
    
    return (uint32_t)g_userHeap.Size(g_memory.Translate(guestAddress));
}

uint32_t NtClearEvent(uint32_t Handle, uint32_t* previousState)
{
    Event* evt = nullptr;
    if (IsValidKernelHandle(Handle))
    {
        evt = GetKernelObject<Event>(Handle);
    }
    else if (Handle >= 0x10000 && Handle <= 0xF0000000 && g_memory.base != nullptr)
    {
        XDISPATCHER_HEADER* header = reinterpret_cast<XDISPATCHER_HEADER*>(g_memory.Translate(Handle));
        if (header->Type == 0 || header->Type == 1)
        {
            evt = QueryKernelObject<Event>(*header);
        }
    }
    else if (Handle != 0 && Handle != 0xFFFFFFFF && g_memory.base != nullptr)
    {
        void* hostPtr = g_memory.Translate(Handle);
        if (IsValidHostVtable(hostPtr))
        {
            evt = static_cast<Event*>(hostPtr);
        }
    }

    if (!evt || !IsValidHostVtable(evt)) {
        return 0xC0000008; // STATUS_INVALID_HANDLE
    }
    evt->Reset();
    return 0;
}

uint32_t NtResumeThread(uint32_t threadHandle, be<uint32_t>* suspendCount)
{
    if (suspendCount != nullptr)
        *suspendCount = 1;

    if (IsKernelObject(threadHandle)) {
        GuestThreadHandle* hThread = GetKernelObject<GuestThreadHandle>(threadHandle);
        if (hThread) {
            LOGF_INFO("[TRACE] NtResumeThread: handle=0x{:08X} (resuming)", threadHandle);
            hThread->suspended = false;
            hThread->suspended.notify_all();
            return S_OK;
        }
    }

    LOGF_WARNING("[TRACE] NtResumeThread: handle=0x{:08X} not found", threadHandle);
    return S_OK;
}

uint32_t NtSetEvent(uint32_t Handle, uint32_t* previousState)
{
    Event* evt = nullptr;
    if (IsValidKernelHandle(Handle))
    {
        evt = GetKernelObject<Event>(Handle);
    }
    else if (Handle >= 0x10000 && Handle <= 0xF0000000 && g_memory.base != nullptr)
    {
        XDISPATCHER_HEADER* header = reinterpret_cast<XDISPATCHER_HEADER*>(g_memory.Translate(Handle));
        if (header->Type == 0 || header->Type == 1)
        {
            evt = QueryKernelObject<Event>(*header);
        }
    }
    else if (Handle != 0 && Handle != 0xFFFFFFFF && g_memory.base != nullptr)
    {
        void* hostPtr = g_memory.Translate(Handle);
        if (IsValidHostVtable(hostPtr))
        {
            evt = static_cast<Event*>(hostPtr);
        }
    }

    if (!evt || !IsValidHostVtable(evt)) {
        return 0xC0000008; // STATUS_INVALID_HANDLE
    }
    evt->Set();
    return 0;
}

uint32_t NtCreateSemaphore(be<uint32_t>* Handle, XOBJECT_ATTRIBUTES* ObjectAttributes, uint32_t InitialCount, uint32_t MaximumCount)
{
    static int s_count = 0; ++s_count;
    PPCContext* ppcContext = GetPPCContext();
    const uint32_t callerLR = GET_CALLER_LR();
    const auto stackTrace = CaptureGuestStackTrace();
    const uint32_t outputGuestAddress =
        Handle != nullptr && g_memory.IsInMemoryRange(Handle)
            ? g_memory.MapVirtual(Handle)
            : 0;
    
    Semaphore* sem = CreateKernelObject<Semaphore>(InitialCount, MaximumCount);
    *Handle = GetKernelHandle(sem);
    uint32_t handle = *Handle;
    
    // Track blocking semaphores (count=0) for signaling after init
    if (InitialCount == 0) {
        // Store the kernel handle for later signaling
        std::lock_guard<std::mutex> lock(g_semaphoreTrackMutex);
        g_blockingSemaphoreAddrs.insert(handle);  // Track handle directly
        LOGF_WARNING("[NtCreateSemaphore] #{} handle=0x{:08X} out=0x{:08X} count=0 max={} caller=0x{:08X} sp=0x{:08X} tls=0x{:08X} stack={:08X},{:08X},{:08X},{:08X}",
                     s_count, handle, outputGuestAddress, MaximumCount, callerLR,
                     ppcContext ? ppcContext->r1.u32 : 0u, ppcContext ? ppcContext->r13.u32 : 0u,
                     stackTrace[0], stackTrace[1], stackTrace[2], stackTrace[3]);
    } else if (s_count <= 50) {
        LOGF_WARNING("[NtCreateSemaphore] #{} handle=0x{:08X} out=0x{:08X} count={} max={} caller=0x{:08X} sp=0x{:08X} tls=0x{:08X} stack={:08X},{:08X},{:08X},{:08X}",
                     s_count, handle, outputGuestAddress, InitialCount, MaximumCount, callerLR,
                     ppcContext ? ppcContext->r1.u32 : 0u, ppcContext ? ppcContext->r13.u32 : 0u,
                     stackTrace[0], stackTrace[1], stackTrace[2], stackTrace[3]);
    }
    
    return STATUS_SUCCESS;
}

uint32_t NtReleaseSemaphore(uint32_t Handle, uint32_t ReleaseCount, int32_t* PreviousCount)
{
    static int s_count = 0;
    ++s_count;
    
    KernelObject* obj = nullptr;
    if (IsValidKernelHandle(Handle))
    {
        obj = GetKernelObject(Handle);
    }
    else if (Handle >= 0x10000 && Handle <= 0xF0000000 && g_memory.base != nullptr)
    {
        XDISPATCHER_HEADER* header = reinterpret_cast<XDISPATCHER_HEADER*>(g_memory.Translate(Handle));
        if (header->Type == 5)
        {
            obj = QueryKernelObject<Semaphore>(*header);
        }
        else if (header->Type == 0 || header->Type == 1)
        {
            obj = QueryKernelObject<Event>(*header);
        }
        else if (header->WaitListHead.Flink == OBJECT_SIGNATURE)
        {
            uint32_t blink = header->WaitListHead.Blink.get();
            if (IsValidKernelHandle(blink))
            {
                obj = GetKernelObject(blink);
            }
        }
    }

    if (!IsValidHostVtable(obj))
    {
        if (s_count <= 20 || s_count % 100 == 0)
        {
            LOGF_WARNING("[NtReleaseSemaphore] #{} Invalid or unresolvable handle 0x{:08X} callerLR=0x{:08X}", s_count, Handle, GET_CALLER_LR());
        }
        return STATUS_INVALID_HANDLE;
    }
    PPCContext* ppcContext = GetPPCContext();
    const uint32_t callerLR = GET_CALLER_LR();
    const auto stackTrace = CaptureGuestStackTrace();
    // Check if this is actually a semaphore
    Semaphore* sem = dynamic_cast<Semaphore*>(obj);
    if (!sem)
    {
        // Not a semaphore - might be an event, try to signal it as an event
        Event* evt = dynamic_cast<Event*>(obj);
        if (evt)
        {
            if (s_count <= 30 || s_count % 100 == 0)
            {
                LOGF_WARNING("[NtReleaseSemaphore] #{} handle=0x{:08X} is EVENT, signaling as event", s_count, Handle);
            }
            evt->Set();
            return STATUS_SUCCESS;
        }
        
        LOGF_WARNING("[NtReleaseSemaphore] #{} handle=0x{:08X} is not a semaphore or event", s_count, Handle);
        return STATUS_INVALID_HANDLE;  // Object type mismatch
    }
    
    // Always log releases for blocking semaphores, otherwise limit logging
    bool isBlockingSemaphore = (Handle >= 0xEB2D0000 && Handle <= 0xEB2E0000);
    if (s_count <= 30 || isBlockingSemaphore || s_count % 100 == 0)
    {
        uint32_t curCount = sem->count.load();
        std::stringstream ssTid;
        ssTid << std::this_thread::get_id();
        LOGF_WARNING("[NtReleaseSemaphore] #{} handle=0x{:08X} release={} count={}/{}{} tid={} caller=0x{:08X} sp=0x{:08X} tls=0x{:08X} stack={:08X},{:08X},{:08X},{:08X}",
                  s_count, Handle, ReleaseCount, curCount, sem->maximumCount,
                  isBlockingSemaphore ? " *** BLOCKING SEM ***" : "", ssTid.str(), callerLR,
                  ppcContext ? ppcContext->r1.u32 : 0u, ppcContext ? ppcContext->r13.u32 : 0u,
                  stackTrace[0], stackTrace[1], stackTrace[2], stackTrace[3]);
    }
    
    // the game releases semaphore with 1 maximum number of releases more than once
    if (sem->count + ReleaseCount > sem->maximumCount)
        return STATUS_SEMAPHORE_LIMIT_EXCEEDED;

    uint32_t previousCount;
    sem->Release(ReleaseCount, &previousCount);

    if (PreviousCount != nullptr)
        *PreviousCount = ByteSwap(previousCount);

    return STATUS_SUCCESS;
}

void NtWaitForMultipleObjectsEx()
{
    LOG_UTILITY("!!! STUB !!!");
}

void RtlCompareStringN()
{
    LOG_UTILITY("!!! STUB !!!");
}

void _snprintf_x()
{
    LOG_UTILITY("!!! STUB !!!");
}

void StfsControlDevice()
{
    LOG_UTILITY("!!! STUB !!!");
}

void StfsCreateDevice()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t NtFlushBuffersFile(uint32_t FileHandle, XIO_STATUS_BLOCK* IoStatusBlock)
{
    if (IoStatusBlock)
    {
        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = 0;
    }
    return STATUS_SUCCESS;
}

void KeQuerySystemTime(be<uint64_t>* time)
{
    // Pump SDL events periodically to prevent window from becoming unresponsive
    PumpSdlEventsIfNeeded();
    
    constexpr int64_t FILETIME_EPOCH_DIFFERENCE = 116444736000000000LL;

    auto now = std::chrono::system_clock::now();
    auto timeSinceEpoch = now.time_since_epoch();

    int64_t currentTime100ns = std::chrono::duration_cast<std::chrono::duration<int64_t, std::ratio<1, 10000000>>>(timeSinceEpoch).count();
    currentTime100ns += FILETIME_EPOCH_DIFFERENCE;

    *time = currentTime100ns;
}

struct TIME_FIELDS {
    be<uint16_t> Year;
    be<uint16_t> Month;
    be<uint16_t> Day;
    be<uint16_t> Hour;
    be<uint16_t> Minute;
    be<uint16_t> Second;
    be<uint16_t> Milliseconds;
    be<uint16_t> Weekday;
};

void RtlTimeToTimeFields(const be<uint64_t>* time, TIME_FIELDS* timeFields)
{
    constexpr uint64_t TICKS_PER_MILLISECOND = 10000;
    constexpr uint64_t TICKS_PER_SECOND = 10000000;
    constexpr uint64_t TICKS_PER_MINUTE = 600000000;
    constexpr uint64_t TICKS_PER_HOUR = 36000000000;
    constexpr uint64_t TICKS_PER_DAY = 864000000000;

    static const int DaysInMonth[2][12] = {
            {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}, // Non-leap
            {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}  // Leap
    };

    // Calculate total days since January 1, 1601
    uint64_t days = *time / TICKS_PER_DAY;
    uint64_t remainingTicks = *time % TICKS_PER_DAY;

    timeFields->Hour = static_cast<uint16_t>(remainingTicks / TICKS_PER_HOUR);
    remainingTicks %= TICKS_PER_HOUR;

    timeFields->Minute = static_cast<uint16_t>(remainingTicks / TICKS_PER_MINUTE);
    remainingTicks %= TICKS_PER_MINUTE;

    timeFields->Second = static_cast<uint16_t>(remainingTicks / TICKS_PER_SECOND);
    remainingTicks %= TICKS_PER_SECOND;

    timeFields->Milliseconds = static_cast<uint16_t>(remainingTicks / TICKS_PER_MILLISECOND);

    // Calculate day of week (January 1, 1601 was a Monday = 1)
    timeFields->Weekday = static_cast<uint16_t>((days + 1) % 7);

    // Calculate year
    uint32_t year = 1601;

    // Each 400-year cycle has 146097 days
    uint32_t cycles400 = static_cast<uint32_t>(days / 146097);
    days %= 146097;
    year += cycles400 * 400;

    // Handle 100-year cycles (24 leap years + 76 normal years = 36524 days)
    // Except the 4th century which has 36525 days
    uint32_t cycles100 = static_cast<uint32_t>(days / 36524);
    if (cycles100 == 4) cycles100 = 3; // Last day of 400-year cycle
    days -= cycles100 * 36524;
    year += cycles100 * 100;

    // Handle 4-year cycles (1 leap year + 3 normal years = 1461 days)
    uint32_t cycles4 = static_cast<uint32_t>(days / 1461);
    days %= 1461;
    year += cycles4 * 4;

    // Handle individual years within 4-year cycle
    uint32_t yearInCycle = static_cast<uint32_t>(days / 365);
    if (yearInCycle == 4) yearInCycle = 3; // Last day of leap cycle
    days -= yearInCycle * 365;
    if (yearInCycle > 0) {
        // Account for leap days in previous years of this cycle
        days -= (yearInCycle - 1) / 4;
    }
    year += yearInCycle;

    timeFields->Year = static_cast<uint16_t>(year);

    // Determine if current year is a leap year
    bool isLeapYear = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);

    // Calculate month and day
    const int* monthDays = DaysInMonth[isLeapYear ? 1 : 0];
    uint32_t dayOfYear = static_cast<uint32_t>(days) + 1; // Convert to 1-based

    uint16_t month = 1;
    while (dayOfYear > static_cast<uint32_t>(monthDays[month - 1])) {
        dayOfYear -= monthDays[month - 1];
        month++;
    }

    timeFields->Month = month;
    timeFields->Day = static_cast<uint16_t>(dayOfYear);
}

void RtlFreeAnsiString()
{
    LOG_UTILITY("!!! STUB !!!");
}

void RtlUnicodeStringToAnsiString()
{
    LOG_UTILITY("!!! STUB !!!");
}

void RtlInitUnicodeString()
{
    LOG_UTILITY("!!! STUB !!!");
}

void ExTerminateThread(uint32_t exitCode)
{
    // Thread termination request. The calling thread should exit.
    // Since this is typically called for cleanup during VdInitializeEngines,
    // we can safely ignore it for now.
    (void)exitCode;
}

uint32_t ExCreateThread(be<uint32_t>* handle, uint32_t stackSize, be<uint32_t>* threadId, uint32_t xApiThreadStartup, uint32_t startAddress, uint32_t startContext, uint32_t creationFlags)
{

    uint32_t hostThreadId;

    uint32_t entry = startAddress;
    uint32_t r3 = startContext;
    uint32_t r4 = 0;

    // Many titles use an XAPI thread startup wrapper that takes (entry, arg) in (r3, r4)
    // and then performs an indirect call to entry.
    if (xApiThreadStartup != 0)
    {
        entry = xApiThreadStartup;
        r3 = startAddress;
        r4 = startContext;
    }

    // Debug: Dump memory at startContext to see what callback should be there
    if (startContext != 0 && startContext >= 0x80000000 && startContext < PPC_MEMORY_SIZE)
    {
        uint32_t* ctxMem = reinterpret_cast<uint32_t*>(g_memory.Translate(startContext));
        LOGF_IMPL(Utility, "ExCreateThread", "startContext@0x{:08X}: [0]=0x{:08X}, [1]=0x{:08X}, [2]=0x{:08X}, [3]=0x{:08X}",
            startContext, 
            ByteSwap(ctxMem[0]), ByteSwap(ctxMem[1]), ByteSwap(ctxMem[2]), ByteSwap(ctxMem[3]));

        // If parent thread did not set the allocator in startContext[2], provide master allocator
        if (ctxMem[2] == 0)
        {
            ctxMem[2] = ByteSwap(0x83887168u);
            LOGF_INFO("[ExCreateThread] Injected master allocator 0x83887168 into startContext@0x{:08X}[2]", startContext);
        }
    }


    LOGF_WARNING("[ExCreateThread] Creating thread entry=0x{:08X} r3=0x{:08X} r4=0x{:08X} flags=0x{:X}", 
                 entry, r3, r4, creationFlags);
    GuestThreadHandle* thObj = GuestThread::Start({ entry, r3, r4, creationFlags }, &hostThreadId);
    if (!thObj)
    {
        LOGF_ERROR("[ExCreateThread] Failed to create thread object (OOM)");
        if (handle != nullptr)
            *handle = 0;
        if (threadId != nullptr)
            *threadId = 0;
        return 0xC0000017; // STATUS_NO_MEMORY
    }
    if (handle != nullptr) {
        *handle = GetKernelHandle(thObj);
    }
    LOGF_UTILITY("0x{:X}, 0x{:X}, 0x{:X}, 0x{:X}, 0x{:X}, 0x{:X}, 0x{:X} {:X}",
        (intptr_t)handle, stackSize, (intptr_t)threadId, xApiThreadStartup, startAddress, startContext, creationFlags, hostThreadId);
    if (threadId != nullptr)
        *threadId = hostThreadId;

    return 0;
}

void IoInvalidDeviceRequest()
{
    LOG_UTILITY("!!! STUB !!!");
}

void ObReferenceObject()
{
    LOG_UTILITY("!!! STUB !!!");
}

void IoCreateDevice()
{
    LOG_UTILITY("!!! STUB !!!");
}

void IoDeleteDevice()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t ExAllocatePoolTypeWithTag(uint32_t poolType, uint32_t size, uint32_t tag)
{
    const uint32_t lr = GET_CALLER_LR();
    void* host = g_userHeap.Alloc(size);
    if (!host) {
        LOGF_WARNING("[ExAllocatePoolTypeWithTag] OOM: poolType={} size=0x{:X} tag=0x{:08X} caller=0x{:08X}",
                     poolType, size, tag, lr);
        return 0;
    }
    memset(host, 0, size);
    uint32_t guest = g_memory.MapVirtual(host);
    LOGF_INFO("[ExAllocatePoolTypeWithTag] poolType={} size=0x{:X} tag=0x{:08X} -> guest=0x{:08X} caller=0x{:08X}",
              poolType, size, tag, guest, lr);
    return guest;
}

uint32_t ExAllocatePool(uint32_t size)
{
    const uint32_t lr = GET_CALLER_LR();
    void* host = g_userHeap.Alloc(size);
    if (!host) {
        LOGF_WARNING("[ExAllocatePool] OOM: size=0x{:X} caller=0x{:08X}", size, lr);
        return 0;
    }
    memset(host, 0, size);
    uint32_t guest = g_memory.MapVirtual(host);
    LOGF_INFO("[ExAllocatePool] size=0x{:X} -> guest=0x{:08X} caller=0x{:08X}", size, guest, lr);
    return guest;
}

void RtlTimeFieldsToTime()
{
    LOG_UTILITY("!!! STUB !!!");
}

void IoCompleteRequest()
{
    LOG_UTILITY("!!! STUB !!!");
}

void RtlUpcaseUnicodeChar()
{
    LOG_UTILITY("!!! STUB !!!");
}

void ObIsTitleObject()
{
    LOG_UTILITY("!!! STUB !!!");
}

void IoCheckShareAccess()
{
    LOG_UTILITY("!!! STUB !!!");
}

void IoSetShareAccess()
{
    LOG_UTILITY("!!! STUB !!!");
}

void IoRemoveShareAccess()
{
    LOG_UTILITY("!!! STUB !!!");
}

// XNetGetTitleXnAddr - Return fake IP to unblock network wait
// The game loops on this until it gets a valid network state
// Return value: 0=Pending, 1=None, 2=Ethernet (connected)
uint32_t NetDll_XNetGetTitleXnAddr(uint32_t pAddr)
{
    static int s_callCount = 0;
    ++s_callCount;
    
    if (pAddr != 0)
    {
        uint8_t* base = g_memory.base;
        
        // XNADDR structure layout:
        // 0x00: IN_ADDR ina (4 bytes) - Local IP
        // 0x04: IN_ADDR inaOnline (4 bytes) - Online IP  
        // 0x08: WORD wPortOnline (2 bytes)
        // 0x0A: BYTE abEnet[6] - MAC address
        // 0x10: BYTE abOnline[20] - Online key
        
        // Write fake IP: 192.168.1.100 (0xC0A80164 in network byte order)
        *reinterpret_cast<be<uint32_t>*>(base + pAddr + 0x00) = 0x6401A8C0; // 192.168.1.100
        *reinterpret_cast<be<uint32_t>*>(base + pAddr + 0x04) = 0x6401A8C0; // Same for online
        *reinterpret_cast<be<uint16_t>*>(base + pAddr + 0x08) = 3074; // Port
        
        // Fake MAC address: 00:11:22:33:44:55
        base[pAddr + 0x0A] = 0x00;
        base[pAddr + 0x0B] = 0x11;
        base[pAddr + 0x0C] = 0x22;
        base[pAddr + 0x0D] = 0x33;
        base[pAddr + 0x0E] = 0x44;
        base[pAddr + 0x0F] = 0x55;
    }
    
    if (s_callCount <= 5)
    {
        LOGF_UTILITY("XNetGetTitleXnAddr #{} -> Returning ETHERNET (2) with fake IP 192.168.1.100", s_callCount);
    }
    
    // Return XNET_GET_XNADDR_ETHERNET (2) = Connected
    return 2;
}

uint32_t KeWaitForMultipleObjects(uint32_t Count, xpointer<XDISPATCHER_HEADER>* Objects, uint32_t WaitType, uint32_t WaitReason, uint32_t WaitMode, uint32_t Alertable, be<int64_t>* Timeout)
{
    // SONIC APPROACH: Simple WaitMultiple implementation
    // WaitType: 0 = WaitAll, 1 = WaitAny
    // Returns: STATUS_WAIT_0 + index for WaitAny, STATUS_SUCCESS for WaitAll
    
    uint32_t callerLR = GET_CALLER_LR();
    uint64_t timeoutMs = GuestTimeoutToMilliseconds(Timeout);
    
    // Convert timeout to uint32_t (clamp if needed)
    uint32_t timeoutMs32 = (timeoutMs > 0xFFFFFFFF) ? INFINITE : static_cast<uint32_t>(timeoutMs);
    
    // Collect object addresses from the dispatcher headers
    // Convert host pointers back to guest addresses
    std::vector<uint32_t> addresses(Count);
    for (size_t i = 0; i < Count; i++)
    {
        // Objects[i].get() returns host pointer, convert to guest address
        addresses[i] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(Objects[i].get()) - reinterpret_cast<uintptr_t>(g_memory.base));
    }
    
    // Delegate to SyncTable_WaitMultiple
    // TODO: Implement proper WaitMultiple without SyncTable
    return STATUS_SUCCESS;
}

uint32_t KeRaiseIrqlToDpcLevel()
{
    return 0;
}

void KfLowerIrql() { }

uint32_t KeReleaseSemaphore(XKSEMAPHORE* semaphore, uint32_t increment, uint32_t adjustment, uint32_t wait)
{
    static int s_count = 0; ++s_count;
    uint32_t callerLR = GET_CALLER_LR();
    
    // Log all releases to see if blocking semaphores are ever signaled
    uint32_t semAddr = (uint32_t)((uint8_t*)semaphore - g_memory.base);
    if (s_count <= 30 || (semAddr >= 0xEB2D0000 && semAddr <= 0xEB2E0000)) {
        LOGF_WARNING("[KeReleaseSemaphore] #{} sem=0x{:08X} adj={}", s_count, semAddr, adjustment);
    }
    
    auto* object = QueryKernelObject<Semaphore>(semaphore->Header);
    if (!object) {
        LOGF_WARNING("[KeReleaseSemaphore] #{} sem=0x{:08X} QueryKernelObject returned NULL!", s_count, semAddr);
        return STATUS_INVALID_HANDLE;
    }
    object->Release(adjustment, nullptr);
    return STATUS_SUCCESS;
}

void XAudioGetVoiceCategoryVolume()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t XAudioGetVoiceCategoryVolumeChangeMask(uint32_t Driver, be<uint32_t>* Mask)
{
    *Mask = 0;
    return 0;
}

uint32_t KeResumeThread(uint32_t threadHandle)
{
    static int s_count = 0; ++s_count;
    if (IsKernelObject(threadHandle)) {
        GuestThreadHandle* hThread = GetKernelObject<GuestThreadHandle>(threadHandle);
        if (hThread) {
            LOGF_INFO("[RESUME] KeResumeThread #{} thread=0x{:08X}", s_count, threadHandle);
            hThread->suspended = false;
            hThread->suspended.notify_all();
            return 1;
        }
    }
    LOGF_WARNING("[RESUME] KeResumeThread #{} thread=0x{:08X} not found", s_count, threadHandle);
    return 0;
}

void KeInitializeSemaphore(XKSEMAPHORE* semaphore, uint32_t count, uint32_t limit)
{
    static int s_count = 0; ++s_count;
    uint32_t callerLR = GET_CALLER_LR();
    
    semaphore->Header.Type = 5;
    semaphore->Header.SignalState = count;
    semaphore->Limit = limit;

    auto* object = QueryKernelObject<Semaphore>(semaphore->Header);
    
    // Track semaphores initialized with count=0 (will block immediately)
    uint32_t semAddr = (uint32_t)((uint8_t*)semaphore - g_memory.base);
    
    if (count == 0) {
        TrackBlockingSemaphoreAddr(semAddr);
        LOGF_WARNING("[KeInitializeSemaphore] #{} BLOCKING sem=0x{:08X} count=0 limit={} - TRACKED", 
                     s_count, semAddr, limit);
    } else if (s_count <= 30) {
        LOGF_WARNING("[KeInitializeSemaphore] #{} sem=0x{:08X} count={} limit={} object={}", 
                     s_count, semAddr, count, limit, object ? "OK" : "NULL");
    }
}

void XMAReleaseContext()
{
    LOG_UTILITY("!!! STUB !!!");
}

void XMACreateContext()
{
    LOG_UTILITY("!!! STUB !!!");
}

// uint32_t XAudioRegisterRenderDriverClient(be<uint32_t>* callback, be<uint32_t>* driver)
// {
//     //printf("XAudioRegisterRenderDriverClient(): %x %x\n");
// 
//     *driver = apu::RegisterClient(callback[0], callback[1]);
//     return 0;
// }

// void XAudioUnregisterRenderDriverClient()
// {
//     printf("!!! STUB !!! XAudioUnregisterRenderDriverClient\n");
// }

// uint32_t XAudioSubmitRenderDriverFrame(uint32_t driver, void* samples)
// {
//     // printf("!!! STUB !!! XAudioSubmitRenderDriverFrame\n");
//     apu::SubmitFrames(samples);
// 
//     return 0;
// }

void XapiInitProcess()
{
    printf("XapiInitProcess Invoked\n");

    int *XapiProcessHeap = (int *)g_memory.Translate(0x82D57540);

    *XapiProcessHeap = 1;
}

// GTA V specific stubs
// XamTaskSchedule - schedule an async task
// CRITICAL FIX: Actually execute the task function so completion signals fire
uint32_t XamTaskSchedule(uint32_t funcAddr, uint32_t context, uint32_t processId,
                         uint32_t stackSize, uint32_t priority, uint32_t flags,
                         be<uint32_t>* phTask)
{
    static int s_count = 0;
    static uint32_t s_nextHandle = 0x80001000;
    
    ++s_count;
    if (s_count <= 30 || s_count % 100 == 0)
    {
        LOGF_WARNING("[XamTaskSchedule] #{} func=0x{:08X} ctx=0x{:08X} stack={} prio={} flags=0x{:X}",
                  s_count, funcAddr, context, stackSize, priority, flags);
    }
    
    // Assign a task handle
    uint32_t taskHandle = s_nextHandle++;
    if (phTask)
    {
        *phTask = taskHandle;
    }
    
    // ACTUALLY EXECUTE THE TASK FUNCTION
    // Without this, tasks are "scheduled" but never run, so completion signals never fire
    // This is likely why the scheduler blocks forever
    if (funcAddr != 0 && funcAddr >= 0x82000000 && funcAddr < 0x83000000)
    {
        auto func = g_memory.FindFunction(funcAddr);
        if (func)
        {
            if (s_count <= 20)
            {
                LOGF_WARNING("[XamTaskSchedule] EXECUTING task func=0x{:08X} ctx=0x{:08X}", funcAddr, context);
            }
            
            // Execute the task function with context as r3
            PPCContext taskCtx = *g_ppcContext;
            taskCtx.r3.u32 = context;
            func(taskCtx, g_memory.base);
            
            if (s_count <= 20)
            {
                LOGF_WARNING("[XamTaskSchedule] Task func=0x{:08X} COMPLETED", funcAddr);
            }
        }
        else
        {
            LOGF_WARNING("[XamTaskSchedule] Could not find function at 0x{:08X}", funcAddr);
        }
    }
    
    // Since we execute synchronously, signal this event so the wait doesn't block
    constexpr uint32_t kTaskCompletionEventAddr = 0x83131B34;  // FIXED: was 0x82A97F5C
    XDISPATCHER_HEADER* completionEvent = reinterpret_cast<XDISPATCHER_HEADER*>(g_memory.Translate(kTaskCompletionEventAddr));
    if (completionEvent && (completionEvent->Type == 0 || completionEvent->Type == 1))
    {
        QueryKernelObject<Event>(*completionEvent)->Set();
        if (s_count <= 20)
        {
            LOGF_WARNING("[XamTaskSchedule] Signaled completion event at 0x{:08X}", kTaskCompletionEventAddr);
        }
    }
    
    return ERROR_SUCCESS;
}

uint32_t XamTaskShouldExit(uint32_t taskHandle)
{
    // Return 0 so tasks continue running - returning 1 causes task loops to exit
    // immediately, which prevents initialization from completing and blocks main loop
    static int s_count = 0;
    if (++s_count <= 10)
    {
        LOGF_UTILITY("handle=0x{:08X} -> returning 0 (task continues)", taskHandle);
    }
    return 0;
}

uint32_t XamTaskCloseHandle(uint32_t taskHandle)
{
    LOG_UTILITY("!!! STUB !!!");
    return ERROR_SUCCESS;
}

uint32_t XamUserAreUsersFriends(uint32_t userIndex, uint64_t* xuids, uint32_t count, uint32_t* results, void* overlapped)
{
    // Return that no users are friends
    if (results)
        memset(results, 0, count * sizeof(uint32_t));
    return ERROR_SUCCESS;
}

uint32_t XamUserCheckPrivilege(uint32_t userIndex, uint32_t privilegeType, uint32_t* result)
{
    // Grant all privileges
    if (result)
        *result = 1;
    return ERROR_SUCCESS;
}

uint32_t XamUserCreateStatsEnumerator(uint32_t titleId, uint32_t userIndex, uint32_t xuidCount,
    void* pxuid, uint32_t dwNumStatSpecs, void* pStatSpecs, be<uint32_t>* pcbBuffer, be<uint32_t>* phEnum)
{
    // Return no stats available
    if (phEnum)
        *phEnum = 0;
    if (pcbBuffer)
        *pcbBuffer = 0;
    return ERROR_NO_MORE_FILES;
}

uint32_t XamUserGetName(uint32_t userIndex, char* buffer, uint32_t bufferSize)
{
    if (userIndex != 0)
        return ERROR_NO_SUCH_USER;
    if (buffer && bufferSize > 0)
    {
        const char* name = "Player";
        strncpy(buffer, name, bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
    }
    return ERROR_SUCCESS;
}

uint32_t XamUserGetXUID(uint32_t userIndex, uint64_t* xuid)
{
    if (userIndex != 0)
        return ERROR_NO_SUCH_USER;
    if (xuid)
        *xuid = 0x0001000000000001ULL; // Fake XUID
    return ERROR_SUCCESS;
}

uint32_t XamUserWriteProfileSettings(uint32_t userIndex, uint32_t numSettings, void* settings, void* overlapped)
{
    LOG_UTILITY("!!! STUB !!!");
    return ERROR_SUCCESS;
}

uint32_t XamVoiceCreate(uint32_t userIndex, uint32_t flags, void** voice)
{
    LOG_UTILITY("!!! STUB !!!");
    if (voice)
        *voice = nullptr;
    return ERROR_DEVICE_NOT_CONNECTED;
}

void XamVoiceClose(void* voice)
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t XamVoiceHeadsetPresent(uint32_t userIndex)
{
    // No headset present
    return 0;
}

uint32_t XamVoiceSubmitPacket(void* voice, uint32_t size, void* data)
{
    LOG_UTILITY("!!! STUB !!!");
    return ERROR_SUCCESS;
}

uint32_t XeKeysConsolePrivateKeySign(void* signature, void* data, uint32_t dataSize)
{
    LOG_UTILITY("!!! STUB !!!");
    return ERROR_SUCCESS;
}

void IoDismountVolumeByFileHandle()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t NetDll_XNetGetEthernetLinkStatus()
{
    // Return link up
    return 1;
}

uint32_t KeTryToAcquireSpinLockAtRaisedIrql(void* spinlock)
{
    // Try to acquire, always succeed for now
    return 1;
}

uint32_t XamShowGamerCardUIForXUID(uint32_t userIndex, uint64_t xuid, void* overlapped)
{
    LOG_UTILITY("!!! STUB !!!");
    return ERROR_SUCCESS;
}

uint32_t XamShowPlayerReviewUI(uint32_t userIndex, uint64_t xuid, void* overlapped)
{
    LOG_UTILITY("!!! STUB !!!");
    return ERROR_SUCCESS;
}


// Debug counters for XamAlloc/XamFree
static std::atomic<int64_t> g_xamAllocCount{0};
static std::atomic<int64_t> g_xamFreeCount{0};

// XamAlloc - Xbox memory allocation API
// API: DWORD XamAlloc(DWORD dwFlags, DWORD dwSize, PVOID *ppBuffer)
// Returns STATUS (0=success), stores allocated pointer at *ppBuffer
void* XamAlloc(uint32_t flags, uint32_t size, void* pBuffer)
{
    if (size == 0) {
        if (pBuffer) *(be<uint32_t>*)pBuffer = 0;
        return nullptr;  // STATUS_SUCCESS
    }
    
    void* ptr = g_userHeap.AllocPhysical(size, 16);
    if (!ptr) {
        LOGF_WARNING("[XamAlloc] FAILED size={} flags=0x{:X}", size, flags);
        if (pBuffer) *(be<uint32_t>*)pBuffer = 0;
        return (void*)(uintptr_t)0x8007000E;  // E_OUTOFMEMORY
    }
    
    // Store allocated pointer at *pBuffer (OUTPUT parameter)
    uint32_t guestAddr = g_memory.MapVirtual(ptr);
    if (pBuffer) {
        *(be<uint32_t>*)pBuffer = guestAddr;
    }
    
    int64_t count = ++g_xamAllocCount;
    if (count <= 10 || count % 10000 == 0) {
        LOGF_WARNING("[XamAlloc] #{} size={} guest=0x{:08X} frees={}", count, size, guestAddr, g_xamFreeCount.load());
    }
    
    return nullptr;  // STATUS_SUCCESS (0)
}

// XamFree - buffer is already a host pointer (framework translated from guest)
void XamFree(void* buffer)
{
    if (!buffer) return;
    int64_t count = ++g_xamFreeCount;
    if (count <= 10 || count % 10000 == 0) {
        LOGF_WARNING("[XamFree] #{} ptr={:p} allocs={}", count, buffer, g_xamAllocCount.load());
    }
    g_userHeap.Free(buffer);
}


uint32_t NtSetTimerEx(uint32_t timerHandle, void* dueTime, void* apcRoutine, void* apcContext, uint32_t resume, uint32_t period, void* previousState)
{
    LOGF_INFO("[TRACE] NtSetTimerEx: timerHandle=0x{:08X} period={}", timerHandle, period);
    if (timerHandle == 0 || timerHandle == GUEST_INVALID_HANDLE_VALUE || timerHandle >= 0xFFFFFFF0u) {
        return 0xC0000008; // STATUS_INVALID_HANDLE
    }
    if (IsValidKernelHandle(timerHandle)) {
        Event* evt = GetKernelObject<Event>(timerHandle);
        if (evt) {
            evt->Set();
        }
    }
    return 0;
}

uint32_t NtCancelTimer(uint32_t timerHandle, uint32_t* currentState)
{
    LOGF_INFO("[TRACE] NtCancelTimer: timerHandle=0x{:08X}", timerHandle);
    if (timerHandle == 0 || timerHandle == GUEST_INVALID_HANDLE_VALUE || timerHandle >= 0xFFFFFFF0u) {
        return 0xC0000008; // STATUS_INVALID_HANDLE
    }
    if (IsValidKernelHandle(timerHandle)) {
        Event* evt = GetKernelObject<Event>(timerHandle);
        if (evt) {
            evt->Reset();
        }
    }
    if (currentState) {
        *currentState = 0;
    }
    return 0;
}

uint32_t NtCreateTimer(be<uint32_t>* timerHandle, uint32_t desiredAccess, void* objectAttributes, uint32_t timerType)
{
    Event* timerEvt = CreateKernelObject<Event>(true, false);
    uint32_t h = GetKernelHandle(timerEvt);
    if (timerHandle) {
        *timerHandle = h;
    }
    LOGF_INFO("[TRACE] NtCreateTimer: handle=0x{:08X} type={}", h, timerType);
    return 0;
}

uint32_t NtCreateMutant(be<uint32_t>* mutantHandle, uint32_t desiredAccess, void* objectAttributes, uint32_t initialOwner)
{
    // Mutants (mutexes) in the Xbox 360 kernel are recursive mutexes with
    // ownership tracking. Since our recompilation doesn't track thread
    // ownership, we create the mutant as an auto-reset event that starts
    // signaled (free). This allows any thread to acquire it immediately.
    // When initialOwner is true (non-zero), the creating thread "owns" the
    // mutex, but since we don't implement real ownership, we still start
    // signaled so worker threads don't deadlock waiting for a release that
    // never happens in the recompiled code path.
    Event* mutantEvt = CreateKernelObject<Event>(false, true);  // auto-reset, signaled
    uint32_t h = GetKernelHandle(mutantEvt);
    if (mutantHandle) {
        *mutantHandle = h;
    }
    LOGF_INFO("[TRACE] NtCreateMutant: handle=0x{:08X} initialOwner={} (forced signaled)", h, initialOwner);
    return 0;
}

uint32_t NtReleaseMutant(uint32_t mutantHandle, uint32_t* previousCount)
{
    if (previousCount) {
        *previousCount = 1;
    }
    if (mutantHandle == 0 || mutantHandle == GUEST_INVALID_HANDLE_VALUE || mutantHandle >= 0xFFFFFFF0u) {
        return 0xC0000008; // STATUS_INVALID_HANDLE
    }
    if (IsValidKernelHandle(mutantHandle)) {
        Event* evt = GetKernelObject<Event>(mutantHandle);
        if (evt) {
            evt->Set();
        }
    }
    return 0;
}

void IoDismountVolume()
{
    LOG_UTILITY("!!! STUB !!!");
}

void XNotifyPositionUI(uint32_t position)
{
    LOG_UTILITY("!!! STUB !!!");
}

int32_t NetDll_getsockname(uint32_t socket, void* name, void* namelen)
{
    LOG_UTILITY("!!! STUB !!!");
    return -1;
}

int32_t NetDll_ioctlsocket(uint32_t socket, uint32_t cmd, void* argp)
{
    LOG_UTILITY("!!! STUB !!!");
    return 0;
}

int32_t NetDll_sendto(uint32_t socket, void* buf, uint32_t len, uint32_t flags, void* to, uint32_t tolen)
{
    LOG_UTILITY("!!! STUB !!!");
    return -1;
}

int32_t NetDll_recvfrom(uint32_t socket, void* buf, uint32_t len, uint32_t flags, void* from, void* fromlen)
{
    LOG_UTILITY("!!! STUB !!!");
    return -1;
}

int32_t NetDll_shutdown(uint32_t socket, uint32_t how)
{
    LOG_UTILITY("!!! STUB !!!");
    return 0;
}

void XMsgCancelIORequest()
{
    LOG_UTILITY("!!! STUB !!!");
}

uint32_t XAudioGetSpeakerConfig(void* config)
{
    LOG_UTILITY("!!! STUB !!!");
    return 0;
}

uint32_t XamContentSetThumbnail(const char* rootName, void* imageData, uint32_t imageSize, void* overlapped)
{
    LOG_UTILITY("!!! STUB !!!");
    return ERROR_SUCCESS;
}

uint32_t XamInputGetKeystrokeEx(uint32_t userIndex, uint32_t flags, void* keystroke)
{
    // Validate user index
    if (userIndex >= 4) {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    
    // Try to dequeue a keystroke
    hid::KeystrokeEvent event;
    if (!hid::DequeueKeystroke(userIndex, event)) {
        return ERROR_EMPTY;
    }
    
    // Write to guest memory
    if (keystroke != nullptr) {
        XINPUT_KEYSTROKE* pKeystroke = reinterpret_cast<XINPUT_KEYSTROKE*>(keystroke);
        pKeystroke->VirtualKey = event.virtualKey;
        pKeystroke->Unicode = event.unicode;
        pKeystroke->Flags = event.flags;
        pKeystroke->UserIndex = event.userIndex;
        pKeystroke->HidCode = 0;
    }
    
    return ERROR_SUCCESS;
}

uint32_t XamSessionCreateHandle(void* sessionInfo, void* handle)
{
    LOG_UTILITY("!!! STUB !!!");
    return ERROR_SUCCESS;
}

uint32_t XamSessionRefObjByHandle(uint32_t handle, void* obj)
{
    LOG_UTILITY("!!! STUB !!!");
    return ERROR_INVALID_HANDLE;
}

void KeSetDisableBoostThread(void* thread, uint32_t disable)
{
    // Thread priority boost control is not implemented.
    // This is safe to ignore as it only affects scheduling hints.
    (void)thread;
    (void)disable;
}

uint32_t XamCreateEnumeratorHandle(uint32_t type, uint32_t size, void* data, void* handle)
{
    LOG_UTILITY("!!! STUB !!!");
    return ERROR_SUCCESS;
}

uint32_t NtDeviceIoControlFile(void* fileHandle, void* event, void* apcRoutine, void* apcContext, void* ioStatusBlock, uint32_t ioControlCode, void* inputBuffer, uint32_t inputBufferLength, void* outputBuffer, uint32_t outputBufferLength)
{
    LOG_UTILITY("!!! STUB !!!");
    return 0;
}

int32_t NetDll_WSAGetLastError()
{
    LOG_UTILITY("!!! STUB !!!");
    return 0;
}

// =============================================================================
// GTA V: Bypass dirty disc error display only
// =============================================================================

// Previously STUBBED: This function called an indirect task function that blocks forever
// FIX: Execute the task function ONCE synchronously, then return
// This allows resource loading to complete while avoiding the infinite semaphore-wait loop
//
// CRITICAL: Startup context structure is:
//   [0] = task function pointer
//   [1] = REAL worker context (with semaphores at +36, +40)
// Must pass ctxData[1] to task function, NOT the startup context!


// This function has an indirect call at context+36 that can contain garbage
// Validate the callback pointer before allowing the original to run




// =============================================================================
// UNIFIED BOOT SEQUENCE REWRITE - Complete _xstart Replacement
// =============================================================================
// This is a comprehensive replacement for the entire Xbox 360 boot sequence.
// It replaces ALL functions in the _xstart execution path with modern equivalents
// that work correctly on PC platforms and support online multiplayer.
//
// Original _xstart (0x829A0860) execution path:
//
// This unified replacement:
// - SKIPS all Xbox-specific hardware/security checks
// - MODERNIZES CRT/TLS init without Xbox thread pool timing
// - PRESERVES all memory state the game expects
// - KEEPS essential game code (init tables, constructors, game main)
// - ENABLES online multiplayer by removing timing assumptions
// =============================================================================

// Extern declarations for boot sequence functions
extern "C" void __imp___xstart(PPCContext& ctx, uint8_t* base);

// =============================================================================
// Storage Device Constants (still needed for PC storage hooks)
// =============================================================================
namespace StorageConstants {
    constexpr uint32_t PC_STORAGE_DEVICE_ADDR  = 0x83132000;
    constexpr uint32_t PC_STORAGE_VTABLE_ADDR  = 0x83132100;
    constexpr uint32_t PC_STORAGE_DATA_ADDR    = 0x83132200;
}


// =============================================================================
// Boot Sequence Global Addresses - DISABLED
// =============================================================================
// These were used by the _xstart replacement which is now disabled.
// Per UnleashedRecomp architecture, we let the original game CRT run.
// =============================================================================
#if 0  // DISABLED - boot hacks removed
namespace BootGlobals {
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // XEX header pointer location (from lis r11,-32256 = 0x81300000)
    constexpr uint32_t XEX_HEADER_PTR    = 0x8130063C;  // [0x81300000+1596]
    constexpr uint32_t XEX_VTABLE_PTR    = 0x813006B0;  // [0x81300000+1712]
    constexpr uint32_t ALLOC_RESULT_ADDR = 0x83008440;  // [0x83010000-31780] - also used by exception handler
    
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    constexpr uint32_t CALLBACK_CRIT_ADDR = 0x82A97FB4; // Critical section for list
    constexpr uint32_t CALLBACK_LIST_ADDR = 0x82A97FD0; // Doubly-linked list head
    
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // CRT vtable pointers (from lis r30,-31981 = 0x81200000)
    constexpr uint32_t VTABLE1_ADDR = 0x812017E4;  // offset 6116 - thread create
    constexpr uint32_t VTABLE2_ADDR = 0x812017E8;  // offset 6120 - TLS context
    constexpr uint32_t VTABLE3_ADDR = 0x812017EC;  // offset 6124 - thread register
    constexpr uint32_t VTABLE4_ADDR = 0x812017F0;  // offset 6128 - thread destroy
    
    constexpr uint32_t CRT_FINALIZE_ADDR = 0x812019A8;  // offset 6568
    constexpr uint32_t IO_SYSTEM_ADDR    = 0x812019AC;  // offset 6572
    constexpr uint32_t MEM_MANAGER_ADDR  = 0x812019B0;  // offset 6576
    constexpr uint32_t HEAP_INIT_ADDR    = 0x812019B4;  // offset 6580
    
    // TLS storage (from 0x82A90000 base)
    constexpr uint32_t TLS_INDEX_ADDR     = 0x82A96E64;  // offset 28260
    constexpr uint32_t THREAD_HANDLE_ADDR = 0x82A96E60;  // offset 28256
    constexpr uint32_t NEW_THREAD_ALLOC   = 0x82A96B30;  // offset 27472
    
    constexpr uint32_t THREAD_POOL_ADDR   = 0x82A97200;  // 36 slots × 8 bytes
    constexpr uint32_t THREAD_POOL_SIZE   = 36;
    
    // Thread context (pre-allocated area)
    constexpr uint32_t THREAD_CONTEXT_ADDR = 0x83080000;
    constexpr uint32_t THREAD_CONTEXT_SIZE = 196;
    
    // -------------------------------------------------------------------------
    // Global Thread Context Base (0x83130000)
    // -------------------------------------------------------------------------
    // This is the address that [r13+0] must point to for memory allocation to work.
    // Original code stores memory manager at 0x83130000 + 1676.
    constexpr uint32_t GLOBAL_THREAD_CTX_BASE = 0x83130000;
    constexpr uint32_t MEM_MGR_OFFSET         = 1676;      // Offset to memory manager pointer
    constexpr uint32_t MEM_MGR_PTR_ADDR       = 0x8313068C; // 0x83130000 + 1676
    
    // Memory manager structure: needs vtable at offset 0, allocator at vtable+8
    // We'll create a simple memory manager structure in guest memory
    constexpr uint32_t MEM_MGR_STRUCT_ADDR    = 0x83131000; // Our memory manager object
    constexpr uint32_t MEM_MGR_VTABLE_ADDR    = 0x83131100; // Our vtable for memory manager
    
    // -------------------------------------------------------------------------
    // PC Storage Device (Platform Glue for Xbox Storage System)
    // -------------------------------------------------------------------------
    // Replaces Xbox content mounting with VFS-based file access.
    // The game uses a vtable-based storage interface:
    //
    // Our PC storage device provides vtable functions that use VFS.
    constexpr uint32_t PC_STORAGE_DEVICE_ADDR  = 0x83132000; // PC storage device object
    constexpr uint32_t PC_STORAGE_VTABLE_ADDR  = 0x83132100; // Vtable for storage device
    constexpr uint32_t PC_STORAGE_DATA_ADDR    = 0x83132200; // Storage device internal data
    
    // Vtable offsets used by the game:
    //   +0  = destructor
    //   +4  = matchPath (check if path matches device)
    //   +8  = openFile (open file, return handle)
    //   +24 = readFile
    //   +40 = seekFile
    //   +44 = closeFile
    //   +72 = getFileSize
    //   +76 = getFileInfo
    
    constexpr uint32_t CRT_CONTEXT_BASE   = 0x81201820;  // offset 6176
    
    // Vtable values (computed from PPC immediates)
    constexpr uint32_t VTABLE1_VALUE = 0x82A543E8;  // 0x82A50000 + 17384
    constexpr uint32_t VTABLE2_VALUE = 0x82A0270C;  // 0x82A00000 + 9996
    constexpr uint32_t VTABLE3_VALUE = 0x82A0271C;  // 0x82A00000 + 10012
    constexpr uint32_t VTABLE4_VALUE = 0x82A0272C;  // 0x82A00000 + 10028
    constexpr uint32_t CRT_FINALIZE_VTABLE = 0x82A58D38;  // 0x82A60000 - 29432
    constexpr uint32_t NEW_THREAD_VTABLE   = 0x82A52660;  // 0x82A50000 + 9824
    
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // No persistent memory writes - just returns success/failure
    
    // -------------------------------------------------------------------------
    // _xstart globals
    // -------------------------------------------------------------------------
    // From original _xstart prologue (lis r10,-31979 = 0x83010000)
    constexpr uint32_t XSTART_FLAG1 = 0x8300844C;  // [0x83010000-31796]
    constexpr uint32_t XSTART_FLAG2 = 0x83008450;  // [0x83010000-31792]
    
    // -------------------------------------------------------------------------
    // Command-line storage
    // -------------------------------------------------------------------------
    constexpr uint32_t CMDLINE_BUFFER_ADDR = 0x83100000;
    constexpr uint32_t CMDLINE_ARGV_ADDR   = 0x83100800;
    constexpr uint32_t CMDLINE_MAX_ARGS    = 16;
    constexpr uint32_t CMDLINE_BUFFER_SIZE = 2048;
}  // namespace BootGlobals
#endif  // DISABLED BootGlobals

// =============================================================================
// Modern CRT/TLS Initialization - DISABLED
// =============================================================================
// This was part of the _xstart replacement which is now disabled.
// Per UnleashedRecomp architecture, we let the original game CRT run.
// =============================================================================
#if 0  // DISABLED - InitializeModernCRT not used
static void InitializeModernCRT(PPCContext& ctx, uint8_t* base)
{
    LOG_WARNING("[CRT] * Modern CRT/TLS initialization starting");
    
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    PPC_STORE_U32(BootGlobals::VTABLE1_ADDR, BootGlobals::VTABLE1_VALUE);
    PPC_STORE_U32(BootGlobals::VTABLE2_ADDR, BootGlobals::VTABLE2_VALUE);
    PPC_STORE_U32(BootGlobals::VTABLE3_ADDR, BootGlobals::VTABLE3_VALUE);
    PPC_STORE_U32(BootGlobals::VTABLE4_ADDR, BootGlobals::VTABLE4_VALUE);
    
    // -------------------------------------------------------------------------
    // Allocate and set TLS slot
    // -------------------------------------------------------------------------
    uint32_t tlsIndex = KeTlsAlloc();
    PPC_STORE_U32(BootGlobals::TLS_INDEX_ADDR, tlsIndex);
    
    if (tlsIndex == 0xFFFFFFFF) {
        LOG_WARNING("[CRT] ERROR: KeTlsAlloc failed!");
        return;
    }
    
    KeTlsSetValue(tlsIndex, BootGlobals::VTABLE2_VALUE);
    LOGF_WARNING("[CRT] TLS slot {} allocated, value=0x{:08X}", tlsIndex, BootGlobals::VTABLE2_VALUE);
    
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    PPC_STORE_U32(BootGlobals::HEAP_INIT_ADDR, 0);
    
    PPC_STORE_U32(BootGlobals::MEM_MANAGER_ADDR, 0);
    
    PPC_STORE_U32(BootGlobals::ALLOC_RESULT_ADDR, 0);
    
    PPC_STORE_U32(BootGlobals::IO_SYSTEM_ADDR, 0);
    
    ctx.r3.s64 = 0;
    __imp__sub_828E0AB8(ctx, base);
    
    PPC_STORE_U32(BootGlobals::CRT_FINALIZE_ADDR, BootGlobals::CRT_FINALIZE_VTABLE);
    
    PPC_STORE_U32(BootGlobals::NEW_THREAD_ALLOC, BootGlobals::NEW_THREAD_VTABLE);
    
    LOG_WARNING("[CRT] CRT subsystem flags initialized");
    
    // -------------------------------------------------------------------------
    // Original: 36 slots, 4000ms timeout per thread
    // Reason: Xbox-specific timing that breaks multiplayer
    // -------------------------------------------------------------------------
    for (uint32_t i = 0; i < BootGlobals::THREAD_POOL_SIZE; ++i) {
        uint32_t slotAddr = BootGlobals::THREAD_POOL_ADDR + (i * 8);
        PPC_STORE_U32(slotAddr, 0);      // slot pointer
        PPC_STORE_U32(slotAddr + 4, 0);  // slot state
    }
    LOG_WARNING("[CRT] Thread pool zeroed (skipped Xbox 4s timeout init)");
    
    // -------------------------------------------------------------------------
    // Create synthetic main thread handle
    // Original: vtable1 indirect call
    // -------------------------------------------------------------------------
    uint32_t mainThreadHandle = std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0x7FFFFFFF;
    if (mainThreadHandle == 0 || mainThreadHandle == 0xFFFFFFFF) mainThreadHandle = 1;
    PPC_STORE_U32(BootGlobals::THREAD_HANDLE_ADDR, mainThreadHandle);
    LOGF_WARNING("[CRT] Main thread handle: 0x{:08X}", mainThreadHandle);
    
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    uint32_t contextAddr = BootGlobals::THREAD_CONTEXT_ADDR;
    memset(base + contextAddr, 0, BootGlobals::THREAD_CONTEXT_SIZE);
    
    // Initialize context fields (from original code analysis)
    PPC_STORE_U32(contextAddr + 0, mainThreadHandle);   // Thread ID
    PPC_STORE_U32(contextAddr + 4, 0xFFFFFFFF);         // -1
    PPC_STORE_U32(contextAddr + 20, 1);                 // Flag
    PPC_STORE_U32(contextAddr + 92, 0x82A97300);        // Callback ptr
    
    LOGF_WARNING("[CRT] Thread context at 0x{:08X} initialized", contextAddr);
    
    // -------------------------------------------------------------------------
    // Original: Registers callback in doubly-linked list
    // -------------------------------------------------------------------------
    PPC_STORE_U32(BootGlobals::CALLBACK_LIST_ADDR, BootGlobals::CALLBACK_LIST_ADDR);
    PPC_STORE_U32(BootGlobals::CALLBACK_LIST_ADDR + 4, BootGlobals::CALLBACK_LIST_ADDR);
    
    LOG_WARNING("[CRT] * Modern CRT/TLS initialization complete");
}
#endif  // DISABLED InitializeModernCRT, sub_82994700, sub_829A7960

// =============================================================================
// =============================================================================
// ISSUE: The original function computes constructor table addresses as:
//   Array 1: 0x820214FC to 0x82021508 (3 entries)
//   Array 2: 0x82020010 to 0x820214F8 (1338 entries)
//
// However, these addresses contain GAME DATA (strings like "Look", "Time"),
// not actual function pointers. Attempting to call these causes crashes.
//
// ROOT CAUSE: The XEX constructor tables may be at different addresses than
// the hardcoded offsets in the recompiled PPC code, or they may not exist
// in the same form as the original Xbox CRT expected.
//
// SOLUTION: Skip constructor execution and return success. If game-specific
// static constructors are needed later, they should be identified and called
// explicitly.
//
// TODO: If game initialization fails later, revisit this to identify which
// constructors (if any) are actually required.
// =============================================================================

// DISABLED - Let original constructor execution run per UnleashedRecomp architecture
#if 0
#endif  // DISABLED sub_829A7DC8

// =============================================================================
// Command-Line Support Helper (only used by disabled _xstart)
// =============================================================================
#if 0  // DISABLED - part of boot sequence hacks
static int BuildGuestCommandLine(uint8_t* base, const std::vector<std::string>& args)
{
    if (args.empty()) return 0;
    
    uint32_t stringOffset = 0;
    uint32_t argCount = 0;
    
    for (size_t i = 0; i < args.size() && i < BootGlobals::CMDLINE_MAX_ARGS && 
         stringOffset < BootGlobals::CMDLINE_BUFFER_SIZE - 256; ++i)
    {
        const std::string& arg = args[i];
        
        uint32_t stringAddr = BootGlobals::CMDLINE_BUFFER_ADDR + stringOffset;
        memcpy(base + stringAddr, arg.c_str(), arg.size() + 1);
        
        uint32_t argvSlot = BootGlobals::CMDLINE_ARGV_ADDR + (i * 4);
        PPC_STORE_U32(argvSlot, stringAddr);
        
        stringOffset += arg.size() + 1;
        argCount++;
    }
    
    PPC_STORE_U32(BootGlobals::CMDLINE_ARGV_ADDR + (argCount * 4), 0);
    return argCount;
}
#endif  // DISABLED BuildGuestCommandLine

// =============================================================================
// UNIFIED _xstart - Complete Xbox 360 Boot Sequence Replacement
// =============================================================================
// This is the master boot function that replaces the entire _xstart execution
// path. It performs all necessary memory initialization and calls only the
// essential game code functions.
//
// Replaced functions (Xbox-specific):
//
// Preserved functions (game code):
// =============================================================================

// =============================================================================
// REFACTORING NOTE: _xstart replacement DISABLED
// =============================================================================
// Per UnleashedRecomp architecture, we should NOT replace _xstart.
// Let the game's original CRT run, and only hook Xbox kernel/XAM APIs.
// The GUEST_FUNCTION_HOOK macros at the end of this file handle Xbox APIs.
//
// If boot fails, identify which specific API needs a hook and add it properly.
// Do NOT re-enable this inline boot sequence - fix the proper way.
// =============================================================================
#if 0  // DISABLED - Let original _xstart run
PPC_FUNC(_xstart_DISABLED)
{
    LOG_WARNING("[BOOT] ============================================================");
    LOG_WARNING("[BOOT] * UNIFIED BOOT SEQUENCE - Modern _xstart replacement");
    LOG_WARNING("[BOOT] ============================================================");
    
    // =========================================================================
    // PHASE 1: SKIP Xbox-specific early init
    // =========================================================================
    //   - Checks RtlImageXexHeaderField for field 0x20001025
    //   - Not needed: We're not running from an XEX
    LOG_WARNING("[BOOT] [1/7] SKIP sub_829A7FF8 (XEX validation / firmware fallback)");
    
    // =========================================================================
    // PHASE 2: SKIP Xbox runtime callbacks
    // =========================================================================
    //   - Enters critical section at 0x82A97FB4
    //   - Iterates doubly-linked list at 0x82A97FD0
    //   - Calls each callback with r3=1 (startup notification)
    //   - Not needed: Xbox runtime notification system
    LOG_WARNING("[BOOT] [2/7] SKIP sub_829A7960 (Xbox runtime callbacks)");
    
    // =========================================================================
    // PHASE 3: SKIP HDCP/privilege check
    // =========================================================================
    //   - XexCheckExecutablePrivilege(10) - HDCP privilege
    //   - XGetAVPack() - checks for HDMI/VGA/Component
    //   - ExGetXConfigSetting - video config checks
    //   - On failure: displays localized error, terminates
    //   - Not needed: PC has no HDCP requirements
    LOG_WARNING("[BOOT] [3/7] SKIP sub_829A0678 (HDCP/privilege check - always pass)");
    
    // =========================================================================
    // PHASE 4: MODERNIZED CRT/TLS initialization
    // =========================================================================
    //   - Replaced with InitializeModernCRT() which:
    //     * Stores 4 CRT vtable pointers at 0x812017E4-F0
    //     * Allocates TLS slot, stores at 0x82A96E64
    //     * Initializes CRT subsystem flags (heap, I/O, memory manager)
    //     * SKIPS Xbox thread pool (36 slots × 4s timeout)
    //     * Creates synthetic thread handle
    //     * Allocates 196-byte thread context at 0x83080000
    //     * Initializes empty callback list at 0x82A97FD0
    LOG_WARNING("[BOOT] [4/7] CALL sub_82994700 (MODERNIZED CRT/TLS init)");
    sub_82994700(ctx, base);
    
    if (ctx.r3.u32 == 0) {
        LOG_WARNING("[BOOT] ERROR: CRT initialization failed!");
        return;
    }
    LOG_WARNING("[BOOT]       CRT/TLS initialization successful");
    
    // =========================================================================
    // PHASE 5: Execute init table (KEEP - game code)
    // =========================================================================
    //   - Base address: 0x82020000 (from lis r11,-32094)
    //   - End address: 0x8202000C (3 entries × 4 bytes)
    //   - For each non-zero pointer: call the function
    //   - KEEP: Essential game initialization
    LOG_WARNING("[BOOT] [5/7] CALL sub_829A7EA8 (init table executor @ 0x82020000)");
    
    {
        uint32_t tableBase = 0x82020000;
        uint32_t tableEnd = 0x8202000C;
        LOGF_WARNING("[BOOT]       Init table at 0x{:08X}-0x{:08X}:", tableBase, tableEnd);
        
        for (uint32_t addr = tableBase; addr < tableEnd; addr += 4) {
            uint32_t funcPtr = PPC_LOAD_U32(addr);
            LOGF_WARNING("[BOOT]         [0x{:08X}] = 0x{:08X}", addr, funcPtr);
            
            if (funcPtr != 0) {
                LOGF_WARNING("[BOOT]         -> Calling 0x{:08X}...", funcPtr);
                ctx.r3.u64 = 0;  // Set r3=0 as original does
                ctx.ctr.u64 = funcPtr;
                PPC_CALL_INDIRECT_FUNC(funcPtr);
                LOGF_WARNING("[BOOT]         -> Returned from 0x{:08X}, r3={}", funcPtr, ctx.r3.u32);
                
                if (ctx.r3.u32 != 0) {
                    LOGF_WARNING("[BOOT]         -> Function returned error (r3={}), stopping", ctx.r3.u32);
                    break;
                }
            }
        }
    }
    // Skip original call since we're doing it manually above
    LOG_WARNING("[BOOT]       Init table execution complete");
    
    // =========================================================================
    // PHASE 6: Run C++ static constructors (KEEP - game code)
    // =========================================================================
    //   - Calls optional pre-constructor at [0x81308F70]
    //   - Array 1: 0x8202150C (3 entries)
    //   - Array 2: 0x82020010 (1338 entries to 0x820214F8)
    //   - For each non-zero, non-(-1) pointer: call the function
    //   - KEEP: Essential C++ initialization
    LOG_WARNING("[BOOT] [6/7] CALL C++ static constructors (safe scan mode)");
    
    // Based on memory analysis:
    // - Array 1 (0x820214FC-0x82021508) contains STRING DATA, not constructors
    // - Array 2 (0x82020010+) contains VALID CODE POINTERS, but mixed with data
    //
    // Strategy: Scan from 0x82020010, call only valid code pointers (0x82XXXXXX),
    // skip NULL/0xFFFFFFFF, stop when we hit consecutive non-code data.
    
    uint32_t ctorTableStart = 0x82020010;
    uint32_t ctorTableMaxEnd = 0x820214F8;  // Original expected end
    int ctorCount = 0;
    int skipCount = 0;
    int consecutiveDataCount = 0;
    
    LOG_WARNING("[BOOT]       Executing constructors from 0x82020010 (safe scan)...");
    
    for (uint32_t addr = ctorTableStart; addr < ctorTableMaxEnd; addr += 4) {
        uint32_t funcPtr = PPC_LOAD_U32(addr);
        
        // Skip NULL and -1 entries (standard ctor table terminators)
        if (funcPtr == 0 || funcPtr == 0xFFFFFFFF) {
            skipCount++;
            consecutiveDataCount = 0;  // Reset - these are valid table entries
            continue;
        }
        
        // Check if it's a valid code pointer (0x82XXXXXX range)
        if (funcPtr >= 0x82000000 && funcPtr <= 0x82FFFFFF) {
            // Valid code pointer - call it
            ctx.ctr.u64 = funcPtr;
            PPC_CALL_INDIRECT_FUNC(funcPtr);
            ctorCount++;
            consecutiveDataCount = 0;
            
            // Log progress every 100 constructors
            if (ctorCount % 100 == 0) {
                LOGF_WARNING("[BOOT]         Progress: {} constructors called", ctorCount);
            }
        } else {
            // Non-code data encountered
            consecutiveDataCount++;
            
            // If we hit 3+ consecutive non-code entries, we've likely reached
            // the end of the constructor table and entered string/data section
            if (consecutiveDataCount >= 3) {
                LOGF_WARNING("[BOOT]       Stopping at 0x{:08X} - hit data section", addr - 8);
                break;
            }
        }
    }
    
    LOGF_WARNING("[BOOT]       Executed {} constructors, skipped {} null entries", ctorCount, skipCount);
    ctx.r3.s64 = 0;  // Return success
    
    // =========================================================================
    // PHASE 7: Enter game main (KEEP - game code)
    // =========================================================================
    //     * Indirect vtable call for game-specific init
    //     * Cleanup functions
    //   - Arguments: r3=argc, r4=argv, r5=envp
    //   - KEEP: This is the game!
    
    // Build command-line (empty for now, can be extended)
    std::vector<std::string> commandLineArgs;
    // Future: commandLineArgs = Config::GetLaunchArgs();
    
    int argc = 0;
    uint32_t argv = 0;
    
    if (!commandLineArgs.empty()) {
        argc = BuildGuestCommandLine(base, commandLineArgs);
        argv = BootGlobals::CMDLINE_ARGV_ADDR;
        LOGF_WARNING("[BOOT]       Built {} command-line args", argc);
    }
    
    LOGF_WARNING("[BOOT] [7/7] CALL sub_8218BEA8 (game main) argc={} argv=0x{:08X}", argc, argv);
    LOG_WARNING("[BOOT] ============================================================");
    
    ctx.r3.s64 = argc;
    ctx.r4.u64 = argv;
    ctx.r5.s64 = 0;  // envp = NULL
    __imp__sub_8218BEA8(ctx, base);
    
    LOG_WARNING("[BOOT] ============================================================");
    LOGF_WARNING("[BOOT] * Game main returned, r3={}", ctx.r3.u32);
    LOG_WARNING("[BOOT] ============================================================");
}
#endif  // DISABLED _xstart replacement

// =============================================================================
// INITIALIZATION FLOW TRACING
// =============================================================================

//   r3 = [r31+0]      // render_ctx object
//   r11 = [r3+0]      // vtable (should be 0x82000970)
//   r11 = [r11+4]     // vtable[1] - ENGINE METHOD
//   bctrl             // CALL - THIS BLOCKS
//
// We need to trace what function is at vtable[1]

// The vtable call happens AFTER this function returns
static void trace_vtable_after_82856C90(PPCContext& ctx, uint8_t* base) {
    // lwz r3,0(r31)   - load render_ctx from global
    // lwz r11,0(r3)   - load vtable
    // lwz r11,4(r11)  - load vtable[1]
    // bctrl           - call it
    
    // r31 contains the global pointer (0x83042DEC based on playbook)
    // Let's read what's there
    uint32_t globalPtr = 0x83042DEC;  // From playbook: render_ctx stored here
    uint32_t renderCtx = PPC_LOAD_U32(globalPtr);
    if (renderCtx != 0) {
        uint32_t vtable = PPC_LOAD_U32(renderCtx + 0);
        uint32_t vtable1 = PPC_LOAD_U32(vtable + 4);
        LOGF_WARNING("[VTABLE] After sub_82856C90: render_ctx=0x{:08X} vtable=0x{:08X} vtable[1]=0x{:08X}", 
                     renderCtx, vtable, vtable1);
    }
}

static std::atomic<int> g_initResult{-999};  // -999 = not initialized yet

// Set to true to use the new GameInit module, false to use original PPC code
static constexpr bool USE_GAME_INIT_MODULE = false;  // TODO: Enable when ready


// Subsystems after Camera system [15-30] - INSTRUMENTATION TO FIND BLOCKER


// =============================================================================
// =============================================================================


// =============================================================================
// =============================================================================
// This function opens a configuration file for streaming initialization.
// If the file doesn't exist (common on PC), it should return 0 to signal failure.
// =============================================================================

// =============================================================================
// STREAM FUNCTION BYPASSES REMOVED - Testing vtable fix with corrected addresses
// =============================================================================
// The null stream vtable now uses addresses within valid PPC code range.
// Bypasses removed to test if vtable fix works properly.
// =============================================================================

// =============================================================================
// =============================================================================
// This function tries to open platform:/stream.ini and parse streaming config.
// FIXED: Instead of calling original (which fails due to corrupted buffer),
// we directly apply the stream.ini values and initialize the stream pool.
//
// stream.ini contents:
//   virtual           = 0
//   physical          = 226544 (0x374F0)
//   virtual_optimised = 0
//   physical_optimised = 226832 (0x37610)
// =============================================================================


// ============================================================================
// SHADER BINDING HOOKS - HYBRID APPROACH
// Call original PPC code to maintain game state. The GUEST_FUNCTION_HOOKs
// in video.cpp won't work because they replace the function entirely,
// breaking the game's state management.
// ============================================================================


// DrawPrimitive hook is implemented in gpu/video.cpp


// =============================================================================
// =============================================================================
// This function parses Xbox 360 config files and accesses vtables that are
// uninitialized on PC. Attempting to call through garbage vtable pointers
// causes PAC authentication failures (crash at 0xd73f0b51f2e09f11).
// SOLUTION: Bypass and return 0 (success/null result)
// =============================================================================


// =============================================================================
// =============================================================================
// This function looks up strings/resources from game data files.
// We trace but let it run - the vtable method handles file reading.
// =============================================================================


// =============================================================================
// =============================================================================
// This function originally calls vtable[27] on storage device for file reading.
// Since dynamic vtable registration is unreliable, we reimplement the logic
// directly using VFS file operations.
//
// Original flow:
//   3. vtable[27] called with (device, pathBuffer, outputPtr) -> bytesRead
//   5. Else return 1
//
// Our implementation:
//   - Format path, resolve via VFS, return file info
// =============================================================================


// =============================================================================
// =============================================================================
// This function loads string tables from game files.
// With vtable[27] properly implemented, this should work naturally.
// We trace but let it run.
// =============================================================================


// =============================================================================
// =============================================================================
// FIXED: Let original run - no blocking detected in call tree.
// This triggers XAudioRegisterRenderDriverClient via vtable call.
// =============================================================================

// =============================================================================
// =============================================================================
// this function can also run safely.
// =============================================================================





// =============================================================================
// =============================================================================
// Original Xbox behavior: Parses semicolon-separated path config string using
// strtok, stores up to 4 paths in context structure at offsets 0, 256, 512, 768.
// Also handles '*' prefixed paths for special counting.
//
// This reimplementation uses native C++ string handling to avoid TLS issues
// with the PPC strtok implementation. Properly populates all path slots so
//
// Context structure layout:
//   [ctx+0]    = path slot 0 (256 bytes)
//   [ctx+256]  = path slot 1 (256 bytes)
//   [ctx+512]  = path slot 2 (256 bytes)
//   [ctx+768]  = path slot 3 (256 bytes)
//   [ctx+3076] = pathCount (number of paths parsed, 1-4)
//   [ctx+3080] = specialCount (count of '*' prefixed paths)
// =============================================================================


// =============================================================================
// =============================================================================
// This function blocks on Xbox-specific path validation.
//
// PC replacement: Skip validation, just store path length and return success.
// =============================================================================

// =============================================================================
// =============================================================================
// Original Xbox behavior: Validates path and registers it with storage system.
//
// PC replacement: Skip path validation - VFS handles all paths.
// Just store the path length and return success.
// =============================================================================

// =============================================================================
// =============================================================================
// Original Xbox behavior: Checks if storage path is configured at global addr
//
// PC replacement: Always return 1 - VFS handles all paths
// =============================================================================

// =============================================================================
// =============================================================================
// Original Xbox behavior: Takes path string, returns storage device object
// with vtable for file operations. Compares path prefixes to find device type.
//
// PC replacement: Return our PC storage device that uses VFS for all paths.
// The device has a vtable with functions that delegate to VFS.
// =============================================================================
static bool s_pcStorageInitialized = false;

// This is used by vtable[19] to populate the FileStream structure
static uint32_t g_lastShaderFileHandle = 0;

// Addresses for our custom vtable implementations
// These must be within valid code range: 0x82120000 to 0x82A13D5C (PPC_CODE_BASE + PPC_CODE_SIZE)
// Using high addresses within range that are unlikely to conflict with game code
constexpr uint32_t STORAGE_DEVICE_READ_ADDR = 0x82A13D00;
constexpr uint32_t STORAGE_DEVICE_GETFILEINFO_ADDR = 0x82A13D10;
constexpr uint32_t STORAGE_DEVICE_READFILE_ADDR = 0x82A13D20;
constexpr uint32_t STORAGE_DEVICE_CLOSEFILE_ADDR = 0x82A13D30;
constexpr uint32_t STORAGE_DEVICE_VTABLE6_ADDR = 0x82A13D60;   // vtable[6] offset 24
constexpr uint32_t STORAGE_DEVICE_VTABLE7_ADDR = 0x82A13D70;   // vtable[7] offset 28
constexpr uint32_t STORAGE_DEVICE_VTABLE9_ADDR = 0x82A13D80;   // vtable[9] offset 36
constexpr uint32_t STORAGE_DEVICE_VTABLE17_ADDR = 0x82A13D90;  // vtable[17] offset 68
constexpr uint32_t STORAGE_DEVICE_VTABLE21_ADDR = 0x82A13D50;  // vtable[21] offset 84
constexpr uint32_t STORAGE_DEVICE_VTABLE22_ADDR = 0x82A13DA0;  // vtable[22] offset 88
constexpr uint32_t STORAGE_DEVICE_VTABLE23_ADDR = 0x82A13D40;  // vtable[23] offset 92

// =============================================================================
// StorageDevice_ReadFile - vtable[27] implementation
// =============================================================================
// Parameters:
//   r3 = storage device pointer
//   r4 = path buffer (null-terminated string in guest memory)
//   r5 = output pointer (where to write result data pointer)
// Returns:
//   r3 = VALIDATION TOKEN (7) on success, 0 on failure
//   Writes data pointer to [r5]
// =============================================================================
static void StorageDevice_ReadFile(PPCContext& ctx, uint8_t* base) {
    static int s_count = 0; ++s_count;
    
    uint32_t devicePtr = ctx.r3.u32;
    uint32_t pathAddr = ctx.r4.u32;
    uint32_t outputAddr = ctx.r5.u32;
    
    // Read path from guest memory
    char pathBuf[260] = {0};
    for (int i = 0; i < 259 && pathAddr != 0; i++) {
        uint8_t c = PPC_LOAD_U8(pathAddr + i);
        if (c == 0) break;
        pathBuf[i] = c;
    }
    
    // COMPREHENSIVE LOGGING: Log ALL vtable[27] calls
    if (App::s_logVfs)
    {
        printf("[VTABLE27] #%d path='%s' device=0x%08X output=0x%08X LR=0x%08X\n",
               s_count, pathBuf, devicePtr, outputAddr, GET_CTX_LR(ctx));
        fflush(stdout);
    }
    
    // Resolve path via VFS
    std::string guestPath(pathBuf);
    auto hostPath = VFS::Resolve(guestPath);
    
    if (!VFS::Exists(guestPath)) {
        if (App::s_logVfs)
        {
            printf("[VTABLE27] #%d -> NOT FOUND\n", s_count);
            fflush(stdout);
        }
        // Write 0 to output pointer
        if (outputAddr != 0) {
            PPC_STORE_U32(outputAddr, 0);
        }
        ctx.r3.s64 = 0;  // Return 0 = file not found
        return;
    }
    
    // Get file size
    uint64_t fileSize = VFS::GetFileSize(guestPath);
    
    if (App::s_logVfs)
    {
        printf("[VTABLE27] #%d -> FOUND size=%llu bytes, returning token=7\n", s_count, (unsigned long long)fileSize);
        fflush(stdout);
    }
    
    if (outputAddr != 0) {
        PPC_STORE_U32(outputAddr, static_cast<uint32_t>(fileSize));
    }
    
    // CRITICAL FIX: Return validation token (7), NOT file size
    // If return != 7, it fails. The file size goes to the output pointer.
    ctx.r3.s64 = 7;  // Return validation token for success
}

// Helper to dynamically register a function in the PPC function table
static bool RegisterDynamicFunction(uint32_t guestAddr, PPCFunc* hostFunc) {
    g_memory.InsertFunction(guestAddr, hostFunc);
    LOGF_WARNING("[STORAGE] Registered dynamic function at 0x{:08X}", guestAddr);
    return true;
}

// =============================================================================
// StorageDevice_ReadFileVtable5 - vtable[5] implementation
// =============================================================================
// Parameters (PPC calling convention):
//   r3 = storage device pointer
//   r4 = file handle
//   r5 = buffer pointer (in PPC memory)
//   r6 = size to read
// Returns:
//   r3 = bytes read, or -1 on error
// =============================================================================
static void StorageDevice_ReadFileVtable5(PPCContext& ctx, uint8_t* base) {
    static int s_count = 0; ++s_count;
    
    uint32_t deviceAddr = ctx.r3.u32;
    uint32_t fileHandle = ctx.r4.u32;
    uint32_t bufferAddr = ctx.r5.u32;
    uint32_t sizeToRead = ctx.r6.u32;
    
    LOGF_WARNING("[vtable[5]] ReadFile #{} device=0x{:08X} handle=0x{:08X} buffer=0x{:08X} size={}",
                 s_count, deviceAddr, fileHandle, bufferAddr, sizeToRead);
    
    // Use the existing RPF reading infrastructure
    auto it = g_ntFileHandles.find(fileHandle);
    if (it == g_ntFileHandles.end()) {
        LOGF_WARNING("[vtable[5]] Invalid file handle 0x{:08X}", fileHandle);
        ctx.r3.s32 = -1;
        return;
    }
    
    NtFileHandle* hFile = it->second;
    uint8_t* hostBuffer = base + bufferAddr;
    
    // For shader files in common.rpf, we need to read from the correct offset
    // The game expects to read shader file headers starting with magic "rgxa" (0x61786772)
    // Use ReadFromBestRpf which handles offset-based reading from RPF files
    std::string rpfName;
    uint32_t bytesRead = 0;
    
    // Try to read using the RPF offset system
    // For shader validation, the game typically reads from offset 0 of the "file"
    // which corresponds to specific offsets in common.rpf
    const uint32_t chosen = ReadFromBestRpf(fileHandle, hostBuffer, sizeToRead, 0, s_count, rpfName);
    
    if (chosen != UINT32_MAX) {
        // ReadFromBestRpf succeeded, data is already in hostBuffer
        // Return the size that was requested (ReadFromBestRpf always reads full size)
        LOGF_WARNING("[vtable[5]] Read {} bytes from RPF '{}' at offset 0", sizeToRead, rpfName);
        ctx.r3.s32 = static_cast<int32_t>(sizeToRead);
        return;
    }
    
    // Fallback: direct file stream read
    hFile->stream.read(reinterpret_cast<char*>(hostBuffer), sizeToRead);
    bytesRead = static_cast<uint32_t>(hFile->stream.gcount());
    
    LOGF_WARNING("[vtable[5]] Direct read {} bytes from handle 0x{:08X}", bytesRead, fileHandle);
    
    ctx.r3.s32 = static_cast<int32_t>(bytesRead);
}

// Compile-first stubs for GTA V imports that do not have host implementations yet.
#define GTA5_IMPORT_STUB(name) PPC_FUNC(name) { ctx.r3.u64 = 0; }
GTA5_IMPORT_STUB(__imp__XamUserGetOnlineCountryFromXUID)
GTA5_IMPORT_STUB(__imp__XamGetLanguage)
GTA5_IMPORT_STUB(__imp__KeInsertQueueDpc)
GTA5_IMPORT_STUB(__imp__XamUserGetMembershipTierFromXUID)
GTA5_IMPORT_STUB(__imp__XAudioGetDuckerLevel)
GTA5_IMPORT_STUB(__imp__XNetLogonGetTitleID)
GTA5_IMPORT_STUB(__imp__KeInitializeDpc)
GTA5_IMPORT_STUB(__imp__XamVoiceIsActiveProcess)
GTA5_IMPORT_STUB(__imp__XNetLogonGetMachineID)
GTA5_IMPORT_STUB(__imp__NetDll_XHttpStartup)
GTA5_IMPORT_STUB(__imp__NetDll_XHttpShutdown)
GTA5_IMPORT_STUB(__imp__NetDll_XHttpOpen)
GTA5_IMPORT_STUB(__imp__NetDll_XHttpCloseHandle)
GTA5_IMPORT_STUB(__imp__NetDll_XHttpConnect)
GTA5_IMPORT_STUB(__imp__NetDll_XHttpSetStatusCallback)
GTA5_IMPORT_STUB(__imp__NetDll_XHttpOpenRequest)
GTA5_IMPORT_STUB(__imp__NetDll_XHttpSendRequest)
GTA5_IMPORT_STUB(__imp__NetDll_XHttpReceiveResponse)
GTA5_IMPORT_STUB(__imp__NetDll_XHttpQueryHeaders)
GTA5_IMPORT_STUB(__imp__NetDll_XHttpReadData)
GTA5_IMPORT_STUB(__imp__NetDll_XHttpWriteData)
GTA5_IMPORT_STUB(__imp__NetDll_XHttpDoWork)
GTA5_IMPORT_STUB(__imp__NetDll_XNetRegisterKey)
GTA5_IMPORT_STUB(__imp__NetDll_XNetUnregisterKey)
GTA5_IMPORT_STUB(__imp__NetDll_XNetInAddrToXnAddr)
GTA5_IMPORT_STUB(__imp__NetDll_XNetInAddrToServer)
GTA5_IMPORT_STUB(__imp__NetDll_XNetConnect)
GTA5_IMPORT_STUB(__imp__NetDll_XNetDnsLookup)
GTA5_IMPORT_STUB(__imp__NetDll_XNetDnsRelease)
GTA5_IMPORT_STUB(__imp__NetDll_XNetGetOpt)
GTA5_IMPORT_STUB(__imp__NetDll_XnpLogonGetStatus)
GTA5_IMPORT_STUB(__imp__XampXAuthStartup)
GTA5_IMPORT_STUB(__imp__XampXAuthShutdown)
GTA5_IMPORT_STUB(__imp__XampXAuthGetTitleBuffer)
GTA5_IMPORT_STUB(__imp__XamGetToken)
GTA5_IMPORT_STUB(__imp__XamFreeToken)
GTA5_IMPORT_STUB(__imp__XamUserGetDeviceContext)
GTA5_IMPORT_STUB(__imp__XamShowKeyboardUI)
GTA5_IMPORT_STUB(__imp__XamShowMarketplaceUI)
GTA5_IMPORT_STUB(__imp__XamShowMessageComposeUI)
GTA5_IMPORT_STUB(__imp__XamShowFriendRequestUI)
GTA5_IMPORT_STUB(__imp__XamShowMarketplaceDownloadItemsUI)
GTA5_IMPORT_STUB(__imp__XamMarketplaceAcquireFreeContent)
GTA5_IMPORT_STUB(__imp__XAudioGetDuckerThreshold)
GTA5_IMPORT_STUB(__imp__XAudioGetDuckerHoldTime)
GTA5_IMPORT_STUB(__imp__XAudioGetDuckerAttackTime)
GTA5_IMPORT_STUB(__imp__XAudioGetDuckerReleaseTime)
GTA5_IMPORT_STUB(__imp__XAudioEnableDucker)
GTA5_IMPORT_STUB(__imp__KeSetCurrentProcessType)
GTA5_IMPORT_STUB(__imp__VdSetDisplayModeOverride)
GTA5_IMPORT_STUB(__imp__NtWriteFileGather)
GTA5_IMPORT_STUB(__imp__XamBackgroundDownloadSetMode)
GTA5_IMPORT_STUB(__imp__XamBackgroundDownloadGetMode)
GTA5_IMPORT_STUB(__imp__XamBackgroundDownloadItemGetStatus)
GTA5_IMPORT_STUB(__imp__XamBackgroundDownloadItemGetHistoryStatus)
GTA5_IMPORT_STUB(__imp__XamContentResolve)
GTA5_IMPORT_STUB(__imp__XamSwapCancel)
GTA5_IMPORT_STUB(__imp__XexUnloadImage)
#undef GTA5_IMPORT_STUB

PPC_FUNC(__imp__XamSwapDisc)
{
    const uint32_t discNumber = ctx.r3.u32;
    const uint32_t completion = ctx.r4.u32;
    bool signaled = false;

    if (IsValidKernelHandle(completion)) {
        if (Event* event = GetKernelObject<Event>(completion)) {
            event->Set();
            signaled = true;
        }
    } else if (completion >= 0x10000 &&
               completion <= PPC_MEMORY_SIZE - sizeof(XDISPATCHER_HEADER)) {
        auto* header = reinterpret_cast<XDISPATCHER_HEADER*>(g_memory.Translate(completion));
        if (header->Type == 0 || header->Type == 1) {
            QueryKernelObject<Event>(*header)->Set();
            header->SignalState = 1;
            signaled = true;
        }
    }

    if (signaled) {
        ++g_keSetEventGeneration;
        g_keSetEventGeneration.notify_all();
        LOGF_INFO("[XamSwapDisc] disc={} completion=0x{:08X} signaled", discNumber, completion);
        ctx.r3.u64 = ERROR_SUCCESS;
    } else {
        LOGF_WARNING("[XamSwapDisc] disc={} invalid completion event=0x{:08X}",
                     discNumber, completion);
        ctx.r3.u64 = ERROR_INVALID_PARAMETER;
    }
}

PPC_FUNC(__imp__NtYieldExecution)
{
    std::this_thread::yield();
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__MmMapIoSpace)
{
    const uint32_t lr = GET_CALLER_LR();
    uint32_t physAddr;
    uint32_t size;
    uint32_t protect;

    if (ctx.r3.u32 == 0 && ctx.r4.u32 != 0) {
        // 64-bit LARGE_INTEGER PhysicalAddress in r3:r4
        physAddr = ctx.r4.u32;
        size = ctx.r5.u32;
        protect = ctx.r6.u32;
    } else {
        physAddr = ctx.r3.u32;
        size = ctx.r4.u32;
        protect = ctx.r5.u32;
    }

    LOGF_INFO("[MmMapIoSpace] caller=0x{:08X} physAddr=0x{:08X} size=0x{:X} prot=0x{:X}",
              lr, physAddr, size, protect);

    // In FiveInvader, virtual space directly maps physical space (1:1 address translation)
    // If physAddr is 0, allocate physical memory block to avoid returning NULL
    if (physAddr == 0) {
        void* host = g_userHeap.AllocPhysical(size ? size : 0x1000, 0x1000);
        if (host) {
            physAddr = g_memory.MapVirtual(host);
            LOGF_WARNING("[MmMapIoSpace] physAddr was 0, allocated fallback virtual address 0x{:08X}", physAddr);
        }
    }

    ctx.r3.u64 = physAddr;
}

PPC_FUNC(__imp__XexLoadImage)
{
    const uint32_t lr = GET_CALLER_LR();
    const char* name = ctx.r3.u32 ? (const char*)(base + ctx.r3.u32) : "NULL";
    LOGF_WARNING("[XexLoadImage] caller=0x{:08X} name='{}' flags=0x{:X}", lr, name, ctx.r4.u32);
    // Write pseudo module handle if output pointer provided in r6
    if (ctx.r6.u32 != 0) {
        *reinterpret_cast<be<uint32_t>*>(base + ctx.r6.u32) = 0x82000000;
    }
    ctx.r3.u64 = 0; // STATUS_SUCCESS
}

PPC_FUNC(__imp__XamUserGetAge)
{
    if (ctx.r4.u32 != 0) {
        *reinterpret_cast<be<uint32_t>*>(base + ctx.r4.u32) = 25;
    }
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamUserGetAgeGroup)
{
    if (ctx.r4.u32 != 0) {
        *reinterpret_cast<be<uint32_t>*>(base + ctx.r4.u32) = 3;
    }
    ctx.r3.u64 = 0;
}

// =============================================================================
// StorageDevice_CloseFile - vtable[10] implementation
// =============================================================================
// Parameters (PPC calling convention):
//   r3 = storage device pointer
//   r4 = file handle
// Returns:
//   void (no return value)
// =============================================================================
static void StorageDevice_CloseFile(PPCContext& ctx, uint8_t* base) {
    static int s_count = 0; ++s_count;
    
    uint32_t deviceAddr = ctx.r3.u32;
    uint32_t fileHandle = ctx.r4.u32;
    
    LOGF_WARNING("[vtable[10]] CloseFile #{} device=0x{:08X} handle=0x{:08X}",
                 s_count, deviceAddr, fileHandle);
    
    // Close the file handle if it exists
    auto it = g_ntFileHandles.find(fileHandle);
    if (it != g_ntFileHandles.end()) {
        NtFileHandle* hFile = it->second;
        if (hFile->stream.is_open()) {
            hFile->stream.close();
        }
        // Note: Don't delete the handle yet as it might be reused
        LOGF_WARNING("[vtable[10]] Closed file handle 0x{:08X}", fileHandle);
    } else {
        LOGF_WARNING("[vtable[10]] Handle 0x{:08X} not found (already closed?)", fileHandle);
    }
}

// =============================================================================
// StorageDevice_GetFileInfo - vtable[19] implementation
// =============================================================================
// Called by shader loading code to get file metadata
// Parameters (PPC calling convention):
//   r3 = storage device pointer
//   r4 = path string pointer
// Returns:
//   r3 = file info (64-bit value or pointer)
// =============================================================================
static void StorageDevice_GetFileInfo(PPCContext& ctx, uint8_t* base) {
    static int s_count = 0; ++s_count;
    
    uint32_t deviceAddr = ctx.r3.u32;
    uint32_t pathAddr = ctx.r4.u32;
    
    // Read path string for logging
    char path[256] = {0};
    for (int i = 0; i < 255; i++) {
        uint8_t c = PPC_LOAD_U8(pathAddr + i);
        if (c == 0) break;
        path[i] = c;
    }
    
    LOGF_WARNING("[vtable[19]] GetFileInfo #{} device=0x{:08X} path='{}' LR=0x{:08X}",
                 s_count, deviceAddr, path, GET_CTX_LR(ctx));
    
    // Phase 1B: Allocate and return a FileStream structure
    //   +0:  Storage device pointer
    //   +4:  File handle
    //   +8:  Buffer pointer
    //   +12: Bytes read counter
    //   +16: Current position in buffer
    //   +20: Buffer end position
    //   +24: Buffer size
    
    // Allocate FileStream structure (32 bytes minimum)
    static uint32_t s_streamAddr = 0;
    if (s_streamAddr == 0) {
        // Allocate in high memory area (near our vtable functions)
        s_streamAddr = 0x82A14000;
    }
    
    // This is tracked globally when shader files are opened
    uint32_t fileHandle = g_lastShaderFileHandle;
    
    // Initialize FileStream structure
    PPC_STORE_U32(s_streamAddr + 0, deviceAddr);      // Storage device
    PPC_STORE_U32(s_streamAddr + 4, fileHandle);      // File handle
    PPC_STORE_U32(s_streamAddr + 8, 0);               // Buffer (NULL for now)
    PPC_STORE_U32(s_streamAddr + 12, 0);              // Bytes read
    PPC_STORE_U32(s_streamAddr + 16, 0);              // Current position
    PPC_STORE_U32(s_streamAddr + 20, 0);              // Buffer end
    PPC_STORE_U32(s_streamAddr + 24, 4096);           // Buffer size
    
    LOGF_WARNING("[vtable[19]] Allocated FileStream at 0x{:08X} with handle=0x{:08X}",
                 s_streamAddr, fileHandle);
    
    // Return pointer to FileStream structure
    ctx.r3.u64 = s_streamAddr;
}

// =============================================================================
// Generic vtable stub implementations for storage device
// =============================================================================
void StorageDevice_Vtable6(PPCContext& ctx, uint8_t* base) {
    static int s_count = 0;
    if (++s_count <= 10) LOGF_WARNING("[vtable[6]] #{} device=0x{:08X} r4=0x{:08X}", s_count, ctx.r3.u32, ctx.r4.u32);
    ctx.r3.s32 = 0;
}

void StorageDevice_Vtable7(PPCContext& ctx, uint8_t* base) {
    static int s_count = 0;
    if (++s_count <= 10) LOGF_WARNING("[vtable[7]] #{} device=0x{:08X} r4=0x{:08X}", s_count, ctx.r3.u32, ctx.r4.u32);
    ctx.r3.s32 = 0;
}

void StorageDevice_Vtable9(PPCContext& ctx, uint8_t* base) {
    static int s_count = 0;
    if (++s_count <= 10) LOGF_WARNING("[vtable[9]] #{} device=0x{:08X} r4=0x{:08X}", s_count, ctx.r3.u32, ctx.r4.u32);
    ctx.r3.s32 = 0;
}

void StorageDevice_Vtable17(PPCContext& ctx, uint8_t* base) {
    static int s_count = 0;
    if (++s_count <= 10) LOGF_WARNING("[vtable[17]] #{} device=0x{:08X} r4=0x{:08X}", s_count, ctx.r3.u32, ctx.r4.u32);
    ctx.r3.s32 = 0;
}

void StorageDevice_Vtable22(PPCContext& ctx, uint8_t* base) {
    static int s_count = 0;
    if (++s_count <= 10) LOGF_WARNING("[vtable[22]] #{} device=0x{:08X} r4=0x{:08X}", s_count, ctx.r3.u32, ctx.r4.u32);
    ctx.r3.s32 = 0;
}

// =============================================================================
// StorageDevice_Vtable21 - vtable[21] implementation (offset 84)
// =============================================================================
// Parameters (PPC calling convention):
//   r3 = device object pointer
//   r4 = path buffer
//   r5 = another buffer/path
// Returns: result value stored to r23
// =============================================================================
void StorageDevice_Vtable21(PPCContext& ctx, uint8_t* base) {
    static int s_count = 0;
    ++s_count;
    
    uint32_t deviceAddr = ctx.r3.u32;
    uint32_t pathAddr = ctx.r4.u32;
    uint32_t param2 = ctx.r5.u32;
    
    if (s_count <= 20 || s_count % 100 == 0) {
        LOGF_WARNING("[vtable[21]] #{} device=0x{:08X} path=0x{:08X} param2=0x{:08X} LR=0x{:08X}",
                     s_count, deviceAddr, pathAddr, param2, GET_CTX_LR(ctx));
    }
    
    // Return success (0) - this appears to be a path/file validation function
    ctx.r3.s32 = 0;
}

// =============================================================================
// StorageDevice_Vtable23 - vtable[23] implementation (offset 92)
// =============================================================================
// Parameters (PPC calling convention):
//   r3 = device object pointer
//   r4 = parameter (possibly path length or index)
// Returns: appears to be called before vtable[21] (offset 84)
// =============================================================================
void StorageDevice_Vtable23(PPCContext& ctx, uint8_t* base) {
    static int s_count = 0;
    ++s_count;
    
    uint32_t deviceAddr = ctx.r3.u32;
    uint32_t param = ctx.r4.u32;
    
    if (s_count <= 20 || s_count % 100 == 0) {
        LOGF_WARNING("[vtable[23]] #{} device=0x{:08X} param=0x{:08X} LR=0x{:08X}",
                     s_count, deviceAddr, param, GET_CTX_LR(ctx));
    }
    
    // Return success (0) - this appears to be a validation/setup function
    ctx.r3.s32 = 0;
}

// Initialize PC storage device structure in guest memory
static void InitializePCStorageDevice(uint8_t* base) {
    if (s_pcStorageInitialized) return;
    
    LOG_WARNING("[STORAGE] Initializing PC storage device");
    
    // Register our vtable implementations
    if (!RegisterDynamicFunction(STORAGE_DEVICE_READFILE_ADDR, StorageDevice_ReadFileVtable5)) {
        LOG_WARNING("[STORAGE] WARNING: Failed to register StorageDevice_ReadFileVtable5");
    }
    if (!RegisterDynamicFunction(STORAGE_DEVICE_CLOSEFILE_ADDR, StorageDevice_CloseFile)) {
        LOG_WARNING("[STORAGE] WARNING: Failed to register StorageDevice_CloseFile");
    }
    if (!RegisterDynamicFunction(STORAGE_DEVICE_GETFILEINFO_ADDR, StorageDevice_GetFileInfo)) {
        LOG_WARNING("[STORAGE] WARNING: Failed to register StorageDevice_GetFileInfo");
    }
    if (!RegisterDynamicFunction(STORAGE_DEVICE_VTABLE6_ADDR, StorageDevice_Vtable6)) {
        LOG_WARNING("[STORAGE] WARNING: Failed to register StorageDevice_Vtable6");
    }
    if (!RegisterDynamicFunction(STORAGE_DEVICE_VTABLE7_ADDR, StorageDevice_Vtable7)) {
        LOG_WARNING("[STORAGE] WARNING: Failed to register StorageDevice_Vtable7");
    }
    if (!RegisterDynamicFunction(STORAGE_DEVICE_VTABLE9_ADDR, StorageDevice_Vtable9)) {
        LOG_WARNING("[STORAGE] WARNING: Failed to register StorageDevice_Vtable9");
    }
    if (!RegisterDynamicFunction(STORAGE_DEVICE_VTABLE17_ADDR, StorageDevice_Vtable17)) {
        LOG_WARNING("[STORAGE] WARNING: Failed to register StorageDevice_Vtable17");
    }
    if (!RegisterDynamicFunction(STORAGE_DEVICE_VTABLE21_ADDR, StorageDevice_Vtable21)) {
        LOG_WARNING("[STORAGE] WARNING: Failed to register StorageDevice_Vtable21");
    }
    if (!RegisterDynamicFunction(STORAGE_DEVICE_VTABLE22_ADDR, StorageDevice_Vtable22)) {
        LOG_WARNING("[STORAGE] WARNING: Failed to register StorageDevice_Vtable22");
    }
    if (!RegisterDynamicFunction(STORAGE_DEVICE_VTABLE23_ADDR, StorageDevice_Vtable23)) {
        LOG_WARNING("[STORAGE] WARNING: Failed to register StorageDevice_Vtable23");
    }
    if (!RegisterDynamicFunction(STORAGE_DEVICE_READ_ADDR, StorageDevice_ReadFile)) {
        LOG_WARNING("[STORAGE] WARNING: Failed to register StorageDevice_ReadFile");
    }
    
    // Set up device object: vtable pointer at offset 0
    PPC_STORE_U32(StorageConstants::PC_STORAGE_DEVICE_ADDR + 0, StorageConstants::PC_STORAGE_VTABLE_ADDR);
    
    // Fill vtable with placeholder values for unused slots
    // Use high addresses in code range so they're at least in valid range
    for (int i = 0; i < 120; i += 4) {
        PPC_STORE_U32(StorageConstants::PC_STORAGE_VTABLE_ADDR + i, 0x82120000 + i);
    }
    
    PPC_STORE_U32(StorageConstants::PC_STORAGE_VTABLE_ADDR + 20, STORAGE_DEVICE_READFILE_ADDR);
    
    // vtable[10] at offset 40 - CloseFile implementation
    PPC_STORE_U32(StorageConstants::PC_STORAGE_VTABLE_ADDR + 40, STORAGE_DEVICE_CLOSEFILE_ADDR);
    
    // vtable[19] at offset 76 - GetFileInfo implementation
    PPC_STORE_U32(StorageConstants::PC_STORAGE_VTABLE_ADDR + 76, STORAGE_DEVICE_GETFILEINFO_ADDR);
    
    // vtable[6] at offset 24
    PPC_STORE_U32(StorageConstants::PC_STORAGE_VTABLE_ADDR + 24, STORAGE_DEVICE_VTABLE6_ADDR);
    
    // vtable[7] at offset 28
    PPC_STORE_U32(StorageConstants::PC_STORAGE_VTABLE_ADDR + 28, STORAGE_DEVICE_VTABLE7_ADDR);
    
    // vtable[9] at offset 36
    PPC_STORE_U32(StorageConstants::PC_STORAGE_VTABLE_ADDR + 36, STORAGE_DEVICE_VTABLE9_ADDR);
    
    // vtable[17] at offset 68
    PPC_STORE_U32(StorageConstants::PC_STORAGE_VTABLE_ADDR + 68, STORAGE_DEVICE_VTABLE17_ADDR);
    
    // vtable[21] at offset 84
    PPC_STORE_U32(StorageConstants::PC_STORAGE_VTABLE_ADDR + 84, STORAGE_DEVICE_VTABLE21_ADDR);
    
    // vtable[22] at offset 88
    PPC_STORE_U32(StorageConstants::PC_STORAGE_VTABLE_ADDR + 88, STORAGE_DEVICE_VTABLE22_ADDR);
    
    // vtable[23] at offset 92
    PPC_STORE_U32(StorageConstants::PC_STORAGE_VTABLE_ADDR + 92, STORAGE_DEVICE_VTABLE23_ADDR);
    
    // vtable[27] at offset 108 - ReadFile implementation (alternative)
    PPC_STORE_U32(StorageConstants::PC_STORAGE_VTABLE_ADDR + 108, STORAGE_DEVICE_READ_ADDR);
    
    s_pcStorageInitialized = true;
    LOGF_WARNING("[STORAGE] PC storage device initialized, vtable[5]=0x{:08X} vtable[10]=0x{:08X} vtable[19]=0x{:08X} vtable[23]=0x{:08X} vtable[27]=0x{:08X}",
                 STORAGE_DEVICE_READFILE_ADDR, STORAGE_DEVICE_CLOSEFILE_ADDR, STORAGE_DEVICE_GETFILEINFO_ADDR, STORAGE_DEVICE_VTABLE23_ADDR, STORAGE_DEVICE_READ_ADDR);
}

static std::atomic<bool> g_afterStorageInit{false};

// =============================================================================
// =============================================================================
// Original Xbox behavior: Mounts Xbox content package, makes vtable calls:
//   [vtable+8]  - Open file
//   [vtable+72] - Get file size
//   [vtable+76] - Get file info
//   [vtable+24] - Read file
//   [vtable+44] - Close file
//
// PC replacement: Skip Xbox mount, initialize context for VFS-based access.
// The game stores results in the context structure at r3 (r31 in function).
// =============================================================================

// =============================================================================
// INSTRUMENTATION: Caller Range 0x827E0400-0x827E0500
// =============================================================================
// LR=0x827E049C suggests the caller is in this address range
// =============================================================================


// =============================================================================
// INSTRUMENTATION: Next Subsystem Functions
// =============================================================================
// These hooks track initialization progress after storage device init
// Based on game_init.cpp analysis of 63-subsystem init sequence
// =============================================================================


// =============================================================================
// INSTRUMENTATION: Additional Subsystem Init Functions
// =============================================================================
// Track progress through the 63-subsystem initialization sequence
// =============================================================================

// Subsystem #2 after streaming init

// Subsystem loader/iterator


// =============================================================================
// Internal calls:
// =============================================================================

// =============================================================================
// =============================================================================
// This function orchestrates GPU state initialization including:
//
// All of these lead to vtable calls that expect Xbox GPU hardware.
// 
// SOLUTION: Bypass entirely. Our shader cache (g_shaderCache in video.cpp)
// already provides pre-compiled platform-native shaders. Shader loading
// happens on-demand via GetOrLinkShader() when the render path needs them.
//
// This collapse eliminates the "first call works, second call hangs" pattern
// caused by mixing partial Xbox GPU state with partial modern GPU state.
// =============================================================================

// =============================================================================
// =============================================================================
// This function performs GPU synchronization with a retry loop.
// After 25 failed retries (counter at offset 30780 >= 25), it enters an
// infinite loop at loc_8285B214: goto loc_8285B214;
// 
// Problem: Xbox GPU fence/sync operations don't work on PC, causing timeout
// after 25 retries and entering infinite spin loop.
// 
// Solution: Bypass entirely - host GPU handles synchronization through
// the modern graphics layer (Metal/Vulkan/OpenGL).
// =============================================================================


// =============================================================================
// =============================================================================
// Original behavior:
//   - Loads GPU device from TLS[1676]
//   - Calls device->vtable[15] (offset 60) for resource validation/creation
//   - Returns 0 for success, negative for error
//
// Problem: TLS[1676] vtable chain is uninitialized on PC, causing hang.
//
// Solution: Bypass vtable call, return success. Game's own structures
// will be initialized by the allocation code that runs before the vtable call.
// =============================================================================


// =============================================================================
// =============================================================================
// This function loads an object from global+22000 and calls vtable[3] (offset 12)
// twice to set up shader resources. The vtable calls lead to complex GPU init
// code with many more vtable calls that block on PC.
//
// Solution: Bypass entirely - shader resources will be set up by host GPU layer.
// =============================================================================


// =============================================================================
// =============================================================================
// Original Xbox behavior: Constructs a file path from the storage device context.
// It reads a path configuration index from context+3072, and if non-zero,
// calculates an offset to read the path string from the context structure.
// If context+3072 is 0 and no source path is provided, it writes an empty string.
//
// PC replacement: Call the original implementation first. If it returns an empty
// path (due to uninitialized path config), construct a default path based on
// the path index. This allows shader loading to proceed without Xbox path config.
// =============================================================================


// =============================================================================
// =============================================================================
// Original Xbox behavior: Calls storage device vtable[1] to find/open a file.
// Our PC storage device has a fake vtable that crashes when called.
//
// PC replacement: Extract path from context, use VFS to check if file exists,
// return a valid file handle or 0 if not found.
// =============================================================================


// =============================================================================
// =============================================================================
// This function loads from TLS[1676] (GPU device) and calls vtable[7] (offset 28)
//
// SOLUTION: Bypass entirely. GPU setup handled by host rendering layer.
// =============================================================================

// =============================================================================
// =============================================================================
//
// SOLUTION: Bypass - shader effects handled by host shader cache
// =============================================================================


// =============================================================================
// =============================================================================
// This function initializes file streaming queues by processing file handles
// from an array at global+2656. The loop never terminates (infinite loop).
//
// Root cause: The array at global+2656 contains file handles that were
// registered during storage/file system init. The loop condition checks
// for NULL to terminate, but the array doesn't end with NULL.
//
// SOLUTION: Bypass - file streaming handled by VFS layer.
// =============================================================================


// Analysis: Contains busy-wait loops waiting for worker tasks that are stubbed.
// Since workers are stubbed, these loops would run forever.
// Stubbing this function allows init to continue.


// Analysis: Xbox 360 GPU hardware initialization that blocks waiting for hardware responses
// Host rendering layer in gpu/video.cpp handles GPU setup, so Xbox init can be bypassed

// Analysis: Xbox 360 world/game state initialization that likely blocks on hardware
// Host game engine handles world setup, so Xbox init can be bypassed


// Returns non-zero while tasks are pending. Force return 0 after iteration limit.


// This is called with r3=actual_worker_func, r4=context
// It should call the worker function - trace to see if it's blocking

// =============================================================================
// =============================================================================
// Original Xbox behavior:
//   r11 = [r13+0]           // Thread context base
//   r3 = [r11+1676]         // Memory manager pointer
//   r11 = [r3+0]            // Vtable
//   r11 = [r11+8]           // Allocation function
//   call r11(size, align=16, flags=0)
//
// Problem: Original code stores memory manager at fixed address 0x83130000+1676,
// but [r13+0] points to dynamic TLS address, causing NULL vtable lookup.
//
// Solution: Replace with direct allocation using guest heap, bypassing vtable.
// =============================================================================


// =============================================================================
// =============================================================================


// =============================================================================
// =============================================================================


// =============================================================================
// =============================================================================
// Original Xbox behavior: Allocates memory with flags, uses same vtable chain
// Problem: Same vtable chain issue - memory manager not initialized.
// Solution: Direct allocation using guest heap.
// =============================================================================


// =============================================================================
// =============================================================================


// Based on PPC disassembly analysis - addresses verified with MCP math tools

// =============================================================================
// SYNC PRIMITIVE HOOKS - Make blocking sync functions non-blocking
// These are the ROOT CAUSE of all the stubbed functions blocking
// =============================================================================

// This function enters critical section, sets event, WAITS on event at 0x82A97F5C, resets event, leaves critical section
// Fix: Pre-signal the event so the wait returns immediately

// This function calls XamTaskSchedule, then computes event addr 0x82A97F5C, then KeWaitForSingleObject
// The event must be signaled BEFORE the wait. We pre-signal it so the wait returns immediately.

// Fix: Call the task scheduling but return success immediately without blocking

// Original: Blocks waiting for async completion
// Fix: Execute the pre-wait setup, skip blocking, return success

// Direct PPC calls bypass PPCFuncMappings patches, so we pre-signal the event HERE

// =============================================================================
// REIMPLEMENTED: Previously stubbed functions - now call __imp__ to run fully
// The sync hooks above prevent blocking, so these can now execute normally
// =============================================================================

// The blocking sync functions are already hooked to be non-blocking, so let original run
// This follows MarathonRecomp pattern: hook blocking sub-functions, let parent run full init


// =============================================================================
// =============================================================================


// This is the massive initialization function that sets up all game systems

// Returns 0 on success (all init complete), -1 on failure

// =============================================================================
// =============================================================================

// Xbox 360 runtime called this repeatedly; recomp needs to emulate that

// =============================================================================
// FRAME RATE COUNTER - tracks actual FPS and frame timing
// =============================================================================
struct FrameRateCounter {
    std::chrono::steady_clock::time_point lastFrameTime;
    std::chrono::steady_clock::time_point lastFpsUpdate;
    int frameCount = 0;
    int framesThisSecond = 0;
    double currentFps = 0.0;
    double avgFrameTimeMs = 0.0;
    double minFrameTimeMs = 999999.0;
    double maxFrameTimeMs = 0.0;
    bool initialized = false;
    
    void Init() {
        lastFrameTime = std::chrono::steady_clock::now();
        lastFpsUpdate = lastFrameTime;
        initialized = true;
    }
    
    void Update() {
        if (!initialized) Init();
        
        auto now = std::chrono::steady_clock::now();
        double frameTimeMs = std::chrono::duration<double, std::milli>(now - lastFrameTime).count();
        lastFrameTime = now;
        
        frameCount++;
        framesThisSecond++;
        
        // Track min/max frame times
        if (frameTimeMs < minFrameTimeMs && frameCount > 5) minFrameTimeMs = frameTimeMs;
        if (frameTimeMs > maxFrameTimeMs && frameCount > 5) maxFrameTimeMs = frameTimeMs;
        
        // Update FPS every second
        double timeSinceUpdate = std::chrono::duration<double>(now - lastFpsUpdate).count();
        if (timeSinceUpdate >= 1.0) {
            currentFps = framesThisSecond / timeSinceUpdate;
            avgFrameTimeMs = (timeSinceUpdate * 1000.0) / framesThisSecond;
            
            LOGF_WARNING("[FPS] {:.1f} fps | avg={:.2f}ms min={:.2f}ms max={:.2f}ms | total frames={}",
                        currentFps, avgFrameTimeMs, minFrameTimeMs, maxFrameTimeMs, frameCount);
            
            framesThisSecond = 0;
            lastFpsUpdate = now;
            minFrameTimeMs = 999999.0;
            maxFrameTimeMs = 0.0;
        }
    }
};
static FrameRateCounter g_fpsCounter;


// This callback needs to signal semaphores that the scheduler is waiting on


// Validates stream struct pointers before dereferencing to prevent crash at 0x400000000
// Stream struct layout: [0]=object ptr, [4]=ctx, [8]=buffer, [12]=pos, [16]=cursor, [20]=end, [24]=capacity

// Helper to repair corrupted stream at 0x82003890
static void RepairCorruptedStream(uint32_t streamPtr)
{
    static int s_repairCount = 0;
    be<uint32_t>* stream = reinterpret_cast<be<uint32_t>*>(g_memory.Translate(streamPtr));
    stream[0] = 0;  // Null object - indicates stream not initialized
    stream[1] = 0;
    stream[2] = 0;
    stream[3] = 0;
    stream[4] = 0;
    stream[5] = 0;
    stream[6] = 0;
    if (++s_repairCount <= 10) {
        LOGF_WARNING("[sub_827E8420] Repaired corrupted stream at 0x{:08X} (repair #{})", streamPtr, s_repairCount);
    }
}

// =============================================================================
// =============================================================================
// This function reads stream[0] -> object -> vtable -> vtable[40] and calls it
// Crashes with PAC failure if vtable[40] is garbage/uninitialized

// =============================================================================
// =============================================================================
// This function loads a vtable from an object and calls a function at vtable+48.
// The PAC crash occurs when the vtable or function pointer is corrupted.
// 
// Original code flow:
//   r11 = r3 (object pointer)
//   r3 = [r11+0] (load vtable from object)
//   r4 = [r11+4] (load context)
//   r11 = [r3+0] (load vtable pointer from object)
//   r11 = [r11+48] (load function pointer from vtable+48)
//   call r11 (PAC CRASH HERE if pointer is invalid)
// =============================================================================

// =============================================================================
// =============================================================================
// It loads object from stream[0], then calls vtable[36] and vtable[52].
// The PAC crash occurs when vtable entries contain garbage pointers.
//
// Original code flow:
//   r31 = r3 (stream pointer)
//   r3 = [r31+0] (load object from stream)
//   r11 = [r3+0] (load vtable from object)
//   r11 = [r11+36] (load function from vtable+36)
//   call r11 <- CRASH HERE if garbage
//   ... later ...
//   r11 = [r11+52] (load function from vtable+52)
//   call r11 <- CRASH HERE if garbage
// =============================================================================

// =============================================================================
// =============================================================================
// Validates vtable[36] before calling original to prevent PAC crashes.
// =============================================================================

// =============================================================================
// =============================================================================
// Add validation here to catch bad stream objects before they cause PAC crashes.
// The stream object is passed in r3 and stored in r30.
// =============================================================================

// =============================================================================
// =============================================================================
// This function reads from stream and returns pointer to buffer or 0.
// Adding wrapper to trace crash in file parsing path.
// =============================================================================

// =============================================================================
// =============================================================================
// Prevents crash when called with NULL stream from failed file open.
// =============================================================================

// =============================================================================
// =============================================================================
// Prevents crash when called with NULL or invalid structure pointer.
// The crash occurs at line 39351: ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 3076);
// When r31 (from r3 parameter) is NULL, this reads from address 0x0C04 → crash.
// =============================================================================

// Forward declare the original implementation from recompiled code

// Track completed async operations to avoid re-serving data
// Key: async pointer, Value: pair<handle, offset> of completed operation
static std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> g_completedAsyncOps;

// SIMPLIFIED APPROACH: Call original recompiled function, then ensure async completion
// Parameters: r3=handle, r4=buffer(guest), r5=size, r6=offset, r7=asyncInfo


// sub_82192578 - Thread initialization called by recompiled code
// Let it run naturally - return success
// TODO: Replace GTA IV virtual address with GTA V offset
#if 0
PPC_FUNC(sub_82192578) {
    ctx.r3.u32 = 1;  // Return success
}

GUEST_FUNCTION_HOOK(sub_825383D8, XapiInitProcess)
#endif
GUEST_FUNCTION_HOOK(__imp__XGetVideoMode, VdQueryVideoMode); // XGetVideoMode
GUEST_FUNCTION_HOOK(__imp__XNotifyGetNext, XNotifyGetNext);
GUEST_FUNCTION_HOOK(__imp__XGetGameRegion, XGetGameRegion);
GUEST_FUNCTION_HOOK(__imp__XMsgStartIORequest, XMsgStartIORequest);

GUEST_FUNCTION_HOOK(__imp__XamGetSystemVersion, XamGetSystemVersion);
GUEST_FUNCTION_HOOK(__imp__XamContentCreateEx, XamContentCreateEx);
GUEST_FUNCTION_HOOK(__imp__XamContentDelete, XamContentDelete);
GUEST_FUNCTION_HOOK(__imp__XamContentClose, XamContentClose);
GUEST_FUNCTION_HOOK(__imp__XamContentGetCreator, XamContentGetCreator);
GUEST_FUNCTION_HOOK(__imp__XamContentCreateEnumerator, XamContentCreateEnumerator);
GUEST_FUNCTION_HOOK(__imp__XamContentGetDeviceState, XamContentGetDeviceState);
GUEST_FUNCTION_HOOK(__imp__XamContentGetDeviceData, XamContentGetDeviceData);
GUEST_FUNCTION_HOOK(__imp__XamEnumerate, XamEnumerate);
GUEST_FUNCTION_HOOK(__imp__XamNotifyCreateListener, XamNotifyCreateListener);
GUEST_FUNCTION_HOOK(__imp__XamUserGetSigninInfo, XamUserGetSigninInfo);
GUEST_FUNCTION_HOOK(__imp__XamShowSigninUI, XamShowSigninUI);
GUEST_FUNCTION_HOOK(__imp__XamShowDeviceSelectorUI, XamShowDeviceSelectorUI);
GUEST_FUNCTION_HOOK(__imp__XamShowMessageBoxUI, XamShowMessageBoxUI);
GUEST_FUNCTION_HOOK(__imp__XamUserCreateAchievementEnumerator, XamUserCreateAchievementEnumerator);
GUEST_FUNCTION_HOOK(__imp__XeKeysConsoleSignatureVerification, XeKeysConsoleSignatureVerification);
GUEST_FUNCTION_HOOK(__imp__XamGetPrivateEnumStructureFromHandle, XamGetPrivateEnumStructureFromHandle);
GUEST_FUNCTION_HOOK(__imp__XamTaskSchedule, XamTaskSchedule);
GUEST_FUNCTION_HOOK(__imp__XamTaskShouldExit, XamTaskShouldExit);
GUEST_FUNCTION_HOOK(__imp__XamTaskCloseHandle, XamTaskCloseHandle);
GUEST_FUNCTION_HOOK(__imp__XamUserAreUsersFriends, XamUserAreUsersFriends);
GUEST_FUNCTION_HOOK(__imp__XamUserCheckPrivilege, XamUserCheckPrivilege);
GUEST_FUNCTION_HOOK(__imp__XamUserCreateStatsEnumerator, XamUserCreateStatsEnumerator);
GUEST_FUNCTION_HOOK(__imp__XamUserGetName, XamUserGetName);
GUEST_FUNCTION_HOOK(__imp__XamUserGetXUID, XamUserGetXUID);
GUEST_FUNCTION_HOOK(__imp__XamUserWriteProfileSettings, XamUserWriteProfileSettings);
GUEST_FUNCTION_HOOK(__imp__XamVoiceCreate, XamVoiceCreate);
GUEST_FUNCTION_HOOK(__imp__XamVoiceClose, XamVoiceClose);
GUEST_FUNCTION_HOOK(__imp__XamVoiceHeadsetPresent, XamVoiceHeadsetPresent);
GUEST_FUNCTION_HOOK(__imp__XamVoiceSubmitPacket, XamVoiceSubmitPacket);
GUEST_FUNCTION_HOOK(__imp__XeKeysConsolePrivateKeySign, XeKeysConsolePrivateKeySign);
GUEST_FUNCTION_HOOK(__imp__IoDismountVolumeByFileHandle, IoDismountVolumeByFileHandle);
GUEST_FUNCTION_HOOK(__imp__NetDll_XNetGetEthernetLinkStatus, Net::XNetGetEthernetLinkStatus);
GUEST_FUNCTION_HOOK(__imp__KeTryToAcquireSpinLockAtRaisedIrql, KeTryToAcquireSpinLockAtRaisedIrql);
GUEST_FUNCTION_HOOK(__imp__XamShowGamerCardUIForXUID, XamShowGamerCardUIForXUID);
GUEST_FUNCTION_HOOK(__imp__XamShowPlayerReviewUI, XamShowPlayerReviewUI);
GUEST_FUNCTION_HOOK(__imp__XamShowDirtyDiscErrorUI, XamShowDirtyDiscErrorUI);
GUEST_FUNCTION_HOOK(__imp__XamEnableInactivityProcessing, XamEnableInactivityProcessing);
GUEST_FUNCTION_HOOK(__imp__XamResetInactivity, XamResetInactivity);
GUEST_FUNCTION_HOOK(__imp__XamShowMessageBoxUIEx, XamShowMessageBoxUIEx);
GUEST_FUNCTION_HOOK(__imp__XGetLanguage, XGetLanguage);
GUEST_FUNCTION_HOOK(__imp__XGetAVPack, XGetAVPack);
GUEST_FUNCTION_HOOK(__imp__XamLoaderTerminateTitle, XamLoaderTerminateTitle);
GUEST_FUNCTION_HOOK(__imp__XamGetExecutionId, XamGetExecutionId);
GUEST_FUNCTION_HOOK(__imp__XamLoaderLaunchTitle, XamLoaderLaunchTitle);
GUEST_FUNCTION_HOOK(__imp__NtOpenFile, NtOpenFile);
GUEST_FUNCTION_HOOK(__imp__RtlInitAnsiString, RtlInitAnsiString);
GUEST_FUNCTION_HOOK(__imp__NtCreateFile, NtCreateFile);
GUEST_FUNCTION_HOOK(__imp__NtClose, NtClose);
GUEST_FUNCTION_HOOK(__imp__NtSetInformationFile, NtSetInformationFile);
GUEST_FUNCTION_HOOK(__imp__FscGetCacheElementCount, FscGetCacheElementCount);
GUEST_FUNCTION_HOOK(__imp__FscSetCacheElementCount, FscSetCacheElementCount);
GUEST_FUNCTION_HOOK(__imp__XamLoaderGetLaunchDataSize, XamLoaderGetLaunchDataSize);
GUEST_FUNCTION_HOOK(__imp__XamLoaderGetLaunchData, XamLoaderGetLaunchData);
GUEST_FUNCTION_HOOK(__imp__XamLoaderSetLaunchData, XamLoaderSetLaunchData);
GUEST_FUNCTION_HOOK(__imp__NtWaitForSingleObjectEx, NtWaitForSingleObjectEx);
GUEST_FUNCTION_HOOK(__imp__NtWriteFile, NtWriteFile);
GUEST_FUNCTION_HOOK(__imp__ExGetXConfigSetting, ExGetXConfigSetting);
GUEST_FUNCTION_HOOK(__imp__NtQueryVirtualMemory, NtQueryVirtualMemory);
GUEST_FUNCTION_HOOK(__imp__MmQueryStatistics, MmQueryStatistics);
GUEST_FUNCTION_HOOK(__imp__NtCreateEvent, NtCreateEvent);
GUEST_FUNCTION_HOOK(__imp__XexCheckExecutablePrivilege, XexCheckExecutablePrivilege);
GUEST_FUNCTION_HOOK(__imp__DbgPrint, DbgPrint);
GUEST_FUNCTION_HOOK(__imp____C_specific_handler, __C_specific_handler_x);
GUEST_FUNCTION_HOOK(__imp__RtlNtStatusToDosError, RtlNtStatusToDosError);
GUEST_FUNCTION_HOOK(__imp__XexGetProcedureAddress, XexGetProcedureAddress);
GUEST_FUNCTION_HOOK(__imp__XexGetModuleSection, XexGetModuleSection);
GUEST_FUNCTION_HOOK(__imp__RtlUnicodeToMultiByteN, RtlUnicodeToMultiByteN);
GUEST_FUNCTION_HOOK(__imp__KeDelayExecutionThread, KeDelayExecutionThread);
GUEST_FUNCTION_HOOK(__imp__ExFreePool, ExFreePool);
GUEST_FUNCTION_HOOK(__imp__ExAllocatePool, ExAllocatePool);
GUEST_FUNCTION_HOOK(__imp__NtQueryInformationFile, NtQueryInformationFile);
GUEST_FUNCTION_HOOK(__imp__NtQueryVolumeInformationFile, NtQueryVolumeInformationFile);
GUEST_FUNCTION_HOOK(__imp__NtQueryDirectoryFile, NtQueryDirectoryFile);
GUEST_FUNCTION_HOOK(__imp__NtReadFileScatter, NtReadFileScatter);
GUEST_FUNCTION_HOOK(__imp__NtReadFile, NtReadFile);
GUEST_FUNCTION_HOOK(__imp__NtDuplicateObject, NtDuplicateObject);
GUEST_FUNCTION_HOOK(__imp__NtAllocateVirtualMemory, NtAllocateVirtualMemory);
GUEST_FUNCTION_HOOK(__imp__NtFreeVirtualMemory, NtFreeVirtualMemory);
GUEST_FUNCTION_HOOK(__imp__ObDereferenceObject, ObDereferenceObject);
GUEST_FUNCTION_HOOK(__imp__KeSetBasePriorityThread, KeSetBasePriorityThread);
GUEST_FUNCTION_HOOK(__imp__ObReferenceObjectByHandle, ObReferenceObjectByHandle);
GUEST_FUNCTION_HOOK(__imp__KeQueryBasePriorityThread, KeQueryBasePriorityThread);
GUEST_FUNCTION_HOOK(__imp__NtSuspendThread, NtSuspendThread);
GUEST_FUNCTION_HOOK(__imp__KeSetAffinityThread, KeSetAffinityThread);
GUEST_FUNCTION_HOOK(__imp__RtlLeaveCriticalSection, RtlLeaveCriticalSection);
GUEST_FUNCTION_HOOK(__imp__RtlEnterCriticalSection, RtlEnterCriticalSection);
GUEST_FUNCTION_HOOK(__imp__RtlImageXexHeaderField, RtlImageXexHeaderField);
GUEST_FUNCTION_HOOK(__imp__HalReturnToFirmware, HalReturnToFirmware);
GUEST_FUNCTION_HOOK(__imp__RtlFillMemoryUlong, RtlFillMemoryUlong);
GUEST_FUNCTION_HOOK(__imp__KeBugCheckEx, KeBugCheckEx);
GUEST_FUNCTION_HOOK(__imp__KeGetCurrentProcessType, KeGetCurrentProcessType);
GUEST_FUNCTION_HOOK(__imp__RtlCompareMemoryUlong, RtlCompareMemoryUlong);
GUEST_FUNCTION_HOOK(__imp__RtlInitializeCriticalSection, RtlInitializeCriticalSection);
GUEST_FUNCTION_HOOK(__imp__RtlRaiseException, RtlRaiseException_x);
GUEST_FUNCTION_HOOK(__imp__KfReleaseSpinLock, KfReleaseSpinLock);
GUEST_FUNCTION_HOOK(__imp__KfAcquireSpinLock, KfAcquireSpinLock);
GUEST_FUNCTION_HOOK(__imp__KeQueryPerformanceFrequency, KeQueryPerformanceFrequency);
GUEST_FUNCTION_HOOK(__imp__MmFreePhysicalMemory, MmFreePhysicalMemory);
GUEST_FUNCTION_HOOK(__imp__VdPersistDisplay, VdPersistDisplay);
GUEST_FUNCTION_HOOK(__imp__VdSwap, VdSwap);
GUEST_FUNCTION_HOOK(__imp__VdGetSystemCommandBuffer, VdGetSystemCommandBuffer);
GUEST_FUNCTION_HOOK(__imp__KeReleaseSpinLockFromRaisedIrql, KeReleaseSpinLockFromRaisedIrql);
GUEST_FUNCTION_HOOK(__imp__KeAcquireSpinLockAtRaisedIrql, KeAcquireSpinLockAtRaisedIrql);
GUEST_FUNCTION_HOOK(__imp__KiApcNormalRoutineNop, KiApcNormalRoutineNop);
GUEST_FUNCTION_HOOK(__imp__VdEnableRingBufferRPtrWriteBack, VdEnableRingBufferRPtrWriteBack);
GUEST_FUNCTION_HOOK(__imp__VdInitializeRingBuffer, VdInitializeRingBuffer);
GUEST_FUNCTION_HOOK(__imp__MmGetPhysicalAddress, MmGetPhysicalAddress);
GUEST_FUNCTION_HOOK(__imp__VdSetSystemCommandBufferGpuIdentifierAddress, VdSetSystemCommandBufferGpuIdentifierAddress);
GUEST_FUNCTION_HOOK(__imp__ExRegisterTitleTerminateNotification, ExRegisterTitleTerminateNotification);
GUEST_FUNCTION_HOOK(__imp__VdShutdownEngines, VdShutdownEngines);
GUEST_FUNCTION_HOOK(__imp__VdQueryVideoMode, VdQueryVideoMode);
GUEST_FUNCTION_HOOK(__imp__VdGetCurrentDisplayInformation, VdGetCurrentDisplayInformation);
GUEST_FUNCTION_HOOK(__imp__VdSetDisplayMode, VdSetDisplayMode);
GUEST_FUNCTION_HOOK(__imp__VdSetGraphicsInterruptCallback, VdSetGraphicsInterruptCallback);
GUEST_FUNCTION_HOOK(__imp__VdInitializeEngines, VdInitializeEngines);
GUEST_FUNCTION_HOOK(__imp__VdIsHSIOTrainingSucceeded, VdIsHSIOTrainingSucceeded);
GUEST_FUNCTION_HOOK(__imp__VdGetCurrentDisplayGamma, VdGetCurrentDisplayGamma);
GUEST_FUNCTION_HOOK(__imp__VdQueryVideoFlags, VdQueryVideoFlags);
GUEST_FUNCTION_HOOK(__imp__VdCallGraphicsNotificationRoutines, VdCallGraphicsNotificationRoutines);
GUEST_FUNCTION_HOOK(__imp__VdInitializeScalerCommandBuffer, VdInitializeScalerCommandBuffer);
GUEST_FUNCTION_HOOK(__imp__KeLeaveCriticalRegion, KeLeaveCriticalRegion);
GUEST_FUNCTION_HOOK(__imp__VdRetrainEDRAM, VdRetrainEDRAM);
GUEST_FUNCTION_HOOK(__imp__VdRetrainEDRAMWorker, VdRetrainEDRAMWorker);
GUEST_FUNCTION_HOOK(__imp__KeEnterCriticalRegion, KeEnterCriticalRegion);
GUEST_FUNCTION_HOOK(__imp__MmAllocatePhysicalMemoryEx, MmAllocatePhysicalMemoryEx);
GUEST_FUNCTION_HOOK(__imp__ObDeleteSymbolicLink, ObDeleteSymbolicLink);
GUEST_FUNCTION_HOOK(__imp__ObCreateSymbolicLink, ObCreateSymbolicLink);
GUEST_FUNCTION_HOOK(__imp__MmQueryAddressProtect, MmQueryAddressProtect);
GUEST_FUNCTION_HOOK(__imp__VdEnableDisableClockGating, VdEnableDisableClockGating);
GUEST_FUNCTION_HOOK(__imp__KeBugCheck, KeBugCheck);
GUEST_FUNCTION_HOOK(__imp__KeLockL2, KeLockL2);
GUEST_FUNCTION_HOOK(__imp__KeUnlockL2, KeUnlockL2);
GUEST_FUNCTION_HOOK(__imp__KeSetEvent, KeSetEvent);
GUEST_FUNCTION_HOOK(__imp__KeResetEvent, KeResetEvent);
GUEST_FUNCTION_HOOK(__imp__KeWaitForSingleObject, KeWaitForSingleObject);
GUEST_FUNCTION_HOOK(__imp__KeTlsGetValue, KeTlsGetValue);
GUEST_FUNCTION_HOOK(__imp__KeTlsSetValue, KeTlsSetValue);
GUEST_FUNCTION_HOOK(__imp__KeTlsAlloc, KeTlsAlloc);
GUEST_FUNCTION_HOOK(__imp__KeTlsFree, KeTlsFree);
GUEST_FUNCTION_HOOK(__imp__XMsgInProcessCall, XMsgInProcessCall);
GUEST_FUNCTION_HOOK(__imp__XamUserReadProfileSettings, XamUserReadProfileSettings);
GUEST_FUNCTION_HOOK(__imp__NetDll_WSAStartup, Net::WSAStartup);
GUEST_FUNCTION_HOOK(__imp__NetDll_WSACleanup, Net::WSACleanup);
GUEST_FUNCTION_HOOK(__imp__NetDll_socket, Net::Socket);
GUEST_FUNCTION_HOOK(__imp__NetDll_closesocket, Net::CloseSocket);
GUEST_FUNCTION_HOOK(__imp__NetDll_setsockopt, Net::SetSockOpt);
GUEST_FUNCTION_HOOK(__imp__NetDll_bind, Net::Bind);
GUEST_FUNCTION_HOOK(__imp__NetDll_connect, Net::Connect);
GUEST_FUNCTION_HOOK(__imp__NetDll_listen, Net::Listen);
GUEST_FUNCTION_HOOK(__imp__NetDll_accept, Net::Accept);
GUEST_FUNCTION_HOOK(__imp__NetDll_select, Net::Select);
GUEST_FUNCTION_HOOK(__imp__NetDll_recv, Net::Recv);
GUEST_FUNCTION_HOOK(__imp__NetDll_send, Net::Send);
GUEST_FUNCTION_HOOK(__imp__NetDll_inet_addr, Net::InetAddr);
GUEST_FUNCTION_HOOK(__imp__NetDll___WSAFDIsSet, NetDll___WSAFDIsSet);
GUEST_FUNCTION_HOOK(__imp__XMsgStartIORequestEx, XMsgStartIORequestEx);
GUEST_FUNCTION_HOOK(__imp__XamInputGetCapabilities, XamInputGetCapabilities);
GUEST_FUNCTION_HOOK(__imp__XamInputGetState, XamInputGetState);
GUEST_FUNCTION_HOOK(__imp__XamInputSetState, XamInputSetState);
GUEST_FUNCTION_HOOK(__imp__XexGetModuleHandle, XexGetModuleHandle);
GUEST_FUNCTION_HOOK(__imp__RtlTryEnterCriticalSection, RtlTryEnterCriticalSection);
GUEST_FUNCTION_HOOK(__imp__RtlInitializeCriticalSectionAndSpinCount, RtlInitializeCriticalSectionAndSpinCount);
GUEST_FUNCTION_HOOK(__imp__XeCryptBnQwBeSigVerify, XeCryptBnQwBeSigVerify);
GUEST_FUNCTION_HOOK(__imp__XeKeysGetKey, XeKeysGetKey);
GUEST_FUNCTION_HOOK(__imp__XeCryptRotSumSha, XeCryptRotSumSha);
GUEST_FUNCTION_HOOK(__imp__XeCryptSha, XeCryptSha);
GUEST_FUNCTION_HOOK(__imp__KeEnableFpuExceptions, KeEnableFpuExceptions);
GUEST_FUNCTION_HOOK(__imp__RtlUnwind, RtlUnwind_x);
GUEST_FUNCTION_HOOK(__imp__RtlCaptureContext, RtlCaptureContext_x);
GUEST_FUNCTION_HOOK(__imp__NtQueryFullAttributesFile, NtQueryFullAttributesFile);
GUEST_FUNCTION_HOOK(__imp__RtlMultiByteToUnicodeN, RtlMultiByteToUnicodeN);
GUEST_FUNCTION_HOOK(__imp__DbgBreakPoint, DbgBreakPoint);
GUEST_FUNCTION_HOOK(__imp__MmQueryAllocationSize, MmQueryAllocationSize);
GUEST_FUNCTION_HOOK(__imp__NtClearEvent, NtClearEvent);
GUEST_FUNCTION_HOOK(__imp__NtResumeThread, NtResumeThread);
GUEST_FUNCTION_HOOK(__imp__NtSetEvent, NtSetEvent);
GUEST_FUNCTION_HOOK(__imp__NtCreateSemaphore, NtCreateSemaphore);
GUEST_FUNCTION_HOOK(__imp__NtReleaseSemaphore, NtReleaseSemaphore);
GUEST_FUNCTION_HOOK(__imp__NtWaitForMultipleObjectsEx, NtWaitForMultipleObjectsEx);
GUEST_FUNCTION_HOOK(__imp__RtlCompareStringN, RtlCompareStringN);
GUEST_FUNCTION_HOOK(__imp__StfsControlDevice, StfsControlDevice);
GUEST_FUNCTION_HOOK(__imp__StfsCreateDevice, StfsCreateDevice);
GUEST_FUNCTION_HOOK(__imp__NtFlushBuffersFile, NtFlushBuffersFile);
GUEST_FUNCTION_HOOK(__imp__KeQuerySystemTime, KeQuerySystemTime);
GUEST_FUNCTION_HOOK(__imp__RtlTimeToTimeFields, RtlTimeToTimeFields);
GUEST_FUNCTION_HOOK(__imp__RtlFreeAnsiString, RtlFreeAnsiString);
GUEST_FUNCTION_HOOK(__imp__RtlUnicodeStringToAnsiString, RtlUnicodeStringToAnsiString);
GUEST_FUNCTION_HOOK(__imp__RtlInitUnicodeString, RtlInitUnicodeString);
GUEST_FUNCTION_HOOK(__imp__ExTerminateThread, ExTerminateThread);
GUEST_FUNCTION_HOOK(__imp__ExCreateThread, ExCreateThread);
GUEST_FUNCTION_HOOK(__imp__IoInvalidDeviceRequest, IoInvalidDeviceRequest);
GUEST_FUNCTION_HOOK(__imp__ObReferenceObject, ObReferenceObject);
GUEST_FUNCTION_HOOK(__imp__IoCreateDevice, IoCreateDevice);
GUEST_FUNCTION_HOOK(__imp__IoDeleteDevice, IoDeleteDevice);
GUEST_FUNCTION_HOOK(__imp__ExAllocatePoolTypeWithTag, ExAllocatePoolTypeWithTag);
GUEST_FUNCTION_HOOK(__imp__RtlTimeFieldsToTime, RtlTimeFieldsToTime);
GUEST_FUNCTION_HOOK(__imp__IoCompleteRequest, IoCompleteRequest);
GUEST_FUNCTION_HOOK(__imp__RtlUpcaseUnicodeChar, RtlUpcaseUnicodeChar);
GUEST_FUNCTION_HOOK(__imp__ObIsTitleObject, ObIsTitleObject);
GUEST_FUNCTION_HOOK(__imp__IoCheckShareAccess, IoCheckShareAccess);
GUEST_FUNCTION_HOOK(__imp__IoSetShareAccess, IoSetShareAccess);
GUEST_FUNCTION_HOOK(__imp__IoRemoveShareAccess, IoRemoveShareAccess);
GUEST_FUNCTION_HOOK(__imp__NetDll_XNetStartup, Net::XNetStartup);
GUEST_FUNCTION_HOOK(__imp__NetDll_XNetGetTitleXnAddr, Net::XNetGetTitleXnAddr);
GUEST_FUNCTION_HOOK(__imp__KeWaitForMultipleObjects, KeWaitForMultipleObjects);
GUEST_FUNCTION_HOOK(__imp__KeRaiseIrqlToDpcLevel, KeRaiseIrqlToDpcLevel);
GUEST_FUNCTION_HOOK(__imp__KfLowerIrql, KfLowerIrql);
GUEST_FUNCTION_HOOK(__imp__KeReleaseSemaphore, KeReleaseSemaphore);
GUEST_FUNCTION_HOOK(__imp__XAudioGetVoiceCategoryVolume, XAudioGetVoiceCategoryVolume);
GUEST_FUNCTION_HOOK(__imp__XAudioGetVoiceCategoryVolumeChangeMask, XAudioGetVoiceCategoryVolumeChangeMask);
GUEST_FUNCTION_HOOK(__imp__KeResumeThread, KeResumeThread);
GUEST_FUNCTION_HOOK(__imp__KeInitializeSemaphore, KeInitializeSemaphore);
GUEST_FUNCTION_HOOK(__imp__XMAReleaseContext, XMAReleaseContext);
GUEST_FUNCTION_HOOK(__imp__XMACreateContext, XMACreateContext);
GUEST_FUNCTION_HOOK(__imp__XAudioRegisterRenderDriverClient, XAudioRegisterRenderDriverClient);
GUEST_FUNCTION_HOOK(__imp__XAudioUnregisterRenderDriverClient, XAudioUnregisterRenderDriverClient);
GUEST_FUNCTION_HOOK(__imp__XAudioSubmitRenderDriverFrame, XAudioSubmitRenderDriverFrame);
// Additional GTA V stubs
GUEST_FUNCTION_HOOK(__imp__XamAlloc, XamAlloc);
GUEST_FUNCTION_HOOK(__imp__XamFree, XamFree);
GUEST_FUNCTION_HOOK(__imp__NtSetTimerEx, NtSetTimerEx);
GUEST_FUNCTION_HOOK(__imp__NtCancelTimer, NtCancelTimer);
GUEST_FUNCTION_HOOK(__imp__NtCreateTimer, NtCreateTimer);
GUEST_FUNCTION_HOOK(__imp__NtCreateMutant, NtCreateMutant);
GUEST_FUNCTION_HOOK(__imp__NtReleaseMutant, NtReleaseMutant);
GUEST_FUNCTION_HOOK(__imp__IoDismountVolume, IoDismountVolume);
GUEST_FUNCTION_HOOK(__imp__XNotifyPositionUI, XNotifyPositionUI);
GUEST_FUNCTION_HOOK(__imp__NetDll_XNetCleanup, Net::XNetCleanup);
GUEST_FUNCTION_HOOK(__imp__NetDll_getsockname, Net::GetSockName);
GUEST_FUNCTION_HOOK(__imp__NetDll_ioctlsocket, Net::IOCtlSocket);
GUEST_FUNCTION_HOOK(__imp__NetDll_sendto, Net::SendTo);
GUEST_FUNCTION_HOOK(__imp__NetDll_recvfrom, Net::RecvFrom);
GUEST_FUNCTION_HOOK(__imp__NetDll_shutdown, Net::Shutdown);
GUEST_FUNCTION_HOOK(__imp__XMsgCancelIORequest, XMsgCancelIORequest);
GUEST_FUNCTION_HOOK(__imp__XAudioGetSpeakerConfig, XAudioGetSpeakerConfig);
GUEST_FUNCTION_HOOK(__imp__XamContentSetThumbnail, XamContentSetThumbnail);
GUEST_FUNCTION_HOOK(__imp__XamInputGetKeystrokeEx, XamInputGetKeystrokeEx);
GUEST_FUNCTION_HOOK(__imp__XamSessionCreateHandle, Net::XamSessionCreateHandle);
GUEST_FUNCTION_HOOK(__imp__XamSessionRefObjByHandle, Net::XamSessionRefObjByHandle);
GUEST_FUNCTION_HOOK(__imp__KeSetDisableBoostThread, KeSetDisableBoostThread);
GUEST_FUNCTION_HOOK(__imp__XamCreateEnumeratorHandle, XamCreateEnumeratorHandle);
GUEST_FUNCTION_HOOK(__imp__NtDeviceIoControlFile, NtDeviceIoControlFile);
GUEST_FUNCTION_HOOK(__imp__NetDll_WSAGetLastError, Net::WSAGetLastError);
GUEST_FUNCTION_HOOK(__imp__NetDll_XNetQosListen, Net::XNetQosListen);
GUEST_FUNCTION_HOOK(__imp__NetDll_XNetQosLookup, Net::XNetQosLookup);
GUEST_FUNCTION_HOOK(__imp__NetDll_XNetQosRelease, Net::XNetQosRelease);
GUEST_FUNCTION_HOOK(__imp__NetDll_XNetServerToInAddr, Net::XNetServerToInAddr);
GUEST_FUNCTION_HOOK(__imp__NetDll_XNetXnAddrToInAddr, Net::XNetXnAddrToInAddr);
GUEST_FUNCTION_HOOK(__imp__NetDll_XNetGetConnectStatus, Net::XNetGetConnectStatus);
GUEST_FUNCTION_HOOK(__imp__NetDll_XNetUnregisterInAddr, Net::XNetUnregisterInAddr);

// =============================================================================
// GTA V File System API Hooks (UnleashedRecomp Pattern)
// =============================================================================
// Following UnleashedRecomp's elegant approach, we hook at the file API
// boundary rather than the low-level storage device vtable layer.
// This avoids validation token complexity and lets game logic run naturally.
// =============================================================================

GUEST_FUNCTION_HOOK(sub_8249BE88, GTA::FileResolve);


// =============================================================================
// =============================================================================


// PHASE 1: Enhanced instrumentation with global tracking flag


// =============================================================================
// PHASE 1: Deep Instrumentation - File Parsing Chain
// =============================================================================
// Global helper to read string from guest memory
static std::string ReadGuestString(uint8_t* base, uint32_t addr, int maxLen = 260) {
    if (addr < 0x80000000 || addr >= 0x90000000) return "<invalid addr>";
    std::string result;
    for (int i = 0; i < maxLen; i++) {
        uint8_t c = *(base + addr + i);
        if (c == 0) break;
        if (c >= 0x20 && c <= 0x7E) result += (char)c;
        else result += '.';
    }
    return result.empty() ? "<empty>" : result;
}

// PHASE 1: Deep instrumentation to find blocking point

// FIX: When file parsing finds no entries (context[0]=0), the original code crashes
// trying to access memory at loc_822F8950 after stream close. This safe implementation


// =============================================================================
// =============================================================================
// FIX: Global at 0x82A2DB54 (offset -9388 from 0x82A30000) is NEVER initialized
// r4 and passes it here. Inside this function, r4 is copied to r28, and later
// the code crashes trying to access r28+22000.
//
// Call site (ppc_recomp.11.cpp:111705-111711):
//   r3 = 256 (constant)
//   r4 = garbage from uninitialized global 0x82A2DB54
//
// This wrapper validates r4 before proceeding.

// =========================================================================
// Remaining 63-subsystem hooks (after Stats system)
// =========================================================================


// =========================================================================
// Main Loop Tracing Hooks
// =========================================================================

// =============================================================================
// RENDER OBJECT STUB - Option B: Manual initialization
// Creates a stub object with vtable[16] pointing to our render trigger function
// =============================================================================
static bool s_renderObjectInitialized = false;
static uint32_t s_stubObjectAddr = 0;
static uint32_t s_stubVtableAddr = 0;

// Forward declaration for render trigger function
void RenderTriggerFromVtable();

// PPC wrapper for the render trigger - called from vtable[16]
PPC_FUNC(RenderTriggerStub) {
    static int s_count = 0; ++s_count;
    if (s_count <= 10 || s_count % 100 == 0) {
        LOGF_WARNING("[RENDER_TRIGGER] vtable[16] called #{} - triggering host render", s_count);
    }
    
    // Trigger host rendering - call Video::Present or DrawPrimitive
    // For now, just log that we got here - the actual rendering happens via VdSwap
    // The important thing is that the render path is now unblocked
}


// Called after the loop

// Called conditionally before rendering


// =============================================================================
// This function was incorrectly hooked to CreateDevice in video.cpp.
// Original behavior: Writes GPU commands then spins on [r3+11000] waiting for completion.
// Fix: Skip the spin loop since host GPU handles sync via Video::Present.
// =============================================================================

// =========================================================================
// =========================================================================

// =========================================================================
// =========================================================================

// =========================================================================
// =========================================================================

// =========================================================================
// =========================================================================


// This function blocks waiting for workers to exit. Stub it to bypass the wait.

// This function loops waiting for all workers to exit.
// We must signal all semaphores before the loop to ensure workers wake up and exit.


// =============================================================================
// This is a "dirty disc" UI function - stub it to skip
// =============================================================================

// =============================================================================
// =============================================================================
// =============================================================================

// =============================================================================
// RAGE Small Block Allocator (SBA) / Slab Page Formatter Hook
// =============================================================================
// sub_82380A50 formats newly allocated 4096-byte pages into singly linked chunks
// for a small block allocator bucket. When chunk count <= 2, the underflowed
// 'addic. r9, r3, -2' / 'mtctr' loop loops 4.2 billion times.
// We provide a strong implementation that safely skips the loop if count <= 2.
// =============================================================================
PPC_EXTERN_FUNC(__savegprlr_26);
PPC_EXTERN_FUNC(__restgprlr_26);
PPC_EXTERN_FUNC(sub_82AA08A0);
PPC_EXTERN_FUNC(sub_823C0348);

// =============================================================================
// Hash Table / Category String Hasher
// =============================================================================
// sub_82D925B8 formats 8 category strings and hashes them into an array at r31.
// =============================================================================
PPC_FUNC(sub_82D925B8) {
	PPC_FUNC_PROLOGUE();
	PPCCRRegister cr0{};
	PPCCRRegister cr6{};
	uint32_t ea{};
	PPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	__savegprlr_26(ctx, base);
	ea = -176 + ctx.r1.u32;
	PPC_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	ctx.r11.s64 = -2088828928;
	ctx.r30.u64 = ctx.r4.u64;
	ctx.r10.s64 = ctx.r11.s64 + 3384;
	ctx.r11.s64 = -2113601536;
	ctx.r31.u64 = ctx.r3.u64;
	ctx.r29.s64 = 8;
	ctx.r27.s64 = ctx.r10.s64 - ctx.r3.s64;
	ctx.r26.s64 = 0;
	ctx.r28.s64 = ctx.r11.s64 + 20196;

	static uint32_t s_callCount = 0;
	bool shouldLog = App::s_verboseAlloc && (++s_callCount <= 3);
	if (shouldLog) {
		LOGF_INFO("[TRACE sub_82D925B8] Start 8-iteration category hash: r3=0x{:08X} r4=0x{:08X}",
		          ctx.r3.u32, ctx.r4.u32);
	}

	for (int iter = 0; iter < 8; ++iter) {
		cr6.compare<uint32_t>(ctx.r30.u32, 0, ctx.xer);
		PPC_STORE_U8(ctx.r1.u32 + 80, ctx.r26.u8);
		if (!cr6.eq) {
			ctx.r5.u64 = ctx.r28.u64;
			ctx.r6.u64 = PPC_LOAD_U32(ctx.r27.u32 + ctx.r31.u32);
			ctx.r7.u64 = ctx.r30.u64;
			ctx.r4.s64 = 32;
			ctx.r3.s64 = ctx.r1.s64 + 80;
			uint64_t saved_r27 = ctx.r27.u64;
			uint64_t saved_r28 = ctx.r28.u64;
			uint64_t saved_r30 = ctx.r30.u64;
			uint64_t saved_r31 = ctx.r31.u64;
			sub_82AA08A0(ctx, base);
			ctx.r4.s64 = 0;
			ctx.r3.s64 = ctx.r1.s64 + 80;
			sub_823C0348(ctx, base);
			ctx.r27.u64 = saved_r27;
			ctx.r28.u64 = saved_r28;
			ctx.r30.u64 = saved_r30;
			ctx.r31.u64 = saved_r31;
		} else {
			ctx.r3.u64 = ctx.r26.u64;
		}

		PPC_STORE_U32(ctx.r31.u32 + 0, ctx.r3.u32);
		if (shouldLog) {
			LOGF_INFO("[TRACE sub_82D925B8] iter {}/8: [0x{:08X}] = 0x{:08X}", 
			          iter + 1, ctx.r31.u32, ctx.r3.u32);
		}
		ctx.r31.s64 = ctx.r31.s64 + 4;
	}

	ctx.r29.s64 = 0;
	ctx.r1.s64 = ctx.r1.s64 + 176;
	__restgprlr_26(ctx, base);
	return;
}


PPC_EXTERN_FUNC(__savegprlr_24);
PPC_EXTERN_FUNC(__restgprlr_24);

PPC_FUNC(sub_82374FC0)
{
	PPC_FUNC_PROLOGUE();
	PPCCRRegister cr6{};
	PPCRegister temp{};
	int loopSteps = 0;
	__savegprlr_24(ctx, base);
	cr6.compare<uint32_t>(ctx.r5.u32, 16, ctx.xer);
	if (!cr6.lt) goto loc_82374FD4;
	ctx.r5.s64 = 16;
loc_82374FD4:
	cr6.compare<uint32_t>(ctx.r4.u32, 16, ctx.xer);
	if (!cr6.lt) goto loc_82374FE0;
	ctx.r4.s64 = 16;
loc_82374FE0:
	ctx.r11.s64 = ctx.r4.s64 + 15;
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r3.u32 + 216);
	ctx.r26.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 0) & 0xFFFFFFF0;
	cr6.compare<uint32_t>(ctx.r26.u32, ctx.r10.u32, ctx.xer);
	if (cr6.gt) goto loc_823750DC;
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r26.u32 | (ctx.r26.u64 << 32), 28) & 0xFFFFFFF;
	cr6.compare<uint32_t>(ctx.r11.u32, 15, ctx.xer);
	if (cr6.lt) goto loc_82375008;
	ctx.r27.s64 = 15;
	goto loc_82375014;
loc_82375008:
	ctx.r27.u64 = ctx.r11.u64;
	cr6.compare<int32_t>(ctx.r11.s32, 16, ctx.xer);
	if (!cr6.lt) goto loc_823750DC;
loc_82375014:
	ctx.r11.s64 = ctx.r27.s64 + 3;
	ctx.r25.s64 = 0;
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 2) & 0xFFFFFFFC;
	ctx.r28.s64 = -1;
	ctx.r30.u64 = ctx.r11.u64 + ctx.r3.u64;
loc_82375028:
	if (ctx.r30.u32 < 0x10000 || ctx.r30.u32 >= 0xA0000000) goto loc_823750DC;
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r30.u32 + 0);
	ctx.r11.u64 = ctx.r25.u64;
	ctx.r31.u64 = ctx.r28.u64;
	cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	if (cr6.eq || ctx.r10.u32 < 0x10000 || ctx.r10.u32 >= 0xA0000000) goto loc_823750CC;
	ctx.r29.s64 = ctx.r5.s64 + -1;
	ctx.r6.u64 = PPC_LOAD_U32(ctx.r10.u32 + 20);
	ctx.r7.s64 = ctx.r5.s64 + 15;
	ctx.r4.u64 = ~ctx.r29.u64;
	loopSteps = 0;
loc_8237504C:
	if (ctx.r10.u32 < 0x10000 || ctx.r10.u32 >= 0xA0000000) {
		ctx.r10.u64 = 0;
		goto loc_823750C4;
	}
	if (++loopSteps > 1000 || (ctx.r10.u32 == ctx.r6.u32)) {
		static int s_cycleWarn = 0;
		if (s_cycleWarn++ < 10) {
			LOGF_WARNING("[sub_82374FC0] Cycle/limit hit in allocator free list (r10=0x{:08X}, steps={})! Breaking loop.", ctx.r10.u32, loopSteps);
		}
		ctx.r10.u64 = 0;
		goto loc_823750C4;
	}
	ctx.r9.u64 = PPC_LOAD_U32(ctx.r10.u32 + 0);
	cr6.compare<uint32_t>(ctx.r9.u32, ctx.r10.u32, ctx.xer);
	if (cr6.eq) goto loc_8237505C;
loc_8237505C:
	ctx.r9.u64 = PPC_LOAD_U32(ctx.r10.u32 + 4);
	ctx.r24.u64 = ctx.r7.u64 + ctx.r10.u64;
	ctx.r8.u64 = ctx.r9.u64 + ctx.r10.u64;
	ctx.r9.u64 = ctx.r24.u64 & ctx.r4.u64;
	ctx.r8.s64 = ctx.r8.s64 + 16;
	cr6.compare<uint32_t>(ctx.r8.u32, ctx.r9.u32, ctx.xer);
	if (!cr6.gt) goto loc_82375098;
	ctx.r9.s64 = ctx.r8.s64 - ctx.r9.s64;
	cr6.compare<uint32_t>(ctx.r9.u32, ctx.r26.u32, ctx.xer);
	if (cr6.eq) goto loc_823750C0;
	if (!cr6.gt) goto loc_82375098;
	cr6.compare<uint32_t>(ctx.r9.u32, ctx.r31.u32, ctx.xer);
	if (!cr6.lt) goto loc_82375098;
	ctx.r11.u64 = ctx.r10.u64;
	ctx.r31.u64 = ctx.r9.u64;
loc_82375098:
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r10.u32 + 20);
	cr6.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	if (cr6.eq || ctx.r6.u32 < 0x10000 || ctx.r6.u32 >= 0xA0000000) goto loc_823750B4;
	ctx.r6.u64 = PPC_LOAD_U32(ctx.r6.u32 + 20);
	cr6.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	if (cr6.eq || ctx.r6.u32 < 0x10000 || ctx.r6.u32 >= 0xA0000000) goto loc_823750B4;
	ctx.r6.u64 = PPC_LOAD_U32(ctx.r6.u32 + 20);
loc_823750B4:
	cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	if (!cr6.eq && ctx.r10.u32 >= 0x10000 && ctx.r10.u32 < 0xA0000000) goto loc_8237504C;
	goto loc_823750C4;
	goto loc_823750C4;
loc_823750C0:
	ctx.r11.u64 = ctx.r10.u64;
loc_823750C4:
	cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	if (!cr6.eq) goto loc_823750E4;
loc_823750CC:
	ctx.r27.s64 = ctx.r27.s64 + 1;
	ctx.r30.s64 = ctx.r30.s64 + 4;
	cr6.compare<int32_t>(ctx.r27.s32, 16, ctx.xer);
	if (cr6.lt) goto loc_82375028;
loc_823750DC:
	ctx.r3.s64 = 0;
	__restgprlr_24(ctx, base);
	return;
loc_823750E4:
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 12);
	ctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);
	ctx.r8.u64 = ctx.r10.u64 | 16;
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r9.u32 | (ctx.r9.u64 << 32), 28) & 0xFFFFFFF;
	ctx.r7.u64 = ctx.r8.u64;
	PPC_STORE_U32(ctx.r11.u32 + 12, ctx.r8.u32);
	ctx.r6.u64 = PPC_LOAD_U32(ctx.r3.u32 + 268);
	cr6.compare<uint32_t>(ctx.r10.u32, 15, ctx.xer);
	ctx.r7.u64 = (__builtin_rotateleft32(ctx.r6.u32, 6) & 0xFFFFFFC0) | (ctx.r7.u64 & 0xFFFFFFFF0000003F);
	PPC_STORE_U32(ctx.r11.u32 + 12, ctx.r7.u32);
	ctx.r9.s64 = 15;
	if (!cr6.lt) goto loc_82375118;
	ctx.r9.u64 = ctx.r10.u64;
loc_82375118:
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 16);
	ctx.r7.s64 = ctx.r11.s64 + 16;
	cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	if (cr6.eq) goto loc_82375134;
	ctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + 20);
	PPC_STORE_U32(ctx.r10.u32 + 20, ctx.r9.u32);
	goto loc_82375144;
loc_82375134:
	ctx.r10.s64 = ctx.r9.s64 + 3;
	ctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + 20);
	ctx.r8.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 2) & 0xFFFFFFFC;
	PPC_STORE_U32(ctx.r8.u32 + ctx.r3.u32, ctx.r9.u32);
loc_82375144:
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 20);
	cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	if (cr6.eq) goto loc_82375158;
	ctx.r9.u64 = PPC_LOAD_U32(ctx.r7.u32 + 0);
	PPC_STORE_U32(ctx.r10.u32 + 16, ctx.r9.u32);
loc_82375158:
	ctx.r10.u64 = ctx.r7.u64 & ctx.r29.u64;
	cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	if (cr6.eq) goto loc_82375290;
	ctx.r9.u64 = ctx.r7.u64 + ctx.r5.u64;
	ctx.r8.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 8);
	ctx.r6.s64 = ctx.r9.s64 + -1;
	ctx.r9.u64 = ctx.r8.u64 + ctx.r11.u64;
	ctx.r8.u64 = ctx.r6.u64 & ctx.r4.u64;
	ctx.r5.s64 = ctx.r9.s64 + 16;
	ctx.r9.s64 = ctx.r8.s64 - ctx.r7.s64;
	cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	if (cr6.eq) goto loc_823751C0;
	cr6.compare<uint32_t>(ctx.r9.u32, 16, ctx.xer);
	if (!cr6.eq) goto loc_823751C0;
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r10.u32 + 4);
	ctx.r9.u64 = PPC_LOAD_U32(ctx.r10.u32 + 12);
	ctx.r7.s64 = ctx.r11.s64 + 16;
	ctx.r11.u64 = ctx.r9.u32 & 0xF;
	PPC_STORE_U32(ctx.r10.u32 + 4, ctx.r7.u32);
	ctx.r6.s64 = ctx.r11.s64 + 22;
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 2) & 0xFFFFFFFC;
	ctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + ctx.r3.u32);
	ctx.r4.s64 = ctx.r9.s64 + 16;
	PPC_STORE_U32(ctx.r11.u32 + ctx.r3.u32, ctx.r4.u32);
	goto loc_82375218;
loc_823751C0:
	ctx.r9.s64 = ctx.r9.s64 + -16;
	ctx.r10.u64 = ctx.r11.u64;
	PPC_STORE_U32(ctx.r11.u32 + 4, ctx.r9.u32);
	ctx.r6.u64 = __builtin_rotateleft32(ctx.r9.u32, 0);
	ctx.r4.u64 = PPC_LOAD_U32(ctx.r11.u32 + 12);
	ctx.r4.u64 = ctx.r4.u32 & 0x3F;
	ctx.r4.u64 = __builtin_rotateleft64(ctx.r4.u32 | (ctx.r4.u64 << 32), 0) & 0xFFFFFFFFFFFFFFEF;
	ctx.r9.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 28) & 0xFFFFFFF;
	PPC_STORE_U32(ctx.r11.u32 + 12, ctx.r4.u32);
	cr6.compare<uint32_t>(ctx.r9.u32, 15, ctx.xer);
	if (cr6.lt) goto loc_823751F0;
	ctx.r9.s64 = 15;
loc_823751F0:
	ctx.r9.s64 = ctx.r9.s64 + 3;
	ctx.r9.u64 = __builtin_rotateleft64(ctx.r9.u32 | (ctx.r9.u64 << 32), 2) & 0xFFFFFFFC;
	ctx.r6.u64 = PPC_LOAD_U32(ctx.r9.u32 + ctx.r3.u32);
	PPC_STORE_U32(ctx.r11.u32 + 20, ctx.r6.u32);
	ctx.r6.u64 = PPC_LOAD_U32(ctx.r9.u32 + ctx.r3.u32);
	cr6.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	if (cr6.eq) goto loc_82375210;
	PPC_STORE_U32(ctx.r6.u32 + 16, ctx.r11.u32);
loc_82375210:
	PPC_STORE_U32(ctx.r7.u32 + 0, ctx.r25.u32);
	PPC_STORE_U32(ctx.r9.u32 + ctx.r3.u32, ctx.r11.u32);
loc_82375218:
	ctx.r11.s64 = ctx.r8.s64 + -16;
	ctx.r9.s64 = ctx.r5.s64 - ctx.r8.s64;
	PPC_STORE_U32(ctx.r8.u32 + -16, ctx.r11.u32);
	PPC_STORE_U32(ctx.r8.u32 + -12, ctx.r9.u32);
	PPC_STORE_U32(ctx.r8.u32 + -8, ctx.r10.u32);
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r8.u32 + -12);
	ctx.r5.u64 = PPC_LOAD_U32(ctx.r8.u32 + -4);
	ctx.r4.u64 = ctx.r5.u32 & 0xF;
	ctx.r7.u64 = PPC_LOAD_U32(ctx.r3.u32 + 268);
	ctx.r6.u64 = __builtin_rotateleft64(ctx.r7.u32 | (ctx.r7.u64 << 32), 6) & 0xFFFFFFC0;
	ctx.r9.u64 = ctx.r4.u64 | ctx.r6.u64;
	ctx.r10.u64 = ctx.r10.u64 + ctx.r11.u64;
	ctx.r7.u64 = ctx.r9.u64 | 16;
	ctx.r10.s64 = ctx.r10.s64 + 16;
	PPC_STORE_U32(ctx.r8.u32 + -4, ctx.r7.u32);
	ctx.r9.u64 = PPC_LOAD_U32(ctx.r3.u32 + 76);
	ctx.r8.u64 = PPC_LOAD_U32(ctx.r3.u32 + 4);
	ctx.r6.u64 = ctx.r9.u64 + ctx.r8.u64;
	ctx.xer.ca = ctx.r10.u32 >= ctx.r6.u32;
	ctx.r5.s64 = ctx.r10.s64 - ctx.r6.s64;
	temp.u8 = (~ctx.r4.u32 + ctx.r4.u32 < ~ctx.r4.u32) | (~ctx.r4.u32 + ctx.r4.u32 + ctx.xer.ca < ctx.xer.ca);
	ctx.r9.u64 = ~ctx.r4.u64 + ctx.r4.u64 + ctx.xer.ca;
	ctx.xer.ca = temp.u8;
	ctx.r10.u64 = ctx.r9.u64 & ctx.r10.u64;
	cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	if (cr6.eq) goto loc_82375278;
	PPC_STORE_U32(ctx.r10.u32 + 8, ctx.r11.u32);
loc_82375278:
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r3.u32 + 216);
	ctx.r9.s64 = ctx.r10.s64 + -16;
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r3.u32 + 84);
	ctx.r8.s64 = ctx.r10.s64 + 16;
	PPC_STORE_U32(ctx.r3.u32 + 216, ctx.r9.u32);
	PPC_STORE_U32(ctx.r3.u32 + 84, ctx.r8.u32);
loc_82375290:
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);
	ctx.r9.s64 = ctx.r26.s64 + 16;
	cr6.compare<uint32_t>(ctx.r10.u32, ctx.r9.u32, ctx.xer);
	if (!cr6.gt) goto loc_8237534C;
	ctx.r10.u64 = ctx.r11.u64 + ctx.r26.u64;
	ctx.r10.s64 = ctx.r10.s64 + 16;
	PPC_STORE_U32(ctx.r10.u32 + 0, ctx.r10.u32);
	ctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);
	ctx.r9.s64 = ctx.r9.s64 - ctx.r26.s64;
	ctx.r8.s64 = ctx.r9.s64 + -16;
	PPC_STORE_U32(ctx.r10.u32 + 4, ctx.r8.u32);
	PPC_STORE_U32(ctx.r10.u32 + 8, ctx.r11.u32);
	PPC_STORE_U32(ctx.r10.u32 + 12, ctx.r25.u32);
	ctx.r7.u64 = PPC_LOAD_U32(ctx.r3.u32 + 4);
	ctx.r9.u64 = PPC_LOAD_U32(ctx.r10.u32 + 4);
	ctx.r9.u64 = ctx.r10.u64 + ctx.r9.u64;
	ctx.r8.u64 = PPC_LOAD_U32(ctx.r3.u32 + 76);
	ctx.r9.s64 = ctx.r9.s64 + 16;
	ctx.r7.u64 = ctx.r8.u64 + ctx.r7.u64;
	ctx.xer.ca = ctx.r9.u32 >= ctx.r7.u32;
	ctx.r6.s64 = ctx.r9.s64 - ctx.r7.s64;
	temp.u8 = (~ctx.r5.u32 + ctx.r5.u32 < ~ctx.r5.u32) | (~ctx.r5.u32 + ctx.r5.u32 + ctx.xer.ca < ctx.xer.ca);
	ctx.r4.u64 = ~ctx.r5.u64 + ctx.r5.u64 + ctx.xer.ca;
	ctx.xer.ca = temp.u8;
	ctx.r9.u64 = ctx.r4.u64 & ctx.r9.u64;
	cr6.compare<uint32_t>(ctx.r9.u32, 0, ctx.xer);
	if (cr6.eq) goto loc_823752F4;
	PPC_STORE_U32(ctx.r9.u32 + 8, ctx.r10.u32);
loc_823752F4:
	PPC_STORE_U32(ctx.r11.u32 + 4, ctx.r26.u32);
	ctx.r9.u64 = PPC_LOAD_U32(ctx.r10.u32 + 4);
	ctx.r9.u64 = __builtin_rotateleft64(ctx.r9.u32 | (ctx.r9.u64 << 32), 28) & 0xFFFFFFF;
	cr6.compare<uint32_t>(ctx.r9.u32, 15, ctx.xer);
	if (cr6.lt) goto loc_8237530C;
	ctx.r9.s64 = 15;
loc_8237530C:
	ctx.r9.s64 = ctx.r9.s64 + 3;
	ctx.r9.u64 = __builtin_rotateleft64(ctx.r9.u32 | (ctx.r9.u64 << 32), 2) & 0xFFFFFFFC;
	ctx.r8.u64 = PPC_LOAD_U32(ctx.r9.u32 + ctx.r3.u32);
	PPC_STORE_U32(ctx.r10.u32 + 20, ctx.r8.u32);
	ctx.r8.u64 = PPC_LOAD_U32(ctx.r9.u32 + ctx.r3.u32);
	cr6.compare<uint32_t>(ctx.r8.u32, 0, ctx.xer);
	if (cr6.eq) goto loc_8237532C;
	PPC_STORE_U32(ctx.r8.u32 + 16, ctx.r10.u32);
loc_8237532C:
	PPC_STORE_U32(ctx.r10.u32 + 16, ctx.r25.u32);
	PPC_STORE_U32(ctx.r9.u32 + ctx.r3.u32, ctx.r10.u32);
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r3.u32 + 216);
	ctx.r10.s64 = ctx.r10.s64 + -16;
	ctx.r9.u64 = PPC_LOAD_U32(ctx.r3.u32 + 84);
	PPC_STORE_U32(ctx.r3.u32 + 216, ctx.r10.u32);
	ctx.r9.s64 = ctx.r9.s64 + 16;
	PPC_STORE_U32(ctx.r3.u32 + 84, ctx.r9.u32);
loc_8237534C:
	ctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);
	ctx.r8.s64 = 176;
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r3.u32 + 84);
	ctx.r7.u64 = PPC_LOAD_U32(ctx.r3.u32 + 216);
	ctx.r6.u64 = ctx.r10.u64 + ctx.r9.u64;
	ctx.r5.u64 = PPC_LOAD_U32(ctx.r13.u32 + 0);
	PPC_STORE_U32(ctx.r3.u32 + 84, ctx.r6.u32);
	ctx.r4.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);
	ctx.r10.s64 = ctx.r7.s64 - ctx.r4.s64;
	PPC_STORE_U32(ctx.r3.u32 + 216, ctx.r10.u32);
	ctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);
	ctx.r8.u64 = PPC_LOAD_U32(ctx.r8.u32 + ctx.r5.u32);
	ctx.r7.u64 = PPC_LOAD_U32(ctx.r11.u32 + 12);
	ctx.r8.u64 = (__builtin_rotateleft32(ctx.r7.u32, 0) & 0xFFFFFFF0) | (ctx.r8.u64 & 0xFFFFFFFF0000000F);
	ctx.r10.u64 = ctx.r8.u32 & 0xF;
	PPC_STORE_U32(ctx.r11.u32 + 12, ctx.r8.u32);
	ctx.r6.s64 = ctx.r10.s64 + 22;
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 2) & 0xFFFFFFFC;
	ctx.r8.u64 = PPC_LOAD_U32(ctx.r10.u32 + ctx.r3.u32);
	ctx.r5.u64 = ctx.r9.u64 + ctx.r8.u64;
	PPC_STORE_U32(ctx.r10.u32 + ctx.r3.u32, ctx.r5.u32);
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r3.u32 + 216);
	ctx.r4.u64 = PPC_LOAD_U32(ctx.r3.u32 + 220);
	cr6.compare<uint32_t>(ctx.r10.u32, ctx.r4.u32, ctx.xer);
	if (!cr6.lt) goto loc_823753B4;
	PPC_STORE_U32(ctx.r3.u32 + 220, ctx.r10.u32);
loc_823753B4:
	ctx.r3.s64 = ctx.r11.s64 + 16;
	__restgprlr_24(ctx, base);
	return;
}

PPC_FUNC(sub_82380A50)
{
    PPC_FUNC_PROLOGUE();
    PPCRegister ctr{};
    PPCCRRegister cr0{};
    PPCCRRegister cr6{};
    uint32_t ea{};
    PPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
    PPC_STORE_U64(ctx.r1.u32 + -24, ctx.r30.u64);
    PPC_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
    ea = -112 + ctx.r1.u32;
    PPC_STORE_U32(ea, ctx.r1.u32);
    ctx.r1.u32 = ea;
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
    ctx.r31.u64 = ctx.r3.u64;
    ctx.r30.u64 = ctx.r4.u64;
    cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
    if (cr6.eq) goto loc_82380A90;
loc_82380A78:
    ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 8);
    cr6.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
    if (cr6.gt) goto loc_82380AB0;
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);
    cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
    if (!cr6.eq) goto loc_82380A78;
loc_82380A90:
    ctx.r5.s64 = 4096;
    ctx.r4.s64 = 4080;
    ctx.r3.u64 = ctx.r30.u64;
    sub_82374FC0(ctx, base);
    ctx.r11.u64 = ctx.r3.u64;
    cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
    if (!cr6.eq) goto loc_82380AC8;
    goto loc_82380B7C;
loc_82380AB0:
    ctx.r3.u64 = PPC_LOAD_U32(ctx.r11.u32 + 12);
    ctx.r10.s64 = ctx.r10.s64 + -1;
    ctx.r9.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
    PPC_STORE_U32(ctx.r11.u32 + 8, ctx.r10.u32);
    PPC_STORE_U32(ctx.r11.u32 + 12, ctx.r9.u32);
    goto loc_82380B7C;
loc_82380AC8:
    ctx.r10.u64 = PPC_LOAD_U32(ctx.r30.u32 + 528);
    ctx.r8.s64 = 1;
    ctx.r7.s64 = ctx.r11.s64 - ctx.r10.s64;
    ctx.xer.ca = (ctx.r7.s32 < 0) & ((ctx.r7.u32 & 0xFFF) != 0);
    ctx.r10.s64 = ctx.r7.s32 >> 12;
    ctx.r9.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 27) & 0x7FFFFFF;
    ctx.r6.u64 = ctx.r10.u32 & 0x1F;
    ctx.r5.s64 = ctx.r9.s64 + 133;
    ctx.r4.u64 = ctx.r6.u8 & 0x20 ? 0 : (ctx.r8.u32 << (ctx.r6.u8 & 0x3F));
    ctx.r10.u64 = __builtin_rotateleft64(ctx.r5.u32 | (ctx.r5.u64 << 32), 2) & 0xFFFFFFFC;
    ctx.r3.u64 = PPC_LOAD_U32(ctx.r10.u32 + ctx.r30.u32);
    ctx.r9.u64 = ctx.r4.u64 | ctx.r3.u64;
    PPC_STORE_U32(ctx.r10.u32 + ctx.r30.u32, ctx.r9.u32);
    ctx.r10.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
    cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
    if (cr6.eq) goto loc_82380B08;
    PPC_STORE_U32(ctx.r10.u32 + 0, ctx.r11.u32);
loc_82380B08:
    {
        uint16_t cSize = PPC_LOAD_U16(ctx.r31.u32 + 4);
        uint16_t cCount = PPC_LOAD_U16(ctx.r31.u32 + 6);
        if (cSize == 0 || cCount == 0) {
            uint32_t offset = (ctx.r31.u32 >= ctx.r30.u32) ? (ctx.r31.u32 - ctx.r30.u32) : 296;
            if (cSize == 0) {
                uint32_t slot = (offset >= 264) ? ((offset - 264) >> 3) : 4;
                cSize = (slot > 0) ? static_cast<uint16_t>(slot << 2) : 16;
                if (cSize < 16) cSize = 16;
                if (cSize > 2048) cSize = 2048;
                PPC_STORE_U16(ctx.r31.u32 + 4, cSize);
            }
            cCount = (4096 - 32) / cSize;
            if (cCount == 0) cCount = 1;
            PPC_STORE_U16(ctx.r31.u32 + 6, cCount);
            if (App::s_verboseAlloc)
                LOGF_INFO("[sub_82380A50] Initialized SBA bucket 0x{:08X} (offset={}) -> chunkSize={} chunkCount={}",
                          ctx.r31.u32, offset, cSize, cCount);
        }
    }
    ctx.r7.s64 = 0;
    ctx.r9.u64 = PPC_LOAD_U32(ctx.r13.u32 + 0);
    ctx.r8.s64 = 176;
    PPC_STORE_U32(ctx.r11.u32 + 0, ctx.r7.u32);
    ctx.r6.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
    PPC_STORE_U32(ctx.r11.u32 + 4, ctx.r6.u32);
    ctx.r10.u64 = PPC_LOAD_U16(ctx.r31.u32 + 6);
    ctx.r5.s64 = ctx.r10.s64 + -1;
    PPC_STORE_U32(ctx.r11.u32 + 16, ctx.r31.u32);
    PPC_STORE_U32(ctx.r11.u32 + 8, ctx.r5.u32);
    ctx.r4.u64 = PPC_LOAD_U32(ctx.r8.u32 + ctx.r9.u32);
    PPC_STORE_U32(ctx.r11.u32 + 20, ctx.r4.u32);
    ctx.r10.u64 = PPC_LOAD_U16(ctx.r31.u32 + 4);
    ctx.r10.u64 = ctx.r10.u64 + ctx.r11.u64;
    ctx.r10.s64 = ctx.r10.s64 + 32;
    PPC_STORE_U32(ctx.r11.u32 + 12, ctx.r10.u32);
    ctx.r3.u64 = PPC_LOAD_U16(ctx.r31.u32 + 6);

    {
        uint16_t cSize = PPC_LOAD_U16(ctx.r31.u32 + 4);
        uint16_t cCount = static_cast<uint16_t>(ctx.r3.u16);
        static std::atomic<int> s_allocCalls{0};
        if (App::s_verboseAlloc && s_allocCalls++ < 20) {
            LOGF_INFO("[TRACE sub_82380A50] bucket=0x{:08X} chunkSize={} chunkCount={} page=0x{:08X}",
                      ctx.r31.u32, cSize, cCount, ctx.r11.u32);
        }
    }

    ctx.xer.ca = ctx.r3.u32 > 1;
    ctx.r9.s64 = ctx.r3.s64 + -2;
    cr0.compare<int32_t>(ctx.r9.s32, 0, ctx.xer);
    if (cr0.eq || ctx.r3.u32 <= 2) goto loc_82380B70;
    ctx.r8.u64 = PPC_LOAD_U16(ctx.r31.u32 + 4);
    ctr.u64 = ctx.r9.u64;
loc_82380B5C:
    ctx.r9.u64 = ctx.r8.u64 + ctx.r10.u64;
    PPC_STORE_U32(ctx.r10.u32 + 0, ctx.r9.u32);
    ctx.r8.u64 = PPC_LOAD_U16(ctx.r31.u32 + 4);
    ctx.r10.u64 = ctx.r8.u64 + ctx.r10.u64;
    --ctr.u64;
    if (ctr.u32 != 0) goto loc_82380B5C;
loc_82380B70:
    PPC_STORE_U32(ctx.r10.u32 + 0, ctx.r7.u32);
    ctx.r3.s64 = ctx.r11.s64 + 32;
    PPC_STORE_U32(ctx.r31.u32 + 0, ctx.r11.u32);
loc_82380B7C:
    ctx.r1.s64 = ctx.r1.s64 + 112;
    ctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);
    ctx.r30.u64 = PPC_LOAD_U64(ctx.r1.u32 + -24);
    ctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 + -16);
    return;
}

// =============================================================================
// RAGE Worker Thread Pool Startup Hook
// =============================================================================
PPC_EXTERN_FUNC(sub_83639F48);
PPC_EXTERN_FUNC(sub_83196A28);

PPC_FUNC(sub_831977F8)
{
    LOGF_WARNING("[sub_831977F8] Initializing RAGE worker thread pool (4 workers)");

    // 1. Mark pool active: *(0x83BB1800 + 32808) = 1
    PPC_STORE_U32(0x83BB1800 + 32808, 1);

    // 2. Spawn exactly 4 worker threads for CPU cores 1, 2, 3, 5
    uint32_t r30 = 0x83BB1800 + 32824;
    for (uint32_t i = 0; i < 4; i++)
    {
        // Thread name in caller stack buffer (r1 + 80)
        char nameBuf[64];
        snprintf(nameBuf, sizeof(nameBuf), "WorkerThread_%u", i);
        uint32_t nameGuestAddr = ctx.r1.u32 + 80;
        memcpy(base + nameGuestAddr, nameBuf, strlen(nameBuf) + 1);

        // Call sub_83639F48 to create event
        PPCContext eventCtx = ctx;
        eventCtx.r3.u64 = 0;
        eventCtx.r4.u64 = 0;
        eventCtx.r5.u64 = 0;
        eventCtx.r6.u64 = 0;
        sub_83639F48(eventCtx, base);
        uint32_t eventHandle = eventCtx.r3.u32;

        // Store event handle at 0x83BB1800 + 32812 + (i * 4)
        PPC_STORE_U32(r30 - 12, eventHandle);

        // Core assignment: 1, 2, 3, 5
        uint32_t core = (i + 1 == 4) ? 5 : (i + 1);
        PPCContext threadCtx = ctx;
        threadCtx.r3.u64 = 0x83197450;
        threadCtx.r4.u64 = i;
        threadCtx.r5.u64 = 65536;
        threadCtx.r6.u64 = 1;
        threadCtx.r7.u64 = nameGuestAddr;
        threadCtx.r8.u64 = core;
        threadCtx.r9.u64 = 0;
        sub_83196A28(threadCtx, base);
        uint32_t threadHandle = threadCtx.r3.u32;

        // Store thread handle at r30 + 4
        r30 += 4;
        PPC_STORE_U32(r30, threadHandle);
        LOGF_INFO("[sub_831977F8] Worker {} event=0x{:08X} threadHandle=0x{:08X} core={}", 
                  i, eventHandle, threadHandle, core);
    }
    LOGF_WARNING("[sub_831977F8] Successfully initialized 4 worker threads! Worker pool ready.");
}

// =============================================================================
// RAGE Memory Allocator Hooks (sub_8255DC20 / sub_8255DC88)
// =============================================================================
// In RAGE engine, global operator new / sysMemAllocator::Allocate is routed
// through sub_8255DC20, which loads the thread allocator from [r13+0] + 120 (0x78)
// and invokes allocator->Allocate(size, alignment, flags).
// Similarly, operator delete is routed through sub_8255DC88 -> allocator->Free(ptr).
//
// If the thread TLS allocator is not yet initialized (or points to an unmapped
// vtable), calling the indirect function pointer causes an immediate SIGSEGV.
// Here we provide direct allocation using g_userHeap when the vtable is unmapped,
// ensuring early engine allocations (e.g. sub_8343F7F8 container initialization)
// succeed smoothly.
// =============================================================================

extern "C" void __imp__sub_8255DC20(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_8255DC88(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_836B4378(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_8363FCB0(PPCContext& ctx, uint8_t* base);

static std::mutex s_fallbackAllocMutex;
static std::unordered_set<uint32_t> s_fallbackAllocAddrs;

PPC_FUNC(sub_8363FCB0)
{
    const uint32_t allocator = ctx.r3.u32;
    const uint32_t size = ctx.r5.u32;
    if (allocator < 0x10000u || allocator > PPC_MEMORY_SIZE - sizeof(uint32_t) ||
        size > 0x7FFFFFFFu)
    {
        static std::atomic<uint32_t> s_invalidAllocatorCount{0};
        const uint32_t count = ++s_invalidAllocatorCount;
        if (count <= 32)
        {
            LOGF_WARNING("[MEM] Rejected invalid allocator receiver #{} object=0x{:08X} "
                         "flags=0x{:08X} size=0x{:08X} lr=0x{:08X}",
                         count, allocator, ctx.r4.u32, size,
                         static_cast<uint32_t>(ctx.lr));
        }
        ctx.r3.u64 = 0;
        return;
    }

    __imp__sub_8363FCB0(ctx, base);
}

PPC_FUNC(sub_8255DC20)
{
    static uint32_t s_allocCount = 0;
    ++s_allocCount;

    uint32_t size = ctx.r3.u32;
    uint32_t alignment = ctx.r4.u32 ? ctx.r4.u32 : 16;
    if (alignment < 16) alignment = 16;

    if (size > 0x7FFFFFFFu)
    {
        LOGF_WARNING("[MEM] Rejected invalid allocation size=0x{:08X} align=0x{:08X} "
                     "lr=0x{:08X} r13=0x{:08X} r1=0x{:08X}",
                     size, alignment, static_cast<uint32_t>(ctx.lr),
                     ctx.r13.u32, ctx.r1.u32);
        ctx.r3.u64 = 0;
        return;
    }

    // Check if the thread has a valid allocator vtable in TLS
    uint32_t threadCtx = (ctx.r13.u32 != 0) ? PPC_LOAD_U32(ctx.r13.u32 + 0) : 0;
    uint32_t allocator = (threadCtx != 0) ? PPC_LOAD_U32(threadCtx + 120) : 0;
    if (allocator == 0)
    {
        allocator = 0x83887168; // Master RAGE allocator
    }
    uint32_t vtable = (allocator != 0) ? PPC_LOAD_U32(allocator + 0) : 0;
    uint32_t allocFn = (vtable != 0) ? PPC_LOAD_U32(vtable + 8) : 0;

    if (s_allocCount == 1)
    {
        for (int i = 0; i < 10; ++i)
        {
            LOGF_INFO("[ALLOC] 0x83887168[{}] = 0x{:08X}", i, PPC_LOAD_U32(0x83887168 + i * 4));
        }
    }

    if (allocFn >= PPC_CODE_BASE && allocFn < (PPC_CODE_BASE + PPC_CODE_SIZE))
    {
        uint32_t numLayers = (allocator == 0x83887168) ? PPC_LOAD_U32(allocator + 36) : 1;
        if (numLayers == 0 || numLayers > 8) numLayers = 1;
        for (uint32_t layer = 0; layer < numLayers; ++layer)
        {
            ctx.r3.u64 = allocator;
            ctx.r4.u64 = size;
            ctx.r5.u64 = alignment;
            ctx.r6.u64 = layer;
            PPC_CALL_INDIRECT_FUNC(allocFn);
            if (ctx.r3.u32 != 0)
            {
                if (s_allocCount <= 10)
                {
                    LOGF_INFO("[ALLOC_RET] allocFn=0x{:08X} layer={} returned 0x{:08X} for size={}",
                              allocFn, layer, ctx.r3.u32, size);
                }
                return;
            }
        }
    }

    // Direct guest heap allocation fallback (when SBA is exhausted or allocator returns NULL)
    void* hostPtr = g_userHeap.AllocPhysical(size, alignment ? alignment : 16);

    if (hostPtr)
    {
        uint32_t guestAddr = g_memory.MapVirtual(reinterpret_cast<uint8_t*>(hostPtr));
        {
            std::lock_guard<std::mutex> lock(s_fallbackAllocMutex);
            s_fallbackAllocAddrs.insert(guestAddr);
        }
        ctx.r3.u32 = guestAddr;
        if (App::s_verboseAlloc && (s_allocCount <= 30 || (s_allocCount % 500 == 0) || size > 500000))
        {
            LOGF_INFO("[ALLOC] sub_8255DC20 #{} size={} align={} -> 0x{:08X} (fallback)", 
                      s_allocCount, size, alignment, guestAddr);
        }
    }
    else
    {
        ctx.r3.u32 = 0;
        static std::atomic<uint32_t> s_failedAllocCount{0};
        uint32_t fcount = ++s_failedAllocCount;
        if (fcount <= 30 || (fcount % 5000 == 0))
        {
            LOGF_ERROR("[ALLOC] sub_8255DC20 #{} FAILED for size={} (fail #{})", s_allocCount, size, fcount);
        }
    }
}

PPC_FUNC(sub_836B4378)
{
    const uint32_t object = ctx.r3.u32;
    const uint32_t output = ctx.r5.u32;
    if (object < 0x40000000u || object > PPC_MEMORY_SIZE - 884)
    {
        static std::atomic<uint32_t> s_invalidStreamObjectCount{0};
        const uint32_t count = ++s_invalidStreamObjectCount;
        if (count <= 8)
        {
            LOGF_WARNING("[MEM] Rejected invalid stream object #{} object=0x{:08X} "
                         "destination=0x{:08X} lr=0x{:08X}",
                         count, object, ctx.r4.u32, static_cast<uint32_t>(ctx.lr));
        }
        if (ctx.r4.u32 == 0 && output >= 0x10000 &&
            output <= PPC_MEMORY_SIZE - sizeof(uint32_t))
        {
            PPC_STORE_U32(output, 0);
        }
        ctx.r3.u64 = 0;
        return;
    }

    const uint32_t streamLength = PPC_LOAD_U32(object + 876);
    const uint32_t streamOffset = PPC_LOAD_U32(object + 880);
    const uint64_t remaining = static_cast<uint64_t>(streamLength) + streamOffset;
    if (streamLength > 0x7FFFFFFFu || remaining > 0x7FFFFFFFu)
    {
        static std::atomic<uint32_t> s_invalidStreamCount{0};
        const uint32_t count = ++s_invalidStreamCount;
        LOGF_WARNING("[MEM] Rejected invalid stream range #{} object=0x{:08X} "
                     "length=0x{:08X} offset=0x{:08X} remaining=0x{:08X} lr=0x{:08X}",
                     count, object, streamLength, streamOffset,
                     static_cast<uint32_t>(remaining), static_cast<uint32_t>(ctx.lr));
        PPC_STORE_U32(object + 876, 0);
        PPC_STORE_U32(object + 880, 0);
        if (ctx.r4.u32 == 0 && output >= 0x10000 &&
            output <= PPC_MEMORY_SIZE - sizeof(uint32_t))
        {
            PPC_STORE_U32(output, 0);
        }
        ctx.r3.u64 = 0;
        return;
    }

    __imp__sub_836B4378(ctx, base);
}

PPC_FUNC(sub_8255DC88)
{
    uint32_t guestAddr = ctx.r3.u32;
    if (guestAddr == 0) return;

    if ((guestAddr & 3) != 0 || guestAddr < 0x10000)
    {
        static int s_unalignedFreeCount = 0;
        if (s_unalignedFreeCount++ < 10)
        {
            LOGF_WARNING("[FREE] sub_8255DC88 ignoring invalid/unaligned pointer 0x{:08X} (r12=0x{:08X})", guestAddr, ctx.r12.u32);
        }
        return;
    }

    // Check if this pointer was allocated by our fallback allocator
    bool isFallback = false;
    {
        std::lock_guard<std::mutex> lock(s_fallbackAllocMutex);
        auto it = s_fallbackAllocAddrs.find(guestAddr);
        if (it != s_fallbackAllocAddrs.end())
        {
            s_fallbackAllocAddrs.erase(it);
            isFallback = true;
        }
    }
    if (isFallback)
    {
        void* hostPtr = g_memory.Translate(guestAddr);
        if (hostPtr)
        {
            g_userHeap.Free(hostPtr);
        }
        return;
    }

    // Don't attempt to free static image memory or unmapped space
    if (guestAddr >= 0x80000000 && guestAddr < 0xA0000000)
    {
        return;
    }

    uint32_t threadCtx = (ctx.r13.u32 != 0) ? PPC_LOAD_U32(ctx.r13.u32 + 0) : 0;
    uint32_t allocator = (threadCtx != 0) ? PPC_LOAD_U32(threadCtx + 120) : 0;
    if (allocator == 0)
    {
        // Fall back to master RAGE allocator if thread allocator is unpopulated
        allocator = 0x83887168;
    }

    uint32_t vtable = (allocator != 0) ? PPC_LOAD_U32(allocator + 0) : 0;
    uint32_t freeFn = (vtable != 0) ? PPC_LOAD_U32(vtable + 16) : 0;

    static uint32_t s_freeCount = 0;
    if (App::s_verboseAlloc && (++s_freeCount <= 20 || (s_freeCount % 500 == 0)))
    {
        LOGF_INFO("[FREE] sub_8255DC88 #{} ptr=0x{:08X} threadCtx=0x{:08X} alloc=0x{:08X} vtable=0x{:08X} freeFn=0x{:08X}",
                  s_freeCount, guestAddr, threadCtx, allocator, vtable, freeFn);
    }

    if (freeFn >= PPC_CODE_BASE && freeFn < (PPC_CODE_BASE + PPC_CODE_SIZE))
    {
        ctx.r3.u64 = allocator;
        ctx.r4.u64 = guestAddr;
        PPC_CALL_INDIRECT_FUNC(freeFn);
        return;
    }

    // Do NOT free unmanaged guest addresses in physicalHeap (> 0xA0000000) directly with g_userHeap.Free.
    // If it was allocated by our fallback allocator, it was already handled above via s_fallbackAllocAddrs.
    // Calling g_userHeap.Free on guest internal allocations corrupts o1heap metadata.
    LOGF_WARNING("[FREE] sub_8255DC88 skipping unmanaged free ptr=0x{:08X} allocator=0x{:08X} vtable=0x{:08X} freeFn=0x{:08X}",
                 guestAddr, allocator, vtable, freeFn);
}

// =============================================================================
// RAGE Scoped Allocator Push & Commit Hooks (sub_831938B8, sub_83193930)
// =============================================================================
// When pushing a scoped allocator, sub_831938B8 and sub_83193930 query the
// current allocator from [PCR + 120]. If the current allocator is NULL on a
// newly spawned guest thread, calling vtable[2] or vtable[6] causes SIGSEGV.
// These hooks guard the indirect calls and ensure thread safety.
// =============================================================================
PPC_FUNC(sub_831938B8)
{
    uint32_t r3_in = ctx.r3.u32;
    uint32_t r4_in = ctx.r4.u32;

    uint32_t r11 = (r3_in != 0 && r3_in < PPC_MEMORY_SIZE) ? PPC_LOAD_U32(r3_in + 8) : 0;
    if (r11 != 0 && r11 < PPC_MEMORY_SIZE)
    {
        PPC_STORE_U32(r11 + 4, r4_in);
    }

    uint32_t r31 = (ctx.r13.u32 != 0 && ctx.r13.u32 < PPC_MEMORY_SIZE) ? PPC_LOAD_U32(ctx.r13.u32 + 0) : 0;
    uint32_t currentAlloc = (r31 != 0 && r31 < PPC_MEMORY_SIZE) ? PPC_LOAD_U32(r31 + 120) : 0;
    if (currentAlloc == 0) currentAlloc = 0x83887168;
    uint32_t result = 0;

    if (currentAlloc != 0 && currentAlloc >= 0x80000000 && currentAlloc < PPC_MEMORY_SIZE)
    {
        uint32_t vtable = PPC_LOAD_U32(currentAlloc + 0);
        if (vtable != 0 && vtable >= 0x80000000 && vtable < PPC_MEMORY_SIZE)
        {
            uint32_t fn = PPC_LOAD_U32(vtable + 8);
            if (fn >= 0x82000000 && fn < 0x84000000)
            {
                ctx.r3.u64 = currentAlloc;
                ctx.r4.u64 = r4_in;
                ctx.r5.u64 = 16;
                ctx.r6.u64 = 0;
                PPC_CALL_INDIRECT_FUNC(fn);
                result = ctx.r3.u32;
            }
            else
            {
                LOGF_WARNING("[TRACE] sub_831938B8 vtable[2]=0x{:08X} not code, skipping indirect call", fn);
            }
        }
    }
    else
    {
        LOGF_INFO("[TRACE] sub_831938B8 currentAlloc=0x{:08X} (no allocator on thread), skipping indirect call", currentAlloc);
    }

    uint32_t r7 = (r3_in != 0 && r3_in < PPC_MEMORY_SIZE) ? PPC_LOAD_U32(r3_in + 8) : 0;
    if (r7 != 0 && r7 < PPC_MEMORY_SIZE)
    {
        PPC_STORE_U32(r7 + 0, result);
    }

    if (r31 != 0 && r31 < PPC_MEMORY_SIZE && r3_in != 0 && r3_in < PPC_MEMORY_SIZE)
    {
        uint32_t oldAlloc = PPC_LOAD_U32(r31 + 116);
        PPC_STORE_U32(r31 + 116, r3_in);
        PPC_STORE_U32(r3_in + 12, oldAlloc);
    }
}

PPC_FUNC(sub_83193930)
{
    uint32_t r3_in = ctx.r3.u32;
    if (r3_in == 0 || r3_in >= PPC_MEMORY_SIZE) return;

    uint32_t r11 = PPC_LOAD_U32(r3_in + 4);
    uint32_t r8 = r11 + 15;
    uint32_t r9 = PPC_LOAD_U32(r3_in + 8);
    uint32_t r5 = r8 & 0xFFFFFFF0;
    PPC_STORE_U32(r3_in + 4, r5);

    uint32_t r30 = (ctx.r13.u32 != 0 && ctx.r13.u32 < PPC_MEMORY_SIZE) ? PPC_LOAD_U32(ctx.r13.u32 + 0) : 0;
    uint32_t currentAlloc = (r30 != 0 && r30 < PPC_MEMORY_SIZE) ? PPC_LOAD_U32(r30 + 120) : 0;
    if (currentAlloc == 0) currentAlloc = 0x83887168;

    if (currentAlloc != 0 && currentAlloc >= 0x80000000 && currentAlloc < PPC_MEMORY_SIZE)
    {
        uint32_t vtable = PPC_LOAD_U32(currentAlloc + 0);
        if (vtable != 0 && vtable >= 0x80000000 && vtable < PPC_MEMORY_SIZE)
        {
            uint32_t fn = PPC_LOAD_U32(vtable + 24);
            if (fn >= 0x82000000 && fn < 0x84000000)
            {
                ctx.r3.u64 = currentAlloc;
                ctx.r4.u64 = (r9 != 0 && r9 < PPC_MEMORY_SIZE) ? PPC_LOAD_U32(r9 + 0) : 0;
                ctx.r5.u64 = r5;
                PPC_CALL_INDIRECT_FUNC(fn);
            }
            else
            {
                LOGF_WARNING("[TRACE] sub_83193930 vtable[6]=0x{:08X} not code, skipping indirect call", fn);
            }
        }
    }
    else
    {
        LOGF_INFO("[TRACE] sub_83193930 currentAlloc=0x{:08X} (no allocator on thread), skipping indirect call", currentAlloc);
    }

    uint32_t r5_check = PPC_LOAD_U32(r3_in + 4);
    if (r5_check == 0 && r9 != 0 && r9 < PPC_MEMORY_SIZE)
    {
        PPC_STORE_U32(r9 + 0, 0);
    }

    if (r9 != 0 && r9 < PPC_MEMORY_SIZE)
    {
        PPC_STORE_U32(r9 + 4, r5_check);
    }

    if (r30 != 0 && r30 < PPC_MEMORY_SIZE)
    {
        uint32_t r7 = PPC_LOAD_U32(r3_in + 12);
        PPC_STORE_U32(r30 + 116, r7 ? r7 : 0x83887168);
    }
    PPC_STORE_U32(r3_in + 12, 0);
}

// =============================================================================
// RAGE Scoped Allocator Pop Hook (sub_831939E0)
// =============================================================================
// When popping a scoped allocator, sub_831939E0 restores the previous allocator
// saved at [r3 + 12]. If the previous allocator was NULL (e.g. at the bottom
// of the allocator stack on a newly spawned thread), dereferencing it to call
// vtable slot 4 causes an immediate SIGSEGV. This hook safely performs the call
// only when prev and its vtable are valid, and always executes the remaining
// stack cleanup.
// =============================================================================
PPC_FUNC(sub_831939E0)
{
    uint32_t r31 = ctx.r3.u32;
    uint32_t prev = PPC_LOAD_U32(r31 + 12);
    uint32_t r11 = PPC_LOAD_U32(r31 + 8);

    if (prev != 0 && prev >= 0x80000000)
    {
        uint32_t vtable = PPC_LOAD_U32(prev + 0);
        if (vtable != 0 && vtable >= 0x80000000)
        {
            uint32_t fn = PPC_LOAD_U32(vtable + 16);
            if (fn >= 0x82000000 && fn < 0x84000000)
            {
                ctx.r3.u64 = prev;
                ctx.r4.u64 = (r11 != 0) ? PPC_LOAD_U32(r11 + 0) : 0;
                PPC_CALL_INDIRECT_FUNC(fn);
            }
            else
            {
                LOGF_WARNING("[TRACE] sub_831939E0 vtable[4]=0x{:08X} not code, skipping indirect call", fn);
            }
        }
    }
    else
    {
        LOGF_INFO("[TRACE] sub_831939E0 prev=0x{:08X} (allocator root), skipping indirect call", prev);
    }

    uint32_t r6 = (r11 != 0) ? r11 : PPC_LOAD_U32(r31 + 8);
    if (r6 != 0 && r6 >= 0x80000000)
    {
        PPC_STORE_U32(r6 + 4, 0);
        PPC_STORE_U32(r6 + 0, 0);
    }
    PPC_STORE_U32(r31 + 4, 0);

    uint32_t r8 = (ctx.r13.u32 != 0) ? PPC_LOAD_U32(ctx.r13.u32 + 0) : 0;
    if (r8 != 0 && r8 >= 0x80000000)
    {
        PPC_STORE_U32(r8 + 120, prev ? prev : 0x83887168);
    }
    PPC_STORE_U32(r31 + 12, 0);
}

// =============================================================================
// GPU Ring Buffer Wait & PM4 Dispatch Hooks (sub_8250D988, sub_82220140)
// =============================================================================
// GTA V calls sub_8250D988 to ensure sufficient room in the ring buffer.
// The hook observes the ring without fabricating a read-pointer/fence update.
// =============================================================================
PPC_FUNC(sub_8250D988)
{
    static uint32_t s_flushCount = 0;
    ++s_flushCount;

    uint32_t dev = ctx.r3.u32;
    uint32_t requestedSize = ctx.r4.u32;

    if (s_flushCount <= 20 || (s_flushCount % 200 == 0))
    {
        LOGF_INFO("[GPU RING] sub_8250D988 #{} dev=0x{:08X} requestedSize={} flags=0x{:08X}",
                  s_flushCount, dev, requestedSize, ctx.r5.u32);
    }

    ScanRingBufferForPM4Packets();
}

PPC_FUNC(sub_82220140)
{
    // Signal that no wait is required; do not modify guest fence state.
    ctx.r3.s64 = 0;
}

// =============================================================================
// CRT & RAGE Memcpy Hooks (sub_83642890, sub_83641FE0, sub_823CFAF0)
// =============================================================================
// On PowerPC / Xenon, CRT memcpy / XMemCpy / RAGE memcpy use AltiVec vector
// instructions and the dcbzl (data cache block zero) instruction to zero 128-byte
// cache lines ahead of the destination. In lifted C++, dcbzl is translated to:
//   memset(base + ((...) & ~127), 0, 128);
// which overwrites memory past buffer bounds and triggers segmentation faults
// on unaligned or boundary-crossing buffers during file streaming and engine init.
// Replacing these functions with native std::memmove guarantees memory safety,
// eliminates out-of-bounds dcbzl zeroing, and achieves maximum host throughput.
// =============================================================================

PPC_FUNC(sub_83642890)
{
    uint32_t dst = ctx.r3.u32;
    uint32_t src = ctx.r4.u32;
    uint32_t size = ctx.r5.u32;

    if (size > 0 && dst != 0 && src != 0)
    {
        if ((uint64_t)dst + size <= PPC_MEMORY_SIZE && (uint64_t)src + size <= PPC_MEMORY_SIZE)
        {
            std::memmove(base + dst, base + src, size);
        }
        else
        {
            LOGF_ERROR("[MEMCPY] sub_83642890 OUT OF BOUNDS! dst=0x{:08X} src=0x{:08X} size={}", dst, src, size);
        }
    }
    // ctx.r3 remains dst
}

PPC_FUNC(sub_83641FE0)
{
    uint32_t dst = ctx.r3.u32;
    uint32_t src = ctx.r4.u32;
    uint32_t size = ctx.r5.u32;

    if (size > 0 && dst != 0 && src != 0)
    {
        if ((uint64_t)dst + size <= PPC_MEMORY_SIZE && (uint64_t)src + size <= PPC_MEMORY_SIZE)
        {
            std::memmove(base + dst, base + src, size);
        }
        else
        {
            LOGF_ERROR("[MEMCPY] sub_83641FE0 OUT OF BOUNDS! dst=0x{:08X} src=0x{:08X} size={}", dst, src, size);
        }
    }
    // ctx.r3 remains dst
}

#include <dlfcn.h>

PPC_FUNC(sub_823CFAF0)
{
    uint32_t dst = ctx.r3.u32;
    uint32_t src = ctx.r4.u32;
    const uint32_t size = ctx.r5.u32;

    if (size == 0)
        return;

    if (size == 0xFFFFFFFFu || size >= 0x10000000u ||
        static_cast<uint64_t>(dst) + size > PPC_MEMORY_SIZE ||
        static_cast<uint64_t>(src) + size > PPC_MEMORY_SIZE)
    {
        static std::atomic<uint32_t> s_rejectedCopyCount{0};
        if (s_rejectedCopyCount.fetch_add(1, std::memory_order_relaxed) < 32)
        {
            LOGF_WARNING("[MEM] sub_823CFAF0 rejected invalid copy dst=0x{:08X} src=0x{:08X} size=0x{:08X} lr=0x{:08X}",
                         dst, src, size, static_cast<uint32_t>(ctx.lr));
        }
        return;
    }

    std::memmove(base + dst, base + src, size);
}

PPC_FUNC(sub_83622DF0)
{
    const uint32_t dst = ctx.r3.u32;
    const uint8_t val = static_cast<uint8_t>(ctx.r4.u32);
    const uint32_t size = ctx.r5.u32;

    if (size == 0)
        return;

    if (size >= 0x10000000u || dst < 0x10000 || static_cast<uint64_t>(dst) + size > PPC_MEMORY_SIZE)
    {
        static std::atomic<uint32_t> s_rejectedMemsetCount{0};
        if (s_rejectedMemsetCount.fetch_add(1, std::memory_order_relaxed) < 32)
        {
            LOGF_WARNING("[MEMSET] sub_83622DF0 rejected invalid memset dst=0x{:08X} val=0x{:02X} size=0x{:08X} lr=0x{:08X}",
                         dst, val, size, static_cast<uint32_t>(ctx.lr));
        }
        return;
    }

    std::memset(base + dst, val, size);
}

PPC_FUNC(sub_82EF3FA0)
{
    if (ctx.r3.u32 < 0x10000 || ctx.r4.u32 < 0x10000 ||
        ctx.r3.u32 >= PPC_MEMORY_SIZE - 8 || ctx.r4.u32 >= PPC_MEMORY_SIZE - 4)
    {
        ctx.r3.u64 = 0;
        return;
    }

    uint16_t numBuckets = PPC_LOAD_U16(ctx.r3.u32 + 4);
    if (numBuckets == 0)
    {
        ctx.r3.u64 = 0;
        return;
    }

    uint32_t key = PPC_LOAD_U32(ctx.r4.u32 + 0);
    uint32_t bucketsArray = PPC_LOAD_U32(ctx.r3.u32 + 0);
    if (bucketsArray < 0x10000 || bucketsArray >= PPC_MEMORY_SIZE - numBuckets * 4)
    {
        ctx.r3.u64 = 0;
        return;
    }

    uint32_t bucketIndex = key % numBuckets;
    uint32_t node = PPC_LOAD_U32(bucketsArray + bucketIndex * 4);
    
    int depth = 0;
    while (node >= 0x10000 && node < PPC_MEMORY_SIZE - 12)
    {
        uint32_t nodeKey = PPC_LOAD_U32(node + 0);
        if (key == nodeKey)
        {
            ctx.r3.u64 = node + 4;
            return;
        }
        if (++depth > 256)
        {
            static int s_cycleWarn = 0;
            if (s_cycleWarn++ < 10)
            {
                LOGF_WARNING("[sub_82EF3FA0] atMap::Find cycle detected! map=0x{:08X} key=0x{:08X} bucket={}",
                             ctx.r3.u32, key, bucketIndex);
            }
            break;
        }
        node = PPC_LOAD_U32(node + 8);
    }

    ctx.r3.u64 = 0;
}

PPC_FUNC(sub_827E1D70)
{
    if (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 10)
    {
        ctx.r3.u64 = 0;
        return;
    }

    uint16_t numBuckets = PPC_LOAD_U16(ctx.r3.u32 + 8);
    if (numBuckets == 0)
    {
        ctx.r3.u64 = 0;
        return;
    }

    uint32_t bucketsArray = PPC_LOAD_U32(ctx.r3.u32 + 4);
    if (bucketsArray < 0x10000 || bucketsArray >= PPC_MEMORY_SIZE - numBuckets * 4)
    {
        ctx.r3.u64 = 0;
        return;
    }

    uint32_t key = ctx.r4.u32;
    uint32_t bucketIndex = key % numBuckets;
    uint32_t node = PPC_LOAD_U32(bucketsArray + bucketIndex * 4);

    int depth = 0;
    while (node >= 0x10000 && node < PPC_MEMORY_SIZE - 12)
    {
        uint32_t nodeKey = PPC_LOAD_U32(node + 0);
        if (key == nodeKey)
        {
            ctx.r3.u64 = PPC_LOAD_U32(node + 4);
            return;
        }
        if (++depth > 256)
        {
            static int s_cycleWarn = 0;
            if (s_cycleWarn++ < 10)
            {
                LOGF_WARNING("[sub_827E1D70] atMap::Find cycle detected! map=0x{:08X} key=0x{:08X} bucket={}",
                             ctx.r3.u32, key, bucketIndex);
            }
            break;
        }
        uint32_t nextNode = PPC_LOAD_U32(node + 8);
        if (nextNode == node) break;
        node = nextNode;
    }

    ctx.r3.u64 = 0;
}

extern "C" void __imp__sub_831C1B10(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_831C1B10)
{
    __imp__sub_831C1B10(ctx, base);
}

PPC_FUNC(sub_831E3240)
{
    uint32_t stream = ctx.r3.u32;
    if (stream == 0 || stream >= PPC_MEMORY_SIZE)
    {
        ctx.r3.s64 = 0;
        return;
    }

    uint32_t device = PPC_LOAD_U32(stream + 0);
    uint32_t handle = PPC_LOAD_U32(stream + 4);

    if (App::s_logVfs)
        LOGF_INFO("[TRACE] sub_831E3240 (fiStream::Close): stream=0x{:08X} device=0x{:08X} handle=0x{:08X}",
                  stream, device, handle);

    if (device != 0 && device < PPC_MEMORY_SIZE)
    {
        uint32_t vtable = PPC_LOAD_U32(device + 0);
        if (vtable >= PPC_CODE_BASE && vtable < PPC_MEMORY_SIZE)
        {
            uint32_t closeFn = PPC_LOAD_U32(vtable + 48);
            if (App::s_logVfs)
                LOGF_INFO("[TRACE] sub_831E3240: device=0x{:08X} vtable=0x{:08X} closeFn=0x{:08X}",
                          device, vtable, closeFn);
            if (closeFn >= PPC_CODE_BASE && closeFn < PPC_CODE_BASE + PPC_CODE_SIZE)
            {
                ctx.r3.u64 = device;
                ctx.r4.u64 = handle;
                PPC_CALL_INDIRECT_FUNC(closeFn);
                if (App::s_logVfs)
                    LOGF_INFO("[TRACE] sub_831E3240: closeFn returned.");
            }
            else
            {
                LOGF_WARNING("[TRACE] sub_831E3240: device=0x{:08X} vtable=0x{:08X} invalid closeFn=0x{:08X}",
                             device, vtable, closeFn);
            }
        }
    }

    // Clear handle and device in stream struct
    PPC_STORE_U32(stream + 4, 0xFFFFFFFF);
    PPC_STORE_U32(stream + 0, 0);
    ctx.r3.s64 = 0;
    if (App::s_logVfs)
        LOGF_INFO("[TRACE] sub_831E3240 done.");
}

// =============================================================================
// CRT STRING FORMATTING HOOKS (vsnprintf / _output_l)
// =============================================================================
// Prevents indirect unmapped bctr switch jump corruption in sub_823F4118.
// Handles Xbox 360 big-endian PowerPC 8-byte aligned va_list argument fetching.

struct GuestVaList {
    uint8_t* base;
    uint32_t current;

    explicit GuestVaList(uint8_t* b, uint32_t start) : base(b), current(start) {}

    uint32_t GetU32() {
        uint32_t aligned = (current + 7) & ~7;
        current = aligned + 8;
        if (aligned + 8 > PPC_MEMORY_SIZE) return 0;
        return ByteSwap(*(uint32_t*)(base + aligned + 4));
    }

    int32_t GetS32() {
        return static_cast<int32_t>(GetU32());
    }

    uint64_t GetU64() {
        uint32_t aligned = (current + 7) & ~7;
        current = aligned + 8;
        if (aligned + 8 > PPC_MEMORY_SIZE) return 0;
        return ByteSwap(*(uint64_t*)(base + aligned));
    }

    int64_t GetS64() {
        return static_cast<int64_t>(GetU64());
    }

    double GetDouble() {
        uint64_t u = GetU64();
        double d;
        std::memcpy(&d, &u, sizeof(d));
        return d;
    }

    const char* GetString() {
        uint32_t gPtr = GetU32();
        if (gPtr == 0 || gPtr >= PPC_MEMORY_SIZE) return "(null)";
        return reinterpret_cast<const char*>(base + gPtr);
    }
};

static int FormatGuestVsnprintf(uint8_t* base, char* dst, size_t maxLen, const char* fmt, uint32_t argptrGuest)
{
    if (!dst || maxLen == 0) return 0;
    if (!fmt) {
        dst[0] = '\0';
        return 0;
    }

    GuestVaList va(base, argptrGuest);
    std::string result;
    result.reserve(256);

    const char* p = fmt;
    while (*p) {
        if (*p != '%') {
            result.push_back(*p++);
            continue;
        }

        if (*(p + 1) == '%') {
            result.push_back('%');
            p += 2;
            continue;
        }

        const char* specStart = p;
        p++; // skip '%'

        // Flags
        while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0') {
            p++;
        }

        // Width
        bool widthFromArg = false;
        int widthVal = 0;
        if (*p == '*') {
            widthFromArg = true;
            widthVal = va.GetS32();
            p++;
        } else {
            while (isdigit(static_cast<unsigned char>(*p))) {
                p++;
            }
        }

        // Precision
        bool precFromArg = false;
        int precVal = 0;
        if (*p == '.') {
            p++;
            if (*p == '*') {
                precFromArg = true;
                precVal = va.GetS32();
                p++;
            } else {
                while (isdigit(static_cast<unsigned char>(*p))) {
                    p++;
                }
            }
        }

        // Length modifiers
        enum LengthMod { LenDefault, LenShort, LenLong, LenLongLong };
        LengthMod lenMod = LenDefault;
        if (*p == 'h') {
            lenMod = LenShort;
            p++;
            if (*p == 'h') p++;
        } else if (*p == 'l') {
            lenMod = LenLong;
            p++;
            if (*p == 'l') {
                lenMod = LenLongLong;
                p++;
            }
        } else if (*p == 'I') {
            if (*(p + 1) == '6' && *(p + 2) == '4') {
                lenMod = LenLongLong;
                p += 3;
            } else if (*(p + 1) == '3' && *(p + 2) == '2') {
                lenMod = LenLong;
                p += 3;
            } else {
                p++;
            }
        } else if (*p == 'z' || *p == 'j' || *p == 't') {
            p++;
        }

        char spec = *p;
        if (!spec) break;
        p++;

        std::string token;
        token.push_back('%');
        const char* flagPtr = specStart + 1;
        while (*flagPtr == '-' || *flagPtr == '+' || *flagPtr == ' ' || *flagPtr == '#' || *flagPtr == '0') {
            token.push_back(*flagPtr++);
        }
        if (widthFromArg) {
            token += std::to_string(widthVal);
        } else {
            while (flagPtr < p - 1 && isdigit(static_cast<unsigned char>(*flagPtr))) {
                token.push_back(*flagPtr++);
            }
        }
        if (precFromArg) {
            token += "." + std::to_string(precVal);
        } else if (flagPtr < p - 1 && *flagPtr == '.') {
            token.push_back(*flagPtr++);
            while (flagPtr < p - 1 && isdigit(static_cast<unsigned char>(*flagPtr))) {
                token.push_back(*flagPtr++);
            }
        }

        char buf[1024];
        int written = 0;

        switch (spec) {
            case 's': {
                token.push_back('s');
                const char* s = va.GetString();
                written = snprintf(buf, sizeof(buf), token.c_str(), s);
                break;
            }
            case 'c': {
                token.push_back('c');
                int c = va.GetS32();
                written = snprintf(buf, sizeof(buf), token.c_str(), c);
                break;
            }
            case 'd':
            case 'i': {
                if (lenMod == LenLongLong) {
                    token += "lld";
                    int64_t v = va.GetS64();
                    written = snprintf(buf, sizeof(buf), token.c_str(), (long long)v);
                } else {
                    token.push_back(spec);
                    int32_t v = va.GetS32();
                    written = snprintf(buf, sizeof(buf), token.c_str(), v);
                }
                break;
            }
            case 'u':
            case 'x':
            case 'X':
            case 'o': {
                if (lenMod == LenLongLong) {
                    token += (spec == 'u' ? "llu" : (spec == 'x' ? "llx" : (spec == 'X' ? "llX" : "llo")));
                    uint64_t v = va.GetU64();
                    written = snprintf(buf, sizeof(buf), token.c_str(), (unsigned long long)v);
                } else {
                    token.push_back(spec);
                    uint32_t v = va.GetU32();
                    written = snprintf(buf, sizeof(buf), token.c_str(), v);
                }
                break;
            }
            case 'p': {
                uint32_t ptr = va.GetU32();
                written = snprintf(buf, sizeof(buf), "0x%08X", ptr);
                break;
            }
            case 'f':
            case 'F':
            case 'g':
            case 'G':
            case 'e':
            case 'E': {
                token.push_back(spec);
                double d = va.GetDouble();
                written = snprintf(buf, sizeof(buf), token.c_str(), d);
                break;
            }
            default: {
                token.push_back(spec);
                written = snprintf(buf, sizeof(buf), "%s", token.c_str());
                break;
            }
        }

        if (written > 0) {
            result.append(buf, written);
        }
    }

    size_t copyLen = std::min(result.size(), maxLen - 1);
    std::memcpy(dst, result.data(), copyLen);
    dst[copyLen] = '\0';
    return static_cast<int>(result.size());
}

PPC_FUNC(sub_82409368)
{
    uint32_t dstGuest = ctx.r3.u32;
    uint32_t maxLen = ctx.r4.u32 + 1; // sub_82409368 passed maxLen-1 in r4
    uint32_t fmtGuest = ctx.r5.u32;
    uint32_t argptrGuest = ctx.r6.u32;

    char* dst = reinterpret_cast<char*>(base + dstGuest);
    const char* fmt = reinterpret_cast<const char*>(base + fmtGuest);

    int count = FormatGuestVsnprintf(base, dst, maxLen, fmt, argptrGuest);

    static int s_logCount = 0;
    if (++s_logCount <= 30) {
        LOGF_INFO("[CRT_VSNPRINTF] #{} fmt='{}' out='{}'", s_logCount, fmt ? fmt : "null", dst ? dst : "null");
    }

    ctx.r3.s64 = count;
}

PPC_FUNC(sub_83626570)
{
    uint32_t dstGuest = ctx.r3.u32;
    uint32_t maxLen = ctx.r4.u32;
    uint32_t fmtGuest = ctx.r5.u32;
    uint32_t argptrGuest = ctx.r6.u32;

    char* dst = reinterpret_cast<char*>(base + dstGuest);
    const char* fmt = reinterpret_cast<const char*>(base + fmtGuest);

    int count = FormatGuestVsnprintf(base, dst, maxLen, fmt, argptrGuest);

    static int s_logCount = 0;
    if (++s_logCount <= 30) {
        LOGF_INFO("[CRT_VSNPRINTF_2] #{} fmt='{}' out='{}'", s_logCount, fmt ? fmt : "null", dst ? dst : "null");
    }

    ctx.r3.s64 = count;
}

PPC_FUNC(sub_823F4000)
{
    uint32_t dstGuest = ctx.r3.u32;
    uint32_t maxLen = ctx.r4.u32;
    uint32_t fmtGuest = ctx.r5.u32;
    // ctx.r6 is locale
    uint32_t argptrGuest = ctx.r7.u32;

    char* dst = reinterpret_cast<char*>(base + dstGuest);
    const char* fmt = reinterpret_cast<const char*>(base + fmtGuest);

    int count = FormatGuestVsnprintf(base, dst, maxLen, fmt, argptrGuest);

    static int s_logCount = 0;
    if (++s_logCount <= 30) {
        LOGF_INFO("[CRT_VSNPRINTF_L] #{} fmt='{}' out='{}'", s_logCount, fmt ? fmt : "null", dst ? dst : "null");
    }

    ctx.r3.s64 = count;
}

PPC_FUNC(sub_823F4118)
{
    // MSVC CRT _output_l(FILE* str, const char* format, _locale_t locale, va_list argptr)
    uint32_t fileGuest = ctx.r3.u32;
    uint32_t fmtGuest = ctx.r4.u32;
    // ctx.r5 is locale
    uint32_t argptrGuest = ctx.r6.u32;

    if (fileGuest == 0 || fileGuest >= PPC_MEMORY_SIZE) {
        ctx.r3.s64 = -1;
        return;
    }

    uint32_t ptrGuest = ByteSwap(*(uint32_t*)(base + fileGuest + 0));
    int32_t cnt = static_cast<int32_t>(ByteSwap(*(uint32_t*)(base + fileGuest + 4)));
    const char* fmt = reinterpret_cast<const char*>(base + fmtGuest);

    size_t maxLen = (cnt > 0) ? static_cast<size_t>(cnt) : 65536;
    if (maxLen > 65536) maxLen = 65536;

    char* dst = reinterpret_cast<char*>(base + ptrGuest);
    int count = FormatGuestVsnprintf(base, dst, maxLen, fmt, argptrGuest);

    static int s_logCount = 0;
    if (++s_logCount <= 30) {
        LOGF_INFO("[CRT_OUTPUT_L] #{} fmt='{}' out='{}'", s_logCount, fmt ? fmt : "null", dst ? dst : "null");
    }

    size_t written = std::min(static_cast<size_t>(count), maxLen - 1);
    *(uint32_t*)(base + fileGuest + 0) = ByteSwap(static_cast<uint32_t>(ptrGuest + written));
    if (cnt > 0) {
        *(uint32_t*)(base + fileGuest + 4) = ByteSwap(static_cast<uint32_t>(cnt - written));
    }

    ctx.r3.s64 = count;
}

PPC_FUNC(sub_829A3AC8)
{
    uint32_t arr = ctx.r4.u32;
    int32_t count = ctx.r5.s32;

    static int s_dtorCount = 0;
    ++s_dtorCount;
    if (s_dtorCount <= 30 || s_dtorCount % 500 == 0) {
        LOGF_WARNING("[sub_829A3AC8] Vector dtor #{} arr=0x{:08X} count={}", s_dtorCount, arr, count);
    }

    if (arr >= 0x10000 && count > 0 && count < 100000)
    {
        uint32_t oldSp = ctx.r1.u32;
        ctx.r1.u32 -= 112;
        PPC_STORE_U32(ctx.r1.u32, oldSp);

        uint32_t elem = arr;
        for (int32_t i = 0; i < count; ++i)
        {
            uint32_t vtable = PPC_LOAD_U32(elem);
            if (vtable >= PPC_CODE_BASE && vtable < (PPC_CODE_BASE + PPC_CODE_SIZE))
            {
                uint32_t dtor = PPC_LOAD_U32(vtable);
                if (dtor >= PPC_CODE_BASE && dtor < (PPC_CODE_BASE + PPC_CODE_SIZE))
                {
                    auto target = PPC_LOOKUP_FUNC(base, dtor);
                    if (target != nullptr)
                    {
                        ctx.r3.u64 = elem;
                        ctx.r4.u64 = 0;
                        target(ctx, base);
                    }
                }
            }
            elem += 16;
        }

        ctx.r1.u32 = oldSp;
    }

    // Free array memory
    if (arr >= 0x10000)
    {
        ctx.r3.u64 = arr;
        sub_8255DCC0(ctx, base);
    }
}

PPC_FUNC(sub_827AA6C0)
{
    uint32_t arr = ctx.r4.u32;
    int32_t count = ctx.r5.s32;

    static int s_dtorCount = 0;
    ++s_dtorCount;
    if (s_dtorCount <= 30 || s_dtorCount % 500 == 0) {
        LOGF_WARNING("[sub_827AA6C0] Vector dtor #{} arr=0x{:08X} count={}", s_dtorCount, arr, count);
    }

    if (arr >= 0x10000 && count > 0 && count < 100000)
    {
        uint32_t oldSp = ctx.r1.u32;
        ctx.r1.u32 -= 112;
        PPC_STORE_U32(ctx.r1.u32, oldSp);

        uint32_t elem = arr;
        for (int32_t i = 0; i < count; ++i)
        {
            uint32_t vtable = PPC_LOAD_U32(elem);
            if (vtable >= PPC_CODE_BASE && vtable < (PPC_CODE_BASE + PPC_CODE_SIZE))
            {
                uint32_t dtor = PPC_LOAD_U32(vtable);
                if (dtor >= PPC_CODE_BASE && dtor < (PPC_CODE_BASE + PPC_CODE_SIZE))
                {
                    auto target = PPC_LOOKUP_FUNC(base, dtor);
                    if (target != nullptr)
                    {
                        ctx.r3.u64 = elem;
                        ctx.r4.u64 = 0;
                        target(ctx, base);
                    }
                }
            }
            elem += 20; // 20-byte structs
        }

        ctx.r1.u32 = oldSp;
    }

    // Free array memory
    if (arr >= 0x10000)
    {
        ctx.r3.u64 = arr;
        sub_8255DCC0(ctx, base);
    }
}


PPC_FUNC(sub_832301B8)
{
    uint32_t r3 = ctx.r3.u32;
    if (r3 < 0x10000) return;

    int32_t count = static_cast<int32_t>(PPC_LOAD_U32(r3));
    if (count <= 0 || count > 128) {
        // 776-byte channel holds at most ~64-128 slots. If count > 128 it is uninitialized BSS garbage.
        return;
    }

    static int s_callCount = 0;
    if (++s_callCount <= 10) {
        LOGF_INFO("[sub_832301B8] #{} r3=0x{:08X} count={}", s_callCount, r3, count);
    }

    // r7 points to static default state at 0x8383D120
    uint32_t r7 = 0x8383D120;

    for (int32_t r4 = 0; r4 < count; ++r4)
    {
        uint32_t r5 = r3 + 8 + (r4 * 4);
        uint32_t r11 = PPC_LOAD_U32(r5 + 512);
        if (s_callCount <= 2 && r4 < 10) {
            LOGF_WARNING("[sub_832301B8] r4={} r5=0x{:08X} r11=0x{:08X}", r4, r5, r11);
        }
        if (r11 < 0x10000) {
            // Null or invalid slot - skip to prevent writing to page zero
            continue;
        }

        uint8_t flag38 = PPC_LOAD_U8(r11 + 38);
        uint8_t flag39 = PPC_LOAD_U8(r11 + 39);
        bool cond = (flag38 != 0 && flag39 == 0);
        if (!cond) {
            uint32_t val40 = PPC_LOAD_U32(r11 + 40);
            if ((val40 & 1) != 0) {
                continue;
            }
        }

        PPC_STORE_U32(r11 + 4, 0);
        uint32_t r9_0 = PPC_LOAD_U32(r7 + 0);
        PPC_STORE_U32(r11 + 12, r9_0);
        uint32_t r9_4 = PPC_LOAD_U32(r7 + 4);
        PPC_STORE_U32(r11 + 16, r9_4);
        uint32_t r8 = PPC_LOAD_U32(r11 + 44);
        uint32_t r10_8 = PPC_LOAD_U32(r7 + 8);
        PPC_STORE_U32(r11 + 20, r10_8);

        if (r8 >= 0x10000) {
            uint8_t byte37 = PPC_LOAD_U8(r11 + 37);
            uint32_t idx = static_cast<uint32_t>(byte37) * 20;
            uint32_t baseAddr = r8 + idx;
            PPC_STORE_U32(baseAddr + 0, 0);
            PPC_STORE_U32(baseAddr + 8, r9_0);
            PPC_STORE_U32(baseAddr + 12, r9_4);
            PPC_STORE_U32(baseAddr + 16, r10_8);
        }
    }
}

PPC_FUNC(sub_83159B48)
{
    uint32_t mapPtr = ctx.r3.u32;
    uint32_t newCap = ctx.r4.u32 & 0xFFFF;
    uint16_t rawNewCap = ctx.r4.u16;

    static int s_rehashCount = 0;
    ++s_rehashCount;
    if (s_rehashCount <= 50 || s_rehashCount % 100 == 0) {
        uint16_t oldCap = (mapPtr >= 0x10000) ? PPC_LOAD_U16(mapPtr + 4) : 0;
        uint16_t oldElemCount = (mapPtr >= 0x10000) ? PPC_LOAD_U16(mapPtr + 6) : 0;
        LOGF_WARNING("[sub_83159B48] Rehash #{} map=0x{:08X} oldCap={} oldElems={} newCap={}",
                     s_rehashCount, mapPtr, oldCap, oldElemCount, newCap);
    }

    if (mapPtr < 0x10000 || newCap == 0)
        return;

    // Allocate new bucket array (newCap * 4 bytes, aligned to 4)
    uint32_t allocSize = (newCap <= 0x3FFF) ? (newCap * 4) : 0xFFFFFFFF;
    ctx.r3.u64 = allocSize;
    ctx.r4.u64 = 4;
    sub_8255DC60(ctx, base);
    uint32_t newBuckets = ctx.r3.u32;
    if (newBuckets < 0x10000) {
        LOGF_ERROR("[sub_83159B48] Allocation failed for newBuckets! size={}", allocSize);
        return;
    }

    // Zero the new buckets array
    memset(base + newBuckets, 0, newCap * 4);

    uint16_t oldCap = PPC_LOAD_U16(mapPtr + 4);
    uint32_t oldBuckets = PPC_LOAD_U32(mapPtr + 0);

    if (oldCap != 0 && oldBuckets >= 0x10000)
    {
        for (uint32_t bucketIdx = 0; bucketIdx < oldCap; ++bucketIdx)
        {
            uint32_t node = PPC_LOAD_U32(oldBuckets + bucketIdx * 4);
            int loopLimit = 0;
            while (node >= 0x10000 && loopLimit++ < 10000)
            {
                uint32_t nextNode = PPC_LOAD_U32(node + 8);
                uint32_t hash = PPC_LOAD_U32(node + 0);
                uint32_t newIdx = hash % newCap;
                uint32_t currentHead = PPC_LOAD_U32(newBuckets + newIdx * 4);
                PPC_STORE_U32(node + 8, currentHead);
                PPC_STORE_U32(newBuckets + newIdx * 4, node);
                node = nextNode;
            }
            if (loopLimit >= 10000) {
                LOGF_ERROR("[sub_83159B48] CYCLE DETECTED in bucketIdx={} oldBuckets=0x{:08X} node=0x{:08X}",
                           bucketIdx, oldBuckets, node);
            }
        }
    }

    // Update map fields
    PPC_STORE_U16(mapPtr + 4, rawNewCap);
    if (oldBuckets >= 0x10000) {
        ctx.r3.u64 = oldBuckets;
        sub_8255DCC0(ctx, base);
    }
    PPC_STORE_U32(mapPtr + 0, newBuckets);
}

PPC_FUNC(sub_831C1A68)
{
    // Stack allocation of 128 bytes
    uint32_t oldSp = ctx.r1.u32;
    ctx.r1.u32 -= 128;
    PPC_STORE_U32(ctx.r1.u32, oldSp);

    uint32_t in_r3 = ctx.r3.u32;
    uint32_t in_r4 = ctx.r4.u32;

    PPC_STORE_U32(ctx.r1.u32 + 88, in_r3);
    PPC_STORE_U32(ctx.r1.u32 + 80, 0);
    PPC_STORE_U32(ctx.r1.u32 + 84, 0);

    ctx.r3.u64 = ctx.r1.u32 + 80;
    sub_82FAFBF8(ctx, base);

    uint32_t r31 = PPC_LOAD_U32(ctx.r1.u32 + 84);
    uint32_t returnVal = 0xFFFFFFFF; // -1

    static int s_c1a68Count = 0;
    ++s_c1a68Count;
    if (s_c1a68Count <= 30 || s_c1a68Count % 100 == 0) {
        LOGF_INFO("[sub_831C1A68] #{} r3=0x{:08X} r4=0x{:08X} r31=0x{:08X}",
                  s_c1a68Count, in_r3, in_r4, r31);
    }

    int loopCount = 0;
    while (r31 != 0 && r31 >= 0x10000 && loopCount++ < 1000)
    {
        uint32_t fn = PPC_LOAD_U32(r31 + 8);
        uint32_t arg = PPC_LOAD_U32(r31 + 4);

        if (fn >= PPC_CODE_BASE && fn < (PPC_CODE_BASE + PPC_CODE_SIZE))
        {
            auto target = PPC_LOOKUP_FUNC(base, fn);
            if (target != nullptr)
            {
                if (arg != 0) {
                    ctx.r3.u64 = arg;
                    ctx.r4.u64 = in_r4;
                } else {
                    ctx.r3.u64 = in_r4;
                }
                target(ctx, base);
                if ((ctx.r3.u32 & 0xFF) != 0)
                {
                    returnVal = PPC_LOAD_U32(r31 + 0);
                    break;
                }
            }
        }
        else
        {
            LOGF_WARNING("[sub_831C1A68] Invalid callback fn=0x{:08X} in element 0x{:08X}", fn, r31);
        }

        ctx.r3.u64 = ctx.r1.u32 + 80;
        sub_831B8078(ctx, base);
        r31 = PPC_LOAD_U32(ctx.r1.u32 + 84);
    }

    ctx.r1.u32 = oldSp;
    ctx.r3.s64 = static_cast<int32_t>(returnVal);
}

PPC_FUNC(sub_831E0BE8)
{
    uint32_t archive = ctx.r3.u32;
    uint32_t pathGuest = ctx.r4.u32;

    if (archive < 0x10000 || pathGuest < 0x10000)
    {
        ctx.r3.u64 = 0;
        return;
    }

    uint32_t entriesPtr = PPC_LOAD_U32(archive + 16);
    uint32_t namesPtr = PPC_LOAD_U32(archive + 8);
    if (entriesPtr < 0x10000 || namesPtr < 0x10000)
    {
        ctx.r3.u64 = 0;
        return;
    }

    const char* rawPath = reinterpret_cast<const char*>(base + pathGuest);
    while (*rawPath == '/' || *rawPath == '\\')
        rawPath++;

    if (*rawPath == '\0')
    {
        ctx.r3.u64 = 0;
        return;
    }

    static int s_findCount = 0;
    ++s_findCount;
    if (s_findCount <= 30 || s_findCount % 100 == 0) {
        LOGF_INFO("[sub_831E0BE8] #{} archive=0x{:08X} path='{}'", s_findCount, archive, rawPath);
    }

    uint8_t shift = PPC_LOAD_U8(archive + 136);
    uint32_t currentDirEntry = entriesPtr; // root directory entry

    while (*rawPath != '\0')
    {
        // Extract component into lowercase buffer
        char comp[256];
        size_t compLen = 0;
        while (*rawPath != '\0' && *rawPath != '/' && *rawPath != '\\' && compLen < sizeof(comp) - 1)
        {
            char c = *rawPath++;
            if (c >= 'A' && c <= 'Z') c += 32;
            comp[compLen++] = c;
        }
        comp[compLen] = '\0';

        // Skip subsequent slashes
        while (*rawPath == '/' || *rawPath == '\\')
            rawPath++;

        uint32_t startIdx = PPC_LOAD_U32(currentDirEntry + 8);
        uint32_t count = PPC_LOAD_U32(currentDirEntry + 12);
        if (count == 0)
        {
            ctx.r3.u64 = 0;
            return;
        }

        int32_t low = static_cast<int32_t>(startIdx);
        int32_t high = static_cast<int32_t>(startIdx + count - 1);
        uint32_t matchedEntry = 0;

        while (low <= high)
        {
            int32_t mid = (low + high) / 2;
            uint32_t entry = entriesPtr + static_cast<uint32_t>(mid) * 16;
            uint32_t nameOffset = PPC_LOAD_U32(entry + 4) & 0xFFFF;
            uint32_t nameAddr = namesPtr + (nameOffset << shift);
            const char* entryName = reinterpret_cast<const char*>(base + nameAddr);

            int cmp = strcasecmp(comp, entryName);
            if (cmp < 0)
            {
                high = mid - 1;
            }
            else if (cmp > 0)
            {
                low = mid + 1;
            }
            else
            {
                matchedEntry = entry;
                break;
            }
        }

        if (matchedEntry == 0)
        {
            ctx.r3.u64 = 0;
            return;
        }

        uint64_t entryHdr = PPC_LOAD_U64(matchedEntry + 0);
        uint64_t dirFlag = (__builtin_rotateleft64(entryHdr, 24)) & 0x7FFFFF;
        bool isDir = (dirFlag == 0x7FFFFF);

        if (*rawPath != '\0')
        {
            // More segments remain, matched entry must be a directory
            if (!isDir)
            {
                ctx.r3.u64 = 0;
                return;
            }
            currentDirEntry = matchedEntry;
        }
        else
        {
            // Final segment matched
            ctx.r3.u64 = matchedEntry;
            return;
        }
    }

    ctx.r3.u64 = 0;
}

PPC_EXTERN_FUNC(sub_8255DC58);
PPC_EXTERN_FUNC(sub_831CF770);
void sub_831D1618(PPCContext& ctx, uint8_t* base);

PPC_FUNC(sub_831E20C8)
{
    const uint32_t caller = GET_CALLER_LR();
    LOGF_WARNING("[sub_831E20C8] Fatal XML/AST error handler! caller_lr=0x{:08X} r3=0x{:08X}", caller, ctx.r3.u32);
    ctx.r3.u64 = 0;
}

// =============================================================================
// XML / AST Node Allocator & Factory (sub_831D1618)
// Resolves switch jump tables at 0x831D1664 (types 0-15) and 0x831D17C8 (types 17-30)
// =============================================================================
PPC_FUNC(sub_831D1618)
{
    uint32_t nodePtr = ctx.r3.u32;
    if (nodePtr == 0 || nodePtr < 0x10000)
    {
        ctx.r3.u64 = 0;
        return;
    }

    uint8_t type = PPC_LOAD_U8(nodePtr + 8);

    static int s_nodeCount = 0;
    ++s_nodeCount;
    if (s_nodeCount <= 30 || type == 255)
    {
        uint32_t namePtr = PPC_LOAD_U32(nodePtr + 0);
        const char* name = (namePtr >= 0x10000 && namePtr < PPC_MEMORY_SIZE) ? reinterpret_cast<const char*>(base + namePtr) : "(null)";
        LOGF_INFO("[sub_831D1618] #{} node=0x{:08X} type={} name='{}'", s_nodeCount, nodePtr, type, name);
    }

    if (type == 16)
    {
        ctx.r3.s64 = 16;
        sub_8255DC58(ctx, base);
        uint32_t alloc = ctx.r3.u32;
        if (alloc == 0)
        {
            ctx.r3.u64 = 0;
            return;
        }
        ctx.r4.u64 = nodePtr;
        sub_831CF770(ctx, base);
        return;
    }

    if (type == 255)
    {
        ctx.r3.u64 = nodePtr;
        sub_831E20C8(ctx, base);
        return;
    }

    uint32_t vtable = 0;
    bool isType13 = false;

    if (type <= 15)
    {
        switch (type)
        {
            case 0: case 1: case 2: case 3:
            case 4: case 5: case 6: case 7:
                vtable = 0x82077430; // Block 0x831D1800
                break;
            case 8: case 9: case 10:
                vtable = 0x82077350; // Block 0x831D1830
                break;
            case 11:
                vtable = 0x82077400; // Block 0x831D16CC
                break;
            case 12:
                vtable = 0x82077418; // Block 0x831D16A4
                break;
            case 13:
                vtable = 0x82077368; // Block 0x831D16F4 (12 bytes, child)
                isType13 = true;
                break;
            case 14:
                vtable = 0x82077380; // Block 0x831D1728
                break;
            case 15:
                vtable = 0x820773D0; // Block 0x831D1750
                break;
            default:
                break;
        }
    }
    else if (type >= 17 && type <= 30)
    {
        uint32_t subIdx = type - 17;
        switch (subIdx)
        {
            case 0: case 1: case 5: case 6: case 7:
                vtable = 0x820773E8; // Block 0x831D1858
                break;
            case 2: case 3: case 4: case 10:
                vtable = 0x82077350; // Block 0x831D1830
                break;
            case 8: case 9: case 11: case 12: case 13:
                vtable = 0x82077430; // Block 0x831D1800
                break;
            default:
                break;
        }
    }

    if (vtable == 0)
    {
        ctx.r3.u64 = 0;
        return;
    }

    if (isType13)
    {
        ctx.r3.s64 = 12;
        sub_8255DC58(ctx, base);
        uint32_t alloc = ctx.r3.u32;
        if (alloc == 0)
        {
            ctx.r3.u64 = 0;
            return;
        }
        PPC_STORE_U32(alloc + 0, vtable);
        PPC_STORE_U32(alloc + 4, nodePtr);

        // Recursively create child node
        uint32_t childNode = PPC_LOAD_U32(nodePtr + 28);
        ctx.r3.u64 = childNode;
        sub_831D1618(ctx, base);
        PPC_STORE_U32(alloc + 8, ctx.r3.u32);

        ctx.r3.u64 = alloc;
        return;
    }
    else
    {
        ctx.r3.s64 = 8;
        sub_8255DC58(ctx, base);
        uint32_t alloc = ctx.r3.u32;
        if (alloc == 0)
        {
            ctx.r3.u64 = 0;
            return;
        }
        PPC_STORE_U32(alloc + 0, vtable);
        PPC_STORE_U32(alloc + 4, nodePtr);

        ctx.r3.u64 = alloc;
        return;
    }
}

// =============================================================================
// AST / Node Virtual Method Dispatcher (sub_831C4EA8)
// Resolves 31-entry switch jump table at 0x831C4F50
// =============================================================================
PPC_FUNC(sub_831C4EA8)
{
    uint32_t r31_in = ctx.r3.u32;
    uint32_t r29_in = ctx.r4.u32;
    uint32_t r30_in = ctx.r5.u32;

    if (r31_in < 0x10000 || r30_in < 0x10000)
    {
        return;
    }

    uint32_t r8 = PPC_LOAD_U32(r30_in + 4);
    if (r8 < 0x10000)
    {
        return;
    }

    uint8_t type = PPC_LOAD_U8(r8 + 8);

    if (type <= 30 && type != 255)
    {
        uint32_t targetAddr = PPC_LOAD_U32(0x831C4F50 + type * 4);
        if (targetAddr >= 0x82000000 && targetAddr < 0x84000000)
        {
            uint32_t instr = PPC_LOAD_U32(targetAddr + 12);
            uint32_t vtableOffset = instr & 0xFFFF;

            uint32_t r11_child = PPC_LOAD_U32(r8 + 4);
            uint32_t arg_r4 = r11_child + r29_in;

            uint32_t vtable = PPC_LOAD_U32(r31_in + 0);
            if (vtable >= 0x80000000)
            {
                uint32_t methodPtr = PPC_LOAD_U32(vtable + vtableOffset);
                auto* method = g_memory.FindFunction(methodPtr);
                if (method)
                {
                    ctx.r3.u64 = r31_in;
                    ctx.r4.u64 = arg_r4;
                    ctx.r5.u64 = r30_in;
                    method(ctx, base);
                }
                else
                {
                    LOGF_WARNING("[sub_831C4EA8] Method 0x{:08X} (offset {}) not found", methodPtr, vtableOffset);
                }
            }
        }
        PPC_STORE_U32(r31_in + 4, 0);
    }
}

PPC_FUNC(sub_831D7238)
{
    uint32_t r3_in = ctx.r3.u32;
    uint32_t r4_in = ctx.r4.u32;

    static int s_sub831D7238Count = 0;
    ++s_sub831D7238Count;
    if (s_sub831D7238Count <= 10) {
        LOGF_INFO("[sub_831D7238] #{} r3=0x{:08X} r4=0x{:08X}", s_sub831D7238Count, r3_in, r4_in);
    }

    if (r3_in < 0x10000 || r3_in >= PPC_MEMORY_SIZE - 32 || r4_in < 0x10000 || r4_in >= PPC_MEMORY_SIZE - 32)
    {
        static int s_invalidArgCount = 0;
        if (s_invalidArgCount++ < 5) {
            LOGF_WARNING("[sub_831D7238] Invalid pointer arguments: r3=0x{:08X} r4=0x{:08X}", r3_in, r4_in);
        }
        return;
    }

    PPCCRRegister cr6{};
    PPCRegister temp{};
    uint32_t ea{};
    __savegprlr_25(ctx, base);
    ea = -144 + ctx.r1.u32;
    PPC_STORE_U32(ea, ctx.r1.u32);
    ctx.r1.u32 = ea;

    ctx.r11.s64 = -2084765696; // 0x83BD0000
    ctx.r10.u64 = PPC_LOAD_U32(ctx.r4.u32 + 20);
    ctx.r9.u64 = PPC_LOAD_U32(ctx.r4.u32 + 0);
    ctx.r26.u64 = ctx.r3.u64;
    ctx.xer.ca = ctx.r10.u32 > 0;
    ctx.r8.s64 = ctx.r10.s64 + -1;
    ctx.r27.u64 = ctx.r4.u64;
    temp.u8 = (~ctx.r8.u32 + ctx.r10.u32 < ~ctx.r8.u32) | (~ctx.r8.u32 + ctx.r10.u32 + ctx.xer.ca < ctx.xer.ca);
    ctx.r10.u64 = ~ctx.r8.u64 + ctx.r10.u64 + ctx.xer.ca;
    ctx.xer.ca = temp.u8;
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 19732);
    ctx.r25.u64 = ctx.r10.u32 & 0xFF;
    cr6.compare<uint32_t>(ctx.r25.u32, 0, ctx.xer);
    ctx.r30.u64 = (ctx.r11.u32 >= 0x10000 && ctx.r11.u32 < PPC_MEMORY_SIZE - 48) ? PPC_LOAD_U8(ctx.r11.u32 + 48) : 0;
    PPC_STORE_U32(ctx.r3.u32 + 4, ctx.r9.u32);

    if (cr6.eq) goto loc_831D7320;

    ctx.r31.u64 = PPC_LOAD_U32(ctx.r4.u32 + 4);
    cr6.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);
    if (cr6.eq) goto loc_831D7294;
    ctx.r4.s64 = 0;
    ctx.r3.u64 = ctx.r31.u64;
    sub_823C0FB8(ctx, base);
    goto loc_831D7298;

loc_831D7294:
    ctx.r3.s64 = 0;

loc_831D7298:
    ctx.r4.u64 = ctx.r31.u64;
    sub_831B10D8(ctx, base);
    ctx.r28.u64 = ctx.r30.u32 & 0xFF;
    cr6.compare<uint32_t>(ctx.r28.u32, 0, ctx.xer);
    if (cr6.eq) goto loc_831D72B4;
    ctx.r3.u64 = PPC_LOAD_U32(ctx.r27.u32 + 4);
    sub_831D5FE0(ctx, base);

loc_831D72B4:
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r27.u32 + 20);
    ctx.r29.s64 = 0;
    if (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 4) goto loc_831D7320;
    ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);
    cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
    if (cr6.eq) goto loc_831D7320;
    ctx.r30.s64 = 0;

loc_831D72CC:
    if (ctx.r11.u32 + ctx.r30.u32 >= PPC_MEMORY_SIZE - 4) goto loc_831D7320;
    ctx.r31.u64 = PPC_LOAD_U32(ctx.r30.u32 + ctx.r11.u32);
    cr6.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);
    if (cr6.eq) goto loc_831D72E8;
    ctx.r4.s64 = 0;
    ctx.r3.u64 = ctx.r31.u64;
    sub_823C0FB8(ctx, base);
    goto loc_831D72EC;

loc_831D72E8:
    ctx.r3.s64 = 0;

loc_831D72EC:
    ctx.r4.u64 = ctx.r31.u64;
    sub_831B10D8(ctx, base);
    cr6.compare<uint32_t>(ctx.r28.u32, 0, ctx.xer);
    if (cr6.eq) goto loc_831D7308;
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r27.u32 + 20);
    if (ctx.r11.u32 >= 0x10000 && ctx.r11.u32 + ctx.r30.u32 < PPC_MEMORY_SIZE - 4) {
        ctx.r3.u64 = PPC_LOAD_U32(ctx.r11.u32 + ctx.r30.u32);
        sub_831D5FE0(ctx, base);
    }

loc_831D7308:
    ctx.r29.s64 = ctx.r29.s64 + 1;
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r27.u32 + 20);
    ctx.r30.u64 = __builtin_rotateleft64(ctx.r29.u32 | (ctx.r29.u64 << 32), 2) & 0xFFFFFFFC;
    if (ctx.r11.u32 >= 0x10000 && ctx.r30.u32 + ctx.r11.u32 < PPC_MEMORY_SIZE - 4) {
        ctx.r10.u64 = PPC_LOAD_U32(ctx.r30.u32 + ctx.r11.u32);
        cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
        if (!cr6.eq) goto loc_831D72CC;
    }

loc_831D7320:
    ctx.r11.u64 = PPC_LOAD_U8(ctx.r27.u32 + 24);
    ctx.r10.s64 = -ctx.r25.s64;
    ctx.r9.u64 = PPC_LOAD_U8(ctx.r27.u32 + 25);
    ctx.r30.s64 = 0;
    ctx.r8.s64 = -ctx.r11.s64;
    ctx.r7.u64 = PPC_LOAD_U8(ctx.r26.u32 + 20);
    ctx.r6.s64 = -ctx.r9.s64;
    ctx.r5.u64 = ctx.r8.u32 & 0x1;
    ctx.r4.u64 = __builtin_rotateleft64(ctx.r7.u32 | (ctx.r7.u64 << 32), 28) & 0x1;
    ctx.r3.u64 = ctx.r10.u64 ^ ctx.r5.u64;
    ctx.r11.s64 = -ctx.r4.s64;
    ctx.r10.u64 = __builtin_rotateleft64(ctx.r3.u32 | (ctx.r3.u64 << 32), 0) & 0x2;
    ctx.r9.u64 = ctx.r10.u64 ^ ctx.r5.u64;
    ctx.r8.u64 = ctx.r9.u32 & 0xFF;
    ctx.r7.u64 = ctx.r6.u64 ^ ctx.r8.u64;
    ctx.r6.u64 = __builtin_rotateleft64(ctx.r7.u32 | (ctx.r7.u64 << 32), 0) & 0x4;
    ctx.r5.u64 = ctx.r6.u64 ^ ctx.r8.u64;
    ctx.r4.u64 = ctx.r5.u32 & 0xFF;
    ctx.r3.u64 = ctx.r11.u64 ^ ctx.r4.u64;
    ctx.r11.u64 = __builtin_rotateleft64(ctx.r3.u32 | (ctx.r3.u64 << 32), 0) & 0x10;
    ctx.r10.u64 = ctx.r11.u64 ^ ctx.r4.u64;
    PPC_STORE_U8(ctx.r26.u32 + 20, ctx.r10.u8);

    ctx.r28.u64 = PPC_LOAD_U32(ctx.r27.u32 + 12);
    if (ctx.r28.u32 < 0x10000 || ctx.r28.u32 >= PPC_MEMORY_SIZE - 4) goto loc_831D739C;
    ctx.r9.u64 = PPC_LOAD_U32(ctx.r28.u32 + 0);
    cr6.compare<uint32_t>(ctx.r9.u32, 0, ctx.xer);
    if (cr6.eq) goto loc_831D739C;
    ctx.r11.u64 = ctx.r28.u64;

loc_831D738C:
    ea = 4 + ctx.r11.u32;
    if (ea >= PPC_MEMORY_SIZE - 4) goto loc_831D739C;
    ctx.r10.u64 = PPC_LOAD_U32(ea);
    ctx.r11.u32 = ea;
    ctx.r30.s64 = ctx.r30.s64 + 1;
    if (ctx.r30.u32 > 1000) {
        LOGF_ERROR("[sub_831D7238] Loop overrun counting r28 entries! r28=0x{:08X}", ctx.r28.u32);
        goto loc_831D739C;
    }
    cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
    if (!cr6.eq) goto loc_831D738C;

loc_831D739C:
    ctx.r31.s64 = ctx.r26.s64 + 24;
    cr6.compare<uint32_t>(ctx.r30.u32, 0, ctx.xer);
    if (cr6.eq) goto loc_831D73B8;
    ctx.r4.s64 = 16;
    ctx.r3.u64 = __builtin_rotateleft64(ctx.r30.u32 | (ctx.r30.u64 << 32), 2) & 0xFFFFFFFC;
    sub_8255DC78(ctx, base);
    goto loc_831D73BC;

loc_831D73B8:
    ctx.r3.s64 = 0;

loc_831D73BC:
    ctx.r11.u64 = ctx.r30.u64;
    PPC_STORE_U32(ctx.r31.u32 + 0, ctx.r3.u32);
    ctx.r30.s64 = 0;
    PPC_STORE_U16(ctx.r31.u32 + 6, ctx.r11.u16);
    if (ctx.r28.u32 < 0x10000 || ctx.r28.u32 >= PPC_MEMORY_SIZE - 4) goto loc_831D7438;
    ctx.r10.u64 = PPC_LOAD_U32(ctx.r28.u32 + 0);
    cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
    if (cr6.eq) goto loc_831D7438;
    ctx.r11.u64 = ctx.r28.u64;
    ctx.r29.s64 = 0;

loc_831D73E0:
    if (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 4) goto loc_831D7438;
    ctx.r3.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);
    sub_831D1618(ctx, base);

    // CRITICAL FIX: Safe check of r11 (r27 + 16), r10 (r3 + 4), and r11 + r29
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r27.u32 + 16);
    cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
    if (cr6.eq) goto loc_831D7400;

    if (ctx.r3.u32 >= 0x10000 && ctx.r3.u32 < PPC_MEMORY_SIZE - 8 &&
        ctx.r11.u32 >= 0x10000 && ctx.r11.u32 + ctx.r29.u32 < PPC_MEMORY_SIZE - 4)
    {
        ctx.r10.u64 = PPC_LOAD_U32(ctx.r3.u32 + 4);
        if (ctx.r10.u32 >= 0x10000 && ctx.r10.u32 < PPC_MEMORY_SIZE - 8) {
            ctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + ctx.r29.u32);
            PPC_STORE_U32(ctx.r10.u32 + 4, ctx.r9.u32);
        } else {
            LOGF_WARNING("[sub_831D7238] Invalid r10 pointer: 0x{:08X}", ctx.r10.u32);
        }
    } else {
        LOGF_WARNING("[sub_831D7238] Invalid r11/r29 array pointer: r11=0x{:08X} r29=0x{:08X} r3=0x{:08X}",
                     ctx.r11.u32, ctx.r29.u32, ctx.r3.u32);
    }

loc_831D7400:
    ctx.r9.u64 = PPC_LOAD_U16(ctx.r31.u32 + 4);
    ctx.r30.s64 = ctx.r30.s64 + 1;
    ctx.r10.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
    ctx.r11.u64 = ctx.r9.u64;
    ctx.r9.u64 = __builtin_rotateleft32(ctx.r9.u32, 2);
    ctx.r8.s64 = ctx.r11.s64 + 1;
    ctx.r29.u64 = __builtin_rotateleft64(ctx.r30.u32 | (ctx.r30.u64 << 32), 2) & 0xFFFFFFFC;
    ctx.r7.u64 = ctx.r8.u32 & 0xFFFF;
    ctx.r11.u64 = ctx.r29.u64 + ctx.r28.u64;
    PPC_STORE_U16(ctx.r31.u32 + 4, ctx.r7.u16);
    if (ctx.r10.u32 >= 0x10000 && ctx.r9.u32 + ctx.r10.u32 < PPC_MEMORY_SIZE - 4) {
        PPC_STORE_U32(ctx.r9.u32 + ctx.r10.u32, ctx.r3.u32);
    }
    if (ctx.r28.u32 >= 0x10000 && ctx.r29.u32 + ctx.r28.u32 < PPC_MEMORY_SIZE - 4) {
        ctx.r6.u64 = PPC_LOAD_U32(ctx.r29.u32 + ctx.r28.u32);
        cr6.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
        if (!cr6.eq) goto loc_831D73E0;
    }

loc_831D7438:
    ctx.r1.s64 = ctx.r1.s64 + 144;
    __restgprlr_25(ctx, base);
}

PPC_FUNC(sub_831D96E0)
{
    LOGF_INFO("[sub_831D96E0] Entering: r3=0x{:08X}", ctx.r3.u32);
    if (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 64)
    {
        LOGF_WARNING("[sub_831D96E0] Invalid r3=0x{:08X}", ctx.r3.u32);
        return;
    }

    PPCRegister ctr{};
    PPCCRRegister cr6{};
    uint32_t ea{};
    __savegprlr_26(ctx, base);
    ea = -144 + ctx.r1.u32;
    PPC_STORE_U32(ea, ctx.r1.u32);
    ctx.r1.u32 = ea;

    ctx.r11.s64 = -2084765696;
    ctx.r10.s64 = 1;
    ctx.r31.u64 = ctx.r3.u64;
    PPC_STORE_U8(ctx.r3.u32 + 48, ctx.r10.u8);
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 19732);
    ctx.r9.u64 = (ctx.r11.u32 >= 0x10000 && ctx.r11.u32 < PPC_MEMORY_SIZE - 44) ? PPC_LOAD_U32(ctx.r11.u32 + 40) : 0;
    ctx.r11.u64 = __builtin_rotateleft64(ctx.r9.u32 | (ctx.r9.u64 << 32), 14) & 0x1;
    ctx.r26.u64 = ctx.r11.u64;
    cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
    if (cr6.eq) goto loc_831D9718;
    sub_8247BF78(ctx, base);

loc_831D9718:
    ctx.r10.u64 = PPC_LOAD_U32(ctx.r31.u32 + 40);
    ctx.r11.s64 = ctx.r31.s64 + 40;
    ctx.r29.s64 = -2084765696;
    ctx.r28.u64 = PPC_LOAD_U32(ctx.r31.u32 + 36);
    ctx.r8.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 0) & 0x2000;
    ctx.r9.u64 = ctx.r10.u64;
    ctx.r7.u64 = ctx.r8.u64 ^ ctx.r10.u64;
    ctx.r27.u64 = ctx.r10.u64;
    ctx.r5.u64 = __builtin_rotateleft32(ctx.r7.u32, 0);
    PPC_STORE_U32(ctx.r31.u32 + 40, ctx.r7.u32);
    ctx.r6.u64 = __builtin_rotateleft32(ctx.r7.u32, 0);
    ctx.r4.u64 = __builtin_rotateleft64(ctx.r5.u32 | (ctx.r5.u64 << 32), 0) & 0x8000;
    ctx.r3.u64 = ctx.r4.u64 ^ ctx.r6.u64;
    ctx.r10.u64 = __builtin_rotateleft32(ctx.r3.u32, 0);
    PPC_STORE_U32(ctx.r31.u32 + 40, ctx.r3.u32);
    ctx.r11.u64 = __builtin_rotateleft32(ctx.r3.u32, 0);
    ctx.r9.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 0) & 0x4000;
    ctx.r8.u64 = ctx.r9.u64 ^ ctx.r11.u64;
    PPC_STORE_U32(ctx.r31.u32 + 40, ctx.r8.u32);

    // CRITICAL: Safe traversal of the init callback linked list at 0x83BD4D0C
    {
        uint32_t curNode = PPC_LOAD_U32(ctx.r29.u32 + 19724);
        int cbCount = 0;
        std::unordered_set<uint32_t> visited;
        while (curNode >= 0x10000 && curNode < PPC_MEMORY_SIZE - 8 && cbCount < 2000) {
            if (!visited.insert(curNode).second) {
                LOGF_WARNING("[sub_831D96E0] Callback list cycle detected at node 0x{:08X}! Breaking traversal.", curNode);
                break;
            }
            uint32_t fn = PPC_LOAD_U32(curNode + 0);
            uint32_t nextNode = PPC_LOAD_U32(curNode + 4);
            if (fn >= PPC_CODE_BASE && fn < PPC_CODE_BASE + PPC_CODE_SIZE) {
                ctx.r30.u64 = curNode;
                PPC_CALL_INDIRECT_FUNC(fn);
            } else {
                LOGF_WARNING("[sub_831D96E0] Skipping invalid callback target 0x{:08X} at node 0x{:08X}", fn, curNode);
            }
            curNode = nextNode;
            ++cbCount;
        }
        if (cbCount >= 2000) {
            LOGF_ERROR("[sub_831D96E0] Callback list cycle detected! count={}", cbCount);
        }
    }

loc_831D9788:
    ctx.r11.s64 = -2088566784;
    ctx.r30.s64 = ctx.r11.s64 + 23592;
    ctx.r11.u64 = PPC_LOAD_U8(ctx.r11.u32 + 23592);
    cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
    if (!cr6.eq) goto loc_831D97C8;
    ctx.r11.u64 = PPC_LOAD_U16(ctx.r30.u32 + 8);
    cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
    if (cr6.eq) goto loc_831D97C0;
    ctx.r3.u64 = PPC_LOAD_U32(ctx.r30.u32 + 4);
    ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 3) & 0xFFFFFFF8;
    ctx.r10.s64 = -2095382528;
    ctx.r4.u64 = ctx.r11.u64 + ctx.r3.u64;
    ctx.r5.s64 = ctx.r10.s64 + 16120;
    sub_82F8BCA8(ctx, base);

loc_831D97C0:
    ctx.r11.s64 = 1;
    PPC_STORE_U8(ctx.r30.u32 + 0, ctx.r11.u8);

loc_831D97C8:
    ctx.r11.s64 = 0;
    cr6.compare<uint32_t>(ctx.r26.u32, 0, ctx.xer);
    PPC_STORE_U32(ctx.r29.u32 + 19724, ctx.r11.u32);
    PPC_STORE_U32(ctx.r31.u32 + 36, ctx.r28.u32);
    PPC_STORE_U32(ctx.r31.u32 + 40, ctx.r27.u32);
    PPC_STORE_U8(ctx.r31.u32 + 48, ctx.r11.u8);
    if (cr6.eq) goto loc_831D97E8;
    sub_8247BFD0(ctx, base);

loc_831D97E8:
    ctx.r1.s64 = ctx.r1.s64 + 144;
    __restgprlr_26(ctx, base);
}

PPC_FUNC(sub_8227B8A0)
{
    PPC_FUNC_PROLOGUE();
    PPCRegister ctr{};
    PPCCRRegister cr6{};
    uint32_t ea{};

    __savegprlr_29(ctx, base);
    ea = -112 + ctx.r1.u32;
    PPC_STORE_U32(ea, ctx.r1.u32);
    ctx.r1.u32 = ea;
    PPC_STORE_U64(ctx.r1.u32 + 144, ctx.r4.u64);
    ctx.r11.s64 = -2083848192; // 0x83CE0000
    ctx.r31.u64 = ctx.r3.u64;
    PPC_STORE_U64(ctx.r1.u32 + 152, ctx.r5.u64);
    ctx.r29.s64 = ctx.r11.s64 + -20632;
    ctx.r3.u64 = ctx.r29.u64;
    sub_823C04D8(ctx, base);
    ctx.r30.s64 = ctx.r31.s64 + 56;
    ctx.r4.s64 = ctx.r1.s64 + 144;
    ctx.r3.u64 = ctx.r30.u64;
    sub_8250E020(ctx, base);
    ctx.r11.u64 = ctx.r3.u32 & 0xFF;
    cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
    if (!cr6.eq) goto loc_8227B92C;

    {
        uint32_t r31_addr = ctx.r31.u32;
        uint32_t listener = (r31_addr >= 0x10000 && r31_addr < PPC_MEMORY_SIZE - 4) ? PPC_LOAD_U32(r31_addr + 4) : 0;
        ctx.r3.u64 = listener;
        ctx.r6.u64 = PPC_LOAD_U64(ctx.r1.u32 + 144);
        ctx.r4.u64 = PPC_LOAD_U64(ctx.r30.u32 + 0);
        ctx.r7.u64 = PPC_LOAD_U64(ctx.r1.u32 + 152);
        ctx.r5.u64 = PPC_LOAD_U64(ctx.r30.u32 + 8);

        uint32_t vtable = (listener >= 0x10000 && listener < PPC_MEMORY_SIZE - 4) ? PPC_LOAD_U32(listener + 0) : 0;
        uint32_t targetFn = (vtable >= 0x10000 && vtable < PPC_MEMORY_SIZE - 32) ? PPC_LOAD_U32(vtable + 28) : 0;

        ctr.u64 = targetFn;
        if (targetFn >= PPC_CODE_BASE && targetFn < (PPC_CODE_BASE + PPC_CODE_SIZE)) {
            PPC_CALL_INDIRECT_FUNC(ctr.u32);
        } else {
            static int s_sub8227B8A0NullCount = 0;
            if (s_sub8227B8A0NullCount++ < 5) {
                LOGF_WARNING("[sub_8227B8A0] No valid listener callback for obj 0x{:08X} (listener=0x{:08X}, vtable=0x{:08X}, fn=0x{:08X})",
                             r31_addr, listener, vtable, targetFn);
            }
        }

        ctx.r9.s64 = ctx.r1.s64 + 144;
        ctx.r8.u64 = PPC_LOAD_U32(ctx.r9.u32 + 0);
        ctx.r7.u64 = PPC_LOAD_U32(ctx.r9.u32 + 4);
        ctx.r6.u64 = PPC_LOAD_U32(ctx.r9.u32 + 8);
        ctx.r5.u64 = PPC_LOAD_U32(ctx.r9.u32 + 12);
        PPC_STORE_U32(ctx.r30.u32 + 0, ctx.r8.u32);
        PPC_STORE_U32(ctx.r30.u32 + 4, ctx.r7.u32);
        PPC_STORE_U32(ctx.r30.u32 + 8, ctx.r6.u32);
        PPC_STORE_U32(ctx.r30.u32 + 12, ctx.r5.u32);
    }

loc_8227B92C:
    ctx.r11.s64 = ctx.r1.s64 + 144;
    ctx.r3.u64 = ctx.r29.u64;
    ctx.r10.s64 = ctx.r31.s64 + 72;
    ctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);
    ctx.r8.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);
    ctx.r7.u64 = PPC_LOAD_U32(ctx.r11.u32 + 8);
    ctx.r6.u64 = PPC_LOAD_U32(ctx.r11.u32 + 12);
    PPC_STORE_U32(ctx.r31.u32 + 72, ctx.r9.u32);
    PPC_STORE_U32(ctx.r31.u32 + 76, ctx.r8.u32);
    PPC_STORE_U32(ctx.r31.u32 + 80, ctx.r7.u32);
    PPC_STORE_U32(ctx.r31.u32 + 84, ctx.r6.u32);
    sub_823C0578(ctx, base);
    ctx.r1.s64 = ctx.r1.s64 + 112;
    __restgprlr_29(ctx, base);
    return;
}

PPC_FUNC(sub_82985058)
{
    // Fatal dirty disc check - bypass on PC/Linux recompiled runtime
    static int s_dirtyDiscBypassCount = 0;
    if (s_dirtyDiscBypassCount++ < 5) {
        LOGF_INFO("[sub_82985058] Bypassing Xbox 360 dirty disc error check (call #{})", s_dirtyDiscBypassCount);
    }
    // Return cleanly without invoking sub_8363AAA0 / XamShowDirtyDiscErrorUI
    return;
}

PPC_FUNC(sub_822381C0)
{
    // Storage is not optical disc (returns 0 = false)
    // On PC/Linux recompiled runtime, we run from filesystem/NVMe, not optical DVD.
    // Returning 0 prevents RAGE from taking the dirty disc error branch (loc_8299E8E8).
    static int s_discCheckCount = 0;
    ++s_discCheckCount;
    LOGF_INFO("[sub_822381C0] IsOpticalDisc check -> returning 0 (call #{}, callerLR=0x{:08X})", s_discCheckCount, GET_CALLER_LR());
    ctx.r3.u64 = 0;
}

PPC_FUNC(sub_8363AAA0)
{
    // Fatal disc error UI / XamLoaderLaunchTitle(0, 0) bypass
    static int s_fatalDiscErrCount = 0;
    if (s_fatalDiscErrCount++ < 5) {
        LOGF_WARNING("[sub_8363AAA0] Bypassing fatal disc error / XamLoaderLaunchTitle(0, 0) (call #{})", s_fatalDiscErrCount);
    }
    return;
}

PPC_EXTERN_FUNC(sub_8255DCC0);

PPC_FUNC(sub_83216C58)
{
	PPC_FUNC_PROLOGUE();
	PPCRegister ctr{};
	PPCCRRegister cr6{};
	uint32_t ea{};
	PPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	PPC_STORE_U64(ctx.r1.u32 + -24, ctx.r30.u64);
	PPC_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	ea = -112 + ctx.r1.u32;
	PPC_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	ctx.r11.u64 = PPC_LOAD_U16(ctx.r3.u32 + 4);
	ctx.r31.u64 = ctx.r3.u64;
	cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	if (cr6.eq) goto loc_83216CC8;
	ctx.r11.s64 = 0;
	ctx.r30.u64 = ctx.r11.u64 | 65535;
loc_83216C84:
	ctx.r10.u64 = PPC_LOAD_U16(ctx.r31.u32 + 4);
	ctx.r4.s64 = 0;
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
	ctx.r9.u64 = ctx.r10.u64 + ctx.r30.u64;
	ctx.r8.u64 = ctx.r9.u32 & 0xFFFF;
	ctx.r7.u64 = __builtin_rotateleft64(ctx.r8.u32 | (ctx.r8.u64 << 32), 5) & 0x1FFFE0;
	PPC_STORE_U16(ctx.r31.u32 + 4, ctx.r8.u16);
	ctx.r6.u64 = ctx.r8.u32 & 0xFFFF;
	ctx.r10.u64 = __builtin_rotateleft32(ctx.r6.u32, 5);
	ctx.r5.u64 = PPC_LOAD_U32(ctx.r7.u32 + ctx.r11.u32);
	ctx.r3.u64 = ctx.r10.u64 + ctx.r11.u64;
	{
		uint32_t fn = 0;
		if (ctx.r5.u32 != 0 && ctx.r5.u32 < PPC_MEMORY_SIZE - 4) {
			fn = PPC_LOAD_U32(ctx.r5.u32 + 0);
		}
		ctx.r11.u64 = fn;
		ctr.u64 = fn;
		if (ctr.u32 >= PPC_CODE_BASE && ctr.u32 < (PPC_CODE_BASE + PPC_CODE_SIZE)) {
			PPCFunc* targetFunc = PPC_LOOKUP_FUNC(base, ctr.u32);
			if (targetFunc) {
				targetFunc(ctx, base);
			}
		} else {
			static int s_skipDestruct = 0;
			if (s_skipDestruct++ < 10) {
				LOGF_WARNING("[sub_83216C58] Skipping invalid/NULL destructor target 0x{:08X} (r5=0x{:08X}, elem=0x{:08X})",
				             ctr.u32, ctx.r5.u32, ctx.r3.u32);
			}
		}
	}
	ctx.r10.u64 = PPC_LOAD_U16(ctx.r31.u32 + 4);
	cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	if (!cr6.eq) goto loc_83216C84;
loc_83216CC8:
	ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
	sub_8255DCC0(ctx, base);
	ctx.r11.s64 = 0;
	PPC_STORE_U32(ctx.r31.u32 + 0, ctx.r11.u32);
	PPC_STORE_U16(ctx.r31.u32 + 6, ctx.r11.u16);
	ctx.r1.s64 = ctx.r1.s64 + 112;
	ctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);
	ctx.r30.u64 = PPC_LOAD_U64(ctx.r1.u32 + -24);
	ctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 + -16);
	return;
}

PPC_FUNC(sub_83216CF8)
{
	PPC_FUNC_PROLOGUE();
	PPCRegister ctr{};
	PPCCRRegister cr6{};
	uint32_t ea{};
	PPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	PPC_STORE_U64(ctx.r1.u32 + -24, ctx.r30.u64);
	PPC_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	ea = -112 + ctx.r1.u32;
	PPC_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	ctx.r11.u64 = PPC_LOAD_U16(ctx.r3.u32 + 4);
	ctx.r31.u64 = ctx.r3.u64;
	cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	if (cr6.eq) goto loc_83216D78;
	ctx.r11.s64 = 0;
	ctx.r30.u64 = ctx.r11.u64 | 65535;
loc_83216D24:
	ctx.r11.u64 = PPC_LOAD_U16(ctx.r31.u32 + 4);
	ctx.r4.s64 = 0;
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
	ctx.r9.u64 = ctx.r11.u64 + ctx.r30.u64;
	ctx.r11.u64 = ctx.r9.u32 & 0xFFFF;
	ctx.r8.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 1) & 0x1FFFE;
	PPC_STORE_U16(ctx.r31.u32 + 4, ctx.r11.u16);
	ctx.r9.u64 = ctx.r11.u32 & 0xFFFF;
	ctx.r8.u64 = ctx.r11.u64 + ctx.r8.u64;
	ctx.r11.u64 = __builtin_rotateleft32(ctx.r9.u32, 1);
	ctx.r7.u64 = __builtin_rotateleft64(ctx.r8.u32 | (ctx.r8.u64 << 32), 3) & 0xFFFFFFF8;
	ctx.r6.u64 = ctx.r9.u64 + ctx.r11.u64;
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 3) & 0xFFFFFFF8;
	ctx.r5.u64 = PPC_LOAD_U32(ctx.r7.u32 + ctx.r10.u32);
	ctx.r3.u64 = ctx.r11.u64 + ctx.r10.u64;
	{
		uint32_t fn = 0;
		if (ctx.r5.u32 != 0 && ctx.r5.u32 < PPC_MEMORY_SIZE - 4) {
			fn = PPC_LOAD_U32(ctx.r5.u32 + 0);
		}
		ctx.r11.u64 = fn;
		ctr.u64 = fn;
		if (ctr.u32 >= PPC_CODE_BASE && ctr.u32 < (PPC_CODE_BASE + PPC_CODE_SIZE)) {
			PPCFunc* targetFunc = PPC_LOOKUP_FUNC(base, ctr.u32);
			if (targetFunc) {
				targetFunc(ctx, base);
			}
		} else {
			static int s_skipDestruct2 = 0;
			if (s_skipDestruct2++ < 10) {
				LOGF_WARNING("[sub_83216CF8] Skipping invalid/NULL destructor target 0x{:08X} (r5=0x{:08X}, elem=0x{:08X})",
				             ctr.u32, ctx.r5.u32, ctx.r3.u32);
			}
		}
	}
	ctx.r10.u64 = PPC_LOAD_U16(ctx.r31.u32 + 4);
	cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	if (!cr6.eq) goto loc_83216D24;
loc_83216D78:
	ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
	sub_8255DCC0(ctx, base);
	ctx.r11.s64 = 0;
	PPC_STORE_U32(ctx.r31.u32 + 0, ctx.r11.u32);
	PPC_STORE_U16(ctx.r31.u32 + 6, ctx.r11.u16);
	ctx.r1.s64 = ctx.r1.s64 + 112;
	ctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);
	ctx.r30.u64 = PPC_LOAD_U64(ctx.r1.u32 + -24);
	ctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 + -16);
	return;
}

PPC_FUNC(sub_8321F758)
{
	PPC_FUNC_PROLOGUE();
	uint32_t r3 = ctx.r3.u32;
	if (r3 == 0 || r3 >= PPC_MEMORY_SIZE - 32) {
		return;
	}

	uint32_t saved_r28 = ctx.r28.u32;
	uint32_t saved_r29 = ctx.r29.u32;
	uint32_t saved_r30 = ctx.r30.u32;
	uint32_t saved_r31 = ctx.r31.u32;

	uint32_t r10 = PPC_LOAD_U32(r3 + 20);
	uint32_t r28 = r3;

	if (r10 != 0) {
		if (r10 >= 4 && r10 < PPC_MEMORY_SIZE) {
			uint32_t r11 = PPC_LOAD_U32(r10 - 4);
			uint32_t r29 = r10 - 4;
			if (r11 > 0 && r11 <= 65536) {
				uint64_t endOffset = (uint64_t)r10 + (uint64_t)r11 * 12;
				if (endOffset < PPC_MEMORY_SIZE) {
					uint32_t r30 = (uint32_t)endOffset + 8;
					for (int32_t r31 = (int32_t)r11 - 1; r31 >= 0; --r31) {
						r30 -= 12;
						if (r30 >= 4 && r30 < PPC_MEMORY_SIZE) {
							uint32_t tag = PPC_LOAD_U32(r30 - 4);
							if (tag == 2) {
								ctx.r3.u64 = PPC_LOAD_U32(r30);
								sub_8255DCC0(ctx, base);
							}
						}
					}
				} else {
					LOGF_WARNING("[sub_8321F758] Array buffer 0x{:08X} count {} overflows memory! Skipping element cleanup.", r10, r11);
				}
			} else if (r11 > 65536) {
				LOGF_WARNING("[sub_8321F758] Corrupted element count {} at 0x{:08X}! Skipping element cleanup.", r11, r10 - 4);
			}
			ctx.r3.u64 = r29;
			sub_8255DCC0(ctx, base);
		} else {
			LOGF_WARNING("[sub_8321F758] Invalid r10 pointer 0x{:08X} at r3+20!", r10);
		}
		PPC_STORE_U32(r28 + 20, 0);
	}

	uint32_t p24 = PPC_LOAD_U32(r28 + 24);
	if (p24 != 0) {
		ctx.r3.u64 = p24;
		sub_8255DCC0(ctx, base);
		PPC_STORE_U32(r28 + 24, 0);
	}

	ctx.r28.u64 = saved_r28;
	ctx.r29.u64 = saved_r29;
	ctx.r30.u64 = saved_r30;
	ctx.r31.u64 = saved_r31;
}

extern "C" void __imp__sub_831ED658(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_831ED658)
{
    const char* pathStr = "null";
    if (ctx.r4.u32 >= 0x10000 && ctx.r4.u32 < 0xF0000000) {
        pathStr = reinterpret_cast<const char*>(base + ctx.r4.u32);
    }
    LOGF_INFO("[sub_831ED658] Mounting packfile: r3=0x{:08X}, path='{}', r5={}, r6=0x{:X}", ctx.r3.u32, pathStr, ctx.r5.u32, ctx.r6.u32);
    __imp__sub_831ED658(ctx, base);
    LOGF_INFO("[sub_831ED658] Mount result: r3=0x{:08X} (success={})", ctx.r3.u32, (ctx.r3.u32 & 0xFF) != 0);
}

PPC_EXTERN_FUNC(sub_831EADA8);
PPC_EXTERN_FUNC(sub_831E2AD8);

PPC_FUNC(sub_831ECC48)
{
	PPC_FUNC_PROLOGUE();

	uint32_t saved_r28 = ctx.r28.u32;
	uint32_t saved_r29 = ctx.r29.u32;
	uint32_t saved_r30 = ctx.r30.u32;
	uint32_t saved_r31 = ctx.r31.u32;

	ctx.r30.u64 = ctx.r3.u64;
	ctx.r28.u64 = ctx.r4.u64;

	const char* pathStr = (ctx.r30.u32 >= 0x10000 && ctx.r30.u32 < PPC_MEMORY_SIZE)
	                      ? reinterpret_cast<const char*>(base + ctx.r30.u32) : "(null)";

	sub_831EADA8(ctx, base);
	ctx.r29.u64 = ctx.r3.u64;

	if (ctx.r29.u32 == 0 || ctx.r29.u32 >= PPC_MEMORY_SIZE - 4) {
		LOGF_WARNING("[sub_831ECC48] fiDevice not found for path '{}' (0x{:08X})", pathStr, ctx.r30.u32);
		goto loc_831ECCDC;
	}

	{
		uint32_t vtable = PPC_LOAD_U32(ctx.r29.u32 + 0);
		if (vtable < 0x10000 || vtable >= PPC_MEMORY_SIZE - 64) {
			LOGF_WARNING("[sub_831ECC48] fiDevice 0x{:08X} has invalid vtable 0x{:08X}", ctx.r29.u32, vtable);
			goto loc_831ECCDC;
		}

		uint32_t openFn = PPC_LOAD_U32(vtable + 4);
		if (openFn < PPC_CODE_BASE || openFn >= (PPC_CODE_BASE + PPC_CODE_SIZE)) {
			LOGF_WARNING("[sub_831ECC48] fiDevice 0x{:08X} (vtbl 0x{:08X}) openFn 0x{:08X} is not in code range! (path='{}')",
			             ctx.r29.u32, vtable, openFn, pathStr);
			goto loc_831ECCDC;
		}

		ctx.r5.u64 = ctx.r28.u64;
		ctx.r4.u64 = ctx.r30.u64;
		ctx.r3.u64 = ctx.r29.u64;
		PPCFunc* fn = PPC_LOOKUP_FUNC(base, openFn);
		if (!fn) {
			LOGF_WARNING("[sub_831ECC48] openFn 0x{:08X} lookup returned null! (path='{}')", openFn, pathStr);
			goto loc_831ECCDC;
		}
		fn(ctx, base);
	}

	ctx.r31.u64 = ctx.r3.u64;
	if (ctx.r31.s32 == -1) {
		goto loc_831ECCDC;
	}

	{
		uint32_t r11_base = 0x83BE0000 + 14060;
		uint32_t hookFn = (r11_base < PPC_MEMORY_SIZE) ? PPC_LOAD_U32(r11_base) : 0;
		if (hookFn != 0) {
			if (hookFn >= PPC_CODE_BASE && hookFn < (PPC_CODE_BASE + PPC_CODE_SIZE)) {
				ctx.r5.u64 = ctx.r3.u64;
				ctx.r4.u64 = ctx.r28.u64;
				ctx.r3.u64 = ctx.r30.u64;
				PPCFunc* fn = PPC_LOOKUP_FUNC(base, hookFn);
				if (fn) {
					fn(ctx, base);
				}
				if ((ctx.r3.u32 & 0xFF) == 0) {
					uint32_t vtable = PPC_LOAD_U32(ctx.r29.u32 + 0);
					uint32_t closeFn = (vtable >= 0x10000 && vtable < PPC_MEMORY_SIZE - 64) ? PPC_LOAD_U32(vtable + 48) : 0;
					if (closeFn >= PPC_CODE_BASE && closeFn < (PPC_CODE_BASE + PPC_CODE_SIZE)) {
						ctx.r4.u64 = ctx.r31.u64;
						ctx.r3.u64 = ctx.r29.u64;
						PPCFunc* cfn = PPC_LOOKUP_FUNC(base, closeFn);
						if (cfn) cfn(ctx, base);
					}
					goto loc_831ECCDC;
				}
			}
		}
	}

	ctx.r5.u64 = ctx.r29.u64;
	ctx.r4.u64 = ctx.r31.u64;
	ctx.r3.u64 = ctx.r30.u64;
	sub_831E2AD8(ctx, base);
	goto loc_831ECD00;

loc_831ECCDC:
	ctx.r3.s64 = 0;

loc_831ECD00:
	ctx.r28.u64 = saved_r28;
	ctx.r29.u64 = saved_r29;
	ctx.r30.u64 = saved_r30;
	ctx.r31.u64 = saved_r31;
}

extern "C" void __imp__sub_8314C3C8(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_8314C3C8)
{
    uint32_t obj = ctx.r3.u32;
    if (obj != 0 && obj < PPC_MEMORY_SIZE - 64) {
        uint32_t vtbl = PPC_LOAD_U32(obj);
        if (vtbl != 0 && vtbl < PPC_MEMORY_SIZE - 64) {
            uint32_t fn1 = PPC_LOAD_U32(vtbl + 4);
            if (fn1 == 0 || fn1 < PPC_CODE_BASE || fn1 >= (PPC_CODE_BASE + PPC_CODE_SIZE)) {
                // Method 1 is unmapped/NULL: point to stub returning 0 (sub_8255DCE0)
                PPC_STORE_U32(vtbl + 4, 0x8255DCE0);
            }
        }
    }
    __imp__sub_8314C3C8(ctx, base);
}

extern "C" void __imp__sub_831B9330(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_831B9330)
{
    uint32_t node = ctx.r3.u32;
    if (node < 0x10000 || node >= PPC_MEMORY_SIZE - 48)
        return;

    // Clear flags in node + 14
    uint16_t val14 = PPC_LOAD_U16(node + 14);
    val14 &= 0xFFF7;
    PPC_STORE_U16(node + 14, val14);

    // Free node + 28
    uint32_t ptr28 = PPC_LOAD_U32(node + 28);
    if (ptr28 >= 0x10000) {
        ctx.r3.u64 = ptr28;
        sub_8255DCC0(ctx, base);
    }
    PPC_STORE_U32(node + 28, 0);
    PPC_STORE_U32(node + 32, 0);
    PPC_STORE_U32(node + 36, 0);

    // Loop through children at node + 24
    uint32_t child = PPC_LOAD_U32(node + 24);
    int childCount = 0;
    while (child >= 0x10000 && child < PPC_MEMORY_SIZE - 48 && childCount++ < 10000)
    {
        uint32_t next = PPC_LOAD_U32(child + 20);
        // Advance parent's child pointer immediately to prevent any re-entrant loop
        PPC_STORE_U32(node + 24, next);

        // Unlink child from parent
        PPC_STORE_U32(child + 16, 0);
        PPC_STORE_U32(child + 20, 0);

        // Destroy child
        ctx.r3.u64 = child;
        sub_831D0C30(ctx, base);

        // Free child memory
        ctx.r3.u64 = child;
        sub_8255DCB8(ctx, base);

        child = next;
    }
    PPC_STORE_U32(node + 24, 0);
}

extern "C" void __imp__sub_831D0C30(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_831D0C30)
{
    uint32_t node = ctx.r3.u32;
    if (node < 0x10000 || node >= PPC_MEMORY_SIZE - 48)
        return;

    uint32_t parent = PPC_LOAD_U32(node + 16);
    if (parent >= 0x10000 && parent < PPC_MEMORY_SIZE - 32)
    {
        uint32_t firstChild = PPC_LOAD_U32(parent + 24);
        if (firstChild == node) {
            uint32_t next = PPC_LOAD_U32(node + 20);
            PPC_STORE_U32(parent + 24, next);
        } else {
            uint32_t curr = firstChild;
            int walkLimit = 0;
            while (curr >= 0x10000 && curr < PPC_MEMORY_SIZE - 32 && walkLimit++ < 5000) {
                uint32_t next = PPC_LOAD_U32(curr + 20);
                if (next == node) {
                    uint32_t nodeNext = PPC_LOAD_U32(node + 20);
                    PPC_STORE_U32(curr + 20, nodeNext);
                    break;
                }
                curr = next;
            }
        }
    }
    PPC_STORE_U32(node + 16, 0);
    PPC_STORE_U32(node + 20, 0);

    // Destroy children
    ctx.r3.u64 = node;
    sub_831B9330(ctx, base);

    // Free node + 28 if node + 36 != 0
    uint32_t val36 = PPC_LOAD_U32(node + 36);
    if (val36 != 0) {
        uint32_t ptr28 = PPC_LOAD_U32(node + 28);
        if (ptr28 >= 0x10000) {
            ctx.r3.u64 = ptr28;
            sub_8255DCC0(ctx, base);
        }
    }

    // Free node + 0 if flag in node + 14 is set
    uint16_t val14 = PPC_LOAD_U16(node + 14);
    if (((val14 >> 1) & 1) != 0) {
        uint32_t ptr0 = PPC_LOAD_U32(node + 0);
        if (ptr0 >= 0x10000) {
            ctx.r3.u64 = ptr0;
            sub_8255DCC0(ctx, base);
        }
    }

    // Destroy attribute list at node + 4
    ctx.r3.u64 = node + 4;
    sub_831C59C8(ctx, base);

    // Free node + 4 array if node + 10 != 0
    uint16_t val10 = PPC_LOAD_U16(node + 10);
    if (val10 != 0) {
        uint32_t ptr4 = PPC_LOAD_U32(node + 4);
        if (ptr4 >= 0x10000) {
            ctx.r3.u64 = ptr4;
            sub_8255DCC0(ctx, base);
        }
    }
}

extern "C" void __imp__sub_8255DD00(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_8255DD00)
{
    uint64_t frame = ++g_mainFrameCounter;
    if (frame <= 10 || frame % 100 == 0) {
        LOGF_INFO("[MAIN_FRAME] sub_8255DD00 Frame #{} executing", frame);
    }
    __imp__sub_8255DD00(ctx, base);
}

PPC_FUNC(sub_82232F00)
{
    ctx.r1.u32 -= 96;

    LOGF_INFO("[MAIN_LOOP] sub_82232F00 initializing via sub_831978C0...");
    sub_831978C0(ctx, base);
    uint32_t initRet = ctx.r3.u32 & 0xFF;
    LOGF_INFO("[MAIN_LOOP] sub_831978C0 init result: r3={}", initRet);

    if (initRet != 0) {
        uint64_t iter = 0;
        while (true) {
            iter++;
            uint32_t fn = PPC_LOAD_U32(0x8383434C);
            if (iter <= 10 || iter % 100 == 0) {
                LOGF_INFO("[MAIN_LOOP] Loop iter #{} starting: fn=0x{:08X}", iter, fn);
            }
            if (fn < PPC_CODE_BASE || fn >= (PPC_CODE_BASE + PPC_CODE_SIZE)) {
                LOGF_WARNING("[MAIN_LOOP] Loop iter #{}: fn 0x{:08X} invalid/out-of-range! Defaulting to sub_8255DD00", iter, fn);
                fn = 0x8255DD00;
                PPC_STORE_U32(0x8383434C, fn);
            }
            
            PPCFunc* targetFunc = PPC_LOOKUP_FUNC(base, fn);
            if (!targetFunc) {
                LOGF_ERROR("[MAIN_LOOP] Target func for 0x{:08X} is NULL! Calling sub_8255DD00 directly.", fn);
                sub_8255DD00(ctx, base);
            } else {
                targetFunc(ctx, base);
            }

            uint32_t ret = ctx.r3.u32 & 0xFF;
            if (iter <= 10 || iter % 100 == 0) {
                LOGF_INFO("[MAIN_LOOP] Loop iter #{} done: ret(r3)={}, next fn=0x{:08X}",
                          iter, ret, PPC_LOAD_U32(0x8383434C));
            }
            if (ret == 0) {
                LOGF_INFO("[MAIN_LOOP] Game loop exit requested (ret=0) at iter #{}", iter);
                break;
            }
        }
    }

    ctx.r1.u32 += 96;
}

extern "C" void __imp__sub_8344D068(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_8344D068)
{
    uint32_t r3_in = ctx.r3.u32;
    if (r3_in == 0) {
        LOGF_WARNING("[sub_8344D068] NULL object pointer (r3=0)! Returning 0 safely.");
        ctx.r3.u64 = 0;
        return;
    }
    uint32_t r4_in = ctx.r4.u32;
    const char* str4 = (r4_in >= 0x10000 && r4_in < PPC_MEMORY_SIZE) ? reinterpret_cast<const char*>(base + r4_in) : "(null)";
    uint32_t vtbl = (r3_in != 0 && r3_in < PPC_MEMORY_SIZE) ? PPC_LOAD_U32(r3_in) : 0;
    uint32_t fn0 = (vtbl != 0 && vtbl < PPC_MEMORY_SIZE) ? PPC_LOAD_U32(vtbl + 0) : 0;
    uint32_t fn1 = (vtbl != 0 && vtbl < PPC_MEMORY_SIZE) ? PPC_LOAD_U32(vtbl + 4) : 0;
    LOGF_INFO("[sub_8344D068] ENTER: r3=0x{:08X} (vtbl=0x{:08X}, fn0=0x{:08X}, fn1=0x{:08X}), r4=0x{:08X} ('{}')",
              r3_in, vtbl, fn0, fn1, r4_in, str4);
    __imp__sub_8344D068(ctx, base);
    LOGF_INFO("[sub_8344D068] EXIT: r3=0x{:08X}", ctx.r3.u32);
}

extern "C" void __imp__sub_82C10DC8(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82C10DC8)
{
    static thread_local int s_recurseDepth = 0;
    s_recurseDepth++;
    if (s_recurseDepth <= 5) {
        uint32_t vtable = 0;
        uint32_t slot14 = 0;
        if (ctx.r3.u32 != 0 && ctx.r3.u32 < PPC_MEMORY_SIZE - 64) {
            vtable = PPC_LOAD_U32(ctx.r3.u32);
            if (vtable != 0 && vtable < PPC_MEMORY_SIZE - 64) {
                slot14 = PPC_LOAD_U32(vtable + 56);
            }
        }
        LOGF_INFO("[sub_82C10DC8] depth={} r3=0x{:08X} vtable=0x{:08X} slot14=0x{:08X} r12=0x{:08X}",
                  s_recurseDepth, ctx.r3.u32, vtable, slot14, ctx.r12.u32);
        void* bt[16];
        int n = backtrace(bt, 16);
        backtrace_symbols_fd(bt, n, 2);
    }
    if (s_recurseDepth > 8) {
        LOGF_ERROR("[sub_82C10DC8] RECURSION GUARD TRIGGERED at depth={}! Aborting recursion safely.", s_recurseDepth);
        s_recurseDepth--;
        ctx.r3.u64 = 0;
        return;
    }
    __imp__sub_82C10DC8(ctx, base);
    s_recurseDepth--;
}

PPC_FUNC(sub_83767B20)
{
    PPC_FUNC_PROLOGUE();
    uint32_t r3_in = ctx.r3.u32;
    int32_t r4_in = ctx.r4.s32;

    uint32_t r31_thread = r3_in;
    if (r31_thread == 0)
    {
        uint32_t tlsSlot = PPC_LOAD_U32(0x83CE0000 + 30208);
        if (tlsSlot != (uint32_t)-1)
        {
            ctx.r3.u64 = tlsSlot;
            __imp__KeTlsGetValue(ctx, base);
            r31_thread = ctx.r3.u32;
        }
    }

    if (r31_thread == 0 || r31_thread >= PPC_MEMORY_SIZE - 256)
    {
        return;
    }

    uint32_t threadObj = PPC_LOAD_U32(r31_thread + 252);
    if (threadObj == 0 || threadObj >= PPC_MEMORY_SIZE - 60)
    {
        return;
    }

    // Switch table at 0x83767B84: handles priority enum
    int32_t priority = r4_in;
    if (r4_in < -2 || r4_in > 2)
    {
        priority = 0;
    }

    ctx.r3.u64 = PPC_LOAD_U32(threadObj + 52);
    ctx.r4.s64 = priority;
    sub_82283BD8(ctx, base);
}

PPC_EXTERN_FUNC(sub_83210CF0);
PPC_EXTERN_FUNC(sub_828FC6D8);
PPC_EXTERN_FUNC(sub_828FCA78);

// =============================================================================
// RAGE Global Pool Descriptor Resolver (sub_8323C238)
// =============================================================================
// In default.xex, sub_8323C238 indexes a 41-entry switch table at 0x8323C25C.
// Because the table was disassembled as PPC code (due to starting with 0x82/0x83),
// calling it directly resulted in switch table failure and missing pool configurations.
PPC_FUNC(sub_8323C238)
{
    static const uint32_t s_poolFlags[41] = {
        0xFFFFFFFF,  // [ 0]
        0x1A200152,  // [ 1]
        0x1A200153,  // [ 2]
        0x1A200154,  // [ 3]
        0x1A20017C,  // [ 4]
        0x1A20017A,  // [ 5]
        0x1A20017D,  // [ 6]
        0x1A20017B,  // [ 7]
        0x1A200171,  // [ 8]
        0x18280186,  // [ 9]
        0x1A200000,  // [10]
        0x04900102,  // [11]
        0x28000102,  // [12]
        0x0800014A,  // [13]
        0x1828014F,  // [14]
        0x18280143,  // [15]
        0x28280144,  // [16]
        0xFFFFFFFF,  // [17]
        0xFFFFFFFF,  // [18]
        0xFFFFFFFF,  // [19]
        0x182801B6,  // [20]
        0x1A2001B6,  // [21]
        0x1A20015A,  // [22]
        0x2D200199,  // [23]
        0x28000158,  // [24]
        0x1A22AB60,  // [25]
        0x2D22AB9F,  // [26]
        0x2DA2AB5E,  // [27]
        0x1A22ABA6,  // [28]
        0x2D22ABA5,  // [29]
        0x2DA2ABA4,  // [30]
        0xFFFFFFFF,  // [31]
        0x2D200196,  // [32]
        0x1A220197,  // [33]
        0xFFFFFFFF,  // [34]
        0xFFFFFFFF,  // [35]
        0xFFFFFFFF,  // [36]
        0x1A22AAA6,  // [37]
        0x18280086,  // [38]
        0x28000002,  // [39]
        0x18280186   // [40]
    };

    uint32_t index = ctx.r3.u32;
    uint32_t flag = ctx.r4.u32 & 0xFF;

    if (index > 40)
    {
        ctx.r3.s64 = -1;
        return;
    }

    uint32_t val = s_poolFlags[index];
    if (val != 0xFFFFFFFF && flag != 0)
    {
        val &= ~0x00000100;
    }

    if (index <= 10 || val == 0xFFFFFFFF)
    {
        LOGF_INFO("[sub_8323C238] pool[{}] flag={} -> 0x{:08X}", index, flag, val);
    }

    ctx.r3.s64 = (int32_t)val;
}

PPC_FUNC(sub_82918D28)
{
    PPC_FUNC_PROLOGUE();
    sub_828FC6D8(ctx, base);
    sub_828FCA78(ctx, base);
}



// =============================================================================
// GTA V Language selector & Audio/Channel state manager
// =============================================================================
void sub_83639E20(PPCContext& ctx, uint8_t* base);
void sub_83639D48(PPCContext& ctx, uint8_t* base);
void sub_8299D170(PPCContext& ctx, uint8_t* base);
void sub_8245BE60(PPCContext& ctx, uint8_t* base);
void sub_82988A10(PPCContext& ctx, uint8_t* base);
void sub_8259C478(PPCContext& ctx, uint8_t* base);
void sub_824E4C48(PPCContext& ctx, uint8_t* base);
void sub_82995428(PPCContext& ctx, uint8_t* base);
void sub_82984778(PPCContext& ctx, uint8_t* base);
void sub_8274A2D0(PPCContext& ctx, uint8_t* base);
void sub_82688538(PPCContext& ctx, uint8_t* base);
void sub_8273DEB8(PPCContext& ctx, uint8_t* base);
void sub_82453808(PPCContext& ctx, uint8_t* base);
void sub_825A89E8(PPCContext& ctx, uint8_t* base);
void sub_825C2C30(PPCContext& ctx, uint8_t* base);
void sub_825BD7C0(PPCContext& ctx, uint8_t* base);

PPC_FUNC(sub_8257A2A8)
{
    PPC_FUNC_PROLOGUE();
    // Resolves game language: 0 = English
    ctx.r3.u64 = 0;
}

PPC_FUNC(sub_825C9768)
{
    PPC_FUNC_PROLOGUE();
    uint32_t caseId = ctx.r3.u32;
    uint32_t r4 = ctx.r4.u32;
    if (caseId > 27) return;

    switch (caseId)
    {
    case 0:
    {
        sub_8299D170(ctx, base);
        ctx.r3.s64 = 0;
        sub_8245BE60(ctx, base);
        ctx.r4.s64 = 250;
        ctx.r5.s64 = (int32_t)-2084569088 + 3780;
        sub_82988A10(ctx, base);
        uint32_t r11 = PPC_LOAD_U32((uint32_t)-2087387136 - 18256);
        if (!r11) return;
        r11 = PPC_LOAD_U32(r11 + 4);
        if (!r11) return;
        r11 = PPC_LOAD_U32(r11 + 2912);
        if (!r11)
        {
            ctx.r3.s64 = 1827667968 | 14640;
            ctx.r4.u64 = PPC_LOAD_U32((uint32_t)-2088173568 + 23080);
            sub_8259C478(ctx, base);
        }
        else
        {
            uint32_t r11_264 = PPC_LOAD_U32(r11 + 264);
            uint32_t r10 = PPC_LOAD_U32(r11_264 + 3444);
            bool b1 = (r10 >> 30) & 1;
            bool cond1 = false;
            if (b1) {
                cond1 = (PPC_LOAD_U32(r11_264 + 3620) != 0);
            } else {
                cond1 = (PPC_LOAD_U32(r11_264 + 3624) != 0);
            }
            if (cond1) {
                ctx.r3.s64 = 1827667968 | 14640;
                ctx.r4.u64 = PPC_LOAD_U32((uint32_t)-2088173568 + 23080);
                sub_8259C478(ctx, base);
            } else {
                uint32_t r10_2 = PPC_LOAD_U32(r11_264 + 3444);
                bool b2 = (r10_2 >> 30) & 1;
                bool cond2 = false;
                if (b2) {
                    cond2 = (PPC_LOAD_U32(r11_264 + 3620) == 0);
                }
                if (cond2) {
                    sub_824E4C48(ctx, base);
                    if ((ctx.r3.u32 & 0xFF) == 0) {
                        ctx.r3.s64 = 77398016 | 6776;
                        ctx.r4.u64 = PPC_LOAD_U32((uint32_t)-2088173568 + 23080);
                        sub_8259C478(ctx, base);
                    } else {
                        sub_824E4C48(ctx, base);
                        if ((ctx.r3.u32 & 0xFF) != 0) {
                            ctx.r3.s64 = 855244800 | 62851;
                            ctx.r4.u64 = PPC_LOAD_U32((uint32_t)-2088173568 + 23080);
                            sub_8259C478(ctx, base);
                        }
                    }
                } else {
                    sub_824E4C48(ctx, base);
                    if ((ctx.r3.u32 & 0xFF) != 0) {
                        ctx.r3.s64 = 855244800 | 62851;
                        ctx.r4.u64 = PPC_LOAD_U32((uint32_t)-2088173568 + 23080);
                        sub_8259C478(ctx, base);
                    }
                }
            }
        }
        if (PPC_LOAD_U8((uint32_t)-2086731776 - 28650) != 0)
        {
            ctx.r3.s64 = (int32_t)-1287716864 | 23532;
            ctx.r4.u64 = PPC_LOAD_U32((uint32_t)-2088173568 + 23080);
            sub_8259C478(ctx, base);
        }
        break;
    }
    case 1:
    {
        if (r4 != 1) return;
        ctx.r3.s64 = 300;
        ctx.r5.s64 = 0;
        ctx.r6.s64 = 0;
        uint32_t f_u32 = PPC_LOAD_U32((uint32_t)-2113470464 + 9436);
        float f_val;
        memcpy(&f_val, &f_u32, sizeof(float));
        ctx.f1.f64 = (double)f_val;
        sub_82995428(ctx, base);
        uint32_t r11 = PPC_LOAD_U32((uint32_t)-2084175872 - 11968) + 800;
        PPC_STORE_U32((uint32_t)-2088173568 + 23752, r11);
        break;
    }
    case 3:
    {
        sub_82984778(ctx, base);
        break;
    }
    case 7:
    {
        uint32_t val = PPC_LOAD_U32((uint32_t)-2088173568 + 23080 + 28);
        PPC_STORE_U32((uint32_t)-2088173568 - 17920, val);
        break;
    }
    case 10:
    {
        if (r4 != 1 && r4 != 3) return;
        uint32_t r3_val = PPC_LOAD_U32((uint32_t)-2088173568 + 23080 + 40);
        if (r3_val == 0xFFFFFFFF)
        {
            ctx.r3.s64 = (int32_t)-2087911424 - 22296;
            sub_8274A2D0(ctx, base);
        }
        else
        {
            sub_82688538(ctx, base);
            ctx.r4.u64 = ctx.r3.u64;
            ctx.r3.s64 = (int32_t)-2087911424 - 22296;
            sub_8273DEB8(ctx, base);
        }
        break;
    }
    case 12:
    {
        uint32_t f0_u = PPC_LOAD_U32((uint32_t)-2113929216 + 3368);
        uint32_t f13_u = PPC_LOAD_U32((uint32_t)-2113929216 + 3780);
        uint32_t f12_u = PPC_LOAD_U32((uint32_t)-2113929216 + 4072);
        uint32_t f11_u = PPC_LOAD_U32((uint32_t)-2113470464 + 23776);
        float f0, f13, f12, f11;
        memcpy(&f0, &f0_u, 4);
        memcpy(&f13, &f13_u, 4);
        memcpy(&f12, &f12_u, 4);
        memcpy(&f11, &f11_u, 4);
        int32_t r11 = (int32_t)PPC_LOAD_U32((uint32_t)-2088173568 + 23080 + 48);
        uint8_t r9 = PPC_LOAD_U8((uint32_t)-2085552128 + 15083);
        double f10 = (double)r11;
        float f9 = (float)f10;
        float f8 = f9 * f0;
        float f6 = (-f8 >= 0.0f) ? f13 : f8;
        float res = f6 * f12 + f11;
        uint32_t res_u;
        memcpy(&res_u, &res, 4);
        PPC_STORE_U32((uint32_t)-2087387136 + 20372, res_u);
        if (r9 != 0 && r4 == 3)
        {
            sub_82453808(ctx, base);
        }
        break;
    }
    case 27:
    {
        if (r4 == 0) return;
        if (r4 == 1)
        {
            uint32_t r11 = PPC_LOAD_U32((uint32_t)-2084175872 - 11968) + 2000;
            PPC_STORE_U32((uint32_t)-2088173568 + 23892, r11);
        }
        ctx.r4.s64 = 212;
        ctx.r3.u64 = PPC_LOAD_U32((uint32_t)-2088173568 + 23808);
        uint32_t r11_val = PPC_LOAD_U32((uint32_t)-2088173568 + 23080 + 108);
        ctx.r5.s64 = 10 - (int64_t)r11_val;
        sub_825A89E8(ctx, base);
        sub_825C2C30(ctx, base);
        ctx.r4.s64 = 2;
        ctx.r3.u64 = PPC_LOAD_U32((uint32_t)-2088108032 - 28328 + 8);
        sub_825BD7C0(ctx, base);
        uint32_t r11_addr = PPC_LOAD_U32((uint32_t)-2088173568 + 23856);
        PPC_STORE_U8(r11_addr + 168, 1);
        break;
    }
    default:
        break;
    }
}

// =============================================================================
// Xbox 360 D3D / GPU notification packet processor
// =============================================================================
void sub_836BAB18(PPCContext& ctx, uint8_t* base);
void sub_8255DD48(PPCContext& ctx, uint8_t* base);
void sub_836B81B8(PPCContext& ctx, uint8_t* base);
void sub_836B7DE8(PPCContext& ctx, uint8_t* base);
void sub_836B61C8(PPCContext& ctx, uint8_t* base);
void sub_836B6210(PPCContext& ctx, uint8_t* base);

PPC_FUNC(sub_836B2AB0)
{
    PPC_FUNC_PROLOGUE();
    uint32_t r31_in = ctx.r3.u32;
    if (!r31_in) return;

    ctx.r31.u64 = r31_in;
    __imp__KeGetCurrentProcessType(ctx, base);
    uint32_t r11_ptr = (ctx.r3.s32 == 2) ? 
        PPC_LOAD_U32((uint32_t)-2113929216 + 2156) : 
        PPC_LOAD_U32((uint32_t)-2113929216 + 2152);

    ctx.r3.u64 = PPC_LOAD_U32(r11_ptr);
    uint32_t r11_val = PPC_LOAD_U32(ctx.r31.u32);
    uint32_t r10 = (r11_val & 0xF) - 1;
    if (r10 > 11) goto loc_836B2D34;

    {
        uint32_t byteTableAddr = (uint32_t)-2112880640 + 29272; // 0x82107258
        uint32_t r0 = PPC_LOAD_U8(byteTableAddr + r10);
        uint32_t target = 0x836B2B28 + (r0 << 2);

        switch (target)
        {
        case 0x836B2B28: goto case_0x836B2B28;
        case 0x836B2B6C: goto case_0x836B2B6C;
        case 0x836B2BAC: goto case_0x836B2BAC;
        case 0x836B2BB8: goto case_0x836B2BB8;
        case 0x836B2BE8: goto case_0x836B2BE8;
        case 0x836B2C14: goto case_0x836B2C14;
        case 0x836B2C44: goto case_0x836B2C44;
        case 0x836B2C74: goto case_0x836B2C74;
        case 0x836B2CA4: goto case_0x836B2CA4;
        case 0x836B2CD4: goto case_0x836B2CD4;
        case 0x836B2D04: goto case_0x836B2D04;
        default: goto loc_836B2D34;
        }
    }

case_0x836B2B28:
    if ((r11_val & 0x40000000) != 0) goto loc_836B2D34;
    if ((r11_val & 0x80000000) != 0)
    {
        uint32_t r11_44 = PPC_LOAD_U32(ctx.r31.u32 + 44);
        uint32_t r9_28 = PPC_LOAD_U32(ctx.r31.u32 + 28);
        ctx.r4.u32 = r11_44 / 5120;
        ctx.r3.u64 = r9_28 & 0xFFF;
        sub_836BAB18(ctx, base);
    }
    if ((PPC_LOAD_U32(ctx.r31.u32 + 32) & 0x1) != 0)
    {
        PPC_STORE_U32((uint32_t)-2083717120 + 10928, 0);
    }
    goto loc_836B2D34;

case_0x836B2B6C:
    ctx.r4.u64 = PPC_LOAD_U32(ctx.r31.u32 + 8);
    if (ctx.r4.u32 != 0 && ctx.r3.u32 != 0)
    {
        ctx.r7.s64 = 0;
        ctx.r6.u64 = ctx.r31.u64;
        ctx.r5.s64 = 13;
        sub_8250D988(ctx, base);
    }
    {
        uint32_t r11_32 = PPC_LOAD_U32(ctx.r31.u32 + 32);
        uint32_t r10_48 = PPC_LOAD_U32(ctx.r31.u32 + 48);
        ctx.r3.u64 = r11_32 & 0xFFFFF000;
        ctx.r30.u64 = r10_48 & 0xFFFFF000;
        ctx.r4.s64 = -1317011456;
        sub_8255DD48(ctx, base);
        ctx.r3.u64 = ctx.r30.u64;
        goto case_0x836B2BAC;
    }

case_0x836B2BAC:
    ctx.r4.s64 = -1317011456;
    sub_8255DD48(ctx, base);
    goto loc_836B2D34;

case_0x836B2BB8:
    ctx.r4.u64 = PPC_LOAD_U32(ctx.r31.u32 + 8);
    if (ctx.r4.u32 != 0 && ctx.r3.u32 != 0)
    {
        ctx.r7.s64 = 0;
        ctx.r6.u64 = ctx.r31.u64;
        ctx.r5.s64 = 9;
        sub_8250D988(ctx, base);
    }
    ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 24) & 0xFFFFFFFC;
    goto case_0x836B2BAC;

case_0x836B2BE8:
    ctx.r4.u64 = PPC_LOAD_U32(ctx.r31.u32 + 8);
    if (ctx.r4.u32 != 0 && ctx.r3.u32 != 0)
    {
        ctx.r5.s64 = 11;
        ctx.r7.s64 = 0;
        ctx.r6.u64 = ctx.r31.u64;
        sub_8250D988(ctx, base);
    }
    ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 24);
    goto case_0x836B2BAC;

case_0x836B2C14:
    ctx.r4.u64 = PPC_LOAD_U32(ctx.r31.u32 + 8);
    if (ctx.r4.u32 != 0 && ctx.r3.u32 != 0)
    {
        ctx.r7.s64 = 0;
        ctx.r6.u64 = ctx.r31.u64;
        ctx.r5.s64 = 7;
        sub_8250D988(ctx, base);
    }
    ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 32);
    goto case_0x836B2BAC;

case_0x836B2C44:
    ctx.r4.u64 = PPC_LOAD_U32(ctx.r31.u32 + 8);
    if (ctx.r4.u32 != 0 && ctx.r3.u32 != 0)
    {
        ctx.r5.s64 = 8;
        ctx.r7.s64 = 0;
        ctx.r6.u64 = ctx.r31.u64;
        sub_8250D988(ctx, base);
    }
    ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 24);
    goto case_0x836B2BAC;

case_0x836B2C74:
    ctx.r4.u64 = PPC_LOAD_U32(ctx.r31.u32 + 8);
    if (ctx.r4.u32 != 0 && ctx.r3.u32 != 0)
    {
        ctx.r5.s64 = 17;
        ctx.r7.s64 = 0;
        ctx.r6.u64 = ctx.r31.u64;
        sub_8250D988(ctx, base);
    }
    ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 24);
    goto case_0x836B2BAC;

case_0x836B2CA4:
    ctx.r4.u64 = PPC_LOAD_U32(ctx.r31.u32 + 8);
    if (ctx.r4.u32 != 0 && ctx.r3.u32 != 0)
    {
        ctx.r7.s64 = 0;
        ctx.r6.u64 = ctx.r31.u64;
        ctx.r5.s64 = 15;
        sub_8250D988(ctx, base);
    }
    ctx.r3.u64 = ctx.r31.u64;
    sub_836B81B8(ctx, base);
    goto loc_836B2D34;

case_0x836B2CD4:
    ctx.r4.u64 = PPC_LOAD_U32(ctx.r31.u32 + 8);
    if (ctx.r4.u32 != 0 && ctx.r3.u32 != 0)
    {
        ctx.r7.s64 = 0;
        ctx.r6.u64 = ctx.r31.u64;
        ctx.r5.s64 = 21;
        sub_8250D988(ctx, base);
    }
    ctx.r3.u64 = ctx.r31.u64;
    sub_836B7DE8(ctx, base);
    goto loc_836B2D34;

case_0x836B2D04:
    ctx.r4.u64 = PPC_LOAD_U32(ctx.r31.u32 + 8);
    if (ctx.r4.u32 != 0 && ctx.r3.u32 != 0)
    {
        ctx.r7.s64 = 0;
        ctx.r6.u64 = ctx.r31.u64;
        ctx.r5.s64 = 23;
        sub_8250D988(ctx, base);
    }
    ctx.r3.u64 = ctx.r31.u64;
    sub_836B61C8(ctx, base);
    goto loc_836B2D34;

loc_836B2D34:
    ctx.r4.s64 = 612368384;
    ctx.r3.u64 = ctx.r31.u64;
    sub_8255DD48(ctx, base);
}

// =============================================================================
// GTA V Format / Semantic string and Texture Pitch helpers
// =============================================================================
PPC_FUNC(sub_82332868)
{
    PPC_FUNC_PROLOGUE();
    uint32_t id = ctx.r3.u32;
    static const uint32_t kPtrs[] = {
        0x820CBCCC, 0x82000C48, 0x82000CA8, 0x82000B4C, 0x82000C6C, 
        0x820CBCC8, 0x820CBCC4, 0x820CBCC0, 0x820CBCBC, 0x820CBCB8, 
        0x820CBCB4, 0x82000BEC, 0x82000CA4, 0x82020B2C, 0x820013C8, 
        0x820CBCAC, 0x820C66E8, 0x820CBCA8, 0x820CBCA4, 0x8202F438, 
        0x820AE27C, 0x82018990, 0x82045A2C, 0x820CBCA0, 0x820CBC9C, 
        0x820CBC98, 0x820CBC94, 0x820CBC90, 0x820010C4
    };
    if (id < sizeof(kPtrs) / sizeof(kPtrs[0]))
    {
        ctx.r3.u64 = kPtrs[id];
    }
    else
    {
        ctx.r3.u64 = 0;
    }
}

PPC_FUNC(sub_83217D38)
{
    PPC_FUNC_PROLOGUE();
    uint32_t obj = ctx.r3.u32;
    if (!obj) return;

    // Exact bits per pixel / element from switch table at 0x83217D60
    static const uint32_t kFormatBpp[41] = {
         0,  // [ 0] -> sub_83217E34 (0 bpp)
         4,  // [ 1] -> 0x7E04 (4 bpp)
         8,  // [ 2] -> 0x7E0C (8 bpp)
         8,  // [ 3] -> 0x7E0C (8 bpp)
         4,  // [ 4] -> 0x7E04 (4 bpp)
         4,  // [ 5] -> 0x7E04 (4 bpp)
         4,  // [ 6] -> 0x7E04 (4 bpp)
         4,  // [ 7] -> 0x7E04 (4 bpp)
         8,  // [ 8] -> 0x7E0C (8 bpp)
        32,  // [ 9] -> 0x7E14 (32 bpp)
        32,  // [10] -> 0x7E14 (32 bpp)
         8,  // [11] -> 0x7E0C (8 bpp)
         8,  // [12] -> 0x7E0C (8 bpp)
        16,  // [13] -> 0x7E1C (16 bpp)
        16,  // [14] -> 0x7E1C (16 bpp)
        16,  // [15] -> 0x7E1C (16 bpp)
        16,  // [16] -> 0x7E1C (16 bpp)
         8,  // [17] -> 0x7E0C (8 bpp)
        16,  // [18] -> 0x7E1C (16 bpp)
         8,  // [19] -> 0x7E0C (8 bpp)
        32,  // [20] -> 0x7E14 (32 bpp)
        32,  // [21] -> 0x7E14 (32 bpp)
        64,  // [22] -> 0x7E24 (64 bpp)
        32,  // [23] -> 0x7E14 (32 bpp)
        16,  // [24] -> 0x7E1C (16 bpp)
        64,  // [25] -> 0x7E24 (64 bpp)
        32,  // [26] -> 0x7E14 (32 bpp)
        16,  // [27] -> 0x7E1C (16 bpp)
       128,  // [28] -> 0x7E2C (128 bpp)
        64,  // [29] -> 0x7E24 (64 bpp)
        32,  // [30] -> 0x7E14 (32 bpp)
        16,  // [31] -> 0x7E1C (16 bpp)
        32,  // [32] -> 0x7E14 (32 bpp)
        32,  // [33] -> 0x7E14 (32 bpp)
         4,  // [34] -> 0x7E04 (4 bpp)
         8,  // [35] -> 0x7E0C (8 bpp)
        16,  // [36] -> 0x7E1C (16 bpp)
       128,  // [37] -> 0x7E2C (128 bpp)
        32,  // [38] -> 0x7E14 (32 bpp)
         8,  // [39] -> 0x7E0C (8 bpp)
        32   // [40] -> 0x7E14 (32 bpp)
    };

    uint32_t r10 = PPC_LOAD_U32(obj + 4) & 0x1FFFFFFF;
    uint32_t bpp = (r10 < 41) ? kFormatBpp[r10] : 0;
    bool isBlock = (r10 >= 1 && r10 <= 8);
    uint32_t minWidth = isBlock ? 4 : 1;
    uint16_t width = PPC_LOAD_U16(obj + 0);
    uint32_t effWidth = (width > minWidth) ? width : minWidth;
    int32_t r11_val = (int32_t)(effWidth * bpp);
    int32_t r10_val = r11_val >> 3;
    if (r11_val < 0 && (r11_val & 7)) r10_val++;
    PPC_STORE_U16(obj + 12, (uint16_t)r10_val);
    LOGF_INFO("[sub_83217D38] obj=0x{:08X} fmt={} width={} bpp={} -> pitch={}",
              obj, r10, width, bpp, r10_val);
}

PPC_EXTERN_FUNC(sub_823871B8);
PPC_EXTERN_FUNC(sub_822369E0);

PPC_FUNC(sub_8226B3B8)
{
    PPC_FUNC_PROLOGUE();
    uint32_t r31_in = ctx.r3.u32;
    PPC_STORE_U32(ctx.r1.u32 + 80, 0);
    ctx.r4.u32 = ctx.r1.u32 + 80;
    if (r31_in >= 0x10000 && r31_in < PPC_MEMORY_SIZE) {
        ctx.r3.u32 = r31_in + 96;
        sub_823871B8(ctx, base);
        PPC_STORE_U32(r31_in + 48, 3);
    }

    uint32_t r3_obj = PPC_LOAD_U32(0x83CB7660);
    if (r3_obj >= 0x10000 && r3_obj < PPC_MEMORY_SIZE)
    {
        uint32_t vtbl = PPC_LOAD_U32(r3_obj);
        if (vtbl >= 0x10000 && vtbl < PPC_MEMORY_SIZE)
        {
            uint32_t fn = PPC_LOAD_U32(vtbl + 16);
            if (fn >= PPC_CODE_BASE && fn < PPC_CODE_BASE + PPC_CODE_SIZE)
            {
                ctx.r3.u32 = r3_obj;
                PPC_CALL_INDIRECT_FUNC(fn);
            }
        }
    }

    if (r31_in >= 0x10000 && r31_in < PPC_MEMORY_SIZE) {
        uint32_t r11_val = PPC_LOAD_U32(r31_in + 60);
        uint32_t r10_val = (r11_val << 2);
        uint32_t r4_val = 0x83CB7684;
        if (r10_val + r4_val < PPC_MEMORY_SIZE) {
            PPC_STORE_U32(r10_val + r4_val, 0xFFFFFFFF);
        }
    }
    ctx.r3.u32 = 0x8386CC20;
    sub_822369E0(ctx, base);
}

PPC_EXTERN_FUNC(sub_82232580);
PPC_EXTERN_FUNC(sub_82237648);
PPC_EXTERN_FUNC(sub_82381A70);
PPC_EXTERN_FUNC(sub_8239CB60);
PPC_EXTERN_FUNC(sub_824AE6D8);
PPC_EXTERN_FUNC(sub_82505F98);
PPC_EXTERN_FUNC(sub_8253EEC0);
PPC_EXTERN_FUNC(sub_82544478);
PPC_EXTERN_FUNC(__savegprlr_24);
PPC_EXTERN_FUNC(__restgprlr_24);

PPC_FUNC(sub_83480F88)
{
    PPC_FUNC_PROLOGUE();
    PPCRegister ctr{};
    PPCCRRegister cr6{};
    uint32_t ea{};
    static std::atomic<uint32_t> s_barrierTrace{0};
    const uint32_t traceId = ++s_barrierTrace;
    __savegprlr_24(ctx, base);
    ea = -160 + ctx.r1.u32;
    PPC_STORE_U32(ea, ctx.r1.u32);
    ctx.r1.u32 = ea;
    ctx.r31.s64 = -2083848192; // 0x83CB0000
    ctx.r27.u64 = ctx.r3.u64;

    uint32_t r3_obj = PPC_LOAD_U32(ctx.r31.u32 + 30324); // 0x83CB7674
    ctx.r3.u64 = r3_obj;
    if (traceId <= 12) {
        const auto readObjectU32 = [object = ctx.r27.u32, &base](uint32_t offset) {
            return object >= 0x10000 &&
                    object <= PPC_MEMORY_SIZE - offset - sizeof(uint32_t)
                ? PPC_LOAD_U32(object + offset)
                : 0;
        };
        const uint32_t queueObj = readObjectU32(4);
        const uint32_t globalCounter = PPC_LOAD_U32(0x83837FF0);
        LOGF_WARNING(
            "[83480F88] entry #{} obj=0x{:08X} queue=0x{:08X} sems={:08X},{:08X},{:08X},{:08X} "
            "flag105={} counter=0x{:08X} queueFields={:08X},{:08X},{:08X},{:08X}",
            traceId, ctx.r27.u32, queueObj,
            readObjectU32(20), readObjectU32(24), readObjectU32(28), readObjectU32(32),
            ctx.r27.u32 < PPC_MEMORY_SIZE - 105 ? PPC_LOAD_U8(ctx.r27.u32 + 105) : 0,
            globalCounter,
            queueObj != 0 && queueObj < PPC_MEMORY_SIZE - 12 ? PPC_LOAD_U32(queueObj + 4) : 0,
            queueObj != 0 && queueObj < PPC_MEMORY_SIZE - 12 ? PPC_LOAD_U32(queueObj + 8) : 0,
            queueObj != 0 && queueObj < PPC_MEMORY_SIZE - 12 ? PPC_LOAD_U32(queueObj + 12) : 0,
            queueObj != 0 && queueObj < PPC_MEMORY_SIZE - 12 ? PPC_LOAD_U32(queueObj + 16) : 0);
    }

    uint32_t r11_val = 0;
    if (r3_obj >= 0x10000 && r3_obj < PPC_MEMORY_SIZE) {
        r11_val = PPC_LOAD_U32(r3_obj + 48);
    }
    ctx.r11.u64 = r11_val;
    int32_t diff = (int32_t)r11_val - 3;
    uint32_t clz = (diff == 0) ? 32 : __builtin_clz(diff);
    ctx.r10.u64 = clz;
    ctx.r11.u64 = __builtin_rotateleft64((uint64_t)clz | ((uint64_t)clz << 32), 27) & 0x1;
    ctx.r24.u64 = ctx.r11.u64;
    cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
    if (cr6.eq) {
        sub_82505F98(ctx, base);
        ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 30324);
        sub_8226B3B8(ctx, base);
        ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 30324);
    }

    ctx.r26.s64 = 0;
    ctx.r25.s64 = 1;
    ctx.r28.s64 = -2084569088; // 0x83C00000

    goto loc_83480FE0;

loc_83480FDC:
    ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 30324);

loc_83480FE0:
    sub_8253EEC0(ctx, base);
    ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 30324);
    sub_82237648(ctx, base);
    ctx.r30.u64 = ctx.r3.u64;
    sub_824AE6D8(ctx, base);

    // Timer / profiler start: 0x83BFBE5C -> vtbl[16]
    uint32_t prof_obj = PPC_LOAD_U32(ctx.r28.u32 + -16804);
    ctx.r29.u64 = ctx.r3.u64;
    ctx.r3.u64 = prof_obj;
    if (prof_obj >= 0x10000 && prof_obj < PPC_MEMORY_SIZE) {
        uint32_t vtbl = PPC_LOAD_U32(prof_obj);
        if (vtbl >= 0x10000 && vtbl < PPC_MEMORY_SIZE) {
            uint32_t fn = PPC_LOAD_U32(vtbl + 16);
            if (fn >= PPC_CODE_BASE && fn < PPC_CODE_BASE + PPC_CODE_SIZE) {
                PPC_CALL_INDIRECT_FUNC(fn);
            }
        }
    }

    ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 30324);
    sub_82232580(ctx, base);

    cr6.compare<uint32_t>(ctx.r30.u32, 0, ctx.xer);
    if (!cr6.eq) {
        ctx.r3.u64 = ctx.r30.u64;
        sub_82544478(ctx, base);
    }

    // Timer / profiler stop: 0x83BFBE5C -> vtbl[20]
    prof_obj = PPC_LOAD_U32(ctx.r28.u32 + -16804);
    ctx.r3.u64 = prof_obj;
    if (prof_obj >= 0x10000 && prof_obj < PPC_MEMORY_SIZE) {
        uint32_t vtbl = PPC_LOAD_U32(prof_obj);
        if (vtbl >= 0x10000 && vtbl < PPC_MEMORY_SIZE) {
            uint32_t fn = PPC_LOAD_U32(vtbl + 20);
            if (fn >= PPC_CODE_BASE && fn < PPC_CODE_BASE + PPC_CODE_SIZE) {
                PPC_CALL_INDIRECT_FUNC(fn);
            }
        }
    }

    sub_824AE6D8(ctx, base);
    ctx.r9.s64 = ctx.r3.s64 - ctx.r29.s64;
    if (ctx.r27.u32 >= 0x10000 && ctx.r27.u32 < PPC_MEMORY_SIZE) {
        PPC_STORE_U32(ctx.r27.u32 + 112, ctx.r9.u32);
    }

    // Virtual call on 0x83CB7674 -> vtbl[20]
    uint32_t obj_30324 = PPC_LOAD_U32(ctx.r31.u32 + 30324);
    ctx.r3.u64 = obj_30324;
    if (obj_30324 >= 0x10000 && obj_30324 < PPC_MEMORY_SIZE) {
        uint32_t vtbl = PPC_LOAD_U32(obj_30324);
        if (vtbl >= 0x10000 && vtbl < PPC_MEMORY_SIZE) {
            uint32_t fn = PPC_LOAD_U32(vtbl + 20);
            if (fn >= PPC_CODE_BASE && fn < PPC_CODE_BASE + PPC_CODE_SIZE) {
                PPC_CALL_INDIRECT_FUNC(fn);
            }
        }
    }

    ctx.r4.s64 = 1;
    if (ctx.r27.u32 >= 0x10000 && ctx.r27.u32 < PPC_MEMORY_SIZE) {
        ctx.r3.u64 = PPC_LOAD_U32(ctx.r27.u32 + 28);
        if (traceId <= 12) {
            LOGF_WARNING("[83480F88] barrier #{} release28=0x{:08X} wait32=0x{:08X}",
                         traceId, ctx.r3.u32, PPC_LOAD_U32(ctx.r27.u32 + 32));
        }
        sub_8239CB60(ctx, base);
        uint8_t flag = PPC_LOAD_U8(ctx.r27.u32 + 105);
        if (flag != 0) {
            ctx.r4.s64 = 1;
            ctx.r3.u64 = PPC_LOAD_U32(ctx.r27.u32 + 32);
            // Worker barrier wait bypassed: tasks complete synchronously on host
            LOGF_INFO("[83480F88] Worker barrier wait on sem=0x{:08X} bypassed", ctx.r3.u32);
        }
    }

    uint32_t cur_obj = PPC_LOAD_U32(ctx.r31.u32 + 30324);
    ctx.r11.u64 = cur_obj;
    cr6.compare<int32_t>(ctx.r26.s32, 3, ctx.xer);
    if (cur_obj >= 0x10000 && cur_obj < PPC_MEMORY_SIZE) {
        PPC_STORE_U32(cur_obj + 48, ctx.r25.u32);
    }
    if (cr6.lt) {
        ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 30324);
        sub_82505F98(ctx, base);
        ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 30324);
        sub_8226B3B8(ctx, base);
    } else {
        cr6.compare<uint32_t>(ctx.r24.u32, 0, ctx.xer);
        if (!cr6.eq) {
            ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 30324);
            sub_82505F98(ctx, base);
            ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 30324);
            sub_8226B3B8(ctx, base);
        }
    }

    ctx.r26.s64 = ctx.r26.s64 + 1;
    cr6.compare<int32_t>(ctx.r26.s32, 4, ctx.xer);
    if (cr6.lt) goto loc_83480FDC;

    // Queue completion callback: vtbl[32] on r27[4]
    if (ctx.r27.u32 >= 0x10000 && ctx.r27.u32 < PPC_MEMORY_SIZE) {
        uint32_t queue_obj = PPC_LOAD_U32(ctx.r27.u32 + 4);
        ctx.r3.u64 = queue_obj;
        if (queue_obj >= 0x10000 && queue_obj < PPC_MEMORY_SIZE) {
            uint32_t vtbl = PPC_LOAD_U32(queue_obj);
            if (vtbl >= 0x10000 && vtbl < PPC_MEMORY_SIZE) {
                uint32_t fn = PPC_LOAD_U32(vtbl + 32);
                if (fn >= PPC_CODE_BASE && fn < PPC_CODE_BASE + PPC_CODE_SIZE) {
                    PPC_CALL_INDIRECT_FUNC(fn);
                }
            }
        }
    }

    ctx.r9.s64 = -2088501248; // lis r9, -31868: 0x83840000
    uint32_t counter_addr = ctx.r9.u32 + -32752;
    if (counter_addr < PPC_MEMORY_SIZE) {
        uint32_t cnt = PPC_LOAD_U32(counter_addr);
        cnt = (cnt + 2) & 0x7FFFFFFF;
        PPC_STORE_U32(counter_addr, cnt);
    }

    ctx.r1.s64 = ctx.r1.s64 + 160;
    __restgprlr_24(ctx, base);
}

// =============================================================================
// RAGE State Machine & Allocator Traps Instrumentation and Diagnostics
// =============================================================================

extern "C" void __imp__sub_824F4850(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_824F4850)
{
    static std::atomic<uint64_t> s_sCalls{0};
    uint64_t c = ++s_sCalls;
    uint32_t obj = ctx.r3.u32;
    int32_t r4 = (int32_t)ctx.r4.s32;
    int32_t r5 = (int32_t)ctx.r5.s32;
    int32_t r6 = (int32_t)ctx.r6.s32;
    if (c <= 30 || c % 100 == 0) {
        LOGF_INFO("[sub_824F4850] #{} obj=0x{:08X} r4={} r5={} r6={}", c, obj, r4, r5, r6);
    }
    __imp__sub_824F4850(ctx, base);
    if (c <= 30 || c % 100 == 0) {
        LOGF_INFO("[sub_824F4850] #{} exit ret={}", c, (int32_t)ctx.r3.s32);
    }
}

extern "C" void __imp__sub_824759C8(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_824759C8)
{
    static std::atomic<uint64_t> s_s2UpdateCalls{0};
    uint64_t c = ++s_s2UpdateCalls;
    if (c <= 20 || c % 60 == 0) {
        LOGF_INFO("[sub_824759C8] State 2 update Frame #{} obj=0x{:08X}", c, ctx.r3.u32);
    }
    // Pace the loop at ~60 FPS (16ms) to avoid starving worker threads
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    __imp__sub_824759C8(ctx, base);
    ScanRingBufferForPM4Packets();
    if (c <= 20 || c % 60 == 0) {
        LOGF_INFO("[sub_824759C8] State 2 update Frame #{} exit ret={}", c, (int32_t)ctx.r3.s32);
    }
}

extern "C" void __imp__sub_8248B538(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_8248B538)
{
    static std::atomic<uint64_t> s_renderCalls{0};
    uint64_t c = ++s_renderCalls;
    if (c <= 20 || c % 60 == 0) {
        LOGF_INFO("[sub_8248B538] Main render submission Frame #{}", c);
    }
    __imp__sub_8248B538(ctx, base);
}

extern "C" void __imp__sub_82988A60(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82988A60)
{
    __imp__sub_82988A60(ctx, base);
    // Emulated keyboard/mouse controller 0 is always connected
    if (ctx.r3.u32 == 0) {
        static int s_connectedLog = 0;
        if (++s_connectedLog <= 5) {
            LOGF_INFO("[sub_82988A60] Emulated controller 0 marked connected (was 0)");
        }
        ctx.r3.u64 = 1;
    }
}

extern "C" void __imp__sub_82578948(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82578948)
{
    // Storage is never busy in recompilation (no async disc drive latency)
    static int s_count = 0;
    if (++s_count <= 10) {
        LOGF_INFO("[sub_82578948] #{} Storage idle report (ret=0) LR=0x{:08X}", s_count, ctx.lr);
    }
    ctx.r3.u64 = 0;
}

extern "C" void __imp__sub_8294C6C0(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_8294C6C0)
{
    __imp__sub_8294C6C0(ctx, base);
    static int s_count = 0;
    if (++s_count <= 20 || s_count % 100 == 0) {
        uint32_t state = PPC_LOAD_U32(0x839D0000 - 4868);
        LOGF_INFO("[sub_8294C6C0] #{} state={} ret={}", s_count, state, ctx.r3.u32);
    }
}

extern "C" void __imp__sub_82948308(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82948308)
{
    static int s_count = 0;
    if (++s_count <= 20 || s_count % 50 == 0) {
        LOGF_INFO("[sub_82948308] #{} r3={} r4={} r5=0x{:08X} LR=0x{:08X}",
                  s_count, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.lr);
    }
    // Set output flag to 1 (profile / storage setting ready)
    if (ctx.r5.u32 != 0 && ctx.r5.u32 < PPC_MEMORY_SIZE) {
        PPC_STORE_U8(ctx.r5.u32, 1);
    }
    ctx.r3.u64 = 1;
}

extern "C" void __imp__sub_822438F8(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_822438F8)
{
    static int s_count = 0;
    uint32_t stageBefore = PPC_LOAD_U32(0x8388C63C);
    LOGF_INFO("[sub_822438F8] #{} ENTER stage={}", ++s_count, stageBefore);
    __imp__sub_822438F8(ctx, base);
    uint32_t stageAfter = PPC_LOAD_U32(0x8388C63C);
    LOGF_INFO("[sub_822438F8] #{} EXIT stage={}", s_count, stageAfter);
}

PPC_FUNC(sub_8344F920)
{
    static int s_count = 0;
    const char* str = nullptr;
    if (ctx.r3.u32 < PPC_MEMORY_SIZE) {
        str = (const char*)(base + ctx.r3.u32);
    }
    
    uint32_t savedLr = ctx.lr;
    
    ctx.r4.s64 = 0;
    sub_823C0348(ctx, base);
    uint32_t hash = ctx.r3.u32;
    
    uint32_t tableAddr = 0x83C90890;
    uint32_t entriesBase = PPC_LOAD_U32(tableAddr + 0);
    uint32_t bucketsBase = PPC_LOAD_U32(tableAddr + 4);
    uint16_t modulus = PPC_LOAD_U16(tableAddr + 8);
    
    if (modulus == 0 || bucketsBase == 0 || entriesBase == 0) {
        LOGF_WARNING("[sub_8344F920] Table uninitialized: entries=0x{:08X} buckets=0x{:08X} mod={}",
            entriesBase, bucketsBase, modulus);
        ctx.r3.s64 = -1;
        ctx.lr = savedLr;
        return;
    }
    
    uint32_t bucketIdx = hash % modulus;
    uint16_t entryIdx = PPC_LOAD_U16(bucketsBase + bucketIdx * 2);
    
    int loopCount = 0;
    while (entryIdx != 0xFFFF && loopCount < 1000) {
        uint32_t entryAddr = entriesBase + entryIdx * 8;
        uint32_t key = PPC_LOAD_U32(entryAddr + 0);
        if (key == hash) {
            uint16_t value = PPC_LOAD_U16(entryAddr + 4);
            ctx.r3.u64 = value;
            ctx.lr = savedLr;
            if (++s_count <= 20 || (s_count % 500 == 0)) {
                LOGF_INFO("[sub_8344F920] #{} FOUND str='{}' hash=0x{:08X} val={}", s_count, str ? str : "", hash, value);
            }
            return;
        }
        uint16_t nextIdx = PPC_LOAD_U16(entryAddr + 6);
        if (nextIdx == entryIdx) {
            LOGF_WARNING("[sub_8344F920] CYCLE detected in bucket {}: entry {} points to itself (str='{}' hash=0x{:08X})",
                bucketIdx, entryIdx, str ? str : "", hash);
            break;
        }
        entryIdx = nextIdx;
        loopCount++;
    }
    
    if (loopCount >= 1000) {
        LOGF_WARNING("[sub_8344F920] LOOP LIMIT EXCEEDED in bucket {}: entryIdx={} for str='{}' hash=0x{:08X}",
            bucketIdx, entryIdx, str ? str : "", hash);
    }
    
    ctx.r3.s64 = -1;
    ctx.lr = savedLr;
}

PPC_FUNC(sub_827D1138)
{
    static int s_count = 0;
    uint32_t mapPtr = ctx.r3.u32;
    uint32_t key = ctx.r4.u32;
    
    if (mapPtr < PPC_MEMORY_SIZE) {
        uint16_t modulus = PPC_LOAD_U16(mapPtr + 80);
        uint32_t bucketsBase = PPC_LOAD_U32(mapPtr + 76);
        uint32_t entriesBase = PPC_LOAD_U32(mapPtr + 72);
        
        if (modulus == 0 || bucketsBase == 0 || entriesBase == 0) {
            ctx.r3.s64 = -1;
            return;
        }
        
        uint32_t bucketIdx = key % modulus;
        uint16_t entryIdx = PPC_LOAD_U16(bucketsBase + bucketIdx * 2);
        
        int loopCount = 0;
        while (entryIdx != 0xFFFF && loopCount < 1000) {
            uint32_t entryAddr = entriesBase + entryIdx * 8;
            uint32_t entryKey = PPC_LOAD_U32(entryAddr + 0);
            if (entryKey == key) {
                uint16_t val11 = PPC_LOAD_U16(entryAddr + 4);
                if (val11 == 0xFFFF) {
                    ctx.r3.s64 = -1;
                    return;
                }
                uint32_t r8_addr = PPC_LOAD_U32(mapPtr + 40);
                uint32_t r7_stride = PPC_LOAD_U32(mapPtr + 48);
                uint32_t r10_base = PPC_LOAD_U32(mapPtr + 36);
                uint32_t r9_off = r7_stride * val11;
                uint8_t r6_byte = PPC_LOAD_U8(r8_addr + val11);
                uint32_t r5 = r6_byte & 0x80;
                uint32_t r4_res = r9_off + r10_base;
                uint32_t r3_neg = (uint32_t)(- (int32_t)r5);
                uint32_t r10_or = r3_neg | r5;
                int32_t r9_srawi = (int32_t)r10_or >> 31;
                uint32_t r8_andc = r4_res & ~((uint32_t)r9_srawi);
                if (r8_andc != 0) {
                    ctx.r3.u64 = val11;
                    if (++s_count <= 20 || (s_count % 500 == 0)) {
                        LOGF_INFO("[sub_827D1138] #{} FOUND map=0x{:08X} key=0x{:08X} val={}", s_count, mapPtr, key, val11);
                    }
                    return;
                }
                ctx.r3.s64 = -1;
                return;
            }
            uint16_t nextIdx = PPC_LOAD_U16(entryAddr + 6);
            if (nextIdx == entryIdx) {
                LOGF_WARNING("[sub_827D1138] CYCLE detected in bucket {}: entry {} points to itself (map=0x{:08X} key=0x{:08X})",
                    bucketIdx, entryIdx, mapPtr, key);
                break;
            }
            entryIdx = nextIdx;
            loopCount++;
        }
        if (loopCount >= 1000) {
            LOGF_WARNING("[sub_827D1138] LOOP LIMIT EXCEEDED in bucket {}: entryIdx={} for key=0x{:08X}",
                bucketIdx, entryIdx, key);
        }
    }
    ctx.r3.s64 = -1;
}

extern "C" void __imp__sub_831EDB68(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_831EDB68)
{
    static int s_count = 0;
    uint32_t r31 = ctx.r3.u32;
    uint32_t r28 = ctx.r4.u32;
    const char* pathStr = (r28 >= 0x10000 && r28 < PPC_MEMORY_SIZE) ? (const char*)(base + r28) : "null";
    uint32_t count = (r31 >= 0x10000 && r31 < PPC_MEMORY_SIZE - 4096) ? PPC_LOAD_U32(r31 + 3076) : 0;
    if (++s_count <= 50 || s_count % 100 == 0) {
        LOGF_INFO("[sub_831EDB68] #{} enter r3=0x{:08X} path='{}' packCount={} callerLR=0x{:08X}", s_count, r31, pathStr, count, ctx.lr);
    }
    __imp__sub_831EDB68(ctx, base);
    if (s_count <= 50 || s_count % 100 == 0) {
        LOGF_INFO("[sub_831EDB68] #{} exit ret=0x{:08X}", s_count, ctx.r3.u32);
    }
}

extern "C" void __imp__sub_831D7BD8(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_831D7BD8)
{
    static std::atomic<uint64_t> s_calls{0};
    uint64_t c = ++s_calls;
    if (c <= 30 || (c % 100000) == 0) {
        LOGF_INFO("[sub_831D7BD8] #{} called by LR=0x{:08X} r3=0x{:08X} r4=0x{:08X}", c, ctx.lr, ctx.r3.u32, ctx.r4.u32);
    }
    __imp__sub_831D7BD8(ctx, base);
}

extern "C" void __imp__sub_824BBB30(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_824BBB30)
{
    static int s_count = 0;
    if (++s_count <= 20 || s_count % 1000 == 0) {
        LOGF_INFO("[sub_824BBB30] #{} enter", s_count);
    }
    __imp__sub_824BBB30(ctx, base);
    if (s_count <= 20 || s_count % 1000 == 0) {
        LOGF_INFO("[sub_824BBB30] #{} exit", s_count);
    }
}

extern "C" void __imp__sub_822DA770(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_822DA770)
{
    static int s_count = 0;
    if (++s_count <= 20 || s_count % 5000 == 0) {
        LOGF_INFO("[sub_822DA770] #{} r3=0x{:08X}", s_count, ctx.r3.u32);
    }
    __imp__sub_822DA770(ctx, base);
}

static std::atomic<uint32_t> g_lastStateMachineObj{0x83887150};

extern "C" void __imp__sub_824B9C20(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_824B9C20)
{
    static std::atomic<uint64_t> s_smCalls{0};
    uint64_t c = ++s_smCalls;
    uint32_t obj = ctx.r3.u32;
    if (obj >= 0x10000 && obj < PPC_MEMORY_SIZE - 20) {
        g_lastStateMachineObj.store(obj);
        if (PPC_LOAD_U32(obj) == 0) {
            PPC_STORE_U32(obj, 0x82001554);
            LOGF_WARNING("[sub_824B9C20] Restored vtable 0x82001554 on obj=0x{:08X}", obj);
        }
        uint32_t curVtbl = PPC_LOAD_U32(obj);
        if (curVtbl >= 0x10000 && curVtbl < PPC_MEMORY_SIZE - 8) {
            if (PPC_LOAD_U32(curVtbl + 4) == 0) {
                PPC_STORE_U32(curVtbl + 4, 0x824F4850);
                LOGF_WARNING("[sub_824B9C20] Restored vtable[1] = 0x824F4850 at 0x{:08X}", curVtbl + 4);
            }
        }
    }
    uint16_t state = (obj < PPC_MEMORY_SIZE - 20) ? PPC_LOAD_U16(obj + 12) : 0;
    uint16_t nextState = (obj < PPC_MEMORY_SIZE - 20) ? PPC_LOAD_U16(obj + 14) : 0;
    uint16_t pendingState = (obj < PPC_MEMORY_SIZE - 20) ? PPC_LOAD_U16(obj + 16) : 0;
    uint16_t flags = (obj < PPC_MEMORY_SIZE - 20) ? PPC_LOAD_U16(obj + 18) : 0;
    uint32_t vtbl = (obj < PPC_MEMORY_SIZE - 20) ? PPC_LOAD_U32(obj) : 0;
    uint32_t fn4 = (vtbl >= 0x10000 && vtbl < PPC_MEMORY_SIZE - 20) ? PPC_LOAD_U32(vtbl + 4) : 0;
    if (c <= 30 || c % 100 == 0) {
        LOGF_INFO("[sub_824B9C20] StateMachine enter #{} obj=0x{:08X} state={}/{} pending={} flags=0x{:04X} vtbl=0x{:08X} fn4=0x{:08X}",
                  c, obj, state, nextState, pendingState, flags, vtbl, fn4);
    }
    __imp__sub_824B9C20(ctx, base);
    uint16_t endState = (obj < PPC_MEMORY_SIZE - 20) ? PPC_LOAD_U16(obj + 12) : 0;
    if (c <= 30 || c % 100 == 0) {
        LOGF_INFO("[sub_824B9C20] StateMachine exit #{} state={} ret={}",
                  c, endState, (int32_t)ctx.r3.s32);
    }
}

extern "C" void __imp__sub_824F1388(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_824F1388)
{
    if (IsMainThread()) {
        static int s_mainSleep1388 = 0;
        if (s_mainSleep1388++ < 30 || s_mainSleep1388 % 1000 == 0) {
            auto trace = CaptureGuestStackTrace();
            LOGF_WARNING("[sub_824F1388] Main thread Sleep({}, r4={}) called! trace: 0x{:08X}, 0x{:08X}, 0x{:08X}, 0x{:08X}, 0x{:08X}",
                         (int32_t)ctx.r3.s32, (int32_t)ctx.r4.s32,
                         trace.size() > 0 ? trace[0] : 0, trace.size() > 1 ? trace[1] : 0,
                         trace.size() > 2 ? trace[2] : 0, trace.size() > 3 ? trace[3] : 0,
                         trace.size() > 4 ? trace[4] : 0);
        }
    }
    __imp__sub_824F1388(ctx, base);
}

extern "C" void __imp__sub_824F1418(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_824F1418)
{
    if (IsMainThread()) {
        static int s_mainSleep1418 = 0;
        if (s_mainSleep1418++ < 30 || s_mainSleep1418 % 1000 == 0) {
            auto trace = CaptureGuestStackTrace();
            LOGF_WARNING("[sub_824F1418] Main thread Sleep({}) called! trace: 0x{:08X}, 0x{:08X}, 0x{:08X}, 0x{:08X}, 0x{:08X}",
                         (int32_t)ctx.r3.s32,
                         trace.size() > 0 ? trace[0] : 0, trace.size() > 1 ? trace[1] : 0,
                         trace.size() > 2 ? trace[2] : 0, trace.size() > 3 ? trace[3] : 0,
                         trace.size() > 4 ? trace[4] : 0);
        }
    }
    __imp__sub_824F1418(ctx, base);
}

extern "C" void __imp__sub_825777D0(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_825777D0)
{
    static std::atomic<uint64_t> s_s1Calls{0};
    uint64_t c = ++s_s1Calls;
    if (ctx.r3.u32 >= 0x10000) {
        g_lastStateMachineObj.store(ctx.r3.u32);
    }
    LOGF_INFO("[sub_825777D0] State 1 handler enter #{} obj=0x{:08X}", c, ctx.r3.u32);
    __imp__sub_825777D0(ctx, base);
    LOGF_INFO("[sub_825777D0] State 1 handler exit #{} ret={}", c, (int32_t)ctx.r3.s32);
}

extern "C" void __imp__sub_82574338(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82574338)
{
    KernelPhase_EnterRuntime();
    static std::atomic<uint64_t> s_s2Calls{0};
    uint64_t c = ++s_s2Calls;
    LOGF_INFO("[sub_82574338] State 2 handler enter #{} obj=0x{:08X}", c, ctx.r3.u32);
    auto t0 = std::chrono::steady_clock::now();
    __imp__sub_82574338(ctx, base);
    auto t1 = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    LOGF_INFO("[sub_82574338] State 2 handler exit #{} ret={} elapsed={}ms", c, (int32_t)ctx.r3.s32, ms);
}

extern "C" void __imp__sub_82985760(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82985760)
{
    static std::atomic<uint64_t> s_calls{0};
    uint64_t c = ++s_calls;
    __imp__sub_82985760(ctx, base);
    uint32_t origRet = ctx.r3.u32;
    // Packfile status check: Ensure loc_8257785C (sub_8229AD78 state 2 dispatch) is unblocked
    if (ctx.lr == 0x82577834 || ctx.lr == 0x82577800) {
        ctx.r3.u64 = 1;
    }
    LOGF_INFO("[sub_82985760] #{} packfile status check callerLR=0x{:08X} origRet={} -> ret={}",
              c, ctx.lr, origRet, ctx.r3.u32);
}

extern "C" void __imp__sub_82574450(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82574450)
{
    static std::atomic<uint64_t> s_calls{0};
    uint64_t c = ++s_calls;
    if (c <= 30 || c % 60 == 0) {
        LOGF_INFO("[sub_82574450] State 2 Frame Update #{} obj=0x{:08X}", c, ctx.r3.u32);
    }
    __imp__sub_82574450(ctx, base);
}

extern "C" void __imp__sub_822D9768(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_822D9768)
{
    static std::atomic<uint64_t> s_calls{0};
    uint64_t c = ++s_calls;
    uint32_t pool = ctx.r3.u32;
    if (pool < 0x10000 || pool >= PPC_MEMORY_SIZE - 2500) {
        ctx.r3.u64 = 0;
        return;
    }
    uint32_t freeBlocks = PPC_LOAD_U32(pool + 2184);
    uint32_t activeBlocks = (pool < PPC_MEMORY_SIZE - 2400) ? PPC_LOAD_U32(pool + 2380) : 0;

    if (freeBlocks == 0 && activeBlocks == 0) {
        LOGF_WARNING("[sub_822D9768] Pool 0x{:08X} uninitialized: 0 free, 0 active. Populating 48 free slots!", pool);
        PPC_STORE_U32(pool + 2184, 48);
        for (uint32_t i = 0; i < 48; ++i) {
            PPC_STORE_U32(pool + 1992 + i * 4, i);
        }
        freeBlocks = 48;
    }

    if (c <= 30 || c % 100 == 0) {
        LOGF_INFO("[sub_822D9768] #{} pool=0x{:08X} free={} active={} r4={} r5={} r6={} r7={}",
                  c, pool, freeBlocks, activeBlocks, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32);
    }

    __imp__sub_822D9768(ctx, base);

    if (c <= 30 || c % 100 == 0) {
        LOGF_INFO("[sub_822D9768] #{} exit ret=0x{:08X}", c, ctx.r3.u32);
    }
}

extern "C" void __imp__sub_822D9690(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_822D9690)
{
    static std::atomic<uint64_t> s_calls{0};
    uint64_t c = ++s_calls;
    uint32_t pool = ctx.r3.u32;
    if (pool < 0x10000 || pool >= PPC_MEMORY_SIZE - 2500) {
        ctx.r3.s64 = -1;
        return;
    }
    if (c <= 30 || c % 100 == 0) {
        LOGF_INFO("[sub_822D9690] #{} pool=0x{:08X} r4={} r5={} r6={} r7={}",
                  c, pool, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32);
    }
    __imp__sub_822D9690(ctx, base);
    if (c <= 30 || c % 100 == 0) {
        LOGF_INFO("[sub_822D9690] #{} exit ret=0x{:08X}", c, ctx.r3.u32);
    }
}

extern "C" void __imp__sub_834DFFA0(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_834DFFA0)
{
    static std::atomic<uint64_t> s_calls{0};
    uint64_t c = ++s_calls;
    uint32_t desc = ctx.r3.u32;
    uint32_t r4 = ctx.r4.u32;
    uint32_t r5 = ctx.r5.u32;
    if (c <= 30 || c % 100 == 0) {
        LOGF_INFO("[sub_834DFFA0] #{} desc=0x{:08X} r4={} r5={}", c, desc, r4, r5);
    }
    __imp__sub_834DFFA0(ctx, base);
    if (ctx.r3.u32 == 0) {
        // Fallback physical allocation if guest allocator returned null.
        // Cache allocated physical buffer per descriptor to prevent physical heap exhaustion.
        static std::mutex s_descMutex;
        static std::unordered_map<uint32_t, uint32_t> s_descBuffers;
        std::lock_guard<std::mutex> lock(s_descMutex);
        uint32_t guestMem = 0;
        auto it = s_descBuffers.find(desc);
        if (it != s_descBuffers.end()) {
            guestMem = it->second;
        } else {
            void* hostMem = g_userHeap.AllocPhysical(r5, 128);
            if (hostMem) {
                guestMem = g_memory.MapVirtual(hostMem);
                s_descBuffers[desc] = guestMem;
                LOGF_WARNING("[sub_834DFFA0] #{} Allocated fallback physical buffer 0x{:08X} (size={}) for desc=0x{:08X}",
                             c, guestMem, r5, desc);
            }
        }
        if (guestMem != 0) {
            if (desc >= 0x10000 && desc < PPC_MEMORY_SIZE - 20) {
                PPC_STORE_U32(desc + 0, guestMem);
                PPC_STORE_U32(desc + 4, r4);
                PPC_STORE_U32(desc + 16, r5);
                PPC_STORE_U16(desc + 10, 0xFFFF);
            }
            ctx.r3.u64 = 1;
        }
    }
    if (c <= 30 || c % 100 == 0) {
        LOGF_INFO("[sub_834DFFA0] #{} exit ret={}", c, (int32_t)ctx.r3.s32);
    }
}

PPC_EXTERN_FUNC(sub_82F3A5B8);
PPC_EXTERN_FUNC(sub_828FD100);
PPC_EXTERN_FUNC(sub_82918D28);
PPC_EXTERN_FUNC(sub_825FE880);
PPC_EXTERN_FUNC(sub_8255DC58);
PPC_EXTERN_FUNC(sub_8343D470);
PPC_EXTERN_FUNC(sub_829E21D8);
PPC_EXTERN_FUNC(sub_83447AD0);
PPC_EXTERN_FUNC(sub_8299AB58);
PPC_EXTERN_FUNC(sub_829DB848);
PPC_EXTERN_FUNC(sub_829E3CA8);
PPC_EXTERN_FUNC(sub_829B4B60);
PPC_EXTERN_FUNC(sub_8298B940);
PPC_EXTERN_FUNC(sub_829A8968);
PPC_EXTERN_FUNC(sub_825AD950);
PPC_EXTERN_FUNC(sub_825B7C70);
PPC_EXTERN_FUNC(sub_829A5DA8);
PPC_EXTERN_FUNC(sub_834ECCE0);
PPC_EXTERN_FUNC(sub_82FDD930);
PPC_EXTERN_FUNC(sub_829561F0);
PPC_EXTERN_FUNC(sub_825BCBF8);
PPC_EXTERN_FUNC(sub_82574530);
PPC_EXTERN_FUNC(sub_829D3250);
PPC_EXTERN_FUNC(sub_8222ED58);
PPC_EXTERN_FUNC(sub_829847D0);
PPC_EXTERN_FUNC(sub_82525480);
PPC_EXTERN_FUNC(sub_825D17B8);
PPC_EXTERN_FUNC(sub_825C09C0);
PPC_EXTERN_FUNC(sub_82EF52B0);
PPC_EXTERN_FUNC(sub_82EF6B38);

extern "C" void __imp__sub_8299AB58(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_8299AB58)
{
    LOGF_WARNING("[sub_8299AB58] enter r3=0x{:08X}", ctx.r3.u32);
    auto t0 = std::chrono::steady_clock::now();
    __imp__sub_8299AB58(ctx, base);
    auto t1 = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    LOGF_WARNING("[sub_8299AB58] exit elapsed={}ms", ms);
}

extern "C" void __imp__sub_8345BC98(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_8345BC98)
{
    LOGF_WARNING("[sub_8345BC98] enter r3=0x{:08X}", ctx.r3.u32);
    __imp__sub_8345BC98(ctx, base);
    LOGF_WARNING("[sub_8345BC98] exit");
}

extern "C" void __imp__sub_829E3CA8(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_829E3CA8)
{
    static std::atomic<int> s_callCount{0};
    int count = ++s_callCount;
    if (count <= 20) {
        LOGF_WARNING("[sub_829E3CA8] #{} enter r3=0x{:08X} r4=0x{:08X} r5=0x{:08X}", count, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
    }
    __imp__sub_829E3CA8(ctx, base);
    if (count <= 20) {
        LOGF_WARNING("[sub_829E3CA8] #{} exit", count);
    }
}

extern "C" void __imp__sub_8298B940(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_8298B940)
{
    LOGF_WARNING("[sub_8298B940] enter");
    __imp__sub_8298B940(ctx, base);
    LOGF_WARNING("[sub_8298B940] exit");
}

extern "C" void __imp__sub_829C4C00(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_829C4C00)
{
    LOGF_WARNING("[sub_829C4C00] enter r3=0x{:08X}", ctx.r3.u32);
    __imp__sub_829C4C00(ctx, base);
    LOGF_WARNING("[sub_829C4C00] exit");
}

PPC_FUNC(sub_82577590)
{
	PPC_FUNC_PROLOGUE();
	PPCCRRegister cr6{};
	uint32_t ea{};
	LOGF_INFO("[sub_82577590] State 1 sub_82577590 enter obj=0x{:08X}", ctx.r3.u32);
	auto t0 = std::chrono::steady_clock::now();

	ctx.r12.u64 = ctx.lr;
	PPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	PPC_STORE_U64(ctx.r1.u32 + -24, ctx.r30.u64);
	PPC_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	ea = -160 + ctx.r1.u32;
	PPC_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;

	ctx.r11.s64 = -2085683200;
	ctx.r4.s64 = 1;
	ctx.r3.s64 = ctx.r11.s64 + -18376;
	ctx.lr = 0x825775B4;
	LOGF_INFO("[sub_82577590] Step 1: sub_82F3A5B8");
	sub_82F3A5B8(ctx, base);

	ctx.lr = 0x825775B8;
	LOGF_INFO("[sub_82577590] Step 2: sub_828FD100");
	sub_828FD100(ctx, base);

	ctx.lr = 0x825775BC;
	LOGF_INFO("[sub_82577590] Step 3: sub_82918D28");
	sub_82918D28(ctx, base);

	ctx.r4.s64 = 0;
	ctx.r3.s64 = 1;
	ctx.lr = 0x825775C8;
	LOGF_INFO("[sub_82577590] Step 4: sub_825FE880");
	sub_825FE880(ctx, base);

	ctx.r3.s64 = 4;
	ctx.lr = 0x825775D0;
	LOGF_INFO("[sub_82577590] Step 5: sub_8255DC58");
	sub_8255DC58(ctx, base);
	cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	if (!cr6.eq) {
		ctx.r11.s64 = -2113863680;
		ctx.r10.s64 = ctx.r11.s64 + 7516;
		PPC_STORE_U32(ctx.r3.u32 + 0, ctx.r10.u32);
	} else {
		ctx.r3.s64 = 0;
	}

	ctx.lr = 0x825775F0;
	LOGF_INFO("[sub_82577590] Step 6: sub_8343D470");
	sub_8343D470(ctx, base);

	ctx.r31.s64 = -2088173568;
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 11648);
	cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	if (cr6.eq) {
		ctx.r3.s64 = 216;
		ctx.lr = 0x82577608;
		sub_8255DC58(ctx, base);
		ctx.r30.u64 = ctx.r3.u64;
		cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
		if (!cr6.eq) {
			ctx.lr = 0x82577618;
			sub_829E21D8(ctx, base);
			PPC_STORE_U32(ctx.r31.u32 + 11648, ctx.r30.u32);
		} else {
			PPC_STORE_U32(ctx.r31.u32 + 11648, 0);
		}
	}

	ctx.lr = 0x8257762C;
	LOGF_INFO("[sub_82577590] Step 7: sub_83447AD0");
	sub_83447AD0(ctx, base);

	ctx.r3.s64 = 1;
	ctx.lr = 0x82577634;
	LOGF_INFO("[sub_82577590] Step 8: sub_8299AB58");
	sub_8299AB58(ctx, base);

	ctx.r3.s64 = 1;
	ctx.lr = 0x8257763C;
	LOGF_INFO("[sub_82577590] Step 9: sub_829DB848");
	sub_829DB848(ctx, base);

	ctx.r11.s64 = -2113863680;
	ctx.r5.s64 = 1;
	ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 11648);
	ctx.r4.s64 = ctx.r11.s64 + 12472;
	ctx.lr = 0x82577650;
	LOGF_INFO("[sub_82577590] Step 10: sub_829E3CA8");
	sub_829E3CA8(ctx, base);

	ctx.r3.s64 = 1;
	ctx.lr = 0x82577658;
	LOGF_INFO("[sub_82577590] Step 11: sub_829B4B60");
	sub_829B4B60(ctx, base);

	ctx.lr = 0x8257765C;
	LOGF_INFO("[sub_82577590] Step 12: sub_8298B940");
	sub_8298B940(ctx, base);

	ctx.lr = 0x82577660;
	LOGF_INFO("[sub_82577590] Step 13: sub_829A8968");
	sub_829A8968(ctx, base);

	ctx.r10.s64 = -2113863680;
	ctx.r3.s64 = ctx.r10.s64 + 12432;
	ctx.lr = 0x8257766C;
	LOGF_INFO("[sub_82577590] Step 14: sub_825AD950");
	sub_825AD950(ctx, base);

	ctx.lr = 0x82577670;
	LOGF_INFO("[sub_82577590] Step 15: sub_825B7C70");
	sub_825B7C70(ctx, base);

	ctx.r9.s64 = -2113863680;
	ctx.r5.s64 = 1;
	ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 11648);
	ctx.r4.s64 = ctx.r9.s64 + 12408;
	ctx.lr = 0x82577684;
	LOGF_INFO("[sub_82577590] Step 16: sub_829E3CA8");
	sub_829E3CA8(ctx, base);

	ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 11648);
	ctx.lr = 0x8257768C;
	LOGF_INFO("[sub_82577590] Step 17: sub_829A5DA8");
	sub_829A5DA8(ctx, base);

	ctx.r3.s64 = 0;
	ctx.lr = 0x82577694;
	LOGF_INFO("[sub_82577590] Step 18: sub_834ECCE0");
	sub_834ECCE0(ctx, base);

	ctx.r3.s64 = 1;
	ctx.lr = 0x8257769C;
	LOGF_INFO("[sub_82577590] Step 19: sub_82FDD930");
	sub_82FDD930(ctx, base);

	ctx.r3.s64 = 1;
	ctx.lr = 0x825776A4;
	LOGF_INFO("[sub_82577590] Step 20: sub_829561F0");
	sub_829561F0(ctx, base);

	ctx.lr = 0x825776A8;
	LOGF_INFO("[sub_82577590] Step 21: sub_825BCBF8");
	sub_825BCBF8(ctx, base);

	ctx.r31.s64 = -2088239104;
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 28576);
	cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	if (cr6.eq) {
		ctx.r3.s64 = 248;
		ctx.lr = 0x825776C0;
		sub_8255DC58(ctx, base);
		ctx.r30.u64 = ctx.r3.u64;
		cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
		if (!cr6.eq) {
			ctx.lr = 0x825776D0;
			sub_82574530(ctx, base);
			ctx.r11.s64 = -2113863680;
			ctx.r3.u64 = ctx.r30.u64;
			ctx.r10.s64 = ctx.r11.s64 + 10600;
			PPC_STORE_U32(ctx.r30.u32 + 0, ctx.r10.u32);
			PPC_STORE_U32(ctx.r31.u32 + 28576, ctx.r3.u32);
		} else {
			PPC_STORE_U32(ctx.r31.u32 + 28576, 0);
		}
		ctx.lr = 0x825776F0;
		LOGF_INFO("[sub_82577590] Step 22: sub_829D3250");
		sub_829D3250(ctx, base);
	}

	ctx.r11.s64 = -2108293120;
	ctx.r7.s64 = 0;
	ctx.r6.s64 = 0;
	ctx.r5.s64 = ctx.r11.s64 + 32392;
	ctx.r4.s64 = 0;
	ctx.r3.s64 = ctx.r1.s64 + 96;
	ctx.lr = 0x8257770C;
	LOGF_INFO("[sub_82577590] Step 23: sub_8222ED58");
	sub_8222ED58(ctx, base);

	ctx.r10.s64 = -2105933824;
	ctx.r9.s64 = ctx.r1.s64 + 96;
	ctx.r8.s64 = ctx.r10.s64 + -27072;
	ctx.r10.s64 = -2108293120;
	PPC_STORE_U32(ctx.r1.u32 + 108, ctx.r8.u32);
	ctx.r11.s64 = ctx.r1.s64 + 112;
	ctx.r5.s64 = ctx.r10.s64 + 32320;
	ctx.r7.s64 = 0;
	ctx.r6.s64 = 0;
	ctx.r4.s64 = 0;
	ctx.r3.s64 = ctx.r1.s64 + 80;
	ctx.r8.u64 = PPC_LOAD_U32(ctx.r9.u32 + 4);
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r9.u32 + 8);
	ctx.r31.u64 = PPC_LOAD_U32(ctx.r9.u32 + 12);
	ctx.r9.u64 = PPC_LOAD_U32(ctx.r9.u32 + 0);
	PPC_STORE_U32(ctx.r11.u32 + 0, ctx.r9.u32);
	PPC_STORE_U32(ctx.r11.u32 + 4, ctx.r8.u32);
	PPC_STORE_U32(ctx.r11.u32 + 8, ctx.r10.u32);
	PPC_STORE_U32(ctx.r11.u32 + 12, ctx.r31.u32);
	ctx.lr = 0x8257775C;
	LOGF_INFO("[sub_82577590] Step 24: sub_8222ED58");
	sub_8222ED58(ctx, base);

	ctx.r8.s64 = -2105933824;
	ctx.r5.u64 = PPC_LOAD_U64(ctx.r1.u32 + 112);
	ctx.r6.u64 = PPC_LOAD_U64(ctx.r1.u32 + 120);
	ctx.r7.s64 = ctx.r8.s64 + -27072;
	ctx.r3.u64 = PPC_LOAD_U64(ctx.r1.u32 + 80);
	PPC_STORE_U32(ctx.r1.u32 + 92, ctx.r7.u32);
	ctx.r4.u64 = PPC_LOAD_U64(ctx.r1.u32 + 88);
	ctx.lr = 0x8257777C;
	LOGF_INFO("[sub_82577590] Step 25: sub_829847D0");
	sub_829847D0(ctx, base);

	ctx.r6.s64 = -2088173568;
	ctx.r4.s64 = 1;
	ctx.r3.u64 = PPC_LOAD_U32(ctx.r6.u32 + 23808);
	ctx.lr = 0x8257778C;
	LOGF_INFO("[sub_82577590] Step 26: sub_82525480");
	sub_82525480(ctx, base);

	ctx.lr = 0x82577790;
	LOGF_INFO("[sub_82577590] Step 27: sub_825D17B8");
	sub_825D17B8(ctx, base);

	ctx.r3.s64 = 1;
	ctx.lr = 0x82577798;
	LOGF_INFO("[sub_82577590] Step 28: sub_825C09C0");
	sub_825C09C0(ctx, base);

	ctx.lr = 0x8257779C;
	LOGF_INFO("[sub_82577590] Step 29: sub_82EF52B0");
	sub_82EF52B0(ctx, base);

	ctx.r5.s64 = -2088697856;
	ctx.r4.s64 = 0;
	ctx.r3.s64 = ctx.r5.s64 + 3528;
	ctx.lr = 0x825777AC;
	LOGF_INFO("[sub_82577590] Step 30: sub_82EF6B38");
	sub_82EF6B38(ctx, base);

	ctx.r4.s64 = -2088566784;
	ctx.r11.s64 = 4;
	PPC_STORE_U32(ctx.r4.u32 + 31264, ctx.r11.u32);

	ctx.r1.s64 = ctx.r1.s64 + 160;
	ctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);
	ctx.lr = ctx.r12.u64;
	ctx.r30.u64 = PPC_LOAD_U64(ctx.r1.u32 + -24);
	ctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 + -16);

	auto t1 = std::chrono::steady_clock::now();
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
	LOGF_INFO("[sub_82577590] State 1 sub_82577590 exit elapsed={}ms", ms);
}

extern "C" void __imp__sub_835100D0(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_835100D0)
{
    PPC_FUNC_PROLOGUE();
    PPCRegister ctr{};
    PPCCRRegister cr6{};
    uint32_t ea{};
    PPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
    PPC_STORE_U64(ctx.r1.u32 + -24, ctx.r30.u64);
    PPC_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
    ea = -112 + ctx.r1.u32;
    PPC_STORE_U32(ea, ctx.r1.u32);
    ctx.r1.u32 = ea;

    if (ctx.r3.u32 >= 0x10000 && ctx.r3.u32 < PPC_MEMORY_SIZE - 4) {
        ctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
        ctx.r31.u64 = ctx.r4.u64;
        ctx.r30.u64 = ctx.r5.u64;
        if (ctx.r11.u32 >= 0x10000 && ctx.r11.u32 < PPC_MEMORY_SIZE - 4) {
            ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);
            ctr.u64 = ctx.r10.u64;
            if (ctr.u32 >= 0x82000000 && ctr.u32 < 0x83E00000) {
                PPCFunc* target = PPC_LOOKUP_FUNC(base, ctr.u32);
                if (target) target(ctx, base);
            }
        }
    }

    cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
    if (cr6.eq || ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 12) {
        goto loc_8351011C;
    }

    ctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
    ctx.r5.u64 = ctx.r30.u64;
    ctx.r4.u64 = ctx.r31.u64;
    if (ctx.r11.u32 >= 0x10000 && ctx.r11.u32 < PPC_MEMORY_SIZE - 12) {
        ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 8);
        ctr.u64 = ctx.r10.u64;
        if (ctr.u32 >= 0x82000000 && ctr.u32 < 0x83E00000) {
            PPCFunc* target = PPC_LOOKUP_FUNC(base, ctr.u32);
            if (target) target(ctx, base);
        }
    }

loc_8351011C:
    ctx.r1.s64 = ctx.r1.s64 + 112;
    ctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);
    ctx.r30.u64 = PPC_LOAD_U64(ctx.r1.u32 + -24);
    ctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 + -16);
    return;
}

PPC_EXTERN_FUNC(__savegprlr_28);
PPC_EXTERN_FUNC(__restgprlr_28);
PPC_EXTERN_FUNC(sub_824D5AA0);

PPC_FUNC(sub_83194A08)
{
    PPC_FUNC_PROLOGUE();
    PPCCRRegister cr6{};
    uint32_t ea{};
    __savegprlr_28(ctx, base);
    ea = -128 + ctx.r1.u32;
    PPC_STORE_U32(ea, ctx.r1.u32);
    ctx.r1.u32 = ea;
    ctx.r29.u64 = ctx.r3.u64;
    ctx.r31.u64 = ctx.r4.u64;

    cr6.compare<uint32_t>(ctx.r4.u32, 16, ctx.xer);
    if (!cr6.eq) goto loc_83194A30;

    // Out of memory across all buddy orders
    ctx.r3.s64 = -1;
    ctx.r1.s64 = ctx.r1.s64 + 128;
    __restgprlr_28(ctx, base);
    return;

loc_83194A30:
    ctx.r11.s64 = ctx.r31.s64 + 70;
    ctx.r3.u64 = ctx.r29.u64;
    ctx.r10.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 1) & 0xFFFFFFFE;
    ctx.r30.u64 = PPC_LOAD_U16(ctx.r10.u32 + ctx.r29.u32);

    // CRITICAL: 65534 (0xFFFE) and 65535 (0xFFFF) both represent empty list / no free block!
    if (ctx.r30.u32 >= 65534) goto loc_83194A60;

    ctx.r5.u64 = ctx.r31.u64;
    ctx.r4.u64 = ctx.r30.u64;
    sub_8254ABC8(ctx, base);
    ctx.r3.u64 = ctx.r30.u64;
    ctx.r1.s64 = ctx.r1.s64 + 128;
    __restgprlr_28(ctx, base);
    return;

loc_83194A60:
    ctx.r4.s64 = ctx.r31.s64 + 1;
    sub_83194A08(ctx, base);
    ctx.r30.u64 = ctx.r3.u32 & 0xFFFF;
    ctx.r28.u64 = ctx.r3.u64;

    // Higher order returned empty / out of memory: return -1
    if (ctx.r30.u32 >= 65534) {
        ctx.r3.s64 = -1;
        ctx.r1.s64 = ctx.r1.s64 + 128;
        __restgprlr_28(ctx, base);
        return;
    }

    ctx.r11.s64 = 1;
    ctx.r5.u64 = ctx.r31.u64;
    ctx.r10.u64 = ctx.r31.u8 & 0x20 ? 0 : (ctx.r11.u32 << (ctx.r31.u8 & 0x3F));
    ctx.r9.u64 = ctx.r10.u64 ^ ctx.r30.u64;
    ctx.r3.u64 = ctx.r29.u64;
    ctx.r4.u64 = ctx.r9.u32 & 0xFFFF;
    sub_824D5AA0(ctx, base);

    ctx.r11.u64 = __builtin_rotateleft64(ctx.r30.u32 | (ctx.r30.u64 << 32), 3) & 0xFFFFFFF8;
    ctx.r10.u64 = PPC_LOAD_U32(ctx.r29.u32 + 0);
    ctx.r11.u64 = ctx.r30.u64 + ctx.r11.u64;
    ctx.r11.u64 = ctx.r11.u64 + ctx.r10.u64;
    ctx.r10.s64 = ctx.r11.s64 + 4;
    ctx.r8.u64 = PPC_LOAD_U16(ctx.r11.u32 + 4);
    ctx.r8.u64 = (__builtin_rotateleft32(ctx.r31.u32, 11) & 0xFFFFF800) | (ctx.r8.u64 & 0xFFFFFFFF000007FF);
    PPC_STORE_U16(ctx.r11.u32 + 4, ctx.r8.u16);

    ctx.r3.u64 = ctx.r28.u64;
    ctx.r1.s64 = ctx.r1.s64 + 128;
    __restgprlr_28(ctx, base);
    return;
}

PPC_FUNC(sub_8245C968)
{
    uint32_t obj = ctx.r3.u32;
    if (obj < 0x10000 || obj >= PPC_MEMORY_SIZE - 4) {
        return;
    }
    uint32_t vtbl = PPC_LOAD_U32(obj + 0);
    if (vtbl < 0x10000 || vtbl >= PPC_MEMORY_SIZE - 4) {
        return;
    }
    uint32_t fn = PPC_LOAD_U32(vtbl + 0);
    if (fn < 0x82000000 || fn >= 0x83E00000) {
        LOGF_WARNING("[sub_8245C968] Invalid fn=0x{:08X} (vtbl=0x{:08X}, obj=0x{:08X})", fn, vtbl, obj);
        return;
    }
    PPCFunc* target = PPC_LOOKUP_FUNC(base, fn);
    if (!target) {
        LOGF_WARNING("[sub_8245C968] NULL host func for fn=0x{:08X} (vtbl=0x{:08X}, obj=0x{:08X})", fn, vtbl, obj);
        return;
    }
    ctx.r4.s64 = 1;
    target(ctx, base);
}

extern "C" void __imp__sub_82294200(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82294200)
{
    __imp__sub_82294200(ctx, base);
    if (ctx.r3.u32 >= 0xFFFE0000 || ctx.r3.u32 < 0x10000) {
        ctx.r3.u64 = 0;
    }
}

extern "C" void __imp__sub_8254ABC8(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_8254ABC8)
{
    uint32_t block = ctx.r4.u32 & 0xFFFF;
    if (block >= 65534) {
        // Block is empty sentinel (0xFFFE or 0xFFFF) - ignore corrupt unlinking
        return;
    }
    __imp__sub_8254ABC8(ctx, base);
}

// Depth-limited and cycle-guarded Radix Trie insertion routines (sub_834ED4D8, sub_834ED550, sub_834ED5D8)
PPC_FUNC(sub_834ED4D8)
{
    PPC_FUNC_PROLOGUE();
    PPCCRRegister cr6{};
    ctx.r11.s64 = 0;
    PPC_STORE_U32(ctx.r4.u32 + 28, ctx.r11.u32);
    PPC_STORE_U32(ctx.r4.u32 + 24, ctx.r11.u32);
    PPC_STORE_U32(ctx.r4.u32 + 20, ctx.r11.u32);
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
    cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
    if (cr6.eq) {
        PPC_STORE_U32(ctx.r3.u32 + 0, ctx.r4.u32);
        PPC_STORE_U32(ctx.r4.u32 + 20, ctx.r3.u32);
        ctx.r3.s64 = 0;
        return;
    }
    ctx.r10.u64 = ctx.r4.u64;
    ctx.r3.u64 = ctx.r11.u64;
    cr6.compare<uint32_t>(ctx.r11.u32, ctx.r4.u32, ctx.xer);
    if (cr6.eq) return;

    int iter = 0;
    while (++iter <= 32) {
        ctx.r11.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 1) & 0x1;
        ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 1) & 0xFFFFFFFE;
        ctx.r11.s64 = ctx.r11.s64 + 6;
        ctx.r9.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 2) & 0xFFFFFFFC;
        ctx.r11.u64 = PPC_LOAD_U32(ctx.r9.u32 + ctx.r3.u32);
        cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
        if (cr6.eq) {
            PPC_STORE_U32(ctx.r9.u32 + ctx.r3.u32, ctx.r4.u32);
            PPC_STORE_U32(ctx.r4.u32 + 20, ctx.r3.u32);
            ctx.r3.s64 = 0;
            return;
        }
        ctx.r3.u64 = ctx.r11.u64;
        cr6.compare<uint32_t>(ctx.r11.u32, ctx.r4.u32, ctx.xer);
        if (cr6.eq) return;
    }
    LOGF_WARNING("[sub_834ED4D8] Depth limit 32 reached! tree=0x{:08X} node=0x{:08X}", ctx.r3.u32, ctx.r4.u32);
    ctx.r3.s64 = 0;
}

PPC_FUNC(sub_834ED550)
{
    PPC_FUNC_PROLOGUE();
    PPCCRRegister cr6{};
    ctx.r11.s64 = 0;
    PPC_STORE_U32(ctx.r4.u32 + 8, ctx.r11.u32);
    PPC_STORE_U32(ctx.r4.u32 + 4, ctx.r11.u32);
    PPC_STORE_U32(ctx.r4.u32 + 0, ctx.r11.u32);
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
    cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
    if (cr6.eq) {
        PPC_STORE_U32(ctx.r3.u32 + 0, ctx.r4.u32);
        PPC_STORE_U32(ctx.r4.u32 + 0, ctx.r3.u32);
        ctx.r3.s64 = 0;
        return;
    }
    ctx.r3.u64 = ctx.r11.u64;
    ctx.r8.u64 = PPC_LOAD_U32(ctx.r4.u32 + 16);
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 16);
    ctx.r10.u64 = ctx.r8.u64;
    cr6.compare<uint32_t>(ctx.r11.u32, ctx.r8.u32, ctx.xer);
    if (cr6.eq) return;

    int iter = 0;
    uint32_t visited[64];
    while (++iter <= 48) {
        visited[iter - 1] = ctx.r3.u32;
        ctx.r11.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 1) & 0x1;
        ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 1) & 0xFFFFFFFE;
        ctx.r11.s64 = ctx.r11.s64 + 1;
        ctx.r9.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 2) & 0xFFFFFFFC;
        ctx.r11.u64 = PPC_LOAD_U32(ctx.r9.u32 + ctx.r3.u32);
        cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
        if (cr6.eq) {
            PPC_STORE_U32(ctx.r9.u32 + ctx.r3.u32, ctx.r4.u32);
            PPC_STORE_U32(ctx.r4.u32 + 0, ctx.r3.u32);
            ctx.r3.s64 = 0;
            return;
        }
        ctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + 16);
        ctx.r3.u64 = ctx.r11.u64;
        cr6.compare<uint32_t>(ctx.r9.u32, ctx.r8.u32, ctx.xer);
        if (cr6.eq) return;
    }
    LOGF_WARNING("[sub_834ED550] Depth limit reached! node=0x{:08X} key=0x{:08X} cur=0x{:08X} curKey=0x{:08X} visited[0]=0x{:08X} visited[1]=0x{:08X}",
                 ctx.r4.u32, ctx.r8.u32, ctx.r3.u32, ctx.r9.u32, visited[0], visited[1]);
    ctx.r3.s64 = 0;
}

PPC_FUNC(sub_834ED5D8)
{
    PPC_FUNC_PROLOGUE();
    PPCCRRegister cr6{};
    ctx.r11.s64 = 0;
    PPC_STORE_U32(ctx.r4.u32 + 16, ctx.r11.u32);
    PPC_STORE_U32(ctx.r4.u32 + 12, ctx.r11.u32);
    PPC_STORE_U32(ctx.r4.u32 + 8, ctx.r11.u32);
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
    cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
    if (cr6.eq) {
        PPC_STORE_U32(ctx.r3.u32 + 0, ctx.r4.u32);
        PPC_STORE_U32(ctx.r4.u32 + 8, ctx.r3.u32);
        ctx.r3.s64 = 0;
        return;
    }
    ctx.r3.u64 = ctx.r11.u64;
    ctx.r8.u64 = PPC_LOAD_U32(ctx.r4.u32 + 36);
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 36);
    ctx.r10.u64 = ctx.r8.u64;
    cr6.compare<uint32_t>(ctx.r11.u32, ctx.r8.u32, ctx.xer);
    if (cr6.eq) return;

    int iter = 0;
    while (++iter <= 32) {
        ctx.r11.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 1) & 0x1;
        ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 1) & 0xFFFFFFFE;
        ctx.r11.s64 = ctx.r11.s64 + 3;
        ctx.r9.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 2) & 0xFFFFFFFC;
        ctx.r11.u64 = PPC_LOAD_U32(ctx.r9.u32 + ctx.r3.u32);
        cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
        if (cr6.eq) {
            PPC_STORE_U32(ctx.r9.u32 + ctx.r3.u32, ctx.r4.u32);
            PPC_STORE_U32(ctx.r4.u32 + 8, ctx.r3.u32);
            ctx.r3.s64 = 0;
            return;
        }
        ctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + 36);
        ctx.r3.u64 = ctx.r11.u64;
        cr6.compare<uint32_t>(ctx.r9.u32, ctx.r8.u32, ctx.xer);
        if (cr6.eq) return;
    }
    LOGF_WARNING("[sub_834ED5D8] Depth limit 32 reached! tree=0x{:08X} node=0x{:08X} key=0x{:08X}",
                 ctx.r3.u32, ctx.r4.u32, ctx.r8.u32);
    ctx.r3.s64 = 0;
}

// Cycle-guarded spine/trie walk (sub_834F2E48)
PPC_FUNC(sub_834F2E48)
{
    PPC_FUNC_PROLOGUE();
    PPCCRRegister cr6{};
    PPCRegister temp{};

    ctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
    ctx.r3.s64 = 0;
    ctx.r7.s64 = -1;
    cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
    if (cr6.eq) return;

    ctx.r6.s64 = 0;
    ctx.r8.u64 = ctx.r4.u64;

    int iter1 = 0;
    while (++iter1 <= 32)
    {
        ctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + 16);
        ctx.r10.s64 = ctx.r4.s64 - ctx.r9.s64;
        cr6.compare<uint32_t>(ctx.r9.u32, ctx.r4.u32, ctx.xer);
        if (!cr6.gt)
        {
            cr6.compare<uint32_t>(ctx.r10.u32, ctx.r7.u32, ctx.xer);
            if (cr6.lt)
            {
                ctx.r3.u64 = ctx.r11.u64;
                ctx.r7.u64 = ctx.r10.u64;
                cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
                if (cr6.eq) return;
            }
        }
        ctx.r9.u64 = __builtin_rotateleft64(ctx.r8.u32 | (ctx.r8.u64 << 32), 1) & 0x1;
        ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);
        ctx.r9.s64 = ctx.r9.s64 + 1;
        cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
        ctx.r5.u64 = __builtin_rotateleft64(ctx.r9.u32 | (ctx.r9.u64 << 32), 2) & 0xFFFFFFFC;
        ctx.r11.u64 = PPC_LOAD_U32(ctx.r5.u32 + ctx.r11.u32);
        if (!cr6.eq && ctx.r10.u32 != ctx.r11.u32)
        {
            ctx.r6.u64 = ctx.r10.u64;
        }
        if (ctx.r11.u32 == 0)
        {
            goto loc_834F2ECC;
        }
        ctx.r8.u64 = __builtin_rotateleft64(ctx.r8.u32 | (ctx.r8.u64 << 32), 1) & 0xFFFFFFFE;
    }

loc_834F2ECC:
    ctx.r11.u64 = ctx.r6.u64;
    cr6.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
    if (cr6.eq) return;

    int iter2 = 0;
    while (++iter2 <= 64)
    {
        ctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + 16);
        ctx.r10.s64 = ctx.r4.s64 - ctx.r9.s64;
        cr6.compare<uint32_t>(ctx.r9.u32, ctx.r4.u32, ctx.xer);
        if (!cr6.gt)
        {
            cr6.compare<uint32_t>(ctx.r10.u32, ctx.r7.u32, ctx.xer);
            if (cr6.lt)
            {
                ctx.r7.u64 = ctx.r10.u64;
                ctx.r3.u64 = ctx.r11.u64;
            }
        }
        ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 8);
        ctx.xer.ca = ctx.r10.u32 > 0;
        ctx.r9.s64 = ctx.r10.s64 + -1;
        temp.u8 = (~ctx.r9.u32 + ctx.r10.u32 < ~ctx.r9.u32) | (~ctx.r9.u32 + ctx.r10.u32 + ctx.xer.ca < ctx.xer.ca);
        ctx.r10.u64 = ~ctx.r9.u64 + ctx.r10.u64 + ctx.xer.ca;
        ctx.xer.ca = temp.u8;
        ctx.r8.s64 = ctx.r10.s64 + 1;
        ctx.r6.u64 = __builtin_rotateleft64(ctx.r8.u32 | (ctx.r8.u64 << 32), 2) & 0xFFFFFFFC;
        ctx.r11.u64 = PPC_LOAD_U32(ctx.r6.u32 + ctx.r11.u32);
        cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
        if (cr6.eq) return;
    }
    LOGF_WARNING("[sub_834F2E48] Cycle detected in spine walk! Breaking cycle. result=0x{:08X}", ctx.r3.u32);
}

// Loop-guarded memory block allocation spin trap (sub_823EBC80)
PPC_FUNC(sub_823EBC80)
{
    PPC_FUNC_PROLOGUE();
    PPCCRRegister cr6{};
    uint32_t ea{};
    __savegprlr_28(ctx, base);
    ea = -128 + ctx.r1.u32;
    PPC_STORE_U32(ea, ctx.r1.u32);
    ctx.r1.u32 = ea;
    ctx.r11.s64 = 0;
    ctx.r31.u64 = ctx.r3.u64;
    ctx.r29.u64 = ctx.r4.u64;
    PPC_STORE_U8(ctx.r1.u32 + 80, ctx.r11.u8);
    ctx.r28.s64 = ctx.r3.s64 + 20;

    static std::atomic<uint64_t> s_allocTraceCount{0};
    if (s_allocTraceCount.fetch_add(1, std::memory_order_relaxed) == 0) {
        const uint32_t object = ctx.r31.u32;
        if (object >= 0x10000 && object <= UINT32_MAX - 544) {
            const uint32_t pool = PPC_LOAD_U32(object + 8);
            const bool poolValid = pool >= 0x10000 && pool <= UINT32_MAX - 20;
            LOGF_INFO("[sub_823EBC80] first allocation object=0x{:08X} size=0x{:08X} pool=0x{:08X} count={} align={} capacity=0x{:08X} used=0x{:08X} free=0x{:08X} grow=0x{:08X}",
                      object, ctx.r29.u32, pool,
                      PPC_LOAD_U32(object + 16), PPC_LOAD_U32(object + 20),
                      PPC_LOAD_U32(object + 504), PPC_LOAD_U32(object + 520),
                      PPC_LOAD_U32(object + 540), PPC_LOAD_U32(object + 544));
            if (poolValid) {
                LOGF_INFO("[sub_823EBC80] pool fields={:08X},{:08X},{:08X},{:08X},{:08X},{:08X}",
                          PPC_LOAD_U32(pool), PPC_LOAD_U32(pool + 4),
                          PPC_LOAD_U32(pool + 8), PPC_LOAD_U32(pool + 12),
                          PPC_LOAD_U32(pool + 16), PPC_LOAD_U32(pool + 20));
            }
        } else {
            LOGF_WARNING("[sub_823EBC80] invalid allocation object=0x{:08X} size=0x{:08X}",
                         object, ctx.r29.u32);
        }
    }

    int loopCount = 0;
loc_823EBCA0:
    if (++loopCount > 200) {
        LOGF_WARNING("[sub_823EBC80] Loop exceeded 200 iterations (allocator spin trap)! Breaking out.");
        ctx.r3.s64 = 0;
        ctx.r1.s64 = ctx.r1.s64 + 128;
        __restgprlr_28(ctx, base);
        return;
    }

    ctx.r5.s64 = ctx.r1.s64 + 84;
    ctx.r4.u64 = ctx.r29.u64;
    ctx.r3.u64 = ctx.r28.u64;
    sub_823EBB58(ctx, base);
    cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
    if (!cr6.eq) goto loc_823EBD24;
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 16);
    ctx.r3.u64 = ctx.r31.u64;
    ctx.r30.s64 = ctx.r11.s64 + 1;
    sub_834F0450(ctx, base);
    ctx.r6.u64 = ctx.r3.u64;
    ctx.r5.u64 = ctx.r30.u64;
    ctx.r4.u64 = ctx.r29.u64;
    ctx.r3.u64 = ctx.r31.u64;
    ctx.r7.s64 = ctx.r1.s64 + 80;
    sub_8350DDE8(ctx, base);
    cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
    if (!cr6.eq) goto loc_823EBD0C;
    ctx.r10.u64 = PPC_LOAD_U8(ctx.r1.u32 + 80);
    cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
    if (cr6.eq) goto loc_823EBD0C;
    ctx.r7.s64 = ctx.r1.s64 + 80;
    ctx.r6.u64 = PPC_LOAD_U32(ctx.r31.u32 + 504);
    ctx.r5.u64 = ctx.r30.u64;
    ctx.r4.u64 = ctx.r29.u64;
    ctx.r3.u64 = ctx.r31.u64;
    sub_8350DDE8(ctx, base);
loc_823EBD0C:
    ctx.r10.u64 = PPC_LOAD_U8(ctx.r1.u32 + 80);
    cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
    if (!cr6.eq) goto loc_823EBCA0;
    ctx.r3.s64 = 0;
    ctx.r1.s64 = ctx.r1.s64 + 128;
    __restgprlr_28(ctx, base);
    return;
loc_823EBD24:
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r1.u32 + 84);
    ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 16);
    ctx.r10.s64 = ctx.r10.s64 + 1;
    PPC_STORE_U32(ctx.r11.u32 + 16, ctx.r10.u32);
    ctx.r1.s64 = ctx.r1.s64 + 128;
    __restgprlr_28(ctx, base);
    return;
}

// Cycle-guarded BST node unlink (sub_8248EAE0)
PPC_FUNC(sub_8248EAE0)
{
    PPC_FUNC_PROLOGUE();
    PPCCRRegister cr6{};
    PPCRegister temp{};
    if (ctx.r4.u32 < 0x10000u || ctx.r4.u32 > 0xFFFFFFDCu)
    {
        LOGF_WARNING("[sub_8248EAE0] Skipping unlink with invalid node pointer 0x{:08X}", ctx.r4.u32);
        ctx.r3.u64 = 0;
        return;
    }
    // lwz r11,0(r4)
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r4.u32 + 0);
    // lwz r8,20(r4)
    ctx.r8.u64 = PPC_LOAD_U32(ctx.r4.u32 + 20);
    // cmplw cr6,r11,r4
    cr6.compare<uint32_t>(ctx.r11.u32, ctx.r4.u32, ctx.xer);
    // beq cr6,0x8248eb00
    if (cr6.eq) goto loc_8248EB00;
    // lwz r10,4(r4)
    ctx.r10.u64 = PPC_LOAD_U32(ctx.r4.u32 + 4);
    // stw r11,0(r10)
    PPC_STORE_U32(ctx.r10.u32 + 0, ctx.r11.u32);
    // stw r10,4(r11)
    PPC_STORE_U32(ctx.r11.u32 + 4, ctx.r10.u32);
    // b 0x8248eb54
    goto loc_8248EB54;
loc_8248EB00:
    // lwz r11,28(r4)
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r4.u32 + 28);
    // addi r9,r4,28
    ctx.r9.s64 = ctx.r4.s64 + 28;
    // cmplwi cr6,r11,0
    cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
    // bne cr6,0x8248eb20
    if (!cr6.eq) goto loc_8248EB20;
    // lwz r11,24(r4)
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r4.u32 + 24);
    // addi r9,r4,24
    ctx.r9.s64 = ctx.r4.s64 + 24;
    // cmplwi cr6,r11,0
    cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
    // beq cr6,0x8248eb54
    if (cr6.eq) goto loc_8248EB54;
loc_8248EB20:
    {
        uint32_t loop_guard = 0;
        while (true) {
            if (++loop_guard > 5000) {
                LOGF_ERROR("[sub_8248EAE0] CYCLE DETECTED in BST node unlink! Breaking cycle. r3=0x{:08X} r4=0x{:08X} r11=0x{:08X}",
                           ctx.r3.u32, ctx.r4.u32, ctx.r11.u32);
                PPC_STORE_U32(ctx.r9.u32 + 0, 0);
                goto loc_8248EB54;
            }
            // lwz r7,28(r11)
            ctx.r7.u64 = PPC_LOAD_U32(ctx.r11.u32 + 28);
            // addi r10,r11,28
            ctx.r10.s64 = ctx.r11.s64 + 28;
            // cmplwi cr6,r7,0
            cr6.compare<uint32_t>(ctx.r7.u32, 0, ctx.xer);
            // bne cr6,0x8248eb40
            if (!cr6.eq) {
                ctx.r11.u64 = PPC_LOAD_U32(ctx.r10.u32 + 0);
                ctx.r9.u64 = ctx.r10.u64;
                continue;
            }
            // lwz r7,24(r11)
            ctx.r7.u64 = PPC_LOAD_U32(ctx.r11.u32 + 24);
            // addi r10,r11,24
            ctx.r10.s64 = ctx.r11.s64 + 24;
            // cmplwi cr6,r7,0
            cr6.compare<uint32_t>(ctx.r7.u32, 0, ctx.xer);
            // beq cr6,0x8248eb4c
            if (cr6.eq) {
                ctx.r10.s64 = 0;
                PPC_STORE_U32(ctx.r9.u32 + 0, ctx.r10.u32);
                break;
            }
            ctx.r11.u64 = PPC_LOAD_U32(ctx.r10.u32 + 0);
            ctx.r9.u64 = ctx.r10.u64;
        }
    }
loc_8248EB54:
    // cmplwi cr6,r8,0
    cr6.compare<uint32_t>(ctx.r8.u32, 0, ctx.xer);
    // beqlr cr6
    if (cr6.eq) return;
    // lwz r10,32(r4)
    ctx.r10.u64 = PPC_LOAD_U32(ctx.r4.u32 + 32);
    // addi r10,r10,1
    ctx.r10.s64 = ctx.r10.s64 + 1;
    // rlwinm r10,r10,2,0,29
    ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 2) & 0xFFFFFFFC;
    // lwzx r9,r10,r3
    ctx.r9.u64 = PPC_LOAD_U32(ctx.r10.u32 + ctx.r3.u32);
    // cmplw cr6,r4,r9
    cr6.compare<uint32_t>(ctx.r4.u32, ctx.r9.u32, ctx.xer);
    // bne cr6,0x8248eb9c
    if (!cr6.eq) goto loc_8248EB9C;
    // stwx r11,r10,r3
    PPC_STORE_U32(ctx.r10.u32 + ctx.r3.u32, ctx.r11.u32);
    // cmplwi cr6,r11,0
    cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
    // bne cr6,0x8248ebc0
    if (!cr6.eq) goto loc_8248EBC0;
    // lwz r11,32(r4)
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r4.u32 + 32);
    // li r10,1
    ctx.r10.s64 = 1;
    // lwz r9,0(r3)
    ctx.r9.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
    // slw r8,r10,r11
    ctx.r8.u64 = ctx.r11.u8 & 0x20 ? 0 : (ctx.r10.u32 << (ctx.r11.u8 & 0x3F));
    // andc r7,r9,r8
    ctx.r7.u64 = ctx.r9.u64 & ~ctx.r8.u64;
    // stw r7,0(r3)
    PPC_STORE_U32(ctx.r3.u32 + 0, ctx.r7.u32);
    // blr 
    return;
loc_8248EB9C:
    // lwz r10,24(r8)
    ctx.r10.u64 = PPC_LOAD_U32(ctx.r8.u32 + 24);
    // cmplwi cr6,r11,0
    cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
    // subf r9,r10,r4
    ctx.r9.s64 = ctx.r4.s64 - ctx.r10.s64;
    // addic r7,r9,-1
    ctx.xer.ca = ctx.r9.u32 > 0;
    ctx.r7.s64 = ctx.r9.s64 + -1;
    // subfe r10,r7,r9
    temp.u8 = (~ctx.r7.u32 + ctx.r9.u32 < ~ctx.r7.u32) | (~ctx.r7.u32 + ctx.r9.u32 + ctx.xer.ca < ctx.xer.ca);
    ctx.r10.u64 = ~ctx.r7.u64 + ctx.r9.u64 + ctx.xer.ca;
    ctx.xer.ca = temp.u8;
    // addi r6,r10,6
    ctx.r6.s64 = ctx.r10.s64 + 6;
    // rlwinm r5,r6,2,0,29
    ctx.r5.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 2) & 0xFFFFFFFC;
    // stwx r11,r5,r8
    PPC_STORE_U32(ctx.r5.u32 + ctx.r8.u32, ctx.r11.u32);
    // beqlr cr6
    if (cr6.eq) return;
loc_8248EBC0:
    // stw r8,20(r11)
    PPC_STORE_U32(ctx.r11.u32 + 20, ctx.r8.u32);
    // lwz r10,24(r4)
    ctx.r10.u64 = PPC_LOAD_U32(ctx.r4.u32 + 24);
    // cmplwi cr6,r10,0
    cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
    // beq cr6,0x8248ebd8
    if (cr6.eq) goto loc_8248EBD8;
    // stw r10,24(r11)
    PPC_STORE_U32(ctx.r11.u32 + 24, ctx.r10.u32);
    // stw r11,20(r10)
    PPC_STORE_U32(ctx.r10.u32 + 20, ctx.r11.u32);
loc_8248EBD8:
    // lwz r10,28(r4)
    ctx.r10.u64 = PPC_LOAD_U32(ctx.r4.u32 + 28);
    // cmplwi cr6,r10,0
    cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
    // beqlr cr6
    if (cr6.eq) return;
    // stw r10,28(r11)
    PPC_STORE_U32(ctx.r11.u32 + 28, ctx.r10.u32);
    // stw r11,20(r10)
    PPC_STORE_U32(ctx.r10.u32 + 20, ctx.r11.u32);
    return;
}

// Guarded implementation of sub_8351A9E8 (destructor / callback dispatcher)
PPC_FUNC(sub_8351A9E8)
{
    PPC_FUNC_PROLOGUE();
    uint32_t ea = -96 + ctx.r1.u32;
    PPC_STORE_U32(ctx.r1.u32 - 8, ctx.r12.u32);
    PPC_STORE_U64(ctx.r1.u32 - 16, ctx.r31.u64);
    PPC_STORE_U32(ea, ctx.r1.u32);
    ctx.r1.u32 = ea;

    if (ctx.r3.u32 >= 0x10000 && ctx.r3.u32 < PPC_MEMORY_SIZE - 24) {
        ctx.r31.u64 = PPC_LOAD_U32(ctx.r3.u32 + 20);
        if (ctx.r31.u32 != 0 && ctx.r31.u32 >= 0x10000 && ctx.r31.u32 < PPC_MEMORY_SIZE - 24) {
            uint32_t r31_val = ctx.r31.u32;
            PPC_STORE_U32(r31_val + 12, ctx.r4.u32);
            ctx.r4.s64 = (int64_t)r31_val + 16;
            PPC_STORE_U32(r31_val + 20, r31_val);

            uint32_t obj = PPC_LOAD_U32(r31_val + 8);
            if (obj >= 0x10000 && obj < PPC_MEMORY_SIZE - 4) {
                uint32_t vtbl1 = PPC_LOAD_U32(obj + 0);
                if (vtbl1 >= 0x10000 && vtbl1 < PPC_MEMORY_SIZE - 24) {
                    uint32_t fn1 = PPC_LOAD_U32(vtbl1 + 20);
                    if (fn1 >= 0x82000000 && fn1 < 0x83E00000) {
                        ctx.r3.u64 = obj;
                        ctx.r10.u64 = fn1;
                        PPCFunc* target1 = PPC_LOOKUP_FUNC(base, fn1);
                        if (target1) {
                            target1(ctx, base);
                        }
                    } else {
                        LOGF_WARNING("[sub_8351A9E8] Skipping invalid indirect call fn1=0x{:08X} (vtbl1=0x{:08X}, obj=0x{:08X})", fn1, vtbl1, obj);
                    }
                }

                // Second indirect call
                uint32_t vtbl2 = PPC_LOAD_U32(obj + 0);
                if (vtbl2 >= 0x10000 && vtbl2 < PPC_MEMORY_SIZE - 28) {
                    uint32_t fn2 = PPC_LOAD_U32(vtbl2 + 24);
                    if (fn2 >= 0x82000000 && fn2 < 0x83E00000) {
                        ctx.r3.u64 = obj;
                        ctx.r4.u64 = PPC_LOAD_U32(r31_val + 12);
                        ctx.r8.u64 = fn2;
                        PPCFunc* target2 = PPC_LOOKUP_FUNC(base, fn2);
                        if (target2) {
                            target2(ctx, base);
                        }
                    } else {
                        LOGF_WARNING("[sub_8351A9E8] Skipping invalid indirect call fn2=0x{:08X} (vtbl2=0x{:08X}, obj=0x{:08X})", fn2, vtbl2, obj);
                    }
                }
            }
        }
    }

    ctx.r1.s64 = ctx.r1.s64 + 96;
    ctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 - 8);
    ctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 - 16);
}

PPC_EXTERN_FUNC(sub_834FB050);
PPC_EXTERN_FUNC(sub_834FCA30);

// Guarded implementation of sub_83508578 (memory allocator unlinking & callback)
PPC_FUNC(sub_83508578)
{
    PPC_FUNC_PROLOGUE();
    PPCRegister ctr{};
    PPCCRRegister cr6{};
    uint32_t ea = -112 + ctx.r1.u32;
    __savegprlr_29(ctx, base);
    PPC_STORE_U32(ea, ctx.r1.u32);
    ctx.r1.u32 = ea;

    ctx.r30.u64 = ctx.r3.u64;
    ctx.r31.u64 = ctx.r4.u64;
    ctx.r29.u64 = (ctx.r4.u32 >= 0x10000 && ctx.r4.u32 < PPC_MEMORY_SIZE - 28) ? PPC_LOAD_U32(ctx.r4.u32 + 24) : 0;

    if (ctx.r30.u32 >= 0x10000 && ctx.r30.u32 < PPC_MEMORY_SIZE - 548) {
        ctx.r3.u64 = PPC_LOAD_U32(ctx.r30.u32 + 544);
        cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
        if (!cr6.eq && ctx.r3.u32 >= 0x10000 && ctx.r3.u32 < PPC_MEMORY_SIZE - 4) {
            ctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
            ctx.r5.u64 = ctx.r29.u64;
            ctx.r4.u64 = PPC_LOAD_U32(ctx.r30.u32 + 0);
            if (ctx.r11.u32 >= 0x10000 && ctx.r11.u32 < PPC_MEMORY_SIZE - 12) {
                ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 8);
                ctr.u64 = ctx.r10.u64;
                if (ctr.u32 >= 0x82000000 && ctr.u32 < 0x83E00000) {
                    PPCFunc* target = PPC_LOOKUP_FUNC(base, ctr.u32);
                    if (target) target(ctx, base);
                }
            }
        }

        ctx.r11.u64 = PPC_LOAD_U32(ctx.r30.u32 + 520);
        ctx.r10.s64 = -2083782656; // 0x83CC0000
        ctx.r5.u64 = ctx.r29.u64;
        ctx.r9.s64 = ctx.r11.s64 - ctx.r29.s64;
        PPC_STORE_U32(ctx.r30.u32 + 520, ctx.r9.u32);

        ctx.r4.u64 = (ctx.r31.u32 >= 0x10000 && ctx.r31.u32 < PPC_MEMORY_SIZE - 32) ? PPC_LOAD_U32(ctx.r31.u32 + 28) : 0;
        ctx.r3.u64 = PPC_LOAD_U32(ctx.r10.u32 + -40);
        sub_834FB050(ctx, base);

        ctx.r7.u64 = (ctx.r31.u32 >= 0x10000 && ctx.r31.u32 < PPC_MEMORY_SIZE - 20) ? PPC_LOAD_U32(ctx.r31.u32 + 16) : 0;
        ctx.r6.u64 = (ctx.r31.u32 >= 0x10000 && ctx.r31.u32 < PPC_MEMORY_SIZE - 16) ? PPC_LOAD_U16(ctx.r31.u32 + 14) : 0;
        ctx.r8.s64 = 1;
        ctx.r5.u64 = __builtin_rotateleft64(ctx.r7.u32 | (ctx.r7.u64 << 32), 0) & 0x80000000;
        ctx.r6.u64 = ctx.r6.u8 & 0x20 ? 0 : (ctx.r8.u32 << (ctx.r6.u8 & 0x3F));
        cr6.compare<uint32_t>(ctx.r5.u32, 0, ctx.xer);
        if (cr6.eq) {
            cr6.compare<uint32_t>(ctx.r6.u32, 4096, ctx.xer);
            if (!cr6.gt) {
                ctx.r6.s64 = 4096;
            }
            ctx.r3.u64 = PPC_LOAD_U32(ctx.r30.u32 + 4);
            ctx.r5.u64 = ctx.r29.u64;
            ctx.r4.u64 = (ctx.r31.u32 >= 0x10000 && ctx.r31.u32 < PPC_MEMORY_SIZE - 32) ? PPC_LOAD_U32(ctx.r31.u32 + 28) : 0;
            if (ctx.r3.u32 >= 0x10000 && ctx.r3.u32 < PPC_MEMORY_SIZE - 4) {
                ctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
                if (ctx.r11.u32 >= 0x10000 && ctx.r11.u32 < PPC_MEMORY_SIZE - 24) {
                    ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 20);
                    ctr.u64 = ctx.r10.u64;
                    if (ctr.u32 >= 0x82000000 && ctr.u32 < 0x83E00000) {
                        PPCFunc* target = PPC_LOOKUP_FUNC(base, ctr.u32);
                        if (target) target(ctx, base);
                    }
                }
            }
        } else {
            ctx.r11.u64 = __builtin_rotateleft32(ctx.r7.u32, 0);
            ctx.r10.u64 = PPC_LOAD_U32(ctx.r30.u32 + 520);
            ctx.r3.u64 = PPC_LOAD_U32(ctx.r30.u32 + 4);
            ctx.r9.u64 = ctx.r11.u32 & 0x7FFFFFFF;
            ctx.r8.s64 = ctx.r10.s64 - ctx.r9.s64;
            PPC_STORE_U32(ctx.r30.u32 + 520, ctx.r8.u32);
            if (ctx.r3.u32 >= 0x10000 && ctx.r3.u32 < PPC_MEMORY_SIZE - 4) {
                ctx.r5.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
                ctx.r4.u64 = (ctx.r31.u32 >= 0x10000 && ctx.r31.u32 < PPC_MEMORY_SIZE - 20) ? PPC_LOAD_U32(ctx.r31.u32 + 16) : 0;
                ctx.r11.u64 = ctx.r4.u32 & 0x7FFFFFFF;
                ctx.r7.u64 = (ctx.r31.u32 >= 0x10000 && ctx.r31.u32 < PPC_MEMORY_SIZE - 32) ? PPC_LOAD_U32(ctx.r31.u32 + 28) : 0;
                if (ctx.r5.u32 >= 0x10000 && ctx.r5.u32 < PPC_MEMORY_SIZE - 36) {
                    ctx.r10.u64 = PPC_LOAD_U32(ctx.r5.u32 + 32);
                    ctx.r5.u64 = ctx.r11.u64 + ctx.r29.u64;
                    ctx.r4.s64 = ctx.r7.s64 - ctx.r11.s64;
                    ctr.u64 = ctx.r10.u64;
                    if (ctr.u32 >= 0x82000000 && ctr.u32 < 0x83E00000) {
                        PPCFunc* target = PPC_LOOKUP_FUNC(base, ctr.u32);
                        if (target) target(ctx, base);
                    }
                }
            }
        }
    }

    if (ctx.r31.u32 >= 0x10000 && ctx.r31.u32 < PPC_MEMORY_SIZE - 12) {
        ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
        ctx.r4.u64 = ctx.r31.u64;
        ctx.r10.u64 = PPC_LOAD_U32(ctx.r31.u32 + 4);
        if (ctx.r11.u32 >= 0x10000 && ctx.r11.u32 < PPC_MEMORY_SIZE - 8) {
            PPC_STORE_U32(ctx.r11.u32 + 4, ctx.r10.u32);
        }
        ctx.r9.u64 = PPC_LOAD_U32(ctx.r31.u32 + 4);
        ctx.r8.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
        if (ctx.r9.u32 >= 0x10000 && ctx.r9.u32 < PPC_MEMORY_SIZE - 4) {
            PPC_STORE_U32(ctx.r9.u32 + 0, ctx.r8.u32);
        }
        ctx.r5.u64 = PPC_LOAD_U32(ctx.r31.u32 + 8);
        ctx.r3.u64 = (ctx.r30.u32 >= 0x10000 && ctx.r30.u32 < PPC_MEMORY_SIZE - 12) ? PPC_LOAD_U32(ctx.r30.u32 + 8) : 0;
        sub_834FCA30(ctx, base);
    }

    ctx.r1.s64 = ctx.r1.s64 + 112;
    __restgprlr_29(ctx, base);
}

PPC_EXTERN_FUNC(__savegprlr_27);
PPC_EXTERN_FUNC(__restgprlr_27);
PPC_EXTERN_FUNC(sub_83549B08);
PPC_EXTERN_FUNC(sub_83548870);
PPC_EXTERN_FUNC(sub_82400C88);
PPC_EXTERN_FUNC(sub_83523A10);
PPC_EXTERN_FUNC(sub_835238D8);

// Guarded implementation of sub_83549B08 (hash table growth check)
PPC_FUNC(sub_83549B08)
{
    PPC_FUNC_PROLOGUE();
    PPCCRRegister cr6{};
    if (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 4) return;
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
    if (ctx.r11.u32 == 0) {
        ctx.r5.s64 = 4;
        sub_83548870(ctx, base);
        return;
    }
    if (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 8) return;
    uint32_t count = PPC_LOAD_U32(ctx.r11.u32 + 0);
    uint32_t mask = PPC_LOAD_U32(ctx.r11.u32 + 4);
    uint32_t cap = mask + 1;
    if (count >= cap || (count * 5 > cap * 4)) {
        ctx.r5.u64 = (uint64_t)cap * 2;
        if (ctx.r5.u32 < 4) ctx.r5.u32 = 4;
        sub_83548870(ctx, base);
    }
}

// Guarded implementation of sub_83532920 (hash table insertion with loop bounding)
PPC_FUNC(sub_83532920)
{
    PPC_FUNC_PROLOGUE();
    PPCCRRegister cr6{};
    uint32_t ea = -128 + ctx.r1.u32;
    __savegprlr_27(ctx, base);
    PPC_STORE_U32(ea, ctx.r1.u32);
    ctx.r1.u32 = ea;

    ctx.r31.u64 = ctx.r3.u64;
    ctx.r27.u64 = ctx.r5.u64;
    ctx.r30.u64 = ctx.r6.u64;

    sub_83549B08(ctx, base);

    if (ctx.r31.u32 < 0x10000 || ctx.r31.u32 >= PPC_MEMORY_SIZE - 4) {
        ctx.r1.s64 = ctx.r1.s64 + 128;
        __restgprlr_27(ctx, base);
        return;
    }

    ctx.r10.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
    if (ctx.r10.u32 < 0x10000 || ctx.r10.u32 >= PPC_MEMORY_SIZE - 8) {
        ctx.r1.s64 = ctx.r1.s64 + 128;
        __restgprlr_27(ctx, base);
        return;
    }

    ctx.r9.u64 = PPC_LOAD_U32(ctx.r10.u32 + 4);
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r10.u32 + 0);
    ctx.r30.u64 = ctx.r9.u64 & ctx.r30.u64;
    ctx.r8.s64 = ctx.r11.s64 + 1;
    ctx.r11.u64 = __builtin_rotateleft64(ctx.r30.u32 | (ctx.r30.u64 << 32), 1) & 0xFFFFFFFE;
    PPC_STORE_U32(ctx.r10.u32 + 0, ctx.r8.u32);
    ctx.r7.u64 = ctx.r30.u64 + ctx.r11.u64;
    ctx.r10.u64 = __builtin_rotateleft64(ctx.r7.u32 | (ctx.r7.u64 << 32), 2) & 0xFFFFFFFC;
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
    ctx.r10.u64 = ctx.r10.u64 + ctx.r11.u64;
    ctx.r6.u64 = PPC_LOAD_U32(ctx.r10.u32 + 8);
    ctx.r28.s64 = ctx.r10.s64 + 8;
    cr6.compare<int32_t>(ctx.r6.s32, -2, ctx.xer);
    if (!cr6.eq) goto loc_835329A4;

    ctx.r11.s64 = -1;
    PPC_STORE_U32(ctx.r28.u32 + 0, ctx.r11.u32);
    ctx.r3.u64 = PPC_LOAD_U32(ctx.r27.u32 + 0);
    cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
    if (!cr6.eq) {
        sub_82400C88(ctx, base);
    }
loc_83532990:
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r27.u32 + 0);
    PPC_STORE_U32(ctx.r28.u32 + 8, ctx.r11.u32);
    PPC_STORE_U32(ctx.r28.u32 + 4, ctx.r30.u32);
    ctx.r1.s64 = ctx.r1.s64 + 128;
    __restgprlr_27(ctx, base);
    return;

loc_835329A4:
    ctx.r29.u64 = ctx.r30.u64;
    {
        uint32_t mask = (ctx.r11.u32 >= 0x10000 && ctx.r11.u32 < PPC_MEMORY_SIZE - 8) ? PPC_LOAD_U32(ctx.r11.u32 + 4) : 0;
        uint32_t max_probe = (mask + 1) * 2 + 16;
        uint32_t probe_count = 0;
        while (true) {
            if (++probe_count > max_probe) {
                LOGF_WARNING("[sub_83532920] Table full! Linear probe exceeded {} iters (mask={})", probe_count, mask);
                ctx.r1.s64 = ctx.r1.s64 + 128;
                __restgprlr_27(ctx, base);
                return;
            }
            ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);
            ctx.r9.s64 = ctx.r29.s64 + 1;
            ctx.r29.u64 = ctx.r10.u64 & ctx.r9.u64;
            ctx.r10.u64 = __builtin_rotateleft64(ctx.r29.u32 | (ctx.r29.u64 << 32), 1) & 0xFFFFFFFE;
            ctx.r8.u64 = ctx.r29.u64 + ctx.r10.u64;
            ctx.r10.u64 = __builtin_rotateleft64(ctx.r8.u32 | (ctx.r8.u64 << 32), 2) & 0xFFFFFFFC;
            ctx.r10.u64 = ctx.r10.u64 + ctx.r11.u64;
            ctx.r3.s64 = ctx.r10.s64 + 8;
            ctx.r7.u64 = PPC_LOAD_U32(ctx.r10.u32 + 8);
            cr6.compare<int32_t>(ctx.r7.s32, -2, ctx.xer);
            if (cr6.eq) break;
        }
    }

    ctx.r10.u64 = PPC_LOAD_U32(ctx.r28.u32 + 4);
    cr6.compare<uint32_t>(ctx.r10.u32, ctx.r30.u32, ctx.xer);
    if (!cr6.eq) goto loc_83532A0C;
    cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
    if (!cr6.eq) {
        ctx.r4.u64 = ctx.r28.u64;
        sub_83523A10(ctx, base);
    }
loc_835329F0:
    ctx.r3.s64 = ctx.r28.s64 + 8;
    ctx.r4.u64 = ctx.r27.u64;
    sub_835238D8(ctx, base);
    PPC_STORE_U32(ctx.r28.u32 + 0, ctx.r29.u32);
    PPC_STORE_U32(ctx.r28.u32 + 4, ctx.r30.u32);
    ctx.r1.s64 = ctx.r1.s64 + 128;
    __restgprlr_27(ctx, base);
    return;

loc_83532A0C:
    ctx.r9.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 1) & 0xFFFFFFFE;
    ctx.r10.u64 = ctx.r10.u64 + ctx.r9.u64;
    ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 2) & 0xFFFFFFFC;
    ctx.r10.u64 = ctx.r10.u64 + ctx.r11.u64;
    ctx.r31.s64 = ctx.r10.s64 + 8;
    ctx.r9.u64 = PPC_LOAD_U32(ctx.r10.u32 + 8);
    cr6.compare<int32_t>(ctx.r9.s32, ctx.r30.s32, ctx.xer);
    if (cr6.eq) goto loc_83532A50;

    {
        uint32_t chain_count = 0;
        while (true) {
            if (++chain_count > 64) {
                LOGF_WARNING("[sub_83532920] Collision chain cycle detected! iters={}", chain_count);
                ctx.r1.s64 = ctx.r1.s64 + 128;
                __restgprlr_27(ctx, base);
                return;
            }
            ctx.r10.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
            ctx.r9.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 1) & 0xFFFFFFFE;
            ctx.r10.u64 = ctx.r10.u64 + ctx.r9.u64;
            ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 2) & 0xFFFFFFFC;
            ctx.r10.u64 = ctx.r10.u64 + ctx.r11.u64;
            ctx.r31.s64 = ctx.r10.s64 + 8;
            ctx.r9.u64 = PPC_LOAD_U32(ctx.r10.u32 + 8);
            cr6.compare<int32_t>(ctx.r9.s32, ctx.r30.s32, ctx.xer);
            if (cr6.eq) break;
        }
    }

loc_83532A50:
    cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
    if (!cr6.eq) {
        ctx.r4.u64 = ctx.r28.u64;
        sub_83523A10(ctx, base);
    }
loc_83532A60:
    PPC_STORE_U32(ctx.r31.u32 + 0, ctx.r29.u32);
    ctx.r3.s64 = ctx.r28.s64 + 8;
    ctx.r4.u64 = ctx.r27.u64;
    sub_835238D8(ctx, base);
    ctx.r11.s64 = -1;
    PPC_STORE_U32(ctx.r28.u32 + 4, ctx.r30.u32);
    PPC_STORE_U32(ctx.r28.u32 + 0, ctx.r11.u32);
    ctx.r1.s64 = ctx.r1.s64 + 128;
    __restgprlr_27(ctx, base);
}

// Guarded implementation of sub_8266BB00 (display / window listener getter)
PPC_FUNC(sub_8266BB00)
{
    static uint32_t s_dummyObj = 0;
    auto get_dummy = [&]() -> uint32_t {
        if (s_dummyObj == 0) {
            void* objPtr = g_userHeap.Alloc(512);
            memset(objPtr, 0, 512);
            s_dummyObj = g_memory.MapVirtual(objPtr);

            void* vtblPtr = g_userHeap.Alloc(1024);
            memset(vtblPtr, 0, 1024);
            uint32_t dummyVtbl = g_memory.MapVirtual(vtblPtr);

            PPC_STORE_U32(s_dummyObj, dummyVtbl);
            // Initialize vtable entries (256 methods) with sub_83549B4C (which is 'blr')
            for (int i = 0; i < 256; ++i) {
                PPC_STORE_U32(dummyVtbl + i * 4, 0x83549B4C);
            }
            LOGF_INFO("[sub_8266BB00] Initialized dummy listener object at 0x{:08X} (vtbl 0x{:08X})", s_dummyObj, dummyVtbl);
        }
        return s_dummyObj;
    };

    if (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 40) {
        ctx.r3.u64 = get_dummy();
        return;
    }

    uint32_t val = PPC_LOAD_U32(ctx.r3.u32 + 36);
    if (val >= 0x10000 && val < PPC_MEMORY_SIZE - 256) {
        uint32_t vtbl = PPC_LOAD_U32(val + 0);
        if (vtbl >= 0x10000 && vtbl < PPC_MEMORY_SIZE - 220 && (vtbl & 3) == 0) {
            uint32_t target = PPC_LOAD_U32(vtbl + 216);
            if (target >= PPC_CODE_BASE &&
                target < PPC_CODE_BASE + PPC_CODE_SIZE &&
                PPC_LOOKUP_FUNC(base, target) != nullptr) {
                ctx.r3.u64 = val;
                return;
            }
            static bool loggedInvalidTarget = false;
            if (!loggedInvalidTarget) {
                LOGF_INFO("[sub_8266BB00] Replacing invalid listener vtable target 0x{:08X} (object 0x{:08X}, vtable 0x{:08X})",
                    target, val, vtbl);
                loggedInvalidTarget = true;
            }
        } else {
            static bool loggedInvalidVtable = false;
            if (!loggedInvalidVtable) {
                LOGF_INFO("[sub_8266BB00] Replacing invalid listener vtable 0x{:08X} (object 0x{:08X})",
                    vtbl, val);
                loggedInvalidVtable = true;
            }
        }
    }

    ctx.r3.u64 = get_dummy();
}

// RAGE State transition function (sub_8229AD78)
PPC_FUNC(sub_8229AD78)
{
    uint32_t obj = ctx.r3.u32;
    if (obj < 0x10000 || obj >= PPC_MEMORY_SIZE - 20) {
        obj = g_lastStateMachineObj.load();
        if (obj < 0x10000) obj = 0x83887150;
        ctx.r3.u64 = obj;
    } else {
        g_lastStateMachineObj.store(obj);
    }
    uint16_t oldState = (obj >= 0x10000 && obj < PPC_MEMORY_SIZE - 16) ? PPC_LOAD_U16(obj + 14) : 0;
    if (obj >= 0x10000 && obj < PPC_MEMORY_SIZE - 16) {
        PPC_STORE_U16(obj + 14, ctx.r4.u16);
        if (ctx.r4.u16 == 2) {
            PPC_STORE_U16(obj + 16, 0xFFFF);
            KernelPhase_EnterRuntime();
        }
    }
    LOGF_WARNING("===============================================================");
    LOGF_WARNING("[RAGE_STATE] Transition: State {} -> State {} (obj=0x{:08X})", oldState, ctx.r4.u16, obj);
    LOGF_WARNING("===============================================================");
}

extern "C" PPC_FUNC(__imp__sub_829855F8);
PPC_FUNC(sub_829855F8)
{
    if (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 32) {
        ctx.r3.u64 = 0;
        return;
    }
    __imp__sub_829855F8(ctx, base);
}

extern "C" PPC_FUNC(__imp__sub_82985550);
PPC_FUNC(sub_82985550)
{
    if (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 32) {
        ctx.r3.u64 = 0;
        return;
    }
    __imp__sub_82985550(ctx, base);
}

extern "C" PPC_FUNC(__imp__sub_831E2DB8);
PPC_FUNC(sub_831E2DB8)
{
    if (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 32) {
        ctx.r3.s64 = -1;
        return;
    }
    uint32_t obj = PPC_LOAD_U32(ctx.r3.u32 + 0);
    if (obj < 0x10000 || obj >= PPC_MEMORY_SIZE - 4) {
        ctx.r3.s64 = -1;
        return;
    }
    uint32_t vtbl = PPC_LOAD_U32(obj + 0);
    if (vtbl < 0x10000 || vtbl >= PPC_MEMORY_SIZE - 28) {
        ctx.r3.s64 = -1;
        return;
    }
    uint32_t fn = PPC_LOAD_U32(vtbl + 24);
    if (fn < 0x82000000 || fn >= 0x83E00000) {
        ctx.r3.s64 = -1;
        return;
    }
    __imp__sub_831E2DB8(ctx, base);
}

extern "C" PPC_FUNC(__imp__sub_829856D0);
PPC_FUNC(sub_829856D0)
{
    if (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 32) {
        ctx.r3.u64 = 0;
        return;
    }
    __imp__sub_829856D0(ctx, base);
}

PPC_EXTERN_FUNC(sub_83210AF0);
extern "C" PPC_FUNC(__imp__sub_82924148);
PPC_FUNC(sub_82924148)
{
    PPCContext savedCtx = ctx;

    uint32_t dictAddr = (uint32_t)-2087321600 - 30388; // 0x838A894C
    uint32_t dict = (dictAddr >= 0x10000 && dictAddr < PPC_MEMORY_SIZE - 4) ? PPC_LOAD_U32(dictAddr) : 0;
    if (dict == 0) {
        ctx.r3.s64 = 0;
        return;
    }

    // Call sub_823C0348 to get hash of name (r4)
    ctx.r3.u64 = ctx.r4.u64;
    ctx.r4.s64 = 0;
    sub_823C0348(ctx, base);
    uint32_t hash = ctx.r3.u32;

    // Call sub_83210AF0(dict, hash)
    ctx.r3.u64 = dict;
    ctx.r4.u64 = hash;
    sub_83210AF0(ctx, base);
    uint32_t entry = ctx.r3.u32;

    // Validate the entry and its nested pointers
    bool valid = false;
    if (entry >= 0x10000 && entry < PPC_MEMORY_SIZE - 68) {
        uint32_t r10 = PPC_LOAD_U32(entry + 64);
        if (r10 >= 0x10000 && r10 < PPC_MEMORY_SIZE - 4) {
            uint32_t r9 = PPC_LOAD_U32(r10);
            if (r9 >= 0x10000 && r9 < PPC_MEMORY_SIZE - 4) {
                uint32_t r3_obj = PPC_LOAD_U32(r9);
                if (r3_obj >= 0x10000 && r3_obj < PPC_MEMORY_SIZE - 8) {
                    uint32_t r11_arr = PPC_LOAD_U32(r3_obj + 4);
                    if (r11_arr >= 0x10000 && r11_arr < PPC_MEMORY_SIZE - 4) {
                        uint32_t item = PPC_LOAD_U32(r11_arr);
                        if (item >= 0x10000 && item < PPC_MEMORY_SIZE - 4) {
                            uint32_t vtbl = PPC_LOAD_U32(item);
                            if (vtbl >= 0x10000 && vtbl < PPC_MEMORY_SIZE - 52) {
                                uint32_t method = PPC_LOAD_U32(vtbl + 48);
                                if (method >= 0x82000000 && method < 0x83E00000) {
                                    valid = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    ctx = savedCtx;

    if (valid) {
        __imp__sub_82924148(ctx, base);
    } else {
        ctx.r3.s64 = 0;
    }
}

extern "C" PPC_FUNC(__imp__sub_82D95030);
PPC_FUNC(sub_82D95030)
{
    if (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 16 || (ctx.r3.u32 & 3) != 0) {
        return;
    }
    uint32_t obj_struct = ctx.r3.u32;
    uint16_t count = PPC_LOAD_U16(obj_struct + 8);
    uint32_t arr = PPC_LOAD_U32(obj_struct + 4);
    if (count == 0 || arr < 0x10000 || arr >= PPC_MEMORY_SIZE - (count * 8ULL)) {
        PPC_STORE_U16(obj_struct + 8, 0);
        PPC_STORE_U8(obj_struct + 0, 1);
        return;
    }

    // Sanitize any corrupt callback entries so bctrl doesn't explode
    for (uint32_t i = 0; i < count; i++) {
        uint32_t item_obj = PPC_LOAD_U32(arr + i * 8 + 4);
        if (item_obj != 0) {
            if (item_obj < 0x10000 || item_obj >= PPC_MEMORY_SIZE - 4 || (item_obj & 3) != 0) {
                PPC_STORE_U32(arr + i * 8 + 4, 0);
                continue;
            }
            uint32_t vtbl = PPC_LOAD_U32(item_obj);
            if (vtbl < 0x82000000 || vtbl >= PPC_MEMORY_SIZE - 4 || (vtbl & 3) != 0) {
                PPC_STORE_U32(arr + i * 8 + 4, 0);
                continue;
            }
            uint32_t func = PPC_LOAD_U32(vtbl);
            if (func < PPC_CODE_BASE || func >= (PPC_CODE_BASE + PPC_CODE_SIZE)) {
                PPC_STORE_U32(arr + i * 8 + 4, 0);
                continue;
            }
        }
    }

    __imp__sub_82D95030(ctx, base);
}

extern "C" PPC_FUNC(__imp__sub_83521D38);
PPC_FUNC(sub_83521D38)
{
    if (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 4 || (ctx.r3.u32 & 3) != 0) {
        return;
    }
    uint32_t r31 = ctx.r3.u32;
    uint32_t buf = PPC_LOAD_U32(r31);
    if (buf == 0 || buf < 0x10000 || buf >= PPC_MEMORY_SIZE - 8) {
        return;
    }
    uint32_t raw_count = PPC_LOAD_U32(buf + 4);
    // If raw_count == 0xFFFFFFFF (-1), count is 0; avoids mtctr 0 -> bdnz 4-billion-iteration underflow!
    if (raw_count != 0xFFFFFFFF && raw_count < 100000) {
        uint32_t count = raw_count + 1;
        for (uint32_t i = 0; i < count; i++) {
            uint32_t elem = buf + i * 24;
            if (elem < PPC_MEMORY_SIZE - 12) {
                uint32_t val = PPC_LOAD_U32(elem + 8);
                if (val != (uint32_t)-2) {
                    PPC_STORE_U32(elem + 8, (uint32_t)-2);
                }
            }
        }
    }

    // Call the free method if allocator is valid
    uint32_t mgr_addr = (uint32_t)-2083782656 - 68; // 0x83DCD000 - 68 = 0x83DCCFBC
    if (mgr_addr < PPC_MEMORY_SIZE - 4) {
        uint32_t mgr = PPC_LOAD_U32(mgr_addr);
        if (mgr >= 0x10000 && mgr < PPC_MEMORY_SIZE - 4) {
            uint32_t vtbl = PPC_LOAD_U32(mgr);
            if (vtbl >= 0x82000000 && vtbl < PPC_MEMORY_SIZE - 52) {
                uint32_t free_fn = PPC_LOAD_U32(vtbl + 48);
                if (free_fn >= PPC_CODE_BASE && free_fn < (PPC_CODE_BASE + PPC_CODE_SIZE)) {
                    ctx.r3.u64 = mgr;
                    ctx.r4.u64 = buf;
                    PPC_CALL_INDIRECT_FUNC(free_fn);
                }
            }
        }
    }
    PPC_STORE_U32(r31, 0);
}

extern "C" PPC_FUNC(__imp__sub_836B4A68);
PPC_FUNC(sub_836B1768)
{
    // r3 = interrupt source (0 = VBlank, 1 = command-processor interrupt)
    // r4 = userData (device pointer 0x4004CD00)
    const uint32_t interruptReason = ctx.r3.u32;
    if (ctx.r4.u32 < 0x10000 || ctx.r4.u32 >= PPC_MEMORY_SIZE - 11040) {
        return;
    }
    uint32_t r11 = PPC_LOAD_U32(ctx.r4.u32 + 11028);
    static std::atomic<uint32_t> s_vblankCount{0};
    const uint32_t vblankCount = interruptReason == 0 ? ++s_vblankCount : 0;
    if (vblankCount > 0 && vblankCount <= 4)
        LOGF_INFO("[GPU IRQ] sub_836B1768 VBlank userData=0x{:08X} deviceState=0x{:08X}",
                  ctx.r4.u32, r11);
    if (r11 >= 0x10000 && r11 < PPC_MEMORY_SIZE - 32) {
        uint32_t callback = PPC_LOAD_U32(r11 + 16);
        if (vblankCount > 0 && vblankCount <= 4)
            LOGF_INFO("[GPU IRQ] VBlank pending=0x{:08X} guestCallback=0x{:08X} callbackArg=0x{:08X}",
                      PPC_LOAD_U32(r11), callback, PPC_LOAD_U32(r11 + 20));
        // Only call callback if it is a valid code address
        if (callback >= PPC_CODE_BASE && callback < (PPC_CODE_BASE + PPC_CODE_SIZE)) {
            PPCContext cbCtx = ctx;
            cbCtx.r3.u64 = PPC_LOAD_U32(r11 + 20);
            PPCContext* previousContext = GetPPCContext();
            SetPPCContext(cbCtx);
            if (PPCFunc* callbackFunction = g_memory.FindFunction(callback))
                callbackFunction(cbCtx, base);
            if (previousContext)
                SetPPCContext(*previousContext);
            else
                g_ppcContext = nullptr;
        }
    }
    // Now execute the pending flag clear:
    uint8_t cpu = ctx.r13.u32 ? PPC_LOAD_U8(ctx.r13.u32 + 268) : 5;
    uint32_t mask = (1u << (cpu & 0x1F));
    if (r11 >= 0x10000 && r11 < PPC_MEMORY_SIZE - 4) {
        // Clear pending flag for this CPU
        uint32_t flags = PPC_LOAD_U32(r11);
        flags &= ~mask;
        flags &= 0x3F;
        PPC_STORE_U32(r11, flags);
    }
    // Check if reason == 0 (VBlank), call sub_836B4A68 to advance D3D frame presentation
    if (interruptReason == 0) {
        if (vblankCount > 0 && (vblankCount <= 10 || (vblankCount % 60) == 0))
            LOGF_INFO("[GPU IRQ] VBlank dispatch calling __imp__sub_836B4A68 device=0x{:08X} #{}", ctx.r4.u32, vblankCount);
        ctx.r3.u64 = ctx.r4.u64;
        __imp__sub_836B4A68(ctx, base);
    }
}

extern "C" PPC_FUNC(__imp__sub_83536A60);
PPC_FUNC(sub_83536A60)
{
    if (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 140 || (ctx.r3.u32 & 3) != 0) {
        return;
    }
    uint32_t r31 = ctx.r3.u32;
    uint32_t count = PPC_LOAD_U32(r31 + 136);
    uint32_t buf = PPC_LOAD_U32(r31 + 120);
    if (count > 100000 || buf < 0x10000 || buf >= PPC_MEMORY_SIZE - (count * 48ULL)) {
        PPC_STORE_U32(r31 + 136, 0);
    }
    __imp__sub_83536A60(ctx, base);
}
