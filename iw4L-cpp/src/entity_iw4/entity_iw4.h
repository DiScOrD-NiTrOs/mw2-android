// Port of iw4L crates/entity_iw4/src (lib.rs root + the small leaf modules):
// centity, entity_state, client_state, origin, player_angles, trajectory,
// adjust_mover, script_mover, events, corpse_info, glass (schema+logic).
#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <functional>
#include <optional>

#include "math_iw4/math_iw4.h"

namespace entity_iw4 {

// ---------------------------------------------------------------------------
// centity.rs
// ---------------------------------------------------------------------------
struct Centity {
    uint8_t e_type = 0;
    float origin[3] = {0, 0, 0};
    float angles[3] = {0, 0, 0};
    float pose_origin[3] = {0, 0, 0};
    uint8_t current_state[0x70] = {};
    uint8_t next_state[0x7c] = {};
    int32_t client_num = 0;
    int16_t other_entity_num = 0;
    uint8_t current_valid = 0;
    int32_t previous_event_sequence = 0;
};

// ---------------------------------------------------------------------------
// entity_state.rs
// ---------------------------------------------------------------------------
struct EntityState {
    int32_t number = 0;
    int32_t e_type = 0;
    int32_t time2 = 0;
    uint32_t e_flags = 0;
    int32_t tr_time = 0;
    int32_t tr_type = 0;
    int32_t tr_duration = 0;
    float tr_delta[3] = {0, 0, 0};
    float tr_base[3] = {0, 0, 0};
    int32_t apos_tr_time = 0;
    int32_t apos_tr_type = 0;
    int32_t apos_tr_duration = 0;
    float apos_tr_delta[3] = {0, 0, 0};
    float apos_tr_base[3] = {0, 0, 0};
    float cull_dist = 0.0f;
    int32_t period = 0;
    uint32_t anonymous_data_2 = 0;
    uint32_t waist_pitch = 0;
    uint32_t torso_pitch = 0;
    int32_t other_entity_num = 0;
    int32_t attacker_entity_num = 0;
    int32_t ground_entity_num = 0;
    int32_t event_parm = 0;
    uint32_t solid = 0;
    int32_t index = 0;
    int32_t event_sequence = 0;
    int32_t events[4] = {};
    int32_t event_parms[4] = {};
    int32_t client_num = 0;
    uint32_t wes = 0;
    int32_t torso_anim = 0;
    int32_t legs_anim = 0;
    uint8_t weapon_model = 0;
    uint32_t client_link_info = 0;
    uint32_t part_bits_0 = 0;
    uint32_t part_bits_1 = 0;
    uint32_t part_bits_2 = 0;
    uint32_t part_bits_3 = 0;
    uint32_t part_bits_4 = 0;
    uint32_t part_bits_5 = 0;
    uint32_t client_mask_0 = 0;

    int32_t launch_time() const {
        return static_cast<int32_t>(std::bit_cast<uint32_t>(cull_dist));
    }
    void set_launch_time(int32_t time_ms) {
        cull_dist = std::bit_cast<float>(static_cast<uint32_t>(time_ms));
    }
};

// ---------------------------------------------------------------------------
// client_state.rs
// ---------------------------------------------------------------------------
constexpr size_t CLIENT_STATE_NAME_LEN = 16;

constexpr int32_t TEAM_FREE = 0;
constexpr int32_t TEAM_AXIS = 1;
constexpr int32_t TEAM_ALLIES = 2;
constexpr int32_t TEAM_SPECTATOR = 3;

struct ClientState {
    int32_t team = 0;
    int32_t modelindex = 0;
    int32_t dual_wielding = 0;
    int32_t riot_shield_next = 0;
    int32_t attach_model_index_0 = 0;
    int32_t attach_model_index_1 = 0;
    int32_t attach_model_index_2 = 0;
    int32_t attach_model_index_3 = 0;
    int32_t attach_model_index_4 = 0;
    int32_t attach_model_index_5 = 0;
    int32_t attach_tag_index_0 = 0;
    int32_t attach_tag_index_1 = 0;
    int32_t attach_tag_index_2 = 0;
    int32_t attach_tag_index_3 = 0;
    int32_t attach_tag_index_4 = 0;
    int32_t attach_tag_index_5 = 0;
    int32_t name_0 = 0;
    int32_t name_4 = 0;
    int32_t name_8 = 0;
    int32_t name_12 = 0;
    float max_sprint_time_multiplier = 0.0f;
    int32_t prestige = 0;
    int32_t dive_state = 0;
    uint32_t voice_connectivity_bits = 0;
    uint32_t player_card_icon = 0;
    uint32_t player_card_title = 0;
    uint32_t player_card_nameplate = 0;
};

inline const char* get_team_name(int32_t team) {
    switch (team) {
    case TEAM_FREE: return "TEAM_FREE";
    case TEAM_AXIS: return "TEAM_AXIS";
    case TEAM_ALLIES: return "TEAM_ALLIES";
    case TEAM_SPECTATOR: return "TEAM_SPECTATOR";
    default: return "";
    }
}

inline int32_t client_state_team_from_sessionteam(bool team_based, const char* sessionteam) {
    if (!team_based) return TEAM_FREE;
    if (sessionteam == std::string("axis")) return TEAM_AXIS;
    if (sessionteam == std::string("allies")) return TEAM_ALLIES;
    if (sessionteam == std::string("spectator")) return TEAM_SPECTATOR;
    return TEAM_FREE;
}

inline std::array<uint8_t, CLIENT_STATE_NAME_LEN> client_state_name_bytes(int32_t name_0,
                                                                            int32_t name_4,
                                                                            int32_t name_8,
                                                                            int32_t name_12) {
    std::array<uint8_t, CLIENT_STATE_NAME_LEN> bytes{};
    std::memcpy(bytes.data(), &name_0, 4);
    std::memcpy(bytes.data() + 4, &name_4, 4);
    std::memcpy(bytes.data() + 8, &name_8, 4);
    std::memcpy(bytes.data() + 12, &name_12, 4);
    return bytes;
}

inline std::optional<std::string> client_state_name(const uint8_t bytes[CLIENT_STATE_NAME_LEN]) {
    size_t end = CLIENT_STATE_NAME_LEN;
    for (size_t i = 0; i < CLIENT_STATE_NAME_LEN; ++i) {
        if (bytes[i] == 0) { end = i; break; }
    }
    if (end == 0) return std::nullopt;
    std::string s(reinterpret_cast<const char*>(bytes), end);
    if (s.empty()) return std::nullopt;
    return s;
}

inline std::array<uint8_t, CLIENT_STATE_NAME_LEN> pack_client_state_name(const std::string& s) {
    std::array<uint8_t, CLIENT_STATE_NAME_LEN> bytes{};
    size_t n = 0;
    for (char c : s) {
        if (n >= CLIENT_STATE_NAME_LEN - 1) break;
        bytes[n] = static_cast<uint8_t>(c);
        ++n;
    }
    return bytes;
}

// ---------------------------------------------------------------------------
// trajectory.rs
// ---------------------------------------------------------------------------
constexpr int32_t TR_STATIONARY = 0;
constexpr int32_t TR_INTERPOLATE = 1;
constexpr int32_t TR_LINEAR = 2;
constexpr int32_t TR_LINEAR_STOP = 3;
constexpr int32_t TR_GRAVITY = 5;
constexpr float TRAJECTORY_MSEC_TO_SEC = 0.001f;
constexpr float TRAJECTORY_GRAVITY = 400.0f;
constexpr float TRAJECTORY_GRAVITY_DELTA = 800.0f;
constexpr float INV_DELTA_ZERO = 0.0f;

struct Trajectory {
    int32_t tr_time = 0;
    int32_t tr_type = 0;
    int32_t tr_duration = 0;
    float tr_delta[3] = {0, 0, 0};
    float tr_base[3] = {0, 0, 0};
};

namespace {
float inv_delta(float start, float end) {
    const float diff = start - end;
    return diff == 0.0f ? 0.0f : 1.0f / diff;
}

float trajectory_dt(int32_t tr_time, int32_t at_time) {
    return static_cast<float>(static_cast<uint32_t>(at_time - tr_time)) * TRAJECTORY_MSEC_TO_SEC;
}

void vec3_mad(const float base[3], float scale, const float delta[3], float out[3]) {
    out[0] = base[0] + scale * delta[0];
    out[1] = base[1] + scale * delta[1];
    out[2] = base[2] + scale * delta[2];
}

float clamp_ms(int32_t raw, int32_t lo, int32_t hi) {
    if (raw < lo) return static_cast<float>(lo);
    if (raw > hi) return static_cast<float>(hi);
    return static_cast<float>(raw);
}
} // namespace

inline void evaluate_trajectory(const Trajectory& tr, int32_t at_time, float out[3]) {
    switch (tr.tr_type) {
    case TR_STATIONARY:
    case TR_INTERPOLATE:
    case 9:
    case 0xc:
        std::memcpy(out, tr.tr_base, 3 * sizeof(float));
        return;
    case TR_LINEAR:
    case 10: {
        const float dt = trajectory_dt(tr.tr_time, at_time);
        vec3_mad(tr.tr_base, dt, tr.tr_delta, out);
        return;
    }
    case TR_LINEAR_STOP: {
        const int32_t stop_at = tr.tr_time + tr.tr_duration;
        const int32_t t = (stop_at < at_time) ? stop_at : at_time;
        const float dt = trajectory_dt(tr.tr_time, t);
        const float a = (dt < 0.0f) ? 0.0f : dt;
        vec3_mad(tr.tr_base, a, tr.tr_delta, out);
        return;
    }
    case TR_GRAVITY:
    case 6:
    case 0xb: {
        const float dt = trajectory_dt(tr.tr_time, at_time);
        float o[3];
        vec3_mad(tr.tr_base, dt, tr.tr_delta, o);
        o[2] -= TRAJECTORY_GRAVITY * dt * dt;
        std::memcpy(out, o, 3 * sizeof(float));
        return;
    }
    default:
        // Panic-like: set out to zeros and return; callers that rely on the panic
        // should be reconsidered when the real engine links.
        std::memset(out, 0, 3 * sizeof(float));
        return;
    }
}

inline void evaluate_trajectory_delta(const Trajectory& tr, int32_t at_time, float out[3]) {
    switch (tr.tr_type) {
    case TR_STATIONARY:
    case TR_INTERPOLATE:
    case 0xc:
        out[0] = out[1] = out[2] = 0.0f;
        return;
    case TR_LINEAR:
    case 10:
        std::memcpy(out, tr.tr_delta, 3 * sizeof(float));
        return;
    case TR_LINEAR_STOP: {
        const int32_t stop_at = tr.tr_time + tr.tr_duration;
        if (stop_at < at_time) {
            out[0] = out[1] = out[2] = 0.0f;
        } else {
            std::memcpy(out, tr.tr_delta, 3 * sizeof(float));
        }
        return;
    }
    case TR_GRAVITY:
    case 6:
    case 0xb: {
        const float dt = trajectory_dt(tr.tr_time, at_time);
        out[0] = tr.tr_delta[0];
        out[1] = tr.tr_delta[1];
        out[2] = tr.tr_delta[2] - dt * TRAJECTORY_GRAVITY_DELTA;
        return;
    }
    default:
        out[0] = out[1] = out[2] = 0.0f;
        return;
    }
}

inline void truncated_tr_delta(const float delta[3], float out[3]) {
    out[0] = static_cast<float>(static_cast<int32_t>(delta[0]));
    out[1] = static_cast<float>(static_cast<int32_t>(delta[1]));
    out[2] = static_cast<float>(static_cast<int32_t>(delta[2]));
}

// ---------------------------------------------------------------------------
// origin.rs
// ---------------------------------------------------------------------------
constexpr float PARENT_LINK_AXIS_IDENTITY_ROWS[3][3][3] = {
    {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}
};

struct DObjAnimMat {
    float quat[4] = {0, 0, 0, 1};
    float trans[3] = {0, 0, 0};
    float trans_weight = 0.0f;

    static DObjAnimMat identity() {
        DObjAnimMat m;
        m.quat[3] = 1.0f;
        return m;
    }

    void axis(float out[3][3]) const {
        const float x = quat[0], y = quat[1], z = quat[2], w = quat[3];
        const float tw = trans_weight;
        const float tw_x = tw * x, tw_y = y * tw, tw_zz = tw * z * z, tw_zw = tw * z * w;
        out[0][0] = 1.0f - (tw_y * y + tw_zz);
        out[0][1] = y * tw_x + tw_zw;
        out[0][2] = z * tw_x - tw_y * w;
        out[1][0] = y * tw_x - tw_zw;
        out[1][1] = 1.0f - (tw_x * x + tw_zz);
        out[1][2] = tw_x * w + z * tw_y;
        out[2][0] = tw_y * w + z * tw_x;
        out[2][1] = z * tw_y - tw_x * w;
        out[2][2] = 1.0f - (tw_y * y + tw_x * x);
    }
};

inline void set_origin(EntityState& es, const float origin[3]) {
    std::memcpy(es.tr_base, origin, sizeof(es.tr_base));
    es.tr_type = TR_STATIONARY;
    es.tr_time = 0;
    es.tr_duration = 0;
    es.tr_delta[0] = es.tr_delta[1] = es.tr_delta[2] = 0.0f;
}
inline void set_angle(EntityState& es, const float angles[3]) {
    std::memcpy(es.apos_tr_base, angles, sizeof(es.apos_tr_base));
    es.apos_tr_type = TR_STATIONARY;
    es.apos_tr_time = 0;
    es.apos_tr_duration = 0;
    es.apos_tr_delta[0] = es.apos_tr_delta[1] = es.apos_tr_delta[2] = 0.0f;
}
inline void parent_link_pose(EntityState& es, const float origin[3], const float angles[3]) {
    set_origin(es, origin);
    set_angle(es, angles);
    es.tr_type = TR_INTERPOLATE;
    es.apos_tr_type = TR_INTERPOLATE;
}

inline void parent_link_world_from_tag(const float parent_origin[3],
                                        const float parent_angles[3],
                                        const DObjAnimMat& mat,
                                        float out_origin[3], float out_angles[3]) {
    const math_iw4::Vec3 p_ang = {parent_angles[0], parent_angles[1], parent_angles[2]};
    const auto parent_axis = math_iw4::angles_to_axis(p_ang);
    float bone_axis_arr[3][3];
    mat.axis(bone_axis_arr);
    math_iw4::Axis bone_axis;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            bone_axis[i][j] = bone_axis_arr[i][j];
    const auto world_axis = math_iw4::matrix_multiply(bone_axis, parent_axis);
    const math_iw4::Vec3 trans = {mat.trans[0], mat.trans[1], mat.trans[2]};
    const math_iw4::Vec3 p_orig = {parent_origin[0], parent_origin[1], parent_origin[2]};
    const auto world_origin = math_iw4::matrix_transform_vector43(trans, parent_axis, p_orig);
    std::memcpy(out_origin, world_origin.data(), 3 * sizeof(float));
    const auto world_angles_res = math_iw4::axis_to_angles(world_axis);
    std::memcpy(out_angles, world_angles_res.data(), 3 * sizeof(float));
}

inline void parent_link_apply_local(const float world_origin[3], const float world_angles[3],
                                    const float local_axis[3][3], const float local_origin[3],
                                    float out_origin[3], float out_angles[3]) {
    const math_iw4::Vec3 w_ang = {world_angles[0], world_angles[1], world_angles[2]};
    const auto parent_axis = math_iw4::angles_to_axis(w_ang);
    math_iw4::Axis l_axis;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            l_axis[i][j] = local_axis[i][j];
    const math_iw4::Vec3 l_orig = {local_origin[0], local_origin[1], local_origin[2]};
    const math_iw4::Vec3 w_orig = {world_origin[0], world_origin[1], world_origin[2]};
    math_iw4::Axis res_axis;
    math_iw4::Vec3 res_orig;
    math_iw4::matrix_multiply43(l_axis, l_orig, parent_axis, w_orig, res_axis, res_orig);
    std::memcpy(out_origin, res_orig.data(), 3 * sizeof(float));
    const auto angles_res = math_iw4::axis_to_angles(res_axis);
    std::memcpy(out_angles, angles_res.data(), 3 * sizeof(float));
}

// ---------------------------------------------------------------------------
// player_angles.rs
// ---------------------------------------------------------------------------
constexpr float PLAYER_MOVE_FACTOR_ON_TORSO = 0.0f;
constexpr float BG_SWING_SPEED = 0.2f;
constexpr float BG_LEG_YAW_TOLERANCE = 20.0f;
constexpr float LEGS_YAW_CLAMP = 150.0f;
constexpr float TORSO_YAW_CLAMP = 90.0f;
constexpr float SWING_SCALE_K = 0.05000000074505806f;
constexpr float SWING_SCALE_MIN = 0.5f;

struct SwingState {
    float angle = 0.0f;
    bool swinging = false;
};

struct PlayerAngleDvars {
    float move_factor_on_torso = PLAYER_MOVE_FACTOR_ON_TORSO;
    float swing_speed = BG_SWING_SPEED;
    float leg_yaw_tolerance = BG_LEG_YAW_TOLERANCE;
};

struct PlayerAngleInput {
    float base_yaw = 0.0f;
    float movement_yaw = 0.0f;
    PlayerAngleDvars dvars{};
    float frametime_ms = 0.0f;
};

struct PlayerAngleOutput {
    SwingState torso{};
    SwingState legs{};
    float torso_dest = 0.0f;
    float legs_dest = 0.0f;
};

namespace {
void swing_angles(float destination, float swing_tolerance, float clamp_tolerance,
                  float speed, float frametime_ms, SwingState& state) {
    if (!state.swinging) {
        const float delta = math_iw4::angle_subtract(state.angle, destination);
        if (delta <= swing_tolerance && -swing_tolerance <= delta) return;
        state.swinging = true;
    }

    const float swing = math_iw4::angle_subtract(destination, state.angle);
    float scale = std::fabs(swing) * SWING_SCALE_K;
    if (scale < SWING_SCALE_MIN) scale = SWING_SCALE_MIN;

    float step_angle;
    if (swing < 0.0f) {
        float step = -frametime_ms * scale * speed;
        if (swing < step) {
            state.swinging = true;
        } else {
            step = swing;
            state.swinging = false;
        }
        step_angle = step;
    } else {
        float step = frametime_ms * scale * speed;
        if (step < swing) {
            state.swinging = true;
        } else {
            step = swing;
            state.swinging = false;
        }
        step_angle = step;
    }

    state.angle = math_iw4::angle_normalize_360(state.angle + step_angle);

    const float after = math_iw4::angle_subtract(destination, state.angle);
    if (after > clamp_tolerance) {
        state.angle = math_iw4::angle_normalize_360(destination - clamp_tolerance);
    } else if (after < -clamp_tolerance) {
        state.angle = math_iw4::angle_normalize_360(destination + clamp_tolerance);
    }
}
} // namespace

inline PlayerAngleOutput player_angles(const PlayerAngleInput& input,
                                       SwingState& torso, SwingState& legs) {
    const float base = math_iw4::angle_normalize_360(input.base_yaw);
    const float movement = input.movement_yaw;
    const float torso_dest = input.dvars.move_factor_on_torso * movement + base;
    const float legs_dest = math_iw4::angle_normalize_360(base + movement);

    swing_angles(torso_dest, 0.0f, TORSO_YAW_CLAMP,
                 input.dvars.swing_speed, input.frametime_ms, torso);
    const float leg_tol = legs.swinging ? 0.0f : input.dvars.leg_yaw_tolerance;
    swing_angles(legs_dest, leg_tol, LEGS_YAW_CLAMP,
                 input.dvars.swing_speed, input.frametime_ms, legs);

    PlayerAngleOutput out;
    out.torso = torso;
    out.legs = legs;
    out.torso_dest = torso_dest;
    out.legs_dest = legs_dest;
    return out;
}

inline float legs_offset_deg(float torso_yaw, float legs_yaw) {
    return math_iw4::angle_subtract(legs_yaw, torso_yaw);
}

// ---------------------------------------------------------------------------
// adjust_mover.rs
// ---------------------------------------------------------------------------
constexpr int32_t ET_GENERAL = 0;
constexpr int32_t ET_PLAYER = 1;
constexpr int32_t ET_PLAYER_CORPSE = 2;
constexpr int32_t ET_ITEM = 3;
constexpr int32_t ET_MISSILE = 4;
constexpr int32_t ET_SCRIPTMOVER = 6;
constexpr int32_t ET_PLANE = 0xd;
constexpr int32_t ET_PRIMARY_LIGHT = 10;

inline bool mover_num_in_adjust_range(int32_t mover_num) {
    return static_cast<uint32_t>(mover_num - 1) < 0x7fd;
}
inline bool adjust_position_for_mover_evaluates(int32_t mover_num, int32_t e_type) {
    return mover_num_in_adjust_range(mover_num) &&
           (e_type == ET_SCRIPTMOVER || e_type == ET_PLANE);
}
inline void adjust_position_for_mover(const float input[3], int32_t mover_num, int32_t e_type,
                                       const Trajectory* pos, int32_t from_time, int32_t to_time,
                                       float out[3]) {
    if (!adjust_position_for_mover_evaluates(mover_num, e_type) || !pos) {
        std::memcpy(out, input, 3 * sizeof(float));
        return;
    }
    float old[3], new_pos[3];
    evaluate_trajectory(*pos, from_time, old);
    evaluate_trajectory(*pos, to_time, new_pos);
    out[0] = input[0] + (new_pos[0] - old[0]);
    out[1] = input[1] + (new_pos[1] - old[1]);
    out[2] = input[2] + (new_pos[2] - old[2]);
}

// ---------------------------------------------------------------------------
// script_mover.rs
// ---------------------------------------------------------------------------
constexpr uint32_t CG_SCRIPT_MOVER_NODRAW = 0x20;
constexpr uint32_t SCRIPT_MOVER_BMODEL_SOLID = 0xffffff;

inline bool script_mover_add_bmodel(int32_t e_type, uint32_t e_flags,
                                    uint32_t solid, bool dobj_present) {
    return e_type == ET_SCRIPTMOVER &&
           (e_flags & CG_SCRIPT_MOVER_NODRAW) == 0 &&
           !dobj_present &&
           solid == SCRIPT_MOVER_BMODEL_SOLID;
}

// ---------------------------------------------------------------------------
// events.rs — EntityEventKind + EntityState event ring
// ---------------------------------------------------------------------------
constexpr int32_t ET_EVENTS = 0x12;
constexpr int32_t EVENT_SEQUENCE_MASK = 0x7ff;
constexpr int32_t EVENT_SEQUENCE_WRAP_WINDOW = 0x200;
constexpr int32_t EVENT_RING_LEN = 4;
constexpr int32_t LOCAL_SOUND_ENTITY = 0x7ff;

struct EntityEventKind {
    int32_t value;
    EntityEventKind(int32_t v) : value(v) {}
    bool operator==(int32_t v) const { return value == v; }
    bool operator!=(int32_t v) const { return value != v; }
    int32_t operator()() const { return value; }
};

// Event taxonomy constants (subset most relevant to the port). Full list is long;
// expose the ones used by gameplay/movement code.
namespace EntityEventKinds {
constexpr int32_t NONE = 0x00;
constexpr int32_t FOLIAGE_SOUND = 0x01;
constexpr int32_t STOP_WEAPON_SOUND = 0x02;
constexpr int32_t SOUND_ALIAS = 0x03;
constexpr int32_t SOUND_ALIAS_AS_MASTER = 0x04;
constexpr int32_t STOPSOUNDS = 0x05;
constexpr int32_t STANCE_FORCE_STAND = 0x06;
constexpr int32_t STANCE_FORCE_CROUCH = 0x07;
constexpr int32_t STANCE_FORCE_PRONE = 0x08;
constexpr int32_t ITEM_PICKUP = 0x0a;
constexpr int32_t AMMO_PICKUP = 0x0b;
constexpr int32_t NOAMMO = 0x0c;
constexpr int32_t RESET_ADS = 0x11;
constexpr int32_t RELOAD = 0x12;
constexpr int32_t RELOAD_FROM_EMPTY = 0x13;
constexpr int32_t RELOAD_START = 0x14;
constexpr int32_t RELOAD_END = 0x15;
constexpr int32_t RELOAD_ADDAMMO = 0x17;
constexpr int32_t RAISE_WEAPON = 0x18;
constexpr int32_t FIRST_RAISE_WEAPON = 0x19;
constexpr int32_t PUTAWAY_WEAPON = 0x1a;
constexpr int32_t WEAPON_ALT = 0x1b;
constexpr int32_t PULLBACK_WEAPON = 0x1d;
constexpr int32_t FIRE_WEAPON = 0x1e;
constexpr int32_t FIRE_WEAPON_LASTSHOT = 0x1f;
constexpr int32_t RECHAMBER_WEAPON = 0x21;
constexpr int32_t EJECT_BRASS = 0x22;
constexpr int32_t FIRE_WEAPON_LEFT = 0x23;
constexpr int32_t FIRE_WEAPON_LASTSHOT_LEFT = 0x24;
constexpr int32_t EJECT_BRASS_LEFT = 0x25;
constexpr int32_t SV_FIRE_WEAPON = 0x2a;
constexpr int32_t MELEE_SWIPE = 0x2e;
constexpr int32_t FIRE_MELEE = 0x2f;
constexpr int32_t PREP_OFFHAND = 0x30;
constexpr int32_t USE_OFFHAND = 0x31;
constexpr int32_t MELEE_HIT = 0x33;
constexpr int32_t MELEE_MISS = 0x34;
constexpr int32_t MELEE_BLOOD = 0x35;
constexpr int32_t BULLET_HIT = 0x3a;
constexpr int32_t BULLET_HIT_SHIELD = 0x3b;
constexpr int32_t BULLET_HIT_EXPLODE = 0x3c;
constexpr int32_t BULLET_HIT_CLIENT_SMALL = 0x3d;
constexpr int32_t BULLET_HIT_CLIENT_LARGE = 0x3e;
constexpr int32_t BULLET_HIT_CLIENT_EXPLODE = 0x3f;
constexpr int32_t BULLET_HIT_CLIENT_SHIELD = 0x40;
constexpr int32_t EXPLOSIVE_IMPACT_ON_SHIELD = 0x41;
constexpr int32_t EXPLOSIVE_SPLASH_ON_SHIELD = 0x42;
constexpr int32_t GRENADE_BOUNCE = 0x43;
constexpr int32_t GRENADE_STICK = 0x44;
constexpr int32_t GRENADE_REST = 0x45;
constexpr int32_t GRENADE_EXPLODE = 0x46;
constexpr int32_t ROCKET_EXPLODE = 0x49;
constexpr int32_t ROCKET_EXPLODE_NOMARKS = 0x4a;
constexpr int32_t FLASHBANG_EXPLODE = 0x4b;
constexpr int32_t PLAY_FX = 0x53;
constexpr int32_t PLAY_FX_ON_TAG = 0x54;
constexpr int32_t STOP_FX_ON_TAG = 0x55;
constexpr int32_t PHYS_EXPLOSION_SPHERE = 0x56;
constexpr int32_t DETONATE = 0x5e;
constexpr int32_t OBITUARY = 0x62;
constexpr int32_t FOOTSTEP_SPRINT = 0x6b;
constexpr int32_t FOOTSTEP_RUN = 0x6c;
constexpr int32_t FOOTSTEP_WALK = 0x6d;
constexpr int32_t FOOTSTEP_PRONE = 0x6e;
constexpr int32_t JUMP = 0x6f;
constexpr int32_t LANDING_DEFAULT = 0x70;
constexpr int32_t LANDING_BARK = 0x71;
constexpr int32_t LANDING_BRICK = 0x72;
constexpr int32_t LANDING_CARPET = 0x73;
constexpr int32_t LANDING_CLOTH = 0x74;
constexpr int32_t LANDING_CONCRETE = 0x75;
constexpr int32_t LANDING_DIRT = 0x76;
constexpr int32_t LANDING_FLESH = 0x77;
constexpr int32_t LANDING_FOLIAGE = 0x78;
constexpr int32_t LANDING_GLASS = 0x79;
constexpr int32_t LANDING_GRASS = 0x7a;
constexpr int32_t LANDING_GRAVEL = 0x7b;
constexpr int32_t LANDING_ICE = 0x7c;
constexpr int32_t LANDING_METAL = 0x7d;
constexpr int32_t LANDING_MUD = 0x7e;
constexpr int32_t LANDING_PAPER = 0x7f;
constexpr int32_t LANDING_PLASTER = 0x80;
constexpr int32_t LANDING_ROCK = 0x81;
constexpr int32_t LANDING_SAND = 0x82;
constexpr int32_t LANDING_SNOW = 0x83;
constexpr int32_t LANDING_WATER = 0x84;
constexpr int32_t LANDING_WOOD = 0x85;
constexpr int32_t LANDING_ASPHALT = 0x86;
constexpr int32_t LANDING_CERAMIC = 0x87;
constexpr int32_t LANDING_PLASTIC = 0x88;
constexpr int32_t LANDING_RUBBER = 0x89;
constexpr int32_t LANDING_CUSHION = 0x8a;
constexpr int32_t LANDING_FRUIT = 0x8b;
constexpr int32_t LANDING_PAINTEDMETAL = 0x8c;
constexpr int32_t LANDING_RIOTSHIELD = 0x8d;
constexpr int32_t LANDING_SLUSH = 0x8e;
constexpr int32_t LANDING_PAIN_DEFAULT = 0x8f;
constexpr int32_t LANDING_PAIN_BARK = 0x90;
constexpr int32_t LANDING_PAIN_BRICK = 0x91;
constexpr int32_t LANDING_PAIN_CARPET = 0x92;
constexpr int32_t LANDING_PAIN_CLOTH = 0x93;
constexpr int32_t LANDING_PAIN_CONCRETE = 0x94;
constexpr int32_t LANDING_PAIN_DIRT = 0x95;
constexpr int32_t LANDING_PAIN_FLESH = 0x96;
constexpr int32_t LANDING_PAIN_FOLIAGE = 0x97;
constexpr int32_t LANDING_PAIN_GLASS = 0x98;
constexpr int32_t LANDING_PAIN_GRASS = 0x99;
constexpr int32_t LANDING_PAIN_GRAVEL = 0x9a;
constexpr int32_t LANDING_PAIN_ICE = 0x9b;
constexpr int32_t LANDING_PAIN_METAL = 0x9c;
constexpr int32_t LANDING_PAIN_MUD = 0x9d;
constexpr int32_t LANDING_PAIN_PAPER = 0x9e;
constexpr int32_t LANDING_PAIN_PLASTER = 0x9f;
constexpr int32_t LANDING_PAIN_ROCK = 0xa0;
constexpr int32_t LANDING_PAIN_SAND = 0xa1;
constexpr int32_t LANDING_PAIN_SNOW = 0xa2;
constexpr int32_t LANDING_PAIN_WATER = 0xa3;
constexpr int32_t LANDING_PAIN_WOOD = 0xa4;
constexpr int32_t LANDING_PAIN_ASPHALT = 0xa5;
constexpr int32_t LANDING_PAIN_CERAMIC = 0xa6;
constexpr int32_t LANDING_PAIN_PLASTIC = 0xa7;
constexpr int32_t LANDING_PAIN_RUBBER = 0xa8;
constexpr int32_t LANDING_PAIN_CUSHION = 0xa9;
constexpr int32_t LANDING_PAIN_FRUIT = 0xaa;
constexpr int32_t LANDING_PAIN_PAINTEDMETAL = 0xab;
constexpr int32_t LANDING_PAIN_RIOTSHIELD = 0xac;
constexpr int32_t LANDING_PAIN_SLUSH = 0xad;
constexpr int32_t MANTLE = 0xae;
constexpr int32_t PLAY_RUMBLE_ON_ENT = 0xb1;
constexpr int32_t PLAY_RUMBLE_ON_POS = 0xb2;
constexpr int32_t STOP_RUMBLE = 0xb5;

// convenience views
constexpr int32_t LANDING_FIRST = LANDING_DEFAULT;
constexpr int32_t LANDING_LAST = LANDING_SLUSH;
constexpr int32_t LANDING_PAIN_FIRST = LANDING_PAIN_DEFAULT;
constexpr int32_t LANDING_PAIN_LAST = LANDING_PAIN_SLUSH;
} // namespace EntityEventKinds

inline bool entity_event_is_landing_surface(const EntityEventKind& e) {
    const int32_t v = e();
    return (v >= EntityEventKinds::LANDING_FIRST && v <= EntityEventKinds::LANDING_LAST) ||
           (v >= EntityEventKinds::LANDING_PAIN_FIRST && v <= EntityEventKinds::LANDING_PAIN_LAST);
}
inline std::optional<int32_t> entity_event_landing_surface_index(const EntityEventKind& e) {
    const int32_t v = e();
    if (v >= EntityEventKinds::LANDING_FIRST && v <= EntityEventKinds::LANDING_LAST)
        return v - EntityEventKinds::LANDING_FIRST;
    if (v >= EntityEventKinds::LANDING_PAIN_FIRST && v <= EntityEventKinds::LANDING_PAIN_LAST)
        return v - EntityEventKinds::LANDING_PAIN_FIRST;
    return std::nullopt;
}
inline bool entity_event_is_landing_pain(const EntityEventKind& e) {
    const int32_t v = e();
    return v >= EntityEventKinds::LANDING_PAIN_FIRST && v <= EntityEventKinds::LANDING_PAIN_LAST;
}
inline bool entity_event_is_fire_last_shot(const EntityEventKind& e) {
    const int32_t v = e();
    return v == EntityEventKinds::FIRE_WEAPON_LASTSHOT ||
           v == EntityEventKinds::FIRE_WEAPON_LASTSHOT_LEFT ||
           v == 0x27 || v == 0x29 || v == 0x2b || v == 0x2d;
}
inline bool entity_event_is_left_hand_fire(const EntityEventKind& e) {
    const int32_t v = e();
    return v == 0x23 || v == 0x24 || v == 0x25 || v == 0x28 || v == 0x29 || v == 0x2c || v == 0x2d;
}
inline EntityEventKind predicted_weapon_fire_event(int32_t hand, bool last_shot) {
    if (hand != 0) {
        return last_shot ? EntityEventKind(EntityEventKinds::FIRE_WEAPON_LASTSHOT_LEFT)
                         : EntityEventKind(EntityEventKinds::FIRE_WEAPON_LEFT);
    }
    return last_shot ? EntityEventKind(EntityEventKinds::FIRE_WEAPON_LASTSHOT)
                     : EntityEventKind(EntityEventKinds::FIRE_WEAPON);
}
inline std::optional<EntityEventKind> bullet_hit_event(int32_t impact_type, bool local_client) {
    if (local_client) {
        switch (impact_type) {
        case 1: case 9: return EntityEventKind(EntityEventKinds::BULLET_HIT_CLIENT_SMALL);
        case 2: return EntityEventKind(EntityEventKinds::BULLET_HIT_CLIENT_LARGE);
        case 3: return EntityEventKind(EntityEventKinds::BULLET_HIT_CLIENT_SHIELD);
        default: return std::nullopt;
        }
    } else {
        switch (impact_type) {
        case 1: case 2: case 9: return EntityEventKind(EntityEventKinds::BULLET_HIT);
        case 3: return EntityEventKind(EntityEventKinds::BULLET_HIT_SHIELD);
        default: return std::nullopt;
        }
    }
}

struct SequencedEntityEvent {
    int32_t sequence = 0;
    EntityEventKind event{0};
    int32_t event_parm = 0;
};

inline void add_entity_event(EntityState& state, const EntityEventKind& event, int32_t event_parm) {
    if (event() == 0) return;
    const size_t slot = static_cast<size_t>(state.event_sequence & 3);
    state.events[slot] = event();
    state.event_parms[slot] = event_parm;
    state.event_sequence = (state.event_sequence + 1) & EVENT_SEQUENCE_MASK;
}
inline bool packet_entity_uses_event_ring(int32_t e_type) {
    return e_type != 8 && e_type != 9 && e_type < ET_EVENTS;
}
inline void consume_entity_events(const EntityState& state, int32_t* consumed_sequence,
                                  std::function<void(const SequencedEntityEvent&)> dispatch) {
    const int32_t next = state.event_sequence;
    if (*consumed_sequence == next) return;
    if (next == 0) {
        *consumed_sequence = 0;
        return;
    }
    int32_t consumed = *consumed_sequence;
    if (next + EVENT_SEQUENCE_WRAP_WINDOW < consumed) {
        consumed -= EVENT_SEQUENCE_MASK + 1;
    }
    if (next - consumed > EVENT_RING_LEN) {
        consumed = next - EVENT_RING_LEN;
    }
    while (consumed < next) {
        const size_t slot = static_cast<size_t>(static_cast<uint32_t>(consumed) & 3);
        dispatch(SequencedEntityEvent{
            consumed & EVENT_SEQUENCE_MASK,
            EntityEventKind(state.events[slot]),
            state.event_parms[slot],
        });
        ++consumed;
    }
    *consumed_sequence = next;
}

// ---------------------------------------------------------------------------
// corpse_info.rs
// ---------------------------------------------------------------------------
struct CorpseInfoPlayerAnimCopy {
    int32_t legs_anim = 0;
    int32_t torso_anim = 0;
    uint32_t torso_pitch = 0;
    uint32_t waist_pitch = 0;

    static CorpseInfoPlayerAnimCopy from_entity_state(const EntityState& es) {
        CorpseInfoPlayerAnimCopy c;
        c.legs_anim = es.legs_anim;
        c.torso_anim = es.torso_anim;
        c.torso_pitch = es.torso_pitch;
        c.waist_pitch = es.waist_pitch;
        return c;
    }
};

// ---------------------------------------------------------------------------
// glass.rs — schema + logic (the large packed-dir table is in a separate cpp).
// ---------------------------------------------------------------------------
constexpr uint16_t GLASS_DAMAGE_TO_WEAKEN = 25;
constexpr uint16_t GLASS_DAMAGE_TO_DESTROY = 100;
constexpr uint16_t GLASS_MELEE_DAMAGE = 1000;
constexpr uint16_t GLASS_DAMAGE_INVALID = 0xffff;
constexpr uint16_t GLASS_DAMAGE_ADD_CAP = 0xfffe;
constexpr uint32_t GLASS_FRACTURE_PROFILE_VERSION = 1;
constexpr float MISSILE_GLASS_SHATTER_VEL = 600.0f;
constexpr float GLASS_BLAST_RADIUS_CAP = 256.0f;
constexpr float GLASS_BLAST_DAMAGE_SCALE = 4.0f;
constexpr uint32_t GLASS_PROJECTILE_PANE_HOPS = 8;
constexpr uint16_t GLASS_IMPACT_DIR_NONE = 0xff;
constexpr float GLASS_ENCODE_SHATTER_SCALE = 0.123046875f;
constexpr float GLASS_ENCODE_SHATTER_BIAS = 31.5f;
constexpr float GLASS_ENCODE_SHATTER_ROUND = 0.5f;
constexpr float GLASS_ENCODE_SHATTER_CLAMP = 64.0f;

// Decode shatter scale is a f64->f32 transcribe from the retail constant bytes.
constexpr float GLASS_DECODE_SHATTER_SCALE = 8.126984596252441f;

constexpr size_t CG_GLASS_PIECE_LIMIT = 0x400;

constexpr float GLASS_COLLAPSE_SHORT_THRESHOLD = 0.30000001192092896f;
constexpr float GLASS_COLLAPSE_LONG_THRESHOLD = 0.699999988079071f;
constexpr float GLASS_COLLAPSE_SHORT_RANGE_MS = -57000.0f;
constexpr float GLASS_COLLAPSE_LONG_RANGE_MS = -540000.0f;
constexpr int32_t GLASS_COLLAPSE_SHORT_BASE_MS = 3000;
constexpr int32_t GLASS_COLLAPSE_LONG_BASE_MS = 60000;

enum class GlassPieceState : uint8_t {
    Intact = 0,
    Weakened = 1,
    Shattered = 2,
    Deleted = 3,
};
enum class GlassCause : uint8_t {
    Impact = 0,
    Blast = 1,
    Collapse = 2,
    Script = 3,
};

struct GlassShatterSeed {
    uint8_t impact_dir = 0;
    uint8_t impact_pos[2] = {0, 0};
};

struct GlassBreakRecord {
    int32_t break_tick = 0;
    uint64_t deterministic_seed = 0;
    std::optional<GlassShatterSeed> shatter_seed;
    GlassCause cause = GlassCause::Impact;
};

namespace {
uint64_t glass_mix_seed(uint32_t pane, int32_t at_time_ms,
                        const std::optional<GlassShatterSeed>& shatter_seed) {
    uint64_t seed = (static_cast<uint64_t>(pane) << 32) ^ (static_cast<uint64_t>(static_cast<uint32_t>(at_time_ms)));
    if (shatter_seed) {
        seed ^= static_cast<uint64_t>(shatter_seed->impact_dir) << 16;
        seed ^= static_cast<uint64_t>(shatter_seed->impact_pos[0]) << 8;
        seed ^= static_cast<uint64_t>(shatter_seed->impact_pos[1]);
    }
    if (seed == 0) seed = 0x9E3779B97F4A7C15ULL;
    return seed;
}
}

struct ClientGlassPiece {
    uint8_t applied = 0;
    uint8_t pending = 0;
    uint8_t impact_dir = 0;
    uint8_t impact_pos[2] = {0, 0};
};

GlassPieceState glass_state_from_damage(uint16_t damage);

struct ServerGlassPiece {
    uint16_t damage = 0;
    uint16_t collapse_time = 0;
    int32_t last_state_change_time = 0;
    uint8_t impact_dir = 0;
    uint8_t impact_pos[2] = {0, 0};
    uint8_t _gap_0b = 0;

    GlassPieceState state() const {
        return glass_state_from_damage(damage);
    }
    std::optional<GlassShatterSeed> shatter_seed() const {
        if (state() != GlassPieceState::Shattered) return std::nullopt;
        if (impact_dir == GLASS_IMPACT_DIR_NONE || impact_pos[0] > 0x3f || impact_pos[1] > 0x3f)
            return std::nullopt;
        return GlassShatterSeed{impact_dir, {impact_pos[0], impact_pos[1]}};
    }
};

struct GlassStateChange {
    GlassPieceState previous = GlassPieceState::Intact;
    GlassPieceState current = GlassPieceState::Intact;
};

enum class GlassApplyAction {
    None,
    Weaken,
    Shatter,
    Delete,
};

// GlassPaneBasis is currently defined in asset_core; for the standalone
// entity_iw4 port we forward-declare it and keep glass functions that need it
// behind a trait object until asset_core lands.
struct GlassPaneBasis {
    float origin[3] = {0, 0, 0};
    float axis_s[3] = {0, 0, 0};
    float axis_t[3] = {0, 0, 0};
};

inline GlassPieceState glass_state_from_damage_thresholds(uint16_t damage, uint16_t weaken, uint16_t destroy) {
    if (damage == GLASS_DAMAGE_INVALID) return GlassPieceState::Deleted;
    if (damage >= destroy) return GlassPieceState::Shattered;
    if (damage >= weaken) return GlassPieceState::Weakened;
    return GlassPieceState::Intact;
}
inline GlassPieceState glass_state_from_damage(uint16_t damage) {
    return glass_state_from_damage_thresholds(damage, GLASS_DAMAGE_TO_WEAKEN, GLASS_DAMAGE_TO_DESTROY);
}
inline uint16_t glass_add_damage(uint16_t current, uint32_t added) {
    const uint32_t sum = static_cast<uint32_t>(current) + added;
    if (sum < static_cast<uint32_t>(GLASS_DAMAGE_INVALID)) return static_cast<uint16_t>(sum);
    return GLASS_DAMAGE_ADD_CAP;
}
inline bool glass_is_solid_threshold(uint16_t damage, uint16_t destroy) {
    if (damage == GLASS_DAMAGE_INVALID) return false;
    return damage < destroy;
}
inline bool glass_is_solid(uint16_t damage) {
    return glass_is_solid_threshold(damage, GLASS_DAMAGE_TO_DESTROY);
}
inline uint8_t glass_encode_shatter_coord(float uv) {
    const float quantized = std::floor(uv * GLASS_ENCODE_SHATTER_SCALE + GLASS_ENCODE_SHATTER_BIAS + GLASS_ENCODE_SHATTER_ROUND);
    if (quantized >= GLASS_ENCODE_SHATTER_CLAMP) return 0x3f;
    if (quantized <= 0.0f) return 0;
    return static_cast<uint8_t>(quantized);
}
inline float glass_decode_shatter_coord(uint8_t quantized) {
    return (static_cast<float>(quantized) - GLASS_ENCODE_SHATTER_BIAS) * GLASS_DECODE_SHATTER_SCALE;
}

// Packed dir table is large; defined in glass_packed_dir.cpp.
const float* glass_packed_dir_table() noexcept;
inline void glass_packed_dir_to_vec(uint8_t index, float out[3]) {
    const float* table = glass_packed_dir_table();
    if (!table) { out[0]=out[1]=out[2]=0.0f; return; }
    const float* row = table + index * 3;
    out[0] = row[0]; out[1] = row[1]; out[2] = row[2];
}

inline void glass_shatter_impact_from_seed(const GlassPaneBasis& pane, uint8_t impact_dir,
                                           const uint8_t impact_pos[2], float out_hit[3], float out_dir[3]) {
    if (impact_dir == GLASS_IMPACT_DIR_NONE) {
        std::memcpy(out_hit, pane.origin, 3 * sizeof(float));
        out_dir[0] = out_dir[1] = out_dir[2] = 0.0f;
        return;
    }
    float dir[3];
    glass_packed_dir_to_vec(impact_dir, dir);
    const float u = glass_decode_shatter_coord(impact_pos[0]);
    const float v = glass_decode_shatter_coord(impact_pos[1]);
    out_hit[0] = pane.origin[0] + u * pane.axis_s[0] + v * pane.axis_t[0];
    out_hit[1] = pane.origin[1] + u * pane.axis_s[1] + v * pane.axis_t[1];
    out_hit[2] = pane.origin[2] + u * pane.axis_s[2] + v * pane.axis_t[2];
    std::memcpy(out_dir, dir, 3 * sizeof(float));
}

inline void glass_read_change(ClientGlassPiece& row, GlassPieceState pending,
                              const std::optional<GlassShatterSeed>& seed) {
    row.pending = static_cast<uint8_t>(pending);
    if (pending == GlassPieceState::Shattered) {
        if (seed) {
            row.impact_dir = seed->impact_dir;
            row.impact_pos[0] = seed->impact_pos[0];
            row.impact_pos[1] = seed->impact_pos[1];
        } else {
            row.impact_dir = GLASS_IMPACT_DIR_NONE;
            row.impact_pos[0] = 0;
            row.impact_pos[1] = 0;
        }
    }
}

inline GlassApplyAction glass_apply_state(ClientGlassPiece& row,
                                          const std::optional<GlassPaneBasis>& pane) {
    if (static_cast<int32_t>(row.applied) >= static_cast<int32_t>(row.pending)) return GlassApplyAction::None;
    if (row.pending == static_cast<uint8_t>(GlassPieceState::Deleted)) {
        row.applied = static_cast<uint8_t>(GlassPieceState::Deleted);
        return GlassApplyAction::Delete;
    }
    const bool weakened_first = row.pending > 0 && row.applied == 0;
    if (weakened_first && row.pending == static_cast<uint8_t>(GlassPieceState::Weakened)) {
        row.applied = static_cast<uint8_t>(GlassPieceState::Weakened);
        return GlassApplyAction::Weaken;
    }
    if (row.pending > static_cast<uint8_t>(GlassPieceState::Weakened)) {
        float hit[3], dir[3];
        if (pane) glass_shatter_impact_from_seed(*pane, row.impact_dir, row.impact_pos, hit, dir);
        else { hit[0]=hit[1]=hit[2]=0.0f; dir[0]=dir[1]=dir[2]=0.0f; }
        row.applied = row.pending;
        return GlassApplyAction::Shatter;
    }
    row.applied = row.pending;
    return GlassApplyAction::None;
}

inline void glass_update(ClientGlassPiece* rows, size_t count,
                         std::function<std::optional<GlassPaneBasis>(size_t)> pane_at) {
    for (size_t i = 0; i < count; ++i) {
        if (rows[i].applied < rows[i].pending) {
            glass_apply_state(rows[i], pane_at(i));
        }
    }
}

inline uint8_t glass_vec_to_packed_dir(const float dir[3]) {
    float best_dot = -std::numeric_limits<float>::infinity();
    uint8_t best = 0;
    const float* table = glass_packed_dir_table();
    if (!table) return 0;
    for (size_t i = 0; i < CG_GLASS_PIECE_LIMIT; ++i) {
        const float* row = table + i * 3;
        const float dot = dir[2]*row[2] + row[0]*dir[0] + row[1]*dir[1];
        if (best_dot < dot) { best_dot = dot; best = static_cast<uint8_t>(i); }
    }
    return best;
}

inline std::optional<GlassShatterSeed> glass_shatter_seed_from_hit(const GlassPaneBasis& pane,
                                                                   const float hit[3], const float dir[3]) {
    const float len2 = dir[2]*dir[2] + dir[0]*dir[0] + dir[1]*dir[1];
    if (len2 <= 0.0f) return std::nullopt;
    const float delta[3] = {hit[0]-pane.origin[0], hit[1]-pane.origin[1], hit[2]-pane.origin[2]};
    const float u = delta[0]*pane.axis_s[0] + delta[1]*pane.axis_s[1] + delta[2]*pane.axis_s[2];
    const float v = delta[0]*pane.axis_t[0] + delta[1]*pane.axis_t[1] + delta[2]*pane.axis_t[2];
    return GlassShatterSeed{
        glass_vec_to_packed_dir(dir),
        {glass_encode_shatter_coord(u), glass_encode_shatter_coord(v)}
    };
}

inline int32_t glass_ftol_round(float x) {
    if (x >= 0.0f) return static_cast<int32_t>(x + 0.5f);
    return -static_cast<int32_t>(-x + 0.5f);
}

inline uint16_t glass_weakened_collapse_time_cs(std::function<float()> next_random) {
    const float first = next_random();
    int32_t ms;
    if (first < GLASS_COLLAPSE_SHORT_THRESHOLD) {
        const float mag = next_random();
        ms = GLASS_COLLAPSE_SHORT_BASE_MS - glass_ftol_round(mag * GLASS_COLLAPSE_SHORT_RANGE_MS);
    } else if (first < GLASS_COLLAPSE_LONG_THRESHOLD) {
        const float mag = next_random();
        ms = GLASS_COLLAPSE_LONG_BASE_MS - glass_ftol_round(mag * GLASS_COLLAPSE_LONG_RANGE_MS);
    } else {
        return 0;
    }
    return static_cast<uint16_t>(ms / 100);
}
inline bool glass_collapse_due(const ServerGlassPiece& piece, int32_t level_time_ms) {
    if (piece.collapse_time == 0) return false;
    if (piece.damage == GLASS_DAMAGE_INVALID || piece.damage >= GLASS_DAMAGE_TO_DESTROY) return false;
    const int32_t deadline = static_cast<int32_t>(piece.collapse_time) * 100 + piece.last_state_change_time;
    return deadline < level_time_ms;
}
inline std::optional<GlassStateChange> glass_collapse_piece(ServerGlassPiece& piece, int32_t at_time_ms) {
    if (!glass_collapse_due(piece, at_time_ms)) return std::nullopt;
    const GlassPieceState prev = piece.state();
    piece.damage = GLASS_DAMAGE_INVALID;
    const GlassPieceState cur = piece.state();
    return GlassStateChange{prev, cur};
}

} // namespace entity_iw4
