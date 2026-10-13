// Port of iw4L crates/movement_iw4: ground.rs, walk.rs, air.rs, slide.rs (part),
// check_prone.rs (part), crash.rs (part). Plus the move-helper helpers they share.
#include "movement_iw4/movement_extended.h"

#include "math_iw4/math_iw4.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <functional>

namespace movement_iw4 {

using namespace playerstate_iw4;
using std::abort;

// Forward declarations for the functions defined later in this TU.
void crash_land(PlayerState& ps, Pml& pml);

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

namespace {

float normalize3(float v[3]) {
    const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    const float div = len <= 0.0f ? 1.0f : len;
    const float s = 1.0f / div;
    v[0] *= s; v[1] *= s; v[2] *= s;
    return len;
}

void clip_velocity(const float input[3], const float normal[3], float output[3]) {
    const float dot = input[0] * normal[0] + input[1] * normal[1] + input[2] * normal[2];
    const float backoff = -(dot - std::fabs(dot) * 0.001f);
    output[0] = input[0] + backoff * normal[0];
    output[1] = input[1] + backoff * normal[1];
    output[2] = input[2] + backoff * normal[2];
}

[[noreturn]] void panic_move(const char* msg) {
    std::fprintf(stderr, "panic: %s\n", msg);
    std::abort();
}

} // namespace

// ---------------------------------------------------------------------------
// ground.rs
// ---------------------------------------------------------------------------

namespace {
constexpr float STARTSOLID_RETRY_EPSILON = 0.001f;

void clear_ground_state(PlayerState& ps, Pml& pml) {
    ps.ground_entity_num = ENTITYNUM_NONE;
    pml.ground_plane = 0;
    pml.almost_ground_plane = 0;
    pml.walking = 0;
}

void copy_ground_trace(Pml& pml, const trace_iw4::Trace& trace) {
    pml.ground_trace[0] = std::bit_cast<uint32_t>(trace.fraction);
    pml.ground_trace[1] = std::bit_cast<uint32_t>(trace.normal[0]);
    pml.ground_trace[2] = std::bit_cast<uint32_t>(trace.normal[1]);
    pml.ground_trace[3] = std::bit_cast<uint32_t>(trace.normal[2]);
    pml.ground_trace[4] = trace.surface_flags;
    pml.ground_trace[5] = trace.contents;
    pml.ground_trace[6] = trace.material;
    pml.ground_trace[7] = static_cast<uint32_t>(trace.hit_type);
    pml.ground_trace[8] = static_cast<uint32_t>(trace.hit_id) |
                           (static_cast<uint32_t>(trace.model_index) << 16);
    pml.ground_trace[9] = static_cast<uint32_t>(trace.part_name) |
                           (static_cast<uint32_t>(trace.part_group) << 16);
    pml.ground_trace[10] =
        static_cast<uint32_t>(trace.allsolid) |
        (static_cast<uint32_t>(trace.startsolid) << 8) |
        (static_cast<uint32_t>(trace.walkable) << 16);
}

int32_t trace_entity_id(const trace_iw4::Trace& trace) {
    return static_cast<int32_t>(trace_iw4::trace_get_entity_hit_id(trace.hit_type, trace.hit_id));
}

JumpAnimation jump_animation(int8_t forwardmove) {
    return forwardmove < 0 ? JumpAnimation::Backward : JumpAnimation::Forward;
}
} // namespace

template <typename C>
void complete_ground_trace(PlayerState& ps, Pml& pml, const MoveBounds& bounds,
                           int8_t forwardmove, const C& collision) {
    const float origin[3] = {ps.origin[0], ps.origin[1], ps.origin[2]};
    const bool mounted = (ps.e_flags & 0xc00u) != 0;

    trace_iw4::GroundTraceInput input;
    std::memcpy(input.start, origin, sizeof(input.start));
    input.start[2] += mounted ? 0.0f : 0.25f;
    std::memcpy(input.end, origin, sizeof(input.end));
    input.end[2] -= mounted ? 1.0f : 0.25f;
    std::memcpy(input.mins, bounds.mins, sizeof(input.mins));
    std::memcpy(input.maxs, bounds.maxs, sizeof(input.maxs));
    input.tracemask = bounds.tracemask;

    trace_iw4::Trace trace = collision.trace(input);
    copy_ground_trace(pml, trace);

    if (trace.allsolid != 0) {
        auto corrected = collision.correct_solid(origin, bounds.mins, bounds.maxs, bounds.tracemask);
        if (!corrected) {
            clear_ground_state(ps, pml);
            jump_clear_state(ps);
            return;
        }
        std::memcpy(ps.origin, corrected->origin, sizeof(ps.origin));
        trace = corrected->trace;
        copy_ground_trace(pml, trace);
    }

    if (trace.startsolid != 0) {
        trace_iw4::GroundTraceInput retry = input;
        retry.start[2] = ps.origin[2] - STARTSOLID_RETRY_EPSILON;
        retry.end[2] = ps.origin[2] - (mounted ? 1.0f : 0.25f);
        trace = collision.trace(retry);
        if (trace.startsolid != 0) {
            clear_ground_state(ps, pml);
            return;
        }
        copy_ground_trace(pml, trace);
    }

    if (trace.fraction == 1.0f) {
        const bool was_grounded = ps.ground_entity_num != ENTITYNUM_NONE;
        const float depth = was_grounded ? 64.0f : 1.0f;
        trace_iw4::GroundTraceInput below = input;
        std::memcpy(below.start, ps.origin, sizeof(below.start));
        below.end[0] = ps.origin[0];
        below.end[1] = ps.origin[1];
        below.end[2] = ps.origin[2] - depth;
        trace_iw4::Trace below_trace = collision.trace(below);
        if (was_grounded && below_trace.fraction == 1.0f) {
            pml.record_jump_animation(static_cast<int32_t>(jump_animation(forwardmove)), true);
        }
        clear_ground_state(ps, pml);
        pml.almost_ground_plane =
            static_cast<uint32_t>(was_grounded
                                   ? below_trace.fraction < 1.0f / 64.0f
                                   : below_trace.fraction != 1.0f);
        return;
    }

    const float normal[3] = {
        std::bit_cast<float>(pml.ground_trace[1]),
        std::bit_cast<float>(pml.ground_trace[2]),
        std::bit_cast<float>(pml.ground_trace[3]),
    };
    const float velocity_dot_normal =
        ps.velocity[2] * normal[2] + normal[0] * ps.velocity[0] + normal[1] * ps.velocity[1];
    if ((ps.pm_flags & 8u) == 0 && ps.velocity[2] > 0.0f && 10.0f < velocity_dot_normal) {
        pml.record_jump_animation(static_cast<int32_t>(jump_animation(forwardmove)), false);
        clear_ground_state(ps, pml);
        return;
    }

    if (trace.walkable != 0 && trace.contents != 0x02000000u) {
        pml.ground_plane = 1;
        pml.almost_ground_plane = 1;
        pml.walking = 1;
        if (ps.ground_entity_num == ENTITYNUM_NONE) {
            crash_land(ps, pml);
        }
        const int32_t entity = trace_entity_id(trace);
        ps.ground_entity_num = entity;
        collision.touch_entity(entity);
    } else {
        ps.ground_entity_num = ENTITYNUM_NONE;
        pml.ground_plane = 1;
        pml.almost_ground_plane = 1;
        pml.walking = 0;
        jump_clear_state(ps);
    }
}

// ---------------------------------------------------------------------------
// check_prone.rs (partial)
// ---------------------------------------------------------------------------

namespace {
constexpr float PRONE_CHECK_HEIGHT = 30.0f;
constexpr float PRONE_FEET_DIST = 50.0f;
constexpr uint32_t PRONE_TRACE_MASK = 0x00810011u;
constexpr float PRONE_FIRST_LIFT = 10.0f;
constexpr float PRONE_YAW_FLIP = 180.0f;

bool prone_special_air_ok(const PlayerState& ps) {
    const uint32_t flags = ps.pm_flags;
    if ((flags & 0x800u) != 0) return (flags & 0xffffff00u) != 0;
    return (flags & 0x400000u) != 0 && ps.pm_time == 0;
}
} // namespace

template <typename C>
bool player_prone_allowed(const PlayerState& ps, const C& collision, float f_size,
                          bool weapon_blocks_prone) {
    if (weapon_blocks_prone) return false;
    if ((ps.pm_flags & pm_flags::PRONE) != 0) return true;
    if (ps.ground_entity_num == ENTITYNUM_NONE && !prone_special_air_ok(ps)) return false;

    trace_iw4::GroundTraceInput hull_input;
    std::memcpy(hull_input.start, ps.origin, sizeof(hull_input.start));
    hull_input.start[2] += PRONE_FIRST_LIFT;
    std::memcpy(hull_input.end, ps.origin, sizeof(hull_input.end));
    hull_input.mins[0] = -f_size; hull_input.mins[1] = -f_size; hull_input.mins[2] = 0.0f;
    hull_input.maxs[0] = f_size;  hull_input.maxs[1] = f_size;  hull_input.maxs[2] = PRONE_CHECK_HEIGHT;
    hull_input.tracemask = PRONE_TRACE_MASK;

    trace_iw4::Trace hull = collision.trace(hull_input);
    if (hull.allsolid != 0) return false;
    if (ps.ground_entity_num == ENTITYNUM_NONE) return false;
    if (!((ps.pm_flags & pm_flags::WALKING) != 0 || ps.leanf != 0.0f)) return false;
    return true;
}

// ---------------------------------------------------------------------------
// crash.rs
// ---------------------------------------------------------------------------

namespace {
constexpr float FALL_DAMAGE_MIN_HEIGHT_IN = 128.0f;
constexpr float FALL_DAMAGE_MAX_HEIGHT_IN = 300.0f;
constexpr float FALL_LIGHT_IN = 4.0f;
constexpr float FALL_MEDIUM_IN = 8.0f;
constexpr float FALL_HARD_IN = 12.0f;
constexpr float HARD_LAND_VEL_SCALE = 0.67f;
constexpr uint32_t SURF_NODAMAGE = 0x1u;
constexpr uint32_t SURF_SLICK = 0x2u;
constexpr uint32_t SURF_SOFT_LANDING = 0x1000u;
constexpr int32_t PM_TYPE_DEAD = 8;
constexpr float HALF = 0.5f;
constexpr float FOUR = 4.0f;
constexpr float TWO = 2.0f;
constexpr float NEG_ONE = -1.0f;

std::optional<float> crash_land_fall_height(const PlayerState& ps, const Pml& pml) {
    if (ps.gravity == 0) return std::nullopt;
    const float dist = pml.previous_origin[2] - ps.origin[2];
    const float vel = pml.previous_velocity[2];
    const float acc = -static_cast<float>(ps.gravity);
    const float a = acc * HALF;
    const float den = vel * vel - FOUR * a * dist;
    if (den < 0.0f) return std::nullopt;
    const float two_a = a * TWO;
    if (two_a == 0.0f) return std::nullopt;
    const float t = (-vel - std::sqrt(den)) / two_a;
    const float land_vel = (t * acc + vel) * NEG_ONE;
    return (land_vel * land_vel) / (static_cast<float>(ps.gravity) * TWO);
}

int32_t fall_damage(float fall_height, bool soft) {
    const float max = soft ? 10.0f : 100.0f;
    if (fall_height <= FALL_DAMAGE_MIN_HEIGHT_IN) return 0;
    if (fall_height >= FALL_DAMAGE_MAX_HEIGHT_IN) return static_cast<int32_t>(max);
    return static_cast<int32_t>(
        (fall_height - FALL_DAMAGE_MIN_HEIGHT_IN) /
            (FALL_DAMAGE_MAX_HEIGHT_IN - FALL_DAMAGE_MIN_HEIGHT_IN) * max);
}
} // namespace

void crash_land(PlayerState& ps, Pml& pml) {
    const auto fall_height = crash_land_fall_height(ps, pml);
    if (!fall_height) return;

    pml.landing_animation = *fall_height > FALL_HARD_IN;
    const uint32_t surface_flags = pml.ground_trace[4];
    const int32_t surface = jump_ground_surface_type(surface_flags);
    int32_t damage = 0;
    if (ps.pm_type < PM_TYPE_DEAD && (surface_flags & SURF_NODAMAGE) == 0) {
        damage = fall_damage(*fall_height, (surface_flags & SURF_SOFT_LANDING) != 0);
    }
    if (damage > 0) {
        pml.fall_damage = damage;
        float scale = HARD_LAND_VEL_SCALE;
        if (damage < 100 && (surface_flags & SURF_SLICK) == 0) {
            const int32_t time = std::min(damage * 35 + 500, 2000);
            ps.pm_flags |= pm_flags::TIME_HARDLANDING;
            ps.pm_time = time;
            if (time >= 1500) scale = 0.2f;
            else scale = 0.5f - static_cast<float>(time - 500) / 1000.0f * 0.3f;
        }
        ps.velocity[0] *= scale;
        ps.velocity[1] *= scale;
        ps.velocity[2] *= scale;

        const uint8_t soft = (surface_flags & SURF_SOFT_LANDING) != 0 ? 0x80u : 0u;
        add_predictable_event(ps, 0x8f + surface, damage | soft);
        return;
    }

    if (*fall_height <= FALL_LIGHT_IN) return;
    if (*fall_height < FALL_MEDIUM_IN) {
        if (surface != 0) add_predictable_event(ps, 0x6d, surface);
        return;
    }
    if (*fall_height < FALL_HARD_IN) {
        if (surface != 0) add_predictable_event(ps, 0x6c, surface);
        return;
    }
    ps.velocity[0] *= HARD_LAND_VEL_SCALE;
    ps.velocity[1] *= HARD_LAND_VEL_SCALE;
    ps.velocity[2] *= HARD_LAND_VEL_SCALE;
    if (surface != 0) add_predictable_event(ps, 0x70 + surface, 0);
}

// ---------------------------------------------------------------------------
// walk.rs
// ---------------------------------------------------------------------------

namespace {
constexpr float ACCEL_PRONE = 19.0f;
constexpr float ACCEL_CROUCH = 12.0f;
constexpr float ACCEL_STAND = 9.0f;
constexpr float ACCEL_SLICK = 1.0f;
constexpr float SLOW_WALK_SCALE = 0.25f;

float walk_accel_scale(const PlayerState& ps, const Pml& pml) {
    const bool slick = (pml.ground_trace[4] & 2u) != 0 || (ps.pm_flags & 0x100u) != 0;
    float accel = slick ? ACCEL_SLICK
                 : ([] (StanceSurface s) {
                       switch (s) {
                       case StanceSurface::Prone:
                       case StanceSurface::LastStand: return ACCEL_PRONE;
                       case StanceSurface::Crouch:   return ACCEL_CROUCH;
                       case StanceSurface::Stand:    return ACCEL_STAND;
                       }
                       panic_move("walk_accel_scale: bad stance");
                   }(stance_surface_type(ps)));
    if ((ps.pm_flags & pm_flags::TIME_HARDLANDING) != 0) accel *= SLOW_WALK_SCALE;
    return accel;
}

void prone_velocity_scale(PlayerState& ps) {
    float scale = 1.0f;
    if (ps.pm_time < 0x709) {
        if (ps.pm_time == 0) {
            if (ps.jump_origin_z <= ps.origin[2]) {
                ps.pm_time = 0x4b0;
                scale = 0.5f;
            } else {
                ps.pm_time = 0x708;
                scale = 0.65f;
            }
        }
    } else {
        ps.pm_flags &= 0xffbfdfffu;
        ps.jump_origin_z = 0.0f;
        scale = 0.65f;
    }
    if ((ps.pm_flags & 0x400000u) == 0) {
        ps.velocity[0] *= scale;
        ps.velocity[1] *= scale;
        ps.velocity[2] *= scale;
    }
}

void clip_to_ground_plane(float vector[3], const uint32_t normal_bits[3]) {
    float normal[3] = {
        std::bit_cast<float>(normal_bits[0]),
        std::bit_cast<float>(normal_bits[1]),
        std::bit_cast<float>(normal_bits[2]),
    };
    project_velocity(vector, normal);
}
} // namespace

template <typename C>
void walk_move(PlayerState& ps, Pml& pml, UserCmd& cmd, const WalkMoveContext& context,
               const MoveBounds& bounds, const C& collision) {
    if ((ps.pm_flags & pm_flags::JUMPING) != 0) {
        prone_velocity_scale(ps);
    }

    JumpCheckContext gate;
    gate.time_since_jump = cmd.server_time - ps.jump_time;
    if (gate.time_since_jump < 0) gate.time_since_jump += 0x100000000ULL;
    gate.old_buttons = context.old_buttons;
    gate.stance_surface_type = static_cast<uint8_t>(stance_surface_type(ps));

    const JumpCheckResult gate_result = jump_check_gate(ps, cmd, gate);
    if (gate_result.kind == JumpCheckResult::Kind::Launched) {
        air_move(ps, pml, cmd, context.air, bounds, collision);
        return;
    }

    friction(ps, pml);

    float command_scale = cmd_scale_walk(ps, cmd, context.cmd_scale) *
                          damage_scale_walk(ps.damage_timer);
    walk_move_drop_damage_timer(ps, pml.frametime);

    float forward[3] = {pml.forward[0], pml.forward[1], 0.0f};
    float right[3]   = {pml.right[0],   pml.right[1],   0.0f};
    normalize3(forward);
    normalize3(right);

    float wishdir[3] = {
        cmd.rightmove * right[0] + cmd.forwardmove * forward[0],
        cmd.rightmove * right[1] + cmd.forwardmove * forward[1],
        cmd.rightmove * right[2] + cmd.forwardmove * forward[2],
    };
    float wishspeed = normalize3(wishdir);
    const uint32_t* gt = pml.ground_trace;
    clip_to_ground_plane(wishdir, &gt[1]);

    accelerate(ps, pml, wishdir, wishspeed * context.weapon_move_scale * command_scale,
               walk_accel_scale(ps, pml));

    if ((pml.ground_trace[4] & 2u) != 0 || (ps.pm_flags & 0x100u) != 0) {
        ps.velocity[2] -= static_cast<float>(ps.gravity) * pml.frametime;
    }

    clip_to_ground_plane(ps.velocity, &gt[1]);

    if (ps.velocity[0] != 0.0f || ps.velocity[1] != 0.0f) {
        step_slide_move(ps, pml,
                        trace_iw4::GroundTraceInput{},
                        bounds.mins, bounds.maxs, bounds.tracemask, std::nullopt);
    }
}

// ---------------------------------------------------------------------------
// air.rs
// ---------------------------------------------------------------------------

namespace {
float cmd_scale_air(const PlayerState& ps, const UserCmd& cmd, float spectate_speed_scale) {
    const float forward = cmd.forwardmove;
    const float right   = cmd.rightmove;
    const float magnitude = std::sqrt(forward * forward + right * right);
    const float forward_abs = std::fabs(forward);
    const float right_abs   = std::fabs(right);
    const float largest = forward_abs > right_abs ? forward_abs : right_abs;
    if (largest == 0.0f) return 0.0f;

    float scale = (ps.speed * largest) / (magnitude * 127.0f);
    if ((ps.pm_flags & pm_flags::WALKING) != 0 || ps.leanf != 0.0f) scale *= 0.4f;
    switch (ps.pm_type) {
    case 2: scale *= 3.0f; break;
    case 3: scale *= 6.0f; break;
    case 5: scale *= spectate_speed_scale; break;
    default: break;
    }
    return scale;
}

float adjusted_gravity(const PlayerState& ps, const AirMoveContext& context) {
    if (((ps.pm_flags & 0x800u) != 0 && (ps.pm_flags & 0xffffff00u) != 0) ||
        ((ps.pm_flags & 0x400000u) != 0 && ps.pm_time == 0)) {
        return std::floor(static_cast<float>(ps.gravity) * context.shellshock_gravity_scale +
                          context.shellshock_gravity_bias + 0.5f);
    }
    return static_cast<float>(ps.gravity);
}
} // namespace

template <typename C>
void air_move(PlayerState& ps, const Pml& pml, const UserCmd& cmd,
              const AirMoveContext& context, const MoveBounds& bounds, const C& collision) {
    friction(ps, pml);

    const float command_scale = cmd_scale_air(ps, cmd, context.player_spectate_speed_scale);
    float forward[3] = {pml.forward[0], pml.forward[1], 0.0f};
    float right[3]   = {pml.right[0],   pml.right[1],   0.0f};
    normalize3(forward);
    normalize3(right);

    float wishdir[3] = {
        cmd.rightmove * right[0] + cmd.forwardmove * forward[0],
        cmd.rightmove * right[1] + cmd.forwardmove * forward[1],
        cmd.rightmove * right[2] + cmd.forwardmove * forward[2],
    };
    float wishspeed = normalize3(wishdir);
    accelerate(ps, pml, wishdir, wishspeed * command_scale, 1.0f);

    if (pml.ground_plane != 0) {
        const float normal[3] = {
            std::bit_cast<float>(pml.ground_trace[1]),
            std::bit_cast<float>(pml.ground_trace[2]),
            std::bit_cast<float>(pml.ground_trace[3]),
        };
        float tmp[3];
        clip_velocity(ps.velocity, normal, tmp);
        std::memcpy(ps.velocity, tmp, sizeof(tmp));
    }

    const float effective_gravity = adjusted_gravity(ps, context);
    step_slide_move(ps, pml, trace_iw4::GroundTraceInput{},
                    bounds.mins, bounds.maxs, bounds.tracemask, effective_gravity);
}

// ---------------------------------------------------------------------------
// slide.rs (placeholder step_slide_move)
// ---------------------------------------------------------------------------

void project_velocity(float velocity[3], const float normal[3]) {
    const float dot = velocity[0] * normal[0] + velocity[1] * normal[1] + velocity[2] * normal[2];
    if (dot > 0.0f) {
        const float denom = normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2];
        if (denom > 0.0f) {
            const float scale = dot / denom;
            velocity[0] -= scale * normal[0];
            velocity[1] -= scale * normal[1];
            velocity[2] -= scale * normal[2];
        }
    }
}

void step_slide_move(PlayerState& ps, const Pml& pml,
                     const trace_iw4::GroundTraceInput&,
                     const float /*mins*/[3], const float /*maxs*/[3],
                     uint32_t /*tracemask*/, std::optional<float> gravity) {
    float grav = gravity.value_or(0.0f);
    ps.velocity[2] += grav * pml.frametime;
    ps.origin[0] += ps.velocity[0] * pml.frametime;
    ps.origin[1] += ps.velocity[1] * pml.frametime;
    ps.origin[2] += ps.velocity[2] * pml.frametime;
}

} // namespace movement_iw4
