// Port of iw4L crates/movement_iw4: pml.rs, friction.rs, accelerate.rs,
// is_in_air.rs, integrate.rs, pmove.rs, viewangles.rs, cmdscale.rs, stance.rs
// (surface/speed-scale part). Further files land here as they are ported.
#pragma once

#include <array>
#include <cstdint>
#include <cmath>

#include "playerstate_iw4/playerstate_iw4.h"

namespace movement_iw4 {

using playerstate_iw4::PlayerState;
using playerstate_iw4::UserCmd;

// pml.rs -------------------------------------------------------------------

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
    // Four jump-animation producer slots; empty until filled this step.
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

// accelerate.rs --------------------------------------------------------------

void accelerate(PlayerState& ps, const Pml& pml, const float wishdir[3],
                float wishspeed, float accel);

// is_in_air.rs ---------------------------------------------------------------

inline bool is_in_air(const PlayerState& ps, const Pml& pml) {
    if (ps.ground_entity_num != playerstate_iw4::ENTITYNUM_NONE) {
        return false;
    }
    if (ps.pm_type == 1 || ps.pm_type == 7) {
        return false;
    }
    return pml.almost_ground_plane == 0;
}

// integrate.rs ---------------------------------------------------------------

void predict_integrate(PlayerState& ps, const Pml& pml);

// pmove.rs -------------------------------------------------------------------

struct Pmove {
    uint32_t ps = 0;
    uint8_t cmd[0x28] = {};
    uint8_t oldcmd[0x28] = {};
    uint32_t tracemask = 0;
    int32_t numtouch = 0;
    int32_t touchents[32] = {};
    float mins[3] = {0, 0, 0};
    float maxs[3] = {0, 0, 0};
    float xyspeed = 0.0f;
    uint8_t mantle_started = 0;
    float mantle_end_pos[3] = {0, 0, 0};
    int32_t duration = 0;
    uint8_t handler = 0;
};

// viewangles.rs --------------------------------------------------------------

constexpr float SHORT2ANGLE = 0.0054931640625f;   // 360 / 65536
constexpr float ANGLE2SHORT = 65536.0f / 360.0f;

struct ViewAngleClamp {
    float pitch_up = 0.0f;
    float pitch_down = 0.0f;
    bool unclamped_pitch_bit = false;
};

void update_view_angles(PlayerState& ps, const UserCmd& cmd, ViewAngleClamp clamp);

// cmdscale.rs ----------------------------------------------------------------

struct CmdScaleWalkContext {
    float player_back_speed_scale = 0.0f;
    float player_strafe_speed_scale = 0.0f;
    float player_sprint_speed_scale = 0.0f;
    float player_last_stand_crawl_speed_scale = 0.0f;
    float weapon_move_speed_scale = 0.0f;
    float weapon_ads_move_speed_scale = 0.0f;
    bool shellshock_affects_movement = false;
};

float cmd_scale_walk(const PlayerState& ps, const UserCmd& cmd, CmdScaleWalkContext context);

// stance.rs (surface/speed-scale part) --------------------------------------

namespace view_height {
constexpr int32_t PRONE = 0x0b;
constexpr int32_t LAST_STAND = 0x16;
constexpr int32_t CROUCH = 0x28;
constexpr int32_t STAND = 0x3c;
} // namespace view_height

constexpr float STAND_MAXS_Z = 70.0f;
constexpr float CROUCH_MAXS_Z = 50.0f;
constexpr float PRONE_MAXS_Z = 30.0f;

enum class StanceSurface : uint8_t { Stand = 0, Prone = 1, Crouch = 2, LastStand = 3 };

StanceSurface stance_surface_type(const PlayerState& ps);

float stance_speed_scale(const PlayerState& ps, int32_t server_time, float last_stand_scale);

int32_t view_height_lerp_duration(int32_t lerp_target, int32_t lerp_down);

} // namespace movement_iw4
