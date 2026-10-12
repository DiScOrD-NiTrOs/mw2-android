// Port of iw4L crates/playerstate_iw4/src/playerstate.rs.
#pragma once

#include <array>
#include <cstdint>

namespace playerstate_iw4 {

constexpr int BREATH_HOLD_TIME_MS = 4500;
constexpr int BREATH_GASP_TIME_MS = 1000;

struct PlayerState {
    int32_t command_time = 0;
    int32_t pm_type = 0;
    int32_t pm_time = 0;
    uint32_t pm_flags = 0;
    uint32_t other_flags = 0;
    uint32_t link_flags = 0;
    int32_t bob_cycle = 0;
    float origin[3] = {0, 0, 0};
    float velocity[3] = {0, 0, 0};
    int32_t grenade_time_left = 0;
    int32_t throw_back_grenade_owner = 0;
    int32_t gravity = 0;
    float leanf = 0.0f;
    int32_t speed = 0;
    float delta_angles[3] = {0, 0, 0};
    int32_t ground_entity_num = 0;
    float v_ladder_vec[3] = {0, 0, 0};
    int32_t jump_time = 0;
    float jump_origin_z = 0.0f;
    int32_t legs_timer = 0;
    int32_t legs_anim = 0;
    int32_t torso_timer = 0;
    int32_t torso_anim = 0;
    int32_t damage_timer = 0;
    int32_t damage_duration = 0;
    int32_t flinch_yaw_anim = 0;
    int32_t corpse_index = 0;
    int32_t movement_dir = 0;
    uint32_t e_flags = 0;
    int32_t viewlocked = 0;
    int32_t viewlocked_ent_num = 0;
    int32_t event_sequence = 0;
    int32_t events_0 = 0;
    int32_t events_1 = 0;
    int32_t events_2 = 0;
    int32_t events_3 = 0;
    int32_t event_parms_0 = 0;
    int32_t event_parms_1 = 0;
    int32_t event_parms_2 = 0;
    int32_t event_parms_3 = 0;
    int32_t old_event_sequence = 0;
    float viewangles[3] = {0, 0, 0};
    int32_t view_height_target = 0;
    float view_height_current = 0.0f;
    int32_t view_height_lerp_time = 0;
    int32_t view_height_lerp_target = 0;
    int32_t view_height_lerp_down = 0;
    uint32_t damage_event = 0;
    uint32_t damage_yaw = 0;
    uint32_t damage_pitch = 0;
    int32_t damage_count = 0;
    uint32_t damage_flags = 0;
    int32_t health = 0;
    int32_t max_health = 0;
    float link_weapon_angles[3] = {0, 0, 0};
    int32_t cursor_hint = 0;
    int32_t cursor_hint_string = 0;
    int32_t cursor_hint_ent_index = 0;
    int32_t cursor_hint_dual_wield = 0;
    int32_t sprint_button_up_required = 0;
    int32_t sprint_delay = 0;
    int32_t last_sprint_start = 0;
    int32_t last_sprint_end = 0;
    int32_t sprint_start_max_length = 0;
    float move_speed_scale_multiplier = 0.0f;
    float mantle_yaw = 0.0f;
    int32_t mantle_timer = 0;
    int32_t mantle_trans_index = 0;
    uint32_t mantle_flags = 0;
    int32_t weap_anim = 0;
    int32_t weapon_time = 0;
    int32_t weapon_delay = 0;
    int32_t weapon_restrict_kick_time = 0;
    int32_t weaponstate_primary = 0;
    int32_t weap_hand_flags = 0;
    int32_t weapon_shot_count = 0;
    int32_t weap_anim_secondary = 0;
    int32_t weapon_time_secondary = 0;
    int32_t weapon_delay_secondary = 0;
    int32_t weapon_restrict_kick_time_secondary = 0;
    int32_t weaponstate_secondary = 0;
    int32_t weap_hand_flags_secondary = 0;
    int32_t weapon_shot_count_secondary = 0;
    int32_t weapons[15] = {};
    uint8_t weapon_data[76] = {};
    int32_t off_hand_index = 0;
    int32_t offhand_primary = 0;
    int32_t offhand_secondary = 0;
    uint32_t weapon = 0;
    uint32_t weapon_primary = 0;
    uint32_t weap_flags = 0;
    float f_weapon_pos_frac = 0.0f;
    uint32_t scope_zoom_level = 0;
    int32_t hold_breath_timer = 0;
    float hold_breath_scale = 1.0f;
    float aim_spread_scale = 0.0f;
    int32_t ads_delay_time = 0;
    int32_t spread_override = 0;
    int32_t spread_override_state = 0;
    int32_t last_weapon_hand = 0;
    uint8_t ammo[0x78] = {};
    uint8_t ammoclip[0xb4] = {};
    float melee_charge_yaw = 0.0f;
    int32_t melee_charge_dist = 0;
    int32_t melee_charge_time = 0;
    uint32_t perks[2] = {};
    uint32_t perk_slots[8] = {};
    int32_t action_slot_type[4] = {};
    int32_t action_slot_param[4] = {};
    int32_t shellshock_index = 0;
    int32_t shellshock_time = 0;
    int32_t shellshock_duration = 0;
    int32_t stun_time = 0;
    uint8_t objectives[0x380] = {};
    int32_t delta_time = 0;
    int32_t kill_cam_entity = 0;
    int32_t kill_cam_look_at_entity = 0;
    int32_t kill_cam_client_num = 0;
    int32_t recoil_scale = 0;

    static PlayerState zero();

    struct AnimPair {
        int32_t legs_anim = 0;
        int32_t torso_anim = 0;
    };
    AnimPair anim() const { return {legs_anim, torso_anim}; }
};

namespace eflags {
constexpr uint32_t TELEPORT = 0x2;
constexpr uint32_t DUCK = 0x4;
constexpr uint32_t PRONE = 0x8;
constexpr uint32_t NODRAW = 0x20;
constexpr uint32_t KILLCAM_PRESERVED = 0x80;
constexpr uint32_t TURRET_ACTIVE_PRONE = 0x400;
constexpr uint32_t TURRET_ACTIVE_DUCK = 0x800;
constexpr uint32_t RADAR_JAM = 0x200000;
} // namespace eflags

namespace other_flags {
constexpr uint32_t AC130 = 0x8000;
constexpr uint32_t EMP_JAMMED = 0x400;
constexpr uint32_t DEAD_KILLCAM_TPV = 0x800;
constexpr uint32_t PLAYER = 0x1000;
} // namespace other_flags

namespace pm_flags {
constexpr uint32_t PRONE = 0x1;
constexpr uint32_t CROUCH = 0x2;
constexpr uint32_t MANTLE = 0x4;
constexpr uint32_t LADDER = 0x8;
constexpr uint32_t ADS_INTENT = 0x10;
constexpr uint32_t BACKWARDS_RUN = 0x20;
constexpr uint32_t WALKING = 0x40;
constexpr uint32_t TIME_HARDLANDING = 0x80;
constexpr uint32_t PRONEMOVE_OVERRIDDEN = 0x200;
constexpr uint32_t LADDER_FALL = 0x1000;
constexpr uint32_t JUMPING = 0x2000;
constexpr uint32_t SPRINTING = 0x4000;
constexpr uint32_t SHELLSHOCKED = 0x8000;
constexpr uint32_t MELEE_CHARGE = 0x10000;
constexpr uint32_t SPRINT_BLOCKED = 0x00020000;
constexpr uint32_t LAST_STAND = 0x00400000;
} // namespace pm_flags

namespace weap_flags {
constexpr uint32_t HOLD_BREATH = 0x4;
constexpr uint32_t NIGHT_VISION = 0x40;
constexpr uint32_t OFFHAND_VIEW = 0x2;
constexpr uint32_t NO_ADS = 0x20;
constexpr uint32_t DOUBLEBARREL_RECOIL = 0x200;
constexpr uint32_t RECOIL_SCALE = 0x400;
} // namespace weap_flags

namespace mantle_flags {
constexpr uint32_t OVER = 1u << 0;
constexpr uint32_t ACTIVE = 1u << 3;
constexpr uint32_t QUICK = 1u << 4;
constexpr uint32_t FAST_MANTLE = 1u << 6;
} // namespace mantle_flags

constexpr int32_t ENTITYNUM_NONE = 0x7FF;

uint32_t get_viewmodel_weapon_index(const PlayerState& ps);

// perks --------------------------------------------------------------------

constexpr uint32_t PERK_PISTOLDEATH = 1u << 7;
constexpr uint32_t PERK_QUIETER = 1u << 8;
constexpr uint32_t PERK_FASTMANTLE = 1u << 19;
constexpr uint32_t PERK_SCAVENGER = 1u << 22;
constexpr uint32_t PERK_HOLDBREATH = 1u << 4;
constexpr int32_t PERK_EXTRA_BREATH_MS = 5000;
constexpr uint32_t PERK_QUICKDRAW = 1u << 26;
constexpr float PERK_QUICKDRAW_SPEED_SCALE = 1.5f;
constexpr uint32_t PERK_COLDBLOODED = 1u << 27;
constexpr uint32_t PERK_HEARTBREAKER = 1u << 28;
constexpr uint32_t PERK_SELECTIVEHEARING = 1u << 29;
constexpr uint32_t PERK1_FASTSPRINTRECOVERY = 1u << 2;
constexpr uint32_t PERK1_EXTENDEDMELEE = 1u << 3;
constexpr float EXTENDED_MELEE_TARGET_RANGE = 192.0f;
constexpr float SPRINT_RECOVERY_MULTIPLIER = 0.6f;
constexpr uint32_t PERK1_SPYGAME = 1u;

int32_t breath_hold_time_ms(const PlayerState& ps);

// usercmd.rs ----------------------------------------------------------------

struct UserCmd {
    int32_t server_time = 0;
    uint32_t buttons = 0;
    int32_t angles[3] = {0, 0, 0};
    float gun_angle_offset[2] = {0, 0};
    uint16_t weapon = 0;
    uint16_t weapon_mapped = 0;
    uint16_t off_hand_index = 0;
    int8_t forwardmove = 0;
    int8_t rightmove = 0;
    float melee_charge_yaw = 0.0f;
    uint8_t melee_charge_dist = 0;
    uint8_t selected_location[3] = {};
    uint8_t remote_control[2] = {};
};

namespace buttons {
constexpr uint32_t ATTACK = 0x1;
constexpr uint32_t SPRINT = 1u << 1;
constexpr uint32_t MELEE_CHARGE = 0x4;
constexpr uint32_t USE = 0x8;
constexpr uint32_t RELOAD = 0x10;
constexpr uint32_t USE_RELOAD = 0x20;
constexpr uint32_t PRONE = 0x100;
constexpr uint32_t CROUCH = 0x200;
constexpr uint32_t JUMP = 0x400;
constexpr uint32_t ADS = 0x800;
constexpr uint32_t STANCE_HELD = 0x1000;
constexpr uint32_t BREATH = 0x2000;
constexpr uint32_t FRAG = 0x4000;
constexpr uint32_t SMOKE = 0x8000;
constexpr uint32_t LOCATION_SELECT = 0x10000;
constexpr uint32_t LOCATION_CANCEL = 0x20000;
constexpr uint32_t THROW = 0x80000;
constexpr uint32_t REMOTE_CONTROL = 0x100000;
constexpr uint32_t OFFHAND_HOLD_CANCEL = 0x200000;
constexpr uint32_t CHANGE_ZOOM = 1u << 26;
constexpr uint32_t SPRINT_INTERFERING = 0xcc35;
} // namespace buttons

} // namespace playerstate_iw4
