// Port of iw4L crates/fastfile_iw4/src/wire.rs + zone.rs header part.
#include "fastfile_iw4/fastfile_iw4.h"

#include <cstring>

namespace fastfile_iw4 {

namespace {

uint32_t rd_u32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v; // little-endian host assumed; x86/64 target data
}

uint64_t rd_u64(const uint8_t* p) {
    uint64_t v;
    std::memcpy(&v, p, 8);
    return v;
}

struct BytesResult {
    const uint8_t* ptr = nullptr;
    size_t len = 0;
    bool ok = false;
};

BytesResult bytes(const uint8_t* data, size_t len, size_t at, size_t needed) {
    BytesResult r;
    if (at <= len && len - at >= needed) {
        r.ptr = data + at;
        r.len = needed;
        r.ok = true;
    }
    return r;
}

bool is_count_pointer(size_t count, const WirePointer& p) {
    return (count == 0 && p.kind == WirePointer::Kind::Null) ||
           (count >= 1 && p.kind == WirePointer::Kind::Following);
}

} // namespace

std::optional<ZoneHeader> parse_zone_header(const uint8_t* image, size_t len) {
    if (len < XFILE_HEADER_LEN) {
        return std::nullopt;
    }
    ZoneHeader header;
    header.size = rd_u32(image);
    header.external_size = rd_u32(image + 4);
    for (size_t i = 0; i < MAX_XFILE_COUNT; ++i) {
        header.block_size[i] = rd_u32(image + 8 + i * 4);
    }
    return header;
}

std::optional<WirePointer> wire_pointer_decode(uint64_t raw, Iw4WireFormat format) {
    const uint64_t following =
        format == Iw4WireFormat::X86 ? static_cast<uint64_t>(0xFFFFFFFFu) : 0xFFFFFFFFFFFFFFFFull;
    if (raw == 0) {
        WirePointer p; p.kind = WirePointer::Kind::Null;
        return p;
    }
    if (raw == following) {
        WirePointer p; p.kind = WirePointer::Kind::Following;
        return p;
    }
    if (raw == following - 1) {
        WirePointer p; p.kind = WirePointer::Kind::Insert;
        return p;
    }
    if (raw < 1) return std::nullopt; // checked_sub(1)
    const uint64_t minus = raw - 1;
    if (minus > 0xFFFFFFFFull) return std::nullopt; // u32::try_from
    const uint32_t packed = static_cast<uint32_t>(minus);
    const uint8_t block = static_cast<uint8_t>(packed >> 28);
    if (block >= MAX_XFILE_COUNT) return std::nullopt;
    WirePointer p;
    p.kind = WirePointer::Kind::Offset;
    p.ptr.block = block;
    p.ptr.offset = packed & 0x0fffffffu;
    return p;
}

namespace {

struct PointerResult {
    WirePointer p;
    WireTableError error = WireTableError::InvalidPointer;
    bool ok = false;
    size_t error_at = 0;
};

PointerResult pointer(const uint8_t* data, size_t len, size_t at, Iw4WireFormat format) {
    PointerResult r;
    const size_t width = pointer_bytes(format);
    uint8_t raw_bytes[8] = {};
    const BytesResult b = bytes(data, len, at, width);
    if (!b.ok) {
        r.error = WireTableError::Truncated;
        r.error_at = at;
        return r;
    }
    std::memcpy(raw_bytes, b.ptr, width);
    const uint64_t raw = rd_u64(raw_bytes);
    const std::optional<WirePointer> decoded = wire_pointer_decode(raw, format);
    if (!decoded) {
        r.error_at = at;
        return r; // InvalidPointer
    }
    r.p = *decoded;
    r.ok = true;
    return r;
}

bool is_known_asset_type(uint32_t raw) {
    return raw <= 0x2A;
}

} // namespace

std::variant<WireAssetTable, WireTableError> read_wire_asset_table(const uint8_t* data,
                                                                   size_t len,
                                                                   Iw4WireFormat format) {
    const std::optional<ZoneHeader> header = parse_zone_header(data, len);
    if (!header) {
        return WireTableError::Truncated;
    }
    const size_t width = pointer_bytes(format);
    const size_t head = XFILE_HEADER_LEN;
    const BytesResult c1 = bytes(data, len, head, 4);
    if (!c1.ok) return WireTableError::Truncated;
    const size_t string_count = rd_u32(c1.ptr);
    const BytesResult c2 = bytes(data, len, head + 2 * width, 4);
    if (!c2.ok) return WireTableError::Truncated;
    const size_t asset_count = rd_u32(c2.ptr);

    {
        const PointerResult p1 = pointer(data, len, head + width, format);
        if (!p1.ok) return p1.ok ? WireTableError::InvalidPointer : p1.error;
        if (!is_count_pointer(string_count, p1.p)) return WireTableError::InvalidCountPointer;
        const PointerResult p2 = pointer(data, len, head + 3 * width, format);
        if (!p2.ok) return p2.error;
        if (!is_count_pointer(asset_count, p2.p)) return WireTableError::InvalidCountPointer;
    }

    const size_t virtual_size = header->block_size[XFILE_BLOCK_VIRTUAL];
    size_t cursor = head + 4 * width;
    const size_t strings_size = string_count * width;
    if (string_count != 0 && strings_size / string_count != width) {
        return WireTableError::BlockBounds; // checked_mul overflow
    }
    if (strings_size > virtual_size) {
        return WireTableError::BlockBounds;
    }
    const BytesResult slots_bytes = bytes(data, len, cursor, strings_size);
    if (!slots_bytes.ok) return WireTableError::Truncated;
    const size_t slots = cursor;
    cursor += strings_size;
    size_t virtual_end = strings_size;
    for (size_t i = 0; i < string_count; ++i) {
        const size_t at = slots + i * width;
        const PointerResult p = pointer(data, len, at, format);
        if (!p.ok) return p.error;
        switch (p.p.kind) {
        case WirePointer::Kind::Null:
            break;
        case WirePointer::Kind::Following: {
            const uint8_t* from = data + cursor;
            const size_t remaining = len - cursor;
            const void* nul = std::memchr(from, 0, remaining);
            if (!nul) return WireTableError::UnterminatedString;
            const size_t length = static_cast<const uint8_t*>(nul) - from + 1;
            if (length > virtual_size - virtual_end) {
                return WireTableError::BlockBounds;
            }
            virtual_end += length;
            cursor += length;
            break;
        }
        case WirePointer::Kind::Offset:
            if (p.p.ptr.block == XFILE_BLOCK_VIRTUAL &&
                p.p.ptr.offset >= strings_size && p.p.ptr.offset < virtual_end) {
                break;
            }
            return WireTableError::InvalidCountPointer;
        default:
            return WireTableError::InvalidCountPointer;
        }
    }

    if (asset_count > 0) {
        virtual_end = (virtual_end + 3) & ~static_cast<size_t>(3);
    }
    const size_t string_data_start = slots + strings_size;
    const size_t string_data_len = cursor - string_data_start;
    const size_t entry_bytes = asset_count * 2 * width;
    if (asset_count != 0 && entry_bytes / asset_count != 2 * width) {
        return WireTableError::BlockBounds;
    }
    if (entry_bytes > virtual_size - virtual_end) {
        return WireTableError::BlockBounds;
    }
    const BytesResult entries_bytes = bytes(data, len, cursor, entry_bytes);
    if (!entries_bytes.ok) return WireTableError::Truncated;
    for (size_t i = 0; i < asset_count; ++i) {
        const size_t at = cursor + i * 2 * width;
        const BytesResult e = bytes(data, len, at, 4);
        if (!e.ok) return WireTableError::Truncated;
        const uint32_t raw = rd_u32(e.ptr);
        if (!is_known_asset_type(raw)) {
            return WireTableError::UnknownAssetType;
        }
        if (raw == static_cast<uint32_t>(AssetType::SndDriverGlobals)) {
            continue;
        }
        const PointerResult p = pointer(data, len, at + width, format);
        if (!p.ok) return p.error;
        if (p.p.kind == WirePointer::Kind::Offset) {
            if (p.p.ptr.offset >= header->block_size[p.p.ptr.block]) {
                return WireTableError::BlockBounds;
            }
        }
    }
    cursor += entry_bytes;

    if (asset_count == 0 && cursor != len) {
        return WireTableError::TrailingEmptyData;
    }

    WireAssetTable table;
    table.format = format;
    table.string_count = string_count;
    table.asset_count = asset_count;
    table.bodies_offset = cursor;
    table.entries = entries_bytes.ptr;
    table.entries_len = entry_bytes;
    table.string_slots = data + slots;
    table.string_slots_len = strings_size;
    table.string_data = data + string_data_start;
    table.string_data_len = string_data_len;
    return table;
}

} // namespace fastfile_iw4
