// Port of iw4L crates/movement_iw4: pml.rs + friction.rs.
// Further movement files land here as they are ported.
#pragma once

#include <array>
#include <cstdint>
#include <cmath>

#include "playerstate_iw4/playerstate_iw4.h"

namespace movement_iw4 {

using playerstate_iw4::PlayerState;

// pml.rs -------------------------------------------------------------------

struct JumpAnimation; // defined by pmove internals as they are ported

struct Pml {
    float forward[3] = {0, 0, 0};
    float right[3] = {0, 0, 0};
    float up[3] = {0, 0, 0};
    float frametime = 0.0f;
    int32_t msec = 0;
    uint32_t walking = 0;
    uint32_t ground_plane = 0;
    uint32_t almost_ground_plane = 0;
    uint32_t ground_trace[11] = {};
    float previous_origin[3] = {0, 0, 0};
    float previous_velocity[3] = {0, 0, 0};
    int32_t holdrand = 0;
    // Four jump-animation producer slots; None until filled this step.
    struct JumpSlot {
        bool has = false;
        int32_t animation = 0;
        bool force = false;
    };
    JumpSlot jump_animations[4] = {};
    bool has_mantle_movetype = false;
    uint8_t mantle_movetype = 0;
    bool landing_animation = false;
    int32_t fall_damage = 0;

    void record_jump_animation(int32_t animation, bool force);
};

// friction.rs ---------------------------------------------------------------

void friction(PlayerState& ps, const Pml& pml);

} // namespace movement_iw4
