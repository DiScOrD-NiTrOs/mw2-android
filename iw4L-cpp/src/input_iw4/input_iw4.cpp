// Port of iw4L crates/input_iw4/src/{lib,names,adjust_angles,weapon_select}.rs.
#include "input_iw4/input_iw4.h"
#include "playerstate_iw4/playerstate_iw4.h"

#include <cstring>
#include <cmath>
#include <algorithm>
#include <optional>
#include <cctype>
#include <vector>

namespace input_iw4 {

using namespace playerstate_iw4;

// ---- names.rs table ---------------------------------------------------------
namespace {
const char* const NAMES_TABLE[INPUT_COMMAND_NAMES_LEN] = {
    "",
    "+attack",
    "-attack",
    "+melee",
    "-melee",
    "+frag",
    "-frag",
    "+smoke",
    "-smoke",
    "+breath_sprint",
    "-breath_sprint",
    "+usereload",
    "-usereload",
    "+speed_throw",
    "-speed_throw",
    "+actionslot 1",
    "-actionslot 1",
    "+actionslot 2",
    "-actionslot 2",
    "+actionslot 3",
    "-actionslot 3",
    "+actionslot 4",
    "-actionslot 4",
    "+stance",
    "-stance",
    "+gostand",
    "-gostand",
    "+forward",
    "-forward",
    "+back",
    "-back",
    "+moveleft",
    "-moveleft",
    "+moveright",
    "-moveright",
    "+movedown",
    "-movedown",
    "+left",
    "-left",
    "+right",
    "-right",
    "+lookup",
    "-lookup",
    "+lookdown",
    "-lookdown",
    "+strafe",
    "-strafe",
    "+holdbreath",
    "-holdbreath",
    "+activate",
    "-activate",
    "+reload",
    "-reload",
    "+prone",
    "-prone",
    "+mlook",
    "-mlook",
    "+toggleads_throw",
    "-toggleads_throw",
    "+sprint",
    "-sprint",
    "+scores",
    "-scores",
    "+talk",
    "-talk",
    "togglemenu",
    "weapnext",
    "pause",
    "chatmodepublic",
    "chatmodeteam",
    "weapprev",
    "centerview",
    "togglecrouch",
    "toggleprone",
    "goprone",
    "gocrouch",
    "toggleads",
    "leaveads",
    "",
    "+changezoom",
    "-changezoom",
    "+melee_zoom",
    "-melee_zoom",
};

uint32_t name_to_id(const char* name) {
    for (std::size_t i = 0; i < INPUT_COMMAND_NAMES_LEN; ++i) {
        if (std::strcmp(NAMES_TABLE[i], name) == 0) return static_cast<uint32_t>(i);
    }
    return 0;
}
} // namespace

const char* input_command_name(uint32_t id) {
    if (id >= INPUT_COMMAND_NAMES_LEN) return nullptr;
    const char* s = NAMES_TABLE[id];
    return s[0] == '\0' ? nullptr : s;
}

uint32_t command_id_from_name(const char* name) {
    uint32_t id = name_to_id(name);
    return id == 0 ? 0 : id;
}

uint32_t command_id_lookup(const char* name) {
    if (name == nullptr || name[0] == '\0') return 0;
    char buf[32];
    std::size_t n = std::strlen(name);
    if (n > sizeof(buf)) n = sizeof(buf);
    for (std::size_t i = 0; i < n; ++i) {
        buf[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));
    }
    buf[n] = '\0';
    uint32_t id = name_to_id(buf);
    if (id != 0) return id;
    if (buf[0] == '+' || buf[0] == '-') return 0;
    if (n + 1 >= sizeof(buf)) return 0;
    std::memmove(buf + 1, buf, n);
    buf[0] = '+';
    buf[n + 1] = '\0';
    return name_to_id(buf);
}

uint32_t key_up_command_id(uint32_t binding) {
    if (binding == 0) return 0;
    if ((binding < HOLD_PAIR_LIMIT && (binding & 1u) != 0) || binding == 79 || binding == 81)
        return binding + 1;
    return 0;
}

void command_names_for_each(void (*cb)(const char*)) {
    for (std::size_t i = 0; i < INPUT_COMMAND_NAMES_LEN; ++i)
        if (NAMES_TABLE[i][0] != '\0') cb(NAMES_TABLE[i]);
}

const char* const* const INPUT_COMMAND_NAMES = NAMES_TABLE;

// ---- lib.rs button state ----------------------------------------------------
namespace {
struct HeldKey { std::size_t slot; uint64_t mask; };

HeldKey held_key(int32_t key) {
    auto idx = static_cast<std::size_t>(key);
    if (key == SCRIPT_KEYNUM) {
        // SCRIPT_KEYNUM (0x400) is treated as index KEY_COUNT for slot/mask purposes.
        idx = KEY_COUNT;
    } else if (idx >= KEY_COUNT) {
        return {KEY_COUNT, 0};
    }
    return {idx / 64, UINT64_C(1) << (idx % 64)};
}
} // namespace

void in_key_down(Kbutton& btn, int32_t key, int32_t extra_time) {
    HeldKey hk = held_key(key);
    if (hk.mask == 0) return;
    if ((btn.down[hk.slot] & hk.mask) != 0) return;
    btn.down[hk.slot] |= hk.mask;
    if (!btn.active) {
        btn.downtime = extra_time;
        btn.active = true;
        btn.was_pressed = true;
    }
}

void in_key_up(Kbutton& btn, int32_t uptime, int32_t key, uint32_t frame_msec) {
    HeldKey hk = held_key(key);
    if (hk.mask == 0) return;
    if ((btn.down[hk.slot] & hk.mask) == 0) return;
    btn.down[hk.slot] &= ~hk.mask;
    bool any = false;
    for (int i = 0; i < 5; ++i)
        if (btn.down[i] != 0) { any = true; break; }
    if (!any && btn.active) {
        if (btn.downtime != 0) {
            auto delta = uptime - btn.downtime;
            if (delta > 0) btn.msec = btn.msec + static_cast<uint32_t>(delta);
        } else {
            btn.msec = btn.msec + (frame_msec >> 1);
        }
        btn.active = false;
        btn.downtime = 0;
    }
}

float key_state(Kbutton& btn, int32_t now_msec, uint32_t frame_msec) {
    uint32_t sample = btn.msec;
    btn.msec = 0;
    if (btn.active) {
        if (btn.downtime != 0) {
            auto add = now_msec - btn.downtime;
            if (add > 0) sample = sample + static_cast<uint32_t>(add);
        }
        btn.downtime = now_msec;
    }
    if (sample == 0 || frame_msec == 0) return 0.0f;
    if (sample < frame_msec) return static_cast<float>(sample) / static_cast<float>(frame_msec);
    return 1.0f;
}

// ---- lib.rs KbuttonSet helpers ----------------------------------------------
void kbuttonset_visit_active_names(const KbuttonSet& kb, void (*f)(const char*)) {
    if (kb.attack.active || kb.attack.was_pressed) f("+attack");
    if (kb.melee.active || kb.melee.was_pressed) f("+melee");
    if (kb.changezoom.active || kb.changezoom.was_pressed) f("+changezoom");
    if (kb.frag.active || kb.frag.was_pressed) f("+frag");
    if (kb.smoke.active || kb.smoke.was_pressed) f("+smoke");
    if (kb.usereload.active || kb.usereload.was_pressed) f("+usereload");
    if (kb.speed.active || kb.speed.was_pressed) f("+speed_throw");
    if (kb.gostand.active || kb.gostand.was_pressed) f("+gostand");
    if (kb.forward.active || kb.forward.was_pressed) f("+forward");
    if (kb.back.active || kb.back.was_pressed) f("+back");
    if (kb.moveleft.active || kb.moveleft.was_pressed) f("+moveleft");
    if (kb.moveright.active || kb.moveright.was_pressed) f("+moveright");
    if (kb.movedown.active || kb.movedown.was_pressed) f("+movedown");
    if (kb.left.active || kb.left.was_pressed) f("+left");
    if (kb.right.active || kb.right.was_pressed) f("+right");
    if (kb.lookup.active || kb.lookup.was_pressed) f("+lookup");
    if (kb.lookdown.active || kb.lookdown.was_pressed) f("+lookdown");
    if (kb.activate.active || kb.activate.was_pressed) f("+activate");
    if (kb.reload.active || kb.reload.was_pressed) f("+reload");
    if (kb.prone.active || kb.prone.was_pressed) f("+prone");
    if (kb.sprint.active || kb.sprint.was_pressed) f("+sprint");
}

void kbuttonset_clear_was_pressed(KbuttonSet& kb) {
    kb.stance.was_pressed = false;
    kb.attack.was_pressed = false;
    kb.melee.was_pressed = false;
    kb.frag.was_pressed = false;
    kb.smoke.was_pressed = false;
    kb.usereload.was_pressed = false;
    kb.speed.was_pressed = false;
    kb.gostand.was_pressed = false;
    kb.forward.was_pressed = false;
    kb.back.was_pressed = false;
    kb.moveleft.was_pressed = false;
    kb.moveright.was_pressed = false;
    kb.movedown.was_pressed = false;
    kb.left.was_pressed = false;
    kb.right.was_pressed = false;
    kb.lookup.was_pressed = false;
    kb.lookdown.was_pressed = false;
    kb.strafe.was_pressed = false;
    kb.holdbreath.was_pressed = false;
    kb.changezoom.was_pressed = false;
    kb.activate.was_pressed = false;
    kb.reload.was_pressed = false;
    kb.prone.was_pressed = false;
    kb.mlook.was_pressed = false;
    kb.throw_btn.was_pressed = false;
    kb.sprint.was_pressed = false;
    kb.scores.was_pressed = false;
    kb.talk.was_pressed = false;
}

// ---- lib.rs ClientInput dispatch --------------------------------------------
namespace {
static bool pair_down(uint32_t cmd_id) { return key_up_command_id(cmd_id) != 0; }

static void apply_pair(Kbutton& btn, uint32_t cmd_id, int32_t key,
                       int32_t now_msec, uint32_t frame_msec) {
    if (pair_down(cmd_id))
        in_key_down(btn, key, now_msec);
    else
        in_key_up(btn, now_msec, key, frame_msec);
}

static void set_stance_held(ClientInput& client, int32_t from, int32_t now_msec) {
    client.stance_held.from = from;
    client.stance_held.since = now_msec;
    client.stance_held_valid = true;
}

static void clear_stance_held(ClientInput& client) { client.stance_held_valid = false; }

static void update_stance_held(ClientInput& client, int32_t now_msec) {
    client.stance_held.since = now_msec;
}

static void stance_button(ClientInput& client, bool down, int32_t now_msec) {
    if (client.kb.prone.active || client.kb.movedown.active) return;
    auto crouch = static_cast<int32_t>(buttons::CROUCH);
    if (down) {
        set_stance_held(client, client.stance_latch, now_msec);
        client.stance_latch = crouch;
    } else if (client.stance_held_valid && client.stance_held.from == crouch) {
        client.stance_latch = 0;
        clear_stance_held(client);
    }
}

static void stance_hold(ClientInput& client, int32_t now_msec) {
    if (!client.stance_held_valid) return;
    if (client.airborne) {
        update_stance_held(client, now_msec);
        return;
    }
    auto diff = now_msec - client.stance_held.since;
    if (diff >= STANCE_HOLD_MS) {
        auto prone = static_cast<int32_t>(buttons::PRONE);
        client.stance_latch = (client.stance_held.from == prone) ? 0 : prone;
        clear_stance_held(client);
    }
}
} // namespace

void clientinput_input_cmd(ClientInput& client, uint32_t cmd_id, int32_t key,
                           int32_t now_msec, uint32_t frame_msec) {
    switch (cmd_id) {
    case 1:
    case 2:
        apply_pair(client.kb.attack, cmd_id, key, now_msec, frame_msec);
        break;
    case 3:
    case 4:
        apply_pair(client.kb.melee, cmd_id, key, now_msec, frame_msec);
        break;
    case 5:
    case 6:
        apply_pair(client.kb.frag, cmd_id, key, now_msec, frame_msec);
        break;
    case 7:
    case 8:
        apply_pair(client.kb.smoke, cmd_id, key, now_msec, frame_msec);
        break;
    case 9:
    case 10:
        apply_pair(client.kb.holdbreath, cmd_id, key, now_msec, frame_msec);
        apply_pair(client.kb.sprint, cmd_id, key, now_msec, frame_msec);
        break;
    case 11:
    case 12:
        apply_pair(client.kb.usereload, cmd_id, key, now_msec, frame_msec);
        break;
    case 13:
    case 14:
        if (pair_down(cmd_id)) client.using_ads = false;
        apply_pair(client.kb.speed, cmd_id, key, now_msec, frame_msec);
        apply_pair(client.kb.throw_btn, cmd_id, key, now_msec, frame_msec);
        break;
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
        if (pair_down(cmd_id) && client.action_slots_count < 64)
            client.action_slots[client.action_slots_count++] = static_cast<int>((cmd_id - 15) / 2);
        break;
    case 23:
    case 24: {
        bool was_active = client.kb.stance.active;
        apply_pair(client.kb.stance, cmd_id, key, now_msec, frame_msec);
        if (was_active != client.kb.stance.active)
            stance_button(client, client.kb.stance.active, now_msec);
        break;
    }
    case 25:
    case 26:
        apply_pair(client.kb.gostand, cmd_id, key, now_msec, frame_msec);
        break;
    case 27:
    case 28:
        apply_pair(client.kb.forward, cmd_id, key, now_msec, frame_msec);
        break;
    case 29:
    case 30:
        apply_pair(client.kb.back, cmd_id, key, now_msec, frame_msec);
        break;
    case 31:
    case 32:
        apply_pair(client.kb.moveleft, cmd_id, key, now_msec, frame_msec);
        break;
    case 33:
    case 34:
        apply_pair(client.kb.moveright, cmd_id, key, now_msec, frame_msec);
        break;
    case 35:
    case 36:
        apply_pair(client.kb.movedown, cmd_id, key, now_msec, frame_msec);
        break;
    case 37:
    case 38:
        apply_pair(client.kb.left, cmd_id, key, now_msec, frame_msec);
        break;
    case 39:
    case 40:
        apply_pair(client.kb.right, cmd_id, key, now_msec, frame_msec);
        break;
    case 41:
    case 42:
        apply_pair(client.kb.lookup, cmd_id, key, now_msec, frame_msec);
        break;
    case 43:
    case 44:
        apply_pair(client.kb.lookdown, cmd_id, key, now_msec, frame_msec);
        break;
    case 45:
    case 46:
        apply_pair(client.kb.strafe, cmd_id, key, now_msec, frame_msec);
        break;
    case 79:
    case 80:
        apply_pair(client.kb.changezoom, cmd_id, key, now_msec, frame_msec);
        break;
    case 81:
    case 82:
        apply_pair(client.kb.changezoom, cmd_id, key, now_msec, frame_msec);
        apply_pair(client.kb.melee, cmd_id, key, now_msec, frame_msec);
        break;
    case 47:
    case 48:
        apply_pair(client.kb.holdbreath, cmd_id, key, now_msec, frame_msec);
        break;
    case 49:
    case 50:
        apply_pair(client.kb.activate, cmd_id, key, now_msec, frame_msec);
        break;
    case 51:
    case 52:
        apply_pair(client.kb.reload, cmd_id, key, now_msec, frame_msec);
        break;
    case 53:
    case 54:
        apply_pair(client.kb.prone, cmd_id, key, now_msec, frame_msec);
        break;
    case 55:
    case 56:
        apply_pair(client.kb.mlook, cmd_id, key, now_msec, frame_msec);
        break;
    case 57:
    case 58:
        if (pair_down(cmd_id)) client.using_ads = !client.using_ads;
        apply_pair(client.kb.throw_btn, cmd_id, key, now_msec, frame_msec);
        break;
    case 59:
    case 60:
        apply_pair(client.kb.sprint, cmd_id, key, now_msec, frame_msec);
        break;
    case 61:
    case 62:
        apply_pair(client.kb.scores, cmd_id, key, now_msec, frame_msec);
        break;
    case 63:
    case 64:
        apply_pair(client.kb.talk, cmd_id, key, now_msec, frame_msec);
        break;
    case 65:
    case 67:
    case 68:
    case 69:
        break;
    case 66:
    case 70:
        if (client.weapon_cycles_count < 64)
            client.weapon_cycles[client.weapon_cycles_count++] = (cmd_id == 66 ? 1 : 0);
        break;
    case 71:
        client.center_view = true;
        break;
    case 72:
    case 73: {
        auto stance = (cmd_id == 72) ? static_cast<int32_t>(buttons::CROUCH)
                                      : static_cast<int32_t>(buttons::PRONE);
        client.stance_latch = (client.stance_latch == stance) ? 0 : stance;
        break;
    }
    case 74:
        client.stance_latch = static_cast<int32_t>(buttons::PRONE);
        break;
    case 75:
        client.stance_latch = static_cast<int32_t>(buttons::CROUCH);
        break;
    case 76:
        client.using_ads = !client.using_ads;
        break;
    case 77:
        client.using_ads = false;
        break;
    default:
        break;
    }
}

void clientinput_set_ads(ClientInput& client, bool ads) { client.using_ads = ads; }

void clientinput_key_event(ClientInput& client, std::size_t key_num, bool down,
                           int32_t now_msec, uint32_t frame_msec) {
    if (key_num >= KEY_COUNT) return;
    if (down) {
        client.keys[key_num].down = 1;
        client.keys[key_num].repeats = client.keys[key_num].repeats + 1;
        auto id = client.keys[key_num].binding;
        if (id != 0)
            clientinput_input_cmd(client, id, static_cast<int32_t>(key_num), now_msec, frame_msec);
    } else {
        client.keys[key_num].down = 0;
        client.keys[key_num].repeats = 0;
        auto id = client.keys[key_num].binding;
        auto up_id = key_up_command_id(id);
        if (up_id != 0)
            clientinput_input_cmd(client, up_id, static_cast<int32_t>(key_num), now_msec,
                                  frame_msec);
    }
}

// ---- lib.rs move / button helpers ------------------------------------------
std::pair<float, float> apply_mouse_sensitivity(float mx, float my, float mouse_counts,
                                                float frame_msec, float sensitivity,
                                                float mouse_accel, float fov_scale) {
    float frame = (frame_msec > 0.0f) ? frame_msec : 1.0f;
    float rate = mouse_counts / frame;
    float scale = (rate * mouse_accel + sensitivity) * fov_scale;
    return {mx * scale, my * scale};
}

std::pair<int32_t, int32_t> mouse_move_angles(float mx, float my, float m_yaw, float m_pitch) {
    auto pitch = static_cast<int32_t>(my * m_pitch * ANGLE2SHORT);
    auto yaw = static_cast<int32_t>(-mx * m_yaw * ANGLE2SHORT);
    return {pitch, yaw};
}

void update_cmd_button(const Kbutton& btn, uint32_t bit, uint32_t& out_buttons) {
    if (btn.active || btn.was_pressed) out_buttons |= bit;
}

uint32_t cmd_buttons(const KbuttonSet& state) {
    uint32_t bits = 0;
    update_cmd_button(state.attack, buttons::ATTACK, bits);
    update_cmd_button(state.holdbreath, buttons::BREATH, bits);
    update_cmd_button(state.changezoom, buttons::CHANGE_ZOOM, bits);
    update_cmd_button(state.frag, buttons::FRAG, bits);
    update_cmd_button(state.smoke, buttons::SMOKE, bits);
    update_cmd_button(state.melee, buttons::MELEE_CHARGE, bits);
    update_cmd_button(state.activate, buttons::USE, bits);
    update_cmd_button(state.reload, buttons::RELOAD, bits);
    update_cmd_button(state.usereload, buttons::USE_RELOAD, bits);
    update_cmd_button(state.movedown, buttons::CROUCH, bits);
    update_cmd_button(state.prone, buttons::PRONE, bits);
    update_cmd_button(state.gostand, buttons::JUMP, bits);
    update_cmd_button(state.throw_btn, buttons::THROW, bits);
    return bits;
}

uint32_t key_move_ads_bit(const Kbutton& speed, bool using_ads) {
    return (speed.active != using_ads) ? buttons::ADS : 0u;
}

uint32_t key_move_bits(const KbuttonSet& kb, bool using_ads, uint32_t bits) {
    if (kb.prone.active) {
        bits &= ~buttons::CROUCH;
        bits |= buttons::PRONE | buttons::STANCE_HELD;
    } else if (kb.movedown.active) {
        bits &= ~buttons::PRONE;
        bits |= buttons::CROUCH | buttons::STANCE_HELD;
    } else {
        bits &= ~(buttons::PRONE | buttons::CROUCH | buttons::STANCE_HELD);
    }
    if (key_move_ads_bit(kb.speed, using_ads) != 0)
        bits |= buttons::ADS;
    else
        bits &= ~buttons::ADS;
    if (kb.sprint.active || kb.sprint.was_pressed) {
        if (!kb.back.active) bits |= buttons::SPRINT;
    } else {
        bits &= ~buttons::SPRINT;
    }
    return bits;
}

MoveAxes key_move_from_fractions(float forward, float back, float right, float left) {
    auto f = (forward - back);
    if (f > 1.0f) f = 1.0f;
    if (f < -1.0f) f = -1.0f;
    auto r = (right - left);
    if (r > 1.0f) r = 1.0f;
    if (r < -1.0f) r = -1.0f;
    return {f, r};
}

int8_t axis_to_move(float axis) {
    auto v = axis * 127.0f;
    auto rounded = (v >= 0.0f) ? static_cast<int32_t>(v + 0.5f) : static_cast<int32_t>(v - 0.5f);
    if (rounded > 127) return 127;
    if (rounded < -127) return -127;
    return static_cast<int8_t>(rounded);
}

// ---- lib.rs create_cmd -----------------------------------------------------
UserCmd create_cmd(const CreateCmdInput& input) {
    if (input.frozen) {
        UserCmd cmd;
        cmd.server_time = input.server_time;
        cmd.buttons = 0;
        std::memcpy(cmd.angles, input.angles.data(), sizeof(cmd.angles));
        cmd.weapon = 0;
        cmd.weapon_mapped = 0;
        cmd.off_hand_index = 0;
        cmd.forwardmove = 0;
        cmd.rightmove = 0;
        cmd.melee_charge_yaw = 0.0f;
        cmd.melee_charge_dist = 0;
        cmd.selected_location[0] = 0;
        cmd.selected_location[1] = 0;
        cmd.selected_location[2] = 0;
        cmd.remote_control[0] = 0;
        cmd.remote_control[1] = 0;
        cmd.gun_angle_offset[0] = 0.0f;
        cmd.gun_angle_offset[1] = 0.0f;
        return cmd;
    }
    int32_t angles[3];
    std::memcpy(angles, input.angles.data(), sizeof(angles));
    angles[0] = angles[0] + input.key_pitch_delta + input.mouse_pitch_delta;
    angles[1] = angles[1] + input.key_yaw_delta + input.mouse_yaw_delta;
    UserCmd cmd;
    cmd.server_time = input.server_time;
    cmd.buttons = input.buttons;
    std::memcpy(cmd.angles, angles, sizeof(cmd.angles));
    cmd.weapon = 0;
    cmd.weapon_mapped = 0;
    cmd.off_hand_index = 0;
    cmd.forwardmove = input.forwardmove;
    cmd.rightmove = input.rightmove;
    cmd.melee_charge_yaw = 0.0f;
    cmd.melee_charge_dist = 0;
    cmd.selected_location[0] = 0;
    cmd.selected_location[1] = 0;
    cmd.selected_location[2] = 0;
    cmd.remote_control[0] = 0;
    cmd.remote_control[1] = 0;
    cmd.gun_angle_offset[0] = 0.0f;
    cmd.gun_angle_offset[1] = 0.0f;
    return cmd;
}

// ---- lib.rs sample_move -----------------------------------------------------
static float movement_key_state(Kbutton& btn, int32_t now_msec, uint32_t frame_msec) {
    auto fraction = key_state(btn, now_msec, frame_msec);
    return btn.active ? 1.0f : fraction;
}

SampleMoveOutput sample_move(ClientInput& client, int32_t now_msec, uint32_t frame_msec) {
    stance_hold(client, now_msec);
    auto bits = key_move_bits(client.kb, client.using_ads, cmd_buttons(client.kb));
    if (client.kb.gostand.was_pressed) {
        client.stance_latch = 0;
        clear_stance_held(client);
    }
    if ((bits & buttons::STANCE_HELD) == 0) bits |= static_cast<uint32_t>(client.stance_latch);
    auto axes = key_move_from_fractions(
        movement_key_state(client.kb.forward, now_msec, frame_msec),
        movement_key_state(client.kb.back, now_msec, frame_msec),
        movement_key_state(client.kb.moveright, now_msec, frame_msec),
        movement_key_state(client.kb.moveleft, now_msec, frame_msec));
    return {bits, axes, client.stance_latch};
}

// ---- weapon_select.rs -------------------------------------------------------
bool weapon_cycle_allowed(const PlayerState& ps, int32_t time, int32_t select_time,
                          int32_t select_time_override, int32_t cycle_delay) {
    return (ps.pm_flags & 0xc08u) == 0 &&
           (ps.weap_flags & 0x880u) == 0 &&
           (ps.other_flags & 2u) == 0 &&
           (ps.other_flags & 0x1000u) != 0 &&
           (ps.e_flags & 0x100c00u) == 0 &&
           ps.pm_type <= 7 &&
           (time - select_time) >= cycle_delay &&
           select_time_override <= time;
}

std::optional<uint32_t> cycle_weapon(const std::array<int32_t, 15>& weapons, uint32_t selected,
                                     uint32_t mapped, uint32_t last_primary, bool next,
                                     int32_t (*inventory_type)(uint32_t)) {
    auto selected2 = (selected != 0 && inventory_type(selected) == 3) ? mapped : selected;
    auto current_type = (selected2 == 0) ? 0 : inventory_type(selected2);
    if (current_type != 0 && current_type != 4 && last_primary != 0 &&
        std::find(weapons.begin(), weapons.end(), static_cast<int32_t>(last_primary)) != weapons.end())
        return last_primary;
    auto wanted_type = (current_type == 4) ? 4 : 0;
    auto start = (selected2 == 0)
                     ? 0
                     : static_cast<std::size_t>(
                           std::distance(weapons.begin(),
                                         std::find(weapons.begin(), weapons.end(),
                                                   static_cast<int32_t>(selected2))));
    auto count = (selected2 == 0) ? 15 : 14;
    for (std::size_t step = 0; step < count; ++step) {
        auto distance = step + (selected2 == 0 ? 0 : 1);
        auto slot = next ? (start + distance) % 15 : (start + 15 - distance) % 15;
        auto candidate = weapons[slot];
        if (candidate > 0 && inventory_type(static_cast<uint32_t>(candidate)) == wanted_type)
            return static_cast<uint32_t>(candidate);
    }
    return std::nullopt;
}

// ---- adjust_angles.rs -------------------------------------------------------
std::pair<float, float> adjust_angles(KbuttonSet& kb, const AdjustAnglesInput& input) {
    if (input.frozen) return {0.0f, 0.0f};
    float speed = input.dt;
    if (kb.speed.active) speed *= input.cl_anglespeedkey;
    float yaw = 0.0f;
    if (!kb.strafe.active) {
        auto max = input.cl_yawspeed;
        if (input.cgame_max_yaw_speed > 0.0f && max > input.cgame_max_yaw_speed)
            max = input.cgame_max_yaw_speed;
        auto step = max * speed;
        yaw -= key_state(kb.right, input.now_msec, input.frame_msec) * step;
        yaw += key_state(kb.left, input.now_msec, input.frame_msec) * step;
    }
    auto max = input.cl_pitchspeed;
    if (input.cgame_max_pitch_speed > 0.0f && max > input.cgame_max_pitch_speed)
        max = input.cgame_max_pitch_speed;
    auto step = max * speed;
    float pitch = 0.0f;
    pitch -= key_state(kb.lookup, input.now_msec, input.frame_msec) * step;
    pitch += key_state(kb.lookdown, input.now_msec, input.frame_msec) * step;
    return {pitch, yaw};
}

} // namespace input_iw4
