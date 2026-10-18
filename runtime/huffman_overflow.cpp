// CVE-2018-10718 and its client-side twin -- the netchan Huffman decompressor.
//
// A netchan message's payload is Huffman-compressed. MSG_ReadBitsCompress
// (sub_822897C0) expands it one byte per decoded symbol, and its loop stops only
// when it has consumed the *input* bits -- it never checks the *output* against
// the buffer it is writing to. Huffman can expand, so a crafted payload decodes
// to far more bytes than the caller's buffer holds:
//
//   - the server reads a client's message into a 2048-byte buffer on its own
//     stack (SV_ParseClientMessage, sub_822BCB18): an overrun smashes the return
//     address -- a remote, near-pre-auth RCE on the host;
//   - the client reads the host's message into a 64 KB hunk buffer
//     (CL_ParseServerMessage, sub_8218DF10): a malicious host overruns the heap
//     arena -- the same bug, host to client.
//
// The cause is one shared function with no output limit, so the fix is there.
// The decompressor cannot see how big its destination is, so each caller
// declares the size of the buffer it passes (the title's own 2048 / 64 KB) in a
// thread-local before calling the original; the decompressor hook decodes with
// that ceiling and, rather than writing past it, rejects the whole message
// (returns 0, as iw4x's bounds-checked decompress does). A legitimate message
// always fits the engine's own buffer, so nothing real is turned away.
//
// The decode of each symbol stays the title's own Huffman_DecodeSymbol, so the
// bitstream is read exactly as the title reads it -- no second implementation to
// drift, and nothing host-width-dependent.
#include <ppc_recomp_shared.h>
#include "title.h"
#include "log.h"

#include <cstdint>

#if defined(T_MSG_ReadBitsCompress) && defined(T_Huffman_DecodeSymbol)

namespace
{
    // The destination buffer size the current caller passed, or 0 when the
    // decompressor is reached from somewhere this file does not know (then it
    // behaves exactly as the original). Set around the original call below.
    thread_local uint32_t g_outputCeiling = 0;

    struct Ceiling
    {
        explicit Ceiling(uint32_t bytes) { g_outputCeiling = bytes; }
        ~Ceiling() { g_outputCeiling = 0; }
    };
}

// sub_822897C0(from = r3, to = r4, size = r5) -> bytes written (r3).
GUEST_HOOK(T_MSG_ReadBitsCompress)
{
    const uint32_t ceiling = g_outputCeiling;
    g_outputCeiling = 0;   // one-shot: applies only to this call
    if (ceiling == 0)
    {
        GUEST_ORIG(T_MSG_ReadBitsCompress)(ctx, base);
        return;
    }

    const uint32_t from = ctx.r3.u32;
    const uint32_t to = ctx.r4.u32;
    const int32_t size = ctx.r5.s32;
    const uint32_t totalBits = size > 0 ? uint32_t(size) * 8u : 0u;

    const PPCContext saved = ctx;

    // Two 4-byte slots for Huffman_DecodeSymbol's out-pointers (the decoded byte
    // and the bit cursor), in a small guest frame of our own. r1 is moved below
    // them so the callee's own stack use cannot land on them, exactly as the
    // original holds them inside its frame.
    const uint32_t sp = ctx.r1.u32;
    const uint32_t gByte = sp - 12;
    const uint32_t gBitpos = sp - 16;
    ctx.r1.u64 = sp - 64;
    PPC_STORE_U32(gBitpos, 0);

    const uint32_t tree = PPC_LOAD_U32(T_DATA_HuffmanTree + 8);

    uint32_t written = 0;
    bool overran = false;
    if (totalBits != 0)
    {
        for (;;)
        {
            if (written >= ceiling)   // one more byte would leave the buffer
            {
                overran = true;
                break;
            }
            ctx.r3.u64 = tree;
            ctx.r4.u64 = gByte;
            ctx.r5.u64 = from;
            ctx.r6.u64 = gBitpos;
            GUEST_FUNC(T_Huffman_DecodeSymbol)(ctx, base);
            PPC_STORE_U8(to + written, uint8_t(PPC_LOAD_U32(gByte)));
            ++written;
            if (PPC_LOAD_U32(gBitpos) >= totalBits)   // all input consumed: it fit
                break;
        }
    }

    ctx = saved;
    if (overran)
    {
        static bool logged;
        if (!logged)
            LOGW("online: rejected an over-long Huffman message (> %u bytes) -- CVE-2018-10718", ceiling);
        logged = true;
        ctx.r3.u64 = 0;   // empty message; the parser handles a zero-length read
        return;
    }
    ctx.r3.u64 = written;
}

#ifdef T_SV_ParseClientMessage
GUEST_HOOK(T_SV_ParseClientMessage)
{
    Ceiling ceiling(2048);   // the client message's 2048-byte stack buffer
    GUEST_ORIG(T_SV_ParseClientMessage)(ctx, base);
}
#endif

#ifdef T_CL_ParseServerMessage
GUEST_HOOK(T_CL_ParseServerMessage)
{
    Ceiling ceiling(0x10000);   // the server message's 64 KB hunk buffer
    GUEST_ORIG(T_CL_ParseServerMessage)(ctx, base);
}
#endif

#endif
