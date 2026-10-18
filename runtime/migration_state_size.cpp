// A migrated game state is read only from inside its buffer.
//
// In-game host migration: the server streams its game state, a compressed
// save file, to the client it picked as the next host. mhead (sub_8218BCA0)
// takes a 1152-byte save header into the static header (0x83050600, what
// sub_822664A8(0) returns) and points the migration buffer at 0x830FB150,
// 0x180000 bytes, which ends at the buffer's own descriptor (0x8327B150,
// sub_82289370); mdata (sub_8218BA58) fills it in 1200-byte chunks, each kept
// inside the 0x180000. When the client becomes the host, G_InitGame
// (sub_82233A68) restores the state through sub_8223F360, which opens the buffer
// as a file (sub_823125C0) whose length is the header's word at +1116, and the
// header check (sub_8223F2B0) first checksums that many bytes from the buffer
// (sub_82265000). Nothing compares that length with the buffer: a server can
// name any length, and the checksum and the reads run past the buffer.
//
// Inside the file the state is in segments, each a size word (the word itself
// included) and size-4 bytes of compressed data. Opening segment n
// (sub_82312360) adds up the first n size words from offset 0 and hands the
// inflater the n-th segment's data and size-4 as its input. No size word is
// compared with the file's length (+4) either, so a size word can put the
// inflater's input anywhere in guest memory.
//
// The server writes the state into a buffer of the same 0x180000 (sub_822C4DB0
// sets the same one up before saving into it), so a length above it, or a
// segment that runs past the length, is a file no server wrote. Both are
// refused with the error the file's own reader raises for data it cannot read
// (sub_82311F50 at 82311FE0: Com_Error(ERR_DROP, "Trying to read corrupted
// file")), before the read. The segment check is in the segment opener, so it
// holds for every compressed file the title reads this way; a well-formed file
// passes it untouched.
//
// Reads only: the console would fault on an unmapped address, this build reads
// zeros. MP-only: the campaign has no host migration.
#include <ppc_recomp_shared.h>
#include "title.h"
#include "log.h"

#include <cstdint>

#if defined(T_G_LoadMigratedGame) && defined(T_MemFile_BeginSegment) && defined(T_Com_Error) \
    && defined(T_DATA_MigrationHeader) && defined(T_STR_CorruptedFile)

namespace {

constexpr uint32_t kBufferSize = 0x180000;   // the migration buffer, as mhead and the server set it
constexpr uint32_t kHeaderLength = 1116;     // the header's file length
constexpr uint32_t kFileData = 0;            // file object: data, length, read mode, segment
constexpr uint32_t kFileLength = 4;
constexpr uint32_t kFileWriting = 29;

[[noreturn]] void Corrupted(PPCContext& ctx, uint8_t* base)
{
    ctx.r1.u64 = ctx.r1.u32 - 0x80;
    ctx.r3.u64 = 1;                          // ERR_DROP, as the reader raises it
    ctx.r4.u64 = T_STR_CorruptedFile;
    GUEST_FUNC(T_Com_Error)(ctx, base);      // does not return
    __builtin_unreachable();
}

} // namespace

// (r3 = where it returns the header, r4 = where it returns the level time)
GUEST_HOOK(T_G_LoadMigratedGame)
{
    const uint32_t length = PPC_LOAD_U32(T_DATA_MigrationHeader + kHeaderLength);
    if (length > kBufferSize) {
        LOGE("host migration: the game state names %u bytes, more than its %u-byte buffer",
             length, kBufferSize);
        Corrupted(ctx, base);
    }
    GUEST_ORIG(T_G_LoadMigratedGame)(ctx, base);
}

// (file = r3, segment = r4; a negative segment closes the file)
GUEST_HOOK(T_MemFile_BeginSegment)
{
    const uint32_t file = ctx.r3.u32;
    const int32_t segment = ctx.r4.s32;
    if (segment >= 0 && PPC_LOAD_U8(file + kFileWriting) == 0) {
        const uint32_t data = PPC_LOAD_U32(file + kFileData);
        const uint64_t length = PPC_LOAD_U32(file + kFileLength);
        uint64_t offset = 0;
        for (int32_t i = 0; i <= segment; ++i) {
            const uint64_t size = offset + 4 <= length
                                ? PPC_LOAD_U32(uint32_t(data + offset)) : 0;
            if (size < 4 || offset + size > length) {
                LOGE("save file: segment %d of %d runs past the file's %llu bytes "
                     "(at %llu, size word %llu)", i, segment, (unsigned long long)length,
                     (unsigned long long)offset, (unsigned long long)size);
                Corrupted(ctx, base);
            }
            offset += size;
        }
    }
    GUEST_ORIG(T_MemFile_BeginSegment)(ctx, base);
}

#endif
