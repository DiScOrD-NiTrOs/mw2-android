// Extended movement types + template declarations for the walk/air/ground/slide
// ports. Include this from the .cpp that defines those templates and from any
// consumer that needs the struct types. Kept separate from movement_iw4.h to
// avoid introducing a trace_iw4 dependency in the core movement header.
#pragma once

#include "movement_iw4/movement_iw4.h"

#include "trace_iw4/trace_iw4.h"
#include <optional>
#include <cstdint>
#include <array>

namespace movement_iw4 {

using namespace playerstate_iw4;

// Re-export the call-ins used across movement translation units.
void add_predictable_event(PlayerState& ps, int32_t event, int32_t parm);
void add_event(PlayerState& ps, int32_t event);
float damage_scale_walk(int32_t damage_timer);
void walk_move_drop_damage_timer(PlayerState& ps, float frametime_seconds);
bool damage_window_open(int32_t damage_timer, int32_t damage_duration, int32_t window_ms);
size_t surface_type_index(uint32_t surface_flags);
const char* surface_type_to_name(size_t index);
const char* surface_type_name(uint32_t surface_flags);
bool bob_cycle_wrapped(uint8_t old, uint8_t new_);
float get_bob_max_speed(const PlayerState& ps, int8_t forwardmove, int8_t rightmove,
                        bool walking, bool sprinting, int32_t server_time,
                        const CmdScaleWalkContext& scales);
void footsteps_bob_cycle(PlayerState& ps, int32_t msec, int8_t forwardmove, int8_t rightmove,
                         bool almost_ground_plane, int32_t server_time,
                         const CmdScaleWalkContext& scales);
std::optional<uint8_t> footsteps_anim_move_type(const PlayerState& ps, int8_t forwardmove,
                                                int8_t rightmove, bool almost_ground_plane);
bool should_make_footsteps(const PlayerState& ps);
int32_t footstep_event_type(const PlayerState& ps, uint32_t surface_flags);
bool footstep_event(PlayerState& ps, uint8_t old, uint8_t new_, uint32_t surface_flags,
                    bool b_footstep);
bool ladder_footsteps(PlayerState& ps, int32_t msec, int32_t server_time);

// ---------- move contexts (match iw4L walk.rs / air.rs) -------------------

struct MoveBounds {
    float mins[3] = {0, 0, 0};
    float maxs[3] = {0, 0, 0};
    uint32_t tracemask = 0;
};

struct AirMoveContext {
    float player_spectate_speed_scale = 1.0f;
    float shellshock_gravity_scale = 1.0f;
    float shellshock_gravity_bias = 0.0f;
};

struct WalkMoveContext {
    CmdScaleWalkContext cmd_scale;
    float weapon_move_scale = 1.0f;
    uint32_t old_buttons = 0;
    JumpLaunchContext jump;
    AirMoveContext air;
};

// ---------- collision backend contract -------------------------------------

struct CorrectSolidOutcome {
    float origin[3] = {0, 0, 0};
    trace_iw4::Trace trace;
};

// recovery::Contact / ContactBuffer / Coverage use the same names as in the
// Rust recovery module; defined here once so the template code can name them.
namespace recovery {
struct Contact {
    float origin[3] = {0, 0, 0};
    float normal[3] = {0, 0, 0};
    float depth = 0.0f;
};
struct ContactBuffer {
    Contact* data = nullptr;
    size_t count = 0;
    size_t capacity = 0;
};
struct Coverage {
    uint8_t kind = 0;
};
} // namespace recovery

// ---------- template declarations ------------------------------------------

template <typename C>
void complete_ground_trace(PlayerState& ps, Pml& pml, const MoveBounds& bounds,
                           int8_t forwardmove, const C& collision);

template <typename C>
bool player_prone_allowed(const PlayerState& ps, const C& collision, float f_size,
                          bool weapon_blocks_prone);

template <typename C>
void walk_move(PlayerState& ps, Pml& pml, UserCmd& cmd, const WalkMoveContext& context,
               const MoveBounds& bounds, const C& collision);

template <typename C>
void air_move(PlayerState& ps, const Pml& pml, const UserCmd& cmd,
              const AirMoveContext& context, const MoveBounds& bounds, const C& collision);

void project_velocity(float velocity[3], const float normal[3]);

void step_slide_move(PlayerState& ps, const Pml& pml,
                     const trace_iw4::GroundTraceInput& trace_input,
                     const float mins[3], const float maxs[3], uint32_t tracemask,
                     std::optional<float> gravity);

} // namespace movement_iw4
