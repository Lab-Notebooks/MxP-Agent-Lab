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
float computeTimestepLowPrec(float cfl, float fixed_dt, float tmax, const Grid& g, float umax,
                             float vmax, float time) {
    float dt = 0.0f;
    if (fixed_dt > 0.0f) {
        dt = fixed_dt;
    } else {
        const float denom = umax / g.dx_f() + vmax / g.dy_f();
        dt = (denom > 0.0f) ? (cfl / denom) : (tmax - time);
    }
    const float remaining = tmax - time;
    if (dt > remaining) dt = remaining;
    return dt;
}

long advanceOneStepLowPrec(const Grid& g, int ls_iterations, std::vector<float>& phi,
                           const std::vector<float>& u, const std::vector<float>& v, float dt,
                           std::vector<float>& rhs, std::vector<float>& phiOrig) {
    if (rhs.size() != phi.size()) rhs.assign(phi.size(), 0.0f);

    std::fill(rhs.begin(), rhs.end(), 0.0f);
    advectWenoLowPrec(g, phi, u, v, rhs);

    static std::vector<float> comp;
    static std::vector<float> prev_phi;
    static std::vector<float> pre_redist;

    if (comp.size() != phi.size()) {
        comp.assign(phi.size(), 0.0f);
        prev_phi.assign(phi.size(), 0.0f);
        pre_redist.assign(phi.size(), 0.0f);
    }

    int mismatches = 0;
    for (int j = 0; j < g.ny() && mismatches < 5; ++j) {
        for (int i = 0; i < g.nx() && mismatches < 5; ++i) {
            const int k = g.index(i, j);
            if (phi[k] != prev_phi[k]) {
                ++mismatches;
            }
        }
    }
    if (mismatches > 0) {
        comp.assign(phi.size(), 0.0f);
    }

    for (int j = 0; j < g.ny(); ++j) {
        for (int i = 0; i < g.nx(); ++i) {
            const int k = g.index(i, j);
            const float update = dt * rhs[k] + comp[k];
            volatile float s_v = phi[k] + update;
            const float s = s_v;
            volatile float bp_v = s - phi[k];
            volatile float ap_v = s - bp_v;
            volatile float da_v = phi[k] - ap_v;
            volatile float db_v = update - bp_v;
            const float err = da_v + db_v;

            phi[k] = s;
            comp[k] = err;
            pre_redist[k] = s;
        }
    }
    g.exchange_f(phi);

    const long flips = redistanceLowPrec(g, phi, phiOrig, ls_iterations);

    for (int j = 0; j < g.ny(); ++j) {
        for (int i = 0; i < g.nx(); ++i) {
            const int k = g.index(i, j);
            if (phi[k] != pre_redist[k]) {
                comp[k] = 0.0f;
            }
            prev_phi[k] = phi[k];
        }
    }

    return flips;
}
// EVOLVE-BLOCK-END

double computeTimestep(const Params& p, const Grid& g, double umax, double vmax, double time) {
    return static_cast<double>(computeTimestepLowPrec(
        static_cast<float>(p.cfl), static_cast<float>(p.fixed_dt), static_cast<float>(p.tmax), g,
        static_cast<float>(umax), static_cast<float>(vmax), static_cast<float>(time)));
}

long advanceOneStep(const Grid& g, const Params& p, std::vector<double>& phi,
                    const std::vector<double>& u, const std::vector<double>& v, double dt,
                    std::vector<double>& rhs, std::vector<double>& phiOrig) {
    std::vector<float> phi_f(phi.size());
    std::vector<float> u_f(u.size());
    std::vector<float> v_f(v.size());
    std::vector<float> rhs_f(phi.size(), 0.0f);
    std::vector<float> orig_f(phiOrig.size(), 0.0f);
    for (std::size_t k = 0; k < phi.size(); ++k) {
        phi_f[k] = static_cast<float>(phi[k]);
        u_f[k] = static_cast<float>(u[k]);
        v_f[k] = static_cast<float>(v[k]);
    }
    const long flips = advanceOneStepLowPrec(g, p.ls_iterations, phi_f, u_f, v_f,
                                             static_cast<float>(dt), rhs_f, orig_f);
    rhs.resize(rhs_f.size());
    phiOrig.resize(orig_f.size());
    for (std::size_t k = 0; k < phi.size(); ++k) {
        phi[k] = static_cast<double>(phi_f[k]);
        rhs[k] = static_cast<double>(rhs_f[k]);
        phiOrig[k] = static_cast<double>(orig_f[k]);
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
        u[k] = static_cast<double>(u_f[k]);
        v[k] = static_cast<double>(v_f[k]);
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
                u[k] = static_cast<double>(u_f[k]);
                v[k] = static_cast<double>(v_f[k]);
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
