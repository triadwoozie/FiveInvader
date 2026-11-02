# FiveInvader

> [!WARNING]
> **Project Authorship & AI Attribution Notice**
> Approximately **90% of this codebase was authored, compiled, and debugged by Google's Antigravity**, operating under direct human supervision, technical direction, and architectural specification by [@triadwoozie](https://github.com/triadwoozie).

**FiveInvader** is an experimental native static recompilation of the Xbox 360 release of *Grand Theft Auto V* (Title ID: `545408A7`, Title Update & DLC build) targeting 64-bit Linux environments via Vulkan, with a native Android (AArch64) deployment roadmapped.

By pre-translating PowerPC Xenon machine code into native C++ using `XenonRecomp` and providing a dedicated runtime layer for Xbox 360 kernel and system interfaces, FiveInvader eliminates runtime JIT interpretation overhead. The runtime operates within the strict 512 MB guest physical footprint of the original 2013 RAGE engine, paired with a companion Electron + React launcher styled after modern recompilation suites (Bento grid dashboard, Scaleform-inspired styling, and runtime telemetry hooks).

---

## Architecture Overview

```
┌────────────────────────────────────────────────────────┐
│            FiveInvader Runtime Architecture            │
├────────────────────────────────────────────────────────┤
│                                                        │
│  [ Disc 2 / GoD Assets ]      [ Disc 1 Content Packs ] │
│  (common.rpf, xbox360a/b.rpf) (part0.rpf - part3.rpf)  │
│              │                             │           │
│              ▼                             ▼           │
│  ┌──────────────────────────────────────────────────┐  │
│  │            RAGE VFS & RPF7 Archive Parser        │  │
│  │    (fiDevice, fiStream, STFS Container Mounts)   │  │
│  └────────────────────────┬─────────────────────────┘  │
│                           ▼                            │
│  ┌──────────────────────────────────────────────────┐  │
│  │          Recompiled Guest PPC Binary (554 MB)    │  │
│  │        (XenonRecomp Native C++ Translation)      │  │
│  ├──────────────────────────────────────────────────┤  │
│  │  • Unified 512MB RAM Space (0x80000000 Base)     │  │
│  │  • Big-Endian GuestVaList Variadic ABI Engine    │  │
│  │  • PCR/TLS Subsystem Allocators (Slots 116/120)  │  │
│  │  • Buddy SBA Pool Clamps & Heap Cycle Mitigators │  │
│  │  • 64-Thread Worker Task Pool (CREATE_SUSPENDED) │  │
│  │  • Safe Indirect Branch / Jump Table Interceptors│  │
│  └────────────────────────┬─────────────────────────┘  │
│                           ▼                            │
│  ┌──────────────────────────────────────────────────┐  │
│  │     Graphics Pipeline & Command Processing       │  │
│  │    • Vd* Display Config & EDRAM Emulation        │  │
│  │    • Ring Buffer (0xA758E5C0) & PM4 Command Flow │  │
│  │    • Plume / Vulkan Swapchain Presentation       │  │
│  │    • Gated Window Presentation (VdSwap / PM4 > 0)│  │
│  └────────────────────────┬─────────────────────────┘  │
│                           │                            │
│             ┌─────────────┴─────────────┐              │
│             ▼                           ▼              │
│    [ Linux / x86_64 ]          [ Android / AArch64 ]   │
│    (Active Target)             (Roadmapped Target)     │
│                                                        │
└────────────────────────────────────────────────────────┘


```

---

## Technical Milestones & Implemented Fixes

### 1. Memory Arena, Kernel ABI & CRT Stabilization

* **Memory Layout & Zeroing**: Mapped the full 512 MB physical guest memory arena at base `0x80000000`. Initialized and zeroed the 17.9 MB Xenon data section, reaching the main CRT entry point at `0x83639888`.
* **AltiVec `dcbzl` Memory Overrun Prevention**: Replaced unaligned AltiVec cache-zeroing instructions (`dcbzl`) with bounded memory copies to prevent corrupted adjacent heap metadata.
* **Worker Thread Suspension & Semaphore Synchronization**: Resolved worker thread creation loops. Preserved `0x1` (`CREATE_SUSPENDED`) flags during `ExCreateThread`, resolved thread IDs via `GuestThreadHandle`, and patched `KeWaitForSingleObject` / `NtWaitForSingleObjectEx` fail-open hooks on boot synchronization handles (`0xC6053170`) to prevent infinite 16 ms/30 ms worker handshake stalls.

### 2. Thread-Local Allocators & SBA Buddy Churn Elimination

* **PCR/TLS Subsystem Mapping**: Resolved worker thread null pointer dereferences across `TLS + 116` (dictionary/string allocator), `TLS + 120` (scoped `sysMemAllocator`), and `TLS + 132` (fallback) by mapping all spawned threads to fallback allocator `0x83887168`.
* **Small Block Allocator (SBA) Loop Resolution**: Addressed a critical runaway loop between `sub_83194A08` and `sub_8248EAE0` that produced over 3.4 million allocation/deallocation attempts in 20 seconds. Handled the `0xFFFE` (65534) sentinel return on exhausted/uninitialized SBA buckets and patched circular doubly-linked-list unlinking in `imports.cpp`.
* **Heap Free / Cycle Detection (`sub_8255DC88`)**: Mitigated circular pointer traversals in secondary heap teardowns, allowing the engine to successfully transition out of heap initialization loops.
* **Guest Memory Bounds & Copy Guarding (`sub_823CFAF0`)**: Mitigated runaway buffer transfers where uninitialized or negative error return values (`-1` / `0xFFFFFFFF`) triggered heap exhaustion or segfaults at the 4 GB guest boundary (`0x200000000`). Bounded memory copy operations directly through a strong runtime override `PPC_FUNC(sub_823CFAF0)` registered via `PatchFuncMapping` without mutating recompiled PPC translation units.
* **Oversized & Invalid Allocation Clamping (`sub_8255DC20`)**: Trapped and clamped runaway allocator calls (including `0xFEFFFFFF` / ~4.2 GB allocations caused by unsigned underflow from negative status codes) and guarded invalid low-address resource streaming pointers (`< 0x40000000`) within `sub_83215B40` to maintain heap integrity.

### 3. Indirect Dispatch Guarding & Jump Table Restoration

* **PPC Indirect Function Call Guards**: Xenon indirect branch dispatch (`bctrl` via `ctr.u32`) was triggering segmentation faults at host addresses (e.g., `0xd739ac3` in `sub_8351A9E8`) due to unmapped pointers wrapping outside `PPC_CODE_BASE`. Implemented strict bounds checking across object pointers and vtable targets in `imports.cpp`.
* **String Formatting Switch-Table Interception**: PowerPC switch tables in CRT string formatting routines (`vsnprintf` / `sub_8362B5E0` targeting `0x8362BAB4`) were routing into `UnmappedIndirectHandler`. Added explicit symbol mappings and guarded forward wrappers for jump table targets (`sub_829855F8`, `sub_82985550`, `sub_831E2DB8`, `sub_831E3240`, `sub_829856D0`).
* **XML Parser & Worker Jump-Table Recovery**: Reconstructed missing switch jump-table cases and mid-function branch-target labels via automated generation patching (`tools/patch_ppc_xml_switch.py`). Resolved unmapped XML token dispatches at `0x831CB4A8` (with `loc_831CB4EC` and `loc_831CC2EC`), native switch branches in `sub_8321D020` (jump table `0x8321D35C`), and worker task-scheduling indirect branch targets (`0x83223EDC` / `0x83223EFC` in `ppc_recomp.269.cpp` and `0x8362BE00` in `ppc_recomp.328.cpp`).
* **State Transition Dispatch**: Successfully pushed execution out of RAGE State 1 (`sub_8229AD78`) into State 2 (`sub_82577270` / `sub_82985760` with `ctx.r4 = 2`), passing the `lwz r10, 216(r11)` indirect vtable dispatch.

### 4. RPF7 Virtual File System (VFS) & Asset Parsing

* **Archive Extraction & Ingestion**: Supported big-endian RPF7 (`0x52504637`) archive mounting and TOC traversal for `xbox360a.rpf`, `xbox360b.rpf`, and `common.rpf`.
* **Loose-File VFS & Shader Resolution**: Implemented loose-file virtual filesystem routing for extracted Disc 2 assets, resolving primary game configurations (`gameconfig.xml`, `installpartitions.meta`, `standard_scripted.meta`) and native compiled shader packages (`im.fxc`, `rage_bink.fxc`) directly from the extracted `fxl_final_final/` trees.
* **Radix Trie & XML Ingestion**: Native switch implementations bypassed MSVC CRT jump-table crashes in XML token parsing (`sub_831CB460`, `sub_831CC260`), enabling end-to-end ingestion of `installpartitions.meta` and `standard_scripted.meta`.

### 5. Graphics Bootstrap & Presentation Controls

* **Command Stream & EDRAM**: Ring buffer active at `0xA758E5C0` (8 KB buffer); consumes Type 3 PM4 command packets (`ME_INIT`, opcode `0x48`, count 18).
* **Gated Window Visibility**: Patched `game_window.cpp` to prevent premature blank/purple window display, enforcing window visibility strictly when active rendering occurs (`VdSwap > 0 || PM4 > 0`).
* **Presentation Pipeline Stability**: Engine runs continuously at 47–50 FPS presenting to the Vulkan swapchain (>1,400 presents across 30-second execution runs) with verified PM4 packet ingestion.

---

## Companion Launcher (Electron + React)

FiveInvader includes a native companion launcher matching modern recompilation dashboards (Bento-grid modular design, Scaleform dark palette with Siren Red `#FF1E27` and Police Blue `#0066FF` accents):

* **Bento Status Dashboard**: Real-time payloads, active resolution, backend status, and connected input mappings.
* **Hardware Profiles & Presets**: Authentic Xbox 360 defaults (`1280x720`, `30 FPS`, `FIFO Strict VSync`), standard PC (`1080p 60FPS Mailbox`), high-refresh, and custom profiles.
* **Linux Tooling Integration**: Native `MANGOHUD=1` environment injection toggle, CLI string generator, and Radix UI configuration persistence.

---

## Current Status

```
[✓] Step 1: Toolchain, Linker & Base Binary Setup (100%)
[✓] Step 2: Memory Arena, Allocators & CRT Entry (100%)
[✓] Step 3: Threading Model, Sync Primitives & ABI (100%)
[✓] Step 4: Base VFS, Loose Shaders & XML Metadata Ingestion (100%)
[✓] Step 5: SBA Buddy Allocator & Heap Cycle Resolution (100%)
[✓] Step 6: Indirect Dispatch Guarding & Jump-Table Restorations (100%)
[▶] Step 7: Async Worker Semaphore Barrier & State 2 Hand-off (In Progress)
[ ] Step 8: Full PM4 Command Packet Interpretation & Draw Loop
[ ] Step 9: Title Screen & Legal Splash Presentation
[ ] Step 10: Android / AArch64 Cross-Compilation & Port (Roadmapped)


```

| Subsystem | Progress | Status | Notes |
| --- | --- | --- | --- |
| **Toolchain & Link** | **100%** | Stable | Compiles cleanly to a 554 MB native binary via Ninja/Clang. |
| **Memory & Allocators** | **100%** | Stable | SBA buddy loops patched; TLS slots 116, 120, 132 mapped; copy overflow & ~4.2 GB allocations guarded. |
| **Threading & Sync** | **95%** | Active | 64-thread pool operational; tracing async completion handoff on worker semaphores (`0xC21C0CF0`–`0xC21C0E70`). |
| **Variadic ABI & Dispatches** | **100%** | Stable | Native `GuestVaList` active; libc jump tables, XML switch tables, and worker `bctr` targets mapped. |
| **VFS & Metadata** | **100%** | Stable | `gameconfig.xml`, `fxl_final_final/` shaders (`im.fxc`, `rage_bink.fxc`), and RPF7 TOCs verified. |
| **RAGE State Machine** | **90%** | Active | State 1 complete; resolving worker queue barrier to transition into State 2 intro pipeline. |
| **GPU Ring Buffer & GFX** | **55%** | Active | PM4 packet consumption active; swapchain presenting stably (>1,400 presents); gating to `VdSwap > 0`. |
| **Companion Launcher** | **95%** | Stable | Bento UI, presets (Xbox 360 / PC), telemetry bar, and MangoHud injection complete. |
| **Android / AArch64 Port** | **0%** | Roadmapped | Planned post-desktop stabilization via Android NDK & mobile Vulkan. |

---

## Building & Running

### Prerequisites

* Clang / LLVM toolchain supporting C++20
* Ninja build system
* Vulkan SDK (1.3+)
* SDL2 development libraries
* Node.js & npm (for building the companion launcher)

### Building the Native Engine

Build with low process and I/O scheduling priorities:

```bash
nice -n 19 ionice -c 3 ninja -C build -j 3 -l 4.0 FiveInvader


```

### Running Test Traces

Run with a process timeout and pass the extracted game directory:

```bash
timeout -s SIGKILL 30s nice -n 19 ionice -c 3 ./build/FiveInvader/FiveInvader \
  --game-dir /path/to/extracted_disc2


```

### Building & Launching the UI

```bash
cd launcher
npm install
npm run build
npm start


```
