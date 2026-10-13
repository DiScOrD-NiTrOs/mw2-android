// Port of iw4L crates/movement_iw4: footstep.rs + jump.rs.
#include "movement_iw4/movement_extended.h"

#include "math_iw4/math_iw4.h"

#include <cmath>
#include <cstddef>

namespace movement_iw4 {


namespace {

constexpr int32_t LADDER_JUMP_BLOCK_MS_LOCAL = 300;

const char* SURFACE_TYPE_NAMES[31] = {
    "default", "bark", "brick", "carpet", "cloth", "concrete", "dirt", "flesh",
    "foliage", "glass", "grass", "gravel", "ice", "metal", "mud", "paper",
    "plaster", "rock", "sand", "snow", "water", "wood", "asphalt", "ceramic",
    "plastic", "rubber", "cushion", "fruit", "paintedmetal", "riotshield", "slush",
};

constexpr int32_t EV_FOOTSTEP_SPRINT = 0x6b;
constexpr int32_t EV_FOOTSTEP_RUN = 0x6c;
constexpr int32_t EV_FOOTSTEP_WALK = 0x6d;
constexpr int32_t EV_FOOTSTEP_PRONE = 0x6e;
constexpr uint32_t SURF_NOSTEPS = 0x2000;
constexpr float LADDER_CLIMB_REF = 95.25f;
constexpr float LADDER_CLIMB_BOB = 0.45f;
constexpr float LADDER_STRAFE_REF = 38.1f;
constexpr float LADDER_STRAFE_BOB = 0.35f;
constexpr float BOB_FACTOR_TABLE[6][2] = {
    {0.335f, 0.305f}, {0.25f, 0.24f}, {0.34f, 0.315f},
    {0.25f, 0.24f},   {0.36f, 0.325f}, {0.25f, 0.24f},
};
constexpr float PLAYER_MOVE_THRESHHOLD = 10.0f;
constexpr float PLAYER_SPRINT_CAMERA_BOB = 0.5f;
constexpr uint32_t EFLAGS_TURRET = 0xc00;
constexpr uint8_t ANIM_MT_IDLE = 1;
constexpr uint8_t ANIM_MT_IDLECR = 2;
constexpr uint8_t ANIM_MT_IDLEPRONE = 3;
constexpr uint8_t ANIM_MT_SPRINT = 20;
constexpr uint8_t ANIM_MT_STUMBLE_SPRINT_FORWARD = 42;
constexpr uint8_t ANIM_MT_IDLELASTSTAND = 51;
constexpr uint8_t PM_MOVE_ANIM_TABLE[32] = {
    10, 36, 4, 38, 8, 8, 8, 8, 12, 40, 6, 40, 52, 52, 52, 52,
    11, 37, 5, 39, 9, 9, 9, 9, 13, 41, 7, 41, 53, 53, 53, 53,
};

// jump.rs constants (jump port lives below!)
constexpr uint32_t BUTTON_JUMP = 0x400;
constexpr uint32_t JUMP_CLEAR_FLAGS = 0x00402000;
constexpr int32_t EV_JUMP = 0x6f;
constexpr float LADDER_JUMP_VZ_SCALE = 0.75f;
constexpr float LADDER_PUSHOFF_REFLECT = -2.0f;
constexpr float JUMP_GET_STEP_HEIGHT = 39.0f;
constexpr float JUMP_GET_STEP_SIZE = 18.0f;

uint8_t stance_from_height_target(int32_t view_height_target) {
    switch (view_height_target) {
    case 0x0b: return 1;
    case 0x28: return 2;
    case 0x16: return 3;
    default: return 0;
    }
}

uint8_t stance_idle_anim(int32_t view_height_target) {
    switch (stance_from_height_target(view_height_target)) {
    case 1: return ANIM_MT_IDLEPRONE;
    case 2: return ANIM_MT_IDLECR;
    case 3: return ANIM_MT_IDLELASTSTAND;
    default: return ANIM_MT_IDLE;
    }
}

bool is_sprinting_timestamps(const PlayerState& ps) {
    return ps.last_sprint_start != 0 && ps.last_sprint_end < ps.last_sprint_start;
}

void normalize_inplace(float v[3]) {
    const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len > 0.0f) {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
}

} // namespace

// jump.rs ------------------------------------------------------------------------

void jump_clear_state(PlayerState& ps) {
    ps.pm_flags &= !JUMP_CLEAR_FLAGS;
    ps.jump_origin_z = 0.0f;
}

std::optional<float> get_step_height(const PlayerState& ps, const float origin[3]) {
    const float ceiling = ps.jump_origin_z + JUMP_GET_STEP_HEIGHT;
    if (ceiling <= origin[2]) {
        return std::nullopt;
    }
    float step = JUMP_GET_STEP_SIZE;
    if (ceiling < origin[2] + JUMP_GET_STEP_SIZE) {
        step = ceiling - origin[2];
    }
    return step;
}

int32_t jump_ground_surface_type(uint32_t surface_flags) {
    if ((surface_flags & 0x2000) != 0) {
        return 0;
    }
    return static_cast<int32_t>(surface_type_index(surface_flags));
}

void jump_event(PlayerState& ps, uint32_t surface_flags) {
    if ((ps.pm_flags & playerstate_iw4::pm_flags::LADDER) != 0) {
        add_predictable_event(ps, EV_JUMP, 0x15);
        return;
    }
    const int32_t parm = jump_ground_surface_type(surface_flags);
    if (parm != 0) {
        add_predictable_event(ps, EV_JUMP, parm);
    }
}

void jump_start(PlayerState& ps, Pml& pml, const UserCmd& cmd, const JumpLaunchContext& context) {
    float energy = static_cast<float>(ps.gravity) * (context.jump_height + context.jump_height);
    if ((ps.pm_flags & playerstate_iw4::pm_flags::JUMPING) != 0 && ps.pm_time <= 0x708 && !context.dive) {
        energy /= context.crouch_jump_scale;
    }

    pml.walking = 0;
    pml.ground_plane = 0;
    pml.almost_ground_plane = 0;

    ps.ground_entity_num = playerstate_iw4::ENTITYNUM_NONE;
    ps.jump_origin_z = ps.origin[2];
    ps.jump_time = cmd.server_time;
    ps.velocity[2] = std::sqrt(energy);

    const uint32_t flags = ps.pm_flags;
    ps.pm_time = 0;
    ps.sprint_button_up_required = 0;

    const float spread = ps.aim_spread_scale + 64.0f;
    ps.aim_spread_scale = spread > 255.0f ? 255.0f : spread;

    ps.pm_flags = context.dive ? (flags & 0xfffffe7f/*? see note*/) | 0x00402000
                               : (flags & 0xffbffe7fu) | pm_flags::JUMPING;
    // dive mask upstream is 0xffff_fe7f
    if (context.dive) {
        ps.pm_flags = (flags & 0xfffffe7fu) | 0x00402000u;
    }
}

void push_off_ladder(PlayerState& ps, const Pml& pml, float push_vel) {
    // debug_assert(!ladder) ported as a comment: caller guarantees ladder flag set.
    ps.velocity[2] *= LADDER_JUMP_VZ_SCALE;

    float flat_forward[3] = {pml.forward[0], pml.forward[1], 0.0f};
    normalize_inplace(flat_forward);

    const float* ladder = ps.v_ladder_vec;
    const float facing_dot =
        ladder[0] * pml.forward[0] + ladder[1] * pml.forward[1] + ladder[2] * pml.forward[2];

    const float* push = flat_forward;
    float reflected[3];
    if (facing_dot < 0.0f) {
        const float along =
            flat_forward[0] * ladder[0] + flat_forward[1] * ladder[1] + flat_forward[2] * ladder[2];
        const float scale = along * LADDER_PUSHOFF_REFLECT;
        reflected[0] = flat_forward[0] + scale * ladder[0];
        reflected[1] = flat_forward[1] + scale * ladder[1];
        reflected[2] = flat_forward[2] + scale * ladder[2];
        normalize_inplace(reflected);
        push = reflected;
    }

    ps.velocity[0] = push_vel * push[0];
    ps.velocity[1] = push_vel * push[1];
    ps.pm_flags &= !playerstate_iw4::pm_flags::LADDER;
}

namespace {

JumpCheckResult evaluate_gate(uint32_t flags_in, int32_t pm_type, UserCmd& cmd,
                              const JumpCheckContext& context) {
    if ((flags_in & 0x40000) != 0 || context.time_since_jump <= 499 ||
        (flags_in & 0x400) != 0 || (flags_in & playerstate_iw4::pm_flags::MANTLE) != 0 || pm_type >= 8) {
        return JumpCheckResult{JumpCheckResult::Kind::NotEligible};
    }

    if (context.stance_surface_type != 0 || (cmd.buttons & BUTTON_JUMP) == 0) {
        return JumpCheckResult{JumpCheckResult::Kind::NotEligible};
    }

    if ((context.old_buttons & BUTTON_JUMP) != 0) {
        cmd.buttons &= ~BUTTON_JUMP;
        return JumpCheckResult{JumpCheckResult::Kind::HeldJumpCleared};
    }
    return JumpCheckResult{JumpCheckResult::Kind::Ready};
}

} // namespace

JumpCheckResult jump_check_gate(PlayerState& ps, UserCmd& cmd, const JumpCheckContext& context) {
    return evaluate_gate(ps.pm_flags, ps.pm_type, cmd, context);
}

bool stance_allows_jump(const PlayerState& ps) {
    return stance_surface_type(ps) == StanceSurface::Stand;
}

JumpCheckResult jump_check_launch(PlayerState& ps, Pml& pml, UserCmd& cmd,
                                  const JumpCheckContext& gate,
                                  const JumpLaunchContext& launch) {
    const JumpCheckResult result = jump_check_gate(ps, cmd, gate);
    if (result.kind != JumpCheckResult::Kind::Ready) {
        return result;
    }

    jump_start(ps, pml, cmd, launch);
    jump_event(ps, pml.ground_trace[4]);
    if ((ps.pm_flags & playerstate_iw4::pm_flags::LADDER) != 0) {
        push_off_ladder(ps, pml, launch.jump_ladder_push_vel);
    }

    const JumpAnimation animation =
        cmd.forwardmove >= 0 ? JumpAnimation::Forward : JumpAnimation::Backward;
    pml.record_jump_animation(static_cast<int32_t>(animation), true);
    return JumpCheckResult{JumpCheckResult::Kind::Launched, static_cast<int32_t>(animation)};
}

// footstep.rs --------------------------------------------------------------------

size_t surface_type_index(uint32_t surface_flags) {
    return (surface_flags >> 20) & 0x1f;
}

const char* surface_type_to_name(size_t index) {
    if (index >= 1 && index <= 30) {
        return SURFACE_TYPE_NAMES[index];
    }
    return "default";
}

const char* surface_type_name(uint32_t surface_flags) {
    return surface_type_to_name(surface_type_index(surface_flags));
}

constexpr uint32_t LADDER_SURFACE_TYPE_ = 13; // kept out of header name space
bool bob_cycle_wrapped(uint8_t old, uint8_t new_) {
    return static_cast<int8_t>(
               static_cast<uint8_t>(old + 0x40) ^ static_cast<uint8_t>(new_ + 0x40)) < 0;
}

float get_bob_max_speed(const PlayerState& ps, int8_t forwardmove, int8_t rightmove,
                        bool walking, bool sprinting, int32_t server_time,
                        const CmdScaleWalkContext& scales) {
    (void)server_time;
    float max_speed = static_cast<float>(ps.speed);

    constexpr float STRAFE_BLEND = 0.75f;
    constexpr float DIAG_SCALE = 0.5f;
    const float strafe = scales.player_strafe_speed_scale;
    const float back = scales.player_back_speed_scale;
    if (forwardmove == 0) {
        if (rightmove != 0) {
            max_speed *= (strafe - 1.0f) * STRAFE_BLEND + 1.0f;
        }
    } else if (rightmove == 0) {
        if (forwardmove < 0) {
            max_speed *= back;
        }
    } else {
        max_speed *= ((strafe - 1.0f) * STRAFE_BLEND + 1.0f + 1.0f) * DIAG_SCALE;
        if (forwardmove < 0) {
            max_speed = DIAG_SCALE * (back + 1.0f) * max_speed;
        }
    }
    if (walking) {
        max_speed *= 0.4f;
    } else if (sprinting) {
        max_speed *= scales.player_sprint_speed_scale;
    }
    if (ps.weapon != 0) {
        const bool lean_flag = (ps.pm_flags & playerstate_iw4::pm_flags::WALKING) != 0;
        if (scales.weapon_move_speed_scale > 0.0f && !lean_flag) {
            max_speed *= scales.weapon_move_speed_scale;
        } else if (scales.weapon_ads_move_speed_scale > 0.0f) {
            max_speed *= scales.weapon_ads_move_speed_scale;
        }
    }
    return max_speed * stance_speed_scale(ps, server_time, scales.player_last_stand_crawl_speed_scale);
}

void footsteps_bob_cycle(PlayerState& ps, int32_t msec, int8_t forwardmove, int8_t rightmove,
                         bool almost_ground_plane, int32_t server_time,
                         const CmdScaleWalkContext& scales) {
    if (ps.pm_type >= 8) {
        return;
    }
    const float xyspeed =
        std::sqrt(ps.velocity[0] * ps.velocity[0] + ps.velocity[1] * ps.velocity[1]);
    if ((ps.e_flags & EFLAGS_TURRET) != 0) {
        return;
    }
    const bool airborne = ps.ground_entity_num == playerstate_iw4::ENTITYNUM_NONE &&
                          ps.pm_type != 1 && ps.pm_type != 7 && !almost_ground_plane;
    if (airborne) {
        return;
    }
    if (xyspeed <= PLAYER_MOVE_THRESHHOLD || ps.pm_type == 1) {
        return;
    }
    if (forwardmove == 0 && rightmove == 0) {
        return;
    }

    const int stance = stance_from_height_target(ps.view_height_target);
    const bool walking = (ps.pm_flags & playerstate_iw4::pm_flags::WALKING) != 0 || ps.leanf != 0.0f;
    const bool sprinting = (ps.pm_flags & playerstate_iw4::pm_flags::SPRINTING) != 0;
    const float bob_factor =
        (stance == 0 && sprinting) ? PLAYER_SPRINT_CAMERA_BOB
                                   : BOB_FACTOR_TABLE[stance][walking ? 1 : 0];
    const float max_speed =
        get_bob_max_speed(ps, forwardmove, rightmove, walking, sprinting, server_time, scales);
    if (max_speed <= 0.0f) {
        return;
    }
    const float bobmove = (xyspeed / max_speed) * bob_factor;
    const uint8_t old = static_cast<uint8_t>(ps.bob_cycle);
    const float next = static_cast<float>(old) + static_cast<float>(msec) * bobmove;
    // roundf then i32 then u8 cast, matching upstream cast chain
    const float rounded = std::round(next);
    const uint8_t new_ = static_cast<uint8_t>(static_cast<int32_t>(rounded));
    ps.bob_cycle = static_cast<int32_t>(new_);
}

namespace {
uint8_t not_trying_to_move_anim(const PlayerState& ps) {
    switch (ps.view_height_target) {
    case 0x0b: return ANIM_MT_IDLEPRONE;
    case 0x28: return ANIM_MT_IDLECR;
    case 0x16: return ANIM_MT_IDLELASTSTAND;
    default: return ANIM_MT_IDLE;
    }
}
} // namespace

std::optional<uint8_t> footsteps_anim_move_type(const PlayerState& ps, int8_t forwardmove,
                                                int8_t rightmove, bool almost_ground_plane) {
    if (ps.pm_type >= 8) {
        return std::nullopt;
    }
    if ((ps.e_flags & EFLAGS_TURRET) != 0) {
        return stance_idle_anim(ps.view_height_target);
    }
    const bool airborne = ps.ground_entity_num == playerstate_iw4::ENTITYNUM_NONE &&
                          ps.pm_type != 1 && ps.pm_type != 7 && !almost_ground_plane;
    if (airborne) {
        return std::nullopt;
    }
    const float xyspeed =
        std::sqrt(ps.velocity[0] * ps.velocity[0] + ps.velocity[1] * ps.velocity[1]);
    const int stance = stance_from_height_target(ps.view_height_target);
    const bool stumbling = damage_window_open(ps.damage_timer, ps.damage_duration,
                                              PLAYER_DMGTIMER_STUMBLE_TIME_MS);
    if (xyspeed <= PLAYER_MOVE_THRESHHOLD || ps.pm_type == 1) {
        const bool flinching = damage_window_open(ps.damage_timer, ps.damage_duration,
                                                  PLAYER_DMGTIMER_FLINCH_TIME_MS);
        if (stance == 1) return ANIM_MT_IDLEPRONE;
        if (stance == 2) return ANIM_MT_IDLECR;
        if (stance == 3) return ANIM_MT_IDLELASTSTAND;
        if (flinching) {
            return static_cast<uint8_t>(ANIM_MT_FLINCH_FORWARD +
                                        static_cast<uint8_t>(ps.flinch_yaw_anim));
        }
        return ANIM_MT_IDLE;
    }
    if (forwardmove == 0 && rightmove == 0) {
        return not_trying_to_move_anim(ps);
    }

    const bool walking = (ps.pm_flags & playerstate_iw4::pm_flags::WALKING) != 0 || ps.leanf != 0.0f;
    const bool sprinting = (ps.pm_flags & playerstate_iw4::pm_flags::SPRINTING) != 0;
    const bool backward = (ps.pm_flags & playerstate_iw4::pm_flags::BACKWARDS_RUN) != 0;
    if (stance == 0 && !backward && sprinting) {
        return stumbling ? ANIM_MT_STUMBLE_SPRINT_FORWARD : ANIM_MT_SPRINT;
    }
    const size_t index = static_cast<size_t>(stumbling ? 1 : 0) +
                         2 * static_cast<size_t>(walking ? 1 : 0) +
                         4 * static_cast<size_t>(stance) +
                         16 * static_cast<size_t>(backward ? 1 : 0);
    return PM_MOVE_ANIM_TABLE[index];
}

bool should_make_footsteps(const PlayerState& ps) {
    switch (ps.view_height_target) {
    case 0x16:
    case 0x28:
    case 0x0b:
        return false;
    default:
        return (ps.pm_flags & playerstate_iw4::pm_flags::WALKING) == 0;
    }
}

int32_t footstep_event_type(const PlayerState& ps, uint32_t surface_flags) {
    if ((surface_flags & SURF_NOSTEPS) != 0 || surface_type_index(surface_flags) == 0) {
        return 0;
    }
    if ((ps.pm_flags & playerstate_iw4::pm_flags::PRONE) != 0) {
        return EV_FOOTSTEP_PRONE;
    }
    if ((ps.pm_flags & (pm_flags::WALKING | playerstate_iw4::pm_flags::CROUCH)) == 0 && ps.leanf == 0.0f) {
        if (is_sprinting_timestamps(ps)) {
            return EV_FOOTSTEP_SPRINT;
        }
        return EV_FOOTSTEP_RUN;
    }
    return EV_FOOTSTEP_WALK;
}

bool footstep_event(PlayerState& ps, uint8_t old, uint8_t new_, uint32_t surface_flags,
                    bool b_footstep) {
    if (!bob_cycle_wrapped(old, new_)) {
        return false;
    }
    if (ps.ground_entity_num == playerstate_iw4::ENTITYNUM_NONE) {
        return false;
    }
    if (!b_footstep) {
        return false;
    }
    const int32_t event = footstep_event_type(ps, surface_flags);
    if (event == 0) {
        return false;
    }
    add_predictable_event(ps, event, static_cast<int32_t>(surface_type_index(surface_flags)));
    return true;
}

bool ladder_footsteps(PlayerState& ps, int32_t msec, int32_t server_time) {
    if ((ps.pm_flags & playerstate_iw4::pm_flags::LADDER) == 0) {
        return false;
    }
    if (static_cast<int32_t>(static_cast<uint32_t>(server_time) -
                             static_cast<uint32_t>(ps.jump_time)) < LADDER_JUMP_BLOCK_MS_LOCAL) {
        return false;
    }

    const float vz = std::fabs(ps.velocity[2]);
    const float vxy = std::sqrt(ps.velocity[0] * ps.velocity[0] + ps.velocity[1] * ps.velocity[1]);
    const float bobmove =
        vz >= vxy ? (vz / LADDER_CLIMB_REF) * LADDER_CLIMB_BOB
                  : (vxy / LADDER_STRAFE_REF) * LADDER_STRAFE_BOB;

    const uint8_t old = static_cast<uint8_t>(ps.bob_cycle);
    const float next = static_cast<float>(old) + static_cast<float>(msec) * bobmove;
    const uint8_t new_ = static_cast<uint8_t>(static_cast<int32_t>(std::round(next)));
    ps.bob_cycle = static_cast<int32_t>(new_);

    if (!bob_cycle_wrapped(old, new_)) {
        return false;
    }
    if (ps.ground_entity_num != playerstate_iw4::ENTITYNUM_NONE ||
        (ps.pm_flags & playerstate_iw4::pm_flags::LADDER) == 0) {
        return false;
    }
    add_predictable_event(ps, EV_FOOTSTEP_RUN, 13 /* LADDER_SURFACE_TYPE */);
    return true;
}

} // namespace movement_iw4
