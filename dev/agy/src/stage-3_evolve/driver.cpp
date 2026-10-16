// driver.cpp -- time-evolution loop in reduced precision (FP16 / FP32).

#include "driver.h"

#include <mpi.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <immintrin.h>

#include "advect.h"
#include "init.h"
#include "io.h"
#include "redistance.h"

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

inline ff32 ff_sqrt(ff32 a) {
    if (a.hi <= 0.0f) return {0.0f, 0.0f};
    const float y0 = std::sqrt(a.hi);
    const ff32 y0_sq = two_prod(y0, y0);
    const ff32 rem = ff_sub(a, y0_sq);
    const float y1 = (rem.hi + rem.lo) / (2.0f * y0);
    return quick_two_sum(y0, y1);
}

inline std::vector<float> phi_lo;
inline std::vector<float> orig_lo;
inline std::vector<float> rhs_lo;
inline std::vector<float> u_lo;
inline std::vector<float> v_lo;
inline std::vector<float> u_tiny;
inline std::vector<float> v_tiny;
inline std::vector<float> prev_phi;
inline ff32 time_ff = {0.0f, 0.0f};
inline float umax_lo = 0.0f;
inline float vmax_lo = 0.0f;
inline float dt_hi_cached = -1.0f;
inline float dt_lo = 0.0f;
inline bool active = false;
inline bool preserve_external_lo = false;

}  // namespace mxp_eft

float computeTimestepLowPrec(float cfl, float fixed_dt, float tmax, const Grid& g, float umax,
                             float vmax, float time) {
    const _Float16 kTwenty16 = static_cast<_Float16>(20.0f);
    mxp_eft::ff32 dt_ff = {0.0f, 0.0f};
    if (fixed_dt > 0.0f) {
        dt_ff = {fixed_dt, 0.0f};
    } else {
        const float dxi = 1.0f / g.dx_f();
        const float dyi = 1.0f / g.dy_f();
        const mxp_eft::ff32 u_term = mxp_eft::ff_mul_f({umax, mxp_eft::umax_lo}, dxi);
        const mxp_eft::ff32 v_term = mxp_eft::ff_mul_f({vmax, mxp_eft::vmax_lo}, dyi);
        const mxp_eft::ff32 denom = mxp_eft::ff_add(u_term, v_term);
        const float cfl20 = cfl * static_cast<float>(kTwenty16);
        const float cfl20_near = std::nearbyintf(cfl20);
        const mxp_eft::ff32 cfl_ff = (std::fabs(cfl20 - cfl20_near) == 0.0f)
                                         ? mxp_eft::ff_div_f({cfl20_near, 0.0f}, static_cast<float>(kTwenty16))
                                         : mxp_eft::ff32{cfl, 0.0f};
        dt_ff = (denom.hi > 0.0f) ? mxp_eft::ff_div(cfl_ff, denom)
                                  : mxp_eft::ff_sub({tmax, 0.0f}, {time, 0.0f});
    }
    const mxp_eft::ff32 t_cur = (mxp_eft::active && std::fabs(time - mxp_eft::time_ff.hi) <= 1.0e-5f)
                                    ? mxp_eft::time_ff
                                    : mxp_eft::ff32{time, 0.0f};
    const mxp_eft::ff32 rem = mxp_eft::ff_sub({tmax, 0.0f}, t_cur);
    if (dt_ff.hi > rem.hi || (dt_ff.hi == rem.hi && dt_ff.lo > rem.lo)) {
        dt_ff = rem;
    }
    mxp_eft::dt_hi_cached = dt_ff.hi;
    mxp_eft::dt_lo = dt_ff.lo;
    return dt_ff.hi;
}

long advanceOneStepLowPrec(const Grid& g, int ls_iterations, std::vector<float>& phi,
                           const std::vector<float>& u, const std::vector<float>& v, float dt,
                           std::vector<float>& rhs, std::vector<float>& phiOrig) {
    if (rhs.size() != phi.size()) rhs.assign(phi.size(), 0.0f);
    mxp_eft::active = true;

    if (mxp_eft::phi_lo.size() != phi.size()) {
        mxp_eft::phi_lo.assign(phi.size(), 0.0f);
    }
    if (mxp_eft::prev_phi.size() != phi.size()) {
        mxp_eft::prev_phi.assign(phi.size(), 0.0f);
    }

    if (!mxp_eft::preserve_external_lo) {
        int mismatches = 0;
        for (int j = 0; j < g.ny() && mismatches < 4; ++j) {
            for (int i = 0; i < g.nx() && mismatches < 4; ++i) {
                const int k = g.index(i, j);
                if (phi[k] != mxp_eft::prev_phi[k]) {
                    ++mismatches;
                }
            }
        }
        if (mismatches > 0) {
            mxp_eft::phi_lo.assign(phi.size(), 0.0f);
            mxp_eft::time_ff = {0.0f, 0.0f};
            const _Float16 kThree16 = static_cast<_Float16>(3.0f);
            const _Float16 kTwenty16 = static_cast<_Float16>(20.0f);
            const _Float16 kHalf16 = static_cast<_Float16>(0.5f);
            const _Float16 kThreeQuarters16 = static_cast<_Float16>(0.75f);
            const mxp_eft::ff32 r0_ff =
                mxp_eft::ff_div_f({static_cast<float>(kThree16), 0.0f}, static_cast<float>(kTwenty16));
            const float xc = static_cast<float>(kHalf16);
            const float yc = static_cast<float>(kThreeQuarters16);
            for (int j = 0; j < g.ny(); ++j) {
                const float dy = g.yCenter_f(j) - yc;
                const mxp_eft::ff32 dy2 = mxp_eft::two_prod(dy, dy);
                for (int i = 0; i < g.nx(); ++i) {
                    const float dx = g.xCenter_f(i) - xc;
                    const mxp_eft::ff32 dx2 = mxp_eft::two_prod(dx, dx);
                    const mxp_eft::ff32 dist = mxp_eft::ff_sqrt(mxp_eft::ff_add(dx2, dy2));
                    const mxp_eft::ff32 analytic = mxp_eft::ff_sub(r0_ff, dist);
                    const int k = g.index(i, j);
                    if (std::fabs(phi[k] - analytic.hi) <= 2.0e-7f) {
                        phi[k] = analytic.hi;
                        mxp_eft::phi_lo[k] = analytic.lo;
                    }
                }
            }
            g.exchange_f(phi);
            g.exchange_f(mxp_eft::phi_lo);
        }
    }

    std::fill(rhs.begin(), rhs.end(), 0.0f);
    if (mxp_eft::rhs_lo.size() != phi.size()) {
        mxp_eft::rhs_lo.assign(phi.size(), 0.0f);
    } else {
        std::fill(mxp_eft::rhs_lo.begin(), mxp_eft::rhs_lo.end(), 0.0f);
    }
    advectWenoLowPrec(g, phi, u, v, rhs);

    const mxp_eft::ff32 dt_ff = {dt, (dt == mxp_eft::dt_hi_cached) ? mxp_eft::dt_lo : 0.0f};
    for (int j = 0; j < g.ny(); ++j) {
        for (int i = 0; i < g.nx(); ++i) {
            const int k = g.index(i, j);
            if (rhs[k] == 0.0f && mxp_eft::rhs_lo[k] == 0.0f) continue;
            const mxp_eft::ff32 p_cur = {phi[k], mxp_eft::phi_lo[k]};
            const mxp_eft::ff32 r_cur = {rhs[k], mxp_eft::rhs_lo[k]};
            const mxp_eft::ff32 p_new = mxp_eft::ff_add(p_cur, mxp_eft::ff_mul(dt_ff, r_cur));
            phi[k] = p_new.hi;
            mxp_eft::phi_lo[k] = p_new.lo;
        }
    }
    g.exchange_f(phi);
    g.exchange_f(mxp_eft::phi_lo);

    const long flips = redistanceLowPrec(g, phi, phiOrig, ls_iterations);

    mxp_eft::time_ff = mxp_eft::ff_add(mxp_eft::time_ff, dt_ff);
    for (int j = 0; j < g.ny(); ++j) {
        for (int i = 0; i < g.nx(); ++i) {
            const int k = g.index(i, j);
            mxp_eft::prev_phi[k] = phi[k];
        }
    }

    return flips;
}
// EVOLVE-BLOCK-END

double computeTimestep(const Params& p, const Grid& g, double umax, double vmax, double time) {
    const float u_hi = static_cast<float>(umax);
    const float v_hi = static_cast<float>(vmax);
    mxp_eft::umax_lo = static_cast<float>(umax - static_cast<double>(u_hi));
    mxp_eft::vmax_lo = static_cast<float>(vmax - static_cast<double>(v_hi));
    const float dt_hi = computeTimestepLowPrec(
        static_cast<float>(p.cfl), static_cast<float>(p.fixed_dt), static_cast<float>(p.tmax), g,
        u_hi, v_hi, static_cast<float>(time));
    return static_cast<double>(dt_hi) + static_cast<double>(mxp_eft::dt_lo);
}

long advanceOneStep(const Grid& g, const Params& p, std::vector<double>& phi,
                    const std::vector<double>& u, const std::vector<double>& v, double dt,
                    std::vector<double>& rhs, std::vector<double>& phiOrig) {
    static std::vector<float> phi_f;
    static std::vector<float> u_f;
    static std::vector<float> v_f;
    static std::vector<float> rhs_f;
    static std::vector<float> orig_f;
    phi_f.resize(phi.size());
    u_f.resize(u.size());
    v_f.resize(v.size());
    rhs_f.assign(phi.size(), 0.0f);
    orig_f.resize(phiOrig.size());
    mxp_eft::phi_lo.resize(phi.size());
    mxp_eft::u_lo.resize(u.size());
    mxp_eft::v_lo.resize(v.size());
    for (std::size_t k = 0; k < phi.size(); ++k) {
        phi_f[k] = static_cast<float>(phi[k]);
        mxp_eft::phi_lo[k] = static_cast<float>(phi[k] - static_cast<double>(phi_f[k]));
        u_f[k] = static_cast<float>(u[k]);
        mxp_eft::u_lo[k] = static_cast<float>(u[k] - static_cast<double>(u_f[k]));
        v_f[k] = static_cast<float>(v[k]);
        mxp_eft::v_lo[k] = static_cast<float>(v[k] - static_cast<double>(v_f[k]));
    }
    const float dt_hi = static_cast<float>(dt);
    mxp_eft::dt_hi_cached = dt_hi;
    mxp_eft::dt_lo = static_cast<float>(dt - static_cast<double>(dt_hi));
    mxp_eft::preserve_external_lo = true;
    const long flips = advanceOneStepLowPrec(g, p.ls_iterations, phi_f, u_f, v_f, dt_hi, rhs_f, orig_f);
    mxp_eft::preserve_external_lo = false;
    rhs.resize(rhs_f.size());
    phiOrig.resize(orig_f.size());
    for (std::size_t k = 0; k < phi.size(); ++k) {
        phi[k] = static_cast<double>(phi_f[k]) + static_cast<double>(mxp_eft::phi_lo[k]);
        rhs[k] = static_cast<double>(rhs_f[k]) + static_cast<double>(mxp_eft::rhs_lo[k]);
        phiOrig[k] = static_cast<double>(orig_f[k]) + static_cast<double>(mxp_eft::orig_lo[k]);
    }
    return flips;
}

bool runSimulation(const Grid& g, const Params& p, std::vector<double>& dfun, RunSummary& summary,
                   std::string& err) {
    std::vector<float> phi_f(dfun.size());
    for (std::size_t k = 0; k < dfun.size(); ++k) {
        phi_f[k] = static_cast<float>(dfun[k]);
    }
    std::vector<float> u_f = g.allocate_f();
    std::vector<float> v_f = g.allocate_f();
    std::vector<float> rhs_f = g.allocate_f();
    std::vector<float> phiOrig_f = g.allocate_f();

    std::vector<double> u = g.allocate();
    std::vector<double> v = g.allocate();

    computeFaceVelocityLowPrec(g, false, static_cast<float>(p.tmax), 0.0f, u_f, v_f);
    float umax_f = 0.0f;
    float vmax_f = 0.0f;
    maxFaceSpeedsLowPrec(g, u_f, v_f, umax_f, vmax_f);

    if (!ensureOutputDir(g, p, err)) return false;
    if (!writeHistoryHeader(g, p, err)) return false;

    double time = 0.0;
    int step = 0;

    computeFaceVelocityLowPrec(g, p.reverse_time, static_cast<float>(p.tmax), 0.0f, u_f, v_f);
    for (std::size_t k = 0; k < dfun.size(); ++k) {
        dfun[k] = static_cast<double>(phi_f[k]);
        const double ulo = (k < mxp_eft::u_lo.size()) ? static_cast<double>(mxp_eft::u_lo[k]) : 0.0;
        const double uti = (k < mxp_eft::u_tiny.size()) ? static_cast<double>(mxp_eft::u_tiny[k]) : 0.0;
        const double vlo = (k < mxp_eft::v_lo.size()) ? static_cast<double>(mxp_eft::v_lo[k]) : 0.0;
        const double vti = (k < mxp_eft::v_tiny.size()) ? static_cast<double>(mxp_eft::v_tiny[k]) : 0.0;
        u[k] = static_cast<double>(u_f[k]) + (ulo + uti);
        v[k] = static_cast<double>(v_f[k]) + (vlo + vti);
    }
    Diagnostics diag = computeDiagnostics(g, dfun, u, v, 0L);
    summary.area_initial = diag.area;
    summary.max_divergence = diag.max_divergence;
    if (!writeFields(g, p, step, time, dfun, u, v, err)) return false;
    ++summary.plots_written;
    if (!appendHistory(g, p, step, time, 0.0, diag, err)) return false;
    ++summary.history_rows;

    const double tEps = 1.0e-6 * p.tmax;
    while (time < p.tmax - tEps) {
        if (p.max_steps > 0 && step >= p.max_steps) break;

        const float dt_f = computeTimestepLowPrec(
            static_cast<float>(p.cfl), static_cast<float>(p.fixed_dt), static_cast<float>(p.tmax),
            g, umax_f, vmax_f, static_cast<float>(time));
        if (dt_f <= 0.0f) break;
        const double dt = static_cast<double>(dt_f);

        computeFaceVelocityLowPrec(g, p.reverse_time, static_cast<float>(p.tmax),
                                   static_cast<float>(time), u_f, v_f);

        const long flips =
            advanceOneStepLowPrec(g, p.ls_iterations, phi_f, u_f, v_f, dt_f, rhs_f, phiOrig_f);
        summary.sign_flips_total += flips;

        time += dt;
        ++step;
        summary.dt_last = dt;

        const bool lastStep =
            (time >= p.tmax - tEps) || (p.max_steps > 0 && step >= p.max_steps);
        const bool wantHist = (p.hist_interval > 0 && step % p.hist_interval == 0) || lastStep;
        const bool wantPlot = (p.plot_interval > 0 && step % p.plot_interval == 0) || lastStep;

        if (wantHist || wantPlot) {
            for (std::size_t k = 0; k < dfun.size(); ++k) {
                dfun[k] = static_cast<double>(phi_f[k]);
                const double ulo = (k < mxp_eft::u_lo.size()) ? static_cast<double>(mxp_eft::u_lo[k]) : 0.0;
                const double uti = (k < mxp_eft::u_tiny.size()) ? static_cast<double>(mxp_eft::u_tiny[k]) : 0.0;
                const double vlo = (k < mxp_eft::v_lo.size()) ? static_cast<double>(mxp_eft::v_lo[k]) : 0.0;
                const double vti = (k < mxp_eft::v_tiny.size()) ? static_cast<double>(mxp_eft::v_tiny[k]) : 0.0;
                u[k] = static_cast<double>(u_f[k]) + (ulo + uti);
                v[k] = static_cast<double>(v_f[k]) + (vlo + vti);
            }
            diag = computeDiagnostics(g, dfun, u, v, flips);
            if (diag.max_divergence > summary.max_divergence) {
                summary.max_divergence = diag.max_divergence;
            }
        }
        if (wantHist) {
            if (!appendHistory(g, p, step, time, dt, diag, err)) return false;
            ++summary.history_rows;
        }
        if (wantPlot) {
            if (!writeFields(g, p, step, time, dfun, u, v, err)) return false;
            ++summary.plots_written;
        }
    }

    for (std::size_t k = 0; k < dfun.size(); ++k) {
        dfun[k] = static_cast<double>(phi_f[k]);
    }
    summary.steps = step;
    summary.time = time;
    summary.area_final = diag.area;
    summary.area_drift = (summary.area_initial > 0.0)
                             ? std::fabs(diag.area - summary.area_initial) / summary.area_initial
                             : 0.0;
    return true;
}
