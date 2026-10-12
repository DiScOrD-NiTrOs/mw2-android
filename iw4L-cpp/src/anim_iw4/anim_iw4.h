// Port of iw4L crates/anim_iw4 (quat.rs, channel.rs, sample.rs, compose.rs).
#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <tuple>
#include <utility>

namespace anim_iw4 {

using Quat = std::array<float, 4>;
using Vec3 = std::array<float, 3>;

inline constexpr Quat QUAT_IDENTITY = {0.0f, 0.0f, 0.0f, 1.0f};
inline constexpr Vec3 VEC3_ZERO = {0.0f, 0.0f, 0.0f};

inline Quat normalize(const Quat& q) {
    const float len = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (len > 1e-6f) return {q[0]/len, q[1]/len, q[2]/len, q[3]/len};
    return QUAT_IDENTITY;
}
inline float quat_dot(const Quat& a, const Quat& b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
}
inline Quat quat_neg(const Quat& q) { return {-q[0], -q[1], -q[2], -q[3]}; }
inline Quat quat_add_weighted(const Quat& acc, const Quat& q, float weight) {
    return {acc[0] + q[0]*weight, acc[1] + q[1]*weight,
            acc[2] + q[2]*weight, acc[3] + q[3]*weight};
}
inline Quat lerp_quat(const Quat& a, const Quat& b, float t) {
    return {a[0] + t*(b[0]-a[0]), a[1] + t*(b[1]-a[1]),
            a[2] + t*(b[2]-a[2]), a[3] + t*(b[3]-a[3])};
}
inline Vec3 lerp_vec3(const Vec3& a, const Vec3& b, float t) {
    return {a[0] + t*(b[0]-a[0]), a[1] + t*(b[1]-a[1]), a[2] + t*(b[2]-a[2])};
}
inline Vec3 vec3_add_scaled(const Vec3& a, const Vec3& b, float s) {
    return {a[0] + b[0]*s, a[1] + b[1]*s, a[2] + b[2]*s};
}
inline Quat quat_mul(const Quat& a, const Quat& b) {
    return {
        (a[2]*b[1] + b[0]*a[3] + a[0]*b[3]) - a[1]*b[2],
        b[2]*a[0] + b[1]*a[3] + (a[1]*b[3] - a[2]*b[0]),
        b[2]*a[3] + ((a[1]*b[0] + a[2]*b[3]) - a[0]*b[1]),
        ((a[3]*b[3] - a[0]*b[0]) - a[1]*b[1]) - b[2]*a[2],
    };
}

// apply_additive: returns <dest_rot', dest_trans'>.
std::pair<Quat, Vec3> apply_additive(const Quat& dest_rot, const Vec3& dest_trans,
                                     const Quat& add_rot, const Vec3& add_trans, float weight);

// channel.rs --------------------------------------------------------------

Quat half_quat(const std::array<int16_t, 2>& values);
Quat full_quat(const std::array<int16_t, 4>& values);
inline Quat quat16(const std::array<int16_t, 4>& values) { return full_quat(values); }
Vec3 quantized_trans(const Vec3& mins, const Vec3& step, const std::array<float, 3>& values);

// sample.rs ---------------------------------------------------------------

enum class FrameKind : uint8_t { Dense, Sparse };

// returns <a_index, b_index, t>. `frames` must hold `count` entries when kind is Sparse.
std::tuple<size_t, size_t, float> span(FrameKind kind, const uint16_t* frames,
                                       size_t count, float frame);

Quat sample_quat(FrameKind kind, const uint16_t* frames, const Quat* values,
                 size_t value_count, float frame);
Vec3 sample_vec3(FrameKind kind, const uint16_t* frames, const Vec3* values,
                 size_t value_count, float frame);

float time_to_frame(float time, float framerate, uint16_t numframes, bool looping);

// compose.rs --------------------------------------------------------------

struct Local {
    Quat rotation = QUAT_IDENTITY;
    bool control = false;
    Vec3 translation = VEC3_ZERO;

    static Local uncontrolled(const Quat& bind_rotation) {
        return Local{bind_rotation, false, VEC3_ZERO};
    }
    static Local identity() { return Local{QUAT_IDENTITY, false, VEC3_ZERO}; }
};

inline Vec3 compose_translation(const Vec3& bind_translation, float scale, const Vec3& anim_delta) {
    return vec3_add_scaled(bind_translation, anim_delta, scale);
}
inline Quat compose_rotation(const Quat& anim_rotation) { return anim_rotation; }

} // namespace anim_iw4
