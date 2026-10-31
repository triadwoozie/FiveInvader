#pragma once
// Forward to the generated ppc_context.h in FiveInvaderLib
#include "../../FiveInvaderLib/ppc/ppc_context.h"

inline thread_local PPCContext* g_ppcContext;

inline PPCContext* GetPPCContext()
{
    return g_ppcContext;
}

inline void SetPPCContext(PPCContext& ctx)
{
    g_ppcContext = &ctx;
}
