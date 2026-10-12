// Port of iw4L crates/fastfile_iw4/src/envelope.rs, asset_type.rs, stream.rs.
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

// stream.rs ------------------------------------------------------------------

uint32_t convert_offset_to_pointer(uint32_t offset, const uint32_t blocks[16]) {
    const uint32_t adjusted = offset - 1u; // wrapping
    const uint32_t block = blocks[adjusted >> 28];
    return block + (adjusted & 0x0fffffffu);
}

std::optional<uint32_t> convert_offset_to_alias(uint32_t offset, const uint8_t* const blocks[16]) {
    uint32_t adjusted = offset - 1u; // wrapping
    const uint32_t block = adjusted >> 28;
    if (block >= 16 || !blocks[block]) return std::nullopt;
    const size_t start = adjusted & 0x0fffffffu;
    const uint8_t* p = blocks[block];
    // The Rust version reads 4 bytes at start; a missing read is None.
    // A slice bounds check cannot be done without the block length, so we read
    // optimistically like the loader does for validated zones.
    uint32_t value;
    std::memcpy(&value, p + start, 4);
    return value;
}

std::optional<uint32_t> load_stream(uint8_t stream_index, uint8_t active_index,
                                    uint8_t* destination, size_t destination_len,
                                    const uint8_t* source, size_t source_len, uint32_t position) {
    if (stream_index == 0 || destination_len == 0) {
        return position;
    }

    if (active_index == 2) {
        std::memset(destination, 0, destination_len);
    } else {
        if (!source || source_len < destination_len) return std::nullopt;
        std::memcpy(destination, source, destination_len);
    }

    const uint32_t size = static_cast<uint32_t>(destination_len);
    return position + size; // wrapping
}

std::optional<std::pair<size_t, uint32_t>> push_stream_pos(
    size_t active_index, uint32_t position, StreamPosStack& s, size_t target_index) {
    if (active_index >= 8 || target_index >= 8 || s.depth >= 64) {
        return std::nullopt;
    }

    s.stack[s.depth].first = static_cast<uint32_t>(active_index);
    ++s.depth;

    uint32_t next_position;
    if (active_index == target_index) {
        next_position = position;
    } else {
        s.positions[active_index] = position;
        next_position = s.positions[target_index];
    }
    s.stack[s.depth - 1].second = next_position;

    return std::make_pair(target_index, next_position);
}

std::optional<std::pair<size_t, uint32_t>> pop_stream_pos(
    size_t active_index, uint32_t position, StreamPosStack& s) {
    if (active_index >= 8 || s.depth == 0) {
        return std::nullopt;
    }

    --s.depth;
    const uint32_t saved_index = s.stack[s.depth].first;
    const uint32_t saved_position = s.stack[s.depth].second;
    if (saved_index >= 8) {
        ++s.depth;
        return std::nullopt;
    }
    uint32_t out_position = position;
    if (active_index == 0) {
        out_position = saved_position;
    }
    if (active_index != saved_index) {
        s.positions[active_index] = out_position;
        out_position = s.positions[saved_index];
    }

    return std::make_pair(static_cast<size_t>(saved_index), out_position);
}

std::optional<std::tuple<uint32_t, uint32_t, uint32_t>> insert_pointer(
    size_t active_index, uint32_t position, StreamPosStack& s) {
    const auto pushed = push_stream_pos(active_index, position, s, 4);
    if (!pushed) return std::nullopt;
    const auto [insert_stream, stream_position] = *pushed;
    const uint32_t pointer_position = alloc_stream_pos(stream_position, 3);
    const uint32_t advanced_position = inc_stream_pos(pointer_position, 4);
    const auto popped = pop_stream_pos(insert_stream, advanced_position, s);
    if (!popped) return std::nullopt;
    const auto [restored_index, restored_position] = *popped;
    return std::make_tuple(pointer_position, static_cast<uint32_t>(restored_index),
                           restored_position);
}

} // namespace fastfile_iw4
