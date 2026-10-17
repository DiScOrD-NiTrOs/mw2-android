// A host's snapshot chooses which fields of an entity, of the player state and
// of an objective change, by index into each one's field table; refuse an index
// the table does not have.
//
// Three readers decode a snapshot's deltas the same way: MSG_ReadDeltaEntity
// (sub_8228CA90), MSG_ReadDeltaPlayerState (sub_8228D0D8) and
// MSG_ReadDeltaFields (sub_8228C7D8; under MSG_ReadDeltaStruct, and the
// player state's objectives). Each reads the last changed field, then gaps
// (sub_8228C718: a unary run, or groups of the width it is given where the
// all-ones group means "that many more") that it adds to the previous index.
// For each index it takes the descriptor {name, offset, bits, flags} at
// table + index*16, copies the fields it skipped from the old state and decodes
// the field with MSG_ReadDeltaField (sub_8228BFC0), which stores the value at
// to + descriptor->offset. The last changed field is compared with the table's
// length, the indexes are not:
//   - MSG_ReadDeltaFields stops only when an index equals the last changed
//     field, so an index past it decodes and copies on until the sum wraps;
//   - MSG_ReadDeltaPlayerState stops at the first index not below it, so it
//     decodes that one, whatever it is;
//   - MSG_ReadDeltaEntity stops at an index past it, but decodes the first
//     index unchecked (the one after the eType too, when the eType changed).
// And every reader accepts a last changed field one past its table's last.
// The tables lie back to back, so an index past one takes the next table's
// descriptor, or the .rdata after, and the skipped-field copy and the decode
// write a host-chosen value at a host-chosen offset from the state. Once the
// message has been read past its end the gap comes out 0: an index repeats.
//
// The rule is the host's encoders (sub_82290E60, sub_82291CC0, sub_82290BC0):
// the last changed field is the last of the table's fields that differ, every
// changed field is written in order and none past the last. So every index is
// past the previous one, at most the last changed field and a field of the
// table. A message that breaks that cannot have come from a host. It is refused
// the way the title refuses a malformed message, before anything is copied or
// decoded by the index: MSG_Discard marks the message overflowed, so
// CL_ParseSnapshot drops the snapshot, and the reader stops. For the same
// reason an entity whose eType is negative is refused: the eType is a byte on
// the wire, and MSG_ReadByte gives -1 only once the message is read past its
// end; the type's table lookup clamps from above only.
//
// The functions are reimplemented whole, because the check sits inside their
// loops: the same reads, in the same order, with the title's own routines, the
// same copies and the same cl_shownet prints; only the checks are new. Two
// liberties: the copies of a whole state are host memmoves, and the origin the
// player state falls back on is copied as words, not through the FPU (the same
// but for a signalling NaN). The player state's slot loop also scans the slots
// for the one it decoded, unless a dvar is set, and uses nothing it finds; the
// scan only reads, and is left out. MSG_ReadDeltaPlayerState calls the other
// two readers it uses, MSG_ReadDeltaFields and MSG_ReadDeltaHudElems
// (hud_elem_field_index.cpp), through their hooks.
//
// MP only: no twin in default.xex.
#include <ppc_recomp_shared.h>
#include "title.h"
#include "log.h"

#include <atomic>
#include <bit>
#include <cstdint>
#include <cstring>

#if defined(T_MSG_ReadDeltaEntity) && defined(T_MSG_ReadDeltaPlayerState) && defined(T_MSG_ReadDeltaFields) \
    && defined(T_MSG_ReadFieldGap) && defined(T_MSG_EntityTypeFields) && defined(T_MSG_ReadDeltaField) \
    && defined(T_MSG_ReadDeltaHudElems) && defined(T_MSG_ReadBit) && defined(T_MSG_ReadBits) \
    && defined(T_MSG_Discard) && defined(T_MSG_ReadByte) && defined(T_MSG_ReadShort) \
    && defined(T_CL_FindSentOrigin) && defined(T_BG_GetEntityTypeName) && defined(T_Com_Printf) \
    && defined(T_Com_PrintMessage)

namespace {

constexpr uint32_t kFieldSize = 16;   // {name, offset, bits, flags}
constexpr uint32_t kFieldOffset = 4;
constexpr uint32_t kFieldBits = 8;
constexpr uint32_t kFieldFlags = 12;  // a byte
constexpr uint32_t kReadCount = 28;   // msg_t, printed by cl_shownet
constexpr uint32_t kDvarValue = 12;
constexpr uint32_t kEntityStateSize = 256;
constexpr uint32_t kPlayerStateSize = 12672;
constexpr uint32_t kChannel = 25;     // the prints' console channel

// Frames of our own for the calls, the originals' sizes: the prints are
// variadic and spill their register arguments into their caller's frame, and
// the player state's frame holds the empty old state.
constexpr uint32_t kFieldsFrame = 208;
constexpr uint32_t kEntityFrame = 192;
constexpr uint32_t kPlayerStateFrame = 12896;
constexpr uint32_t kEmptyPlayerState = 96;   // in that frame

using GuestFn = void (*)(PPCContext&, uint8_t*);

uint32_t Call(PPCContext& ctx, uint8_t* base, GuestFn fn, uint32_t r3, uint32_t r4 = 0, uint32_t r5 = 0,
              uint32_t r6 = 0, uint32_t r7 = 0, uint32_t r8 = 0, uint32_t r9 = 0, uint32_t r10 = 0)
{
    ctx.r3.u64 = r3;
    ctx.r4.u64 = r4;
    ctx.r5.u64 = r5;
    ctx.r6.u64 = r6;
    ctx.r7.u64 = r7;
    ctx.r8.u64 = r8;
    ctx.r9.u64 = r9;
    ctx.r10.u64 = r10;
    fn(ctx, base);
    return ctx.r3.u32;
}

int32_t ReadBit(PPCContext& ctx, uint8_t* base, uint32_t msg)
{
    return int32_t(Call(ctx, base, GUEST_FUNC(T_MSG_ReadBit), msg));
}

int32_t ReadBits(PPCContext& ctx, uint8_t* base, uint32_t msg, uint32_t bits)
{
    return int32_t(Call(ctx, base, GUEST_FUNC(T_MSG_ReadBits), msg, bits));
}

int32_t ReadGap(PPCContext& ctx, uint8_t* base, uint32_t msg, uint32_t width)
{
    return int32_t(Call(ctx, base, GUEST_FUNC(T_MSG_ReadFieldGap), msg, width));
}

void ReadDeltaField(PPCContext& ctx, uint8_t* base, uint32_t msg, uint32_t time, uint32_t from, uint32_t to,
                    uint32_t field, uint32_t print, bool fromZero)
{
    Call(ctx, base, GUEST_FUNC(T_MSG_ReadDeltaField), msg, time, from, to, field, print, fromZero);
}

// The bits that hold 0..n: what 32 - cntlzw(n) gives.
uint32_t BitsFor(uint32_t n)
{
    return 32 - uint32_t(std::countl_zero(n));
}

// A bit read as the readers turn it into a flag (neg/andc/rlwinm): set for 1,
// clear for 0 and for the -1 of a message read past its end.
bool Set(int32_t bit)
{
    return bit > 0;
}

uint32_t Field(uint32_t table, int32_t index)
{
    return table + uint32_t(index) * kFieldSize;
}

uint32_t FieldOffset(uint8_t* base, uint32_t field)
{
    return PPC_LOAD_U32(field + kFieldOffset);
}

uint8_t FieldFlags(uint8_t* base, uint32_t field)
{
    return PPC_LOAD_U8(field + kFieldFlags);
}

// A field the message leaves unchanged keeps the old state's value.
void CopyField(uint8_t* base, uint32_t from, uint32_t to, uint32_t field)
{
    const uint32_t offset = FieldOffset(base, field);
    PPC_STORE_U32(to + offset, PPC_LOAD_U32(from + offset));
}

void CopyFields(uint8_t* base, uint32_t from, uint32_t to, uint32_t table, int32_t first, int32_t end)
{
    for (int32_t i = first; i < end; ++i)
        CopyField(base, from, to, Field(table, i));
}

// cl_shownet's value, or nothing when the dvar is not registered.
bool Shownet(uint8_t* base, int32_t& value)
{
    const uint32_t dvar = PPC_LOAD_U32(T_DATA_ShownetDvar);
    if (dvar == 0)
        return false;
    value = int32_t(PPC_LOAD_U32(dvar + kDvarValue));
    return true;
}

void Refuse(PPCContext& ctx, uint8_t* base, uint32_t msg, const char* what, int32_t field, int32_t last,
            int32_t fields)
{
    Call(ctx, base, GUEST_FUNC(T_MSG_Discard), msg);
    // Every power of two, so a flood stays a few lines.
    static std::atomic<uint32_t> count;
    const uint32_t n = ++count;
    if (n & (n - 1))
        return;
    LOGW("online: dropped a snapshot whose %s field %d (last changed %d, %d fields) no host sends (%u so far)",
         what, field, last, fields, n);
}

// The rule the host's encoders keep: an index past the previous one, at most
// the last changed field, and a field of the table.
bool Sent(int32_t index, int32_t previous, int32_t last, int32_t fields)
{
    return index > previous && index <= last && index < fields;
}

} // namespace

// MSG_ReadDeltaFields(msg = r3, time = r4, from = r5, to = r6, count = r7,
//                     fields = r8, gap width = r9, compare = r10, a byte)
GUEST_HOOK(T_MSG_ReadDeltaFields)
{
    const PPCContext saved = ctx;
    const uint32_t msg = saved.r3.u32;
    const uint32_t time = saved.r4.u32;
    const uint32_t from = saved.r5.u32;
    const uint32_t to = saved.r6.u32;
    const int32_t count = saved.r7.s32;
    const uint32_t table = saved.r8.u32;
    const uint32_t width = saved.r9.u32;
    const bool compare = (saved.r10.u32 & 0xFF) != 0;
    ctx.r1.u64 = saved.r1.u32 - kFieldsFrame;

    // With compare, the first field tells whether the old state is the same
    // one: if it differs, the fields decode against zero.
    const uint32_t firstOffset = compare ? FieldOffset(base, table) : 0;

    if (ReadBit(ctx, base, msg) == 0) {
        CopyFields(base, from, to, table, 0, count);
        ctx = saved;
        return;
    }

    const int32_t last = ReadBits(ctx, base, msg, BitsFor(uint32_t(count)));
    if (last > count) {
        Call(ctx, base, GUEST_FUNC(T_MSG_Discard), msg);
        Call(ctx, base, GUEST_FUNC(T_Com_PrintMessage), kChannel, T_STR_DeltaFieldsLastChanged, uint32_t(last),
             uint32_t(count));
        ctx = saved;
        return;
    }

    bool same = true;
    if (last != -1) {
        int32_t index = -1;
        do {
            const int32_t previous = index;
            index = previous + ReadGap(ctx, base, msg, width);
            if (!Sent(index, previous, last, count)) {
                Refuse(ctx, base, msg, "struct", index, last, count);
                ctx = saved;
                return;
            }
            CopyFields(base, from, to, table, previous + 1, index);
            if (compare && index > 0)
                same = PPC_LOAD_U32(from + firstOffset) == PPC_LOAD_U32(to + firstOffset);
            ReadDeltaField(ctx, base, msg, time, from, to, Field(table, index), 0, !same);
        } while (index != last);
    }

    // The fields of flags 2 among those before the last decode again.
    for (int32_t i = 0; i < last; ++i) {
        const uint32_t field = Field(table, i);
        if (FieldFlags(base, field) == 2)
            ReadDeltaField(ctx, base, msg, time, from, to, field, 0, !same);
    }
    CopyFields(base, from, to, table, last + 1, count);
    ctx = saved;
}

// MSG_ReadDeltaEntity(msg = r3, time = r4, from = r5, to = r6, number = r7,
//                     gap width = r8) -> 1 if the entity is removed
GUEST_HOOK(T_MSG_ReadDeltaEntity)
{
    const PPCContext saved = ctx;
    const uint32_t msg = saved.r3.u32;
    const uint32_t time = saved.r4.u32;
    const uint32_t from = saved.r5.u32;
    const uint32_t to = saved.r6.u32;
    const uint32_t number = saved.r7.u32;
    const uint32_t width = saved.r8.u32;
    ctx.r1.u64 = saved.r1.u32 - kEntityFrame;

    auto finish = [&](uint32_t result) {
        ctx = saved;
        ctx.r3.u64 = result;
    };
    auto shownet = [&] {
        int32_t value;
        return Shownet(base, value) && (value >= 2 || value == -1);
    };

    if (ReadBit(ctx, base, msg) == 1) {
        if (shownet())
            Call(ctx, base, GUEST_FUNC(T_Com_Printf), kChannel, T_STR_EntityRemove, PPC_LOAD_U32(msg + kReadCount),
                 number);
        finish(1);
        return;
    }
    if (ReadBit(ctx, base, msg) == 0) {
        std::memmove(base + to, base + from, kEntityStateSize);
        finish(0);
        return;
    }

    // When set, the fields of two float kinds (bits -83..-78) decode against
    // zero.
    const bool zeroKinds = ReadBit(ctx, base, msg) != 0;
    const uint32_t lastChanged = uint32_t(
        ReadBits(ctx, base, msg, BitsFor(PPC_LOAD_U32(T_DATA_EntityFieldCount))));
    if (lastChanged > PPC_LOAD_U32(T_DATA_EntityFieldCount)) {
        Call(ctx, base, GUEST_FUNC(T_MSG_Discard), msg);
        finish(0);
        return;
    }
    if (shownet())
        Call(ctx, base, GUEST_FUNC(T_Com_Printf), kChannel, T_STR_EntityNumber, PPC_LOAD_U32(msg + kReadCount),
             PPC_LOAD_U32(to));
    PPC_STORE_U32(to, number);

    // The eType decodes first, from the field every type's table starts with:
    // the type picks the table.
    const uint32_t eTypeOffset = FieldOffset(base, T_DATA_ETypeField);
    int32_t previous = -1;
    int32_t index = ReadGap(ctx, base, msg, width) - 1;
    if (index > 0) {
        PPC_STORE_U32(to + eTypeOffset, PPC_LOAD_U32(from + eTypeOffset));
    } else {
        ReadDeltaField(ctx, base, msg, time, from, to, T_DATA_ETypeField, 0, false);
        previous = 0;
        if (lastChanged != 0)
            index = ReadGap(ctx, base, msg, width);
    }

    const int32_t eType = int32_t(PPC_LOAD_U32(to + eTypeOffset));
    if (eType < 0) {
        Refuse(ctx, base, msg, "entity type", eType, int32_t(lastChanged), 0);
        finish(0);
        return;
    }
    const uint32_t type = Call(ctx, base, GUEST_FUNC(T_MSG_EntityTypeFields), uint32_t(eType));
    const uint32_t table = PPC_LOAD_U32(type);
    const uint32_t fields = PPC_LOAD_U32(type + 4);
    if (lastChanged > fields) {
        Call(ctx, base, GUEST_FUNC(T_Com_Printf), 14, T_STR_EntityLastChanged, lastChanged, fields);
        PPC_STORE_U32(msg, 1);   // overflowed
        finish(0);
        return;
    }
    const int32_t last = int32_t(lastChanged);

    // Whether a field decodes against zero: always when the old state is another
    // entity's (its second word differs), and for the two float kinds when asked.
    auto fromZero = [&](uint32_t field) {
        if (PPC_LOAD_U32(from + 4) != PPC_LOAD_U32(to + 4))
            return true;
        const int32_t bits = int32_t(PPC_LOAD_U32(field + kFieldBits));
        return zeroKinds && bits >= -83 && bits <= -78;
    };

    if (previous != last) {
        for (;;) {
            if (!Sent(index, previous, last, int32_t(fields))) {
                Refuse(ctx, base, msg, "entity", index, last, int32_t(fields));
                finish(0);
                return;
            }
            CopyFields(base, from, to, table, previous + 1, index);
            const uint32_t field = Field(table, index);
            ReadDeltaField(ctx, base, msg, time, from, to, field, 0, fromZero(field));
            previous = index;
            if (index == last)
                break;
            index += ReadGap(ctx, base, msg, width);
        }
    }

    // The fields of flags 2 among those before the last decode again.
    for (int32_t i = 0; i < last; ++i) {
        const uint32_t field = Field(table, i);
        if (FieldFlags(base, field) == 2)
            ReadDeltaField(ctx, base, msg, time, from, to, field, 0, fromZero(field));
    }
    CopyFields(base, from, to, table, previous + 1, int32_t(PPC_LOAD_U32(T_DATA_EntityFieldCount)));

    const uint32_t print = PPC_LOAD_U32(T_DATA_EntityPrintDvar);
    if (PPC_LOAD_U8(print + kDvarValue) != 0) {
        const uint32_t name = Call(ctx, base, GUEST_FUNC(T_BG_GetEntityTypeName), PPC_LOAD_U32(to + eTypeOffset));
        Call(ctx, base, GUEST_FUNC(T_Com_Printf), kChannel, T_STR_EntityChanged, number, name);
    }
    finish(0);
}

// MSG_ReadDeltaPlayerState(localClient = r3, msg = r4, time = r5, from = r6,
//                          to = r7, predicted = r8, a byte)
GUEST_HOOK(T_MSG_ReadDeltaPlayerState)
{
    const PPCContext saved = ctx;
    const uint32_t localClient = saved.r3.u32;
    const uint32_t msg = saved.r4.u32;
    const uint32_t time = saved.r5.u32;
    uint32_t from = saved.r6.u32;
    const uint32_t to = saved.r7.u32;
    const bool predicted = (saved.r8.u32 & 0xFF) != 0;
    ctx.r1.u64 = saved.r1.u32 - kPlayerStateFrame;

    if (from == 0) {
        from = ctx.r1.u32 + kEmptyPlayerState;
        std::memset(base + from, 0, kPlayerStateSize);
    }
    std::memmove(base + to, base + from, kPlayerStateSize);

    int32_t shownet;
    uint32_t print = 0;
    if (Shownet(base, shownet) && (shownet >= 2 || shownet == -2)) {
        Call(ctx, base, GUEST_FUNC(T_Com_Printf), kChannel, T_STR_PlayerState, PPC_LOAD_U32(msg + kReadCount));
        print = 1;
    }

    // Clear, the origin is the one the client sent, looked up below; set, the
    // fields of flags 3 of a predicted state decode against zero.
    const bool ownOrigin = Set(ReadBit(ctx, base, msg));
    const int32_t count = int32_t(PPC_LOAD_U32(T_DATA_PlayerStateFieldCount));
    const uint32_t table = T_DATA_PlayerStateFields;
    // One past the last changed field, 0 for none.
    int32_t lastChanged = ReadBits(ctx, base, msg, BitsFor(uint32_t(count + 1)));
    if (lastChanged > count + 1) {
        Call(ctx, base, GUEST_FUNC(T_MSG_Discard), msg);
        ctx = saved;
        return;
    }

    auto fromZero = [&](uint32_t field) { return predicted && ownOrigin && FieldFlags(base, field) == 3; };

    int32_t previous = -1;
    if (lastChanged > 0) {
        const int32_t last = --lastChanged;
        int32_t index;
        do {
            index = previous + ReadGap(ctx, base, msg, 4);
            if (!Sent(index, previous, last, count)) {
                Refuse(ctx, base, msg, "player state", index, last, count);
                ctx = saved;
                return;
            }
            // The skipped fields of flags 2 are left as they are.
            for (int32_t i = previous + 1; i < index; ++i) {
                const uint32_t field = Field(table, i);
                if (FieldFlags(base, field) != 2)
                    CopyField(base, from, to, field);
            }
            const uint32_t field = Field(table, index);
            ReadDeltaField(ctx, base, msg, time, from, to, field, print, fromZero(field));
            previous = index;
        } while (index < last);
    }
    CopyFields(base, from, to, table, previous + 1, count);

    // The fields of flags 2 among those before the last decode again.
    for (int32_t i = 0; i < lastChanged; ++i) {
        const uint32_t field = Field(table, i);
        if (FieldFlags(base, field) == 2)
            ReadDeltaField(ctx, base, msg, time, from, to, field, print, fromZero(field));
    }

    // The origin sent with the command it was predicted for; without it, the
    // old state's.
    if (!ownOrigin) {
        const uint32_t client = PPC_LOAD_U32(T_DATA_LocalClients) + localClient * T_LOCAL_CLIENT_STRIDE;
        if (Call(ctx, base, GUEST_FUNC(T_CL_FindSentOrigin), client, PPC_LOAD_U32(to), to + 28, to + 40, to + 24,
                 to + 172) == 0) {
            Call(ctx, base, GUEST_FUNC(T_Com_PrintMessage), kChannel, T_STR_OriginNotFound);
            for (uint32_t offset = 28; offset < 52; offset += 4)
                PPC_STORE_U32(to + offset, PPC_LOAD_U32(from + offset));
            PPC_STORE_U32(to + 24, PPC_LOAD_U32(from + 24));
            PPC_STORE_U32(to + 172, PPC_LOAD_U32(from + 172));
        }
    }

    // Stats: four, by a 4-bit mask.
    if (ReadBit(ctx, base, msg) != 0) {
        if (Shownet(base, shownet) && shownet == 4)
            Call(ctx, base, GUEST_FUNC(T_Com_Printf), kChannel, T_STR_PercentSSpace, T_STR_PS_STATS);
        const uint32_t mask = uint32_t(ReadBits(ctx, base, msg, 4));
        if (mask & 1)
            PPC_STORE_U32(to + 336, Call(ctx, base, GUEST_FUNC(T_MSG_ReadShort), msg));
        if (mask & 2)
            PPC_STORE_U32(to + 340, Call(ctx, base, GUEST_FUNC(T_MSG_ReadShort), msg));
        if (mask & 4)
            PPC_STORE_U32(to + 344, Call(ctx, base, GUEST_FUNC(T_MSG_ReadShort), msg));
        if (mask & 8)
            PPC_STORE_U32(to + 348, Call(ctx, base, GUEST_FUNC(T_MSG_ReadByte), msg));
    }

    // Ammo: 12-byte entries by a 4-bit index, an 11-bit word and two optional
    // bytes.
    if (ReadBit(ctx, base, msg) != 0) {
        do {
            const uint32_t entry = to + 848 + uint32_t(ReadBits(ctx, base, msg, 4)) * 12;
            PPC_STORE_U32(entry, uint32_t(ReadBits(ctx, base, msg, 11)));
            for (uint32_t k = 1; k <= 2; ++k)
                if (ReadBit(ctx, base, msg) != 0)
                    PPC_STORE_U32(entry + k * 4, uint32_t(ReadBits(ctx, base, msg, 8)));
        } while (ReadBit(ctx, base, msg) != 0);
    }

    // The 15 slots: a word at +544 and five bytes at +604 each.
    if (ReadBit(ctx, base, msg) != 0) {
        for (uint32_t slot = 0; slot < 15; ++slot) {
            if (ReadBit(ctx, base, msg) == 0)
                continue;
            const uint32_t word = to + 544 + slot * 4;
            const uint32_t bytes = to + 604 + slot * 5;
            if (ReadBit(ctx, base, msg) != 0)
                PPC_STORE_U32(word, uint32_t(ReadBits(ctx, base, msg, 11)));
            PPC_STORE_U8(bytes + 0, Set(ReadBit(ctx, base, msg)));
            PPC_STORE_U8(bytes + 1, Set(ReadBit(ctx, base, msg)));
            PPC_STORE_U8(bytes + 3, Set(ReadBit(ctx, base, msg)));
            PPC_STORE_U8(bytes + 4, Set(ReadBit(ctx, base, msg)));
            if (ReadBit(ctx, base, msg) != 0)
                PPC_STORE_U8(bytes + 2, uint8_t(Call(ctx, base, GUEST_FUNC(T_MSG_ReadByte), msg)));
        }
    }

    // Weapons: two words, by a 4-bit index.
    if (ReadBit(ctx, base, msg) != 0) {
        do {
            const uint32_t weapon = uint32_t(ReadBits(ctx, base, msg, 4));
            PPC_STORE_U32(to + (weapon + 91) * 8, uint32_t(ReadBits(ctx, base, msg, 11)));
            PPC_STORE_U32(to + weapon * 8 + 732, uint32_t(ReadBits(ctx, base, msg, 10)));
        } while (ReadBit(ctx, base, msg) != 0);
    }

    // The 32 objectives, 28 bytes each: a 3-bit state, then their fields.
    if (ReadBit(ctx, base, msg) != 0) {
        for (uint32_t i = 0; i < 32; ++i) {
            const uint32_t objective = 1208 + i * 28;
            PPC_STORE_U32(to + objective, uint32_t(ReadBits(ctx, base, msg, 3)));
            Call(ctx, base, GUEST_FUNC(T_MSG_ReadDeltaFields), msg, time, from + objective, to + objective,
                 PPC_LOAD_U32(T_DATA_ObjectiveFieldCount), T_DATA_ObjectiveFields, 1, 0);
        }
    }

    // The two hud element arrays, 31 elements each.
    if (ReadBit(ctx, base, msg) != 0) {
        Call(ctx, base, GUEST_FUNC(T_MSG_ReadDeltaHudElems), msg, time, from + 7328, to + 7328, 31);
        Call(ctx, base, GUEST_FUNC(T_MSG_ReadDeltaHudElems), msg, time, from + 2120, to + 2120, 31);
    }
    ctx = saved;
}

#endif
