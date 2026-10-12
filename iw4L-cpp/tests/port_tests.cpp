// Unit tests for the ported crates (math_iw4, anim_iw4, playerstate, movement, fastfile).
#include "math_iw4/math_iw4.h"
#include "anim_iw4/anim_iw4.h"
#include "playerstate_iw4/playerstate_iw4.h"
#include "movement_iw4/movement_iw4.h"
#include "fastfile_iw4/fastfile_iw4.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>

using namespace math_iw4;
using namespace anim_iw4;
using namespace playerstate_iw4;
using namespace movement_iw4;
using namespace fastfile_iw4;

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

static void test_playerstate() {
    const PlayerState ps = PlayerState::zero();
    CHECK(ps.viewlocked_ent_num == ENTITYNUM_NONE);
    CHECK(ps.hold_breath_scale == 1.0f);
    CHECK(breath_hold_time_ms(ps) == BREATH_HOLD_TIME_MS);
    PlayerState perk = PlayerState::zero();
    perk.perks[0] = PERK_HOLDBREATH;
    CHECK(breath_hold_time_ms(perk) == BREATH_HOLD_TIME_MS + PERK_EXTRA_BREATH_MS);
    CHECK(get_viewmodel_weapon_index(perk) == 0);
    perk.weapon = 42;
    CHECK(get_viewmodel_weapon_index(perk) == 42);
}

static void test_friction() {
    PlayerState ps = PlayerState::zero();
    ps.pm_type = 0;
    ps.velocity[0] = 300.0f;
    Pml pml = {};
    pml.walking = 1;
    pml.frametime = 1.0f / 60.0f;
    pml.ground_trace[4] = 1; // SURF_NODAMAGE; bit 0x2 (SURF_SLICK) clear so friction applies
    friction(ps, pml);
    CHECK(ps.velocity[0] < 300.0f && ps.velocity[0] > 0.0f);
    // Below the kill speed the velocity zeroes out.
    PlayerState slow = PlayerState::zero();
    slow.velocity[0] = 0.5f;
    friction(slow, pml);
    CHECK(slow.velocity[0] == 0.0f);
}

static void test_fastfile_header() {
    uint8_t header[FILE_PREAMBLE_LEN];
    std::memcpy(header, MAGIC_SIGNED, 8);
    header[8] = 0x14; header[9] = 0x01; header[10] = 0; header[11] = 0; // 0x114 LE
    header[12] = 7;
    header[13] = 0x78; header[14] = 0x56; header[15] = 0x34; header[16] = 0x12;
    header[17] = 0xaa; header[18] = 0xbb; header[19] = 0xcc; header[20] = 0xdd;
    const FileHeaderResult result = parse_file_header(header, sizeof(header));
    CHECK(std::holds_alternative<FileHeader>(result));
    const FileHeader& h = std::get<FileHeader>(result);
    CHECK(h.signing == Signing::Signed);
    CHECK(h.version == ZONE_VERSION_PC);
    CHECK(h.flag == 7);
    CHECK(h.stamp[0] == 0x12345678u);
    CHECK(h.stamp[1] == 0xddccbbaau);
    // Bad magic
    uint8_t bad[FILE_PREAMBLE_LEN] = {};
    std::memcpy(bad, "IWffX100", 8);
    const FileHeaderResult r2 = parse_file_header(bad, sizeof(bad));
    CHECK(std::holds_alternative<FileHeaderErrorBadMagic>(r2));
    // Short buffer
    const FileHeaderResult r3 = parse_file_header(header, 5);
    CHECK(std::holds_alternative<FileHeaderErrorTooShort>(r3));
}

int main() {
    test_format_floor();
    test_angle_wrap();
    test_angle_vectors();
    test_track();
    test_quats();
    test_span_dense();
    test_sparse_span_rust_warn();
    test_playerstate();
    test_friction();
    test_fastfile_header();
    if (failures == 0) {
        std::printf("all port tests passed\\n");
        return 0;
    }
    std::fprintf(stderr, "%d test failure(s)\\n", failures);
    return 1;
}
