// A co-op frame the host sends must lie inside the packet that carries it.
//
// The client takes the host's frames in sub_8233BAF0: a byte, the number of
// frames (1..20), and for each frame a short, its length, followed by the
// frame itself, whose first bytes are its header (sub_8233A4C0: three longs and
// a byte). A frame the client keeps is copied whole, from the packet at the
// position after its length, into one of twenty 4096-byte frame buffers
// (MSG_WriteData, at 8233BD0C), and the next frame is read from that position
// plus the length (8233BDA0). The destination is bounded -- MSG_WriteData
// refuses more than 4096 -- but nothing compares the length with what is left
// of the packet, so the copy reads up to 4 KB past the packet's end: past the
// datagram into the rest of the receive buffer, and past the buffer's 16 KB
// into whatever follows it. Those bytes then run as the frame's commands.
//
// The host writes each frame as its length, the frame's own cursize, followed
// by exactly that many bytes (sub_8233AE80, 8233B0F8..8233B10C). A host's
// packet holds less than a length says only when the packet filled up:
// MSG_WriteShort and MSG_WriteData check for room and not for the overflow
// flag, and the sender's loop never looks at it, so a frame that does not fit
// leaves its length without its bytes, and a later, smaller frame that does fit
// still follows. The client then reads that frame's header out of the next
// frame and, should it match, copies past the packet's end -- a host can cause
// the over-read without meaning to.
//
// So the packet is cut, not dropped: at the first frame whose length runs past
// the end of the packet, its cursize (+20) is set to where that frame's length
// starts. sub_8233BAF0 then takes the frames before it, which are whole, and
// stops at the next length, as it does at the end of any truncated packet;
// what the host meant to send past that point was never in the packet.
//
// The packet is walked with the title's own MSG_ReadByte, MSG_ReadShort and
// header reader, stopping where sub_8233BAF0 stops (a length of 0 or less, a
// read past the end, a header that does not read); the message's read position
// (+28) and overflow flag (+0), the only fields those routines write, are put
// back afterwards. Whether sub_8233BAF0 then keeps or skips a frame (another
// server, an old frame, a full ring) does not change where its bytes are, so
// the walk does not need to know. When sub_8233BAF0 ignores the packet without
// reading it (the byte at 0x83A74289, 8233BB04), the walk is skipped too.
//
// Campaign only: the multiplayer has no co-op frames.
#include <ppc_recomp_shared.h>
#include "title.h"
#include "log.h"

#include <atomic>
#include <cstdint>

#if defined(T_Coop_ReceiveFrames) && defined(T_Coop_ReadFrameHeader) && defined(T_MSG_ReadByte) \
    && defined(T_MSG_ReadShort)

namespace {

constexpr int32_t kMaxFrames = 20;     // sub_8233BAF0's own bound
constexpr uint32_t kOverflowed = 0;    // msg_t
constexpr uint32_t kCurSize = 20;
constexpr uint32_t kReadCount = 28;
constexpr uint32_t kIgnoreFrames = 0x83A74289u;   // sub_8233BAF0 returns at once when set

// The frames' lengths, read as sub_8233BAF0 reads them, with the message's read
// state put back afterwards. Where the first frame whose header reads but whose
// length runs past the end of the packet starts (its length's position), or 0.
uint32_t FirstOverrun(PPCContext& ctx, uint8_t* base, uint32_t msg, int32_t& length, int32_t& left)
{
    const PPCContext saved = ctx;
    const uint32_t overflowed = PPC_LOAD_U32(msg + kOverflowed);
    const uint32_t readCount = PPC_LOAD_U32(msg + kReadCount);
    // A frame of our own below the caller's, with room for the header's four
    // results.
    constexpr uint32_t kOut = 0x80, kFrame = 0x100;
    ctx.r1.u64 = saved.r1.u32 - kFrame;
    const uint32_t out = ctx.r1.u32 + kOut;

    uint32_t overrun = 0;
    ctx.r3.u64 = msg;
    GUEST_FUNC(T_MSG_ReadByte)(ctx, base);
    const int32_t frames = ctx.r3.s32;
    for (int32_t i = 0; frames <= kMaxFrames && i < frames; ++i) {
        const uint32_t lengthAt = PPC_LOAD_U32(msg + kReadCount);
        ctx.r3.u64 = msg;
        GUEST_FUNC(T_MSG_ReadShort)(ctx, base);
        length = ctx.r3.s32;
        if (length <= 0 || PPC_LOAD_U32(msg + kOverflowed))
            break;
        const int32_t start = PPC_LOAD_U32(msg + kReadCount);
        ctx.r3.u64 = msg;
        ctx.r4.u64 = out;
        ctx.r5.u64 = out + 4;
        ctx.r6.u64 = out + 8;
        ctx.r7.u64 = out + 12;
        GUEST_FUNC(T_Coop_ReadFrameHeader)(ctx, base);
        if (!(ctx.r3.u32 & 0xFF))
            break;
        left = int32_t(PPC_LOAD_U32(msg + kCurSize)) - start;
        if (length > left) {
            overrun = lengthAt;
            break;
        }
        PPC_STORE_U32(msg + kReadCount, uint32_t(start + length));
    }

    PPC_STORE_U32(msg + kOverflowed, overflowed);
    PPC_STORE_U32(msg + kReadCount, readCount);
    ctx = saved;
    return overrun;
}

} // namespace

// (msg = r3), the host's frames after the type and direction bytes
GUEST_HOOK(T_Coop_ReceiveFrames)
{
    const uint32_t msg = ctx.r3.u32;
    int32_t length = 0, left = 0;
    const uint32_t overrun = PPC_LOAD_U8(kIgnoreFrames) ? 0 : FirstOverrun(ctx, base, msg, length, left);
    if (overrun) {
        PPC_STORE_U32(msg + kCurSize, overrun);
        // Every power of two, so a flood stays a few lines.
        static std::atomic<uint32_t> count;
        const uint32_t n = ++count;
        if (!(n & (n - 1)))
            LOGW("online: cut co-op frames from the host before a %d-byte frame with %d bytes left in the packet (%u so far)",
                 length, left, n);
    }
    GUEST_ORIG(T_Coop_ReceiveFrames)(ctx, base);
}

#endif
