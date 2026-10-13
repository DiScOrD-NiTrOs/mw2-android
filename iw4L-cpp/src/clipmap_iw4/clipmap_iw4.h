// Port of iw4L crates/clipmap_iw4: lib.rs root + area_entities + capsule_query +
// cmodel + layouts + temp_box + mesh + world_trace + xmodel_trace.
#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <optional>
#include <functional>
#include <vector>

#include "trace_iw4/trace_iw4.h"

namespace clipmap_iw4 {

// Re-export the trace types we build on.
using trace_iw4::Trace;
using trace_iw4::BrushRef;
using trace_iw4::GroundTraceInput;

// ---------------------------------------------------------------------------
// BrushView trait — the clipmap code abstracts over brush sources (raw BrushRef
// lists, cmodel brushes, loaded mesh brushes, ...). In C++ we model it as a
// consumer type plus free functions; the lib exposes helpers that accept either
// a BrushView implementation or a plain BrushRef*.
// ---------------------------------------------------------------------------
struct BrushView {
    virtual ~BrushView() = default;
    virtual const float* planes() const = 0;        // [count][4]
    virtual uint32_t plane_count() const = 0;
    virtual uint32_t contents() const = 0;
    virtual const uint32_t* plane_surface_flags() const = 0; // [count]
    virtual uint16_t glass_encoded() const = 0;
};

// Adapter so a raw BrushRef* satisfies BrushView.
struct BrushRefView final : BrushView {
    const trace_iw4::BrushRef* b = nullptr;
    BrushRefView() = default;
    explicit BrushRefView(const trace_iw4::BrushRef& ref_) : b(&ref_) {}
    const float* planes() const override { return b->planes; }
    uint32_t plane_count() const override { return b->plane_count; }
    uint32_t contents() const override { return b->contents; }
    const uint32_t* plane_surface_flags() const override { return b->plane_surface_flags; }
    uint16_t glass_encoded() const override { return b->glass_encoded; }
};

// brush_ref + glass helpers (lib.rs)
inline trace_iw4::BrushRef brush_ref(const BrushView& v) {
    trace_iw4::BrushRef r;
    r.planes = v.planes();
    r.plane_count = v.plane_count();
    r.contents = v.contents();
    r.plane_surface_flags = v.plane_surface_flags();
    r.glass_encoded = v.glass_encoded();
    return r;
}

inline bool glass_is_solid(uint16_t damage, uint16_t destroy_threshold) {
    if (damage == 0xffff) return false;
    return damage < destroy_threshold;
}

inline bool brush_glass_allowed(const BrushView& brush, const std::function<bool(uint16_t)>& is_solid) {
    uint16_t enc = brush.glass_encoded();
    if (enc == 0) return true;
    return is_solid(enc - 1);
}

// ---------------------------------------------------------------------------
// TraceExtents / ClipNode / ClipLeaf — lib.rs
// ---------------------------------------------------------------------------
struct TraceExtents {
    float start[3] = {0, 0, 0};
    float end[3] = {0, 0, 0};
    float mins[3] = {0, 0, 0};
    float maxs[3] = {0, 0, 0};
    uint32_t mask = 0;
};

inline TraceExtents make_trace_extents(const float start[3], const float end[3],
                                       const float mins[3], const float maxs[3],
                                       uint32_t mask) {
    TraceExtents e;
    std::memcpy(e.start, start, sizeof(e.start));
    std::memcpy(e.end, end, sizeof(e.end));
    std::memcpy(e.mins, mins, sizeof(e.mins));
    std::memcpy(e.maxs, maxs, sizeof(e.maxs));
    e.mask = mask;
    return e;
}

struct ClipNode {
    float plane[4] = {0, 0, 0, 0};
    int32_t children[2] = {0, 0};
};

struct ClipLeaf {
    uint32_t first_brush = 0;
    uint16_t num_brushes = 0;
    uint16_t first_coll_aabb_index = 0;
    uint16_t coll_aabb_count = 0;
};

// ClipMapRef — borrowed clip map data. In the Rust code this is generic over
// BrushView; here we store a brush array + count, and we provide both the
// brush-list iterators and the tree walk entry points.
struct ClipMapRef {
    const ClipNode* nodes = nullptr;
    size_t node_count = 0;
    const ClipLeaf* leaves = nullptr;
    size_t leaf_count = 0;
    const uint16_t* leafbrushes = nullptr;
    size_t leafbrushes_count = 0;
    const trace_iw4::BrushRef* brushes = nullptr;
    size_t brush_count = 0;

    // BrushView-style iteration used by the helpers below.
    struct BrushIterator {
        const ClipMapRef* map = nullptr;
        size_t index = 0;
        bool end = false;
        BrushRef current;
    };
    BrushIterator brush_iter() const {
        BrushIterator it;
        it.map = this;
        it.index = 0;
        it.end = (nodes == nullptr || leaves == nullptr);
        if (!it.end && it.index < brush_count) {
            it.current = brushes[0];
        }
        return it;
    }
};

// Trace-related helpers (lib.rs)
bool leaves_have_coll_aabb(const ClipLeaf* leaves, size_t leaf_count);

// walk_clip_tree + TreeWalk — ported from lib.rs / the private tree-walk helpers.
struct TreeWalkCensus {
    uint32_t early_out_n = 0;
};

TreeWalkCensus walk_clip_tree(const ClipMapRef& map, const TraceExtents& ext,
                              const std::function<float()>& fraction,
                              std::function<void(const ClipLeaf&)> on_leaf);

// trace_linear_with_glass / trace_selected_brushes_with_glass / collect_leaf_hits
Trace trace_linear_with_glass(const ClipMapRef& map, const TraceExtents& ext,
                              const std::function<bool(uint16_t)>& glass_is_solid);

Trace trace_selected_brushes_with_glass(const ClipMapRef& map, const uint16_t* brush_ids,
                                        size_t brush_ids_count, const TraceExtents& ext,
                                        const std::function<bool(uint16_t)>& glass_is_solid);

// ---------------------------------------------------------------------------
// cmodel.rs — clip model transforms + ClipCmodel
// ---------------------------------------------------------------------------
struct CmodelTransform {
    float axis[3][3] = {};
    float origin[3] = {0, 0, 0};
};

struct ClipCmodel {
    const CmodelTransform* transforms = nullptr;
    size_t transform_count = 0;
    const float* model_mins = nullptr;
    const float* model_maxs = nullptr;
    size_t model_count = 0;
};

// clip_handle_to_model — in the real engine this maps a handle to a loaded cmodel.
// For the port we expose the lookup as a consumer callback.
using CmodelLookup = std::function<const clipmap_iw4::ClipCmodel*(int32_t)>;

std::optional<const ClipCmodel*> clip_handle_to_model(int32_t handle, const CmodelLookup& lookup);

// transformed_capsule_trace — thin wrapper over the temp-box capsule sweep.
Trace transformed_capsule_trace(const float start[3], const float end[3],
                                const float mins[3], const float maxs[3],
                                const CmodelTransform& xform,
                                const float box_mins[3], const float box_maxs[3],
                                uint32_t contents, uint32_t mask);

// ---------------------------------------------------------------------------
// temp_box.rs — transformed_temp_capsule_trace
// ---------------------------------------------------------------------------
Trace transformed_temp_capsule_trace(const float start[3], const float end[3],
                                     const float mins[3], const float maxs[3],
                                     const float origin[3],
                                     const float box_mins[3], const float box_maxs[3],
                                     uint32_t contents, uint32_t mask);

// ---------------------------------------------------------------------------
// area_entities.rs — AreaBounds / AreaSector / AreaEntityWorld (schema only)
// ---------------------------------------------------------------------------
constexpr size_t AREA_SECTOR_COUNT = 1024;
constexpr uint16_t AREA_ROOT_SECTOR = 1;
constexpr uint16_t AREA_FIRST_FREE_SECTOR = 2;
constexpr float AREA_NODE_MIN_EXTENT = 512.0f;

struct AreaBounds {
    float mid[3] = {0, 0, 0};
    float half[3] = {0, 0, 0};
};

struct AreaSector {
    uint32_t contents_entities = 0;
    uint32_t linkcontents_entities = 0;
    uint16_t entities = 0;
    float dist = 0.0f;
    uint16_t axis = 0;
    uint16_t parent_or_next_free = 0;
    uint16_t child[2] = {0, 0};
};

struct AreaEntityLink {
    uint16_t world_sector = 0;
    uint16_t next_entity_in_world_sector = 0;
    uint32_t linkcontents = 0;
    float linkmin[2] = {0, 0};
    float linkmax[2] = {0, 0};
};

struct AreaEntityWorld {
    AreaBounds world;
    uint16_t free_head = AREA_FIRST_FREE_SECTOR;
    AreaSector sectors[AREA_SECTOR_COUNT] = {};
    AreaEntityLink links[AREA_SECTOR_COUNT] = {};
    uint32_t entity_contents[AREA_SECTOR_COUNT] = {};
    const AreaBounds* entity_bounds = nullptr; // [AREA_SECTOR_COUNT] lazily provided by consumer
};

// ---------------------------------------------------------------------------
// capsule_query.rs — capsule + swept-point helpers (used by temp_box / cmodel)
// ---------------------------------------------------------------------------
namespace capsule_query {
struct SweepHit {
    float fraction = 1.0f;
    float normal[3] = {0, 0, 0};
    bool startsolid = false;
    bool allsolid = false;
};

// swept-point vs a sphere of radius r, with capsule half-segment `half_seg`.
std::optional<SweepHit> sweep_point(const float start[3], const float end[3],
                                    float r, float half_seg);
} // namespace capsule_query

using std::vector;

// ---------------------------------------------------------------------------
// layouts.rs types — the loaded clipmap structural schema.
// ---------------------------------------------------------------------------

struct Cbrush {
    uint16_t numsides = 0;
    uint16_t glass_piece_index = 0;
    uint32_t sides = 0;
    uint32_t base_adjacent_side = 0;
    int16_t axial_material_num[6] = {};
};

struct Cbrushside {
    uint32_t plane = 0;
    uint16_t material_num = 0;
};

struct Cnode {
    uint32_t plane = 0;
    int16_t children[2] = {0, 0};
};

struct Cleaf {
    uint16_t first_coll_aabb_index = 0;
    uint16_t coll_aabb_count = 0;
    int32_t brush_contents = 0;
    int32_t terrain_contents = 0;
    int32_t leaf_brush_node = 0;
};

struct CleafBrushNode {
    int8_t axis = 0;
    int16_t leaf_brush_count = 0;
    int32_t contents = 0;
    uint16_t child_offset_0 = 0;
    uint16_t child_offset_1 = 0;
};

struct Cmodel {
    float bounds_mid_point[3] = {0, 0, 0};
    float bounds_half_size[3] = {0, 0, 0};
    float radius = 0.0f;
    int32_t leaf_brush_node = 0;
};

struct CollisionAabbTree {
    float origin[3] = {0, 0, 0};
    uint16_t material_index = 0;
    uint16_t child_count = 0;
    float half_size[3] = {0, 0, 0};
    int32_t u = 0;
};

struct CollisionPartition {
    uint8_t tri_count = 0;
    uint8_t border_count = 0;
    uint8_t first_vert_segment = 0;
    int32_t first_tri = 0;
    uint32_t borders = 0;
};

struct ClipMaterial {
    uint32_t name = 0;
    uint32_t surface_flags = 0;
    uint32_t content_flags = 0;
};

// ---------------------------------------------------------------------------
// mesh.rs — ClipAabbNode / ClipPartition / ClipBorder / MeshWalkCensus /
// ClipMeshTables / ClipMeshRef / aabb_forest_roots / trace_through_mesh*
// ---------------------------------------------------------------------------

struct ClipAabbNode {
    float origin[3] = {0, 0, 0};
    float half_size[3] = {0, 0, 0};
    uint16_t material_index = 0;
    uint16_t child_count = 0;
    int32_t u = 0;
};

struct ClipPartition {
    uint8_t tri_count = 0;
    uint8_t border_count = 0;
    uint8_t first_vert_segment = 0;
    int32_t first_tri = 0;
    uint32_t borders = 0;
};

struct ClipBorder {
    float dist_eq[3] = {0, 0, 0}; // [nx, ny, dist]
    float z_base = 0.0f;
    float z_slope = 0.0f;
    float start = 0.0f;
    float length = 0.0f;
};

struct MeshWalkCensus {
    uint32_t tris_tested = 0;
    uint32_t aabb_nodes_visited = 0;
    uint8_t forest_fallback = 0;
    uint32_t aabb_roots = 0;
};

struct ClipMeshRef {
    const float* verts = nullptr;            // [N][3] flat
    size_t vert_count = 0;
    const uint16_t* tri_indices = nullptr;   // [3*T]
    size_t tri_index_count = 0;
    const uint8_t* tri_edge_is_walkable = nullptr; // bit array
    size_t tri_edge_is_walkable_bytes = 0;
    const uint32_t* tri_surface_flags = nullptr;
    size_t tri_surface_flags_count = 0;
    const uint32_t* tri_content_flags = nullptr;
    size_t tri_content_flags_count = 0;
    const ClipAabbNode* aabb_trees = nullptr;
    size_t aabb_tree_count = 0;
    const ClipPartition* partitions = nullptr;
    size_t partition_count = 0;
    const ClipBorder* borders = nullptr;
    size_t border_count = 0;
    const uint16_t* aabb_roots = nullptr;
    size_t aabb_root_count = 0;
};

struct ClipMeshTables {
    vector<std::array<float,3>> verts;
    vector<uint16_t> tri_indices;
    vector<uint8_t> tri_edge_is_walkable;
    vector<uint32_t> tri_surface_flags;
    vector<uint32_t> tri_content_flags;
    vector<ClipAabbNode> aabb_trees;
    vector<ClipPartition> partitions;
    vector<ClipBorder> borders;
    vector<uint16_t> aabb_roots;
};

// AABB forest root indexes (roots are AABB nodes whose u is not a child).
vector<uint16_t> aabb_forest_roots(const ClipAabbNode* trees, size_t tree_count);

// Mesh tracing entry points.
Trace trace_through_mesh(const ClipMeshRef& mesh, const TraceExtents& ext);
std::pair<Trace, MeshWalkCensus> trace_through_mesh_with_census(const ClipMeshRef& mesh,
                                                                  const TraceExtents& ext);
void trace_through_mesh_into(const ClipMeshRef& mesh, const TraceExtents& ext,
                             Trace* best, MeshWalkCensus* census);

// Post-build helpers that flatten partition leaves into per-triangle arrays.
vector<uint32_t> flatten_tri_surface_flags(const std::pair<uint16_t,int32_t>* aabb_leaves,
                                            size_t aabb_leaves_count,
                                            const std::pair<uint8_t,int32_t>* partitions,
                                            size_t partition_count,
                                            const uint32_t* material_surface_flags,
                                            size_t material_surface_flags_count,
                                            size_t tri_count);
vector<uint16_t> flatten_tri_material_index(const std::pair<uint16_t,int32_t>* aabb_leaves,
                                             size_t aabb_leaves_count,
                                             const std::pair<uint8_t,int32_t>* partitions,
                                             size_t partition_count,
                                             size_t tri_count);

// ---------------------------------------------------------------------------
// world_trace.rs — ClipWorldWinner / WorldTraceCensus / brush+tree+mesh combos
// ---------------------------------------------------------------------------

enum class ClipWorldWinner : uint8_t {
    Open = 0,
    Brush = 1,
    Mesh = 2,
    StaticModel = 3,
};

struct WorldTraceCensus {
    uint32_t leaves_visited = 0;
    uint32_t early_out_n = 0;
    uint32_t brush_ids_n = 0;
    MeshWalkCensus mesh{};
};

struct TraceLeafBrushesIntoOptions {
    const ClipMapRef* map = nullptr;
    const ClipLeaf* leaf = nullptr;
    const TraceExtents* ext = nullptr;
    std::function<bool(uint16_t)> glass_is_solid;
    Trace* best = nullptr;
};

// Brush-leaf trace used by the tree walk.
uint32_t trace_leaf_brushes_into(const TraceLeafBrushesIntoOptions& opts);

struct TraceLeafMeshIntoOptions {
    const ClipMeshRef* mesh = nullptr;
    const ClipLeaf* leaf = nullptr;
    const TraceExtents* ext = nullptr;
    bool has_leaf_aabbs = false;
    Trace* best = nullptr;
    MeshWalkCensus* census = nullptr;
    vector<uint16_t>* scratch = nullptr;
};

void trace_leaf_mesh_into(const TraceLeafMeshIntoOptions& opts);

void finish_mesh_forest_fallback(const ClipMeshRef& mesh, bool has_leaf_aabbs,
                                 const TraceExtents& ext, Trace* best,
                                 ClipWorldWinner* winner, MeshWalkCensus* census);

std::pair<Trace, ClipWorldWinner> combine_brush_mesh(Trace brush, std::optional<Trace> mesh);

void absorb_hit(Trace* best, const Trace* other, ClipWorldWinner src, ClipWorldWinner* winner);
void finalize_winner(Trace* best, ClipWorldWinner* winner);

// Combined brush+tree+mesh trace. The `map` is a ClipMapRef whose brushes satisfy
// BrushView. To keep this TU from depending on the generic BrushView dispatch, we
// expose a template that the consumer instantiates with their brush backend.
template <typename B>
std::tuple<Trace, ClipWorldWinner, WorldTraceCensus>
trace_brush_and_mesh_with_census(const ClipMapRef& map, const B& brushes,
                                 const ClipMeshRef& mesh, const TraceExtents& ext,
                                 std::function<bool(uint16_t)> glass_is_solid) {
    Trace best;
    best.fraction = 1.0f;
    std::memcpy(best.endpos, ext.end, sizeof(best.endpos));
    ClipWorldWinner winner = ClipWorldWinner::Open;
    WorldTraceCensus census{};

    const bool no_brushes = brushes.brush_count == 0;
    const bool no_mesh = mesh.tri_index_count < 3;
    if (no_brushes && no_mesh) return {best, winner, census};

    if (map.nodes == nullptr || map.leaves == nullptr ||
        map.node_count == 0 || map.leaf_count == 0) {
        if (!no_brushes) {
            Trace brush = trace_linear_with_glass(map, ext, glass_is_solid);
            absorb_hit(&best, &brush, ClipWorldWinner::Brush, &winner);
        }
        if (!no_mesh) {
            float f0 = best.fraction;
            trace_through_mesh_into(mesh, ext, &best, &census.mesh);
            census.mesh.forest_fallback = 1;
            if (best.fraction < f0) winner = ClipWorldWinner::Mesh;
        }
        finalize_winner(&best, &winner);
        return {best, winner, census};
    }

    const bool has_leaf_aabbs = !no_mesh && leaves_have_coll_aabb(map.leaves, map.leaf_count);
    std::function<float()> frac_fn = [&best]() { return best.fraction; };
    vector<uint16_t> scratch;
    TreeWalkCensus walk = walk_clip_tree(map, ext, frac_fn, [&](const ClipLeaf& leaf) {
        census.leaves_visited = census.leaves_visited + 1;
        if (best.fraction == 0.0f) { frac_fn = []() { return 0.0f; }; return; }
        if (!no_brushes) {
            float f0 = best.fraction;
            TraceLeafBrushesIntoOptions lo;
            lo.map = &map; lo.leaf = &leaf; lo.ext = &ext;
            lo.glass_is_solid = glass_is_solid; lo.best = &best;
            uint32_t n = trace_leaf_brushes_into(lo);
            census.brush_ids_n = census.brush_ids_n + n;
            if (best.fraction < f0) winner = ClipWorldWinner::Brush;
            if (best.fraction == 0.0f) { frac_fn = []() { return 0.0f; }; return; }
        }
        if (!no_mesh) {
            float f0 = best.fraction;
            TraceLeafMeshIntoOptions mo;
            mo.mesh = &mesh; mo.leaf = &leaf; mo.ext = &ext;
            mo.has_leaf_aabbs = has_leaf_aabbs; mo.best = &best;
            mo.census = &census.mesh; mo.scratch = &scratch;
            trace_leaf_mesh_into(mo);
            if (best.fraction < f0) winner = ClipWorldWinner::Mesh;
        }
        frac_fn = [&best]() { return best.fraction; };
    });
    census.early_out_n = walk.early_out_n;
    if (!no_mesh) {
        finish_mesh_forest_fallback(mesh, has_leaf_aabbs, ext, &best, &winner, &census.mesh);
    }
    finalize_winner(&best, &winner);
    return {best, winner, census};
}

// Non-template convenience for the brush-list-only path.
std::tuple<Trace, ClipWorldWinner, WorldTraceCensus>
trace_brush_and_mesh_with_census_brushes(const ClipMapRef& map,
                                          const trace_iw4::BrushRef* brushes,
                                          size_t brush_count,
                                          const ClipMeshRef& mesh,
                                          const TraceExtents& ext,
                                          std::function<bool(uint16_t)> glass_is_solid);

void absorb_hit(Trace* best, const Trace* other, ClipWorldWinner src, ClipWorldWinner* winner);
void finalize_winner(const Trace* best, ClipWorldWinner* winner);

// ---------------------------------------------------------------------------
// xmodel_trace.rs — XModelColl* / ClipStaticModel / RigidXform / XModelAnimBone
// ---------------------------------------------------------------------------

struct XModelCollTri {
    float plane[4] = {0, 0, 0, 0};
    float svec[4] = {0, 0, 0, 0};
    float tvec[4] = {0, 0, 0, 0};
};

struct XModelCollSurf {
    vector<XModelCollTri> tris;
    float midpoint[3] = {0, 0, 0};
    float half_size[3] = {0, 0, 0};
    int32_t bone_idx = 0;
    uint32_t contents = 0;
    uint32_t surf_flags = 0;
};

struct XModelColl {
    int16_t coll_lod = 0;
    uint32_t contents = 0;
    vector<XModelCollSurf> surfs;
};

struct ClipStaticModel {
    float origin[3] = {0, 0, 0};
    float inv_scaled_axis[3][3] = {};
    float bounds_mid[3] = {0, 0, 0};
    float bounds_half[3] = {0, 0, 0};
    XModelColl coll;
};

struct StaticModelHit {
    size_t index = 0;
    int32_t bone_idx = 0;
};

struct StaticModelWalkStats {
    uint32_t considered = 0;
    uint32_t aabb_miss = 0;
    uint32_t traced = 0;
};

struct RigidXform {
    float axis[3][3] = {};
    float trans[3] = {0, 0, 0};

    static RigidXform identity() {
        RigidXform r;
        r.axis[0][0] = 1.0f; r.axis[0][1] = 0.0f; r.axis[0][2] = 0.0f;
        r.axis[1][0] = 0.0f; r.axis[1][1] = 1.0f; r.axis[1][2] = 0.0f;
        r.axis[2][0] = 0.0f; r.axis[2][1] = 0.0f; r.axis[2][2] = 1.0f;
        return r;
    }
};

struct XModelAnimBone {
    RigidXform posed;
    RigidXform bind;
};

// XModel trace primitives.
int32_t xmodel_trace_line(const XModelColl& model, Trace* results,
                          const float local_start[3], const float local_end[3],
                          uint32_t contentmask);
int32_t xmodel_trace_line_animated(const XModelColl& model, Trace* results,
                                   const float local_start[3], const float local_end[3],
                                   uint32_t contentmask,
                                   const XModelAnimBone* bones, size_t bone_count,
                                   const uint32_t* hide, size_t hide_count);
int32_t trace_static_model(const ClipStaticModel& sm, Trace* results,
                           const float start[3], const float end[3],
                           uint32_t contentmask);

template <typename I>
std::optional<StaticModelHit> point_trace_static_models(I&& models,
                                                        const float start[3],
                                                        const float end[3],
                                                        uint32_t contentmask,
                                                        Trace* results) {
    StaticModelWalkStats stats{};
    return point_trace_static_models_stats(std::forward<I>(models), start, end,
                                           contentmask, results, &stats);
}

template <typename I>
std::optional<StaticModelHit> point_trace_static_models_stats(I&& models,
                                                             const float start[3],
                                                             const float end[3],
                                                             uint32_t contentmask,
                                                             Trace* results,
                                                             StaticModelWalkStats* stats) {
    std::optional<StaticModelHit> winner;
    size_t index = 0;
    for (auto& sm : models) {
        if ((sm.coll.contents & contentmask) == 0) { ++index; continue; }
        stats->considered = stats->considered + 1;
        if (trace_box_misses(start, end, sm.bounds_mid, sm.bounds_half, results->fraction)) {
            stats->aabb_miss = stats->aabb_miss + 1;
            ++index; continue;
        }
        stats->traced = stats->traced + 1;
        int32_t bone = trace_static_model(sm, results, start, end, contentmask);
        if (bone >= 0) {
            winner = StaticModelHit{index, bone};
        }
        ++index;
    }
    return winner;
}

bool trace_box_misses(const float start[3], const float end[3],
                      const float mid[3], const float half[3], float fraction);

// ---------------------------------------------------------------------------
} // namespace clipmap_iw4

