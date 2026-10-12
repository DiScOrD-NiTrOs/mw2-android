// Port of iw4L crates/math_iw4 (angles.rs, lean.rs, mat.rs, vec.rs).
// Retail constants are transcribed at full precision; do not "clean" them.
#pragma once

#include <array>
#include <cstdint>
#include <cmath>

namespace math_iw4 {

using Vec3 = std::array<float, 3>;
using Axis = std::array<Vec3, 3>;

// angles.rs ---------------------------------------------------------------

// forward is the return value; right/up optional outputs as in the Rust tuple.
Vec3 angle_vectors(const Vec3& angles, Vec3* out_right = nullptr, Vec3* out_up = nullptr);

// returns {forward2, right2}
std::array<std::array<float, 2>, 2> yaw_vectors_2d(float yaw_degrees);

Axis angles_to_axis(const Vec3& angles);

float angle_normalize_360(float angle);

float track(float current, float target, float scale, float frame);

float track_angle(float current, float target, float scale, float frame);

float vec_to_yaw(float x, float y);

Vec3 vect_to_angles(const Vec3& forward);

Axis axis_to_angles(const Axis& axis);

Vec3 snap_angles(const Vec3& angles);

float pitch_for_yaw_on_normal(float yaw_degrees, const Vec3& normal);

float angle_subtract(float a, float b);

// lives in angles.rs; exported from lib.rs
float format_floor(float value);

// mat.rs ------------------------------------------------------------------

Axis matrix_multiply(const Axis& a, const Axis& b);

Vec3 matrix_transform_vector43(const Vec3& v, const Axis& axis, const Vec3& origin);

void matrix_multiply43(const Axis& a_axis, const Vec3& a_origin,
                       const Axis& b_axis, const Vec3& b_origin,
                       Axis& out_axis, Vec3& out_origin);

// vec.rs / lean.rs --------------------------------------------------------

inline float vec3_length(const Vec3& v) {
    const float x = v[0], y = v[1], z = v[2];
    return std::sqrt(z * z + x * x + y * y);
}

inline float get_lean_fraction(float x) { return (2.0f - std::fabs(x)) * x; }

Vec3 add_lean_to_position(const Vec3& position, float view_yaw, float lean_frac,
                          float view_roll, float lean_dist);

} // namespace math_iw4
