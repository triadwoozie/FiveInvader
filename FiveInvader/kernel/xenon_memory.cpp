#include "xenon_memory.h"
#include <os/logger.h>
#include <cstring>

void InitializeXenonMemoryRegions(uint8_t* base) {
    LOG_WARNING("[Xenon] Initializing Xbox 360 memory regions per Xenon contract");
    
    // Zero ENTIRE PPC image data and BSS region in one contiguous call (0x82000000-0x83DC0000)
    // In GTA V (Disc 2), default.xex imageSize is 0x1DC0000 (31,195,136 bytes).
    // This covers all game data and BSS regions:
    // - Stream pool (0x82000000-0x82020000)
    // - XEX data (0x82020000-0x82120000)
    // - Code data (0x82120000-0x82A90000)
    // - Kernel runtime (0x82A90000-0x82AA0000)
    // - Game globals (0x82AA0000-0x83000000)
    // - Static data/BSS (0x83000000-0x83DC0000)
    //
    // Zeroing up to 0x83DC0000 ensures that uninitialized critical sections and BSS
    // locks (e.g. 0x83B9D368, 0x83C02A88) start at zero and avoid deadlocks.
    //
    // Size: 0x83DC0000 - 0x82000000 = 0x1DC0000 = 31,195,136 bytes (~29.75 MB)
    LOG_WARNING("[Xenon] Zeroing entire PPC data region: 0x82000000-0x83DC0000 (29.75 MB)");
    memset(base + 0x82000000, 0, 0x1DC0000);
    
    LOG_WARNING("[Xenon] Memory region initialization complete");
    LOG_WARNING("[Xenon] Total zeroed: 29.75 MB contiguous (0x82000000-0x83DC0000)");
}
