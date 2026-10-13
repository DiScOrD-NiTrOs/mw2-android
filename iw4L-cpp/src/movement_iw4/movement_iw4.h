// Port of iw4L crates/movement_iw4: pml.rs, friction.rs, accelerate.rs,
// is_in_air.rs, integrate.rs, pmove.rs, viewangles.rs, cmdscale.rs, stance.rs
// (surface/speed-scale part). Further files land here as they are ported.
#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <optional>

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

// jump.rs -------------------------------------------------------------------

enum class JumpAnimation : int32_t { Forward = 0, Backward = 1 };

struct JumpCheckContext {
    int32_t time_since_jump = 0;
    uint32_t old_buttons = 0;
    uint8_t stance_surface_type = 0;
};

struct JumpLaunchContext {
    float jump_height = 0.0f;
    bool dive = false;
    float crouch_jump_scale = 0.0f;
    float jump_ladder_push_vel = 0.0f;
};

struct JumpCheckResult {
    enum class Kind : uint8_t { NotEligible, HeldJumpCleared, Ready, Launched } kind =
        Kind::NotEligible;
    int32_t animation = 0; // JumpAnimation when kind == Launched
};

void jump_clear_state(PlayerState& ps);

std::optional<float> get_step_height(const PlayerState& ps, const float origin[3]);

int32_t jump_ground_surface_type(uint32_t surface_flags);

void jump_event(PlayerState& ps, uint32_t surface_flags);

void jump_start(PlayerState& ps, Pml& pml, const UserCmd& cmd, const JumpLaunchContext& context);

void push_off_ladder(PlayerState& ps, const Pml& pml, float push_vel);

JumpCheckResult jump_check_gate(PlayerState& ps, UserCmd& cmd, const JumpCheckContext& context);

bool stance_allows_jump(const PlayerState& ps);

JumpCheckResult jump_check_launch(PlayerState& ps, Pml& pml, UserCmd& cmd,
                                  const JumpCheckContext& gate, const JumpLaunchContext& launch);

// footstep.rs / dmgtimer.rs cross-references
size_t surface_type_index(uint32_t surface_flags);

const char* surface_type_to_name(size_t index);

const char* surface_type_name(uint32_t surface_flags);

bool bob_cycle_wrapped(uint8_t old, uint8_t new_);

float get_bob_max_speed(const PlayerState& ps, int8_t forwardmove, int8_t rightmove,
                        bool walking, bool sprinting, int32_t server_time,
                        const CmdScaleWalkContext& scales);

void footsteps_bob_cycle(PlayerState& ps, int32_t msec, int8_t forwardmove, int8_t rightmove,
                         bool almost_ground_plane, int32_t server_time,
                         const CmdScaleWalkContext& scales);

std::optional<uint8_t> footsteps_anim_move_type(const PlayerState& ps, int8_t forwardmove,
                                                int8_t rightmove, bool almost_ground_plane);

bool should_make_footsteps(const PlayerState& ps);

int32_t footstep_event_type(const PlayerState& ps, uint32_t surface_flags);

bool footstep_event(PlayerState& ps, uint8_t old, uint8_t new_, uint32_t surface_flags,
                    bool b_footstep);

bool ladder_footsteps(PlayerState& ps, int32_t msec, int32_t server_time);

float damage_scale_walk(int32_t damage_timer);

void walk_move_drop_damage_timer(PlayerState& ps, float frametime_seconds);

bool damage_window_open(int32_t damage_timer, int32_t damage_duration, int32_t window_ms);

// dmgtimer constants (shared across movement translation units)
inline constexpr float PLAYER_DMGTIMER_TIME_PER_POINT = 100.0f;
inline constexpr float PLAYER_DMGTIMER_MAX_TIME = 750.0f;
inline constexpr float PLAYER_DMGTIMER_MIN_SCALE = 0.0f;
inline constexpr int32_t PLAYER_DMGTIMER_STUMBLE_TIME_MS = 500;
inline constexpr int32_t PLAYER_DMGTIMER_FLINCH_TIME_MS = 500;
inline constexpr uint8_t ANIM_MT_FLINCH_FORWARD = 32;

// ---- Extended types used by the walk/air/ground/slide/ladder ports ---------
// These are intentionally kept in the .cpp file to avoid introducing a hard
// dependency on trace_iw4 inside this header. The public API that other movement
// code needs from these ports is re-exported there via movement_extended.h.

// ladder.rs constants (used by the ladder port when it lands)
constexpr int32_t LADDER_JUMP_BLOCK_MS = 300;
constexpr float LADDER_TRACE_DIST_AIR = 30.0f;
constexpr float LADDER_TRACE_DIST_WALK = 8.0f;
constexpr float LADDER_ATTRACT_SPEED = 50.0f;
constexpr uint32_t SURF_LADDER = 0x8u;

} // namespace movement_iw4
