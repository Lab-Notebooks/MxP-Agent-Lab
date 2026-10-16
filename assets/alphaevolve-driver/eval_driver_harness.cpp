// eval_driver_harness.cpp -- Fast deterministic harness for AlphaEvolve driver evaluation
#include <mpi.h>
#include <cmath>
#include <cstdio>
#include <vector>

#include "driver.h"
#include "grid.h"
#include "init.h"
#include "io.h"
#include "params.h"
#include "redistance.h"

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    Params p;
    p.nx = 64;
    p.ny = 64;
    p.tmax = 0.5;
    p.cfl = 0.1;
    p.ls_iterations = 2;
    p.verbosity = 0;

    Grid g;
    std::string err;
    if (!g.create(p, MPI_COMM_WORLD, err)) {
        if (rank == 0) std::printf("FAIL grid: %s\n", err.c_str());
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    std::vector<double> phi;
    initLevelSet(g, p, phi);
    const double eps = heavisideWidth(g);
    const double cellArea = g.dx() * g.dy();

    auto measureArea = [&](const std::vector<double>& fld) {
        double local = 0.0;
        for (int j = 0; j < g.ny(); ++j) {
            for (int i = 0; i < g.nx(); ++i) {
                local += smoothedHeaviside(fld[g.index(i, j)], eps);
            }
        }
        double global = 0.0;
        MPI_Allreduce(&local, &global, 1, MPI_DOUBLE, MPI_SUM, g.comm());
        return global * cellArea;
    };

    const double a0 = measureArea(phi);
    std::vector<double> u = g.allocate();
    std::vector<double> v = g.allocate();
    std::vector<double> rhs = g.allocate();
    std::vector<double> phiOrig = g.allocate();

    computeFaceVelocity(g, p, 0.0, u, v);
    double umax = 0.0, vmax = 0.0;
    maxFaceSpeeds(g, u, v, umax, vmax);

    double time = 0.0;
    int step = 0;
    long total_flips = 0;
    const int max_steps = 150;

    while (step < max_steps && time < p.tmax) {
        const double dt = computeTimestep(p, g, umax, vmax, time);
        if (dt <= 0.0) break;
        computeFaceVelocity(g, p, time, u, v);
        total_flips += advanceOneStep(g, p, phi, u, v, dt, rhs, phiOrig);
        time += dt;
        ++step;
    }

    const double a1 = measureArea(phi);
    const double drift = (a0 > 0.0) ? std::fabs(a1 - a0) / a0 : 0.0;

    if (rank == 0) {
        std::printf("HARNESS_RESULT: steps=%d time=%.4f area0=%.6f area1=%.6f drift=%.6e flips=%ld\n",
                    step, time, a0, a1, drift, total_flips);
        if (drift <= 0.05) {
            std::printf("PASS\n");
        } else {
            std::printf("FAIL_DRIFT: drift=%.4f%% > 5%%\n", drift * 100.0);
        }
        std::fflush(stdout);
    }

    g.destroy();
    MPI_Finalize();
    return 0;
}
