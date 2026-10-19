#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <mpi.h>

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, nproc;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);

    // --- Problem Parameters ---
    const double L = 1.0;      // Length of the domain
    const int N_global = 100;  // Total number of spatial points (globally)
    const double alpha = 0.01; // Thermal diffusivity
    const double T_end = 1.0;  // Total simulation time
    const double dx = L / (N_global - 1);
    const double dt = 0.5 * dx * dx / alpha; // Stability condition: dt <= 0.5 * dx^2 / alpha
    const int num_time_steps = static_cast<int>(T_end / dt);

    // Ensure stable step size
    if (rank == 0)
    {
        if (dt > 0.5 * dx * dx / alpha)
        {
            std::cerr << "Error: dt is too large for stability!" << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        std::cout << "Running 1D Heat Diffusion with MPI domain decomposition." << std::endl;
        std::cout << "Global spatial points: " << N_global << ", Time steps: " << num_time_steps << std::endl;
        std::cout << "dx: " << dx << ", dt: " << dt << std::endl;
    }

    // --- Domain Decomposition ---
    int points_per_process = N_global / nproc;
    int remainder = N_global % nproc;

    // Distribute remaining points to the first 'remainder' processes
    int my_N_local = points_per_process + (rank < remainder ? 1 : 0);

    // Calculate starting global index for this process
    int my_global_start_index = 0;
    for (int i = 0; i < rank; ++i)
    {
        my_global_start_index += (points_per_process + (i < remainder ? 1 : 0));
    }

    // Size of the local array including 2 ghost cells (1 on each side)
    // Processes at boundaries only need one ghost cell, but using 2 simplifies communication logic.
    // We will handle boundary conditions separately.
    int my_array_size = my_N_local + 2; // +2 for ghost cells

    // --- Local Arrays ---
    std::vector<double> u_current(my_array_size);
    std::vector<double> u_next(my_array_size);

    // --- Initial Condition (Example: a sine wave) ---
    // Initialize only the internal points for this process
    for (int i = 0; i < my_N_local; ++i)
    {
        double global_x = (my_global_start_index + i) * dx;
        u_current[i + 1] = std::sin(M_PI * global_x); // Add 1 to index for ghost cell offset
    }

    // Set boundary conditions (Fixed temperature at ends)
    // Process 0 handles the left boundary, Process nproc-1 handles the right boundary
    if (rank == 0)
    {
        u_current[1] = 0.0; // Left physical boundary (index 1 in local array)
    }
    if (rank == nproc - 1)
    {
        u_current[my_N_local] = 0.0; // Right physical boundary (index my_N_local in local array)
    }

    // --- Time Stepping ---
    double alpha_dt_dx2 = alpha * dt / (dx * dx);

    for (int step = 0; step < num_time_steps; ++step)
    {

        // --- MPI Communication: Exchange Ghost Cells ---

        // Define neighboring ranks
        int left_neighbor = (rank == 0) ? MPI_PROC_NULL : rank - 1;
        int right_neighbor = (rank == nproc - 1) ? MPI_PROC_NULL : rank + 1;

        // Use MPI_Sendrecv for safe blocking exchange
        // Send right boundary to right neighbor's left ghost cell
        // Receive into left ghost cell from left neighbor's right boundary
        MPI_Sendrecv(&u_current[my_N_local], 1, MPI_DOUBLE, right_neighbor, 0,
                     &u_current[0], 1, MPI_DOUBLE, left_neighbor, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Send left boundary to left neighbor's right ghost cell
        // Receive into right ghost cell from right neighbor's left boundary
        MPI_Sendrecv(&u_current[1], 1, MPI_DOUBLE, left_neighbor, 0,
                     &u_current[my_N_local + 1], 1, MPI_DOUBLE, right_neighbor, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // --- Apply Physical Boundary Conditions (Ghost cells reflecting BCs) ---
        // For fixed boundary conditions, the ghost cells of the boundary processes
        // should reflect the fixed value.
        if (rank == 0)
        {
            u_current[0] = 0.0; // Left ghost cell (index 0) reflects left boundary
        }
        if (rank == nproc - 1)
        {
            u_current[my_N_local + 1] = 0.0; // Right ghost cell reflects right boundary
        }

        // --- Compute Next Time Step for Internal Points ---
        // Loop from index 1 to my_N_local (inclusive), these are the internal points
        for (int i = 1; i <= my_N_local; ++i)
        {
            u_next[i] = u_current[i] + alpha_dt_dx2 * (u_current[i - 1] - 2.0 * u_current[i] + u_current[i + 1]);
        }

        // Swap buffers for the next time step
        u_current.swap(u_next);

        // Optional: Print progress occasionally
        if (rank == 0 && (step + 1) % (num_time_steps / 10) == 0)
        {
            std::cout << "Time step " << step + 1 << "/" << num_time_steps << " completed." << std::endl;
        }
    }

    // --- Output or Gather Results ---
    // For a simple verification, each process can print its final segment.
    // A more robust solution would gather results to the root process.

    // Each process prints its final segment (excluding ghost cells)
    std::cout << "Rank " << rank << " final local solution:" << std::endl;
    std::cout << "Global indices [" << my_global_start_index << ", " << my_global_start_index + my_N_local - 1 << "]" << std::endl;
    std::cout << "Local indices [" << 1 << ", " << my_N_local << "]" << std::endl;

    for (int i = 1; i <= my_N_local; ++i)
    {
        // Print global index and corresponding value
        double global_x = (my_global_start_index + i - 1) * dx; // i-1 to get back to 0-based local index
        std::cout << "  x = " << std::fixed << std::setprecision(4) << global_x
                  << ", u = " << std::scientific << std::setprecision(8) << u_current[i] << std::endl;
    }
    std::cout << std::endl;

    MPI_Finalize();

    return 0;
}
