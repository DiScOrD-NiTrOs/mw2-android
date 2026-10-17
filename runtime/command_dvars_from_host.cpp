// A host can run console commands on a client through a dvar the client later
// executes -- never let the host set those dvars.
//
// The client's set-dvar server command (sub_82154738, the client side of the
// script's setClientDvar, called only from the server command switch
// sub_82155228) intercepts three names and hands every other to
// Dvar_SetFromStringByName with source 0. Dvar_SetVariant tests the dvar's
// flags only for sources 1 and 2, so no flag stops it. Two dvars the host can
// reach that way are run as console commands by the client:
//
//   com_errorResolveCommand (flags 0x2000, ROM) -- the command an error popup
//     runs when the player dismisses it. The title sets it itself
//     (xrequirelivesignin puts "xsigninlive;" there); the menu script command
//     resolveError (sub_822EA450, in the UI script table beside kickPlayer and
//     makehost) passes a non-empty value to Cbuf_AddText and then clears it.
//     The stock error_popmenu runs resolveError on its back action, so the
//     host sets the dvar, drops the player, and dismissing the popup runs it.
//     iw4x's DvarSetFromStringByName_Stub on CG_SetClientDvarFromServer
//     refuses the same name.
//   nextmap (flags 0) -- sub_82172B48, called from CL_Disconnect (sub_821839F0)
//     and the disconnected-from-server path (sub_82183FB0), passes a non-empty
//     value with a newline to Cbuf_AddText and then clears it, but only when
//     the client's connection state is 1 (a cinematic), which only the local
//     `cinematic` console command sets. So a value the host plants waits for
//     the player's own cinematic; refusing it is defensive. The title's own
//     writers are local (SV_SpawnServer sets "map_restart"), and a host has no
//     stock reason to set it on a client.
//
// Both readers have the shape of CL_RunActiveAction (active_action_exec.cpp),
// but these fixes are at the setter, not the reader, because a local value is
// legitimate (the title's own popup command; a nextmap set at the console).
// The hook refuses those names and passes every other through unchanged. It
// does not switch the path to an external source: Dvar_SetVariant would then
// refuse cheat dvars unless sv_cheats is on and defer latched ones, which a
// host's stock scripts may set. Names compare as I_stricmp does (sub_82315b70,
// the handler's own compare), ASCII case-folded, since Dvar_FindVar matches
// without case. MP-only: the campaign's handler (sub_82108090) accepts
// hud_drawHUD alone.
#include <ppc_recomp_shared.h>
#include "title.h"
#include "log.h"
#include "guest.h"

#include <cctype>
#include <cstdint>

#ifdef T_CG_SetClientDvarFromServer

namespace {

const char* const kRefused[] = { "com_errorResolveCommand", "nextmap" };

bool SameName(const char* a, const char* b)
{
    for (size_t i = 0;; ++i) {
        const unsigned char x = static_cast<unsigned char>(a[i]);
        const unsigned char y = static_cast<unsigned char>(b[i]);
        if (std::tolower(x) != std::tolower(y))
            return false;
        if (!x)
            return true;
    }
}

} // namespace

// sub_82154738(cg, name, value)
GUEST_HOOK(T_CG_SetClientDvarFromServer)
{
    const char* name = GuestPtr<const char>(ctx.r4.u32);
    if (name) {
        for (size_t i = 0; i < sizeof kRefused / sizeof kRefused[0]; ++i) {
            if (!SameName(name, kRefused[i]))
                continue;
            static bool logged[sizeof kRefused / sizeof kRefused[0]];
            if (!logged[i])
                LOGW("online: not letting the host set %s", kRefused[i]);
            logged[i] = true;
            return;
        }
    }
    GUEST_ORIG(T_CG_SetClientDvarFromServer)(ctx, base);
}

#endif
