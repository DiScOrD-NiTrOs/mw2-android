// Port of iw4L crates/trace_iw4: trace.rs, box_trace.rs, bullet.rs, surface.rs.
#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace trace_iw4 {

struct Trace {
    float fraction = 1.0f;
    float normal[3] = {0, 0, 0};
    uint32_t surface_flags = 0;
    uint32_t contents = 0;
    uint32_t material = 0;
    int32_t hit_type = 0;
    uint16_t hit_id = 0;
    uint16_t model_index = 0;
    uint16_t part_name = 0;
    uint16_t part_group = 0;
    uint8_t allsolid = 0;
    uint8_t startsolid = 0;
    uint8_t walkable = 0;
    float endpos[3] = {0, 0, 0};
};

inline Trace make_trace() { return {}; }

constexpr int32_t HITTYPE_ENTITY = 1;
constexpr int32_t HITTYPE_GLASS = 4;
constexpr uint16_t ENTITYNUM_WORLD = 0x7fe;
constexpr uint16_t ENTITYNUM_NONE = 0x7ff;
constexpr int32_t HITTYPE_DYNENT_FIRST = 2;
constexpr int32_t HITTYPE_DYNENT_LAST = 4;

inline int32_t trace_get_glass_hit_id(int32_t hit_type, uint16_t hit_id) {
    return hit_type == HITTYPE_GLASS ? hit_id : 0;
}

inline std::pair<int32_t, uint16_t> brush_sweep_hit_kind(uint16_t glass_encoded) {
    if (glass_encoded != 0) return {HITTYPE_GLASS, glass_encoded};
    return {HITTYPE_ENTITY, ENTITYNUM_WORLD};
}

inline uint16_t trace_get_entity_hit_id(int32_t hit_type, uint16_t hit_id) {
    if (hit_type >= HITTYPE_DYNENT_FIRST && hit_type <= HITTYPE_DYNENT_LAST) return ENTITYNUM_WORLD;
    if (hit_type == HITTYPE_ENTITY) return hit_id;
    return ENTITYNUM_NONE;
}

// bullet.rs
struct BulletFireParams {
    int32_t ignore_hit_ent = 0;
    float start[3] = {0, 0, 0};
    float dir[3] = {0, 0, 0};
};

// surface.rs
inline uint32_t surface_type_from_flags(uint32_t surface_flags) {
    return (surface_flags >> 20) & 0x1fu;
}

// box_trace.rs / trace_capsule_hit
//
// This is the brush-list trace. The caller supplies the brush list via the
// template/iterable parameter; for the C++ port we expose the low-level
// trace_through_brush + capsule math and let the consumer build the brush
// iterator. To keep the surface working without an allocator, we provide a
// fixed-capacity "brush list" struct here.
//
// BrushRef matches iw4L's BrushRef<'a>.
struct BrushRef {
    const float* planes = nullptr;   // [N][4], tightly packed
    uint32_t plane_count = 0;
    uint32_t contents = 0;
    const uint32_t* plane_surface_flags = nullptr; // [N]
    uint16_t glass_encoded = 0;
};

// trace_box_into — the core sweep. Returns glass_encoded.
uint16_t trace_box_into(const BrushRef* brushes, uint32_t brush_count,
                        const float start[3], const float end[3],
                        const float mins[3], const float maxs[3],
                        uint32_t mask, Trace* best);

// Convenience wrappers matching the Rust pub API.
Trace trace_box(const BrushRef* brushes, uint32_t brush_count,
                const float start[3], const float end[3],
                const float mins[3], const float maxs[3], uint32_t mask);
Trace trace_capsule(const BrushRef* brushes, uint32_t brush_count,
                    const float start[3], const float end[3],
                    const float mins[3], const float maxs[3], uint32_t mask);
std::pair<Trace, uint16_t> trace_capsule_hit(const BrushRef* brushes, uint32_t brush_count,
                                             const float start[3], const float end[3],
                                             const float mins[3], const float maxs[3],
                                             uint32_t mask);

// GroundTraceInput — used by movement_iw4's collision backend.
struct GroundTraceInput {
    float start[3] = {0, 0, 0};
    float end[3] = {0, 0, 0};
    float mins[3] = {0, 0, 0};
    float maxs[3] = {0, 0, 0};
    uint32_t tracemask = 0;
};

} // namespace trace_iw4
