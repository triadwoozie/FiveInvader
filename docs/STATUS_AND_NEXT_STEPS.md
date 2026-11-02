# Status and Next Steps

## Current Progress & Milestones

1. **MultiAllocator Layer Delegation in `sub_8255DC20`**:
   - Resolved heap exhaustion and o1heap free-list corruption. RAGE's `operator new` (`sub_8255DC20`) delegates across allocator layers 0 through `numLayers - 1` (where `0x83887168` has 5 registered layers). Allocations are cleanly satisfied by Layer 1 (`0x83887190`, main heap) and freed natively by `sub_83194238`, dropping raw `AllocPhysical` calls from 22,000+ to ~30 system pools with zero host heap corruptions.

2. **`atMap::Find` Cycle and Depth Protection (`sub_82EF3FA0`)**:
   - Resolved spinlock hang in `sub_82EF3FA0` (`atMap::Find`) by implementing a cycle-guarded hook with a max depth limit (256 entries), patched into `PatchSyncPrimitives`.

3. **XML DOM Sibling Unlink Bounds & Pointer Validation (`sub_831D0C30`)**:
   - Resolved recursive XML tree teardown unlinking loop in `sub_831D0C30` (`ppc_recomp.263.cpp:loc_831D0C60`), checking node pointer ranges and NULL terminators. Codified in `tools/patch_ppc_xml_switch.py`.

4. **Heap Double-Free Guarding & Dynamic Symbol Inspection**:
   - Added invalid/double-free pointer poisoning in `heap.cpp` and boundary-checked dynamic vtable symbol inspection in `xdm.h`.

## Next Steps

1. Continue tracing the XML tree teardown in `sub_8344D068` / `sub_831B9330` / `sub_831D0C30` and verify complete return of `gameconfig.xml` loader.
2. Advance to the worker synchronization array across `0xC21C0CF0`–`0xC21C0E70` in `sub_82577590` and satisfy the completion barrier for State 1 -> State 2 transition.
3. Once State 2 is reached, monitor Scaleform frontend initialization and trace initial PM4 draw calls (`PM4_DRAW_INDX` / `PM4_DRAW_INDX_2`) into Vulkan render commands.

