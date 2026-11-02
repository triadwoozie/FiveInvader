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

## Next Steps

1. Verify `gameconfig.xml` loader (`sub_8344D068`) cleanly returns.
2. Advance through the worker synchronization barrier across `0xC21C0CF0`–`0xC21C0E70` in `sub_82577590` to execute the State 1 -> State 2 transition.
3. Monitor Scaleform frontend initialization and trace initial PM4 draw calls (`PM4_DRAW_INDX` / `PM4_DRAW_INDX_2`) into Vulkan render commands.

