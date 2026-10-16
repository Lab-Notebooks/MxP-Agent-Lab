#include <mpi.h>
#include <chrono>
#include <cstdio>
#include <vector>
#include "advect.h"
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
    p.nx = 128;
    p.ny = 128;
    p.tmax = 2.0;
    p.cfl = 0.05;
    p.max_steps = 1024;
    p.ls_iterations = 2;

    Grid g;
    std::string err;
    g.create(p, MPI_COMM_WORLD, err);

    std::vector<double> phi, u = g.allocate(), v = g.allocate(), rhs = g.allocate(), phiOrig = g.allocate();
    initLevelSet(g, p, phi);
    computeFaceVelocity(g, p, 0.0, u, v);
    double umax = 0.0, vmax = 0.0;
    maxFaceSpeeds(g, u, v, umax, vmax);

    double t_init_vel = 0.0;
    double t_advect = 0.0;
    double t_driver_euler_halo = 0.0;
    double t_redistance = 0.0;
    double time = 0.0;

    auto t_start_total = MPI_Wtime();
    for (int step = 0; step < p.max_steps; ++step) {
        double dt = computeTimestep(p, g, umax, vmax, time);

        double t0 = MPI_Wtime();
        computeFaceVelocity(g, p, time, u, v);
        double t1 = MPI_Wtime();
        t_init_vel += (t1 - t0);

        std::fill(rhs.begin(), rhs.end(), 0.0);
        advectWeno(g, phi, u, v, rhs);
        double t2 = MPI_Wtime();
        t_advect += (t2 - t1);

        for (int j = 0; j < g.ny(); ++j) {
            for (int i = 0; i < g.nx(); ++i) {
                const int k = g.index(i, j);
                phi[k] += dt * rhs[k];
            }
        }
        g.exchange(phi);
        double t3 = MPI_Wtime();
        t_driver_euler_halo += (t3 - t2);

        redistance(g, phi, phiOrig, p.ls_iterations);
        double t4 = MPI_Wtime();
        t_redistance += (t4 - t3);

        time += dt;
    }
    double t_total = MPI_Wtime() - t_start_total;

    if (rank == 0) {
        std::printf("{\n");
        std::printf("  \"grid\": \"128x128\",\n");
        std::printf("  \"steps\": %d,\n", p.max_steps);
        std::printf("  \"total_sec\": %.6f,\n", t_total);
        std::printf("  \"hotspots\": {\n");
        std::printf("    \"advectWeno\": {\"sec\": %.6f, \"pct\": %.2f},\n", t_advect, 100.0 * t_advect / t_total);
        std::printf("    \"redistance\": {\"sec\": %.6f, \"pct\": %.2f},\n", t_redistance, 100.0 * t_redistance / t_total);
        std::printf("    \"computeFaceVelocity\": {\"sec\": %.6f, \"pct\": %.2f},\n", t_init_vel, 100.0 * t_init_vel / t_total);
        std::printf("    \"driver_euler_halo\": {\"sec\": %.6f, \"pct\": %.2f}\n", t_driver_euler_halo, 100.0 * t_driver_euler_halo / t_total);
        std::printf("  }\n}\n");
    }

    g.destroy();
    MPI_Finalize();
    return 0;
}
