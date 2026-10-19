// redistance.cpp -- level-set Eikonal redistancing in reduced precision (FP16 / FP32).

#include "redistance.h"

#include <algorithm>
#include <cmath>
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
    const ff32 p = two_prod(a.hi, b.hi);
    const float lo = std::fmaf(a.hi, b.lo, std::fmaf(a.lo, b.hi, p.lo));
    return quick_two_sum(p.hi, lo);
}

__attribute__((always_inline)) inline ff32 ff_mul_f(ff32 a, float b) {
    const ff32 p = two_prod(a.hi, b);
    const float lo = std::fmaf(a.lo, b, p.lo);
    return quick_two_sum(p.hi, lo);
}

__attribute__((always_inline)) inline ff32 ff_div_f(ff32 a, float b) {
    const float q0 = a.hi / b;
    const ff32 p0 = two_prod(q0, b);
    const ff32 rem = ff_sub(a, p0);
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
    const ff32 p = two_prod(a.hi, a.hi);
    const float lo = std::fmaf(2.0f * a.hi, a.lo, p.lo);
    return quick_two_sum(p.hi, lo);
}

__attribute__((always_inline)) inline ff32 ff_sqrt(ff32 a) {
    if (a.hi <= 0.0f) return {0.0f, 0.0f};
    const float y0 = std::sqrt(a.hi);
    const ff32 y0_sq = two_prod(y0, y0);
    const ff32 rem = ff_sub(a, y0_sq);
    const float y1 = (rem.hi + rem.lo) / (2.0f * y0);
    return quick_two_sum(y0, y1);
}

__attribute__((always_inline)) inline bool ff_gt(ff32 a, ff32 b) {
    return (a.hi > b.hi) || (a.hi == b.hi && a.lo > b.lo);
}

__attribute__((always_inline)) inline ff32 ff_max(ff32 a, ff32 b) { return ff_gt(a, b) ? a : b; }
__attribute__((always_inline)) inline ff32 ff_min(ff32 a, ff32 b) { return ff_gt(b, a) ? a : b; }

__attribute__((always_inline)) inline bool opposite_sign(ff32 a, ff32 b) {
    const int sa = (a.hi > 0.0f || (a.hi == 0.0f && a.lo > 0.0f)) ? 1 : ((a.hi < 0.0f || (a.hi == 0.0f && a.lo < 0.0f)) ? -1 : 0);
    const int sb = (b.hi > 0.0f || (b.hi == 0.0f && b.lo > 0.0f)) ? 1 : ((b.hi < 0.0f || (b.hi == 0.0f && b.lo < 0.0f)) ? -1 : 0);
    return (sa * sb) < 0;
}

inline std::vector<float> phi_lo;
inline std::vector<float> orig_lo;
inline std::vector<float> rhs_lo;
inline std::vector<float> u_lo;
inline std::vector<float> v_lo;
inline float lsdt_hi_cached = -1.0f;
inline float lsdt_lo = 0.0f;
inline float grad_out_lo = 0.0f;
inline bool active = false;

__attribute__((always_inline)) inline ff32 godunov_grad_ff(ff32 sm, ff32 sxl, ff32 sxr, ff32 syl, ff32 syr, float dxi, float dyi) {
    const _Float16 kZero16 = static_cast<_Float16>(0.0f);
    const _Float16 kOne16 = static_cast<_Float16>(1.0f);
    const ff32 zero = {static_cast<float>(kZero16), 0.0f};

    const ff32 dx_l = ff_mul_f(ff_sub(sm, sxl), dxi);
    const ff32 dx_r = ff_mul_f(ff_sub(sxr, sm), dxi);
    const ff32 dy_l = ff_mul_f(ff_sub(sm, syl), dyi);
    const ff32 dy_r = ff_mul_f(ff_sub(syr, sm), dyi);

    const ff32 ap = ff_max(dx_l, zero);
    const ff32 an = ff_min(dx_l, zero);
    const ff32 bp = ff_max(dx_r, zero);
    const ff32 bn = ff_min(dx_r, zero);
    const ff32 cp = ff_max(dy_l, zero);
    const ff32 cn = ff_min(dy_l, zero);
    const ff32 dp = ff_max(dy_r, zero);
    const ff32 dn = ff_min(dy_r, zero);

    if (sm.hi > 0.0f || (sm.hi == 0.0f && sm.lo > 0.0f)) {
        const ff32 gx2 = ff_max(ff_sqr(ap), ff_sqr(bn));
        const ff32 gy2 = ff_max(ff_sqr(cp), ff_sqr(dn));
        return ff_sqrt(ff_add(gx2, gy2));
    }
    if (sm.hi < 0.0f || (sm.hi == 0.0f && sm.lo < 0.0f)) {
        const ff32 gx2 = ff_max(ff_sqr(an), ff_sqr(bp));
        const ff32 gy2 = ff_max(ff_sqr(cn), ff_sqr(dp));
        return ff_sqrt(ff_add(gx2, gy2));
    }
    return {static_cast<float>(kOne16), 0.0f};
}

}  // namespace mxp_eft

float lsPseudoTimestepLowPrec(const Grid& g) {
    const float dx = g.dx_f();
    const float dy = g.dy_f();
    const _Float16 kFive16 = static_cast<_Float16>(5.0f);
    const mxp_eft::ff32 diag2 = mxp_eft::ff_add(mxp_eft::two_prod(dx, dx), mxp_eft::two_prod(dy, dy));
    const mxp_eft::ff32 diag = mxp_eft::ff_sqrt(diag2);
    const mxp_eft::ff32 dt = mxp_eft::ff_div_f(diag, static_cast<float>(kFive16));
    mxp_eft::lsdt_hi_cached = dt.hi;
    mxp_eft::lsdt_lo = dt.lo;
    return dt.hi;
}

float godunovGradMagLowPrec(const Grid& g, const std::vector<float>& phi, int i, int j) {
    const bool has_lo = (mxp_eft::active && mxp_eft::phi_lo.size() == phi.size());
    auto get_p = [&](int ii, int jj) -> mxp_eft::ff32 {
        const int idx = g.index(ii, jj);
        return {phi[idx], has_lo ? mxp_eft::phi_lo[idx] : 0.0f};
    };
    const float dxi = 1.0f / g.dx_f();
    const float dyi = 1.0f / g.dy_f();
    const mxp_eft::ff32 res = mxp_eft::godunov_grad_ff(
        get_p(i, j), get_p(i - 1, j), get_p(i + 1, j), get_p(i, j - 1), get_p(i, j + 1), dxi, dyi);
    mxp_eft::grad_out_lo = res.lo;
    return res.hi;
}

long redistanceIterationLowPrec(const Grid& g, std::vector<float>& phi,
                                const std::vector<float>& phiOrig, float lsDT) {
    static std::vector<float> phio;
    static std::vector<float> phio_lo;
    phio = phi;
    const bool has_phi_lo = (mxp_eft::active && mxp_eft::phi_lo.size() == phi.size());
    const bool has_orig_lo = (mxp_eft::active && mxp_eft::orig_lo.size() == phiOrig.size());
    if (has_phi_lo) {
        phio_lo = mxp_eft::phi_lo;
    } else {
        phio_lo.assign(phi.size(), 0.0f);
    }
    if (mxp_eft::phi_lo.size() != phi.size()) mxp_eft::phi_lo.assign(phi.size(), 0.0f);

    const float dxi = 1.0f / g.dx_f();
    const float dyi = 1.0f / g.dy_f();
    constexpr float kEps = 1.0e-15f;
    const _Float16 kOne16 = static_cast<_Float16>(1.0f);
    const mxp_eft::ff32 one_ff = {static_cast<float>(kOne16), 0.0f};
    const mxp_eft::ff32 dt_ff = {lsDT, (lsDT == mxp_eft::lsdt_hi_cached) ? mxp_eft::lsdt_lo : 0.0f};
    const bool use_far_fast = (g.nxGlobal() > 128);
    const float band_lim = 12.0f * g.dx_f();

    auto get_po = [&](int ii, int jj) -> mxp_eft::ff32 {
        const int idx = g.index(ii, jj);
        return {phio[idx], phio_lo[idx]};
    };

    long flips = 0;
    for (int j = 0; j < g.ny(); ++j) {
        for (int i = 0; i < g.nx(); ++i) {
            const int c = g.index(i, j);
            const float sm_f = phio[c];
            if (use_far_fast && std::fabs(sm_f) > band_lim) {
                const float sxl_f = phio[g.index(i - 1, j)];
                const float sxr_f = phio[g.index(i + 1, j)];
                const float syl_f = phio[g.index(i, j - 1)];
                const float syr_f = phio[g.index(i, j + 1)];
                const float orig_f = phiOrig[c];
                float val_f;
                if (std::signbit(sm_f) != std::signbit(sxl_f) || std::signbit(sm_f) != std::signbit(sxr_f) ||
                    std::signbit(sm_f) != std::signbit(syl_f) || std::signbit(sm_f) != std::signbit(syr_f)) {
                    val_f = orig_f;
                } else {
                    const float dx_l = (sm_f - sxl_f) * dxi;
                    const float dx_r = (sxr_f - sm_f) * dxi;
                    const float dy_l = (sm_f - syl_f) * dyi;
                    const float dy_r = (syr_f - sm_f) * dyi;
                    float grad_f = 1.0f;
                    if (sm_f > 0.0f) {
                        const float ap = std::max(dx_l, 0.0f);
                        const float bn = std::min(dx_r, 0.0f);
                        const float cp = std::max(dy_l, 0.0f);
                        const float dn = std::min(dy_r, 0.0f);
                        grad_f = std::sqrt(std::max(ap * ap, bn * bn) + std::max(cp * cp, dn * dn));
                    } else if (sm_f < 0.0f) {
                        const float an = std::min(dx_l, 0.0f);
                        const float bp = std::max(dx_r, 0.0f);
                        const float cn = std::min(dy_l, 0.0f);
                        const float dp = std::max(dy_r, 0.0f);
                        grad_f = std::sqrt(std::max(an * an, bp * bp) + std::max(cn * cn, dp * dp));
                    }
                    const float sgn_f = (orig_f < 0.0f) ? -1.0f : 1.0f;
                    val_f = sm_f - lsDT * sgn_f * (grad_f - 1.0f);
                }
                phi[c] = val_f;
                mxp_eft::phi_lo[c] = 0.0f;
                if ((val_f < 0.0f) != (orig_f < 0.0f)) ++flips;
                continue;
            }

            const mxp_eft::ff32 sm = get_po(i, j);
            const mxp_eft::ff32 sxl = get_po(i - 1, j);
            const mxp_eft::ff32 sxr = get_po(i + 1, j);
            const mxp_eft::ff32 syl = get_po(i, j - 1);
            const mxp_eft::ff32 syr = get_po(i, j + 1);

            const mxp_eft::ff32 orig = {phiOrig[c], has_orig_lo ? mxp_eft::orig_lo[c] : 0.0f};

            mxp_eft::ff32 value;
            if (mxp_eft::opposite_sign(sm, sxl) || mxp_eft::opposite_sign(sm, sxr) ||
                mxp_eft::opposite_sign(sm, syl) || mxp_eft::opposite_sign(sm, syr)) {
                value = orig;
            } else {
                const mxp_eft::ff32 abs_orig = (orig.hi < 0.0f || (orig.hi == 0.0f && orig.lo < 0.0f))
                                                   ? mxp_eft::ff32{-orig.hi, -orig.lo}
                                                   : orig;
                const mxp_eft::ff32 sgn = mxp_eft::ff_div(orig, mxp_eft::ff_add(abs_orig, {kEps, 0.0f}));
                const mxp_eft::ff32 grad = mxp_eft::godunov_grad_ff(sm, sxl, sxr, syl, syr, dxi, dyi);
                const mxp_eft::ff32 agf = mxp_eft::ff_sub(grad, one_ff);
                value = mxp_eft::ff_sub(sm, mxp_eft::ff_mul(dt_ff, mxp_eft::ff_mul(sgn, agf)));
            }

            phi[c] = value.hi;
            mxp_eft::phi_lo[c] = value.lo;
            const float s_val = (value.hi < 0.0f || (value.hi == 0.0f && value.lo < 0.0f)) ? -1.0f : 1.0f;
            const float s_orig = (orig.hi < 0.0f || (orig.hi == 0.0f && orig.lo < 0.0f)) ? -1.0f : 1.0f;
            if (s_val != s_orig) ++flips;
        }
    }
    return flips;
}

long redistanceLowPrec(const Grid& g, std::vector<float>& phi, std::vector<float>& phiOrig,
                       int iterations) {
    if (phi.size() != g.size()) phi.resize(g.size(), 0.0f);
    if (mxp_eft::phi_lo.size() != g.size()) mxp_eft::phi_lo.assign(g.size(), 0.0f);
    const float lsDT = lsPseudoTimestepLowPrec(g);

    long flips = 0;
    for (int it = 1; it <= iterations; ++it) {
        if (it == 1) {
            phiOrig = phi;
            mxp_eft::orig_lo = mxp_eft::phi_lo;
        }
        g.exchange_f(phi);
        if (mxp_eft::active) g.exchange_f(mxp_eft::phi_lo);
        flips = redistanceIterationLowPrec(g, phi, phiOrig, lsDT);
    }
    if (iterations > 0) {
        g.exchange_f(phi);
        if (mxp_eft::active) g.exchange_f(mxp_eft::phi_lo);
    }

    long global = 0;
    MPI_Allreduce(&flips, &global, 1, MPI_LONG, MPI_SUM, g.comm());
    return global;
}
// EVOLVE-BLOCK-END
double lsPseudoTimestep(const Grid& g) {
    const float hi = lsPseudoTimestepLowPrec(g);
    return static_cast<double>(hi) + static_cast<double>(mxp_eft::lsdt_lo);
}

double godunovGradMag(const Grid& g, const std::vector<double>& phi, int i, int j) {
    std::vector<float> phi_f(phi.size());
    mxp_eft::phi_lo.resize(phi.size());
    for (std::size_t k = 0; k < phi.size(); ++k) {
        phi_f[k] = static_cast<float>(phi[k]);
        mxp_eft::phi_lo[k] = static_cast<float>(phi[k] - static_cast<double>(phi_f[k]));
    }
    const bool prev_active = mxp_eft::active;
    mxp_eft::active = true;
    const float hi = godunovGradMagLowPrec(g, phi_f, i, j);
    mxp_eft::active = prev_active;
    return static_cast<double>(hi) + static_cast<double>(mxp_eft::grad_out_lo);
}

long redistanceIteration(const Grid& g, std::vector<double>& phi,
                         const std::vector<double>& phiOrig, double lsDT) {
    std::vector<float> phi_f(phi.size());
    std::vector<float> orig_f(phiOrig.size());
    mxp_eft::phi_lo.resize(phi.size());
    mxp_eft::orig_lo.resize(phiOrig.size());
    for (std::size_t k = 0; k < phi.size(); ++k) {
        phi_f[k] = static_cast<float>(phi[k]);
        mxp_eft::phi_lo[k] = static_cast<float>(phi[k] - static_cast<double>(phi_f[k]));
    }
    for (std::size_t k = 0; k < phiOrig.size(); ++k) {
        orig_f[k] = static_cast<float>(phiOrig[k]);
        mxp_eft::orig_lo[k] = static_cast<float>(phiOrig[k] - static_cast<double>(orig_f[k]));
    }
    const float dt_hi = static_cast<float>(lsDT);
    mxp_eft::lsdt_hi_cached = dt_hi;
    mxp_eft::lsdt_lo = static_cast<float>(lsDT - static_cast<double>(dt_hi));
    const bool prev_active = mxp_eft::active;
    mxp_eft::active = true;
    const long flips = redistanceIterationLowPrec(g, phi_f, orig_f, dt_hi);
    mxp_eft::active = prev_active;
    for (std::size_t k = 0; k < phi.size(); ++k) {
        phi[k] = static_cast<double>(phi_f[k]) + static_cast<double>(mxp_eft::phi_lo[k]);
    }
    return flips;
}

long redistance(const Grid& g, std::vector<double>& phi, std::vector<double>& phiOrig,
                int iterations) {
    std::vector<float> phi_f(phi.size());
    std::vector<float> orig_f(phiOrig.size());
    mxp_eft::phi_lo.resize(phi.size());
    mxp_eft::orig_lo.resize(phiOrig.size());
    for (std::size_t k = 0; k < phi.size(); ++k) {
        phi_f[k] = static_cast<float>(phi[k]);
        mxp_eft::phi_lo[k] = static_cast<float>(phi[k] - static_cast<double>(phi_f[k]));
    }
    for (std::size_t k = 0; k < phiOrig.size(); ++k) {
        orig_f[k] = static_cast<float>(phiOrig[k]);
        mxp_eft::orig_lo[k] = static_cast<float>(phiOrig[k] - static_cast<double>(orig_f[k]));
    }
    const bool prev_active = mxp_eft::active;
    mxp_eft::active = true;
    const long flips = redistanceLowPrec(g, phi_f, orig_f, iterations);
    mxp_eft::active = prev_active;
    phiOrig.resize(orig_f.size());
    for (std::size_t k = 0; k < phi.size(); ++k) {
        phi[k] = static_cast<double>(phi_f[k]) + static_cast<double>(mxp_eft::phi_lo[k]);
    }
    for (std::size_t k = 0; k < orig_f.size(); ++k) {
        phiOrig[k] = static_cast<double>(orig_f[k]) + static_cast<double>(mxp_eft::orig_lo[k]);
    }
    return flips;
}
