// The reason a server disconnects a client with is used as a print format.
//
// The server drops a client with the server command 'w' and a reason, normally
// a localisation key (EXE_SERVERQUIT, EXE_PLAYERKICKED...). The client passes
// it to sub_82184918, which handles the keys it knows and treats any other
// reason, and any beginning with EXE_PLAYERKICKED, alike:
//
//     text = UI_ReplaceReason(Localize("EXE_SERVERDISCONNECTREASON"), Localize(reason))
//     Com_Error(2, text)
//
// The localiser hands an unknown key back inside its result, so the reason ends
// up in the text, and the text is Com_Error's format. A stock server cannot put
// a '%' in it: it stores every server command through sub_8228F9D0, which turns
// '%' into '.'. But the client reads server commands without such a filter
// (the connectionless reader has one), so a host running a modified game can
// have whatever '%' directives it puts in a reason interpreted on the client,
// with no arguments behind them. On a console '%n' stops the title (the
// C library refuses it and its invalid-parameter path ends in a trap) and '%s'
// reads through whatever pointer follows; here the trap is not emitted and every
// guest read succeeds, so the message comes out garbled.
//
// Those two paths are done here instead, with the same calls in the same order,
// and the text is raised through "%s". Every other reason goes to the original
// function untouched. The keys are compared with the title's own I_stricmp and
// I_strnicmp, in the original's order.
//
// Multiplayer only: the campaign's disconnect handler raises literal messages.
#include <ppc_recomp_shared.h>
#include "title.h"

#include <cstdint>

#if defined(T_CL_DisconnectReason)

namespace
{
    // A frame of our own for the calls: Com_Error is variadic and spills its
    // register arguments into its caller's frame.
    constexpr uint32_t kFrame = 0x80;

    uint32_t Call(PPCContext& ctx, uint8_t* base, void (*fn)(PPCContext&, uint8_t*),
                  uint32_t r3, uint32_t r4, uint32_t r5 = 0)
    {
        ctx.r3.u64 = r3;
        ctx.r4.u64 = r4;
        ctx.r5.u64 = r5;
        fn(ctx, base);
        return ctx.r3.u32;
    }
}

// sub_82184918(reason = r3)
GUEST_HOOK(T_CL_DisconnectReason)
{
    const PPCContext saved = ctx;
    const uint32_t reason = saved.r3.u32;
    ctx.r1.u64 = saved.r1.u32 - kFrame;

    auto same = [&](uint32_t key) {
        return Call(ctx, base, GUEST_FUNC(T_I_stricmp), key, reason) == 0;
    };
    auto original = [&] {
        ctx = saved;
        GUEST_ORIG(T_CL_DisconnectReason)(ctx, base);
    };

    // The keys the original handles before either generic path, in its order.
    if (same(T_STR_EXE_MATCHENDED) || same(T_STR_EXE_SERVERQUIT) ||
        same(T_STR_EXE_MIGRATIONABORTED) || same(T_STR_EXE_MIGRATION_IN_PROGRESS) ||
        same(T_STR_hostquit))
    {
        original();
        return;
    }
    const bool kicked = Call(ctx, base, GUEST_FUNC(T_I_strnicmp), T_STR_EXE_PLAYERKICKED, reason, 16) == 0;
    // Past the kicked prefix, two more keys the original handles itself; it
    // compares the second after the party call below, which it then makes too.
    if (!kicked && (same(T_STR_roundsdone) || same(T_STR_XBOXLIVE_SIGNINCHANGED)))
    {
        original();
        return;
    }

    const uint32_t party = T_DATA_PartyForDisconnect;
    if (kicked)
        Call(ctx, base, GUEST_FUNC(T_Party_StopParty), party, 0);
    else
        Call(ctx, base, GUEST_FUNC(T_Party_OnDisconnect), party, PPC_LOAD_U32(PPC_LOAD_U32(party) + 1440));

    const uint32_t localized = Call(ctx, base, GUEST_FUNC(T_SEH_LocalizeTextMessage), reason, 0);
    const uint32_t message = Call(ctx, base, GUEST_FUNC(T_SEH_LocalizeTextMessage), T_STR_EXE_SERVERDISCONNECTREASON, 0);
    const uint32_t text = Call(ctx, base, GUEST_FUNC(T_UI_ReplaceReason), message, localized);
    Call(ctx, base, GUEST_FUNC(T_Com_Error), 2, T_STR_PercentS, text);

    ctx = saved;
}

#endif
