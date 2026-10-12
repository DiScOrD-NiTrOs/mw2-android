// Port of iw4L crates/fastfile_iw4/src/envelope.rs.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>
#include <utility>
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

// asset_type.rs ------------------------------------------------------------

enum class AssetType : uint32_t {
    PhysPreset = 0x00,
    PhysCollmap = 0x01,
    XAnimParts = 0x02,
    XModelSurfs = 0x03,
    XModel = 0x04,
    Material = 0x05,
    PixelShader = 0x06,
    VertexShader = 0x07,
    VertexDecl = 0x08,
    TechniqueSet = 0x09,
    Image = 0x0A,
    Sound = 0x0B,
    SoundCurve = 0x0C,
    LoadedSound = 0x0D,
    ClipMapSp = 0x0E,
    ClipMapMp = 0x0F,
    ComWorld = 0x10,
    GameWorldSp = 0x11,
    GameWorldMp = 0x12,
    MapEnts = 0x13,
    FxWorld = 0x14,
    GfxWorld = 0x15,
    LightDef = 0x16,
    UiMap = 0x17,
    Font = 0x18,
    MenuList = 0x19,
    Menu = 0x1A,
    Localize = 0x1B,
    Weapon = 0x1C,
    SndDriverGlobals = 0x1D,
    Fx = 0x1E,
    ImpactFx = 0x1F,
    AiType = 0x20,
    MpType = 0x21,
    Character = 0x22,
    XModelAlias = 0x23,
    RawFile = 0x24,
    StringTable = 0x25,
    LeaderBoard = 0x26,
    StructuredDataDef = 0x27,
    Tracer = 0x28,
    Vehicle = 0x29,
    AddonMapEnts = 0x2A,
};

// Returns 0xFF when the value is not a valid asset type.
inline AssetType asset_type_from_u32(uint32_t v) {
    // Upstream matches 0x00..0x2A with no gaps (0x1F is ImpactFx).
    return v <= 0x2A ? static_cast<AssetType>(v) : static_cast<AssetType>(0xFF);
}

// stream.rs ----------------------------------------------------------------

uint32_t convert_offset_to_pointer(uint32_t offset, const uint32_t blocks[16]);

std::optional<uint32_t> convert_offset_to_alias(uint32_t offset, const uint8_t* const blocks[16]);

std::optional<uint32_t> load_stream(uint8_t stream_index, uint8_t active_index,
                                    uint8_t* destination, size_t destination_len,
                                    const uint8_t* source, size_t source_len, uint32_t position);

inline uint32_t alloc_stream_pos(uint32_t position, uint32_t alignment) {
    return position + alignment & ~alignment;
}
inline uint32_t inc_stream_pos(uint32_t position, uint32_t size) {
    return position + size;
}

struct StreamPosStack {
    uint32_t positions[8] = {};
    // stack of (index, position)
    std::pair<uint32_t, uint32_t> stack[64] = {};
    size_t depth = 0;
};

std::optional<std::tuple<uint32_t, uint32_t, uint32_t>> insert_pointer(
    size_t active_index, uint32_t position, StreamPosStack& s);

std::optional<std::pair<size_t, uint32_t>> push_stream_pos(
    size_t active_index, uint32_t position, StreamPosStack& s, size_t target_index);

std::optional<std::pair<size_t, uint32_t>> pop_stream_pos(
    size_t active_index, uint32_t position, StreamPosStack& s);

// zone.rs (header part) -----------------------------------------------------

inline constexpr size_t MAX_XFILE_COUNT = 8;
inline constexpr size_t XFILE_BLOCK_VIRTUAL = 3;
inline constexpr size_t XFILE_HEADER_LEN = 8 + 4 * MAX_XFILE_COUNT;

struct ZoneHeader {
    uint32_t size = 0;
    uint32_t external_size = 0;
    uint32_t block_size[MAX_XFILE_COUNT] = {};
};

std::optional<ZoneHeader> parse_zone_header(const uint8_t* image, size_t len);

// wire.rs -------------------------------------------------------------------

enum class Iw4WireFormat : uint8_t { X86, X64 };

inline size_t pointer_bytes(Iw4WireFormat f) { return f == Iw4WireFormat::X86 ? 4 : 8; }

struct WirePtr {   // Ptr in lib.rs
    uint8_t block = 0;
    uint32_t offset = 0;
};

struct WirePointer {
    enum class Kind : uint8_t { Null, Following, Insert, Offset } kind = Kind::Null;
    WirePtr ptr = {}; // valid when kind == Offset
};

std::optional<WirePointer> wire_pointer_decode(uint64_t raw, Iw4WireFormat format);

enum class WireTableError : uint8_t {
    Truncated,
    InvalidCountPointer,
    InvalidPointer,
    BlockBounds,
    UnknownAssetType,
    UnterminatedString,
    TrailingEmptyData,
};

struct WireAssetTable {
    Iw4WireFormat format = Iw4WireFormat::X86;
    size_t string_count = 0;
    size_t asset_count = 0;
    size_t bodies_offset = 0;
    // Borrowed views into the input buffer.
    const uint8_t* entries = nullptr;
    size_t entries_len = 0;
    const uint8_t* string_slots = nullptr;
    size_t string_slots_len = 0;
    const uint8_t* string_data = nullptr;
    size_t string_data_len = 0;
};

std::variant<WireAssetTable, WireTableError> read_wire_asset_table(const uint8_t* data,
                                                                   size_t len,
                                                                   Iw4WireFormat format);

} // namespace fastfile_iw4
