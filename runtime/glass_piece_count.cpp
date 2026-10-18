// The glass state's piece count must fit the 1024-piece table it fills.
//
// The glass state (sub_820F1D50) fills a table of 1024 5-byte pieces
// (0x82437EB0, 5120 bytes; sub_820F1FD8 clears it) in one of two forms, chosen
// by its second bit:
//
//   1  pieces by index: an 11-bit index, refused above 1024 with the title's
//      own Com_Error (ERR_FATAL, "Bad glass piece index (%i) in glass state")
//      and ending the list at 1024
//   0  a run of pieces from 0: an 11-bit count, then that many pieces in order
//
// The run's count is never compared with 1024, so a count up to 2047 writes
// pieces past the table, over the fields after it (+5124..+5132) and into the
// dvar pointers that follow (0x824392C0, sub_820F1FF8).
//
// No packet carries this message: the client parses a glass state the same
// process has just written from its own server's state (sub_82340BD0 ->
// sub_82340668, whose glass part is sub_821F4210, which writes no more pieces
// than there are). So this is hardening, not a remote fix: a run longer than
// the table is refused the way the title refuses the other form, with the same
// error, for the index the run would reach.
//
// The three bits are read with the title's own MSG_ReadBit / MSG_ReadBits, and
// the message's read state (+0 overflow, +28 byte, +32 bit), the only fields
// they write, is put back before the parse runs.
//
// Campaign only: the multiplayer's glass state is other code.
#include <ppc_recomp_shared.h>
#include "title.h"
#include "log.h"

#include <cstdint>

#if defined(T_GlassState_Parse) && defined(T_MSG_ReadBit) && defined(T_MSG_ReadBits) \
    && defined(T_STR_BadGlassPieceIndex) && defined(T_Com_Error)

namespace {

constexpr uint32_t kPieces = 1024;
constexpr uint32_t kCountBits = 11;
constexpr uint32_t kOverflowed = 0;    // msg_t fields the bit readers write
constexpr uint32_t kReadCount = 28;
constexpr uint32_t kBit = 32;

// The run's count, or 0 when the message holds the other form, nothing, or ran
// out before the count was read whole: the run's loop then writes no piece
// (820F1E34 stops on the overflow flag).
uint32_t RunLength(PPCContext& ctx, uint8_t* base, uint32_t msg)
{
    const PPCContext saved = ctx;
    const uint32_t overflowed = PPC_LOAD_U32(msg + kOverflowed);
    const uint32_t readCount = PPC_LOAD_U32(msg + kReadCount);
    const uint32_t bit = PPC_LOAD_U32(msg + kBit);
    ctx.r1.u64 = saved.r1.u32 - 0x80;

    uint32_t count = 0;
    ctx.r3.u64 = msg;
    GUEST_FUNC(T_MSG_ReadBit)(ctx, base);
    if (ctx.r3.u32 != 0) {
        ctx.r3.u64 = msg;
        GUEST_FUNC(T_MSG_ReadBit)(ctx, base);
        if (ctx.r3.u32 == 0) {
            ctx.r3.u64 = msg;
            ctx.r4.u64 = kCountBits;
            GUEST_FUNC(T_MSG_ReadBits)(ctx, base);
            count = ctx.r3.u32;
        }
    }
    if (PPC_LOAD_U32(msg + kOverflowed))
        count = 0;

    PPC_STORE_U32(msg + kOverflowed, overflowed);
    PPC_STORE_U32(msg + kReadCount, readCount);
    PPC_STORE_U32(msg + kBit, bit);
    ctx = saved;
    return count;
}

} // namespace

// (msg = r3)
GUEST_HOOK(T_GlassState_Parse)
{
    const uint32_t count = RunLength(ctx, base, ctx.r3.u32);
    if (count > kPieces) {
        LOGE("glass state: a run of %u pieces, more than the table's %u", count, kPieces);
        ctx.r1.u64 = ctx.r1.u32 - 0x80;
        ctx.r3.u64 = 0;                         // ERR_FATAL, as for a bad index
        ctx.r4.u64 = T_STR_BadGlassPieceIndex;
        ctx.r5.u64 = count - 1;                 // the last index the run would write
        GUEST_FUNC(T_Com_Error)(ctx, base);     // does not return
        return;
    }
    GUEST_ORIG(T_GlassState_Parse)(ctx, base);
}

#endif
