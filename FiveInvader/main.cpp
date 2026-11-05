#include <cstdio>
#include <stdafx.h>
#ifdef __x86_64__
#include <cpuid.h>
#endif
#include <cpu/guest_thread.h>
#include <gpu/video.h>
#include <kernel/function.h>
#include <kernel/memory.h>
#include <kernel/heap.h>
#include <kernel/xam.h>
#include <kernel/save_system.h>
#include <kernel/io/file_system.h>
#include <kernel/vfs.h>
#include <file.h>
#include <vector>
#include <image.h>
#include <apu/audio.h>
#include <hid/hid.h>
#include <user/config.h>
#include <user/paths.h>
#include <user/registry.h>
#include <kernel/xdbf.h>
#include <install/installer.h>
#include <install/update_checker.h>
#include <os/logger.h>
#include <os/process.h>
#include <os/registry.h>
#include <ui/game_window.h>
#include <ui/installer_wizard.h>
#include <ui/launcher.h>
#include <mod/mod_loader.h>
#include <preload_executable.h>
#include <iostream>
#include <app.h>
#ifndef _WIN32
#include <csignal>
#include <execinfo.h>
#include <unistd.h>

static void CrashSignalHandler(int sig, siginfo_t* info, void* ucontext)
{
    ucontext_t* uc = reinterpret_cast<ucontext_t*>(ucontext);
    uintptr_t rip = uc ? uc->uc_mcontext.gregs[REG_RIP] : 0;
    uintptr_t rsp = uc ? uc->uc_mcontext.gregs[REG_RSP] : 0;
    uintptr_t caller_rip = (rip == 0 && rsp != 0) ? *reinterpret_cast<uintptr_t*>(rsp) : 0;
    fprintf(stderr, "\n========== CRASH CAUGHT: signal %d (fault addr=%p, host rip=0x%lx, caller_rip=0x%lx) ==========\n",
            sig, info ? info->si_addr : nullptr, (unsigned long)rip, (unsigned long)caller_rip);
    if (uc) {
        fprintf(stderr, "[CRASH_REGS] RAX=0x%lx RBX=0x%lx RCX=0x%lx RDX=0x%lx RSI=0x%lx RDI=0x%lx RBP=0x%lx RSP=0x%lx\n",
            (unsigned long)uc->uc_mcontext.gregs[REG_RAX],
            (unsigned long)uc->uc_mcontext.gregs[REG_RBX],
            (unsigned long)uc->uc_mcontext.gregs[REG_RCX],
            (unsigned long)uc->uc_mcontext.gregs[REG_RDX],
            (unsigned long)uc->uc_mcontext.gregs[REG_RSI],
            (unsigned long)uc->uc_mcontext.gregs[REG_RDI],
            (unsigned long)uc->uc_mcontext.gregs[REG_RBP],
            (unsigned long)uc->uc_mcontext.gregs[REG_RSP]);
        fprintf(stderr, "[CRASH_REGS] R8=0x%lx R9=0x%lx R10=0x%lx R11=0x%lx R12=0x%lx R13=0x%lx R14=0x%lx R15=0x%lx\n",
            (unsigned long)uc->uc_mcontext.gregs[REG_R8],
            (unsigned long)uc->uc_mcontext.gregs[REG_R9],
            (unsigned long)uc->uc_mcontext.gregs[REG_R10],
            (unsigned long)uc->uc_mcontext.gregs[REG_R11],
            (unsigned long)uc->uc_mcontext.gregs[REG_R12],
            (unsigned long)uc->uc_mcontext.gregs[REG_R13],
            (unsigned long)uc->uc_mcontext.gregs[REG_R14],
            (unsigned long)uc->uc_mcontext.gregs[REG_R15]);
    }
    if (g_ppcContext) {
        fprintf(stderr, "[CRASH_CONTEXT] PPCContext=%p r1=0x%llX r3=0x%llX r4=0x%llX r5=0x%llX r10=0x%llX r12=0x%llX r13=0x%llX\n",
            g_ppcContext,
            (unsigned long long)g_ppcContext->r1.u64,
            (unsigned long long)g_ppcContext->r3.u64,
            (unsigned long long)g_ppcContext->r4.u64,
            (unsigned long long)g_ppcContext->r5.u64,
            (unsigned long long)g_ppcContext->r10.u64,
            (unsigned long long)g_ppcContext->r12.u64,
            (unsigned long long)g_ppcContext->r13.u64);
    } else {
        fprintf(stderr, "[CRASH_CONTEXT] g_ppcContext is NULL\n");
    }
    if (g_memory.base) {
        uint32_t val83bfbe8c = *reinterpret_cast<uint32_t*>(g_memory.Translate(0x83BFBE8C));
        fprintf(stderr, "[CRASH_CONTEXT] *0x83BFBE8C = 0x%08X (le: 0x%08X)\n", val83bfbe8c, ByteSwap(val83bfbe8c));
    }
    void* frames[64];
    int count = backtrace(frames, 64);
    backtrace_symbols_fd(frames, count, STDERR_FILENO);
    fprintf(stderr, "===============================================================\n");
    fflush(stderr);
    _exit(139);
}
#endif

#ifdef _WIN32
#include <timeapi.h>
#endif

#if defined(_WIN32) && defined(FIVEINVADER_D3D12)
static std::array<std::string_view, 3> g_D3D12RequiredModules =
{
    "D3D12/D3D12Core.dll",
    "dxcompiler.dll",
    "dxil.dll"
};
#endif

const size_t XMAIOBegin = 0x7FEA0000;
const size_t XMAIOEnd = XMAIOBegin + 0x0000FFFF;

Memory g_memory;
Heap g_userHeap;
XDBFWrapper g_xdbfWrapper;
std::unordered_map<uint16_t, GuestTexture*> g_xdbfTextureCache;

void HostStartup()
{
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif

    hid::Init();
}

// Forward declarations from imports.cpp
void InitKernelMainThread();

// Xenon memory initialization
#include <kernel/xenon_memory.h>

// Name inspired from nt's entry point
void KiSystemStartup()
{
    // Initialize main thread ID for SDL event safety
    InitKernelMainThread();
    
    if (g_memory.base == nullptr)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), Localise("System_MemoryAllocationFailed").c_str(), GameWindow::s_pWindow);
        std::_Exit(1);
    }

    g_userHeap.Init();
    
    // CRITICAL: Initialize Xbox 360 Xenon memory regions per memory contract
    // This zeros all regions the game assumes are pre-allocated and zeroed on boot.
    // Must happen BEFORE any game code executes to prevent corruption.
    InitializeXenonMemoryRegions(g_memory.base);

    // Initialize save system early - creates directories and registers save content
    SaveSystem::Initialize();

    std::filesystem::path gameRoot = GetGamePath() / "game";
    if (!std::filesystem::exists(gameRoot) && std::filesystem::exists(GetGamePath() / "default.xex"))
    {
        gameRoot = GetGamePath();
    }

    const auto gameContent = XamMakeContent(XCONTENTTYPE_RESERVED, "Game");
    const std::string gamePath = (const char*)gameRoot.u8string().c_str();

    BuildPathCache(gamePath);
    if (!GetStreamPath().empty())
        BuildPathCache(GetStreamPath().string());

    // Also cache extracted assets living under "RPF DUMP" (nested RPFS, audio packs, etc.).
    // This improves hit rate when the title requests files that are not present in game/.
    const std::string rpfDumpPath = (const char*)(GetGamePath() / "RPF DUMP").u8string().c_str();
    if (std::filesystem::exists(rpfDumpPath))
        BuildPathCache(rpfDumpPath);

    XamRegisterContent(gameContent, gamePath);

    // Register and mount update content (CRITICAL for file override)
    const auto updateContent = XamMakeContent(XCONTENTTYPE_RESERVED, "Update");
    const std::filesystem::path updateRoot = GetGamePath() / "update";
    const std::string updatePath = (const char*)updateRoot.u8string().c_str();

    if (std::filesystem::exists(updateRoot))
    {
        XamRegisterContent(updateContent, updatePath);
        XamContentCreateEx(0, "update", &updateContent, OPEN_EXISTING, nullptr, nullptr, 0, 0, nullptr);
        
        // Create root mappings for update paths (case variations)
        XamRootCreate("update", updatePath);
        XamRootCreate("Update", updatePath);
        
        LOGF_IMPL(Utility, "Main", "Registered update: -> {}", updatePath);
    }
    else
    {
        LOGF_IMPL(Utility, "Main", "No update directory found (this is normal if no Title Update installed)");
    }

    // Note: Save system initialization already handled by SaveSystem::Initialize() above
    // This section is kept for backwards compatibility with old save file format
    const auto saveFilePath = GetSaveFilePath(true);
    bool saveFileExists = std::filesystem::exists(saveFilePath);

    if (!saveFileExists)
    {
        // Copy base save data to modded save as fallback.
        std::error_code ec;
        std::filesystem::create_directories(saveFilePath.parent_path(), ec);

        if (!ec)
        {
            std::filesystem::copy_file(GetSaveFilePath(false), saveFilePath, ec);
            saveFileExists = !ec;
        }
    }
    if (saveFileExists)
    {
        std::u8string savePathU8 = saveFilePath.parent_path().u8string();
        XamRegisterContent(XamMakeContent(XCONTENTTYPE_SAVEDATA, "GTA5SaveData.bin"), (const char*)(savePathU8.c_str()));
    }

    // Mount game
    XamContentCreateEx(0, "game", &gameContent, OPEN_EXISTING, nullptr, nullptr, 0, 0, nullptr);

    // OS mounts game data to D:
    XamContentCreateEx(0, "D", &gameContent, OPEN_EXISTING, nullptr, nullptr, 0, 0, nullptr);

    // Register game: and dvd: root paths
    XamRootCreate("game", gamePath);
    XamRootCreate("Game", gamePath);
    XamRootCreate("dvd", gamePath);
    XamRootCreate("Dvd", gamePath);
    XamRootCreate("d", gamePath);
    XamRootCreate("D", gamePath);

    // GTA V uses "common:" and "platform:" root paths
    // All game files are in: GetGamePath() / "game" / (common/, xbox360/, audio/)
    // Structure: ~/Library/Application Support/FiveInvader/game/common/, etc.
    const std::string commonPath = (const char*)(gameRoot / "common").u8string().c_str();
    const std::string platformPath = (const char*)(gameRoot / "xbox360").u8string().c_str();
    const std::string audioPath = (const char*)(gameRoot / "audio").u8string().c_str();
    
    // Register main root paths
    XamRootCreate("common", commonPath);
    XamRootCreate("platform", platformPath);
    XamRootCreate("audio", audioPath);
    
    // Also register alternate names the game might use
    XamRootCreate("xbox360", platformPath);  // Some code uses xbox360: instead of platform:
    
    LOGF_IMPL(Utility, "Main", "Game root: {}", gameRoot.string());
    LOGF_IMPL(Utility, "Main", "Registered common: -> {}", commonPath);
    LOGF_IMPL(Utility, "Main", "Registered platform: -> {}", platformPath);
    LOGF_IMPL(Utility, "Main", "Registered audio: -> {}", audioPath);
    LOGF_IMPL(Utility, "Main", "Registered game/dvd: -> {}", gamePath);

    // Initialize Virtual File System for direct file serving
    // This bypasses complex RPF offset-based reading that causes stream issues
    VFS::Initialize(gameRoot);
    LOGF_IMPL(Utility, "Main", "VFS initialized with root: {}", gameRoot.string());

    std::error_code ec;
    for (auto& file : std::filesystem::directory_iterator(GetGamePath() / "dlc", ec))
    {
        if (file.is_directory())
        {
            std::u8string fileNameU8 = file.path().filename().u8string();
            std::u8string filePathU8 = file.path().u8string();
            const char* fileName = (const char*)(fileNameU8.c_str());
            const char* filePath = (const char*)(filePathU8.c_str());
            
            // Register DLC content
            XamRegisterContent(XamMakeContent(XCONTENTTYPE_DLC, fileName), filePath);
            
            // Mount DLC to virtual path
            auto dlcContent = XamMakeContent(XCONTENTTYPE_DLC, fileName);
            XamContentCreateEx(0, fileName, &dlcContent, OPEN_EXISTING, nullptr, nullptr, 0, 0, nullptr);
            
            // Create root mapping for DLC-specific paths
            XamRootCreate(fileName, filePath);
            
            LOGF_IMPL(Utility, "Main", "Registered DLC: {} -> {}", fileName, filePath);
        }
    }

    // Register Disc 1 Mandatory Install Partitions (part0.rpf - part3.rpf)
    const char* partitionNames[] = {
        "545408A700000000", "545408A700000001", "545408A700000002", "545408A700000003"
    };
    const char* partFiles[] = {
        "part0.rpf", "part1.rpf", "part2.rpf", "part3.rpf"
    };

    for (int i = 0; i < 4; ++i) {
        auto partPath = GetGamePath() / partFiles[i];
        if (std::filesystem::exists(partPath)) {
            std::string partPathStr = (const char*)partPath.u8string().c_str();
            
            // Register for DLC and marketplace content types
            auto installContentDLC = XamMakeContent(XCONTENTTYPE_DLC, partitionNames[i]);
            XamRegisterContent(installContentDLC, partPathStr);
            XamContentCreateEx(0, partitionNames[i], &installContentDLC, OPEN_EXISTING, nullptr, nullptr, 0, 0, nullptr);

            auto partContentDLC = XamMakeContent(XCONTENTTYPE_DLC, partFiles[i]);
            XamRegisterContent(partContentDLC, partPathStr);

            // These roots are RPF7 archive devices, not directories containing the archives.
            XamRootCreate(partitionNames[i], partPathStr);
            std::string partRootName = std::string("part") + std::to_string(i);
            XamRootCreate(partRootName, partPathStr);
            std::string installRootName = std::string("install") + std::to_string(i);
            XamRootCreate(installRootName, partPathStr);

            LOGF_IMPL(Utility, "Main", "Registered Disc 1 Partition {} archive device: {} -> {}", i, partitionNames[i], partPathStr);
        }
    }

    XAudioInitializeSystem();
}

uint32_t LdrLoadModule(const std::filesystem::path &path)
{
    const auto loadResult = LoadFile(path);
    if (loadResult.empty())
    {
        assert("Failed to load module" && false);
        return 0;
    }

    const auto image = Image::ParseImage(loadResult.data(), loadResult.size());

    memcpy(g_memory.Translate(image.base), image.data.get(), image.size);
    g_xdbfWrapper = XDBFWrapper(static_cast<uint8_t*>(g_memory.Translate(image.resource_offset)), image.resource_size);

#if 0
    // GTA V Memory Layout Collision Fix
    // Address 0x82003890 aliases with "common.rpf" string in image .rdata section.
    // The game expects this to be heap-backed stream object storage.
    // Xbox 360 kept these regions separate, but native recompilation does not.
    // Fix: Zero the collision range and fully initialize stream struct.
    {
        uint8_t* collisionBase = static_cast<uint8_t*>(g_memory.Translate(0x82003880));
        memset(collisionBase, 0, 0x80);  // Zero 0x82003880 - 0x82003900
        
        // Fully initialize stream struct at 0x82003890
        // Stream struct layout (28 bytes / 7 dwords):
        //   [0] = Object pointer (vtable holder)
        //   [1] = Context/parameter
        //   [2] = Buffer pointer
        //   [3] = File position
        //   [4] = Buffer cursor
        //   [5] = Buffer end/limit
        //   [6] = Buffer capacity
        be<uint32_t>* streamPtr = reinterpret_cast<be<uint32_t>*>(g_memory.Translate(0x82003890));
        streamPtr[0] = 0;  // Null object - stream ops will return early gracefully
        streamPtr[1] = 0;           // Context (null is safe)
        streamPtr[2] = 0;           // Buffer (null = no buffer)
        streamPtr[3] = 0;           // File position
        streamPtr[4] = 0;           // Buffer cursor
        streamPtr[5] = 0;           // Buffer end (0 = empty)
        streamPtr[6] = 0;           // Capacity (0 = no buffer)
    }
#endif
    
    // Worker Thread Global Memory Initialization
    // Address range 0x830F5000-0x830F8000 contains worker-related global structures.
    // Workers read semaphore handles from this region (e.g., 0x830F7684 = base+9860).
    // Without zeroing, workers read stale data from previous runs causing infinite polling.
    // Fix: Zero worker globals so uninitialized handles read as NULL.
    {
        uint8_t* workerGlobals = static_cast<uint8_t*>(g_memory.Translate(0x830F5000));
        if (workerGlobals) {
            memset(workerGlobals, 0, 0x3000);  // Zero 12KB of worker globals
            printf("[LdrLoadModule] Zeroed worker globals 0x830F5000-0x830F8000\n");
        }
    }

    return image.entry_point;
}

#ifdef __x86_64__
__attribute__((constructor(101), target("no-avx,no-avx2"), noinline))
void init()
{
    uint32_t eax, ebx, ecx, edx;

    // Execute CPUID for processor info and feature bits.
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);

    // Check for AVX support.
    if ((ecx & (1 << 28)) == 0)
    {
        printf("[*] CPU does not support the AVX instruction set.\n");

#ifdef _WIN32
        MessageBoxA(nullptr, "Your CPU does not meet the minimum system requirements.", "FiveInvader", MB_ICONERROR);
#endif

        std::_Exit(1);
    }
}
#endif

int main(int argc, char *argv[])
{
#ifndef _WIN32
    struct sigaction sa{};
    sa.sa_sigaction = CrashSignalHandler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
#endif

#ifdef _WIN32
    timeBeginPeriod(1);
#endif

    os::process::CheckConsole();

    if (!os::registry::Init())
        LOGN_WARNING("OS does not support registry.");

    os::logger::Init();

    PreloadContext preloadContext;
    preloadContext.PreloadExecutable();

    bool forceInstaller = false;
    bool forceDLCInstaller = false;
    bool useDefaultWorkingDirectory = false;
    bool forceInstallationCheck = false;
    bool graphicsApiRetry = false;
    bool skipLauncher = false;
    const char *sdlVideoDriver = nullptr;
    std::string cliConfigPath;
    std::string cliLang;
    int cliWidth = 0;
    int cliHeight = 0;
    int cliVsync = -1;
    int cliFullscreen = -1;
    int cliFps = -1;
    std::string cliVsyncMode;
    std::string cliWindowGate;

    for (uint32_t i = 1; i < argc; i++)
    {
        forceInstaller = forceInstaller || (strcmp(argv[i], "--install") == 0);
        forceDLCInstaller = forceDLCInstaller || (strcmp(argv[i], "--install-dlc") == 0);
        useDefaultWorkingDirectory = useDefaultWorkingDirectory || (strcmp(argv[i], "--use-cwd") == 0);
        forceInstallationCheck = forceInstallationCheck || (strcmp(argv[i], "--install-check") == 0);
        graphicsApiRetry = graphicsApiRetry || (strcmp(argv[i], "--graphics-api-retry") == 0);
        App::s_isSkipLogos = App::s_isSkipLogos || (strcmp(argv[i], "--skip-logos") == 0);

        if (strcmp(argv[i], "--sdl-video-driver") == 0)
        {
            if ((i + 1) < argc)
                sdlVideoDriver = argv[++i];
            else
                LOGN_WARNING("No argument was specified for --sdl-video-driver. Option will be ignored.");
        }
        else if (strcmp(argv[i], "--game-dir") == 0 || strcmp(argv[i], "--game-path") == 0)
        {
            if ((i + 1) < argc)
            {
                SetUserPath(argv[++i]);
            }
            else
            {
                LOGN_WARNING("No argument was specified for --game-dir. Option will be ignored.");
            }
        }
        else if (strcmp(argv[i], "--config") == 0)
        {
            if ((i + 1) < argc)
            {
                cliConfigPath = argv[++i];
            }
        }
        else if (strcmp(argv[i], "--stream-dir") == 0)
        {
            if ((i + 1) < argc)
                SetStreamPath(argv[++i]);
            else
                LOGN_WARNING("No argument was specified for --stream-dir. Option will be ignored.");
        }
        else if (strcmp(argv[i], "--lang") == 0 || strcmp(argv[i], "--language") == 0)
        {
            if ((i + 1) < argc)
            {
                cliLang = argv[++i];
            }
        }
        else if (strcmp(argv[i], "--width") == 0)
        {
            if ((i + 1) < argc)
            {
                cliWidth = std::atoi(argv[++i]);
            }
        }
        else if (strcmp(argv[i], "--height") == 0)
        {
            if ((i + 1) < argc)
            {
                cliHeight = std::atoi(argv[++i]);
            }
        }
        else if (strcmp(argv[i], "--res") == 0 || strcmp(argv[i], "--resolution") == 0)
        {
            if ((i + 1) < argc)
            {
                std::string resStr = argv[++i];
                auto pos = resStr.find('x');
                if (pos == std::string::npos) pos = resStr.find('X');
                if (pos != std::string::npos)
                {
                    cliWidth = std::atoi(resStr.substr(0, pos).c_str());
                    cliHeight = std::atoi(resStr.substr(pos + 1).c_str());
                }
            }
        }
        else if (strcmp(argv[i], "--vsync") == 0)
        {
            cliVsync = 1;
            if ((i + 1) < argc)
            {
                const char* value = argv[i + 1];
                if (strcmp(value, "0") == 0 || strcmp(value, "1") == 0)
                {
                    cliVsync = value[0] - '0';
                    ++i;
                }
                else if (value[0] != '-')
                    fprintf(stderr, "[Main] Invalid value for --vsync: %s. Expected 0 or 1.\n", value);
            }
        }
        else if (strcmp(argv[i], "--no-vsync") == 0)
        {
            cliVsync = 0;
        }
        else if (strcmp(argv[i], "--vsync-mode") == 0)
        {
            if ((i + 1) < argc)
                cliVsyncMode = argv[++i];
        }
        else if (strcmp(argv[i], "--window-gate") == 0)
        {
            if ((i + 1) < argc)
                cliWindowGate = argv[++i];
        }
        else if (strcmp(argv[i], "--verbose-alloc") == 0)
        {
            App::s_verboseAlloc = true;
        }
        else if (strcmp(argv[i], "--log-vfs") == 0)
        {
            App::s_logVfs = true;
        }
        else if (strcmp(argv[i], "--fullscreen") == 0)
        {
            cliFullscreen = 1;
            if ((i + 1) < argc)
            {
                const char* value = argv[i + 1];
                if (strcmp(value, "0") == 0 || strcmp(value, "1") == 0)
                {
                    cliFullscreen = value[0] - '0';
                    ++i;
                }
                else if (value[0] != '-')
                    fprintf(stderr, "[Main] Invalid value for --fullscreen: %s. Expected 0 or 1.\n", value);
            }
        }
        else if (strcmp(argv[i], "--windowed") == 0)
        {
            cliFullscreen = 0;
        }
        else if (strcmp(argv[i], "--fps") == 0)
        {
            if ((i + 1) < argc)
            {
                cliFps = std::atoi(argv[++i]);
            }
        }
        else if (strcmp(argv[i], "--skip-launcher") == 0 || strcmp(argv[i], "--no-launcher") == 0)
        {
            skipLauncher = true;
        }
    }

    if (!useDefaultWorkingDirectory)
    {
        // Set the current working directory to the executable's path.
        std::error_code ec;
        std::filesystem::current_path(os::process::GetExecutableRoot(), ec);
    }

    if (!cliConfigPath.empty())
    {
        Config::SetConfigPath(cliConfigPath);
    }
    Config::Load();

    // External launcher CLI overrides
    if (!cliLang.empty())
    {
        std::string l = toLower(cliLang);
        if (l == "en" || l == "english") Config::Language = ELanguage::English;
        else if (l == "fr" || l == "french") Config::Language = ELanguage::French;
        else if (l == "de" || l == "german") Config::Language = ELanguage::German;
        else if (l == "it" || l == "italian") Config::Language = ELanguage::Italian;
        else if (l == "es" || l == "spanish") Config::Language = ELanguage::Spanish;
        else if (l == "ja" || l == "japanese") Config::Language = ELanguage::Japanese;
        App::s_language = Config::Language;
    }

    if (cliWidth > 0 && cliHeight > 0)
    {
        Config::WindowWidth = cliWidth;
        Config::WindowHeight = cliHeight;
    }

    if (cliVsync != -1)
    {
        Config::VSync = (cliVsync == 1);
    }
    if (!cliVsyncMode.empty())
    {
        if (cliVsyncMode == "fifo" || cliVsyncMode == "mailbox")
        {
            Config::VSync = true;
            if (cliVsyncMode == "mailbox")
                LOGN_WARNING("Mailbox VSync is not exposed by the current render backend; using FIFO.");
        }
        else if (cliVsyncMode == "immediate")
        {
            Config::VSync = false;
        }
        else
        {
            LOGF_WARNING("Invalid --vsync-mode '{}'. Expected fifo, mailbox, or immediate.", cliVsyncMode);
        }
    }

    GameWindow::s_showOnInit = (cliWindowGate == "immediate");
    if (!cliWindowGate.empty() && cliWindowGate != "immediate" && cliWindowGate != "render")
        LOGF_WARNING("Invalid --window-gate '{}'. Expected immediate or render.", cliWindowGate);

    if (cliFullscreen != -1)
    {
        Config::Fullscreen = (cliFullscreen == 1);
    }

    if (cliFps >= 0)
    {
        Config::FPS = cliFps;
    }

    if (forceInstallationCheck)
    {
        // Create the console to show progress to the user, otherwise it will seem as if the game didn't boot at all.
        os::process::ShowConsole();

        Journal journal;
        double lastProgressMiB = 0.0;
        double lastTotalMib = 0.0;
        Installer::checkInstallIntegrity(GAME_INSTALL_DIRECTORY, journal, [&]()
        {
            constexpr double MiBDivisor = 1024.0 * 1024.0;
            constexpr double MiBProgressThreshold = 128.0;
            double progressMiB = double(journal.progressCounter) / MiBDivisor;
            double totalMiB = double(journal.progressTotal) / MiBDivisor;
            if (journal.progressCounter > 0)
            {
                if ((progressMiB - lastProgressMiB) > MiBProgressThreshold)
                {
                    fprintf(stdout, "Checking files: %0.2f MiB / %0.2f MiB\n", progressMiB, totalMiB);
                    lastProgressMiB = progressMiB;
                }
            }
            else
            {
                if ((totalMiB - lastTotalMib) > MiBProgressThreshold)
                {
                    fprintf(stdout, "Scanning files: %0.2f MiB\n", totalMiB);
                    lastTotalMib = totalMiB;
                }
            }

            return true;
        });

        char resultText[512];
        uint32_t messageBoxStyle;
        if (journal.lastResult == Journal::Result::Success)
        {
            snprintf(resultText, sizeof(resultText), "%s", Localise("IntegrityCheck_Success").c_str());
            fprintf(stdout, "%s\n", resultText);
            messageBoxStyle = SDL_MESSAGEBOX_INFORMATION;
        }
        else
        {
            snprintf(resultText, sizeof(resultText), Localise("IntegrityCheck_Failed").c_str(), journal.lastErrorMessage.c_str());
            fprintf(stderr, "%s\n", resultText);
            messageBoxStyle = SDL_MESSAGEBOX_ERROR;
        }

        SDL_ShowSimpleMessageBox(messageBoxStyle, GameWindow::GetTitle(), resultText, GameWindow::s_pWindow);
        std::_Exit(int(journal.lastResult));
    }

#if defined(_WIN32) && defined(FIVEINVADER_D3D12)
    for (auto& dll : g_D3D12RequiredModules)
    {
        if (!std::filesystem::exists(g_executableRoot / dll))
        {
            char text[512];
            snprintf(text, sizeof(text), Localise("System_Win32_MissingDLLs").c_str(), dll.data());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), text, GameWindow::s_pWindow);
            std::_Exit(1);
        }
    }
#endif

    // Update checker disabled (leftover from UnleashedRecomp)
    /*
    constexpr double TimeBetweenUpdateChecksInSeconds = 6 * 60 * 60;
    time_t timeNow = std::time(nullptr);
    double timeDifferenceSeconds = difftime(timeNow, Config::LastChecked);
    if (timeDifferenceSeconds > TimeBetweenUpdateChecksInSeconds)
    {
        UpdateChecker::initialize();
        UpdateChecker::start();
        Config::LastChecked = timeNow;
        Config::Save();
    }
    */

    if (Config::ShowConsole)
        os::process::ShowConsole();
    LOGN_WARNING("Host Startup");
    HostStartup();

    std::filesystem::path currentPath = GetGamePath();
    if (!skipLauncher)
    {
        if (!Launcher::Run(currentPath))
        {
            LOGN_INFO("[Main] Launcher closed by user. Exiting.");
            return 0;
        }
        SetUserPath(currentPath);
    }
    else
    {
        // If launcher is skipped and current path lacks default.xex, check saved config
        std::filesystem::path dummyXex;
        if (!Launcher::ValidateGameDirectory(currentPath, dummyXex))
        {
            std::filesystem::path savedPath;
            int dummyW = 0, dummyH = 0, dummyFps = 0;
            bool dummyFs = false, dummyVs = false;
            if (Launcher::LoadConfig(savedPath, dummyW, dummyH, dummyFs, dummyVs, dummyFps))
            {
                if (Launcher::ValidateGameDirectory(savedPath, dummyXex))
                {
                    currentPath = savedPath;
                    SetUserPath(currentPath);
                }
            }
        }
    }

    std::filesystem::path modulePath;
    bool isGameInstalled = Installer::checkGameInstall(GetGamePath(), modulePath);

    if (!isGameInstalled)
    {
        fprintf(stderr, "[Main] Fatal: GTA V executable (default.xex) not found in '%s' or fallback paths.\n",
                GetGamePath().string().c_str());
        fprintf(stderr, "[Main] Please provide a valid game directory via --game-dir <path>.\n");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(),
            "GTA V executable (default.xex) not found.\nPlease specify the game directory using --game-dir <path>.",
            GameWindow::s_pWindow);
        std::_Exit(1);
    }

    // Installer wizard is permanently bypassed for native GTA V direct execution
    printf("[Main] Calling KiSystemStartup...\n"); fflush(stdout);
    KiSystemStartup();
    printf("[Main] KiSystemStartup done\n"); fflush(stdout);

    printf("[Main] Loading module: %s\n", modulePath.string().c_str()); fflush(stdout);
    uint32_t entry = LdrLoadModule(modulePath);
    printf("[Main] Module loaded, entry=0x%08X\n", entry); fflush(stdout);

    printf("[Main] Creating video device...\n"); fflush(stdout);
    if (!Video::CreateHostDevice(sdlVideoDriver, graphicsApiRetry))
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), Localise("Video_BackendError").c_str(), GameWindow::s_pWindow);
        std::_Exit(1);
    }
    printf("[Main] Video device created\n"); fflush(stdout);

    LOGN_WARNING("Start Guest Thread");
    LOGN_WARNING(modulePath.string());
    // Video::StartPipelinePrecompilation();

    GuestThread::Start({ entry, 0, 0, 0 });

    return 0;
}

GUEST_FUNCTION_STUB(__imp__vsprintf);
GUEST_FUNCTION_STUB(__imp___vsnprintf);
GUEST_FUNCTION_STUB(__imp__sprintf);
GUEST_FUNCTION_STUB(__imp___snprintf);
GUEST_FUNCTION_STUB(__imp___snwprintf);
GUEST_FUNCTION_STUB(__imp__vswprintf);
GUEST_FUNCTION_STUB(__imp___vscwprintf);
GUEST_FUNCTION_STUB(__imp__swprintf);
