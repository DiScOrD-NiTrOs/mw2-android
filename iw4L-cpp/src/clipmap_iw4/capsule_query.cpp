// Port of iw4L crates/clipmap_iw4/src/capsule_query.rs: swept-sphere vs point+
// half-segment circle sweep. This is the primitive used by temp_box and cmodel
// capsule traces.
#include "clipmap_iw4/clipmap_iw4.h"

#include <cmath>
#include <optional>
#include <cstring>

namespace clipmap_iw4 {
namespace capsule_query {

// f64 helpers — the Rust code uses f64 for the sweep math (capsule query uses
// f64 to match the original clipmap precision). Keep it f64 here.
namespace {
    using F64 = double;

    F64 sqrtf64(F64 x) { return std::sqrt(x < 0.0 ? 0.0 : x); }

    // squared length
    F64 len2(const F64 v[3]) {
        return v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
    }

    // normalize into `out`; returns length.
    F64 normalize_to(F64 out[3], const F64 v[3]) {
        const F64 l2 = len2(v);
        const F64 len = sqrtf64(l2);
        if (len > 0.0) {
            out[0] = v[0] / len;
            out[1] = v[1] / len;
            out[2] = v[2] / len;
        } else {
            out[0] = 0.0; out[1] = 0.0; out[2] = 0.0;
        }
        return len;
    }
}

std::optional<SweepHit> sweep_point(const float start_f[3], const float end_f[3],
                                     float radius_f, float half_seg_f) {
    SweepHit hit;
    hit.fraction = 1.0f;
    std::memset(hit.normal, 0, sizeof(hit.normal));
    hit.startsolid = false;
    hit.allsolid = false;

    const F64 r = static_cast<F64>(radius_f);
    const F64 half_seg = static_cast<F64>(half_seg_f);

    const F64 s[3] = {static_cast<F64>(start_f[0]), static_cast<F64>(start_f[1]), static_cast<F64>(start_f[2])};
    const F64 e[3] = {static_cast<F64>(end_f[0]),   static_cast<F64>(end_f[1]),   static_cast<F64>(end_f[2])};

    const F64 d[3] = {e[0]-s[0], e[1]-s[1], e[2]-s[2]};
    const F64 d2 = len2(d);
    const F64 inv_d = (d2 > 0.0) ? (1.0 / std::sqrt(d2)) : 0.0;

    // The sweep is performed against the origin (a point) in the relative frame
    // created by transformed_temp_capsule_trace. That means we sweep a sphere of
    // radius (r + 0) against a point, but the real call pairs two capsules whose
    // radii have already been summed by the caller. In the clipmap code, this
    // function is always invoked in a relative frame where the "fixed" object is a
    // point at the origin with a half-segment of `half_seg`, and the "moving" object
    // is a sphere of radius `r` swept along the segment. That matches the original
    // implementation: sweep_point(start, end, r, half_seg) sweeps the circle of
    // radius r+half_seg around the segment against the origin.

    const F64 R = r + half_seg;
    const F64 R2 = R * R;

    // The segment is s -> e. Closest point on the infinite line to the origin:
    F64 t = 0.0;
    if (d2 > 0.0) {
        t = -(s[0]*d[0] + s[1]*d[1] + s[2]*d[2]) / d2;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
    }
    F64 c[3];
    for (int i = 0; i < 3; ++i) c[i] = s[i] + t * d[i];

    const F64 c2 = len2(c);
    if (c2 > R2) {
        // No hit.
        return hit;
    }

    // The segment endpoint circles have radius R. Find the earliest fraction where
    // the segment endpoint reaches distance R from the origin.
    // We do a swept-sphere test against a point: solve |s + t*d| = R.
    // (s + t*d)^2 = R^2  =>  d2*t^2 + 2*(s.d)*t + (|s|^2 - R^2) = 0
    const F64 s2 = len2(s);
    const F64 sd = s[0]*d[0] + s[1]*d[1] + s[2]*d[2];
    const F64 A = d2;
    const F64 B = 2.0 * sd;
    const F64 C = s2 - R2;
    F64 t_hit = 2.0; // past the segment
    if (A > 0.0) {
        const F64 disc = B*B - 4.0*A*C;
        if (disc >= 0.0) {
            const F64 sqrt_disc = std::sqrt(disc);
            F64 t1 = (-B - sqrt_disc) / (2.0*A);
            F64 t2 = (-B + sqrt_disc) / (2.0*A);
            if (t1 > t2) { const F64 tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 >= 0.0 && t1 <= 1.0 && t1 < t_hit) t_hit = t1;
            if (t2 >= 0.0 && t2 <= 1.0 && t2 < t_hit) t_hit = t2;
        }
    } else {
        // Degenerate segment: treat as a point at s.
        if (s2 <= R2) {
            t_hit = 0.0;
        }
    }

    if (t_hit <= 1.0) {
        hit.fraction = static_cast<float>(t_hit);
        // normal = from hit point toward origin (for capsule sweep the normal is
        // the normalized vector from the hit point on the segment to the origin,
        // scaled by the fact this is a sphere-vs-point test).
        F64 hp[3];
        for (int i = 0; i < 3; ++i) hp[i] = s[i] + t_hit * d[i];
        F64 n[3];
        normalize_to(n, hp); // hp points from origin to hit point; we want origin->hp reversed
        for (int i = 0; i < 3; ++i) hit.normal[i] = static_cast<float>(-n[i]);
        hit.startsolid = (t_hit == 0.0 && s2 <= R2);
        hit.allsolid = false;
        return hit;
    }
    return std::nullopt;
}

} // namespace capsule_query
} // namespace clipmap_iw4
