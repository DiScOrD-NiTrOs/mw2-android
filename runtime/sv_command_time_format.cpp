// A client's name is used as a print format when its command time is refused.
//
// The server checks the time each client command carries (sub_822BC778). When
// it is more than 20 seconds ahead of the server's own, the function says so
// and drops the command:
//
//     va("Invalid command time %i from client %s, current server time is %i\n",
//        commandTime, client->name, serverTime)
//     Com_PrintMessage(15, <that text>)
//
// The finished text, the client's name in it, is handed to the printer as its
// format. The name is the client's own choice and so is the command time, so a
// client in the match can have whatever '%' directives it puts in its name
// interpreted on the host, with no arguments behind them.
//
// The message is printed here instead, the same text through "%s", and the
// command is dropped as before. Any other command time goes to the original
// function untouched. The text is still built by the title's own va, from the
// title's own format.
//
// Multiplayer only: the campaign has no such check.
#include <ppc_recomp_shared.h>
#include "title.h"

#include <cstdint>

#if defined(T_SV_CheckCommandTime) && defined(T_va) && defined(T_Com_PrintMessage)

namespace
{
    constexpr uint32_t kClientName = 0x212B8;   // client->name
    constexpr int32_t kFarAhead = 20000;        // milliseconds, the function's own limit
}

// sub_822BC778(client = r3, command = r4); the command's time is its first word.
GUEST_HOOK(T_SV_CheckCommandTime)
{
    const uint32_t client = ctx.r3.u32;
    const uint32_t commandTime = PPC_LOAD_U32(ctx.r4.u32);
    const uint32_t serverTime = PPC_LOAD_U32(T_DATA_ServerTime);
    // The function's own test: a signed difference of the two 32-bit times.
    if (int32_t(commandTime - serverTime) <= kFarAhead)
    {
        GUEST_ORIG(T_SV_CheckCommandTime)(ctx, base);
        return;
    }

    // A frame of our own for the two calls, as the function has one: both are
    // variadic and spill their register arguments into their caller's frame.
    const PPCContext saved = ctx;
    ctx.r1.u64 = saved.r1.u32 - 0x80;

    ctx.r3.u64 = T_STR_InvalidCommandTime;
    ctx.r4.u64 = commandTime;
    ctx.r5.u64 = client + kClientName;
    ctx.r6.u64 = serverTime;
    GUEST_FUNC(T_va)(ctx, base);

    ctx.r5.u64 = ctx.r3.u32;        // the text, now an argument
    ctx.r4.u64 = T_STR_PercentS;    // "%s"
    ctx.r3.u64 = 15;
    GUEST_FUNC(T_Com_PrintMessage)(ctx, base);

    ctx = saved;
}

#endif
