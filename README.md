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
│              ┌────────────┴─────────────┐              │
│              ▼                          ▼              │
│    [ Linux / x86_64 ]          [ Android / AArch64 ]   │
│    (Active Target)            (Roadmapped Target)      │
│                                                        │
└────────────────────────────────────────────────────────┘

```

---

## Technical Milestones & Implemented Fixes

### 1. Memory Arena, Kernel ABI & CRT Stabilization

* **Memory Layout & Zeroing**: Mapped the full 512 MB physical guest memory arena at base `0x80000000`. Initialized and zeroed the 17.9 MB Xenon data section, reaching the main CRT entry point at `0x83639888`.
* **AltiVec `dcbzl` Memory Overrun Prevention**: Replaced unaligned AltiVec cache-zeroing instructions (`dcbzl`) with bounded memory copies to prevent corrupted adjacent heap metadata.
* **Worker Thread Suspension, Dispatcher Caching & Contention Elimination**: Resolved worker thread creation loops and spinlock starvation. Preserved `0x1` (`CREATE_SUSPENDED`) flags during `ExCreateThread`, resolved thread IDs via `GuestThreadHandle`, introduced `s_dispatcherMap` in `xdm.h` to cache kernel dispatcher objects across high-frequency `KeWaitForSingleObject` queries, and implemented tiered wait backoff (`yield` $\rightarrow$ $50\,\mu\text{s}$ $\rightarrow$ $1\,\text{ms}$) in `RtlEnterCriticalSection` and `NtWaitForSingleObjectEx`. Preserved caller non-volatile registers (`r28`–`r31`) across `PPC_CALL_INDIRECT_FUNC` in `sub_83641190` (`ppc_recomp.330.cpp`), eliminating spinlock contention on `cs=0x83878764`.

### 2. Thread-Local Allocators & SBA Buddy Churn Elimination

* **PCR/TLS Subsystem Mapping**: Resolved worker thread null pointer dereferences across `TLS + 116` (dictionary/string allocator), `TLS + 120` (scoped `sysMemAllocator`), and `TLS + 132` (fallback) by mapping all spawned threads to fallback allocator `0x83887168`.
* **Small Block Allocator (SBA) Loop Resolution**: Addressed a critical runaway loop between `sub_83194A08` and `sub_8248EAE0` that produced over 3.4 million allocation/deallocation attempts in 20 seconds. Handled the `0xFFFE` (65534) sentinel return on exhausted/uninitialized SBA buckets and patched circular doubly-linked-list unlinking in `imports.cpp`.
* **Heap Free, `o1heap` Decoupling & Cycle Detection (`sub_8255DC88`)**: Decoupled `o1heap` metadata from guest framebuffers, implemented multi-allocator layer delegation, and mitigated circular pointer traversals in secondary heap teardowns and chunk allocation loops (`sub_83508B48` in `ppc_recomp.311.cpp`).
* **Guest Memory Bounds & Copy Guarding (`sub_823CFAF0`)**: Mitigated runaway buffer transfers where uninitialized or negative error return values (`-1` / `0xFFFFFFFF`) triggered heap exhaustion or segfaults at the 4 GB guest boundary (`0x200000000`). Bounded memory copy operations directly through a strong runtime override `PPC_FUNC(sub_823CFAF0)` registered via `PatchFuncMapping` without mutating recompiled PPC translation units.
* **Oversized & Invalid Allocation Guards**: Reject runaway allocator calls (including `0xFEFFFFFF` / ~4.2 GB sizes caused by unsigned underflow from negative status codes) and guard invalid low-address resource streaming pointers (`< 0x40000000`) within `sub_83215B40` to maintain heap integrity.

### 3. Indirect Dispatch Guarding & Jump Table Restoration

* **PPC Indirect Function Call Guards**: Unprotected the PPC function pointer table and added strict bounds checking across object pointers, `atMap` buckets (`sub_82273368`, `sub_827BE4C0`), and vtable targets in `imports.cpp` to prevent segmentation faults on unmapped indirect branches (`bctrl` via `ctr.u32`).
* **String Formatting Switch-Table Interception**: PowerPC switch tables in CRT string formatting routines (`vsnprintf` / `sub_8362B5E0` targeting `0x8362BAB4`) were routing into `UnmappedIndirectHandler`. Added explicit symbol mappings and guarded forward wrappers for jump table targets (`sub_829855F8`, `sub_82985550`, `sub_831E2DB8`, `sub_831E3240`, `sub_829856D0`).
* **XML Parser, Event Routing & Worker Jump-Table Recovery**: Reconstructed missing switch jump-table cases, CTR switches, and mid-function branch-target labels via `tools/patch_ppc_xml_switch.py`, including XML dispatch `0x831CB4A8`, worker variant `0x8321D35C`, target range `0x83223EDC`–`0x83223EFC`, `0x8362BE00`, and a 14-way intra-function event routing switch table in `sub_824C7980` (`ppc_recomp.28.cpp`).
* **State 1 $\rightarrow$ State 2 Breakthrough**: Replaced broken `bdzf` PowerPC branch instructions in `sub_830E0730` (`ppc_recomp.246.cpp`) with a native switch table to cleanly signal task type 2 batch completion at `loc_830E0934`. The engine now completes all 30 steps of the `sub_82577590` barrier and transitions into `sub_825777D0` (**State 2**), parsing Scaleform preallocations, loading control metadata, and mounting `platform:/data/lang/american.rpf`.

### 4. RPF7 Virtual File System (VFS) & Asset Parsing

* **Archive Extraction & Ingestion**: Supported big-endian RPF7 (`0x52504637`) archive mounting and TOC traversal for `xbox360a.rpf`, `xbox360b.rpf`, `common.rpf`, and localization archives (`american.rpf`).
* **Loose-File VFS, Shader Resolution & Lookup Filtering**: Implemented loose-file virtual filesystem routing for extracted Disc 2 assets, resolving primary game configurations (`gameconfig.xml`, `installpartitions.meta`, `standard_scripted.meta`) and native compiled shader packages (`im.fxc`, `rage_bink.fxc`) directly from the extracted `fxl_final_final/` trees. Guarded `sub_831EDB68`, `sub_831E2570`, and `sub_831E2460` (`ppc_recomp.265.cpp`) against non-printable ASCII strings and corrupt shader counts (`> 350`), eliminating 87,000+ spurious VFS file lookups and mapping `STATUS_NO_SUCH_FILE` (`0xC000000F`) to `ERROR_FILE_NOT_FOUND`.
* **Radix Trie & XML Ingestion**: Native switch implementations and XML unlink safety guards bypassed MSVC CRT jump-table crashes in XML token parsing (`sub_831CB460`, `sub_831CC260`, `sub_8344D068`), enabling end-to-end ingestion of `gameconfig.xml`, `installpartitions.meta`, and `standard_scripted.meta`.

### 5. Allocator, Stream Reader & Dispatch Stabilization

* **Large-Block Free-List, Division-by-Zero & Critical Section Stabilization**: Bounded the circular free-list walk in `sub_8363E870`, routed corrupt-list exits through allocator cleanup, and added atomic refcount guards in `sub_82556BD0` and `sub_82556C70`. Added zero-divisor guards to `divw` instructions in `sub_8321D020` (`ppc_recomp.269.cpp`) to prevent `SIGFPE` crashes when `ctx.r11 == 0`.
* **Stream and Allocation Bounds**: Prevented worker stream buffer underflows, guarded array cleanup, and rejected resource-stream pointers below `0x40000000` or outside guest memory. `sub_8255DC20` and `sub_8363FCB0` reject oversized unsigned-underflow values (including `0xFEFFFFFF`) before they can become runaway allocations or copies.
* **Automated Recompiled-Code Patches**: `tools/patch_ppc_xml_switch.py` applies idempotent AST/text transformations across `ppc_recomp.6.cpp`, `.15.cpp`, `.28.cpp`, `.36.cpp`, `.77.cpp`, `.246.cpp`, `.263.cpp`, `.265.cpp`, `.268.cpp`, `.269.cpp`, `.288.cpp`, `.311.cpp`, `.328.cpp`, and `.330.cpp`.

### 6. Graphics Bootstrap & Presentation Controls

* **Command Stream, EDRAM & Presentation Stability**: Ring buffer active at `0xA758E5C0` (8 KB buffer); consumes Type 3 PM4 command packets (`ME_INIT`, opcode `0x48`, count 18) with framebuffer bounds and safety guards active. Sustained multi-threaded execution verified across 21 concurrent worker threads with over 167,400 swapchain video presents (`VdSwap`) and zero crashes.
* **Gated Window Visibility**: By default, the window stays hidden until active rendering occurs (`VdSwap > 0 || PM4 > 0`). `--window-gate immediate` can show it at startup for diagnostics.

---

## Companion Launcher (Electron + React)

FiveInvader includes a native companion launcher matching modern recompilation dashboards (Bento-grid modular design, Scaleform dark palette with Siren Red `#FF1E27` and Police Blue `#0066FF` accents):

* **Bento Status Dashboard**: Runtime status, game-data inspection, controller configuration, streaming logs, and a live CLI preview.
* **Persisted Runtime Options**: Editable primary game path, optional secondary loose-file/Disc 1 stream path, resolution presets (`1280x720`, `1920x1080`, `2560x1440`), `30/60/uncapped` FPS, FIFO/Mailbox/Immediate selection, and immediate/render-gated window visibility.
* **Debug & Test Controls**: MangoHud and Vulkan validation environment toggles, allocator/VFS telemetry, optional run-log files, and `None/20s/30s/60s` launcher timeouts. Mailbox currently maps to FIFO because the render interface exposes only enabled/disabled VSync.

### Launcher Screenshots

---

## Current Status

```
[✓] Step 1: Toolchain, Linker & Base Binary Setup (100%)
[✓] Step 2: Memory Arena, Allocators & CRT Entry (100%)
[✓] Step 3: Threading Model, Sync Primitives & ABI (100%)
[✓] Step 4: Base VFS, Loose Shaders & XML Metadata Ingestion (100%)
[✓] Step 5: SBA Buddy Allocator & Heap Cycle Resolution (100%)
[✓] Step 6: Indirect Dispatch Guarding & Jump-Table Restorations (100%)
[✓] Step 7: Async Worker Semaphore Barrier & State 2 Hand-off (100%)
[▶] Step 8: Full PM4 Command Packet Interpretation & Draw Loop (In Progress)
[ ] Step 9: Title Screen & Legal Splash Presentation
[ ] Step 10: Android / AArch64 Cross-Compilation & Port (Roadmapped)

```

| Subsystem | Progress | Status | Notes |
| --- | --- | --- | --- |
| **Toolchain & Link** | **100%** | Stable | Compiles cleanly to a 554 MB native binary via Ninja/Clang. |
| **Memory & Allocators** | **100%** | Stable | `o1heap` decoupled; SBA buddy loops patched; TLS slots 116, 120, 132 mapped; copy overflow and oversized allocations guarded. |
| **Threading & Sync** | **100%** | Stable | `s_dispatcherMap` caching active; `cs=0x83878764` spinlock contention eliminated; 21 concurrent worker threads stable. |
| **Variadic ABI & Dispatches** | **100%** | Stable | Native `GuestVaList` active; libc jump tables, XML switches, event switches (`sub_824C7980`), and `divw` guards mapped. |
| **VFS & Metadata** | **100%** | Stable | `gameconfig.xml`, compiled shaders, RPF7 TOCs, and `american.rpf` mounted; 87,000+ bogus shader lookups eliminated. |
| **RAGE State Machine** | **90%** | Active | Completed all 30 steps of `sub_82577590`; transitioned into State 2 (`sub_825777D0`) with Scaleform preallocations parsed. |
| **GPU Ring Buffer & GFX** | **65%** | Active | Framebuffer safety guards active; 167,400+ video presents (`VdSwap`) verified; advancing to Bink/Scaleform `PM4_DRAW_INDX`. |
| **Companion Launcher** | **95%** | Stable | Persisted paths, display/window options, debug toggles, timeouts, run logs, and screenshots documented. |
| **Android / AArch64 Port** | **0%** | Roadmapped | Planned post-desktop stabilization via Android NDK & mobile Vulkan. |

---

For the exact current State 2 progression and next rendering milestones, see [`docs/STATUS_AND_NEXT_STEPS.md`](https://www.google.com/search?q=docs/STATUS_AND_NEXT_STEPS.md).

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
