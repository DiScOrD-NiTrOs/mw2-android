// A host's snapshot chooses which hud element fields change, by index into the
// 42-entry hud field table; refuse an index the table does not have.
//
// MSG_ReadDeltaPlayerState (sub_8228D0D8) reads the player state's two hud
// element arrays (31 elements each, 168 bytes apart) with
// MSG_ReadDeltaHudElems (sub_8228CF10). Per element it reads lastChanged in 6
// bits (0..63), and then, until the index reaches lastChanged-1, a gap (a bit
// for 1, else 3-bit groups where 7 means "7 more") that it adds to the previous
// index. For each index it takes the descriptor table + index*16, copies the
// fields it skipped and decodes the field with MSG_ReadDeltaField
// (sub_8228BFC0), which stores the value at to + descriptor->offset. Nothing
// compares lastChanged or an index with the table's 42 entries -- the loop
// after them, which copies the unchanged tail, does. An index of 42..62 takes
// its descriptor from the .rdata past the table, whose offset word (31322 for
// 42) puts a host-chosen 32-bit value ~26 KB past the player state; a gap can
// also run the index past lastChanged-1 to any value the message pays for. And
// once the message has been read past its end MSG_ReadBits returns -1, the gap
// comes out 0, and the first field's index is -1: the word before the table
// becomes the offset.
//
// The rule is the host's encoder (sub_82291AE8): lastChanged is one past the
// last of the 42 fields that differ, and it writes every changed field below
// it, so every index it sends is past the previous one and at most
// lastChanged-1, which is at most 41. A message that breaks that cannot have
// come from a host. It is refused the way the title refuses a malformed message:
// the message is marked overflowed, so CL_ParseSnapshot drops the snapshot,
// and nothing more is decoded into the elements.
//
// The function is reimplemented whole, because the check sits inside its
// loop: the same reads, in the same order, with the title's own MSG_ReadBit,
// MSG_ReadBits and MSG_ReadDeltaField, and the same copies; only the check is
// new.
//
// MP only: no twin in default.xex.
#include <ppc_recomp_shared.h>
#include "title.h"
#include "log.h"

#include <atomic>
#include <cstdint>
#include <cstring>

#if defined(T_MSG_ReadDeltaHudElems) && defined(T_MSG_ReadDeltaField) && defined(T_MSG_ReadBit) \
    && defined(T_MSG_ReadBits)

namespace {

constexpr uint32_t kElementSize = 168;
constexpr uint32_t kFieldSize = 16;     // {name, offset, bits, flags}
constexpr uint32_t kFieldOffset = 4;
constexpr uint32_t kOverflowed = 0;     // msg_t

// The title's routines; each returns its value in r3.
int32_t ReadBits(PPCContext& ctx, uint8_t* base, uint32_t msg, uint32_t bits)
{
    ctx.r3.u64 = msg;
    ctx.r4.u64 = bits;
    GUEST_FUNC(T_MSG_ReadBits)(ctx, base);
    return ctx.r3.s32;
}

int32_t ReadBit(PPCContext& ctx, uint8_t* base, uint32_t msg)
{
    ctx.r3.u64 = msg;
    GUEST_FUNC(T_MSG_ReadBit)(ctx, base);
    return ctx.r3.s32;
}

uint32_t FieldOffset(uint8_t* base, int32_t field)
{
    return PPC_LOAD_U32(T_DATA_HudElemFields + uint32_t(field) * kFieldSize + kFieldOffset);
}

// A field the message leaves unchanged keeps the old element's value.
void CopyField(uint8_t* base, uint32_t from, uint32_t to, int32_t field)
{
    const uint32_t offset = FieldOffset(base, field);
    PPC_STORE_U32(to + offset, PPC_LOAD_U32(from + offset));
}

void Refuse(uint8_t* base, uint32_t msg, int32_t lastChanged, int32_t field)
{
    PPC_STORE_U32(msg + kOverflowed, 1);
    // Every power of two, so a flood stays a few lines.
    static std::atomic<uint32_t> count;
    const uint32_t n = ++count;
    if (n & (n - 1))
        return;
    LOGW("online: dropped a snapshot whose hud element field %d (of %d changed, %u fields) no host sends (%u so far)",
         field, lastChanged, PPC_LOAD_U32(T_DATA_HudElemFieldCount), n);
}

} // namespace

// MSG_ReadDeltaHudElems(msg = r3, time = r4, from = r5, to = r6, count = r7)
GUEST_HOOK(T_MSG_ReadDeltaHudElems)
{
    const uint32_t msg = ctx.r3.u32;
    const uint32_t time = ctx.r4.u32;
    const uint32_t from = ctx.r5.u32;
    const uint32_t to = ctx.r6.u32;
    const int32_t capacity = ctx.r7.s32;
    const int32_t numFields = int32_t(PPC_LOAD_U32(T_DATA_HudElemFieldCount));

    const int32_t elements = ReadBits(ctx, base, msg, 5);
    for (int32_t e = 0; e < elements; ++e) {
        const uint32_t fromElem = from + uint32_t(e) * kElementSize;
        const uint32_t toElem = to + uint32_t(e) * kElementSize;

        const int32_t lastChanged = ReadBits(ctx, base, msg, 6);
        int32_t field = -1;
        // The original's tests: unsigned against 0, then lastChanged-1 > -1.
        if (lastChanged != 0 && lastChanged - 1 > -1) {
            if (lastChanged > numFields) {
                Refuse(base, msg, lastChanged, lastChanged - 1);
                return;
            }
            const int32_t last = lastChanged - 1;
            do {
                int32_t gap;
                if (ReadBit(ctx, base, msg) != 0) {
                    gap = 1;
                } else {
                    int32_t sevens = 1;
                    int32_t bits = ReadBits(ctx, base, msg, 3);
                    while (bits == 7) {
                        sevens += 7;
                        bits = ReadBits(ctx, base, msg, 3);
                    }
                    gap = bits + sevens;
                }
                const int32_t previous = field;
                field = previous + gap;
                if (field <= previous || field > last) {
                    Refuse(base, msg, lastChanged, field);
                    return;
                }
                for (int32_t skipped = previous + 1; skipped < field; ++skipped)
                    CopyField(base, fromElem, toElem, skipped);

                ctx.r3.u64 = msg;
                ctx.r4.u64 = time;
                ctx.r5.u64 = fromElem;
                ctx.r6.u64 = toElem;
                ctx.r7.u64 = T_DATA_HudElemFields + uint32_t(field) * kFieldSize;
                ctx.r8.u64 = 0;
                ctx.r9.u64 = 0;
                GUEST_FUNC(T_MSG_ReadDeltaField)(ctx, base);
            } while (field < last);
        }
        for (int32_t unchanged = field + 1; unchanged < numFields; ++unchanged)
            CopyField(base, fromElem, toElem, unchanged);
    }

    // The elements past the message's clear until the first already clear one
    // (type, the first word, 0). As in the original, an overflowed count (-1)
    // starts one element early.
    for (int32_t e = elements; e < capacity; ++e) {
        const uint32_t elem = to + uint32_t(e) * kElementSize;
        if (PPC_LOAD_U32(elem) == 0)
            break;
        std::memset(base + elem, 0, kElementSize);
    }
}

#endif
