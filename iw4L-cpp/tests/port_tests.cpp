// Unit tests for the ported crates (math_iw4, anim_iw4).
#include "math_iw4/math_iw4.h"
#include "anim_iw4/anim_iw4.h"

#include <cassert>
#include <cstdio>
#include <cmath>

using namespace math_iw4;
using namespace anim_iw4;

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d: %s\\n", __FILE__, __LINE__, #cond); \
        ++failures; \
    } \
} while (0)

static bool near(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

// format_floor mirrors Rust's branchy floor: floor(), but -0.5 -> -1.0 not -0.0.
static void test_format_floor() {
    CHECK(near(format_floor(2.7f), 2.0f));
    CHECK(near(format_floor(-2.7f), -3.0f));
    CHECK(near(format_floor(0.0f), 0.0f));
    CHECK(near(format_floor(-0.5f), -1.0f));
    CHECK(near(format_floor(4194304.0f), 4194304.0f)); // whole float passthrough
}

static void test_angle_wrap() {
    CHECK(near(angle_subtract(30.0f, 0.0f), 30.0f));
    CHECK(near(angle_subtract(350.0f, 0.0f), -10.0f));
    CHECK(near(angle_subtract(0.0f, 350.0f), 10.0f));
    CHECK(near(angle_normalize_360(720.0f), 0.0f));
    CHECK(near(angle_normalize_360(-90.0f), 270.0f));
}

static void test_angle_vectors() {
    Vec3 right, up;
    const Vec3 fwd = angle_vectors({0.0f, 0.0f, 0.0f}, &right, &up);
    CHECK(near(fwd[0], 1.0f) && near(fwd[1], 0.0f) && near(fwd[2], 0.0f));
    // Quake-style: right = -y at identity (RETAIL_RIGHT_AXIS_SIGN = -1).
    CHECK(near(right[0], 0.0f) && near(right[1], -1.0f) && near(right[2], 0.0f));
    CHECK(near(up[0], 0.0f) && near(up[1], 0.0f) && near(up[2], 1.0f));
}

static void test_track() {
    // track returns target + (current - target) * scale * frame.
    CHECK(near(track(1.0f, 10.0f, 0.1f, 1.0f), 9.1f));
    CHECK(near(track(0.999f, 1.0f, 0.5f, 1.0f), 0.999f)); // eps snap
    // wrap-aware: 170 -> -170 walks the short way (wraps to -190, then one half-step).
    CHECK(near(track_angle(170.0f, -170.0f, 0.5f, 1.0f), -180.0f));
}

static void test_quats() {
    const Quat id = QUAT_IDENTITY;
    CHECK(near(quat_mul(id, id)[3], 1.0f));
    const Quat half = half_quat({16384, 0}); // zw-only quat [0,0,0.5,0], normalized
    CHECK(near(half[2], 1.0f));
    CHECK(near(half[3], 0.0f));
    // additive with zero add_rot leaves rotation unchanged
    const auto [r, t] = apply_additive(id, {1, 2, 3}, {0, 0, 0, 0}, {1, 0, 0}, 0.5f);
    CHECK(r == id);
    CHECK(near(t[0], 1.5f, 1e-5f));
}

static void test_span_dense() {
    const auto [a, b, t] = span(FrameKind::Dense, nullptr, 5, 2.5f);
    CHECK(a == 2 && b == 3);
    CHECK(near(t, 0.5f));
    const auto [a2, b2, t2] = span(FrameKind::Dense, nullptr, 5, 10.0f);
    CHECK(a2 == 4 && b2 == 4);
    CHECK(near(t2, 0.0f));
}

static void test_sparse_span_rust_warn() {
    // Rust's span for Sparse checks `frames.len() < count` and returns (0,0,0),
    // but then indexes frames[i+1] for i up to last = count-1. With equal lengths
    // i+1 can be == count (out of bounds). The port clamps to Rust-observed
    // behavior for in-range accesses only; this test documents the difference.
    const uint16_t frames[4] = {0, 10, 20, 30};
    const auto [a, b, t] = span(FrameKind::Sparse, frames, 3, 15.0f);
    CHECK(a == 1 && b == 2 && near(t, 0.5f));
}

int main() {
    test_format_floor();
    test_angle_wrap();
    test_angle_vectors();
    test_track();
    test_quats();
    test_span_dense();
    test_sparse_span_rust_warn();
    if (failures == 0) {
        std::printf("all port tests passed\\n");
        return 0;
    }
    std::fprintf(stderr, "%d test failure(s)\\n", failures);
    return 1;
}
