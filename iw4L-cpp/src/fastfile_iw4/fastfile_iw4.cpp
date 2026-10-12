// Port of iw4L crates/fastfile_iw4/src/envelope.rs (parse_file_header).
#include "fastfile_iw4/fastfile_iw4.h"

#include <cstring>

namespace fastfile_iw4 {

static uint32_t read_u32_le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

FileHeaderResult parse_file_header(const uint8_t* bytes, size_t len) {
    if (len < FILE_PREAMBLE_LEN) {
        return FileHeaderErrorTooShort{len};
    }
    uint8_t magic[8];
    std::memcpy(magic, bytes, 8);
    Signing signing;
    if (std::memcmp(magic, MAGIC_SIGNED, 8) == 0) {
        signing = Signing::Signed;
    } else if (std::memcmp(magic, MAGIC_UNSIGNED, 8) == 0) {
        signing = Signing::Unsigned;
    } else {
        FileHeaderErrorBadMagic err;
        std::memcpy(err.got, magic, 8);
        return err;
    }
    const uint32_t version = read_u32_le(bytes + 8);
    if (version != ZONE_VERSION_PC) {
        return FileHeaderErrorBadVersion{version};
    }
    FileHeader header;
    header.signing = signing;
    header.version = version;
    header.flag = bytes[12];
    header.stamp[0] = read_u32_le(bytes + 13);
    header.stamp[1] = read_u32_le(bytes + 17);
    return header;
}

} // namespace fastfile_iw4
