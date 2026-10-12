// Port of iw4L crates/movement_iw4: accelerate.rs, is_in_air.rs (inline in
// header), integrate.rs, viewangles.rs, cmdscale.rs, stance.rs (partial).
#include "movement_iw4/movement_iw4.h"

#include "math_iw4/math_iw4.h"

#include <cmath>

namespace movement_iw4 {

namespace {

using namespace playerstate_iw4;

// accelerate.rs helpers --------------------------------------------------

float vec3_normalize(float vector[3]) {
    const float length_squared =
        vector[2] * vector[2] + vector[0] * vector[0] + vector[1] * vector[1];
    const float length = std::sqrt(length_squared);
    const float divisor = length <= 0.0f ? 1.0f : length;
    const float scale = 1.0f / divisor;
    vector[0] = scale * vector[0];
    vector[1] = scale * vector[1];
    vector[2] = scale * vector[2];
    return length;
}

void accelerate_velocity(float velocity[3], uint32_t flags, float frametime,
                         const float wishdir[3], float wishspeed_in, float accel) {
    float wishspeed = wishspeed_in;
    if ((flags & pm_flags::LADDER) != 0) {
        const float wish_x = wishspeed * wishdir[0];
        const float wish_y = wishdir[1] * wishspeed;
        const float wish_z = wishspeed * wishdir[2];
        float delta[3] = {
            wish_x - velocity[0],
            wish_y - velocity[1],
            wish_z - velocity[2],
        };
        const float delta_speed = vec3_normalize(delta);
        float acceleration = frametime * accel * wishspeed;
        if (delta_speed < acceleration) {
            acceleration = delta_speed;
        }
        velocity[0] = delta[0] * acceleration + velocity[0];
        velocity[1] = delta[1] * acceleration + velocity[1];
        velocity[2] = acceleration * delta[2] + velocity[2];
        return;
    }

    const float current_speed =
        velocity[2] * wishdir[2] + velocity[0] * wishdir[0] + velocity[1] * wishdir[1];
    const float add_speed = wishspeed - current_speed;
    if (add_speed <= 0.0f || std::isnan(add_speed)) {
        return;
    }

    if (wishspeed < 100.0f) {
        wishspeed = 100.0f;
    }
    float acceleration = frametime * accel * wishspeed;
    if (add_speed < acceleration) {
        acceleration = add_speed;
    }
    velocity[0] = wishdir[0] * acceleration + velocity[0];
    velocity[1] = wishdir[1] * acceleration + velocity[1];
    velocity[2] = wishdir[2] * acceleration + velocity[2];
}

// integrate.rs helper ------------------------------------------------------

void integrate_origin(float origin[3], const float velocity_in[3], float frametime) {
    origin[0] = origin[0] + frametime * velocity_in[0];
    origin[1] = velocity_in[1] * frametime + origin[1];
    origin[2] = frametime * velocity_in[2] + origin[2];
}

// viewangles helper ---------------------------------------------------------

float wrap_signed(float degrees) {
    return math_iw4::angle_subtract(degrees, 0.0f);
}

// cmdscale helper ------------------------------------------------------------

float scale_command(int8_t forward, int8_t right, float back_speed_scale,
                    float strafe_speed_scale, float speed) {
    const float raw_forward = static_cast<float>(forward);
    const float raw_right = static_cast<float>(right);
    const float scaled_forward = forward < 0 ? raw_forward * back_speed_scale : raw_forward;
    const float forward_abs = std::fabs(scaled_forward);
    const float right_abs = std::fabs(raw_right * strafe_speed_scale);
    const float max_component = right_abs > forward_abs ? right_abs : forward_abs;
    if (max_component == 0.0f) {
        return 0.0f;
    }
    const float magnitude = std::sqrt((raw_forward * raw_forward) + (raw_right * raw_right));
    return (max_component * speed) / (magnitude * 127.0f);
}

// stance.rs ------------------------------------------------------------------

constexpr float STANCE_TRANSITION_MS = 400.0f;
constexpr float PRONE_SCALE = 0.15f;
constexpr float CROUCH_SCALE = 0.65f;

} // namespace

void accelerate(PlayerState& ps, const Pml& pml, const float wishdir[3],
                float wishspeed, float accel) {
    accelerate_velocity(ps.velocity, ps.pm_flags, pml.frametime, wishdir, wishspeed, accel);
}

void predict_integrate(PlayerState& ps, const Pml& pml) {
    integrate_origin(ps.origin, ps.velocity, pml.frametime);
}

void update_view_angles(PlayerState& ps, const UserCmd& cmd, ViewAngleClamp clamp) {
    const bool unclamped =
        (ps.pm_type == 1 || ps.pm_type == 9) && clamp.unclamped_pitch_bit;

    for (size_t axis = 0; axis < 3; ++axis) {
        const float commanded = static_cast<float>(cmd.angles[axis]) * SHORT2ANGLE;
        float temp = wrap_signed(commanded + ps.delta_angles[axis]);

        if (axis == 0 && !unclamped) {
            if (clamp.pitch_down < temp) {
                ps.delta_angles[0] = clamp.pitch_down - commanded;
                temp = clamp.pitch_down;
            } else if (temp < -clamp.pitch_up) {
                ps.delta_angles[0] = -clamp.pitch_up - commanded;
                temp = -clamp.pitch_up;
            }
        }

        ps.viewangles[axis] = wrap_signed(temp);
    }
}

float cmd_scale_walk(const PlayerState& ps, const UserCmd& cmd, CmdScaleWalkContext context) {
    using namespace playerstate_iw4;
    const uint32_t flags = ps.pm_flags;
    const bool sprinting_ads = (flags & pm_flags::PRONE) != 0 && ps.f_weapon_pos_frac > 0.0f;

    const float forward = cmd.forwardmove < 0
                              ? static_cast<float>(cmd.forwardmove) * context.player_back_speed_scale
                              : static_cast<float>(cmd.forwardmove);
    const float forward_abs = std::fabs(forward);
    const float right_abs = std::fabs(static_cast<float>(cmd.rightmove) * context.player_strafe_speed_scale);
    const float max_component = right_abs > forward_abs ? right_abs : forward_abs;

    float scale = scale_command(cmd.forwardmove, cmd.rightmove,
                                context.player_back_speed_scale,
                                context.player_strafe_speed_scale,
                                static_cast<float>(ps.speed));
    if (max_component == 0.0f) {
        return 0.0f;
    }

    if ((flags & pm_flags::WALKING) != 0 || ps.leanf != 0.0f || sprinting_ads) {
        scale *= 0.4f;
    }

    if ((flags & pm_flags::SPRINTING) != 0) {
        scale *= context.player_sprint_speed_scale;
    }

    float pm_scale;
    switch (ps.pm_type) {
    case 2: pm_scale = 3.0f; break;
    case 3: pm_scale = 6.0f; break;
    default:
        pm_scale = stance_speed_scale(ps, cmd.server_time,
                                      context.player_last_stand_crawl_speed_scale);
        break;
    }
    scale *= pm_scale;

    if (ps.weapon != 0) {
        const bool leaning = (flags & pm_flags::WALKING) != 0;
        const bool use_move =
            context.weapon_move_speed_scale > 0.0f && !leaning && !sprinting_ads;
        if (use_move) {
            scale *= context.weapon_move_speed_scale;
        } else if (context.weapon_ads_move_speed_scale > 0.0f) {
            scale *= context.weapon_ads_move_speed_scale;
        }
    }

    if ((flags & pm_flags::SHELLSHOCKED) != 0 && context.shellshock_affects_movement) {
        scale *= 0.4f;
    }

    return scale * ps.move_speed_scale_multiplier;
}

StanceSurface stance_surface_type(const PlayerState& ps) {
    switch (ps.view_height_target) {
    case view_height::LAST_STAND: return StanceSurface::LastStand;
    case view_height::CROUCH: return StanceSurface::Crouch;
    case view_height::PRONE: return StanceSurface::Prone;
    default: return StanceSurface::Stand;
    }
}

float stance_speed_scale(const PlayerState& ps, int32_t server_time, float last_stand_scale) {
    if (ps.view_height_lerp_time != 0 &&
        ps.view_height_lerp_target == view_height::PRONE) {
        const float fraction =
            static_cast<float>(static_cast<uint32_t>(server_time) -
                               static_cast<uint32_t>(ps.view_height_lerp_time)) / STANCE_TRANSITION_MS;
        if (fraction >= 0.0f) {
            const float clamped = fraction > 1.0f ? 1.0f : fraction;
            if (clamped != 0.0f) {
                return clamped * PRONE_SCALE + (1.0f - clamped) * CROUCH_SCALE;
            }
        }
    }

    if (ps.view_height_lerp_time != 0 &&
        ps.view_height_lerp_target == view_height::CROUCH &&
        ps.view_height_lerp_down == 0) {
        const float fraction =
            static_cast<float>(static_cast<uint32_t>(server_time) -
                               static_cast<uint32_t>(ps.view_height_lerp_time)) / STANCE_TRANSITION_MS;
        if (fraction >= 0.0f) {
            const float clamped = fraction > 1.0f ? 1.0f : fraction;
            if (clamped != 0.0f) {
                return clamped * CROUCH_SCALE + (1.0f - clamped) * PRONE_SCALE;
            }
        }
    }

    switch (stance_surface_type(ps)) {
    case StanceSurface::LastStand: return last_stand_scale;
    case StanceSurface::Crouch: return CROUCH_SCALE;
    case StanceSurface::Prone: return PRONE_SCALE;
    case StanceSurface::Stand: return 1.0f;
    }
    return 1.0f;
}

int32_t view_height_lerp_duration(int32_t lerp_target, int32_t lerp_down) {
    if (lerp_target == view_height::PRONE) {
        return 400;
    }
    if (lerp_target == view_height::CROUCH) {
        return lerp_down != 0 ? 200 : 400;
    }
    return 200;
}

} // namespace movement_iw4
