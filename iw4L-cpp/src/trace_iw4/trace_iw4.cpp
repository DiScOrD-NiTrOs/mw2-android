// Port of iw4L crates/trace_iw4: box_trace.rs + trace.rs helpers.
#include "trace_iw4/trace_iw4.h"

#include <cmath>
#include <cstring>
#include <utility>

namespace trace_iw4 {

namespace {

constexpr float SURFACE_CLIP_EPSILON = 0.125f;
constexpr float WALKABLE_NORMAL_Z = 0.7f;

struct CapsuleSize {
    float offset[3] = {0, 0, 0};
    float radius = 0.0f;
    float offset_z = 0.0f;

    void from_bounds(const float mins[3], const float maxs[3]) {
        offset[0] = (mins[0] + maxs[0]) * 0.5f;
        offset[1] = (mins[1] + maxs[1]) * 0.5f;
        offset[2] = (mins[2] + maxs[2]) * 0.5f;
        const float size0 = maxs[0] - offset[0];
        const float size1 = maxs[1] - offset[1];
        const float size2 = maxs[2] - offset[2];
        radius = size0 <= size2 ? size0 : size2;
        offset_z = size2 - radius;
    }

    float plane_radius(const float n[3]) const {
        return radius + std::fabs(n[2]) * offset_z;
    }
};

} // namespace

uint16_t trace_box_into(const BrushRef* brushes, uint32_t brush_count,
                        const float start[3], const float end[3],
                        const float mins[3], const float maxs[3],
                        uint32_t mask, Trace* best) {
    CapsuleSize cap;
    cap.from_bounds(mins, maxs);

    float s[3] = {start[0] + cap.offset[0], start[1] + cap.offset[1], start[2] + cap.offset[2]};
    float e[3] = {end[0] + cap.offset[0], end[1] + cap.offset[1], end[2] + cap.offset[2]};
    uint16_t glass_encoded = 0;

    for (uint32_t i = 0; i < brush_count; ++i) {
        const BrushRef& brush = brushes[i];
        if ((brush.contents & mask) == 0 || brush.plane_count == 0) continue;

        const float* planes = brush.planes;
        const uint32_t* pflags = brush.plane_surface_flags;
        const uint32_t contents = brush.contents;
        const uint16_t genc = brush.glass_encoded;

        float enter_frac = 0.0f;
        float leave_frac = best->fraction;
        bool allsolid = true;
        int lead_plane = -1;
        float lead_normal[3] = {0, 0, 0};

        for (uint32_t pi = 0; pi < brush.plane_count; ++pi) {
            const float* plane = planes + pi * 4;
            const float n[3] = {plane[0], plane[1], plane[2]};
            const float dist = plane[3] + cap.plane_radius(n);
            const float d1 = n[0] * s[0] + n[1] * s[1] + n[2] * s[2] - dist;
            const float d2 = n[0] * e[0] + n[1] * e[1] + n[2] * e[2] - dist;

            if (d1 <= 0.0f) {
                if (d2 > 0.0f) {
                    const float delta = d1 - d2;
                    if (d1 > leave_frac * delta) {
                        leave_frac = d1 / delta;
                        if (leave_frac <= enter_frac) return glass_encoded;
                    }
                    allsolid = false;
                }
                continue;
            }

            const float front = SURFACE_CLIP_EPSILON - d1 < 0.0f ? SURFACE_CLIP_EPSILON : d1;
            if (front <= d2) return glass_encoded;
            if (d2 > 0.0f) allsolid = false;

            const float delta = d1 - d2;
            const float f = d1 - SURFACE_CLIP_EPSILON;
            if (f <= enter_frac * delta) {
                if (lead_plane < 0) {
                    std::memcpy(lead_normal, n, sizeof(lead_normal));
                    lead_plane = static_cast<int>(pi);
                }
            } else {
                enter_frac = f / delta;
                if (leave_frac <= enter_frac) return glass_encoded;
                std::memcpy(lead_normal, n, sizeof(lead_normal));
                lead_plane = static_cast<int>(pi);
            }
        }

        best->contents = contents;
        if (lead_plane >= 0) {
            best->fraction = enter_frac;
            std::memcpy(best->normal, lead_normal, sizeof(best->normal));
            best->surface_flags = pflags ? pflags[lead_plane] : 0;
            auto [ht, hi] = brush_sweep_hit_kind(genc);
            best->hit_type = ht;
            best->hit_id = hi;
            best->walkable = 0;
            glass_encoded = genc;
        } else {
            best->startsolid = 1;
            best->hit_type = HITTYPE_ENTITY;
            best->hit_id = ENTITYNUM_WORLD;
            if (allsolid) {
                best->allsolid = 1;
                best->fraction = 0.0f;
                best->surface_flags = 0;
            }
        }

        if (best->allsolid != 0) break;
    }

    if (best->fraction < 1.0f) {
        best->endpos[0] = s[0] + (e[0] - s[0]) * best->fraction - cap.offset[0];
        best->endpos[1] = s[1] + (e[1] - s[1]) * best->fraction - cap.offset[1];
        best->endpos[2] = s[2] + (e[2] - s[2]) * best->fraction - cap.offset[2];
        if (best->walkable == 0 && best->startsolid == 0) {
            best->walkable = best->normal[2] >= WALKABLE_NORMAL_Z ? 1 : 0;
        }
    } else {
        best->endpos[0] = e[0] - cap.offset[0];
        best->endpos[1] = e[1] - cap.offset[1];
        best->endpos[2] = e[2] - cap.offset[2];
    }
    return glass_encoded;
}

Trace trace_box(const BrushRef* brushes, uint32_t brush_count,
                const float start[3], const float end[3],
                const float mins[3], const float maxs[3], uint32_t mask) {
    Trace best = make_trace();
    best.endpos[0] = end[0]; best.endpos[1] = end[1]; best.endpos[2] = end[2];
    trace_box_into(brushes, brush_count, start, end, mins, maxs, mask, &best);
    return best;
}

Trace trace_capsule(const BrushRef* brushes, uint32_t brush_count,
                    const float start[3], const float end[3],
                    const float mins[3], const float maxs[3], uint32_t mask) {
    Trace best = make_trace();
    best.endpos[0] = end[0]; best.endpos[1] = end[1]; best.endpos[2] = end[2];
    trace_box_into(brushes, brush_count, start, end, mins, maxs, mask, &best);
    return best;
}

std::pair<Trace, uint16_t> trace_capsule_hit(const BrushRef* brushes, uint32_t brush_count,
                                             const float start[3], const float end[3],
                                             const float mins[3], const float maxs[3],
                                             uint32_t mask) {
    Trace best = make_trace();
    best.endpos[0] = end[0]; best.endpos[1] = end[1]; best.endpos[2] = end[2];
    uint16_t g = trace_box_into(brushes, brush_count, start, end, mins, maxs, mask, &best);
    return {best, g};
}

} // namespace trace_iw4
