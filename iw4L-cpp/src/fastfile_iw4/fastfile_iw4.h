// Port of iw4L crates/fastfile_iw4/src/envelope.rs.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>

namespace fastfile_iw4 {

inline constexpr uint8_t MAGIC_SIGNED[8] = {'I','W','f','f','0','1','0','0'};
inline constexpr uint8_t MAGIC_UNSIGNED[8] = {'I','W','f','f','u','1','0','0'};
inline constexpr uint8_t MAGIC_AUTH_HEADER[8] = {'I','W','f','f','s','1','0','0'};
inline constexpr uint32_t ZONE_VERSION_PC = 0x114;
inline constexpr size_t FILE_PREAMBLE_LEN = 8 + 4 + 1 + 8;

enum class Signing : uint8_t { Signed, Unsigned };

struct FileHeader {
    Signing signing = Signing::Unsigned;
    uint32_t version = 0;
    uint8_t flag = 0;
    uint32_t stamp[2] = {0, 0};
};

struct FileHeaderErrorTooShort { size_t len = 0; };
struct FileHeaderErrorBadMagic { uint8_t got[8] = {}; };
struct FileHeaderErrorBadVersion { uint32_t got = 0; };

using FileHeaderResult = std::variant<FileHeader,
                                      FileHeaderErrorTooShort,
                                      FileHeaderErrorBadMagic,
                                      FileHeaderErrorBadVersion>;

FileHeaderResult parse_file_header(const uint8_t* bytes, size_t len);

} // namespace fastfile_iw4
