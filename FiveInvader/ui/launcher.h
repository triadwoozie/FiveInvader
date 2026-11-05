#pragma once

#include <filesystem>
#include <string>

class Launcher
{
public:
    // Run the standalone launcher GUI.
    // Returns true if the user clicked "Launch Game", false if the user closed/exited the launcher.
    // Populates outGameDir with the chosen/verified game directory.
    static bool Run(std::filesystem::path& outGameDir);

    // Helper to find and validate the 32-byte retail AES key file.
    static bool ValidateAesKey(std::filesystem::path& outPath);

    // Helper to check whether default.xex exists in the target directory.
    static bool ValidateGameDirectory(const std::filesystem::path& dir, std::filesystem::path& outXexPath);

    // Save launcher configuration to ~/.fiveinvader/config.toml
    static void SaveConfig(const std::filesystem::path& gameDir, int width, int height, bool fullscreen, bool vsync, int fps);

    // Load launcher configuration from ~/.fiveinvader/config.toml
    static bool LoadConfig(std::filesystem::path& outGameDir, int& outWidth, int& outHeight, bool& outFullscreen, bool& outVSync, int& outFPS);
};
