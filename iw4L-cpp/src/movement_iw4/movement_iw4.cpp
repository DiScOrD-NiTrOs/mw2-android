// Port of iw4L crates/movement_iw4: pml.rs (impl) + friction.rs.
#include "movement_iw4/movement_iw4.h"

#include <cstdlib>
#include <cstdio>

namespace movement_iw4 {

void Pml::record_jump_animation(int32_t animation, bool force) {
    for (JumpSlot& slot : jump_animations) {
        if (!slot.has) {
            slot.has = true;
            slot.animation = animation;
            slot.force = force;
            return;
        }
    }
    // Rust: expect("at most four jump animation producers per movement step")
    std::fprintf(stderr, "panic: at most four jump animation producers per movement step\n");
    std::abort();
}

namespace {

float velocity_speed(const float velocity[3]) {
    const float speed_squared =
        velocity[0] * velocity[0] + velocity[1] * velocity[1] + velocity[2] * velocity[2];
    return std::sqrt(speed_squared);
}

bool stop_slow_velocity(float velocity[3], float speed) {
    if (speed < 1.0f) {
        velocity[0] = velocity[1] = velocity[2] = 0.0f;
        return true;
    }
    return false;
}

// Retail constant transcribed at full precision (the exact f32 from the
// retail image); do not shorten.
constexpr float RETAIL_JUMP_SCALE_STEP = 0.0005882352706976235f;

float walking_scale(PlayerState& ps) {
    if (ps.pm_time < 0x709) {
        if ((ps.pm_flags & 0x400000) == 0) {
            if (ps.pm_time > 0x6a3) {
                return 2.5f;
            }
            return static_cast<float>(ps.pm_time) * 1.5f * RETAIL_JUMP_SCALE_STEP + 1.0f;
        }
    } else {
        ps.pm_flags &= 0xffbfdfffu;
        ps.jump_origin_z = 0.0f;
    }
    return 1.0f;
}

} // namespace

void friction(PlayerState& ps, const Pml& pml) {
    float velocity[3] = {ps.velocity[0], ps.velocity[1], ps.velocity[2]};
    const float speed = velocity_speed(velocity);
    if (stop_slow_velocity(velocity, speed)) {
        ps.velocity[0] = velocity[0];
        ps.velocity[1] = velocity[1];
        ps.velocity[2] = velocity[2];
        return;
    }
    ps.velocity[0] = velocity[0];
    ps.velocity[1] = velocity[1];
    ps.velocity[2] = velocity[2];

    const uint32_t flags = ps.pm_flags;
    float drop = 0.0f;
    if ((flags & playerstate_iw4::pm_flags::MELEE_CHARGE) == 0) {
        if (pml.walking != 0 && (pml.ground_trace[4] & 2) == 0 && (flags & 0x100) == 0) {
            float control = speed;
            if (speed < 100.0f) {
                control = 100.0f;
            }

            if ((flags & playerstate_iw4::pm_flags::TIME_HARDLANDING) != 0) {
                control *= 0.3f;
            } else if ((flags & playerstate_iw4::pm_flags::JUMPING) != 0) {
                control *= walking_scale(ps);
            }

            drop = control * 5.5f;
            drop = drop * pml.frametime + 0.0f;
        }
    } else {
        drop = (speed / (static_cast<float>(ps.melee_charge_time) * 0.001f)) * pml.frametime;
    }

    if (ps.pm_type == 5) {
        drop = speed * 5.0f * pml.frametime + drop;
    }

    float new_speed = speed - drop;
    if (new_speed < 0.0f) {
        new_speed = 0.0f;
    }
    const float scale = new_speed / speed;
    ps.velocity[0] = scale * ps.velocity[0];
    ps.velocity[1] = ps.velocity[1] * scale;
    ps.velocity[2] = scale * ps.velocity[2];
}

} // namespace movement_iw4
