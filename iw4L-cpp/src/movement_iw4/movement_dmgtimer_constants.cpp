// Define the dmgtimer constants once. movement_events.cpp declares them as
// extern, and movement_footstep_jump.cpp uses them; keep a single definition
// in this translation unit.
#include "movement_iw4/movement_iw4.h"

namespace movement_iw4 {
using namespace playerstate_iw4;

const float PLAYER_DMGTIMER_TIME_PER_POINT = 100.0f;
const float PLAYER_DMGTIMER_MAX_TIME = 750.0f;
const float PLAYER_DMGTIMER_MIN_SCALE = 0.0f;
const int32_t PLAYER_DMGTIMER_STUMBLE_TIME_MS = 500;
const int32_t PLAYER_DMGTIMER_FLINCH_TIME_MS = 500;
const uint8_t ANIM_MT_FLINCH_FORWARD = 32;
}
