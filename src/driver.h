// driver.h -- time-evolution loop (U6).
//
// Ported from
//   Flash-X-Development/Flash-X/source/Simulation/SimulationMain/incompFlow/
//     DeformingBubble/Driver_evolveAll.F90-mc
// (per step: recompute the prescribed velocity, advect the level set, integrate
// in time, redistance, then diagnostics and output) and specified in
// dev/tasks/init-project/spec.md, "Advection", "Time integration",
// "Redistance", "Timestep".
//
// One step, in the reference's order:
//   1. face velocity u,v at the current time (steady, but recomputed as the
//      reference driver does; with reverse_time it is scaled by cos(pi t/tmax))
//   2. rhs = 0; WENO5 flux-difference advection accumulates into rhs
//   3. forward Euler:  phi <- phi + dt * rhs     (Stencils_integrateEuler)
//   4. ls_iterations = mph_lsIt = 2 redistance iterations, guard cells
//      refreshed between them
//
// Reduced precision: step 2 runs at p.precision.  Unless that is fp64, the
// loop also advances an FP64 shadow copy of the level set through the same
// steps (same dt, same velocity) and records the spatial norms of the
// difference in history.csv, so one run of one binary is the whole
// mixed-precision comparison.
#ifndef BUBBLE_DRIVER_H
#define BUBBLE_DRIVER_H

#include <string>
#include <vector>

#include "grid.h"
#include "io.h"
#include "params.h"

// CFL timestep: dt = cfl / (umax/dx + vmax/dy), clipped so the step lands
// exactly on tmax; `fixed_dt` overrides the estimate (bit-comparison runs).
// `umax`/`vmax` are the *unscaled* maximum face speeds, so reverse_time (which
// only multiplies the field by cos(pi t/tmax), |.| <= 1) cannot make the step
// unstable or make dt blow up as the field passes through zero.
double computeTimestep(const Params& p, const Grid& g, double umax, double vmax, double time);

// One timestep on an already-halo-filled `phi`: WENO5 advection + forward
// Euler + ls_iterations redistance iterations.  `u`,`v` are the face
// velocities for this step; `rhs`, `phiOrig` are scratch supplied by the
// caller so the loop does not reallocate.  The advection runs at p.precision;
// its wall time is added to `*advectSeconds` if given.  Returns the global
// sign-flip count reported by the redistance.
long advanceOneStep(const Grid& g, const Params& p, std::vector<double>& phi,
                    const std::vector<double>& u, const std::vector<double>& v, double dt,
                    std::vector<double>& rhs, std::vector<double>& phiOrig,
                    double* advectSeconds = nullptr);

// Summary of a completed run.
struct RunSummary {
    int steps = 0;
    double time = 0.0;
    double dt_last = 0.0;
    double area_initial = 0.0;
    double area_final = 0.0;
    double area_drift = 0.0;      // |final - initial| / initial
    double max_divergence = 0.0;  // over the whole run
    long sign_flips_total = 0;
    int plots_written = 0;
    int history_rows = 0;

    // Reduced precision vs the FP64 shadow trajectory (zero in an fp64 run).
    double err_l2_final = 0.0;     // RMS error of the final state
    double err_linf_max = 0.0;     // max Linf over the history rows
    double err_l2_traj = 0.0;      // sqrt(mean over history rows of err_l2^2)
    double area_drift_ref = 0.0;   // area drift of the shadow trajectory
    double advect_seconds = 0.0;   // advection wall time at p.precision (rank 0)
    double advect_seconds_ref = 0.0;  // advection wall time of the shadow (rank 0)
};

// Run the loop from t = 0 to p.tmax (or p.max_steps steps, whichever comes
// first), writing out/history.csv every p.hist_interval steps and the
// .vti/.pvti set every p.plot_interval steps, plus the initial and final
// states.  `dfun` must hold the initial condition with valid guard cells and
// is left holding the final field.  Returns false with `err` filled if output
// cannot be written.
bool runSimulation(const Grid& g, const Params& p, std::vector<double>& dfun, RunSummary& summary,
                   std::string& err);

#endif  // BUBBLE_DRIVER_H
