import matplotlib.pyplot as plt
import numpy as np
import os

def van_der_pol(x, y, mu):
    """
    Args:
        x (float): The current value of x.
        y (float): The current value of y.
        mu (float): The nonlinearity parameter.

    Returns:
        tuple: A tuple containing the derivatives (dx/dt, dy/dt).
    """
    dxdt = y
    dydt = mu * (1 - x**2) * y - x
    return dxdt, dydt

if __name__ == "__main__":

    file_path = "van_der_pol_trajectory.dat"
    x_traj = []
    y_traj = []
    mu = None

    # Check if the file exists using os.path.exists()
    if os.path.exists(file_path):
        print(f"File '{file_path}' found. Reading content...")
        try:
            # Open the file in read mode ('r') with a 'with' statement
            # The 'with' statement ensures the file is closed automatically
            with open(file_path, 'r', encoding='utf-8') as fd:
                for line in fd:
                    if "mu" in line:
                        mu = float(line.split()[1])
                    else:
                        x, y = line.split()[1:]
                        x_traj.append(float(x))
                        y_traj.append(float(y))

        except Exception as e:
            # Handle potential errors during file reading (e.g., permissions)
            print(f"Error reading file '{file_path}': {e}")
            exit(1)
    else:
        # If the file does not exist, print a message and return None
        print(f"File '{file_path}' does not exist.")
        exit(1)
  
    print("Mu:", mu)

    # Plotting
    print("Plotting...")
    plt.figure(figsize=(10, 6))

    # Plot the vector field
    x_min, x_max = -3, 3
    y_min, y_max = -3, 3
    grid_density = 20
    # Create a grid for the vector field
    x_vf = np.linspace(x_min, x_max, grid_density)
    y_vf = np.linspace(y_min, y_max, grid_density)
    X_vf, Y_vf = np.meshgrid(x_vf, y_vf)
    U, V = van_der_pol(X_vf, Y_vf, mu)
    plt.quiver(X_vf, Y_vf, U, V, color='g', alpha=0.6, label='Vector Field')

    plt.plot(x_traj, y_traj, label=f'Trajectory (x0={x_traj[0]}, y0={y_traj[0]})')

    plt.xlabel('x')
    plt.ylabel('y')
    plt.title(f'Phase Portrait and Vector Field of Van der Pol Oscillator')
    #plt.xlim([x_min, x_max])
    #plt.ylim([y_min, y_max])
    plt.grid(True)
    plt.legend()
    plt.show()
