#pragma once
// Recovers the camera motion between two rendered frames from the game's own matrices.
// Streamline gives viewToClip P (row-vector, reversed-Z infinite; view x=right y=up z=forward) and
// clipToPrevClip C (row-vector: clip_prev = clip_current * C). For a view-space point v of the current
// frame, the same world point in the previous frame's view space is v_prev = R * v + t, so t is the
// current camera's position in the previous view space. Recovered numerically: far points give R
// (Horn's quaternion method), finite-depth points then give t.
#include <array>
#include <cmath>

namespace fw {

using M4 = std::array<std::array<double, 4>, 4>;
using V3 = std::array<double, 3>;
using R3 = std::array<std::array<double, 3>, 3>;

inline M4 m4_from(const float* f) {
    M4 m{};
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) m[i][j] = f[i * 4 + j];
    return m;
}
inline std::array<double, 4> mul(const std::array<double, 4>& v, const M4& m) {
    std::array<double, 4> r{};
    for (int j = 0; j < 4; ++j) for (int k = 0; k < 4; ++k) r[j] += v[k] * m[k][j];
    return r;
}
inline bool invert(const M4& m, M4& out) {
    M4 a = m, b{};
    for (int i = 0; i < 4; ++i) b[i][i] = 1;
    for (int c = 0; c < 4; ++c) {
        int p = c;
        for (int r = c + 1; r < 4; ++r) if (std::fabs(a[r][c]) > std::fabs(a[p][c])) p = r;
        std::swap(a[c], a[p]); std::swap(b[c], b[p]);
        const double d = a[c][c];
        if (std::fabs(d) < 1e-12) return false;
        for (int j = 0; j < 4; ++j) { a[c][j] /= d; b[c][j] /= d; }
        for (int r = 0; r < 4; ++r) {
            if (r == c) continue;
            const double f = a[r][c];
            for (int j = 0; j < 4; ++j) { a[r][j] -= f * a[c][j]; b[r][j] -= f * b[c][j]; }
        }
    }
    out = b;
    return true;
}

// Rotation R minimising sum |R a_i - b_i|^2 (Horn, quaternion via power iteration).
template <std::size_t N>
inline R3 fit_rotation(const std::array<V3, N>& a, const std::array<V3, N>& b) {
    double S[3][3] = {};
    for (std::size_t n = 0; n < N; ++n)
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) S[i][j] += a[n][i] * b[n][j];
    const double K[4][4] = {
        {S[0][0] + S[1][1] + S[2][2], S[1][2] - S[2][1], S[2][0] - S[0][2], S[0][1] - S[1][0]},
        {S[1][2] - S[2][1], S[0][0] - S[1][1] - S[2][2], S[0][1] + S[1][0], S[2][0] + S[0][2]},
        {S[2][0] - S[0][2], S[0][1] + S[1][0], -S[0][0] + S[1][1] - S[2][2], S[1][2] + S[2][1]},
        {S[0][1] - S[1][0], S[2][0] + S[0][2], S[1][2] + S[2][1], -S[0][0] - S[1][1] + S[2][2]}};
    double q[4] = {1, 0, 0, 0};
    const double shift = 4.0 * static_cast<double>(N);
    for (int it = 0; it < 60; ++it) {
        double r[4] = {};
        for (int i = 0; i < 4; ++i) { for (int j = 0; j < 4; ++j) r[i] += K[i][j] * q[j]; r[i] += shift * q[i]; }
        const double l = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3]);
        for (int i = 0; i < 4; ++i) q[i] = r[i] / l;
    }
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    return R3{{{1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)},
               {2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)},
               {2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)}}};
}

struct FrameMotion {
    R3 rotation{};   // v_prev = rotation * v_current + translation (view space)
    V3 translation{};
    double residual = 0;  // RMS of the fit, view-space units at the sample depths
    bool valid = false;
};

// sample_depths are view-space distances in game units (UE: cm) for the translation fit.
inline FrameMotion recover_motion(const float* view_to_clip, const float* clip_to_prev_clip) {
    FrameMotion out;
    const M4 P = m4_from(view_to_clip), C = m4_from(clip_to_prev_clip);
    M4 Pinv;
    if (!invert(P, Pinv)) return out;
    // The near-plane scale: for reversed-Z infinite projections clip.z = near * w_view, i.e. the
    // clip-space z of a point at view depth d is near/d. near = P[3][2] (row-vector layout).
    const double near_plane = std::fabs(P[3][2]) > 1e-9 ? std::fabs(P[3][2]) : 1.0;
    auto view_point = [&](double x, double y, double z_clip, const M4& map, bool mapped) -> V3 {
        std::array<double, 4> c{x, y, z_clip, 1.0};
        if (mapped) c = mul(c, map);
        const auto v = mul(c, Pinv);
        return {v[0] / v[3], v[1] / v[3], v[2] / v[3]};
    };
    constexpr std::size_t kGrid = 25;
    std::array<V3, kGrid> cur{}, prev{};
    std::size_t n = 0;
    for (int ix = -2; ix <= 2; ++ix)
        for (int iy = -2; iy <= 2; ++iy) {
            const double x = 0.4 * ix, y = 0.4 * iy, z = near_plane * 1e-6;  // effectively infinitely far
            V3 a = view_point(x, y, z, C, false), b = view_point(x, y, z, C, true);
            const double la = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
            const double lb = std::sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
            if (!(la > 0) || !(lb > 0)) return out;
            for (int k = 0; k < 3; ++k) { a[k] /= la; b[k] /= lb; }
            cur[n] = a; prev[n] = b; ++n;
        }
    out.rotation = fit_rotation(cur, prev);
    // Translation from points at 1 m, 3 m and 10 m (game units assumed cm; scale-free anyway).
    V3 sum{};
    int count = 0;
    double sq = 0;
    for (double depth : {100.0, 300.0, 1000.0})
        for (int ix = -1; ix <= 1; ++ix)
            for (int iy = -1; iy <= 1; ++iy) {
                const double z = near_plane / depth;
                const V3 a = view_point(0.5 * ix, 0.5 * iy, z, C, false), b = view_point(0.5 * ix, 0.5 * iy, z, C, true);
                for (int i = 0; i < 3; ++i) {
                    const double ra = out.rotation[i][0] * a[0] + out.rotation[i][1] * a[1] + out.rotation[i][2] * a[2];
                    sum[i] += b[i] - ra;
                }
                ++count;
            }
    for (int i = 0; i < 3; ++i) out.translation[i] = sum[i] / count;
    for (double depth : {100.0, 300.0, 1000.0})
        for (int ix = -1; ix <= 1; ++ix) {
            const double z = near_plane / depth;
            const V3 a = view_point(0.5 * ix, 0.0, z, C, false), b = view_point(0.5 * ix, 0.0, z, C, true);
            for (int i = 0; i < 3; ++i) {
                const double ra = out.rotation[i][0] * a[0] + out.rotation[i][1] * a[1] + out.rotation[i][2] * a[2];
                const double e = b[i] - ra - out.translation[i];
                sq += e * e;
            }
        }
    out.residual = std::sqrt(sq / 9.0);
    out.valid = std::isfinite(out.translation[0]) && std::isfinite(out.translation[1]) && std::isfinite(out.translation[2]);
    return out;
}

}  // namespace fw
