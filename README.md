# FiveInvader

> [!WARNING]
> **Project Authorship & AI Attribution Notice**
> Approximately **90% of this codebase was authored, compiled, and debugged by Google's Antigravity**, operating under direct human supervision, technical direction, and architectural specification by [@triadwoozie](https://github.com/triadwoozie).

**FiveInvader** is an experimental native static recompilation of the Xbox 360 release of *Grand Theft Auto V* (Title ID: `545408A7`, Title Update & DLC build) targeting 64-bit Linux environments via Vulkan, with a native Android (AArch64) deployment roadmapped.

By pre-translating PowerPC Xenon machine code into native C++ using `XenonRecomp` and providing a dedicated runtime layer for Xbox 360 kernel and system interfaces, FiveInvader eliminates runtime JIT interpretation overhead. The runtime operates within the strict 512 MB guest physical footprint of the original 2013 RAGE engine, targeting low-overhead execution on lower-tier hardware (e.g., 11th Gen Intel i3 processors with integrated graphics) and eventual portability to high-end mobile silicon via the Android NDK.

---

## Architecture Overview

```
┌────────────────────────────────────────────────────────┐
│           FiveInvader Runtime Architecture             │
├────────────────────────────────────────────────────────┤
│                                                        │
│  [ Disc 2 / GoD Assets ]      [ Disc 1 Content Packs ] │
│  (common.rpf, xbox360a/b.rpf) (part0.rpf - part3.rpf)  │
│             │                             │            │
│             ▼                             ▼            │
│  ┌──────────────────────────────────────────────────┐  │
│  │           RAGE VFS & RPF7 Archive Parser         │  │
│  │   (fiDevice, fiStream, STFS Container Mounts)    │  │
│  └────────────────────────┬─────────────────────────┘  │
│                           ▼                            │
│  ┌──────────────────────────────────────────────────┐  │
│  │         Recompiled Guest PPC Binary (554 MB)     │  │
│  │       (XenonRecomp Native C++ Translation)       │  │
│  ├──────────────────────────────────────────────────┤  │
│  │  • Unified 512MB RAM Space (0x80000000 Base)     │  │
│  │  • Big-Endian GuestVaList Variadic ABI Engine    │  │
│  │  • PCR/TLS Subsystem Allocators (Slots 116/120)  │  │
│  │  • 64-Thread Worker Task Pool (CREATE_SUSPENDED) │  │
│  └────────────────────────┬─────────────────────────┘  │
│                           ▼                            │
│  ┌──────────────────────────────────────────────────┐  │
│  │    Graphics Pipeline & Command Processing        │  │
│  │   • Vd* Display Config & EDRAM Emulation         │  │
│  │   • Ring Buffer (0xA758E5C0) & PM4 Command Flow  │  │
│  │   • Plume / Vulkan Swapchain Presentation        │  │
│  └────────────────────────┬─────────────────────────┘  │
│                           │                            │
│            ┌──────────────┴──────────────┐             │
│            ▼                             ▼             │
│   [ Linux / x86_64 ]            [ Android / AArch64 ]  │
│   (Active Target)               (Roadmapped Target)    │
│                                                        │
└────────────────────────────────────────────────────────┘

```

---

## Technical Milestones & Implemented Fixes

### 1. Memory Arena, Kernel ABI & CRT Stabilization

* **Memory Layout & Zeroing**: Mapped the full 512 MB physical guest memory arena at base `0x80000000`. Initialized and zeroed the 17.9 MB Xenon data section, reaching the main CRT entry point at `0x83639888`.
* **AltiVec `dcbzl` Memory Overrun Prevention**: XenonRecomp converts PowerPC AltiVec cache-zeroing instructions (`dcbzl`) into 128-byte block operations (`memset(base + ((dst + offset) & ~127), 0, 128)`). On unaligned boundary buffers, this routine previously overwrote adjacent heap metadata. Replaced unaligned memory clear calls with bounded memory copies.
* **Worker Thread Suspension & Semaphore Synchronization**: Resolved worker thread creation loops. Updated `NtCreateTimer`, `NtSetTimerEx`, `NtCancelTimer`, and `NtCreateMutant` to register valid dispatcher objects. Preserved `0x1` (`CREATE_SUSPENDED`) flags during `ExCreateThread`, and updated `NtResumeThread` and `KeResumeThread` to resolve guest thread IDs via the `GuestThreadHandle` registry rather than treating handle values directly as pointers.

### 2. Thread-Local Allocator Synchronization

* **Root Cause**: RAGE stores subsystem memory allocators inside the Processor Control Region (PCR) Thread-Local Storage array:
* `TLS + 116` (`0x74`): Dictionary and string allocator.
* `TLS + 120` (`0x78`): Active scoped allocator (`sysMemAllocator`).
* `TLS + 132` (`0x84`): Global fallback allocator.


* **Resolution**: The main thread initialized these fields inside `sub_8255DD58`, but newly spawned worker threads received zeroed TLS blocks. Calling `sub_8321D9F8` or `sub_83213570` caused null pointer dereferences and invalid indirect jumps (`PPC_CALL_INDIRECT_FUNC(0)`). TLS slots 116, 120, and 132 are now initialized to fallback allocator `0x83887168` across all threads, resolving worker thread segmentation faults.

### 3. Native Variadic ABI (`GuestVaList`)

* Replaced the MSVC CRT `vsnprintf` (`sub_823F4118`) jump-table routines that failed on unmapped branch targets (`0x823F4644` for `%s` and `0x823F4864` for `%x`).
* Implemented a custom PowerPC 32-bit big-endian variadic formatter adhering to the Xbox 360 ABI (8-byte aligned slots: `(argptr + 7) & ~7`, 32-bit arguments at offset `+4`, 64-bit arguments across all 8 bytes). Game statistics and hash strings (such as `SP0_TOTAL_CASH`, `SP1_TOTAL_CASH`, and `SP0_CHAR_XP_FM`) now format correctly.

### 4. RPF7 Virtual File System (VFS)

* Added support for the big-endian RPF7 (`0x52504637`) archive format used by GTA V on Xbox 360.
* Verified mounting and Table of Contents (TOC) parsing for:
* `xbox360a.rpf` (155 entries)
* `xbox360b.rpf` (141 entries)
* `common.rpf` (streaming `preload.list` at `0x75A000` and `im.fxc` at `0xA90000`)


* Replaced the 17-case XML token parser jump tables at `sub_831CB460` and `sub_831CC260` with native C++ switch implementations, enabling successful parsing of `installpartitions.meta`.

### 5. Graphics Bootstrap & Ring Buffer Initialization

* Configured display modes and parameters in `sub_82220140` and `sub_822D41E8`.
* Added stub implementations for `VdRetrainEDRAM` and `VdRetrainEDRAMWorker` to complete memory calibration passes.
* Initialized the GPU command ring buffer at guest address `0xA758E5C0` (8 KB buffer, writeback register at `0x0000003C`).
* The engine captures and consumes Type 3 PM4 command packets from RAGE (`ME_INIT`, opcode `0x48`, count 18) while advancing read pointers in lockstep with the CPU.

---

## Current Status

```
[✓] Step 1: Toolchain, Linker & Base Binary Setup (100%)
[✓] Step 2: Memory Arena, Allocators & CRT Entry (100%)
[✓] Step 3: Threading Model, Sync Primitives & ABI (100%)
[✓] Step 4: Base VFS & RPF7 Archive Parsing (85%)
[▶] Step 5: Shader Preloading & Disc 1 Partition Mount (In Progress)
[ ] Step 6: Full PM4 Command Packet Interpretation & Draw Loop
[ ] Step 7: Title Screen & Legal Splash Presentation
[ ] Step 8: Android / AArch64 Cross-Compilation & Port (Roadmapped)

```

| Subsystem | Progress | Status | Notes |
| --- | --- | --- | --- |
| **Toolchain & Link** | **100%** | Stable | Compiles cleanly to a 554 MB native binary. |
| **Memory & Allocators** | **100%** | Stable | Hooked `sysMemAllocator`; TLS slots 116, 120, and 132 mapped. |
| **Threading & Sync** | **95%** | Stable | 64-thread worker pool handles wait on semaphores; zero thread leaks. |
| **Variadic CRT Formatting** | **100%** | Stable | Native `GuestVaList` formats engine strings without indirect jump faults. |
| **VFS & Archive Ingestion** | **85%** | Active | RPF7 TOCs mounted; parsing `installpartitions.meta` and `common.rpf`. |
| **GPU Command Ring Buffer** | **45%** | Active | Ring buffer active (`0xA758E5C0`); `ME_INIT` PM4 packets parsed. |
| **Vulkan Presentation** | **30%** | Active | Swapchain configured; awaiting draw loop handoff. |
| **Android / AArch64 Port** | **0%** | Roadmapped | Planned post-desktop stabilization via Android NDK & mobile Vulkan. |

---

## Active Development Focus

1. **Resolving Shader Preload Dependencies**:
* Complete parsing of entries listed in `preload.list` from `common.rpf`.
* Normalize shader path redirection to eliminate path doubling (e.g., `fxl_final_final`) and support `.fxc` lookups (such as `rage_fastmipmap.fxc` and `im.fxc`).


2. **Disc 1 STFS Content Integration**:
* Mount the four Disc 1 install packages (`part0.rpf` through `part3.rpf` located in `545408A7/00000002/`).
* Route `XamContentCreateEnumerator` and `XamContentCreateEx` queries to point directly to extracted install data so the world streaming engine initializes without halting.


3. **PM4 Indirect Buffer Execution**:
* Parse downstream Type 3 PM4 command packets (`PM4_INDIRECT_BUFFER`, `PM4_SET_CONSTANT`, `PM4_DRAW_INDX`).
* Forward VBlank and vertical refresh signals through `VdSwap` and `VdCallGraphicsNotificationRoutines` to advance frames past initialization wait loops.



---

## Platform Roadmap

### 1. Linux / x86_64 (Current Milestone)

* Achieve stable runtime execution through the intro splash screens and title sequence.
* Bring up the in-game world rendering and streaming pipelines on desktop Vulkan backends.
* Validate performance targets on lower-tier PC configurations (e.g., 11th Gen Core i3 with Intel UHD Graphics and dual-channel memory).

### 2. Android / AArch64 (Post-Stabilization Phase)

* **Native Toolchain Cross-Compilation**: Leverage the modular C++ translation generated by `XenonRecomp` to target `aarch64-linux-android` directly via the Android NDK (Clang), bypassing any x86-to-ARM CPU emulation layers.
* **Mobile Vulkan & GPU Driver Adaptation**: Adapt the Plume Vulkan pipeline for mobile tile-based deferred renderers (TBDR), tuning texture untile routines and handling driver-specific extensions across Qualcomm Adreno and ARM Mali GPUs.
* **Android Platform Front-End**: Implement an Android frontend shell with native touch overlay controls, external gamepad polling, OpenSL ES / AAudio sound output, and scoped storage asset mapping.
* **Target Hardware**: High-performance mobile chipsets (Snapdragon 8 Gen 2 / Gen 3 / Gen 4, Dimensity 9300+) utilizing LPDDR5X bandwidth within the lightweight 512 MB guest console footprint.

---

## Building & Running

### Prerequisites

* Clang / LLVM toolchain supporting C++20
* Ninja build system
* Vulkan SDK (1.3+)
* SDL2 development libraries

### Building

To build with managed thread priority and I/O scheduling:

```bash
nice -n 19 ionice -c 3 ninja -C build -j 3 -l 4.0 FiveInvader

```

### Running Test Traces

Run with a process timeout and pass the extracted game directory path:

```bash
timeout -s SIGKILL 12s nice -n 19 ionice -c 3 ./build/FiveInvader/FiveInvader \
  --game-dir /path/to/extracted_disc2

```

*Note: The game window initializes minimized (`SDL_WINDOW_MINIMIZED`) during automated testing runs to prevent focus changes during debugging.*
