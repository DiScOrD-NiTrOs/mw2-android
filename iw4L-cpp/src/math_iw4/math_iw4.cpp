// Port of iw4L crates/math_iw4/angles.rs + mat.rs + lean.rs.
#include "math_iw4/math_iw4.h"

#include <cmath>

namespace math_iw4 {

namespace {

constexpr float RETAIL_TRACK_SNAP_EPSILON = 0.001f;
constexpr float RETAIL_DEGREES_TO_RADIANS = 0.01745329238474369f;
constexpr float RETAIL_RADIANS_TO_DEGREES = 57.295780181884766f;
constexpr float VECT_TO_ANGLES_PITCH_SCALE = -57.2957763671875f;
constexpr float VECT_TO_ANGLES_PITCH_DOWN = 270.0f;
constexpr float VECT_TO_ANGLES_PITCH_UP = 90.0f;
constexpr float AXIS_TO_ANGLES_ROLL_PITCH_NEG = -90.0f;
constexpr float AXIS_TO_ANGLES_ROLL_FLIP_POS = 180.0f;
constexpr float AXIS_TO_ANGLES_ROLL_FLIP_NEG = -180.0f;
// Retail leaves the rounding bias at zero; kept named as in upstream.
constexpr float SNAP_ANGLES_ROUND_BIAS = 0.0f;
// f32::from_bits(0x3586_37be) — transcribe the bit pattern, not a decimal guess.
constexpr float SNAP_ANGLES_EPS2 = [] {
    return std::bit_cast<float>(0x358637beu);
}();
constexpr float PITCH_FOR_YAW_VERTICAL = 270.0f;
constexpr float PITCH_FOR_YAW_ATAN_NUM = 180.0f;
constexpr float PITCH_FOR_YAW_ATAN_DEN = 3.141592741012573f;
constexpr float RETAIL_RIGHT_AXIS_SIGN = -1.0f;
constexpr float RETAIL_DEGREES_TO_TURNS = 1.0f / 360.0f;
constexpr float RETAIL_TRACK_ANGLE_LOWER = 180.0f;
constexpr float RETAIL_TRACK_ANGLE_UPPER = -180.0f;
constexpr float RETAIL_HALF_TURN = 0.5f;
constexpr float RETAIL_TURN_DEGREES = 360.0f;

inline float cosf_(float v) { return std::cos(v); }
inline float sinf_(float v) { return std::sin(v); }
inline float atan2f_(float y, float x) { return std::atan2(y, x); }
inline float atanf_(float v) { return std::atan(v); }
inline float sqrtf_(float v) { return std::sqrt(v); }
inline float roundf_(float v) { return std::round(v); }
inline float floorf_(float v) { return std::floor(v); }

} // namespace

// Rust tuple-returns: forward / right / up.
Vec3 angle_vectors(const Vec3& angles, Vec3* out_right, Vec3* out_up) {
    const float pitch = angles[0] * RETAIL_DEGREES_TO_RADIANS;
    const float yaw = angles[1] * RETAIL_DEGREES_TO_RADIANS;
    const float roll = angles[2] * RETAIL_DEGREES_TO_RADIANS;
    const float cy = cosf_(yaw);
    const float sy = sinf_(yaw);
    const float cp = cosf_(pitch);
    const float sp = sinf_(pitch);
    const float cr = cosf_(roll);
    const float sr = sinf_(roll);

    const Vec3 forward = {cp * cy, cp * sy, -sp};
    const Vec3 right = {
        -sy * cr * RETAIL_RIGHT_AXIS_SIGN - cy * sr * sp,
        cy * cr * RETAIL_RIGHT_AXIS_SIGN - sr * sp * sy,
        RETAIL_RIGHT_AXIS_SIGN * sr * cp,
    };
    const Vec3 up = {
        cy * cr * sp - -sy * sr,  // matches upstream's double-negative literal
        cr * sp * sy - sr * cy,
        cr * cp,
    };
    if (out_right) *out_right = right;
    if (out_up) *out_up = up;
    return forward;
}

Axis angles_to_axis(const Vec3& angles) {
    Vec3 right, up;
    const Vec3 forward = angle_vectors(angles, &right, &up);
    const Vec3 neg_right = {-right[0], -right[1], -right[2]};
    return {forward, neg_right, up};
}

float angle_normalize_360(float angle) {
    const float scaled = angle * RETAIL_DEGREES_TO_TURNS;
    const float turns = format_floor(scaled);
    const float normalized = (scaled - turns) * RETAIL_TURN_DEGREES;
    if (0.0f <= normalized - RETAIL_TURN_DEGREES) {
        return normalized - RETAIL_TURN_DEGREES;
    }
    return normalized;
}

float track(float current, float target, float scale, float frame) {
    const float delta = current - target;
    if (std::fabs(delta) <= RETAIL_TRACK_SNAP_EPSILON) {
        return current;
    }
    const float step = delta * scale * frame;
    if (std::fabs(delta) < std::fabs(step)) {
        return current;
    }
    return target + step;
}

float track_angle(float current, float target, float scale, float frame) {
    float wrapped_current = current;
    while (RETAIL_TRACK_ANGLE_LOWER < wrapped_current - target) {
        wrapped_current -= RETAIL_TURN_DEGREES;
    }
    while (wrapped_current - target < RETAIL_TRACK_ANGLE_UPPER) {
        wrapped_current += RETAIL_TURN_DEGREES;
    }
    const float tracked = track(wrapped_current, target, scale, frame);
    return angle_subtract(tracked, 0.0f);
}

float vec_to_yaw(float x, float y) {
    if (y == 0.0f && x == 0.0f) {
        return 0.0f;
    }
    const float degrees = atan2f_(y, x) * RETAIL_RADIANS_TO_DEGREES;
    if (0.0f <= degrees) {
        return degrees;
    }
    return degrees + RETAIL_TURN_DEGREES;
}

Vec3 vect_to_angles(const Vec3& forward) {
    const float x = forward[0], y = forward[1], z = forward[2];
    if (y == 0.0f && x == 0.0f) {
        const float pitch = 0.0f <= -z ? VECT_TO_ANGLES_PITCH_DOWN : VECT_TO_ANGLES_PITCH_UP;
        return {pitch, 0.0f, 0.0f};
    }
    const float yaw = vec_to_yaw(x, y);
    const float hypot = sqrtf_(x * x + y * y);
    const float degrees = atan2f_(z, hypot) * VECT_TO_ANGLES_PITCH_SCALE;
    const float pitch = 0.0f <= degrees ? degrees : degrees + RETAIL_TURN_DEGREES;
    return {pitch, yaw, 0.0f};
}

float axis_to_angles_roll_atan(const Vec3& v) {
    const float x = v[0], y = v[1], z = v[2];
    if (y == 0.0f && x == 0.0f) {
        return 0.0f <= -z ? VECT_TO_ANGLES_PITCH_UP : AXIS_TO_ANGLES_ROLL_PITCH_NEG;
    }
    const float hypot = sqrtf_(x * x + y * y);
    return atan2f_(z, hypot) * VECT_TO_ANGLES_PITCH_SCALE;
}

Axis axis_to_angles(const Axis& axis) {
    Vec3 angles = vect_to_angles(axis[0]);
    const float yaw_rad = -angles[1] * RETAIL_DEGREES_TO_RADIANS;
    const float cy = cosf_(yaw_rad);
    const float sy = sinf_(yaw_rad);
    const Vec3& left = axis[1];
    const float yawed_x = cy * left[0] - sy * left[1];
    const float yawed_y = left[0] * sy + cy * left[1];
    const float pitch_rad = -angles[0] * RETAIL_DEGREES_TO_RADIANS;
    const float cp = cosf_(pitch_rad);
    const float sp = sinf_(pitch_rad);
    const Vec3 unrot = {
        sp * left[2] + cp * yawed_x,
        yawed_y,
        left[2] * cp - sp * yawed_x,
    };
    const float atan = axis_to_angles_roll_atan(unrot);
    if (0.0f <= unrot[1]) {
        angles[2] = -atan;
    } else if (atan < 0.0f) {
        angles[2] = atan + AXIS_TO_ANGLES_ROLL_FLIP_POS;
    } else {
        angles[2] = atan + AXIS_TO_ANGLES_ROLL_FLIP_NEG;
    }
    return {angles[0], angles[1], angles[2]};
}

Vec3 snap_angles(const Vec3& angles) {
    Vec3 out = angles;
    for (float& a : out) {
        const float rounded = roundf_(a + SNAP_ANGLES_ROUND_BIAS);
        const float delta = rounded - a;
        if (delta * delta < SNAP_ANGLES_EPS2) {
            a = rounded;
        }
    }
    return out;
}

float pitch_for_yaw_on_normal(float yaw_degrees, const Vec3& normal) {
    const auto pairs = yaw_vectors_2d(yaw_degrees);
    const auto& forward = pairs[0];
    if (normal[2] == 0.0f) {
        return PITCH_FOR_YAW_VERTICAL;
    }
    const float t = (normal[0] * forward[0] + normal[1] * forward[1]) / normal[2];
    return atanf_(t) * PITCH_FOR_YAW_ATAN_NUM / PITCH_FOR_YAW_ATAN_DEN;
}

std::array<std::array<float, 2>, 2> yaw_vectors_2d(float yaw_degrees) {
    const float yaw = yaw_degrees * RETAIL_DEGREES_TO_RADIANS;
    const float cy = cosf_(yaw);
    const float sy = sinf_(yaw);
    return std::array<std::array<float, 2>, 2>{{ {{cy, sy}}, {{sy, -cy}} }};
}

float angle_subtract(float a, float b) {
    const float scaled = (a - b) * RETAIL_DEGREES_TO_TURNS;
    const float turns = format_floor(scaled + RETAIL_HALF_TURN);
    return (scaled - turns) * RETAIL_TURN_DEGREES;
}

// format_floor: Rust's branchy f32 floor copy from angles.rs, bit-faithful.
float format_floor(float value) {
    const uint32_t bits = std::bit_cast<uint32_t>(value);
    const uint32_t sign = bits >> 31;
    const int32_t exponent = static_cast<int32_t>((bits >> 23) & 0xff);
    const uint32_t fraction_bits = bits & 0x007fffffu;

    if (exponent == 0xff) {
        return value;
    }
    if (exponent < 127) {
        if ((bits & 0x7fffffffu) == 0) {
            return value;
        }
        return sign == 0 ? 0.0f : -1.0f;
    }

    const int32_t fractional_bits = 150 - exponent;
    if (fractional_bits <= 0) {
        return value;
    }

    const uint32_t mask = (1u << fractional_bits) - 1;
    const float truncated = std::bit_cast<float>(bits & ~mask);
    if (sign != 0 && (fraction_bits & mask) != 0) {
        return truncated - 1.0f;
    }
    return truncated;
}

Axis matrix_multiply(const Axis& a_in, const Axis& b_in) {
    const float a[9] = {a_in[0][0], a_in[0][1], a_in[0][2],
                        a_in[1][0], a_in[1][1], a_in[1][2],
                        a_in[2][0], a_in[2][1], a_in[2][2]};
    const float b[9] = {b_in[0][0], b_in[0][1], b_in[0][2],
                        b_in[1][0], b_in[1][1], b_in[1][2],
                        b_in[2][0], b_in[2][1], b_in[2][2]};
    float out[9];
    out[0] = a[2] * b[6] + a[1] * b[3] + a[0] * b[0];
    out[1] = a[2] * b[7] + b[1] * a[0] + b[4] * a[1];
    out[2] = a[2] * b[8] + b[2] * a[0] + b[5] * a[1];
    out[3] = b[6] * a[5] + a[3] * b[0] + a[4] * b[3];
    out[4] = a[5] * b[7] + a[3] * b[1] + a[4] * b[4];
    out[5] = a[5] * b[8] + a[3] * b[2] + a[4] * b[5];
    out[6] = b[6] * a[8] + a[6] * b[0] + a[7] * b[3];
    out[7] = a[8] * b[7] + a[6] * b[1] + a[7] * b[4];
    out[8] = a[8] * b[8] + a[6] * b[2] + a[7] * b[5];
    return Axis{Vec3{out[0], out[1], out[2]},
                Vec3{out[3], out[4], out[5]},
                Vec3{out[6], out[7], out[8]}};
}

Vec3 matrix_transform_vector43(const Vec3& v, const Axis& axis, const Vec3& origin) {
    const float m[9] = {axis[0][0], axis[0][1], axis[0][2],
                        axis[1][0], axis[1][1], axis[1][2],
                        axis[2][0], axis[2][1], axis[2][2]};
    return {
        m[6] * v[2] + v[0] * m[0] + m[3] * v[1] + origin[0],
        m[7] * v[2] + m[4] * v[1] + m[1] * v[0] + origin[1],
        m[8] * v[2] + m[5] * v[1] + m[2] * v[0] + origin[2],
    };
}

void matrix_multiply43(const Axis& a_axis, const Vec3& a_origin,
                       const Axis& b_axis, const Vec3& b_origin,
                       Axis& out_axis, Vec3& out_origin) {
    out_axis = matrix_multiply(a_axis, b_axis);
    const float b[9] = {b_axis[0][0], b_axis[0][1], b_axis[0][2],
                        b_axis[1][0], b_axis[1][1], b_axis[1][2],
                        b_axis[2][0], b_axis[2][1], b_axis[2][2]};
    out_origin = {
        b[6] * a_origin[2] + a_origin[0] * b[0] + a_origin[1] * b[3] + b_origin[0],
        a_origin[2] * b[7] + a_origin[0] * b[1] + a_origin[1] * b[4] + b_origin[1],
        a_origin[2] * b[8] + a_origin[0] * b[2] + a_origin[1] * b[5] + b_origin[2],
    };
}

Vec3 add_lean_to_position(const Vec3& position, float view_yaw, float lean_frac,
                          float view_roll, float lean_dist) {
    if (lean_frac == 0.0f) {
        return position;
    }
    const float lean = get_lean_fraction(lean_frac);
    Vec3 right;
    angle_vectors({0.0f, view_yaw, view_roll * lean}, &right, nullptr);
    const float scale = lean * lean_dist;
    return {
        position[0] + scale * right[0],
        position[1] + scale * right[1],
        position[2] + scale * right[2],
    };
}

} // namespace math_iw4
