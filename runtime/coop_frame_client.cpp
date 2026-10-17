// A co-op frame's commands that act for a client must come after one that
// says which client.
//
// The host relays every co-op frame: the inputs of both players, which each
// machine then runs in the same order (sub_8233A538, on the host and the
// client alike). A frame is a run of commands, each a 3-bit type:
//
//   3  select a client: a 1-bit slot, which must be 0 or 1 and in use
//   1  the selected client's usercmds (sub_8233A190)
//   2  the selected client's reliable command (sub_82339FA8, through
//      sub_8233A090, which reads the type)
//   4  end
//
// The selected client starts out null (sub_8233A538, 8233A6A8) and only a 3
// sets it, so a 1 or a 2 that comes first runs with a null client: the
// reliable command reads and writes the client's fields at guest 0x1218 and
// up, a 1024-byte Com_sprintf among them, and both hand a client number
// computed from the null pointer (361817) to code that indexes the entities
// and clients with it, unbounded. On the console the first access faults; in
// this recomp guest page 0 is memory like any other, and the writes land.
//
// A host's frames always select first: it builds each player's part of the
// frame as a 3 with the sender's slot followed by that player's own commands
// (sub_8233AA50, 8233ABC8). So a 1 or a 2 with no client selected cannot have
// come from a host, and the frame is refused the way the title refuses a
// malformed one: the message is marked overflowed and the loop in
// sub_8233A538 stops at it, as it does at the end of a message.
//
// Campaign only: the multiplayer has no co-op frames.
#include <ppc_recomp_shared.h>
#include "title.h"
#include "log.h"

#include <atomic>
#include <cstdint>

#if defined(T_CoopFrame_ReliableCommand) && defined(T_CoopFrame_UserCmds)

namespace {

constexpr uint32_t kOverflowed = 0;   // msg_t

void Refuse(uint8_t* base, uint32_t msg, const char* what)
{
    PPC_STORE_U32(msg + kOverflowed, 1);
    // Every power of two, so a flood stays a few lines.
    static std::atomic<uint32_t> count;
    const uint32_t n = ++count;
    if (n & (n - 1))
        return;
    LOGW("online: dropped a co-op frame whose %s came before any client was selected (%u so far)", what, n);
}

} // namespace

// (client = r3, msg = r4) -> nonzero to go on reading the frame
GUEST_HOOK(T_CoopFrame_ReliableCommand)
{
    if (ctx.r3.u32 == 0) {
        Refuse(base, ctx.r4.u32, "reliable command");
        ctx.r3.u64 = 0;
        return;
    }
    GUEST_ORIG(T_CoopFrame_ReliableCommand)(ctx, base);
}

// (client = r3, msg = r4)
GUEST_HOOK(T_CoopFrame_UserCmds)
{
    if (ctx.r3.u32 == 0) {
        Refuse(base, ctx.r4.u32, "usercmds");
        return;
    }
    GUEST_ORIG(T_CoopFrame_UserCmds)(ctx, base);
}

#endif
