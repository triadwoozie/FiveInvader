#include <stdafx.h>
#include "memory.h"
#include <os/logger.h>

static constexpr size_t AlignDown(size_t value, size_t alignment) noexcept
{
    return value & ~(alignment - 1);
}

static constexpr size_t AlignUp(size_t value, size_t alignment) noexcept
{
    return (value + (alignment - 1)) & ~(alignment - 1);
}

#include <execinfo.h>

static void UnmappedIndirectHandler(PPCContext& ctx, uint8_t* base)
{
    static std::atomic<int> s_unmappedCount{0};
    if (s_unmappedCount++ < 25) {
        LOGF_ERROR("[PPC Indirect] Called UNMAPPED indirect function! target(r12)=0x{:08X} r1=0x{:08X}",
                   ctx.r12.u32, ctx.r1.u32);

        // Walk guest stack frames
        uint32_t sp = ctx.r1.u32;
        for (int frame = 0; frame < 16 && sp >= 0x70000000 && sp < 0x90000000; ++frame) {
            uint32_t old_sp = PPC_LOAD_U32(sp);
            if (old_sp <= sp || old_sp >= 0x90000000) break;
            uint32_t lr = PPC_LOAD_U32(old_sp - 8);
            LOGF_ERROR("  [Guest BT #{}] sp=0x{:08X} lr=0x{:08X}", frame, sp, lr);
            sp = old_sp;
        }

        void* callstack[32];
        int frames = backtrace(callstack, 32);
        char** strs = backtrace_symbols(callstack, frames);
        if (strs) {
            for (int i = 0; i < frames; ++i) {
                LOGF_ERROR("  [Host BT #{}] {}", i, strs[i]);
            }
            free(strs);
        }
    }
    // If a NULL function pointer was called, return -1 (error/invalid)
    // rather than 0 (which looks like success to functions checking != -1)
    if (ctx.r12.u32 == 0) {
        ctx.r3.s64 = -1;
    } else {
        ctx.r3.u64 = 0;
    }
}

void PPCHandleInvalidIndirectCall(PPCContext& ctx, uint8_t* base, uint32_t target)
{
    ctx.r12.u64 = target;
    UnmappedIndirectHandler(ctx, base);
}

Memory::Memory()
{
#ifdef _WIN32
    base = (uint8_t*)VirtualAlloc((void*)0x100000000ull, PPC_MEMORY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (base == nullptr)
        base = (uint8_t*)VirtualAlloc(nullptr, PPC_MEMORY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (base == nullptr)
        return;

    // Some titles (e.g., GTA V) legitimately touch low memory (including address 0).
    // Do not install a null-page guard in that case.
#else
    base = (uint8_t*)mmap((void*)0x100000000ull, PPC_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

    if (base == (uint8_t*)MAP_FAILED)
        base = (uint8_t*)mmap(NULL, PPC_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

    if (base == (uint8_t*)MAP_FAILED)
    {
        base = nullptr;
        return;
    }

    // Some titles (e.g., GTA V) legitimately touch low memory (including address 0).
    // Do not install a null-page guard in that case.
#endif

    // Pre-populate the indirect function lookup table with an unmapped handler
    // so any unmapped indirect call emits an actionable error instead of a null crash.
    PPCFunc** table = (PPCFunc**)(base + PPC_IMAGE_BASE + PPC_IMAGE_SIZE);
    constexpr size_t kFuncTableEntries = PPC_CODE_SIZE / 4;
    std::fill_n(table, kFuncTableEntries, &UnmappedIndirectHandler);

    for (size_t i = 0; PPCFuncMappings[i].guest != 0; i++)
    {
        if (PPCFuncMappings[i].host != nullptr)
            InsertFunction(PPCFuncMappings[i].guest, PPCFuncMappings[i].host);
    }

    // Protect the recomp function lookup table from guest writes.
    // If the game overwrites these host function pointers, it can lead to crashes that look like
    // invalid indirect branches / pointer-authentication failures on arm64e.
    constexpr size_t kPageSize = 0x1000;
    constexpr size_t kFuncTableOffset = PPC_IMAGE_BASE + PPC_IMAGE_SIZE;
    constexpr size_t kFuncTableSize = (PPC_CODE_SIZE * 2) + sizeof(PPCFunc*);
    const size_t protectBegin = AlignDown(kFuncTableOffset, kPageSize);
    const size_t protectEnd = AlignUp(kFuncTableOffset + kFuncTableSize, kPageSize);
    if (protectEnd > protectBegin)
    {
#ifdef _WIN32
        DWORD oldProtect{};
        VirtualProtect(base + protectBegin, protectEnd - protectBegin, PAGE_READONLY, &oldProtect);
#else
        mprotect(base + protectBegin, protectEnd - protectBegin, PROT_READ);
#endif
    }
}

void* MmGetHostAddress(uint32_t ptr)
{
    return g_memory.Translate(ptr);
}
