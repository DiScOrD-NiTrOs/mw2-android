// CVE-2019-20893 -- PartyHost_HandleJoinPartyRequest.
//
// The "joinParty" out-of-band packet carries, as its sixth token (argv[5]), a
// count of subparties. The handler (sub_821A6950) checks only that the count is
// >= 1, then loops that many times, each iteration storing one attacker-supplied
// int into a fixed array on its own 272-byte stack frame (base r1+80). A count
// past the array -- anything over the engine's 18-client ceiling -- runs the
// loop off the frame, across the saved registers and the return address: a
// remote, pre-authentication stack smash on whoever is hosting a party or lobby.
// Reportedly found on the Xbox 360 first; iw4x clamps the count to 18.
//
// The packet reaches this handler with no sender authentication (the party
// dispatcher does not check who sent it), so on the lan backend anyone on the
// network can send it, and on steam any member of the host's lobby.
//
// No legitimate joinParty ever names more than 18 subparties -- there are at
// most 18 clients -- so the hook reads the same count the handler is about to
// parse, argv[5] of the local client's command args, and drops the packet when
// it exceeds 18 before the original runs. A count in [1, 18] passes through
// untouched; a count below 1 is left to the handler's own rejection. Dropping an
// out-of-range request is how the engine itself treats a bad count here (it
// already rejects "< 1 subparties"), and for every packet a real client sends it
// is indistinguishable from iw4x's clamp.
//
// The count is parsed by the title's own atoi, the call the handler makes, so
// the hook and the handler cannot disagree on a token: that atoi saturates at
// 0x7FFFFFFF where a 64-bit host's strtol, cut to 32 bits, turns 4294967297
// into 1.
#include <ppc_recomp_shared.h>
#include "title.h"
#include "log.h"

#include <cstdint>

#ifdef T_PartyHost_HandleJoinPartyRequest

namespace
{
    constexpr int32_t kMaxClients = 18;   // the engine's sv_maxclients ceiling

    // argv[5] of the current local client's tokenized command, parsed the way
    // the handler does: args[+0] is the local client index; args[+68 + i*4] is
    // client i's argc; args[+100 + i*4] is the pointer to client i's argv array.
    int32_t JoinPartySubpartyCount(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t args = T_DATA_CmdArgs;
        const uint32_t client = PPC_LOAD_U32(args);
        const uint32_t argc = PPC_LOAD_U32(args + 68 + client * 4);
        if (argc <= 5)
            return 0;                    // no argv[5]; the handler reads "" -> 0
        const uint32_t argv = PPC_LOAD_U32(args + 100 + client * 4);

        // The hook runs at the handler's entry, so the stack below r1 is free
        // for the call, as it is for the handler's own.
        const PPCContext saved = ctx;
        ctx.r3.u64 = PPC_LOAD_U32(argv + 5 * 4);
        GUEST_FUNC(T_atoi)(ctx, base);
        const int32_t count = ctx.r3.s32;
        ctx = saved;
        return count;
    }
}

GUEST_HOOK(T_PartyHost_HandleJoinPartyRequest)
{
    const int32_t count = JoinPartySubpartyCount(ctx, base);
    if (count > kMaxClients)
    {
        // once: whoever sends one can send thousands
        static bool logged;
        if (!logged)
            LOGW("online: dropped a joinParty naming %d subparties (max %d) -- CVE-2019-20893", count, kMaxClients);
        logged = true;
        return;
    }
    GUEST_ORIG(T_PartyHost_HandleJoinPartyRequest)(ctx, base);
}

#endif
