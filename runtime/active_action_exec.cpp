// iw4x: a host can run console commands on a joining client through the
// activeAction dvar -- never run it.
//
// CL_RunActiveAction (sub_82171790) is run once from the client's connection
// state machine (sub_821718B0) as the client goes active. After its other
// set-up work it reads the activeAction dvar (pointer at 0x825089B8, current
// string at dvar+12) and, if the string is not empty, passes it to Cbuf_AddText
// with a fixed follow-up line, then sets the dvar back to "" with
// Dvar_SetString. The host sets that value: the client's set-dvar server
// command (sub_82154738, from CG_ServerCommand) hands every name but three to
// Dvar_SetFromStringByName with no name filter -- the client side of the
// script's setClientDvar. So the host of a game a player joins (a mod, or a
// crafted server command) can have any console command run on that player's
// copy.
//
// The fix is at the reader, not the setter: activeAction is registered with
// flags 0, so no flag test on the set-dvar path could tell it apart from the
// dvars a host legitimately sets. iw4x does the same -- it turns the reader's
// "not empty" branch into an unconditional skip (Set<BYTE>(0x5A1D43, 0xEB)),
// so the value is never executed. Whether the set-dvar command should refuse
// other names -- or honour write-protect flags for this source -- is a
// separate question, not answered here.
//
// The recompiled function cannot be branch-patched, so the hook clears the dvar
// before the original runs: the original then finds it empty and skips the
// execute-and-clear block, and everything else it does is unchanged. The clear
// goes through the title's own Dvar_SetString with the "" literal the original
// itself clears with, rather than writing a 0 into the string: dvar strings are
// interned script strings (CopyStringInternal -> SL_GetString_), shared by
// every user of the same text, so truncating one in place would blank the
// others too. No call the original makes ahead of the read reaches the
// set-dvar handler or CG_ServerCommand (direct calls, by the call graph), so the
// host cannot set the value again between the clear and the read.
// The one difference from the original: when the function takes its early exit
// (a client-state bit it tests first), the dvar is cleared now rather than left
// for a later call -- which only discards a value that would never be run.
// MP-only: the campaign has no twin.
#include <ppc_recomp_shared.h>
#include "title.h"
#include "log.h"

#include <cstdint>

#ifdef T_CL_RunActiveAction

GUEST_HOOK(T_CL_RunActiveAction)
{
    // Guard both pointers rather than lean on a null read returning 0 here.
    const uint32_t dvar = PPC_LOAD_U32(T_DATA_ActiveActionDvar);
    const uint32_t str = dvar ? PPC_LOAD_U32(dvar + 12) : 0;
    if (str && PPC_LOAD_U8(str)) {
        // Log once -- activeAction registers empty, so a value here is
        // normally the host's (an attack or a mod).
        static bool logged;
        if (!logged)
            LOGW("online: not running activeAction set by the host (activeAction is disabled)");
        logged = true;

        const PPCContext saved = ctx;
        ctx.r3.u64 = dvar;
        ctx.r4.u64 = T_DATA_EmptyString;
        GUEST_FUNC(T_Dvar_SetString)(ctx, base);
        ctx = saved;
    }
    GUEST_ORIG(T_CL_RunActiveAction)(ctx, base);
}

#endif
