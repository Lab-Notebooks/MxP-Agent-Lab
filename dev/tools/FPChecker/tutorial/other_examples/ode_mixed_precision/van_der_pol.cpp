#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <iomanip> // For setting precision

typedef double Real_fp64;
typedef float Real_fp32;

// Structure to hold the state variables
struct State
{
    Real_fp64 x;
    Real_fp64 y;
};

// Derivatives of the Van der Pol oscillator
State van_der_pol(const State &state, Real_fp64 mu)
{
    State derivatives;
    derivatives.x = state.y;
    derivatives.y = (Real_fp32)mu * (1.0f - (Real_fp32)state.x * (Real_fp32)state.x) * (Real_fp32)state.y - (Real_fp32)state.x;
    return derivatives;
}

// Perform one step of the 4th order Runge-Kutta method
State rk4_step(const State &state, Real_fp64 h, Real_fp64 mu)
{
    State k1 = van_der_pol(state, mu);
    State k2 = van_der_pol({state.x + h / (Real_fp64)2.0 * k1.x, state.y + h / (Real_fp64)2.0 * k1.y}, mu);
    State k3 = van_der_pol({state.x + h / (Real_fp64)2.0 * k2.x, state.y + h / (Real_fp64)2.0 * k2.y}, mu);
    State k4 = van_der_pol({state.x + h * k3.x, state.y + h * k3.y}, mu);

    State next_state;
    next_state.x = state.x + h / (Real_fp64)6.0 * (k1.x + (Real_fp64)2.0 * k2.x + (Real_fp64)2.0 * k3.x + k4.x);
    next_state.y = state.y + h / (Real_fp64)6.0 * (k1.y + (Real_fp64)2.0 * k2.y + (Real_fp64)2.0 * k3.y + k4.y);
    return next_state;
}

// Solve the Van der Pol oscillator using the RK4 method and save to file
void solve_van_der_pol_rk4(Real_fp64 mu, State initial_state, Real_fp64 t_start, Real_fp64 t_end, Real_fp64 h, const std::string &filename)
{
    std::ofstream outfile(filename);
    if (!outfile.is_open())
    {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    outfile << std::fixed << std::setprecision(10); // Set precision for output

    // Save mu in the file
    outfile << "mu" << " " << mu << std::endl;

    Real_fp64 t = t_start;
    State current_state = initial_state;

    while (t <= t_end)
    {
        outfile << t << " " << current_state.x << " " << current_state.y << std::endl;
        std::cout << t << " " << current_state.x << " " << current_state.y << std::endl;
        current_state = rk4_step(current_state, h, mu);
        t += h;
    }

    outfile.close();
    std::cout << "Trajectories saved to: " << filename << std::endl;
}

int main(int argc, char *argv[])
{
    // Check if the correct number of arguments is provided
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <mu> <init_x> <init_y> <time_step>" << std::endl;
        return 1;
    }

    // Variables to store the numerical parameters
    Real_fp64 mu_value = 1.0;
    Real_fp64 init_x = 0.0, init_y = 0.0;
    Real_fp64 time_step = 0.01;

    try
    {
        mu_value = std::stod(argv[1]);
        init_x = std::stod(argv[2]);
        init_y = std::stod(argv[2]);
        time_step = std::stod(argv[2]);
    }
    catch (const std::invalid_argument &ia)
    {
        std::cerr << "Invalid argument: " << ia.what() << std::endl;
        return 1;
    }

    // Set other parameters
    State initial_conditions = {init_x, init_y};
    Real_fp64 t_start = 0.0;
    Real_fp64 t_end = 100.0;
    // Real_fp64 time_step = 0.01;
    std::string output_filename = "van_der_pol_trajectory.dat";

    // Solve using RK4 and save to file
    solve_van_der_pol_rk4(mu_value, initial_conditions, t_start, t_end, time_step, output_filename);

    return 0;
}