// Unit tests for the ported input_iw4 crate.
#include "input_iw4/input_iw4.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <array>

using namespace input_iw4;

static int failures = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures; \
        } \
    } while (0)

static bool near(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

static void test_button_down_first_press() {
    Kbutton btn = {};
    in_key_down(btn, 1, 100);
    CHECK(btn.active);
    CHECK(btn.was_pressed);
    CHECK(btn.downtime == 100);
    CHECK(btn.msec == 0);

    // re-press of the same key is ignored until released
    in_key_down(btn, 1, 150);
    CHECK(btn.downtime == 100);
}

static void test_button_up_clears_when_last_key() {
    Kbutton btn = {};
    in_key_down(btn, 1, 100);
    CHECK(btn.active);
    // First sample sets downtime=100.
    CHECK(key_state(btn, 100, 16) == 0.0f);
    in_key_up(btn, 200, 1, 16);
    CHECK(!btn.active);
    // downtime was 100, uptime 200 -> msec = 100.
    CHECK(btn.msec == 100);
}

static void test_button_up_ignored_if_not_down() {
    Kbutton btn = {};
    in_key_up(btn, 100, 1, 16);
    CHECK(!btn.active);
    CHECK(btn.msec == 0);
}

static void test_key_state_first_sample_is_zero_when_downtime_unset() {
    Kbutton btn = {};
    in_key_down(btn, 1, 0);
    // First sample after a fresh down with downtime==0 returns 0.
    CHECK(key_state(btn, 10, 16) == 0.0f);
    // Second sample now accumulates the time since the first sample.
    CHECK(near(key_state(btn, 16, 16), 6.0f / 16.0f));
}

static void test_key_state_after_release_returns_accumulated_msec() {
    Kbutton btn = {};
    in_key_down(btn, 1, 0);
    key_state(btn, 16, 16); // sets downtime=16
    in_key_up(btn, 32, 1, 16);
    CHECK(!btn.active);
    // At up time, downtime was 16, so msec gets 32 - 16 = 16.
    CHECK(key_state(btn, 32, 16) == 1.0f);
}

static void test_hold_key_script_keynum() {
    Kbutton btn = {};
    in_key_down(btn, SCRIPT_KEYNUM, 50);
    CHECK(btn.active);
    // First sample sets downtime=50.
    CHECK(key_state(btn, 50, 16) == 0.0f);
    // Release at 80: delta = 80 - 50 = 30 -> msec = 30.
    in_key_up(btn, 80, SCRIPT_KEYNUM, 16);
    CHECK(!btn.active);
    CHECK(btn.msec == 30);
}

static void test_button_bits_assembly() {
    KbuttonSet kb = {};
    in_key_down(kb.attack, 1, 0);
    in_key_down(kb.reload, 2, 0);
    in_key_down(kb.prone, 3, 0);
    auto bits = cmd_buttons(kb);
    CHECK((bits & buttons::ATTACK) != 0);
    CHECK((bits & buttons::RELOAD) != 0);
    CHECK((bits & buttons::PRONE) != 0);
    CHECK((bits & buttons::CROUCH) == 0);
}

static void test_key_move_bits_ads_and_sprint() {
    KbuttonSet kb = {};
    in_key_down(kb.forward, 1, 0);
    in_key_down(kb.speed, 2, 0);
    in_key_down(kb.sprint, 3, 0);
    // using_ads false, speed held -> speed.active != using_ads is true -> ADS set
    auto bits = key_move_bits(kb, false, cmd_buttons(kb));
    CHECK((bits & buttons::ADS) != 0);
    CHECK((bits & buttons::SPRINT) != 0);
    // using_ads true, speed held -> speed.active != using_ads is false -> ADS cleared
    bits = key_move_bits(kb, true, cmd_buttons(kb));
    CHECK((bits & buttons::ADS) == 0);
}

static void test_key_move_bits_crouch_and_prone() {
    KbuttonSet kb = {};
    in_key_down(kb.movedown, 1, 0);
    {
        auto bits = key_move_bits(kb, false, 0);
        CHECK((bits & buttons::CROUCH) != 0);
        CHECK((bits & buttons::STANCE_HELD) != 0);
        CHECK((bits & buttons::PRONE) == 0);
    }
    KbuttonSet kb2 = {};
    in_key_down(kb2.prone, 1, 0);
    {
        auto bits = key_move_bits(kb2, false, 0);
        CHECK((bits & buttons::PRONE) != 0);
        CHECK((bits & buttons::CROUCH) == 0);
    }
}

static void test_key_move_from_fractions_clamps() {
    auto a = key_move_from_fractions(2.0f, -1.0f, 0.5f, -0.5f);
    CHECK(near(a.forward, 1.0f));
    CHECK(near(a.right, 1.0f));
    auto b = key_move_from_fractions(-2.0f, 1.0f, -0.5f, 0.5f);
    CHECK(near(b.forward, -1.0f));
    CHECK(near(b.right, -1.0f));
}

static void test_axis_to_move_rounding() {
    CHECK(axis_to_move(1.0f) == 127);
    CHECK(axis_to_move(-1.0f) == -127);
    CHECK(axis_to_move(0.5f) == 64);
    CHECK(axis_to_move(-0.5f) == -64);
}

static void test_command_dispatch_attack_pair() {
    ClientInput client = {};
    client.keys[1].binding = 1; // +attack
    clientinput_key_event(client, 1, true, 100, 16);
    CHECK(client.kb.attack.active);
    CHECK(client.kb.attack.was_pressed);

    client.keys[1].binding = 1;
    clientinput_key_event(client, 1, false, 200, 16);
    CHECK(!client.kb.attack.active);
}

static void test_command_dispatch_uses_key_up_id() {
    // Bind a key to +attack (id 1). Pressing drives attack down; releasing
    // looks up key_up_command_id(1) == 2 and dispatches -attack, releasing it.
    ClientInput client = {};
    client.keys[42].binding = 1;
    clientinput_key_event(client, 42, true, 100, 16);
    CHECK(client.kb.attack.active);
    clientinput_key_event(client, 42, false, 200, 16);
    CHECK(!client.kb.attack.active);
}

static void test_stance_latch_toggle_crouch_prone() {
    ClientInput client = {};
    clientinput_input_cmd(client, 72, 0, 100, 16);
    CHECK(client.stance_latch == static_cast<int32_t>(buttons::CROUCH));

    clientinput_input_cmd(client, 73, 0, 100, 16);
    CHECK(client.stance_latch == static_cast<int32_t>(buttons::PRONE));

    clientinput_input_cmd(client, 73, 0, 100, 16);
    CHECK(client.stance_latch == 0);
}

static void test_stance_hold_switches_to_prone_after_delay() {
    ClientInput client = {};
    client.airborne = false;
    // +stance: latch was 0, so stance_held.from = 0, stance_latch becomes CROUCH
    clientinput_input_cmd(client, 23, 0, 10, 16);
    CHECK(client.stance_latch == static_cast<int32_t>(buttons::CROUCH));
    CHECK(client.stance_held_valid);
    CHECK(client.stance_held.from == 0);

    // sample_move past the hold window: stance_hold flips latch to PRONE
    auto out = sample_move(client, 10 + STANCE_HOLD_MS + 1, 16);
    CHECK(out.stance_latch == static_cast<int32_t>(buttons::PRONE));
    CHECK((out.buttons & buttons::PRONE) != 0);
    CHECK(!client.stance_held_valid);
}

static void test_stance_hold_paused_while_airborne() {
    ClientInput client = {};
    client.airborne = true;
    clientinput_input_cmd(client, 23, 0, 0, 16); // +stance
    CHECK(client.stance_held_valid);
    clientinput_key_event(client, 22, false, STANCE_HOLD_MS + 1000, 16);
    CHECK(client.stance_held_valid);
    CHECK(client.stance_latch == static_cast<int32_t>(buttons::CROUCH));
}

static void test_gostand_was_pressed_clears_stance() {
    ClientInput client = {};
    in_key_down(client.kb.gostand, 1, 0);
    client.kb.gostand.was_pressed = true;
    client.stance_latch = static_cast<int32_t>(buttons::PRONE);
    auto out = sample_move(client, 0, 16);
    CHECK(out.stance_latch == 0);
    CHECK((out.buttons & buttons::JUMP) != 0);
}

static void test_sample_move_includes_stance_latch_when_not_held() {
    ClientInput client = {};
    client.stance_latch = static_cast<int32_t>(buttons::CROUCH);
    auto out = sample_move(client, 0, 16);
    CHECK((out.buttons & buttons::CROUCH) != 0);
    CHECK((out.buttons & buttons::STANCE_HELD) == 0);
}

static void test_mouse_move_angles() {
    constexpr float A2S = 65536.0f / 360.0f;
    auto [pitch, yaw] = mouse_move_angles(1.0f, 0.5f, 140.0f, 140.0f);
    CHECK(pitch == static_cast<int32_t>(0.5f * 140.0f * A2S));
    CHECK(yaw == static_cast<int32_t>(-1.0f * 140.0f * A2S));
    auto [p2, y2] = mouse_move_angles(0.0f, 1.0f, 140.0f, 140.0f);
    CHECK(y2 == 0);
    CHECK(p2 == static_cast<int32_t>(1.0f * 140.0f * A2S));
}

static void test_apply_mouse_sensitivity() {
    auto [mx, my] = apply_mouse_sensitivity(1.0f, 2.0f, 16.0f, 16.0f, 1.0f, 0.0f, 1.0f);
    CHECK(near(mx, 1.0f));
    CHECK(near(my, 2.0f));
}

static void test_command_name_and_lookup() {
    CHECK(std::strcmp(input_command_name(1), "+attack") == 0);
    CHECK(input_command_name(0) == nullptr);
    CHECK(command_id_from_name("+attack") == 1);
    CHECK(command_id_from_name("nonsense") == 0);
    CHECK(command_id_lookup("+Attack") == 1);
    CHECK(command_id_lookup("attack") == 1);
    CHECK(command_id_lookup("+attack") == 1);
    CHECK(command_id_lookup("") == 0);
}

static void test_key_up_command_id_pairs() {
    CHECK(key_up_command_id(1) == 2);
    CHECK(key_up_command_id(2) == 0);
    CHECK(key_up_command_id(79) == 80);
    CHECK(key_up_command_id(81) == 82);
    CHECK(key_up_command_id(0) == 0);
}

static void test_weapon_cycle_allowed_gating() {
    PlayerState ps = PlayerState::zero();
    // Default state fails the other_flags & 0x1000 != 0 gate.
    CHECK(!weapon_cycle_allowed(ps, 100, 0, 0, 10));
    PlayerState ok = ps;
    ok.other_flags |= 0x1000u;
    CHECK(weapon_cycle_allowed(ok, 100, 0, 0, 10));
    PlayerState blocked = ok;
    blocked.pm_flags |= 0x800u; // bit in the 0xc08 mask
    CHECK(!weapon_cycle_allowed(blocked, 100, 0, 0, 10));
}

static int32_t inv_inv_type_primary(uint32_t w) { return (w == 10) ? 0 : 4; }
static int32_t inv_inv_type_no_match(uint32_t) { return 4; }

static void test_cycle_weapon_prefer_last_primary() {
    std::array<int32_t, 15> weapons = {0, 10, 0};
    auto r = cycle_weapon(weapons, 0, 0, 10, true, inv_inv_type_primary);
    CHECK(r.has_value());
    CHECK(r.value() == 10);
}

static void test_cycle_weapon_none_when_no_match() {
    std::array<int32_t, 15> weapons = {0, 0, 0};
    auto r = cycle_weapon(weapons, 0, 0, 0, true, inv_inv_type_no_match);
    CHECK(!r.has_value());
}

static void test_create_cmd_frozen_keeps_angles() {
    CreateCmdInput in = {};
    in.server_time = 500;
    in.angles = {100, 200, 300};
    in.frozen = true;
    auto cmd = create_cmd(in);
    CHECK(cmd.server_time == 500);
    CHECK(cmd.angles[0] == 100);
    CHECK(cmd.buttons == 0);
    CHECK(cmd.forwardmove == 0);
}

static void test_create_cmd_blends_angle_deltas() {
    CreateCmdInput in = {};
    in.server_time = 500;
    in.angles = {100, 200, 300};
    in.mouse_pitch_delta = 8;
    in.key_pitch_delta = 4;
    in.mouse_yaw_delta = -16;
    in.key_yaw_delta = 8;
    auto cmd = create_cmd(in);
    CHECK(cmd.angles[0] == 112);
    CHECK(cmd.angles[1] == 192); // 200 + 8 + (-16)
    CHECK(cmd.angles[2] == 300);
    CHECK(cmd.buttons == 0);
}

static const char* g_visit_seen[4];
static int g_visit_n = 0;

static void visit_collect(const char* name) {
    if (g_visit_n < 4) g_visit_seen[g_visit_n++] = name;
}

static void test_visit_active_names() {
    KbuttonSet kb = {};
    in_key_down(kb.attack, 1, 0);
    in_key_down(kb.sprint, 2, 0);
    g_visit_n = 0;
    kbuttonset_visit_active_names(kb, visit_collect);
    CHECK(g_visit_n == 2);
    CHECK(std::strcmp(g_visit_seen[0], "+attack") == 0);
    CHECK(std::strcmp(g_visit_seen[1], "+sprint") == 0);
}

static void test_clear_was_pressed() {
    KbuttonSet kb = {};
    kb.attack.was_pressed = true;
    kb.sprint.was_pressed = true;
    kb.stance.was_pressed = true;
    kbuttonset_clear_was_pressed(kb);
    CHECK(!kb.attack.was_pressed);
    CHECK(!kb.sprint.was_pressed);
    CHECK(!kb.stance.was_pressed);
}

static void test_ads_toggle_via_command() {
    ClientInput client = {};
    // +toggleads_throw (57) toggles ads on
    clientinput_input_cmd(client, 57, 0, 0, 16);
    CHECK(client.using_ads);
    // -toggleads_throw (58) does NOT toggle ads (pair_down(58) is false)
    clientinput_input_cmd(client, 58, 0, 0, 16);
    CHECK(client.using_ads);
    // +toggleads_throw (57) toggles ads off
    clientinput_input_cmd(client, 57, 0, 0, 16);
    CHECK(!client.using_ads);
}

static void test_toggleads_down_toggles_using_ads() {
    ClientInput client = {};
    clientinput_input_cmd(client, 57, 0, 0, 16);
    CHECK(client.using_ads);
    clientinput_input_cmd(client, 57, 0, 0, 16);
    CHECK(!client.using_ads);
}

static void test_ads_toggle_throw_also_drives_throw_btn() {
    ClientInput client = {};
    clientinput_input_cmd(client, 57, 0, 0, 16);
    CHECK(client.using_ads);
    CHECK(client.kb.throw_btn.active);
}

static void test_leaveads_clears_ads() {
    ClientInput client = {};
    client.using_ads = true;
    clientinput_input_cmd(client, 77, 0, 0, 16); // leaveads
    CHECK(!client.using_ads);
}

static void test_set_ads_direct() {
    ClientInput client = {};
    clientinput_set_ads(client, true);
    CHECK(client.using_ads);
    clientinput_set_ads(client, false);
    CHECK(!client.using_ads);
}

int main() {
    test_button_down_first_press();
    test_button_up_clears_when_last_key();
    test_button_up_ignored_if_not_down();
    test_key_state_first_sample_is_zero_when_downtime_unset();
    test_key_state_after_release_returns_accumulated_msec();
    test_hold_key_script_keynum();
    test_button_bits_assembly();
    test_key_move_bits_ads_and_sprint();
    test_key_move_bits_crouch_and_prone();
    test_key_move_from_fractions_clamps();
    test_axis_to_move_rounding();
    test_command_dispatch_attack_pair();
    test_command_dispatch_uses_key_up_id();
    test_stance_latch_toggle_crouch_prone();
    test_stance_hold_switches_to_prone_after_delay();
    test_stance_hold_paused_while_airborne();
    test_gostand_was_pressed_clears_stance();
    test_sample_move_includes_stance_latch_when_not_held();
    test_mouse_move_angles();
    test_apply_mouse_sensitivity();
    test_command_name_and_lookup();
    test_key_up_command_id_pairs();
    test_weapon_cycle_allowed_gating();
    test_cycle_weapon_prefer_last_primary();
    test_cycle_weapon_none_when_no_match();
    test_create_cmd_frozen_keeps_angles();
    test_create_cmd_blends_angle_deltas();
    test_visit_active_names();
    test_clear_was_pressed();
    test_ads_toggle_via_command();
    test_toggleads_down_toggles_using_ads();
    test_ads_toggle_throw_also_drives_throw_btn();
    test_leaveads_clears_ads();
    test_set_ads_direct();

    if (failures == 0) {
        std::printf("all input_iw4 tests passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d input_iw4 test failure(s)\n", failures);
    return 1;
}
