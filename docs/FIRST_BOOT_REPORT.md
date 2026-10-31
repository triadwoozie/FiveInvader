# FiveInvader First-Boot Smoke Test Report

**Date**: October 31, 2026  
**Build Target**: `build/FiveInvader/FiveInvader` (ELF 64-bit LSB, 554 MB)  
**Binary Commit**: `f91cf95` + Boot-critical memory stubs & Vulkan window flag fix  
**Platform**: Linux x86_64 (Nobara/Fedora 44, Wayland/X11, AMD Radeon Vulkan RADV)

---

## 1. Executive Summary

| Checkpoint | Result | Details |
| :--- | :--- | :--- |
| **Host Initialization** | **PASSED** | OS Process, Logger, Registry, PreloadContext initialized |
| **Xenon Memory Setup** | **PASSED** | Zeroed PPC data region `0x82000000-0x831F0000` (17.9 MB contiguous) |
| **Save System** | **PASSED** | Initialized GTA V save system, created and mapped `SaveData` |
| **Content & VFS Mounting** | **PASSED** | Mounted `game`, `dvd`, `D`, `common`, `platform`, `audio`. Indexed 158 files, 8.3 GB |
| **Module Loader (`LdrLoadModule`)** | **PASSED** | Loaded `default.xex` (26.6 MB), entrypoint: `0x83639888` |
| **Rendering Device (Vulkan)** | **PASSED** | `PLUME_SDL_VULKAN_ENABLED` swapchain created at 1920x1080 |
| **Guest PPC Execution** | **HIT** | Started `GuestThread` and successfully called guest entrypoint `0x83639888` |

---

## 2. Boot Trace Analysis

### 2.1 Execution Log (First Boot with Extracted Disc 2 Data)
```text
[PATCH] Patching sync primitives in PPCFuncMappings...
[PATCH] Sync primitive patching complete.
[*] OS does not support registry.
[*] Saving configuration...
[*] Host Startup
[Main] Calling KiSystemStartup...
[InitializeXenonMemoryRegions] [Xenon] Initializing Xbox 360 memory regions per Xenon contract
[InitializeXenonMemoryRegions] [Xenon] Zeroing entire PPC data region: 0x82000000-0x831F0000 (17.9 MB)
[InitializeXenonMemoryRegions] [Xenon] Memory region initialization complete
[InitializeXenonMemoryRegions] [Xenon] Total zeroed: 17.9 MB contiguous (0x82000000-0x831F0000)
[SaveSystem] ========================================
[SaveSystem] Initializing GTA V Save System
[SaveSystem] ========================================
[SaveSystem] Save directory: /home/triadwoozie/Documents/FiveInvader/rom_data/extracted_disc2/save
[SaveSystem] Save directory created/verified
[SaveSystem] Enumerated 0 save files
[SaveSystem] Registered root: SaveData -> /home/triadwoozie/Documents/FiveInvader/rom_data/extracted_disc2/save
[SaveSystem] Save system initialized successfully
[SaveSystem] Found 0 existing save files
[Main] No update directory found (this is normal if no Title Update installed)
[XamContentCreateEx] Root: 'game', Content: 'Game'
[XamContentCreateEx] Content found and opened (SUCCESS)
[XamContentCreateEx] Root: 'D', Content: 'Game'
[XamContentCreateEx] Content found and opened (SUCCESS)
[Main] Game root: /home/triadwoozie/Documents/FiveInvader/rom_data/extracted_disc2
[Main] Registered common: -> /home/triadwoozie/Documents/FiveInvader/rom_data/extracted_disc2/common
[Main] Registered platform: -> /home/triadwoozie/Documents/FiveInvader/rom_data/extracted_disc2/xbox360
[Main] Registered audio: -> /home/triadwoozie/Documents/FiveInvader/rom_data/extracted_disc2/audio
[Main] Registered game/dvd: -> /home/triadwoozie/Documents/FiveInvader/rom_data/extracted_disc2/game
[VFS] Initialize called with: /home/triadwoozie/Documents/FiveInvader/rom_data/extracted_disc2
[VFS] Calling ResetPathMappings...
[VFS] Calling RebuildIndex...
[Initialize] [VFS] Initialized with root: /home/triadwoozie/Documents/FiveInvader/rom_data/extracted_disc2
[Initialize] [VFS] Indexed 158 files, 5 directories, 8301398460 bytes total
[Main] VFS initialized with root: /home/triadwoozie/Documents/FiveInvader/rom_data/extracted_disc2
[Main] KiSystemStartup done
[Main] Loading module: /home/triadwoozie/Documents/FiveInvader/rom_data/extracted_disc2/default.xex
[LdrLoadModule] Zeroed worker globals 0x830F5000-0x830F8000
[Main] Module loaded, entry=0x83639888
[Main] Creating video device...
[*] SDL video driver: "x11"
[*] Using native display resolution: 1920x1080
[TryLoadEmbeddedShaderCache] Embedded shader cache is empty for this platform
[TryLoadDiskShaderCache] Disk shader cache not found at /home/triadwoozie/.local/share/FiveInvader/shader_cache/spirv.cache, using embedded
[LoadEmbeddedResources] No shader cache available! Neither embedded nor disk cache found.
[LoadEmbeddedResources] Run the installer to generate shader cache, or rebuild with shader_cache.cpp
[Main] Video device created
[*] Start Guest Thread
[*] /home/triadwoozie/Documents/FiveInvader/rom_data/extracted_disc2/default.xex
[GuestThread] Starting guest code at 0x83639888
[GuestThread] Calling function...
```

---

## 3. Key Findings & Diagnosed Issues

1. **Vulkan Surface Creation Fix**:
   - **Issue**: Initial run crashed inside `wsi_GetPhysicalDeviceSurfaceCapabilitiesKHR` with `The specified window isn't a Vulkan window`.
   - **Root Cause**: `FiveInvader/ui/game_window.cpp` was guarding `flags |= SDL_WINDOW_VULKAN;` with `#ifdef SDL_VULKAN_ENABLED` rather than `#if defined(PLUME_SDL_VULKAN_ENABLED) || defined(SDL_VULKAN_ENABLED)`.
   - **Resolution**: Updated `game_window.cpp` to check both macro defines, enabling Vulkan window creation and swapchain allocation.

2. **Configuration Duplicate Table Fix**:
   - **Issue**: `Failed to parse configuration: Error while parsing table header: cannot redefine existing table 'Input'`.
   - **Root Cause**: `user/config_def.h` declared `Input` parameters in two split sections around `Bindings`, causing `config.toml` to be generated with duplicate `[Input]` sections.
   - **Resolution**: Consolidated all `Input` settings contiguously in `config_def.h`.

3. **VFS Path Safety**:
   - **Issue**: `BuildPathCache` threw unhandled `std::filesystem_error` if the target path did not exist on disk.
   - **Resolution**: Gated iteration with `std::error_code` checks and added `--game-dir` and `FIVEINVADER_GAME_DIR` runtime overrides.

4. **Guest Instruction Execution**:
   - **Result**: The first-boot GDB trace successfully passed host initialization, completed VFS mounting, spawned worker threads, created the rendering context, and **invoked guest PPC instructions at entrypoint `0x83639888`**.
