#include <stdafx.h>
#include "xdm.h"
#include "freelist.h"
#include <unordered_set>

Mutex g_kernelLock;

static std::unordered_set<uint32_t> s_kernelHandles;
static Mutex s_kernelHandlesLock;

void RegisterKernelObject(uint32_t handle)
{
    std::lock_guard lock(s_kernelHandlesLock);
    s_kernelHandles.insert(handle);
}

void UnregisterKernelObject(uint32_t handle)
{
    std::lock_guard lock(s_kernelHandlesLock);
    s_kernelHandles.erase(handle);
}

bool IsValidKernelHandle(uint32_t handle)
{
    std::lock_guard lock(s_kernelHandlesLock);
    return s_kernelHandles.find(handle) != s_kernelHandles.end();
}

void DestroyKernelObject(KernelObject* obj)
{
    if (obj)
    {
        UnregisterKernelObject(g_memory.MapVirtual(obj));
        obj->~KernelObject();
        g_userHeap.Free(obj);
    }
}

uint32_t GetKernelHandle(KernelObject* obj)
{
    assert(obj != GetInvalidKernelObject());
    return g_memory.MapVirtual(obj);
}

void DestroyKernelObject(uint32_t handle)
{
    if (IsValidKernelHandle(handle))
    {
        DestroyKernelObject(GetKernelObject(handle));
    }
}

bool IsKernelObject(uint32_t handle)
{
    return (handle & 0x80000000) != 0;
}

bool IsKernelObject(void* obj)
{
    return IsKernelObject(g_memory.MapVirtual(obj));
}

bool IsInvalidKernelObject(void* obj)
{
    return obj == GetInvalidKernelObject();
}
