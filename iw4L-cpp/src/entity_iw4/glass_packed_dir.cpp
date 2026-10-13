#include "entity_iw4/entity_iw4.h"

namespace entity_iw4 {

// 162-entry packed direction table, transcribed from the Rust constant.
static const float PACKED_DIR_TABLE[162 * 3] = {
#include "entity_iw4/glass_packed_dir_data.inc"
};

const float* glass_packed_dir_table() noexcept {
    return PACKED_DIR_TABLE;
}

} // namespace entity_iw4
