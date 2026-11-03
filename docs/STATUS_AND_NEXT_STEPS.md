# Status and Next Steps

## Current Progress & Milestones

1. **MultiAllocator Layer Delegation in `sub_8255DC20`**:
   - Resolved heap exhaustion and o1heap free-list corruption. RAGE's `operator new` (`sub_8255DC20`) delegates across allocator layers 0 through `numLayers - 1` (where `0x83887168` has 5 registered layers). Allocations are cleanly satisfied by Layer 1 (`0x83887190`, main heap) and freed natively by `sub_83194238`, dropping raw `AllocPhysical` calls from 22,000+ to ~30 system pools with zero host heap corruptions.

2. **`atMap::Find` Cycle and Depth Protection (`sub_82EF3FA0` and `sub_827E1D70`)**:
   - Resolved spinlock hang and invalid bucket index crash in both variants of `atMap::Find` (`sub_82EF3FA0` and `sub_827E1D70`) by implementing cycle-guarded hooks with bounds checking and max depth limits (256 entries), registered in `PatchSyncPrimitives`.

3. **XML DOM Teardown & Indirect Call Guards (`sub_831D0C30`, `sub_831B9330`, `loc_831CED8C`)**:
   - Implemented non-reentrant, cycle-safe XML tree node and child destructors in `sub_831D0C30` and `sub_831B9330`.
   - Guarded indirect call dispatches at `0x831CED88` / `0x831CED90` and `0x831D0B28` against invalid token addresses, codified in `tools/patch_ppc_xml_switch.py`.

4. **Heap Double-Free Guarding & Physical Allocation Tracking**:
   - Added active physical allocation tracking `s_activePhysAllocations` in `heap.cpp` to prevent double-free corruptions in `o1heap`.

5. **Worker Stream Reader & Array Bounds Safeguards (`sub_831E2DB8`, `sub_8321D020`)**:
   - Fixed stream buffer underflow in `ppc_recomp.265.cpp:loc_831E2F1C` where unread byte count <= 0 caused signed comparison underflow to `0xFFFFFFFF` in `sub_823CFAF0`.
   - Added array pointer validation in `ppc_recomp.269.cpp:sub_8321D020` guarding against invalid offsets (`r10 < 0x10000 || r10 >= PPC_MEMORY_SIZE - 4`), preventing SIGSEGV in worker threads during resource loading. Codified in `tools/patch_ppc_xml_switch.py`.

6. **Worker Synchronization Barrier Broken (`sub_830E0730` / `sub_830D6CD0`)**:
   - Identified and resolved the root cause of the State 1 stall at Step 4 (`sub_825FE880` -> `sub_830D6CD0`).
   - In `ppc_recomp.246.cpp:sub_830E0730`, XenonRecomp had commented out PowerPC `bdzf` branch instructions in the task queue dispatch loop, causing task type 2 (which writes completion byte `100(r31) = 1` at `loc_830E0934`) to fall through into type 4 teardown.
   - Replaced the broken dispatch with a native C++ switch table routing types 0–4 directly to their respective case handlers (`loc_830E0878`, `loc_830E0934`, `loc_830E0948`, `loc_830E0954`).
   - With completion signaled, the main thread broke out of the Step 4 polling loop and executed all subsequent initialization steps: **Steps 5 through 28** of `sub_82577590` completed! Codified in `tools/patch_ppc_xml_switch.py`.

7. **`atMap::Find` Memory Bounds Safety Guarding (`sub_82273368`)**:
   - Added memory bounds checks and bucket/node pointer validation in `ppc_recomp.6.cpp:sub_82273368` (`ctx.r3`, `ctx.r8`, `ctx.r11`), along with a cycle guard (`loop_guard > 4096`), preventing invalid pointer dereferences when searching unpopulated or corrupt map buckets during Step 7 (`sub_83447AD0`). Codified in `tools/patch_ppc_xml_switch.py`.

8. **Restored CTR Switch Dispatches & Loop Underflow Guards (`ppc_recomp.269.cpp`, `ppc_recomp.268.cpp`)**:
   - Restored 4 `bdzf` CTR-decrement switch dispatch tables in XML/parMember schema parsing (`sub_83221078`, `sub_83226768`, `sub_8322B7B8`).
   - Prevented 4.2 billion iteration signed underflow loop at `loc_83221538` where `r30 <= 0` wrapped to `0xFFFFFFFF`, eliminating millions of bogus indirect calls to `0x7E000000`.
   - Capped `UnmappedIndirectHandler` logging in `memory.cpp` using atomic CAS to prevent log explosion and wrap-around.
   - Added null schema pointer guards in `ppc_recomp.268.cpp:sub_832193E8` and `sub_83219480`. Codified in `tools/patch_ppc_xml_switch.py`.

9. **Global `atMap::Find` Bounds Safeguards (`sub_827BE448` & `sub_827BE4C0`)**:
   - Added bounds checking and circular loop guard to `ppc_recomp.77.cpp:sub_827BE448` and `sub_827BE4C0` (`atMap::Find` for global map `*(0x83C1D204)`), preventing null bucket table dereference and crash. Codified in `tools/patch_ppc_xml_switch.py`.

10. **Bink Movie & Shader Initialization Reached (`rage_bink.fxc`, `rockstar_logos.bik`)**:
   - Engine advanced cleanly through initialization Steps 1, 2, 3, and 4 (`sub_82F3A5B8`, `sub_828FD100`, `sub_82918D28`, `sub_825FE880`).
   - Successfully loaded and mapped shader `X:\GTA5\build\dev\fxl_final_final\rage_bink.fxc` and movie `platform:\movies\rockstar_logos.bik`.

11. **PPC Function Table Protection Fix, CTR Switches & Task Pointer Safeguards**:
   - Restored CTR decrement switch table in `ppc_recomp.47.cpp:sub_825FE880` (`patch_recomp47_switch_dispatch`).
   - Guarded event count in `ppc_recomp.246.cpp:sub_830DB378` (`r30 <= 4096`) and lookup loop in `ppc_recomp.372.cpp:sub_83757BF0` (`r7 <= 4096`, pointer bounds, iteration cap).
   - Eliminated OOB dereference crash in `ppc_recomp.28.cpp:sub_824C7980` by guarding sentinel `0xFFFFFFFF` pointer entries in task queue iterations.
   - Removed `mprotect(..., PROT_READ)` on the PPC recomp function table in `FiveInvader/kernel/memory.cpp` and `imports.cpp`, which previously collided with guest physical allocations (e.g. `0x86376600`), causing page permission SIGSEGV on guest write.
   - Reached Test Pass 32: 30+ full seconds execution with 0 crashes, 0 segfaults, and >1,200 swapchain presents.

## Next Steps

1. Resolve worker task queue signaling for `sub_830D6CD0` (semaphore handle `0xC1200C70` and task completion at `loc_830E0934`).
2. Advance through movie playback and Scaleform frontend initialization to active 3D/Scaleform draw calls (`PM4_DRAW_INDX` / `PM4_DRAW_INDX_2`).


