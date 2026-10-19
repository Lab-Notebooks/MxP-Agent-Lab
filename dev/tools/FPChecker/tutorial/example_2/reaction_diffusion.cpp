#include <iostream>
#include <vector>
#include <cmath>     // For sin and M_PI
#include <iomanip>   // For setting output precision
#include <algorithm> // For std::max_element

typedef double Real_t;
// typedef float Real_t;

// Defines the initial condition
Real_t initial_condition(Real_t x, Real_t L)
{
    // A simple sine wave pulse that satisfies u(0)=u(L)=0
    return std::sin((Real_t)M_PI * x / L);
}

// Finds and prints the maximum value in the solution vector
void print_max_value(const std::vector<Real_t> &u, Real_t time)
{
    auto max_it = std::max_element(u.begin(), u.end());
    Real_t max_val = *max_it;
    std::cout << std::fixed << std::setprecision(6); // Set precision for time
    std::cout << "At time t = " << time << ", Maximum value of u = ";
    std::cout << std::scientific << std::setprecision(10) << max_val << std::endl; // Use scientific notation for large values
}

int main()
{
    // --- Parameters ---
    const Real_t L = 1.0;       // Length of the spatial domain [0, L]
    const Real_t T = 4.0;       // Total simulation time.
    const int N = 101;          // Number of spatial points (includes boundaries). N-1 intervals.
    const Real_t D = 0.01;      // Diffusion coefficient
    const Real_t lambda = 25.0; // Reaction rate constant (Note: large positive lambda leads to rapid growth).

    // --- Derived Parameters ---
    const Real_t dx = L / (N - 1); // Spatial step size (0.01)
    // Determine time step based on desired number of steps OR stability
    // For explicit method, stability requires dt <= dx^2 / (2*D) without reaction
    // With reaction, stability is more complex, roughly dt <= 1 / (2*D/dx^2 + lambda)
    // Let's choose a dt that satisfies this roughly for stability
    // For D=0.01, dx=0.01, lambda=25.0: dt <= 1 / (2*0.01/0.01^2 + 25) = 1 / (200 + 25) approx 4.44e-03
    const int M = 80000;     // Number of time steps. M = T / dt --> dt = T/M
    const Real_t dt = T / M; // Time step size, 5e-05

    // Check the chosen dt against a simple stability criterion (approximate)
    Real_t stability_limit = 1.0 / (2.0 * D / (dx * dx) + lambda);
    if (dt > stability_limit)
    {
        std::cerr << "WARNING: Chosen dt (" << dt << ") might violate stability criterion (" << stability_limit << ") for explicit method." << std::endl;
        std::cerr << "Expect numerical instability (oscillations, explosion to inf) before true blow-up." << std::endl;
        std::cerr << "Consider increasing M or decreasing T or lambda." << std::endl;
    }
    else
    {
        std::cout << "Stability criterion satisfied: " << dt << " <= " << stability_limit << std::endl;
    }

    std::cout << "Solving 1D Reaction-Diffusion PDE: du/dt = D*d2u/dx2 + lambda*u" << std::endl;
    std::cout << "Parameters: L=" << L << ", T=" << T << ", N=" << N << ", M=" << M << ", D=" << D << ", lambda=" << lambda << std::endl;
    std::cout << "Derived: dx=" << dx << ", dt=" << dt << std::endl;
    std::cout << "Approximate explicit stability limit for dt: " << stability_limit << std::endl;

    // --- Data Structures ---
    // Two vectors to store the solution at the current and next time steps
    std::vector<Real_t> u_current(N);
    std::vector<Real_t> u_next(N);

    // --- Initial Condition ---
    // Set the initial values for u_current based on the initial_condition function
    for (int i = 0; i < N; ++i)
    {
        u_current[i] = initial_condition(i * dx, L);
    }
    print_max_value(u_current, 0.0); // Print initial max value

    // --- Time Stepping Loop ---
    Real_t current_time = 0.0;
    // Interval to print solution during the simulation
    const int print_interval = M / 50;

    // Let's pre-calculate constants for efficiency
    const Real_t D_dt_over_dx2 = D * dt / (dx * dx);
    const Real_t lambda_dt = lambda * dt;

    for (int n = 0; n < M; ++n)
    {
        current_time += dt;

        // --- Apply Boundary Conditions for the next time step ---
        // Using Dirichlet Boundary Conditions: u(0, t) = 0 and u(L, t) = 0
        u_next[0] = 0.0;
        u_next[N - 1] = 0.0;

        // --- Apply Finite Difference Scheme for Interior Points ---
        // The explicit update formula:
        // u_i^{n+1} = u_i^n + dt * ( D * (u_{i+1}^n - 2*u_i^n + u_{i-1}^n)/dx^2 + lambda * u_i^n )
        for (int i = 1; i < N - 1; ++i)
        {
            // Diffusion term approximation at point i using values from time step n
            Real_t diffusion_term = D_dt_over_dx2 * (u_current[i + 1] - 2.0 * u_current[i] + u_current[i - 1]);

            // Reaction term contribution at point i using value from time step n
            Real_t reaction_term = lambda_dt * u_current[i];

            // Update the solution at point i for the next time step
            u_next[i] = u_current[i] + diffusion_term + reaction_term;

            // Equivalent consolidated update (might be slightly different due to order of ops):
            // u_next[i] = u_current[i] * (1.0 + lambda_dt) + D_dt_over_dx2 * (u_current[i+1] - 2.0 * u_current[i] + u_current[i-1]);
        }

        // --- Update for Next Time Step ---
        // The solution at the next time step (u_next) becomes the current solution (u_current)
        u_current = u_next; // Efficient vector assignment (copies contents)

        // --- Output/Logging ---
        // Print the maximum value periodically to observe growth
        if ((n + 1) % print_interval == 0 || n == M - 1)
        {
            print_max_value(u_current, current_time);
        }
    }

    // --- Final Output (optional: print the entire final state) ---
    /*
    std::cout << "\nFinal solution profile at T=" << T << ":" << std::endl;
    std::cout << std::fixed << std::setprecision(6); // Reset precision for profile
    for (int i = 0; i < N; ++i) {
        std::cout << "x = " << i * dx << ", u = " << u_current[i] << std::endl;
    }
    */

    std::cout << "\nSimulation finished." << std::endl;

    return 0;
}