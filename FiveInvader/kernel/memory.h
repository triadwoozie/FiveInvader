#pragma once

#ifndef _WIN32
#define MEM_COMMIT  0x00001000  
#define MEM_RESERVE 0x00002000  
#endif

#include <cstdint>
#include <cstddef>
#include <cassert>

typedef void PPCFunc(struct PPCContext& __restrict__ ctx, uint8_t* base);
extern PPCFunc** g_ppcFunctionTable;

inline PPCFunc*& PpcLookupFuncHelper(uint32_t target) {
    static PPCFunc* s_dummyNullFunc = nullptr;
    if (target >= PPC_CODE_BASE && target < (PPC_CODE_BASE + PPC_CODE_SIZE) && ((target & 3) == 0)) {
        if (g_ppcFunctionTable)
            return g_ppcFunctionTable[(target - PPC_CODE_BASE) >> 2];
    }
    s_dummyNullFunc = nullptr;
    return s_dummyNullFunc;
}

#ifdef PPC_LOOKUP_FUNC
#undef PPC_LOOKUP_FUNC
#endif
#define PPC_LOOKUP_FUNC(x, y) PpcLookupFuncHelper(static_cast<uint32_t>(y))

struct Memory
{
    uint8_t* base{};

    Memory();

    bool IsInMemoryRange(const void* host) const noexcept
    {
        return host >= base && host < (base + PPC_MEMORY_SIZE);
    }

    void* Translate(size_t offset) const noexcept
    {
        if (offset)
            assert(offset < PPC_MEMORY_SIZE);

        return base + offset;
    }

    uint32_t MapVirtual(const void* host) const noexcept
    {
        if (host)
            assert(IsInMemoryRange(host));

        return static_cast<uint32_t>(static_cast<const uint8_t*>(host) - base);
    }

    PPCFunc* FindFunction(uint32_t guest) const noexcept
    {
        if (guest >= PPC_CODE_BASE && guest < (PPC_CODE_BASE + PPC_CODE_SIZE) && ((guest & 3) == 0)) {
            return g_ppcFunctionTable ? g_ppcFunctionTable[(guest - PPC_CODE_BASE) >> 2] : nullptr;
        }
        return nullptr;
    }

    void InsertFunction(uint32_t guest, PPCFunc* host)
    {
        if (guest >= PPC_CODE_BASE && guest < (PPC_CODE_BASE + PPC_CODE_SIZE) && ((guest & 3) == 0)) {
            if (g_ppcFunctionTable)
                g_ppcFunctionTable[(guest - PPC_CODE_BASE) >> 2] = host;
        }
    }
};

extern "C" void* MmGetHostAddress(uint32_t ptr);
extern Memory g_memory;
