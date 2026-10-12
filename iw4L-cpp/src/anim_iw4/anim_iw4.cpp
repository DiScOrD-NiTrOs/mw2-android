// Port of iw4L crates/anim_iw4: quat.rs, channel.rs, sample.rs, compose.rs.
#include "anim_iw4/anim_iw4.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace anim_iw4 {

std::pair<Quat, Vec3> apply_additive(const Quat& dest_rot, const Vec3& dest_trans,
                                     const Quat& add_rot, const Vec3& add_trans, float weight) {
    const Vec3 trans = vec3_add_scaled(dest_trans, add_trans, weight);
    const float len_sq = add_rot[0]*add_rot[0] + add_rot[1]*add_rot[1]
                       + add_rot[2]*add_rot[2] + add_rot[3]*add_rot[3];
    if (len_sq == 0.0f) {
        return {dest_rot, trans};
    }
    const float len = std::sqrt(len_sq);
    const float inv = 1.0f / len;
    Quat aq = {add_rot[0]*inv, add_rot[1]*inv, add_rot[2]*inv, add_rot[3]*inv};
    const float s = weight * len;
    aq[0] *= s;
    aq[1] *= s;
    aq[2] *= s;
    aq[3] = (1.0f - s) + aq[3] * s;
    return {quat_mul(aq, dest_rot), trans};
}

Quat half_quat(const std::array<int16_t, 2>& values) {
    return normalize({0.0f, 0.0f,
                      static_cast<float>(values[0]) / 32767.0f,
                      static_cast<float>(values[1]) / 32767.0f});
}

Quat full_quat(const std::array<int16_t, 4>& values) {
    return normalize({static_cast<float>(values[0]) / 32767.0f,
                      static_cast<float>(values[1]) / 32767.0f,
                      static_cast<float>(values[2]) / 32767.0f,
                      static_cast<float>(values[3]) / 32767.0f});
}

Vec3 quantized_trans(const Vec3& mins, const Vec3& step, const std::array<float, 3>& values) {
    return {mins[0] + step[0]*values[0],
            mins[1] + step[1]*values[1],
            mins[2] + step[2]*values[2]};
}

std::tuple<size_t, size_t, float> span(FrameKind kind, const uint16_t* frames,
                                       size_t count, float frame) {
    if (count <= 1) return {0, 0, 0.0f};
    switch (kind) {
    case FrameKind::Dense: {
        frame = std::clamp(frame, 0.0f, static_cast<float>(count - 1));
        const float f = std::floor(frame);
        const size_t first = static_cast<size_t>(f);
        return {first, std::min(first + 1, count - 1), frame - f};
    }
    case FrameKind::Sparse: {
        if (count < 2) return {0, 0, 0.0f}; // frames length checked by caller contract
        if (frame <= static_cast<float>(frames[0])) return {0, 0, 0.0f};
        const size_t last = count - 1;
        if (frame >= static_cast<float>(frames[last])) return {last, last, 0.0f};
        size_t i = 0;
        while (i < last) {
            const float start = static_cast<float>(frames[i]);
            const float end = static_cast<float>(frames[i + 1]);
            if (frame <= end) {
                const float amount = end > start ? (frame - start) / (end - start) : 0.0f;
                return {i, i + 1, amount};
            }
            ++i;
        }
        return {last, last, 0.0f};
    }
    }
    return {0, 0, 0.0f};
}

Quat sample_quat(FrameKind kind, const uint16_t* frames, const Quat* values,
                 size_t value_count, float frame) {
    if (value_count == 0) return QUAT_IDENTITY;
    const auto [a, b, t] = span(kind, frames, value_count, frame);
    return lerp_quat(values[a], values[b], t);
}

Vec3 sample_vec3(FrameKind kind, const uint16_t* frames, const Vec3* values,
                 size_t value_count, float frame) {
    if (value_count == 0) return VEC3_ZERO;
    const auto [a, b, t] = span(kind, frames, value_count, frame);
    return lerp_vec3(values[a], values[b], t);
}

float time_to_frame(float time, float framerate, uint16_t numframes, bool looping) {
    if (framerate <= 0.0f) return 0.0f;
    const float raw = time * framerate;
    if (looping && numframes > 0) {
        const float n = static_cast<float>(numframes);
        return raw - n * std::floor(raw / n);
    }
    return std::clamp(raw, 0.0f, static_cast<float>(numframes));
}

} // namespace anim_iw4
