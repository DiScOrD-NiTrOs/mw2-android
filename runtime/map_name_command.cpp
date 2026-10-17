// A map name a remote peer chose is pasted unquoted into a "map" command line:
// cut it where the command buffer would end the command, so nothing after it
// runs as a second command.
//
// Three title functions queue a line built from a map-name dvar's current
// string (dvar+12) with Cbuf_AddText:
//   multiplayer  the party frame (sub_8219FBB8), on the party host once a start
//                is pending:          va("map %s 1 %i\n", ui_mapname, 0)
//   multiplayer  the menu script runner (sub_822E2090), verb "StartServer"
//                (systemlink_startmatch, uiscript_startserver):
//                                     va("map %s\n", ui_mapname)
//   campaign     the co-op map load (sub_82338CB0), called at the end of
//                coopLaunch:          va("map %s\n", coop_mapName)
// The dvars are written from the network without any character filter:
// ui_mapname by a party host's partystate (PartyClient_HandlePartyState ->
// sub_82198D80, up to 1024 bytes; the reader only maps byte 0x92 to ') and by
// the set-dvar server command; coop_mapName by the co-op host's coopLaunch
// (sub_82339048, 256 bytes, MSG_ReadString copying every byte). The party
// start (sub_821A1FB8) picks no map; it launches whatever ui_mapname holds.
// So a co-op host can run any console command on its partner as the partner
// loads the map, and a party host's text would run on a member that later
// starts the match as host.
//
// The rule is Cbuf_Execute's own (its line splitter sub_82277C38): a command
// ends at '\n' or '\r', and at ';' when an even number of '"' come before it.
// The text ahead of the name ("map ") holds no quote, so applying the rule to
// the name alone finds the same place. A map name is a fastfile name and holds
// none of these, so a stock name is never changed. The cut keeps the first
// command the original would have run, plus the line's own trailing arguments.
//
// The hooks run ahead of each original and, only when the name holds such an
// end, set the dvar to the text before it with the title's Dvar_SetString (dvar
// strings are interned and shared, so they are never written in place). The
// new text goes just below the guest stack pointer, with the call made at a
// stack pointer pushed past it, as console.cpp does; Dvar_SetString copies it.
// No call ahead of the read in any of the three reaches a network handler that
// could write the dvar again.
#include <ppc_recomp_shared.h>
#include "title.h"
#include "log.h"

#include <cstdint>
#include <cstring>

#ifdef T_DATA_MapNameDvar

namespace {

// Where Cbuf_Execute would end the command that starts with the name, or -1.
int CommandEnd(const char* s)
{
    unsigned quotes = 0;
    for (int i = 0; s[i]; ++i) {
        const char c = s[i];
        if (c == '"')
            ++quotes;
        if ((c == ';' && !(quotes & 1)) || c == '\n' || c == '\r')
            return i;
    }
    return -1;
}

void CutMapName(PPCContext& ctx, uint8_t* base)
{
    const uint32_t dvar = PPC_LOAD_U32(T_DATA_MapNameDvar);
    const uint32_t str = dvar ? PPC_LOAD_U32(dvar + 12) : 0;
    if (!str)
        return;
    const char* name = reinterpret_cast<const char*>(base + str);
    const int end = CommandEnd(name);
    if (end < 0)
        return;

    const uint32_t sp = ctx.r1.u32;
    if (sp < 0x2000)
        return;
    static bool logged;
    if (!logged)
        LOGW("online: cut a map name received from the network at a command separator");
    logged = true;

    const uint32_t scratch = (sp - 0x100 - uint32_t(end) - 1) & ~0xFu;
    std::memmove(base + scratch, name, size_t(end));
    base[scratch + end] = 0;

    const PPCContext saved = ctx;
    ctx.r1.u64 = (scratch - 0x100) & ~0xFull;
    ctx.r3.u64 = dvar;
    ctx.r4.u64 = scratch;
    GUEST_FUNC(T_Dvar_SetString)(ctx, base);
    ctx = saved;
}

} // namespace

#ifdef T_Party_Frame
GUEST_HOOK(T_Party_Frame)
{
    CutMapName(ctx, base);
    GUEST_ORIG(T_Party_Frame)(ctx, base);
}
#endif

#ifdef T_UI_RunMenuScript
GUEST_HOOK(T_UI_RunMenuScript)
{
    CutMapName(ctx, base);
    GUEST_ORIG(T_UI_RunMenuScript)(ctx, base);
}
#endif

#ifdef T_Coop_LoadMap
GUEST_HOOK(T_Coop_LoadMap)
{
    CutMapName(ctx, base);
    GUEST_ORIG(T_Coop_LoadMap)(ctx, base);
}
#endif

#endif
