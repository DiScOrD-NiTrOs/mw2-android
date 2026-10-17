// iw4x issue: UI_ReplaceDirective ("[{name}]") stack overflow.
//
// A HUD/menu string can embed directives of the form "[{name}]" that the UI
// expands to a looked-up value. UI_ReplaceDirective (sub_822F0230 in the
// multiplayer, sub_822CDAA0 in the campaign) walks the source string and, while
// it is inside a directive name, accumulates each name character into a
// 256-byte buffer on its own stack frame:
//
//     loc_...033C:
//         stb  r10,0(r29)        ; namebuf[k] = char
//         addi r29,r29,1         ; k++   -- no bound against namebuf+256
//
// The loop's only guard tests the *output* cursor (r30 >= dest+size-1), and the
// output cursor does not advance while a name is being read, so a directive
// name longer than 256 bytes runs the cursor off the buffer: past it sit a
// second 256-byte scratch, then the saved registers and return address of a
// 688-byte frame -> a stack smash. The source is a server-supplied HUD string
// (scoreboard / hud strings), so a malicious host reaches this on a client; the
// campaign's co-op (Spec Ops) host reaches a campaign client the same way.
//
// iw4x bounds the name copy. The recompiler has no mid-function hook, so this
// replaces the whole function with a faithful transcription of its parse loop
// -- streaming the source exactly as the original does, reusing the title's own
// directive lookup (sub_822F0140), bounded string copy (I_strncpyz,
// sub_823159B0) and message printer (sub_8227F5E0) -- with the one missing
// bound added: the name buffer never takes more than 255 characters (leaving
// room for the terminator). A legitimate directive name is a short token, so
// nothing real is truncated; an over-long name simply fails to resolve, which
// is how the engine already treats a name it cannot find.
#include <ppc_recomp_shared.h>
#include "title.h"

#include <cstdint>

#if defined(T_UI_ReplaceDirective) && defined(T_UI_LookupDirective) && \
    defined(T_I_strncpyz) && defined(T_Com_PrintMessage)

namespace
{
    constexpr uint32_t kNameMax = 255;   // 256-byte buffer, less the terminator

    // sub_8227F5E0(channel = 13, fmt, arg) -- the title's own warning printer,
    // called for a malformed directive exactly as the original does. Loop state
    // lives in this function's locals, so clobbering ctx across the call is
    // fine; r1 already sits below our scratch (see the hook).
    void PrintDirectiveWarning(PPCContext& ctx, uint8_t* base, uint32_t fmt, uint32_t arg)
    {
        ctx.r3.u64 = 13;
        ctx.r4.u64 = fmt;
        ctx.r5.u64 = arg;
        GUEST_FUNC(T_Com_PrintMessage)(ctx, base);
    }
}

// UI_ReplaceDirective(ctx = r3, src = r4, dest = r5, destSize = r6).
GUEST_HOOK(T_UI_ReplaceDirective)
{
    const uint32_t uiCtx = ctx.r3.u32;
    const uint32_t src = ctx.r4.u32;
    const uint32_t dest = ctx.r5.u32;
    const int32_t destSize = ctx.r6.s32;

    // Two 256-byte buffers of our own -- the directive name and the lookup
    // result -- carved below the incoming stack pointer, with r1 moved beneath
    // them (leaving a parameter-save area for the printer's varargs) so the
    // reused guest routines' frames cannot land on them.
    const uint32_t sp0 = ctx.r1.u32;
    const uint32_t gName = sp0 - 0x100;
    const uint32_t gVal = sp0 - 0x200;
    ctx.r1.u64 = sp0 - 0x300;

    const uint32_t end = dest + uint32_t(destSize);
    if (destSize > 0)
        PPC_STORE_U8(end - 1, 0);   // dest[destSize-1] = 0, as the original does

    uint32_t p = src;          // source cursor
    uint32_t out = dest;       // output cursor
    uint32_t nameLen = 0;      // characters in the directive-name buffer
    bool inName = false;

    uint8_t c = PPC_LOAD_U8(p);
    while (c != 0)
    {
        if (out >= end - 1)   // output buffer full
            break;

        if (c == '[' && PPC_LOAD_U8(p + 1) == '{')
        {
            // begin a directive name
            inName = true;
            nameLen = 0;
            ++p;   // skip '[' (the trailing ++p below skips '{')
        }
        else if (c == '}' && PPC_LOAD_U8(p + 1) == ']')
        {
            // end a directive name
            ++p;   // skip '}' (the trailing ++p below skips ']')
            inName = false;
            PPC_STORE_U8(gName + nameLen, 0);
            if (nameLen == 0)
            {
                PrintDirectiveWarning(ctx, base, T_STR_DirectiveEmpty, p);
            }
            else
            {
                // LookupDirective(uiCtx, name, value, 256)
                ctx.r3.u64 = uiCtx;
                ctx.r4.u64 = gName;
                ctx.r5.u64 = gVal;
                ctx.r6.u64 = 256;
                GUEST_FUNC(T_UI_LookupDirective)(ctx, base);

                // I_strncpyz(out, value, remaining) then advance past the value
                const uint32_t remaining = uint32_t(destSize) - (out - dest);
                ctx.r3.u64 = out;
                ctx.r4.u64 = gVal;
                ctx.r5.u64 = remaining;
                GUEST_FUNC(T_I_strncpyz)(ctx, base);
                while (PPC_LOAD_U8(out) != 0)
                    ++out;
            }
        }
        else if (inName)
        {
            // *** the fix: accept at most kNameMax name characters, then stop
            // writing (the name still ends at its "}]"); the original stored
            // every character with no bound.
            if (nameLen < kNameMax)
            {
                PPC_STORE_U8(gName + nameLen, c);
                ++nameLen;
            }
        }
        else
        {
            PPC_STORE_U8(out, c);
            ++out;
        }

        c = PPC_LOAD_U8(++p);
    }

    if (out < end)
        PPC_STORE_U8(out, 0);   // terminate the output
    if (inName)                 // reached the end of the string mid-directive
        PrintDirectiveWarning(ctx, base, T_STR_DirectiveNoEnd, p);

    ctx.r1.u64 = sp0;   // balance the stack the function was entered with
}

#endif
