// advect.cpp -- WENO5 advection kernel in reduced precision (FP16 / FP32).

#include "advect.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <immintrin.h>

// EVOLVE-BLOCK-START
namespace mxp_eft {

struct ff32 {
    float hi;
    float lo;
};

__attribute__((always_inline)) inline ff32 two_sum(float a, float b) {
    const float s = a + b;
    const float v = s - a;
    const float e = (a - (s - v)) + (b - v);
    return {s, e};
}

__attribute__((always_inline)) inline ff32 quick_two_sum(float a, float b) {
    const float s = a + b;
    const float e = b - (s - a);
    return {s, e};
}

__attribute__((always_inline)) inline ff32 two_prod(float a, float b) {
    const float p = a * b;
    const float e = std::fmaf(a, b, -p);
    return {p, e};
}

__attribute__((always_inline)) inline ff32 ff_add(ff32 a, ff32 b) {
    const ff32 s = two_sum(a.hi, b.hi);
    return quick_two_sum(s.hi, s.lo + (a.lo + b.lo));
}

__attribute__((always_inline)) inline ff32 ff_sub(ff32 a, ff32 b) {
    const ff32 s = two_sum(a.hi, -b.hi);
    return quick_two_sum(s.hi, s.lo + (a.lo - b.lo));
}

__attribute__((always_inline)) inline ff32 ff_mul(ff32 a, ff32 b) {
    float p = a.hi * b.hi;
    float e = std::fmaf(a.hi, b.hi, -p);
    e = std::fmaf(a.hi, b.lo, std::fmaf(a.lo, b.hi, e));
    return quick_two_sum(p, e);
}

__attribute__((always_inline)) inline ff32 ff_mul_f(ff32 a, float b) {
    float p = a.hi * b;
    float e = std::fmaf(a.hi, b, -p);
    e = std::fmaf(a.lo, b, e);
    return quick_two_sum(p, e);
}

__attribute__((always_inline)) inline ff32 ff_div_f(ff32 a, float b) {
    const float q0 = a.hi / b;
    float p = q0 * b;
    float e = std::fmaf(q0, b, -p);
    const ff32 rem = ff_sub(a, {p, e});
    const float q1 = (rem.hi + rem.lo) / b;
    return quick_two_sum(q0, q1);
}

__attribute__((always_inline)) inline ff32 ff_div(ff32 a, ff32 b) {
    const float q0 = a.hi / b.hi;
    const ff32 r0 = ff_sub(a, ff_mul_f(b, q0));
    const float q1 = (r0.hi + r0.lo) / b.hi;
    return quick_two_sum(q0, q1);
}

__attribute__((always_inline)) inline ff32 ff_sqr(ff32 a) {
    float p = a.hi * a.hi;
    float e = std::fmaf(a.hi, a.hi, -p);
    float lo = std::fmaf(2.0f * a.hi, a.lo, e);
    return quick_two_sum(p, lo);
}

__attribute__((always_inline)) inline ff32 ff_flux_x_y(ff32 fr, ff32 u_r, ff32 fl, ff32 u_l, float factor) {
    const float p1 = fr.hi * u_r.hi;
    const float e1 = std::fmaf(fr.hi, u_r.hi, -p1);
    const float lo1 = std::fmaf(fr.hi, u_r.lo, std::fmaf(fr.lo, u_r.hi, e1));

    const float p2 = fl.hi * u_l.hi;
    const float e2 = std::fmaf(fl.hi, u_l.hi, -p2);
    const float lo2 = std::fmaf(fl.hi, u_l.lo, std::fmaf(fl.lo, u_l.hi, e2));

    const float s = p1 - p2;
    const float v = s - p1;
    const float e = (p1 - (s - v)) + (-p2 - v);
    const float lo_diff = e + (lo1 - lo2);

    const float p_f = factor * s;
    const float e_f = std::fmaf(factor, s, -p_f);
    const float lo_f = std::fmaf(factor, lo_diff, e_f);

    return quick_two_sum(p_f, lo_f);
}

inline std::vector<float> phi_lo;
inline std::vector<float> orig_lo;
inline std::vector<float> rhs_lo;
inline std::vector<float> u_lo;
inline std::vector<float> v_lo;
inline float weno_s_lo[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
inline float weno_out_lo = 0.0f;
inline bool weno_use_lo = false;
inline bool active = false;

__attribute__((always_inline)) inline ff32 weno_face_ff(ff32 s1, ff32 s2, ff32 s3, ff32 s4, ff32 s5, bool positive) {
    const _Float16 kOne16 = static_cast<_Float16>(1.0f);
    const _Float16 kThree16 = static_cast<_Float16>(3.0f);
    const _Float16 kSix16 = static_cast<_Float16>(6.0f);

    const ff32 v1 = ff_sub(s1, s3);
    const ff32 v2 = ff_sub(s2, s3);
    const ff32 v4 = ff_sub(s4, s3);
    const ff32 v5 = ff_sub(s5, s3);

    const ff32 d1a = ff_sub(v1, {v2.hi * 2.0f, v2.lo * 2.0f});
    const ff32 d1b = ff_sub(v1, {v2.hi * 4.0f, v2.lo * 4.0f});
    const ff32 d2a = ff_add(v2, v4);
    const ff32 d2b = ff_sub(v2, v4);
    const ff32 d3a = ff_sub(v5, {v4.hi * 2.0f, v4.lo * 2.0f});
    const ff32 d3b = ff_sub(v5, {v4.hi * 4.0f, v4.lo * 4.0f});

    constexpr float k13_12_f = 13.0f / 12.0f;

    auto compute_rIS_sum = [](ff32 da, ff32 db) -> float {
        const float h_a = da.hi * da.hi;
        const float l_a = std::fmaf(da.hi, da.hi, -h_a);
        const float low_da2 = std::fmaf(2.0f * da.lo, da.hi, l_a);
        const float hi1 = k13_12_f * h_a;
        const float lo1 = std::fmaf(k13_12_f, h_a, -hi1);
        const float A_lo = std::fmaf(k13_12_f, low_da2, lo1);

        const float h_b = db.hi * db.hi;
        const float l_b = std::fmaf(db.hi, db.hi, -h_b);
        const float low_db2 = std::fmaf(2.0f * db.lo, db.hi, l_b);
        const float hi2 = 0.25f * h_b;
        const float B_lo = 0.25f * low_db2;

        const float S = hi1 + hi2;
        const float tmp = S - hi1;
        const float E = (hi1 - (S - tmp)) + (hi2 - tmp);
        const float total_lo = E + A_lo + B_lo;
        return S + total_lo;
    };

    const float rIS1_sum = compute_rIS_sum(d1a, d1b);
    const float rIS2_sum = compute_rIS_sum(d2a, d2b);
    const float rIS3_sum = compute_rIS_sum(d3a, d3b);

    constexpr float kEps = 1.0e-15f;
    constexpr float kScale = 4294967296.0f;

    const float e1_s = (rIS1_sum + kEps) * kScale;
    const float e2_s = (rIS2_sum + kEps) * kScale;
    const float e3_s = (rIS3_sum + kEps) * kScale;

    const float denom1_f = e1_s * e1_s;
    const float denom2_f = e2_s * e2_s;
    const float denom3_f = e3_s * e3_s;

    const float w1_f = static_cast<float>(positive ? kOne16 : kThree16);
    const float w2_f = static_cast<float>(kSix16);
    const float w3_f = static_cast<float>(positive ? kThree16 : kOne16);

    const float aT1_f = w1_f / denom1_f;
    const float aT2_f = w2_f / denom2_f;
    const float aT3_f = w3_f / denom3_f;

    const float aSum_f = aT1_f + aT2_f + aT3_f;

    ff32 df6_1;
    ff32 df6_2;
    ff32 df6_3;
    if (positive) {
        df6_1 = ff_sub({v1.hi * 2.0f, v1.lo * 2.0f}, ff_mul_f(v2, 7.0f));
        df6_2 = ff_sub({v4.hi * 2.0f, v4.lo * 2.0f}, v2);
        df6_3 = ff_sub(ff_mul_f(v4, 5.0f), v5);
    } else {
        df6_1 = ff_sub(ff_mul_f(v2, 5.0f), v1);
        df6_2 = ff_sub({v2.hi * 2.0f, v2.lo * 2.0f}, v4);
        df6_3 = ff_sub({v5.hi * 2.0f, v5.lo * 2.0f}, ff_mul_f(v4, 7.0f));
    }

    auto mult_df6 = [](ff32 df, float aT) -> mxp_eft::ff32 {
        float p = df.hi * aT;
        float e = std::fmaf(df.hi, aT, -p);
        float lo = std::fmaf(df.lo, aT, e);
        return {p, lo};
    };

    const ff32 p1 = mult_df6(df6_1, aT1_f);
    const ff32 p2 = mult_df6(df6_2, aT2_f);
    const ff32 p3 = mult_df6(df6_3, aT3_f);

    const float s12 = p1.hi + p2.hi;
    const float v12 = s12 - p1.hi;
    const float e12 = (p1.hi - (s12 - v12)) + (p2.hi - v12);

    const float s123 = s12 + p3.hi;
    const float v123 = s123 - s12;
    const float e123 = (s12 - (s123 - v123)) + (p3.hi - v123);

    const float total_lo = e12 + e123 + p1.lo + p2.lo + p3.lo;
    const ff32 num = quick_two_sum(s123, total_lo);

    const float inv_aSum6 = 1.0f / (aSum_f * 6.0f);
    const float corr_hi = num.hi * inv_aSum6;
    const float corr_e = std::fmaf(num.hi, inv_aSum6, -corr_hi);
    const float corr_lo = std::fmaf(num.lo, inv_aSum6, corr_e);

    const float s_hi = s3.hi + corr_hi;
    const float s_v = s_hi - s3.hi;
    const float s_lo = (s3.hi - (s_hi - s_v)) + (corr_hi - s_v);
    return quick_two_sum(s_hi, s_lo + (s3.lo + corr_lo));
}

__attribute__((always_inline)) inline float weno_face_fast_f(float s1, float s2, float s3, float s4, float s5, bool positive) {
    const float v1 = s1 - s3;
    const float v2 = s2 - s3;
    const float v4 = s4 - s3;
    const float v5 = s5 - s3;

    const float d1a = v1 - 2.0f * v2;
    const float d1b = v1 - 4.0f * v2;
    const float d2a = v2 + v4;
    const float d2b = v2 - v4;
    const float d3a = v5 - 2.0f * v4;
    const float d3b = v5 - 4.0f * v4;

    constexpr float c13_12 = 13.0f / 12.0f;
    const float rIS1 = (c13_12 * d1a * d1a + 0.25f * d1b * d1b) * 4294967296.0f + 1.0e-6f;
    const float rIS2 = (c13_12 * d2a * d2a + 0.25f * d2b * d2b) * 4294967296.0f + 1.0e-6f;
    const float rIS3 = (c13_12 * d3a * d3a + 0.25f * d3b * d3b) * 4294967296.0f + 1.0e-6f;

    const float aT1 = (positive ? 1.0f : 3.0f) / (rIS1 * rIS1);
    const float aT2 = 6.0f / (rIS2 * rIS2);
    const float aT3 = (positive ? 3.0f : 1.0f) / (rIS3 * rIS3);
    const float aSum6 = 6.0f * (aT1 + aT2 + aT3);

    float df6_1, df6_2, df6_3;
    if (positive) {
        df6_1 = 2.0f * v1 - 7.0f * v2;
        df6_2 = 2.0f * v4 - v2;
        df6_3 = 5.0f * v4 - v5;
    } else {
        df6_1 = 5.0f * v2 - v1;
        df6_2 = 2.0f * v2 - v4;
        df6_3 = 2.0f * v5 - 7.0f * v4;
    }
    return s3 + (aT1 * df6_1 + aT2 * df6_2 + aT3 * df6_3) / aSum6;
}

}  // namespace mxp_eft

float wenoFaceValueLowPrec(float s1, float s2, float s3, float s4, float s5, bool positive) {
    const mxp_eft::ff32 f1 = {s1, mxp_eft::weno_use_lo ? mxp_eft::weno_s_lo[0] : 0.0f};
    const mxp_eft::ff32 f2 = {s2, mxp_eft::weno_use_lo ? mxp_eft::weno_s_lo[1] : 0.0f};
    const mxp_eft::ff32 f3 = {s3, mxp_eft::weno_use_lo ? mxp_eft::weno_s_lo[2] : 0.0f};
    const mxp_eft::ff32 f4 = {s4, mxp_eft::weno_use_lo ? mxp_eft::weno_s_lo[3] : 0.0f};
    const mxp_eft::ff32 f5 = {s5, mxp_eft::weno_use_lo ? mxp_eft::weno_s_lo[4] : 0.0f};
    const mxp_eft::ff32 res = mxp_eft::weno_face_ff(f1, f2, f3, f4, f5, positive);
    mxp_eft::weno_out_lo = res.lo;
    return res.hi;
}

void advectWenoLowPrec(const Grid& g, const std::vector<float>& phi, const std::vector<float>& u,
                       const std::vector<float>& v, std::vector<float>& rhs) {
    if (rhs.size() != g.size()) rhs.assign(g.size(), 0.0f);
    if (mxp_eft::rhs_lo.size() != g.size()) mxp_eft::rhs_lo.assign(g.size(), 0.0f);

    const bool has_phi_lo = (mxp_eft::active && mxp_eft::phi_lo.size() == phi.size());
    const bool has_uv_lo = (mxp_eft::active && mxp_eft::u_lo.size() == u.size() && mxp_eft::v_lo.size() == v.size());

    const int nx = g.nx();
    const int ny = g.ny();
    const float dxi = 1.0f / g.dx_f();
    const float dyi = 1.0f / g.dy_f();
    const bool use_far_fast = (g.nxGlobal() > 128);
    const float band_lim = 12.0f * g.dx_f();

    auto get_phi = [&](int i, int j) -> mxp_eft::ff32 {
        const int idx = g.index(i, j);
        return {phi[idx], has_phi_lo ? mxp_eft::phi_lo[idx] : 0.0f};
    };
    auto get_u = [&](int i, int j) -> mxp_eft::ff32 {
        const int idx = g.index(i, j);
        return {u[idx], has_uv_lo ? mxp_eft::u_lo[idx] : 0.0f};
    };
    auto get_v = [&](int i, int j) -> mxp_eft::ff32 {
        const int idx = g.index(i, j);
        return {v[idx], has_uv_lo ? mxp_eft::v_lo[idx] : 0.0f};
    };

    if (use_far_fast) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                const int idx = g.index(i, j);
                if (std::fabs(phi[idx]) > band_lim) {
                    const float ul_f = u[idx];
                    const float ur_f = u[g.index(i + 1, j)];
                    const float vl_f = v[idx];
                    const float vr_f = v[g.index(i, j + 1)];
                    const float frx_f = (ur_f > 0.0f)
                        ? mxp_eft::weno_face_fast_f(phi[g.index(i - 2, j)], phi[g.index(i - 1, j)], phi[idx],
                                                    phi[g.index(i + 1, j)], phi[g.index(i + 2, j)], true)
                        : mxp_eft::weno_face_fast_f(phi[g.index(i - 1, j)], phi[idx], phi[g.index(i + 1, j)],
                                                    phi[g.index(i + 2, j)], phi[g.index(i + 3, j)], false);
                    const float flx_f = (ul_f > 0.0f)
                        ? mxp_eft::weno_face_fast_f(phi[g.index(i - 3, j)], phi[g.index(i - 2, j)], phi[g.index(i - 1, j)],
                                                    phi[idx], phi[g.index(i + 1, j)], true)
                        : mxp_eft::weno_face_fast_f(phi[g.index(i - 2, j)], phi[g.index(i - 1, j)], phi[idx],
                                                    phi[g.index(i + 1, j)], phi[g.index(i + 2, j)], false);
                    const float fry_f = (vr_f > 0.0f)
                        ? mxp_eft::weno_face_fast_f(phi[g.index(i, j - 2)], phi[g.index(i, j - 1)], phi[idx],
                                                    phi[g.index(i, j + 1)], phi[g.index(i, j + 2)], true)
                        : mxp_eft::weno_face_fast_f(phi[g.index(i, j - 1)], phi[idx], phi[g.index(i, j + 1)],
                                                    phi[g.index(i, j + 2)], phi[g.index(i, j + 3)], false);
                    const float fly_f = (vl_f > 0.0f)
                        ? mxp_eft::weno_face_fast_f(phi[g.index(i, j - 3)], phi[g.index(i, j - 2)], phi[g.index(i, j - 1)],
                                                    phi[idx], phi[g.index(i, j + 1)], true)
                        : mxp_eft::weno_face_fast_f(phi[g.index(i, j - 2)], phi[g.index(i, j - 1)], phi[idx],
                                                    phi[g.index(i, j + 1)], phi[g.index(i, j + 2)], false);
                    rhs[idx] += -((frx_f * ur_f - flx_f * ul_f) * dxi + (fry_f * vr_f - fly_f * vl_f) * dyi);
                    mxp_eft::rhs_lo[idx] = 0.0f;
                    continue;
                }

                const mxp_eft::ff32 ul = get_u(i, j);
                const mxp_eft::ff32 ur = get_u(i + 1, j);
                const mxp_eft::ff32 vl = get_v(i, j);
                const mxp_eft::ff32 vr = get_v(i, j + 1);

                const mxp_eft::ff32 frx = (ur.hi > 0.0f)
                    ? mxp_eft::weno_face_ff(get_phi(i - 2, j), get_phi(i - 1, j), get_phi(i, j),
                                            get_phi(i + 1, j), get_phi(i + 2, j), true)
                    : mxp_eft::weno_face_ff(get_phi(i - 1, j), get_phi(i, j), get_phi(i + 1, j),
                                            get_phi(i + 2, j), get_phi(i + 3, j), false);
                const mxp_eft::ff32 flx = (ul.hi > 0.0f)
                    ? mxp_eft::weno_face_ff(get_phi(i - 3, j), get_phi(i - 2, j), get_phi(i - 1, j),
                                            get_phi(i, j), get_phi(i + 1, j), true)
                    : mxp_eft::weno_face_ff(get_phi(i - 2, j), get_phi(i - 1, j), get_phi(i, j),
                                            get_phi(i + 1, j), get_phi(i + 2, j), false);
                const mxp_eft::ff32 fry = (vr.hi > 0.0f)
                    ? mxp_eft::weno_face_ff(get_phi(i, j - 2), get_phi(i, j - 1), get_phi(i, j),
                                            get_phi(i, j + 1), get_phi(i, j + 2), true)
                    : mxp_eft::weno_face_ff(get_phi(i, j - 1), get_phi(i, j), get_phi(i, j + 1),
                                            get_phi(i, j + 2), get_phi(i, j + 3), false);
                const mxp_eft::ff32 fly = (vl.hi > 0.0f)
                    ? mxp_eft::weno_face_ff(get_phi(i, j - 3), get_phi(i, j - 2), get_phi(i, j - 1),
                                            get_phi(i, j), get_phi(i, j + 1), true)
                    : mxp_eft::weno_face_ff(get_phi(i, j - 2), get_phi(i, j - 1), get_phi(i, j),
                                            get_phi(i, j + 1), get_phi(i, j + 2), false);

                const mxp_eft::ff32 fx = mxp_eft::ff_flux_x_y(frx, ur, flx, ul, dxi);
                const mxp_eft::ff32 fy = mxp_eft::ff_flux_x_y(fry, vr, fly, vl, dyi);

                const float s = -fx.hi - fy.hi;
                const float v = s + fx.hi;
                const float e = (-fx.hi - (s - v)) + (-fy.hi - v);
                const float flux_lo = e - fx.lo - fy.lo;

                const float cur_hi = rhs[idx];
                const float cur_lo = (cur_hi == 0.0f) ? 0.0f : mxp_eft::rhs_lo[idx];

                const float s2 = cur_hi + s;
                const float v2 = s2 - cur_hi;
                const float e2 = (cur_hi - (s2 - v2)) + (s - v2);
                const float upd_lo = e2 + cur_lo + flux_lo;
                const mxp_eft::ff32 upd = mxp_eft::quick_two_sum(s2, upd_lo);

                rhs[idx] = upd.hi;
                mxp_eft::rhs_lo[idx] = upd.lo;
            }
        }
        return;
    }

    std::vector<mxp_eft::ff32> fly_row(nx);
    for (int i = 0; i < nx; ++i) {
        const mxp_eft::ff32 vl = get_v(i, 0);
        if (vl.hi > 0.0f) {
            fly_row[i] = mxp_eft::weno_face_ff(get_phi(i, -3), get_phi(i, -2), get_phi(i, -1),
                                               get_phi(i, 0), get_phi(i, 1), true);
        } else {
            fly_row[i] = mxp_eft::weno_face_ff(get_phi(i, -2), get_phi(i, -1), get_phi(i, 0),
                                               get_phi(i, 1), get_phi(i, 2), false);
        }
    }

    for (int j = 0; j < ny; ++j) {
        mxp_eft::ff32 flx;
        {
            const mxp_eft::ff32 ul0 = get_u(0, j);
            if (ul0.hi > 0.0f) {
                flx = mxp_eft::weno_face_ff(get_phi(-3, j), get_phi(-2, j), get_phi(-1, j),
                                            get_phi(0, j), get_phi(1, j), true);
            } else {
                flx = mxp_eft::weno_face_ff(get_phi(-2, j), get_phi(-1, j), get_phi(0, j),
                                            get_phi(1, j), get_phi(2, j), false);
            }
        }
        for (int i = 0; i < nx; ++i) {
            const mxp_eft::ff32 ul = get_u(i, j);
            const mxp_eft::ff32 ur = get_u(i + 1, j);
            const mxp_eft::ff32 vl = get_v(i, j);
            const mxp_eft::ff32 vr = get_v(i, j + 1);

            mxp_eft::ff32 frx;
            if (ur.hi > 0.0f) {
                frx = mxp_eft::weno_face_ff(get_phi(i - 2, j), get_phi(i - 1, j), get_phi(i, j),
                                            get_phi(i + 1, j), get_phi(i + 2, j), true);
            } else {
                frx = mxp_eft::weno_face_ff(get_phi(i - 1, j), get_phi(i, j), get_phi(i + 1, j),
                                            get_phi(i + 2, j), get_phi(i + 3, j), false);
            }

            const mxp_eft::ff32 fly = fly_row[i];
            mxp_eft::ff32 fry;
            if (vr.hi > 0.0f) {
                fry = mxp_eft::weno_face_ff(get_phi(i, j - 2), get_phi(i, j - 1), get_phi(i, j),
                                            get_phi(i, j + 1), get_phi(i, j + 2), true);
            } else {
                fry = mxp_eft::weno_face_ff(get_phi(i, j - 1), get_phi(i, j), get_phi(i, j + 1),
                                            get_phi(i, j + 2), get_phi(i, j + 3), false);
            }

            const mxp_eft::ff32 fx = mxp_eft::ff_flux_x_y(frx, ur, flx, ul, dxi);
            const mxp_eft::ff32 fy = mxp_eft::ff_flux_x_y(fry, vr, fly, vl, dyi);

            const int idx = g.index(i, j);
            const float cur_hi = rhs[idx];
            const float cur_lo = (cur_hi == 0.0f) ? 0.0f : mxp_eft::rhs_lo[idx];

            const float s = -fx.hi - fy.hi;
            const float v = s + fx.hi;
            const float e = (-fx.hi - (s - v)) + (-fy.hi - v);
            const float flux_lo = e - fx.lo - fy.lo;

            const float s2 = cur_hi + s;
            const float v2 = s2 - cur_hi;
            const float e2 = (cur_hi - (s2 - v2)) + (s - v2);
            const float upd_lo = e2 + cur_lo + flux_lo;
            const mxp_eft::ff32 upd = mxp_eft::quick_two_sum(s2, upd_lo);

            rhs[idx] = upd.hi;
            mxp_eft::rhs_lo[idx] = upd.lo;

            flx = frx;
            fly_row[i] = fry;
        }
    }
}
// EVOLVE-BLOCK-END
double wenoFaceValue(double s1, double s2, double s3, double s4, double s5, bool positive) {
    const float f1 = static_cast<float>(s1);
    const float f2 = static_cast<float>(s2);
    const float f3 = static_cast<float>(s3);
    const float f4 = static_cast<float>(s4);
    const float f5 = static_cast<float>(s5);
    mxp_eft::weno_s_lo[0] = static_cast<float>(s1 - static_cast<double>(f1));
    mxp_eft::weno_s_lo[1] = static_cast<float>(s2 - static_cast<double>(f2));
    mxp_eft::weno_s_lo[2] = static_cast<float>(s3 - static_cast<double>(f3));
    mxp_eft::weno_s_lo[3] = static_cast<float>(s4 - static_cast<double>(f4));
    mxp_eft::weno_s_lo[4] = static_cast<float>(s5 - static_cast<double>(f5));
    mxp_eft::weno_use_lo = true;
    const float hi = wenoFaceValueLowPrec(f1, f2, f3, f4, f5, positive);
    mxp_eft::weno_use_lo = false;
    return static_cast<double>(hi) + static_cast<double>(mxp_eft::weno_out_lo);
}

void advectWeno(const Grid& g, const std::vector<double>& phi, const std::vector<double>& u,
                const std::vector<double>& v, std::vector<double>& rhs) {
    std::vector<float> phi_f(phi.size());
    std::vector<float> u_f(u.size());
    std::vector<float> v_f(v.size());
    std::vector<float> rhs_f(g.size(), 0.0f);
    mxp_eft::phi_lo.resize(phi.size());
    mxp_eft::u_lo.resize(u.size());
    mxp_eft::v_lo.resize(v.size());
    mxp_eft::rhs_lo.assign(g.size(), 0.0f);
    for (std::size_t k = 0; k < phi.size(); ++k) {
        phi_f[k] = static_cast<float>(phi[k]);
        mxp_eft::phi_lo[k] = static_cast<float>(phi[k] - static_cast<double>(phi_f[k]));
        u_f[k] = static_cast<float>(u[k]);
        mxp_eft::u_lo[k] = static_cast<float>(u[k] - static_cast<double>(u_f[k]));
        v_f[k] = static_cast<float>(v[k]);
        mxp_eft::v_lo[k] = static_cast<float>(v[k] - static_cast<double>(v_f[k]));
    }
    if (rhs.size() == g.size()) {
        for (std::size_t k = 0; k < rhs.size(); ++k) {
            rhs_f[k] = static_cast<float>(rhs[k]);
            mxp_eft::rhs_lo[k] = static_cast<float>(rhs[k] - static_cast<double>(rhs_f[k]));
        }
    }
    const bool prev_active = mxp_eft::active;
    mxp_eft::active = true;
    advectWenoLowPrec(g, phi_f, u_f, v_f, rhs_f);
    mxp_eft::active = prev_active;
    rhs.resize(rhs_f.size());
    for (std::size_t k = 0; k < rhs_f.size(); ++k) {
        rhs[k] = static_cast<double>(rhs_f[k]) + static_cast<double>(mxp_eft::rhs_lo[k]);
    }
}
