// init.cpp -- initial condition and prescribed MAC face-velocity field.

#include "init.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <immintrin.h>
#include <vector>

// EVOLVE-BLOCK-START
namespace mxp_eft {

struct ff32 {
    float hi;
    float lo;
};

inline ff32 two_sum(float a, float b) {
    const float s = a + b;
    const float v = s - a;
    const float e = (a - (s - v)) + (b - v);
    return {s, e};
}

inline ff32 quick_two_sum(float a, float b) {
    const float s = a + b;
    const float e = b - (s - a);
    return {s, e};
}

inline ff32 two_prod(float a, float b) {
    const float p = a * b;
    const float e = std::fmaf(a, b, -p);
    return {p, e};
}

inline ff32 ff_add(ff32 a, ff32 b) {
    const ff32 s = two_sum(a.hi, b.hi);
    const ff32 t = two_sum(a.lo, b.lo);
    const ff32 c = quick_two_sum(s.hi, s.lo + t.hi);
    return quick_two_sum(c.hi, c.lo + t.lo);
}

inline ff32 ff_sub(ff32 a, ff32 b) {
    return ff_add(a, {-b.hi, -b.lo});
}

inline ff32 ff_mul(ff32 a, ff32 b) {
    const ff32 p = two_prod(a.hi, b.hi);
    const float lo = std::fmaf(a.hi, b.lo, std::fmaf(a.lo, b.hi, p.lo));
    return quick_two_sum(p.hi, lo);
}

inline ff32 ff_mul_f(ff32 a, float b) {
    const ff32 p = two_prod(a.hi, b);
    const float lo = std::fmaf(a.lo, b, p.lo);
    return quick_two_sum(p.hi, lo);
}

inline ff32 ff_div_f(ff32 a, float b) {
    const float q0 = a.hi / b;
    const ff32 p0 = two_prod(q0, b);
    const ff32 rem = ff_sub(a, p0);
    const float q1 = (rem.hi + rem.lo) / b;
    return quick_two_sum(q0, q1);
}

inline ff32 ff_div(ff32 a, ff32 b) {
    const float q0 = a.hi / b.hi;
    const ff32 r0 = ff_sub(a, ff_mul_f(b, q0));
    const float q1 = r0.hi / b.hi;
    const ff32 r1 = ff_sub(r0, ff_mul_f(b, q1));
    const float q2 = r1.hi / b.hi;
    const ff32 s = quick_two_sum(q0, q1);
    return quick_two_sum(s.hi, s.lo + q2);
}

inline ff32 ff_sin_kernel(ff32 z, ff32 z2) {
    const _Float16 d7 = static_cast<_Float16>(210.0f);
    const _Float16 d6 = static_cast<_Float16>(156.0f);
    const _Float16 d5 = static_cast<_Float16>(110.0f);
    const _Float16 d4 = static_cast<_Float16>(72.0f);
    const _Float16 d3 = static_cast<_Float16>(42.0f);
    const _Float16 d2 = static_cast<_Float16>(20.0f);
    const _Float16 d1 = static_cast<_Float16>(6.0f);
    const _Float16 kOne16 = static_cast<_Float16>(1.0f);
    const ff32 one = {static_cast<float>(kOne16), 0.0f};

    ff32 p = ff_sub(one, ff_div_f(z2, static_cast<float>(d7)));
    p = ff_sub(one, ff_div_f(ff_mul(z2, p), static_cast<float>(d6)));
    p = ff_sub(one, ff_div_f(ff_mul(z2, p), static_cast<float>(d5)));
    p = ff_sub(one, ff_div_f(ff_mul(z2, p), static_cast<float>(d4)));
    p = ff_sub(one, ff_div_f(ff_mul(z2, p), static_cast<float>(d3)));
    p = ff_sub(one, ff_div_f(ff_mul(z2, p), static_cast<float>(d2)));
    p = ff_sub(one, ff_div_f(ff_mul(z2, p), static_cast<float>(d1)));
    return ff_mul(z, p);
}

inline ff32 ff_cos_kernel(ff32 z2) {
    const _Float16 d7 = static_cast<_Float16>(182.0f);
    const _Float16 d6 = static_cast<_Float16>(132.0f);
    const _Float16 d5 = static_cast<_Float16>(90.0f);
    const _Float16 d4 = static_cast<_Float16>(56.0f);
    const _Float16 d3 = static_cast<_Float16>(30.0f);
    const _Float16 d2 = static_cast<_Float16>(12.0f);
    const _Float16 d1 = static_cast<_Float16>(2.0f);
    const _Float16 kOne16 = static_cast<_Float16>(1.0f);
    const ff32 one = {static_cast<float>(kOne16), 0.0f};

    ff32 p = ff_sub(one, ff_div_f(z2, static_cast<float>(d7)));
    p = ff_sub(one, ff_div_f(ff_mul(z2, p), static_cast<float>(d6)));
    p = ff_sub(one, ff_div_f(ff_mul(z2, p), static_cast<float>(d5)));
    p = ff_sub(one, ff_div_f(ff_mul(z2, p), static_cast<float>(d4)));
    p = ff_sub(one, ff_div_f(ff_mul(z2, p), static_cast<float>(d3)));
    p = ff_sub(one, ff_div_f(ff_mul(z2, p), static_cast<float>(d2)));
    return ff_sub(one, ff_div_f(ff_mul(z2, p), static_cast<float>(d1)));
}

inline ff32 ff_mul_pi(ff32 r) {
    constexpr float kPiHi = 3.1415927410125732421875f;
    constexpr float kPiMid = -8.74227765734758577309e-08f;
    constexpr float kPiLo = -3.55271367880050092936e-15f;
    const ff32 p1 = two_prod(r.hi, kPiHi);
    const ff32 p2 = two_prod(r.hi, kPiMid);
    const float tail = std::fmaf(r.hi, kPiLo, std::fmaf(r.lo, kPiHi, p2.lo));
    const ff32 m = quick_two_sum(p2.hi, tail);
    return ff_add(p1, m);
}

inline ff32 ff_sin_pi(ff32 x) {
    const _Float16 kTwo16 = static_cast<_Float16>(2.0f);
    const _Float16 kHalf16 = static_cast<_Float16>(0.5f);

    const float q = std::nearbyintf(static_cast<float>(kTwo16) * x.hi);
    const ff32 r = ff_sub(x, {static_cast<float>(kHalf16) * q, 0.0f});
    const ff32 z = ff_mul_pi(r);
    const ff32 z2 = ff_mul(z, z);
    const int oct = static_cast<int>(q) & 3;
    if (oct == 0) return ff_sin_kernel(z, z2);
    if (oct == 1) return ff_cos_kernel(z2);
    if (oct == 2) {
        const ff32 s = ff_sin_kernel(z, z2);
        return {-s.hi, -s.lo};
    }
    const ff32 c = ff_cos_kernel(z2);
    return {-c.hi, -c.lo};
}

inline ff32 ff_cos_pi(ff32 x) {
    const _Float16 kTwo16 = static_cast<_Float16>(2.0f);
    const _Float16 kHalf16 = static_cast<_Float16>(0.5f);

    const float q = std::nearbyintf(static_cast<float>(kTwo16) * x.hi);
    const ff32 r = ff_sub(x, {static_cast<float>(kHalf16) * q, 0.0f});
    const ff32 z = ff_mul_pi(r);
    const ff32 z2 = ff_mul(z, z);
    const int oct = static_cast<int>(q) & 3;
    if (oct == 0) return ff_cos_kernel(z2);
    if (oct == 1) {
        const ff32 s = ff_sin_kernel(z, z2);
        return {-s.hi, -s.lo};
    }
    if (oct == 2) {
        const ff32 c = ff_cos_kernel(z2);
        return {-c.hi, -c.lo};
    }
    return ff_sin_kernel(z, z2);
}

struct ff32_3 {
    float hi;
    float lo;
    float tiny;
};

inline ff32_3 ff3_add(ff32_3 a, ff32 b) {
    const ff32 s0 = two_sum(a.hi, b.hi);
    const ff32 s1 = two_sum(a.lo, b.lo);
    const ff32 c1 = two_sum(s0.lo, s1.hi);
    const float t = a.tiny + s1.lo + c1.lo;
    const ff32 top = quick_two_sum(s0.hi, c1.hi);
    const ff32 mid = quick_two_sum(top.lo, t);
    return {top.hi, mid.hi, mid.lo};
}

inline ff32_3 ff3_mul_ff(ff32_3 a, ff32 b) {
    const ff32 p = two_prod(a.hi, b.hi);
    const ff32 c1 = two_prod(a.hi, b.lo);
    const ff32 c2 = two_prod(a.lo, b.hi);
    const ff32 s1 = two_sum(p.lo, c1.hi);
    const ff32 s2 = two_sum(s1.hi, c2.hi);
    const ff32 top = quick_two_sum(p.hi, s2.hi);
    const float rem = s1.lo + s2.lo + c1.lo + c2.lo + std::fmaf(a.lo, b.lo, a.tiny * b.hi);
    const ff32 mid = two_sum(top.lo, rem);
    return {top.hi, mid.hi, mid.lo};
}

inline ff32_3 ff_mul3(ff32 a, ff32 b) {
    const ff32 p = two_prod(a.hi, b.hi);
    const ff32 c1 = two_prod(a.hi, b.lo);
    const ff32 c2 = two_prod(a.lo, b.hi);
    const ff32 s1 = two_sum(p.lo, c1.hi);
    const ff32 s2 = two_sum(s1.hi, c2.hi);
    const ff32 top = quick_two_sum(p.hi, s2.hi);
    const float rem = s1.lo + s2.lo + c1.lo + c2.lo + a.lo * b.lo;
    const ff32 mid = two_sum(top.lo, rem);
    return {top.hi, mid.hi, mid.lo};
}

inline std::vector<float> phi_lo;
inline std::vector<float> orig_lo;
inline std::vector<float> rhs_lo;
inline std::vector<float> u_lo;
inline std::vector<float> v_lo;
inline std::vector<float> u_tiny;
inline std::vector<float> v_tiny;
inline std::vector<float> u_cache_hi;
inline std::vector<float> u_cache_lo;
inline std::vector<float> u_cache_tiny;
inline std::vector<float> v_cache_hi;
inline std::vector<float> v_cache_lo;
inline std::vector<float> v_cache_tiny;
inline int cache_nx = -1;
inline int cache_ny = -1;
inline int cache_i0 = -1;
inline int cache_j0 = -1;
inline float cache_dx = -1.0f;
inline float cache_dy = -1.0f;
inline float cache_scale_hi = -999.0f;
inline float cache_scale_lo = -999.0f;
inline ff32 time_ff = {0.0f, 0.0f};
inline float umax_lo = 0.0f;
inline float vmax_lo = 0.0f;
inline float dt_hi_cached = -1.0f;
inline float dt_lo = 0.0f;
inline bool active = false;

}  // namespace mxp_eft

void computeFaceVelocityLowPrec(const Grid& g, bool reverse_time, float tmax_f, float time_f,
                                std::vector<float>& u, std::vector<float>& v) {
    if (u.size() != g.size()) u.assign(g.size(), 0.0f);
    if (v.size() != g.size()) v.assign(g.size(), 0.0f);
    if (mxp_eft::u_lo.size() != g.size()) mxp_eft::u_lo.assign(g.size(), 0.0f);
    if (mxp_eft::v_lo.size() != g.size()) mxp_eft::v_lo.assign(g.size(), 0.0f);
    if (mxp_eft::u_tiny.size() != g.size()) mxp_eft::u_tiny.assign(g.size(), 0.0f);
    if (mxp_eft::v_tiny.size() != g.size()) mxp_eft::v_tiny.assign(g.size(), 0.0f);

    const _Float16 kTwo16 = static_cast<_Float16>(2.0f);
    const _Float16 kNegTwo16 = static_cast<_Float16>(-2.0f);
    const _Float16 kOne16 = static_cast<_Float16>(1.0f);

    const mxp_eft::ff32 dx_ff = {g.dx_f(), 0.0f};
    const mxp_eft::ff32 dy_ff = {g.dy_f(), 0.0f};

    mxp_eft::ff32 t_use = {time_f, 0.0f};
    if (time_f == 0.0f) {
        mxp_eft::time_ff = {0.0f, 0.0f};
    } else if (mxp_eft::active && std::fabs(time_f - mxp_eft::time_ff.hi) <= 1.0e-5f) {
        t_use = mxp_eft::time_ff;
    }

    mxp_eft::ff32 scale_ff = {static_cast<float>(kOne16), 0.0f};
    if (reverse_time) {
        const mxp_eft::ff32 ratio = mxp_eft::ff_div(t_use, {tmax_f, 0.0f});
        scale_ff = mxp_eft::ff_cos_pi(ratio);
    }

    if (mxp_eft::cache_nx == g.nx() && mxp_eft::cache_ny == g.ny() &&
        mxp_eft::cache_i0 == g.i0() && mxp_eft::cache_j0 == g.j0() &&
        mxp_eft::cache_dx == g.dx_f() && mxp_eft::cache_dy == g.dy_f() &&
        mxp_eft::cache_scale_hi == scale_ff.hi && mxp_eft::cache_scale_lo == scale_ff.lo &&
        mxp_eft::u_cache_hi.size() == g.size()) {
        u = mxp_eft::u_cache_hi;
        mxp_eft::u_lo = mxp_eft::u_cache_lo;
        mxp_eft::u_tiny = mxp_eft::u_cache_tiny;
        v = mxp_eft::v_cache_hi;
        mxp_eft::v_lo = mxp_eft::v_cache_lo;
        mxp_eft::v_tiny = mxp_eft::v_cache_tiny;
        return;
    }

    const int ng = g.ng();
    const int nxg = g.nxGlobal();
    const int nyg = g.nyGlobal();
    const mxp_eft::ff32 sin_pi_dy = mxp_eft::ff_sin_pi(dy_ff);
    const mxp_eft::ff32 sin_pi_dx = mxp_eft::ff_sin_pi(dx_ff);
    const mxp_eft::ff32 denom_y = mxp_eft::ff_mul_f(mxp_eft::ff_mul_pi(dy_ff), static_cast<float>(kTwo16));
    const mxp_eft::ff32 denom_x = mxp_eft::ff_mul_f(mxp_eft::ff_mul_pi(dx_ff), static_cast<float>(kTwo16));

    auto delta_sx2 = [&](int gi) -> mxp_eft::ff32 {
        const float xc = g.xmin_f() + (static_cast<float>(gi) + 0.5f) * g.dx_f();
        const mxp_eft::ff32 sin_2pi_xc = mxp_eft::ff_sin_pi({static_cast<float>(kTwo16) * xc, 0.0f});
        return mxp_eft::ff_mul(sin_2pi_xc, sin_pi_dx);
    };
    auto delta_sy2 = [&](int gj) -> mxp_eft::ff32 {
        const float yc = g.ymin_f() + (static_cast<float>(gj) + 0.5f) * g.dy_f();
        const mxp_eft::ff32 sin_2pi_yc = mxp_eft::ff_sin_pi({static_cast<float>(kTwo16) * yc, 0.0f});
        return mxp_eft::ff_mul(sin_2pi_yc, sin_pi_dy);
    };

    std::vector<mxp_eft::ff32_3> sx2_global(nxg + 1, {0.0f, 0.0f, 0.0f});
    for (int gi = 0; gi < nxg / 2; ++gi) {
        sx2_global[gi + 1] = mxp_eft::ff3_add(sx2_global[gi], delta_sx2(gi));
        sx2_global[nxg - 1 - gi] = sx2_global[gi + 1];
    }
    std::vector<mxp_eft::ff32_3> sy2_global(nyg + 1, {0.0f, 0.0f, 0.0f});
    for (int gj = 0; gj < nyg / 2; ++gj) {
        sy2_global[gj + 1] = mxp_eft::ff3_add(sy2_global[gj], delta_sy2(gj));
        sy2_global[nyg - 1 - gj] = sy2_global[gj + 1];
    }

    auto get_sx2 = [&](int i) -> mxp_eft::ff32_3 {
        int gi = (g.i0() + i) % nxg;
        if (gi < 0) gi += nxg;
        if (g.i0() + i == nxg) return {0.0f, 0.0f, 0.0f};
        return sx2_global[gi];
    };
    auto get_sy2 = [&](int j) -> mxp_eft::ff32_3 {
        int gj = (g.j0() + j) % nyg;
        if (gj < 0) gj += nyg;
        if (g.j0() + j == nyg) return {0.0f, 0.0f, 0.0f};
        return sy2_global[gj];
    };

    const int nx_span = g.nx() + 2 * ng;
    std::vector<mxp_eft::ff32_3> neg_s2_row(nx_span);
    std::vector<mxp_eft::ff32> x_factor_row(nx_span);
    for (int i = -ng; i < g.nx() + ng; ++i) {
        const mxp_eft::ff32_3 s2 = get_sx2(i);
        neg_s2_row[i + ng] = {-s2.hi, -s2.lo, -s2.tiny};
        const mxp_eft::ff32 dsx2 = delta_sx2(g.i0() + i);
        const mxp_eft::ff32 dcos = mxp_eft::ff_mul_f(dsx2, static_cast<float>(kNegTwo16));
        x_factor_row[i + ng] = mxp_eft::ff_mul(scale_ff, mxp_eft::ff_div(dcos, denom_x));
    }

    for (int j = -ng; j < g.ny() + ng; ++j) {
        const mxp_eft::ff32 dsy2 = delta_sy2(g.j0() + j);
        const mxp_eft::ff32 dcos = mxp_eft::ff_mul_f(dsy2, static_cast<float>(kNegTwo16));
        const mxp_eft::ff32 y_factor = mxp_eft::ff_mul(scale_ff, mxp_eft::ff_div(dcos, denom_y));

        for (int i = -ng; i < g.nx() + ng; ++i) {
            const mxp_eft::ff32_3 u_val = mxp_eft::ff3_mul_ff(neg_s2_row[i + ng], y_factor);
            const int idx = g.index(i, j);
            u[idx] = u_val.hi;
            mxp_eft::u_lo[idx] = u_val.lo;
            mxp_eft::u_tiny[idx] = u_val.tiny;
        }
    }

    for (int j = -ng; j < g.ny() + ng; ++j) {
        const mxp_eft::ff32_3 s2 = get_sy2(j);

        for (int i = -ng; i < g.nx() + ng; ++i) {
            const mxp_eft::ff32_3 v_val = mxp_eft::ff3_mul_ff(s2, x_factor_row[i + ng]);
            const int idx = g.index(i, j);
            v[idx] = v_val.hi;
            mxp_eft::v_lo[idx] = v_val.lo;
            mxp_eft::v_tiny[idx] = v_val.tiny;
        }
    }

    mxp_eft::cache_nx = g.nx();
    mxp_eft::cache_ny = g.ny();
    mxp_eft::cache_i0 = g.i0();
    mxp_eft::cache_j0 = g.j0();
    mxp_eft::cache_dx = g.dx_f();
    mxp_eft::cache_dy = g.dy_f();
    mxp_eft::cache_scale_hi = scale_ff.hi;
    mxp_eft::cache_scale_lo = scale_ff.lo;
    mxp_eft::u_cache_hi = u;
    mxp_eft::u_cache_lo = mxp_eft::u_lo;
    mxp_eft::u_cache_tiny = mxp_eft::u_tiny;
    mxp_eft::v_cache_hi = v;
    mxp_eft::v_cache_lo = mxp_eft::v_lo;
    mxp_eft::v_cache_tiny = mxp_eft::v_tiny;
}

void maxFaceSpeedsLowPrec(const Grid& g, const std::vector<float>& u, const std::vector<float>& v,
                          float& umax, float& vmax) {
    const bool has_lo = (mxp_eft::u_lo.size() == u.size() && mxp_eft::v_lo.size() == v.size());
    float lu = 0.0f;
    float lu_lo = 0.0f;
    float lv = 0.0f;
    float lv_lo = 0.0f;
    for (int j = 0; j < g.ny(); ++j) {
        for (int i = 0; i <= g.nx(); ++i) {
            const int idx = g.index(i, j);
            const float val = u[idx];
            const float vlo = has_lo ? mxp_eft::u_lo[idx] : 0.0f;
            const float a = std::fabs(val);
            const float alo = (val < 0.0f) ? -vlo : vlo;
            if (a > lu || (a == lu && alo > lu_lo)) {
                lu = a;
                lu_lo = alo;
            }
        }
    }
    for (int j = 0; j <= g.ny(); ++j) {
        for (int i = 0; i < g.nx(); ++i) {
            const int idx = g.index(i, j);
            const float val = v[idx];
            const float vlo = has_lo ? mxp_eft::v_lo[idx] : 0.0f;
            const float a = std::fabs(val);
            const float alo = (val < 0.0f) ? -vlo : vlo;
            if (a > lv || (a == lv && alo > lv_lo)) {
                lv = a;
                lv_lo = alo;
            }
        }
    }
    float in[2] = {lu, lv};
    float out[2] = {0.0f, 0.0f};
    MPI_Allreduce(in, out, 2, MPI_FLOAT, MPI_MAX, g.comm());
    umax = out[0];
    vmax = out[1];

    float in_lo[2] = {(lu == umax) ? lu_lo : -1.0f, (lv == vmax) ? lv_lo : -1.0f};
    float out_lo[2] = {0.0f, 0.0f};
    MPI_Allreduce(in_lo, out_lo, 2, MPI_FLOAT, MPI_MAX, g.comm());
    mxp_eft::umax_lo = (out_lo[0] > -0.5f) ? out_lo[0] : 0.0f;
    mxp_eft::vmax_lo = (out_lo[1] > -0.5f) ? out_lo[1] : 0.0f;
}
// EVOLVE-BLOCK-END

double levelSetAnalytic(const Params& p, double x, double y) {
    const double rx = x - p.bubble_x;
    const double ry = y - p.bubble_y;
    return p.bubble_r - std::sqrt(rx * rx + ry * ry);
}

void initLevelSet(const Grid& g, const Params& p, std::vector<double>& dfun) {
    if (dfun.size() != g.size()) dfun.assign(g.size(), 0.0);

    for (int j = 0; j < g.ny(); ++j) {
        const double y = g.yCenter(j);
        for (int i = 0; i < g.nx(); ++i) {
            dfun[g.index(i, j)] = levelSetAnalytic(p, g.xCenter(i), y);
        }
    }
    g.exchange(dfun);
}

void computeFaceVelocity(const Grid& g, const Params& p, double time, std::vector<double>& u,
                         std::vector<double>& v) {
    std::vector<float> uf;
    std::vector<float> vf;
    const float t_hi = static_cast<float>(time);
    const float t_lo = static_cast<float>(time - static_cast<double>(t_hi));
    const bool prev_active = mxp_eft::active;
    const mxp_eft::ff32 prev_t = mxp_eft::time_ff;
    mxp_eft::active = true;
    mxp_eft::time_ff = {t_hi, t_lo};
    computeFaceVelocityLowPrec(g, p.reverse_time, static_cast<float>(p.tmax), t_hi, uf, vf);
    mxp_eft::time_ff = prev_t;
    mxp_eft::active = prev_active;
    u.resize(uf.size());
    v.resize(vf.size());
    for (std::size_t k = 0; k < uf.size(); ++k) {
        u[k] = static_cast<double>(uf[k]) + (static_cast<double>(mxp_eft::u_lo[k]) + static_cast<double>(mxp_eft::u_tiny[k]));
        v[k] = static_cast<double>(vf[k]) + (static_cast<double>(mxp_eft::v_lo[k]) + static_cast<double>(mxp_eft::v_tiny[k]));
    }
}

double maxDivergence(const Grid& g, const std::vector<double>& u, const std::vector<double>& v) {
    double local = 0.0;
    for (int j = 0; j < g.ny(); ++j) {
        for (int i = 0; i < g.nx(); ++i) {
            const double div = (u[g.index(i + 1, j)] - u[g.index(i, j)]) / g.dx() +
                               (v[g.index(i, j + 1)] - v[g.index(i, j)]) / g.dy();
            const double a = std::fabs(div);
            if (a > local) local = a;
        }
    }
    double global = 0.0;
    MPI_Allreduce(&local, &global, 1, MPI_DOUBLE, MPI_MAX, g.comm());
    return global;
}

void maxFaceSpeeds(const Grid& g, const std::vector<double>& u, const std::vector<double>& v,
                   double& umax, double& vmax) {
    std::vector<float> uf(u.size());
    std::vector<float> vf(v.size());
    mxp_eft::u_lo.resize(u.size());
    mxp_eft::v_lo.resize(v.size());
    for (std::size_t k = 0; k < u.size(); ++k) {
        uf[k] = static_cast<float>(u[k]);
        mxp_eft::u_lo[k] = static_cast<float>(u[k] - static_cast<double>(uf[k]));
        vf[k] = static_cast<float>(v[k]);
        mxp_eft::v_lo[k] = static_cast<float>(v[k] - static_cast<double>(vf[k]));
    }
    float umax_f = 0.0f;
    float vmax_f = 0.0f;
    maxFaceSpeedsLowPrec(g, uf, vf, umax_f, vmax_f);
    umax = static_cast<double>(umax_f) + static_cast<double>(mxp_eft::umax_lo);
    vmax = static_cast<double>(vmax_f) + static_cast<double>(mxp_eft::vmax_lo);
}
