#include "launcher.h"

#include <SDL2/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <user/config.h>
#include <user/paths.h>
#include <os/logger.h>

#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <algorithm>

bool Launcher::ValidateAesKey(std::filesystem::path& outPath)
{
    std::vector<std::filesystem::path> candidates;

    if (const char* envPath = std::getenv("FIVEINVADER_AES_KEY"))
    {
        if (*envPath)
            candidates.push_back(envPath);
    }

    if (const char* homeDir = std::getenv("HOME"))
    {
        candidates.push_back(std::filesystem::path(homeDir) / ".fiveinvader" / "aes_key.bin");
        candidates.push_back(std::filesystem::path(homeDir) / ".fiveinvader" / "encryption_key.bin");
        candidates.push_back(std::filesystem::path(homeDir) / ".config" / "FiveInvader" / "aes_key.bin");
    }

    candidates.push_back(std::filesystem::current_path() / "aes_key.bin");
    candidates.push_back(std::filesystem::current_path() / "encryption_key.bin");
    candidates.push_back(std::filesystem::current_path() / "rom_data" / "extracted_disc2" / "encryption_key.bin");
    candidates.push_back(std::filesystem::current_path() / "rom_data" / "extracted_disc2" / "aes_key.bin");

    for (const auto& candidate : candidates)
    {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && std::filesystem::file_size(candidate, ec) >= 32)
        {
            outPath = candidate;
            return true;
        }
    }

    return false;
}

bool Launcher::ValidateGameDirectory(const std::filesystem::path& dir, std::filesystem::path& outXexPath)
{
    if (dir.empty())
        return false;

    std::vector<std::filesystem::path> candidates = {
        dir / "default.xex",
        dir / "game" / "default.xex",
        dir / "rom_data" / "extracted_disc2" / "default.xex",
        dir / "FiveInvaderLib" / "private" / "default.xex"
    };

    for (const auto& c : candidates)
    {
        std::error_code ec;
        if (std::filesystem::exists(c, ec))
        {
            outXexPath = c;
            return true;
        }
    }

    return false;
}

void Launcher::SaveConfig(const std::filesystem::path& gameDir, int width, int height, bool fullscreen, bool vsync, int fps)
{
    const char* home = std::getenv("HOME");
    if (!home)
        return;

    std::filesystem::path cfgDir = std::filesystem::path(home) / ".fiveinvader";
    std::error_code ec;
    std::filesystem::create_directories(cfgDir, ec);

    std::filesystem::path cfgPath = cfgDir / "config.toml";
    std::ofstream out(cfgPath);
    if (!out.is_open())
        return;

    out << "# FiveInvader Standalone Configuration\n";
    out << "[Launcher]\n";
    out << "game_dir = \"" << gameDir.string() << "\"\n\n";
    out << "[Video]\n";
    out << "WindowWidth = " << width << "\n";
    out << "WindowHeight = " << height << "\n";
    out << "Fullscreen = " << (fullscreen ? "true" : "false") << "\n";
    out << "VSync = " << (vsync ? "true" : "false") << "\n";
    out << "FPS = " << fps << "\n";
}

bool Launcher::LoadConfig(std::filesystem::path& outGameDir, int& outWidth, int& outHeight, bool& outFullscreen, bool& outVSync, int& outFPS)
{
    const char* home = std::getenv("HOME");
    if (!home)
        return false;

    std::filesystem::path cfgPath = std::filesystem::path(home) / ".fiveinvader" / "config.toml";
    std::ifstream in(cfgPath);
    if (!in.is_open())
        return false;

    std::string line;
    while (std::getline(in, line))
    {
        auto trim = [](std::string& s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
        };
        trim(line);
        if (line.empty() || line[0] == '#' || line[0] == '[')
            continue;

        auto eqPos = line.find('=');
        if (eqPos == std::string::npos)
            continue;

        std::string key = line.substr(0, eqPos);
        std::string val = line.substr(eqPos + 1);
        trim(key);
        trim(val);

        if (key == "game_dir")
        {
            if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
                val = val.substr(1, val.size() - 2);
            outGameDir = val;
        }
        else if (key == "WindowWidth")
        {
            outWidth = std::stoi(val);
        }
        else if (key == "WindowHeight")
        {
            outHeight = std::stoi(val);
        }
        else if (key == "Fullscreen")
        {
            outFullscreen = (val == "true" || val == "1");
        }
        else if (key == "VSync")
        {
            outVSync = (val == "true" || val == "1");
        }
        else if (key == "FPS")
        {
            outFPS = std::stoi(val);
        }
    }

    return true;
}

bool Launcher::Run(std::filesystem::path& outGameDir)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0)
    {
        LOGF_ERROR("[Launcher] Failed to initialize SDL: {}", SDL_GetError());
        return false;
    }

    const int LAUNCHER_WIDTH = 760;
    const int LAUNCHER_HEIGHT = 560;

    SDL_Window* window = SDL_CreateWindow(
        "FiveInvader Launcher",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        LAUNCHER_WIDTH,
        LAUNCHER_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window)
    {
        LOGF_ERROR("[Launcher] Failed to create launcher window: {}", SDL_GetError());
        return false;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        renderer = SDL_CreateRenderer(window, -1, 0);
    }
    if (!renderer)
    {
        LOGF_ERROR("[Launcher] Failed to create SDL_Renderer: {}", SDL_GetError());
        SDL_DestroyWindow(window);
        return false;
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGuiContext* imguiCtx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Apply GTA V style dark palette
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.FramePadding = ImVec2(8.0f, 6.0f);

    auto& colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.22f, 0.26f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.33f, 0.38f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.22f, 0.25f, 0.30f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.32f, 0.37f, 0.45f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.45f, 0.55f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.17f, 0.20f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.24f, 0.28f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.95f, 0.65f, 0.15f, 1.0f);

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Initial config values
    std::filesystem::path currentPath = outGameDir;
    int curWidth = Config::WindowWidth > 0 ? Config::WindowWidth.Value : 1920;
    int curHeight = Config::WindowHeight > 0 ? Config::WindowHeight.Value : 1080;
    bool curFullscreen = Config::Fullscreen;
    bool curVSync = Config::VSync;
    int curFPS = Config::FPS > 0 ? Config::FPS.Value : 60;

    // Try loading saved config from ~/.fiveinvader/config.toml
    std::filesystem::path savedPath;
    if (Launcher::LoadConfig(savedPath, curWidth, curHeight, curFullscreen, curVSync, curFPS))
    {
        if (currentPath.empty() && !savedPath.empty())
            currentPath = savedPath;
    }

    // Default fallback search path if still empty
    if (currentPath.empty())
    {
        std::vector<std::filesystem::path> defaultCandidates = {
            "/home/ryan/FiveInvader_Workspace/rom_data/extracted_disc2",
            std::filesystem::current_path() / "rom_data" / "extracted_disc2",
            std::filesystem::current_path().parent_path() / "FiveInvader_Workspace" / "rom_data" / "extracted_disc2",
            std::filesystem::current_path()
        };

        for (const auto& cand : defaultCandidates)
        {
            std::filesystem::path dummyXex;
            if (ValidateGameDirectory(cand, dummyXex))
            {
                currentPath = cand;
                break;
            }
        }
    }

    char gameDirBuf[1024];
    std::snprintf(gameDirBuf, sizeof(gameDirBuf), "%s", currentPath.string().c_str());

    // Resolutions list
    struct ResOption { const char* label; int w; int h; };
    const ResOption resOptions[] = {
        { "1280 x 720 (720p HD)", 1280, 720 },
        { "1920 x 1080 (1080p FHD)", 1920, 1080 },
        { "2560 x 1440 (1440p QHD)", 2560, 1440 },
        { "3840 x 2160 (4K UHD)", 3840, 2160 }
    };

    int selectedResIndex = 1; // Default 1080p
    for (int i = 0; i < 4; ++i)
    {
        if (resOptions[i].w == curWidth && resOptions[i].h == curHeight)
        {
            selectedResIndex = i;
            break;
        }
    }

    const char* fpsLabels[] = { "30 FPS", "60 FPS (Recommended)", "Unlimited (Uncapped)" };
    const int fpsValues[] = { 30, 60, 0 };
    int selectedFpsIndex = 1; // 60 FPS
    for (int i = 0; i < 3; ++i)
    {
        if (fpsValues[i] == curFPS)
        {
            selectedFpsIndex = i;
            break;
        }
    }

    bool shouldLaunch = false;
    bool shouldQuit = false;

    while (!shouldLaunch && !shouldQuit)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
            {
                shouldQuit = true;
            }
            else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
            {
                shouldQuit = true;
            }
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Render main launcher panel covering entire window
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(float(LAUNCHER_WIDTH), float(LAUNCHER_HEIGHT)));
        ImGui::Begin("LauncherPanel", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        // Header
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.15f, 1.0f), "FIVEINVADER");
        ImGui::SameLine();
        ImGui::TextDisabled("| Grand Theft Auto V Xbox 360 Static Recompiler");
        ImGui::Separator();
        ImGui::Spacing();

        // Section 1: Game Directory
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.95f, 1.0f), "Game Directory (extracted disc files):");
        ImGui::SetNextItemWidth(float(LAUNCHER_WIDTH - 180));
        ImGui::InputText("##GameDirInput", gameDirBuf, sizeof(gameDirBuf));
        ImGui::SameLine();
        if (ImGui::Button("Reset Default", ImVec2(140, 0)))
        {
            std::snprintf(gameDirBuf, sizeof(gameDirBuf), "%s",
                "/home/ryan/FiveInvader_Workspace/rom_data/extracted_disc2");
        }

        // Validate Game Directory
        std::filesystem::path currentCheckDir(gameDirBuf);
        std::filesystem::path foundXex;
        bool gameValid = ValidateGameDirectory(currentCheckDir, foundXex);

        if (gameValid)
        {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f),
                " [OK] Executable found: %s", foundXex.filename().string().c_str());
        }
        else
        {
            ImGui::TextColored(ImVec4(0.95f, 0.25f, 0.25f, 1.0f),
                " [ERROR] default.xex not found in specified directory!");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Section 2: Retail AES Key Status
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.95f, 1.0f), "Retail AES-256 Encryption Key:");
        std::filesystem::path foundKeyPath;
        bool keyValid = ValidateAesKey(foundKeyPath);

        if (keyValid)
        {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f),
                " [OK] Valid Retail AES Key (32 bytes) loaded from:");
            ImGui::TextDisabled("     %s", foundKeyPath.string().c_str());
        }
        else
        {
            ImGui::TextColored(ImVec4(0.95f, 0.25f, 0.25f, 1.0f),
                " [WARN] AES-256 Key missing! Encrypted RPF archives will fail to mount.");
            ImGui::TextDisabled("     Place 32-byte key at ~/.fiveinvader/aes_key.bin or <game_dir>/encryption_key.bin");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Section 3: Graphics & Display Settings
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.95f, 1.0f), "Graphics & Display Settings:");

        // Resolution Dropdown
        ImGui::SetNextItemWidth(300.0f);
        if (ImGui::BeginCombo("Resolution", resOptions[selectedResIndex].label))
        {
            for (int i = 0; i < 4; ++i)
            {
                bool isSelected = (selectedResIndex == i);
                if (ImGui::Selectable(resOptions[i].label, isSelected))
                {
                    selectedResIndex = i;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Fullscreen & VSync Checkboxes
        ImGui::Checkbox("Fullscreen Mode", &curFullscreen);
        ImGui::SameLine(260.0f);
        ImGui::Checkbox("Enable VSync", &curVSync);

        // Framerate Cap
        ImGui::SetNextItemWidth(300.0f);
        if (ImGui::BeginCombo("Framerate Cap", fpsLabels[selectedFpsIndex]))
        {
            for (int i = 0; i < 3; ++i)
            {
                bool isSelected = (selectedFpsIndex == i);
                if (ImGui::Selectable(fpsLabels[i], isSelected))
                {
                    selectedFpsIndex = i;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Section 4: Action Buttons
        ImGui::SetCursorPosY(float(LAUNCHER_HEIGHT - 65));

        // Green Launch Button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.60f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.75f, 0.32f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.50f, 0.20f, 1.0f));

        if (ImGui::Button("Launch Game", ImVec2(240, 42)))
        {
            if (gameValid)
            {
                shouldLaunch = true;
            }
        }
        ImGui::PopStyleColor(3);

        if (!gameValid && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Cannot launch: Valid default.xex not found in game directory.");
        }

        ImGui::SameLine(float(LAUNCHER_WIDTH - 150));
        if (ImGui::Button("Exit", ImVec2(130, 42)))
        {
            shouldQuit = true;
        }

        ImGui::End();

        // Render draw data
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 24, 24, 28, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    if (shouldLaunch)
    {
        outGameDir = std::filesystem::path(gameDirBuf);
        Config::WindowWidth = resOptions[selectedResIndex].w;
        Config::WindowHeight = resOptions[selectedResIndex].h;
        Config::Fullscreen = curFullscreen;
        Config::VSync = curVSync;
        Config::FPS = fpsValues[selectedFpsIndex];

        Launcher::SaveConfig(
            outGameDir,
            Config::WindowWidth,
            Config::WindowHeight,
            Config::Fullscreen,
            Config::VSync,
            Config::FPS
        );

        LOGF_INFO("[Launcher] Game launch confirmed: dir='{}', res={}x{}, fullscreen={}, vsync={}, fps={}",
            outGameDir.string(), Config::WindowWidth.Value, Config::WindowHeight.Value,
            Config::Fullscreen.Value, Config::VSync.Value, Config::FPS.Value);
    }

    // Cleanup ImGui & SDL
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext(imguiCtx);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    // Flush any leftover events
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);

    return shouldLaunch;
}
