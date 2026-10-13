// Port of iw4L crates/movement_iw4: snap.rs, drop_timers.rs, breath.rs,
// dmgtimer.rs, ads_intent.rs, events.rs, ads_frac.rs.
#include "movement_iw4/movement_iw4.h"

#include "math_iw4/math_iw4.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace movement_iw4 {

using namespace playerstate_iw4;

// SequencedPlayerEvent / SequencedPlayerEventSink are declared in
// movement_iw4.h (events part). Reinstate locally because the upstream
// movement_iw4.h only forward-declares them in this build layout.
struct SequencedPlayerEvent {
    int32_t sequence = 0;
    int32_t event = 0;
    int32_t event_parm = 0;
};

struct SequencedPlayerEventSink {
    virtual ~SequencedPlayerEventSink();
    virtual void operator()(const SequencedPlayerEvent&) = 0;
};


namespace {
constexpr float VELOCITY_MOVE_RATIO = 0.25f;

// Rust i32 saturating add/sub for non-negative values.
int32_t sat_add_i32(int32_t a, int32_t b) {
    const int64_t sum = static_cast<int64_t>(a) + b;
    return sum > INT32_MAX ? INT32_MAX : sum < INT32_MIN ? INT32_MIN : static_cast<int32_t>(sum);
}
int32_t sat_sub_i32(int32_t a, int32_t b) {
    return sat_add_i32(a, -b);
}

constexpr int32_t LADDER_JUMP_BLOCK_MS = 300;

// ads_intent.rs button copies
constexpr uint32_t ADI_BUTTON_SPRINT = 0x2;
constexpr uint32_t ADI_BUTTON_ADS = 0x800;
constexpr uint32_t ADI_BUTTON_SCOPE_HOLD = 0x2000;
constexpr int32_t EV_RESET_ADS = 0x11;
} // namespace

// snap.rs --------------------------------------------------------------------

static float round_ties_even(float x) {
    const float abs = std::fabs(x);
    const float truncated = std::trunc(abs);
    const float frac = abs - truncated;
    const bool keep_even_tie = frac == 0.5f && (static_cast<int32_t>(truncated) & 1) == 0;
    const float rounded = (frac < 0.5f || keep_even_tie) ? truncated : truncated + 1.0f;
    return std::copysign(rounded, x);
}

static float snap_component(float x) {
    return static_cast<float>(static_cast<int32_t>(round_ties_even(x)));
}

void snap_vector(float v[3]) {
    v[0] = snap_component(v[0]);
    v[1] = snap_component(v[1]);
    v[2] = snap_component(v[2]);
}

void end_tick_velocity(PlayerState& ps, const Pml& pml) {
    const float dt = pml.frametime;
    if (dt > 0.0f) {
        const float mv[3] = {
            ps.origin[0] - pml.previous_origin[0],
            ps.origin[1] - pml.previous_origin[1],
            ps.origin[2] - pml.previous_origin[2],
        };
        const float real_sq =
            (mv[0] * mv[0] + mv[1] * mv[1] + mv[2] * mv[2]) / (dt * dt);
        const float vel_sq = ps.velocity[0] * ps.velocity[0] +
                             ps.velocity[1] * ps.velocity[1] +
                             ps.velocity[2] * ps.velocity[2];
        if (real_sq < vel_sq * VELOCITY_MOVE_RATIO) {
            const float inv = 1.0f / dt;
            ps.velocity[0] = mv[0] * inv;
            ps.velocity[1] = mv[1] * inv;
            ps.velocity[2] = mv[2] * inv;
        }
    }
    snap_vector(ps.velocity);
}

// drop_timers.rs ---------------------------------------------------------------

namespace {
constexpr uint32_t PM_TIME_EXPIRY_CLEAR = 0x2180;
// jump_clear_state is declared in movement_iw4.h and defined in the jump port.
} // namespace

void drop_timers(PlayerState& ps, const Pml& pml) {
    if (ps.pm_time != 0) {
        if (pml.msec < ps.pm_time) {
            ps.pm_time -= pml.msec;
        } else {
            if ((ps.pm_flags & playerstate_iw4::pm_flags::JUMPING) != 0) {
                jump_clear_state(ps);
            }
            ps.pm_flags &= !PM_TIME_EXPIRY_CLEAR;
            ps.pm_time = 0;
        }
    }
    if (ps.legs_timer > 0) {
        ps.legs_timer -= pml.msec;
        if (ps.legs_timer < 0) {
            ps.legs_timer = 0;
        }
    }
    if (ps.torso_timer > 0) {
        ps.torso_timer -= pml.msec;
        if (ps.torso_timer < 0) {
            ps.torso_timer = 0;
        }
    }
}

// breath.rs ----------------------------------------------------------------------

void update_hold_breath(PlayerState& ps, uint32_t buttons_held, int32_t msec, bool eligible) {
    const bool was_holding = (ps.weap_flags & playerstate_iw4::weap_flags::HOLD_BREATH) != 0;
    const bool wants_hold = eligible && ps.f_weapon_pos_frac == 1.0f &&
                            (buttons_held & playerstate_iw4::buttons::BREATH) != 0 &&
                            (ps.pm_flags & (pm_flags::SPRINTING | pm_flags::MANTLE | playerstate_iw4::pm_flags::LADDER)) == 0 &&
                            (ps.weap_flags & playerstate_iw4::weap_flags::OFFHAND_VIEW) == 0 && ps.health > 0;
    const int32_t hold_time = playerstate_iw4::breath_hold_time_ms(ps);
    const bool holding = wants_hold && (was_holding || ps.hold_breath_timer == 0);
    ps.weap_flags &= ~playerstate_iw4::weap_flags::HOLD_BREATH;
    if (holding) {
        ps.hold_breath_timer = sat_add_i32(ps.hold_breath_timer, msec);
        if (ps.hold_breath_timer >= hold_time) {
            ps.hold_breath_timer = hold_time + playerstate_iw4::BREATH_GASP_TIME_MS;
        } else {
            ps.weap_flags |= playerstate_iw4::weap_flags::HOLD_BREATH;
        }
    } else {
        ps.hold_breath_timer = sat_sub_i32(ps.hold_breath_timer, msec) < 0
                                   ? 0
                                   : sat_sub_i32(ps.hold_breath_timer, msec);
    }
    const bool holding_after = (ps.weap_flags & playerstate_iw4::weap_flags::HOLD_BREATH) != 0;
    float target = holding_after
                       ? 0.0f
                       : 1.0f + 3.5f * static_cast<float>(ps.hold_breath_timer) /
                                    static_cast<float>(hold_time + playerstate_iw4::BREATH_GASP_TIME_MS);
    target = 1.0f + (target - 1.0f) * ps.f_weapon_pos_frac;
    const float rate = holding_after ? 1.0f : 6.0f;
    const float blend = rate * static_cast<float>(msec) * 0.001f;
    ps.hold_breath_scale += (target - ps.hold_breath_scale) * (blend < 1.0f ? blend : 1.0f);
}

// dmgtimer constants are defined in movement_iw4.h (inline constexpr).

namespace {
constexpr float FLINCH_TURN_DEGREES = 360.0f;
constexpr float FLINCH_SECTOR_DEGREES[4] = {45.0f, 135.0f, 225.0f, 315.0f};

int32_t flinch_yaw_anim(float view_yaw, const float dir[3]) {
    const float yaw = view_yaw < 0.0f ? view_yaw + FLINCH_TURN_DEGREES : view_yaw;
    float bearing = math_iw4::vec_to_yaw(dir[0], dir[1]) -
                    static_cast<float>(static_cast<int32_t>(yaw));
    if (bearing < 0.0f) {
        bearing += FLINCH_TURN_DEGREES;
    }
    const float edge_45 = FLINCH_SECTOR_DEGREES[0];
    const float edge_135 = FLINCH_SECTOR_DEGREES[1];
    const float edge_225 = FLINCH_SECTOR_DEGREES[2];
    const float edge_315 = FLINCH_SECTOR_DEGREES[3];
    if (!(bearing >= edge_45 && bearing < edge_315)) {
        return 0;
    }
    if (bearing >= edge_135 && bearing < edge_225) {
        return 1;
    }
    if (bearing >= edge_45 && bearing < edge_135) {
        return 2;
    }
    return 3;
}
} // namespace

void update_damage_timer(PlayerState& ps, int32_t damage, const float* dir /* nullable, 3 */) {
    const int32_t added = static_cast<int32_t>(PLAYER_DMGTIMER_TIME_PER_POINT * static_cast<float>(damage));
    ps.damage_timer = sat_add_i32(ps.damage_timer, added);
    if (PLAYER_DMGTIMER_MAX_TIME < static_cast<float>(ps.damage_timer)) {
        ps.damage_timer = static_cast<int32_t>(PLAYER_DMGTIMER_MAX_TIME);
    }
    ps.damage_duration = ps.damage_timer;
    ps.flinch_yaw_anim = dir ? flinch_yaw_anim(ps.viewangles[1], dir) : 0;
}

float damage_scale_walk(int32_t damage_timer) {
    if (damage_timer == 0 || PLAYER_DMGTIMER_MAX_TIME == 0.0f) {
        return 1.0f;
    }
    return (-PLAYER_DMGTIMER_MIN_SCALE / PLAYER_DMGTIMER_MAX_TIME) * static_cast<float>(damage_timer) + 1.0f;
}

void walk_move_drop_damage_timer(PlayerState& ps, float frametime_seconds) {
    ps.damage_timer -= static_cast<int32_t>(frametime_seconds * 1000.0f);
    if (ps.damage_timer < 1) {
        ps.damage_timer = 0;
    }
}

bool damage_window_open(int32_t damage_timer, int32_t damage_duration, int32_t window_ms) {
    const int64_t start = static_cast<int64_t>(damage_duration) - window_ms;
    const int32_t start_clamped = start < 0 ? 0 : static_cast<int32_t>(start);
    return start_clamped < damage_timer;
}

// events.rs --------------------------------------------------------------------------

// events.rs --------------------------------------------------------------------------

void add_predictable_event(PlayerState& ps, int32_t event, int32_t parm) {
    if (event == 0) {
        return;
    }
    switch (ps.event_sequence & 3) {
    case 0: ps.events_0 = event; ps.event_parms_0 = parm; break;
    case 1: ps.events_1 = event; ps.event_parms_1 = parm; break;
    case 2: ps.events_2 = event; ps.event_parms_2 = parm; break;
    default: ps.events_3 = event; ps.event_parms_3 = parm; break;
    }
    ps.event_sequence = (ps.event_sequence + 1) & 0x7ff; // wrapping via mask
}

void add_event(PlayerState& ps, int32_t event) {
    add_predictable_event(ps, event, 0);
}

SequencedPlayerEventSink::~SequencedPlayerEventSink() = default;

void consume_player_events(const PlayerState& ps, int32_t* consumed_sequence,
                           SequencedPlayerEventSink& sink) {
    constexpr int32_t EVENT_SEQUENCE_MASK = 0x7ff;
    constexpr int32_t EVENT_SEQUENCE_WRAP_WINDOW = 0x200;
    constexpr int32_t EVENT_RING_LEN = 4;

    const int32_t next = ps.event_sequence;
    if (*consumed_sequence == next) {
        return;
    }
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

    auto ring_slot = [&](size_t slot) -> std::pair<int32_t, int32_t> {
        switch (slot) {
        case 0: return {ps.events_0, ps.event_parms_0};
        case 1: return {ps.events_1, ps.event_parms_1};
        case 2: return {ps.events_2, ps.event_parms_2};
        default: return {ps.events_3, ps.event_parms_3};
        }
    };

    while (consumed < next) {
        const size_t slot = static_cast<uint32_t>(consumed) & 3;
        const auto [event, event_parm] = ring_slot(slot);
        if (event != 0) {
            SequencedPlayerEvent ev;
            ev.sequence = consumed & EVENT_SEQUENCE_MASK;
            ev.event = event;
            ev.event_parm = event_parm;
            sink(ev);
        }
        ++consumed;
    }
    *consumed_sequence = next;
}

// ads_intent.rs ----------------------------------------------------------------------

struct AdsIntentContext {
    bool ads_allowed = false;
    bool weapon_def_scope = false;
    bool sprint_hold_ads = false;
};

struct AdsIntentResult {
    bool ads_anim_enabled = false;
    bool exit_ads_event = false;
};

namespace {
bool prone_stance_settled(const PlayerState& ps) {
    return (ps.pm_flags & 0x800u) == 0 && (ps.pm_flags & 0x400000u) != 0 && ps.pm_time == 0;
}

bool scoped_weapon_raised(const PlayerState& ps, bool weapon_def_scope) {
    return weapon_def_scope && ps.f_weapon_pos_frac > 0.0f;
}
} // namespace

AdsIntentResult update_ads_intent(PlayerState& ps, const UserCmd& cmd, uint32_t old_buttons,
                                  const AdsIntentContext& context) {
    ps.pm_flags &= ~pm_flags::ADS_INTENT;

    bool ads_allowed = context.ads_allowed;
    bool exit_ads_event = false;

    if ((cmd.buttons & ADI_BUTTON_SPRINT) != 0 &&
        (!context.weapon_def_scope || (cmd.buttons & ADI_BUTTON_SCOPE_HOLD) == 0) &&
        (!context.sprint_hold_ads || ps.f_weapon_pos_frac == 0.0f)) {
        add_predictable_event(ps, EV_RESET_ADS, 0);
        exit_ads_event = true;
        ps.pm_flags &= ~pm_flags::ADS_INTENT;
        ads_allowed = false;
    }

    if ((cmd.buttons & ADI_BUTTON_ADS) != 0 && ads_allowed) {
        if ((ps.pm_flags & pm_flags::PRONE) != 0 &&
            !scoped_weapon_raised(ps, context.weapon_def_scope)) {
            const bool moving = cmd.forwardmove != 0 || cmd.rightmove != 0;
            if ((old_buttons & ADI_BUTTON_ADS) != 0 && moving && !prone_stance_settled(ps)) {
                // held prone ADS while unsettled: no-op
            } else {
                ps.pm_flags |= pm_flags::ADS_INTENT;
                if (!prone_stance_settled(ps)) {
                    ps.pm_flags |= pm_flags::PRONEMOVE_OVERRIDDEN;
                }
            }
        } else {
            ps.pm_flags |= pm_flags::ADS_INTENT;
        }
    }

    AdsIntentResult result;
    result.ads_anim_enabled = (ps.pm_flags & pm_flags::ADS_INTENT) != 0;
    result.exit_ads_event = exit_ads_event;
    return result;
}

// ads_frac.rs ---------------------------------------------------------------------------

struct AdsFracContext {
    bool aim_down_sight = true;
    int32_t ads_reload_trans_time_ms = 0;
    bool segmented_reload = false;
    float ads_in_rate = 1.0f / 200.0f;
    float ads_out_rate = 1.0f / 200.0f;
    bool rechamber_while_ads = true;
    bool ads_fire_only = false;
};

void update_ads_frac(PlayerState& ps, int32_t msec, const AdsFracContext& context) {
    if (!context.aim_down_sight) {
        ps.f_weapon_pos_frac = 0.0f;
        ps.ads_delay_time = 0;
        return;
    }

    const int32_t ws = ps.weaponstate_primary;
    if (ws == 0xd || ws == 0xe || ws == 0xf) {
        ps.f_weapon_pos_frac = 0.0f;
        ps.ads_delay_time = 0;
        return;
    }

    bool ads_requested = (ps.pm_flags & pm_flags::ADS_INTENT) != 0;
    const bool reload_blocks_ads =
        context.segmented_reload
            ? (ws >= 8 && ws <= 11) || (ws == 12 && ps.weapon_time > context.ads_reload_trans_time_ms)
            : (ws == 8 && ps.weapon_time > context.ads_reload_trans_time_ms);
    if (reload_blocks_ads || (!context.rechamber_while_ads && ws == 7)) {
        ads_requested = false;
    }

    if (context.ads_fire_only &&
        ((ps.weapon_delay != 0 && ws == 6) ||
         (ps.weapon_delay_secondary != 0 && ps.weaponstate_secondary == 6))) {
        ads_requested = true;
    }

    float dt = static_cast<float>(msec < 0 ? 0 : msec);
    if (ps.perks[0] & PERK_QUICKDRAW) {
        dt *= PERK_QUICKDRAW_SPEED_SCALE;
    }

    if (ads_requested) {
        if (ps.f_weapon_pos_frac >= 1.0f) {
            ps.f_weapon_pos_frac = 1.0f;
            ps.ads_delay_time = 0;
            return;
        }
        const float rate = context.ads_in_rate;
        if (rate <= 0.0f) {
            ps.f_weapon_pos_frac = 1.0f;
        } else {
            float v = ps.f_weapon_pos_frac + rate * dt;
            ps.f_weapon_pos_frac = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
        }
        ps.ads_delay_time = 0;
    } else {
        if (ps.f_weapon_pos_frac <= 0.0f) {
            ps.f_weapon_pos_frac = 0.0f;
            ps.ads_delay_time = 0;
            return;
        }
        const float rate = context.ads_out_rate;
        if (rate <= 0.0f) {
            ps.f_weapon_pos_frac = 0.0f;
        } else {
            float v = ps.f_weapon_pos_frac - rate * dt;
            ps.f_weapon_pos_frac = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
        }
        ps.ads_delay_time = 0;
    }
}

} // namespace movement_iw4
