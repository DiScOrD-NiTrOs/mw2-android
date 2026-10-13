// Port of iw4L crates/input_iw4/src/lib.rs (public surface) + names.rs.
#pragma once

#include "playerstate_iw4/playerstate_iw4.h"

#include <array>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <vector>

namespace input_iw4 {

using namespace playerstate_iw4;

// adjust_angles.rs constants -------------------------------------------------
constexpr float CL_YAWSPEED_DEFAULT = 140.0f;
constexpr float CL_PITCHSPEED_DEFAULT = 140.0f;
constexpr float CL_ANGLESPEEDKEY_DEFAULT = 1.5f;

struct AdjustAnglesInput {
    float dt = 0.0f;
    int32_t now_msec = 0;
    uint32_t frame_msec = 0;
    float cl_yawspeed = CL_YAWSPEED_DEFAULT;
    float cl_pitchspeed = CL_PITCHSPEED_DEFAULT;
    float cl_anglespeedkey = CL_ANGLESPEEDKEY_DEFAULT;
    float cgame_max_yaw_speed = 0.0f;
    float cgame_max_pitch_speed = 0.0f;
    bool frozen = false;
};

// lib.rs constants -----------------------------------------------------------
constexpr float ANGLE2SHORT = 65536.0f / 360.0f;
constexpr size_t KEY_COUNT = 256;
constexpr int32_t SCRIPT_KEYNUM = 0x400;

// lib.rs button state ---------------------------------------------------------
struct Kbutton {
    uint64_t down[5] = {};
    int32_t downtime = 0;
    uint32_t msec = 0;
    bool active = false;
    bool was_pressed = false;
};

void in_key_down(Kbutton& btn, int32_t key, int32_t extra_time);
void in_key_up(Kbutton& btn, int32_t uptime, int32_t key, uint32_t frame_msec);
float key_state(Kbutton& btn, int32_t now_msec, uint32_t frame_msec);

// lib.rs KeyState ------------------------------------------------------------
struct KeyState {
    int32_t down = 0;
    int32_t repeats = 0;
    uint32_t binding = 0;
};

// lib.rs KbuttonSet ---------------------------------------------------------
struct KbuttonSet {
    Kbutton attack;
    Kbutton melee;
    Kbutton frag;
    Kbutton smoke;
    Kbutton usereload;
    Kbutton speed;
    Kbutton gostand;
    Kbutton forward;
    Kbutton back;
    Kbutton moveleft;
    Kbutton moveright;
    Kbutton movedown;
    Kbutton left;
    Kbutton right;
    Kbutton lookup;
    Kbutton lookdown;
    Kbutton strafe;
    Kbutton holdbreath;
    Kbutton changezoom;
    Kbutton activate;
    Kbutton reload;
    Kbutton prone;
    Kbutton mlook;
    Kbutton throw_btn;
    Kbutton sprint;
    Kbutton scores;
    Kbutton talk;
    Kbutton stance;
};

void kbuttonset_visit_active_names(const KbuttonSet& kb, void (*f)(const char*));
void kbuttonset_clear_was_pressed(KbuttonSet& kb);

// lib.rs ClientInput ---------------------------------------------------------
struct ClientInput {
    std::array<KeyState, KEY_COUNT> keys;
    KbuttonSet kb;
    bool using_ads = false;
    int32_t stance_latch = 0;
    std::array<char, 64> weapon_cycles = {};
    std::size_t weapon_cycles_count = 0;
    std::array<int, 64> action_slots = {};
    std::size_t action_slots_count = 0;
    bool offhand_hold_cancel = false;
    struct StanceHeld {
        int32_t from = 0;
        int32_t since = 0;
    };
    StanceHeld stance_held;
    bool stance_held_valid = false;
    bool center_view = false;
    bool airborne = false;
};

// names.rs -------------------------------------------------------------------
constexpr std::size_t INPUT_COMMAND_NAMES_LEN = 83;
// Public view of the bind-name table. This is always the same underlying data
// as the internal NAMES_TABLE in input_iw4.cpp, exposed so consumers don't need
// to know about the internal copy.
extern const char* const* const INPUT_COMMAND_NAMES;
const char* input_command_name(uint32_t id);
uint32_t command_id_from_name(const char* name);
uint32_t command_id_lookup(const char* name);
uint32_t key_up_command_id(uint32_t binding);
void command_names_for_each(void (*cb)(const char*));

constexpr uint32_t HOLD_PAIR_LIMIT = 0x41;

// stance.rs -------------------------------------------------------------------
constexpr int32_t STANCE_HOLD_MS = 300;

// lib.rs dispatch ------------------------------------------------------------
void clientinput_set_ads(ClientInput& client, bool ads);
void clientinput_input_cmd(ClientInput& client, uint32_t cmd_id, int32_t key,
                           int32_t now_msec, uint32_t frame_msec);
void clientinput_key_event(ClientInput& client, std::size_t key_num, bool down,
                           int32_t now_msec, uint32_t frame_msec);

// lib.rs move / button helpers -----------------------------------------------
std::pair<float, float> apply_mouse_sensitivity(float mx, float my, float mouse_counts,
                                                float frame_msec, float sensitivity,
                                                float mouse_accel, float fov_scale);
std::pair<int32_t, int32_t> mouse_move_angles(float mx, float my, float m_yaw, float m_pitch);
void update_cmd_button(const Kbutton& btn, uint32_t bit, uint32_t& out_buttons);
uint32_t cmd_buttons(const KbuttonSet& state);
uint32_t key_move_ads_bit(const Kbutton& speed, bool using_ads);
uint32_t key_move_bits(const KbuttonSet& kb, bool using_ads, uint32_t bits);

struct MoveAxes {
    float forward = 0.0f;
    float right = 0.0f;
};
MoveAxes key_move_from_fractions(float forward, float back, float right, float left);
int8_t axis_to_move(float axis);

struct CreateCmdInput {
    int32_t server_time = 0;
    std::array<int32_t, 3> angles = {};
    uint32_t buttons = 0;
    int8_t forwardmove = 0;
    int8_t rightmove = 0;
    int32_t mouse_pitch_delta = 0;
    int32_t mouse_yaw_delta = 0;
    int32_t key_pitch_delta = 0;
    int32_t key_yaw_delta = 0;
    bool frozen = false;
};

UserCmd create_cmd(const CreateCmdInput& input);

struct SampleMoveOutput {
    uint32_t buttons = 0;
    MoveAxes axes{};
    int32_t stance_latch = 0;
};
SampleMoveOutput sample_move(ClientInput& client, int32_t now_msec, uint32_t frame_msec);

// weapon_select.rs -----------------------------------------------------------
bool weapon_cycle_allowed(const PlayerState& ps, int32_t time, int32_t select_time,
                          int32_t select_time_override, int32_t cycle_delay);
std::optional<uint32_t> cycle_weapon(const std::array<int32_t, 15>& weapons, uint32_t selected,
                                     uint32_t mapped, uint32_t last_primary, bool next,
                                     int32_t (*inventory_type)(uint32_t));

// adjust_angles.rs -----------------------------------------------------------
std::pair<float, float> adjust_angles(KbuttonSet& kb, const AdjustAnglesInput& input);

// From lib.rs (used by a few ports / tests)
constexpr uint32_t CMD_RING_MASK = 0x7f;

} // namespace input_iw4
