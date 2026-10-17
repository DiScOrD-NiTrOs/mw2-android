// A baseline snapshot from the host holds at most 1024 entities; refuse one
// that holds more before it writes past the array it is decoded into.
//
// A snapshot whose snapFlags carry 0x8 is a baseline: CL_DeltaEntity
// (sub_8218C4A8) decodes each of its entities into a 1024-entry array of
// 256-byte entity states (0x82561504) instead of the parse-entities ring, at
// index count, the word just before the array, and counts up for every entity
// not removed. CL_ParseSnapshot zeroes the count when the flag is set; nothing
// compares it with 1024. The loop of CL_ParsePacketEntities (sub_8218C848),
// CL_DeltaEntity's only caller, ends at entity number 2047, on an overflowed
// message or past its end, and the entity index can jump by 9 bits or restart
// at an absolute 11-bit value, so the number of entities is bounded only by the
// message: an unchanged entity costs 3 bits, and 64 KB carries ~170 000 of them,
// writing host-chosen entity states over the title's .data past the array.
//
// The bound is the host's: the server builds a baseline snapshot from at most
// 1024 entities (NUM_NODELTA_ENTITIES, sub_822C6180, "Hit
// NUM_NODELTA_ENTITIES[%d] in baseline snapshot - ignoring the rest."). So a
// baseline snapshot with a 1025th entity cannot have come from a host, and it is
// refused the way the title refuses a malformed one: the message is marked
// overflowed, CL_ParsePacketEntities' loop stops at the overflow, and
// CL_ParseSnapshot drops a snapshot whose message overflowed (it is never made
// valid, stored as the baseline or copied into the frames).
//
// Two checks, one per side of the array:
//   - the write: CL_DeltaEntity refuses to decode into the baseline array once
//     its count is 1024;
//   - the read: a stored baseline frame is read back, as the old frame, from the
//     same array up to the frame's own entity count, which also counts the old
//     frame's unchanged entities copied into it, not only the decoded ones. So
//     after CL_ParsePacketEntities a baseline frame with more than 1024 entities
//     is refused too.
//
// MP only: the campaign's snapshot parser is other code (no twin of these
// functions in default.xex).
#include <ppc_recomp_shared.h>
#include "title.h"
#include "log.h"

#include <atomic>
#include <cstdint>

#if defined(T_CL_DeltaEntity) && defined(T_CL_ParsePacketEntities)

namespace {

constexpr uint32_t kMaxBaselineEntities = 1024;   // NUM_NODELTA_ENTITIES
constexpr uint32_t kBaselineFlag = 0x8;
// clSnapshot_t fields, as the two functions read them
constexpr uint32_t kSnapFlags = 12676;
constexpr uint32_t kNumEntities = 12700;
constexpr uint32_t kOverflowed = 0;   // msg_t

bool Baseline(uint8_t* base, uint32_t frame)
{
    return PPC_LOAD_U32(frame + kSnapFlags) & kBaselineFlag;
}

void Refuse(uint8_t* base, uint32_t msg, uint32_t entities)
{
    PPC_STORE_U32(msg + kOverflowed, 1);
    // Every power of two, so a flood stays a few lines.
    static std::atomic<uint32_t> count;
    const uint32_t n = ++count;
    if (n & (n - 1))
        return;
    LOGW("online: dropped a baseline snapshot with %u+ entities, more than the %u a host sends (%u so far)",
         entities, kMaxBaselineEntities, n);
}

} // namespace

// CL_DeltaEntity(cl = r3, msg = r4, time = r5, frame = r6, number = r7, old = r8)
GUEST_HOOK(T_CL_DeltaEntity)
{
    const uint32_t count = PPC_LOAD_U32(T_DATA_BaselineEntityCount);
    if (Baseline(base, ctx.r6.u32) && count >= kMaxBaselineEntities) {
        Refuse(base, ctx.r4.u32, count + 1);
        return;   // the caller ignores the result
    }
    GUEST_ORIG(T_CL_DeltaEntity)(ctx, base);
}

// CL_ParsePacketEntities(cl = r3, msg = r4, time = r5, old = r6, frame = r7)
GUEST_HOOK(T_CL_ParsePacketEntities)
{
    const uint32_t msg = ctx.r4.u32;
    const uint32_t frame = ctx.r7.u32;
    GUEST_ORIG(T_CL_ParsePacketEntities)(ctx, base);
    const uint32_t entities = PPC_LOAD_U32(frame + kNumEntities);
    if (Baseline(base, frame) && entities > kMaxBaselineEntities && !PPC_LOAD_U32(msg + kOverflowed))
        Refuse(base, msg, entities);
}

#endif
