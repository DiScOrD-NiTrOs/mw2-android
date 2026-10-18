// iw4x: a client can freeze the host with a bogus reliable acknowledge.
//
// The server tracks, per connected client, two counters in the client struct:
// reliableSequence (client+0x20E50), the last reliable command the server queued
// for that client, and reliableAcknowledge (client+0x20E54), the last one the
// client says it received. SV_ExecuteClientMessage stores reliableAcknowledge
// straight off the wire before validating it, and its one range check is signed:
// `reliableSequence - reliableAcknowledge < 128`. A client that sends
// reliableAcknowledge = INT_MAX makes that difference hugely negative, so it
// passes the check, and the bogus value is kept.
//
// Three per-client loops then run `for (i = reliableAcknowledge + 1; i <=
// reliableSequence; i++)`:
//   - SV_UpdateServerCommandsToClient (sub_822C8D68), the ordinary resend of
//     queued server commands, on the regular send path;
//   - SV_ResendReliableCommands (sub_822C8E78), its twin for when that message
//     overflows;
//   - SV_DumpPendingReliableCommands (sub_822C8F50), which Com_Printfs every
//     still-pending command.
// The command index is masked into a 128-entry ring, so none of them read out of
// bounds -- but with reliableAcknowledge = INT_MAX the loop counter starts at
// INT_MIN and runs ~2^31 times, and only the second has an early exit, so the
// server spins and freezes.
//
// The engine keeps the difference in [0, 128]: it drops a client only once it
// reaches 129 (sub_822C1608: `subf r8,ack,seq; cmpwi r8,129; bne <no drop>`), so
// a slow-but-legitimate client can legitimately sit at 128 pending commands. So
// before each of the three loops runs, reset reliableAcknowledge to
// reliableSequence whenever the difference is outside [0, 128] -- the same thing
// the engine's own guard does when it rejects a bad acknowledge, and a no-op for
// any client within the range the engine itself allows. The subtraction is done
// on unsigned values: an attacker's acknowledge is near INT_MIN/INT_MAX, where a
// signed subtraction would be undefined overflow.
//
// Multiplayer only: the campaign image has no twin of these functions.
#include <ppc_recomp_shared.h>
#include "title.h"
#include "log.h"

#include <cstdint>

#if defined(T_SV_UpdateServerCommandsToClient) && defined(T_SV_ResendReliableCommands) && \
    defined(T_SV_DumpPendingReliableCommands)

namespace
{
    constexpr uint32_t kReliableSequence = 0x20E50;      // client->reliableSequence
    constexpr uint32_t kReliableAcknowledge = 0x20E54;   // client->reliableAcknowledge

    void SanitizeReliableAcknowledge(uint8_t* base, uint32_t client)
    {
        const uint32_t seq = PPC_LOAD_U32(client + kReliableSequence);
        const uint32_t ack = PPC_LOAD_U32(client + kReliableAcknowledge);
        // Unsigned difference: a value in [0, 128] is left alone; anything larger --
        // including a negative difference (ack ahead of seq), which wraps to a huge
        // unsigned value -- is reset.
        if (seq - ack <= 128u) return;
        PPC_STORE_U32(client + kReliableAcknowledge, seq);
        static bool logged;
        if (!logged)
            LOGW("online: a client acknowledged a command the server never sent; its acknowledge was reset");
        logged = true;
    }
}

// Each takes the client in r3.
GUEST_HOOK(T_SV_UpdateServerCommandsToClient)
{
    SanitizeReliableAcknowledge(base, ctx.r3.u32);
    GUEST_ORIG(T_SV_UpdateServerCommandsToClient)(ctx, base);
}

GUEST_HOOK(T_SV_ResendReliableCommands)
{
    SanitizeReliableAcknowledge(base, ctx.r3.u32);
    GUEST_ORIG(T_SV_ResendReliableCommands)(ctx, base);
}

GUEST_HOOK(T_SV_DumpPendingReliableCommands)
{
    SanitizeReliableAcknowledge(base, ctx.r3.u32);
    GUEST_ORIG(T_SV_DumpPendingReliableCommands)(ctx, base);
}

#endif
