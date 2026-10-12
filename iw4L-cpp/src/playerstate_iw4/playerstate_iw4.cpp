// Port of iw4L crates/playerstate_iw4/src/playerstate.rs (impl part).
#include "playerstate_iw4/playerstate_iw4.h"

namespace playerstate_iw4 {

PlayerState PlayerState::zero() {
    PlayerState ps = {};
    ps.viewlocked_ent_num = ENTITYNUM_NONE;
    ps.hold_breath_scale = 1.0f;
    return ps;
}

uint32_t get_viewmodel_weapon_index(const PlayerState& ps) {
    if ((ps.weap_flags & weap_flags::OFFHAND_VIEW) != 0) {
        return ps.off_hand_index < 0 ? 0u : static_cast<uint32_t>(ps.off_hand_index);
    }
    return ps.weapon;
}

int32_t breath_hold_time_ms(const PlayerState& ps) {
    if (ps.perks[0] & PERK_HOLDBREATH) {
        return BREATH_HOLD_TIME_MS + PERK_EXTRA_BREATH_MS;
    }
    return BREATH_HOLD_TIME_MS;
}

} // namespace playerstate_iw4
