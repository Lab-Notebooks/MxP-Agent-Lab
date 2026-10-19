import numpy as np
import matplotlib.pyplot as plt
from matplotlib.tri import Triangulation

def load_temperature_vector(filename):
    try:
        # Read all lines from the file
        with open(filename, 'r') as infile:
            lines = infile.readlines()

        # Convert each line to a float and store in a list
        temperature_list = [float(line.strip()) for line in lines]

        # Convert the list to a NumPy array
        temperature_array = np.array(temperature_list)

        return temperature_array

    except FileNotFoundError:
        print(f"Error: File not found at {filename}")
        return None
    except ValueError:
        print(f"Error: Could not convert data in {filename} to float. Ensure the file contains valid numbers.")
        return None
    except Exception as e:
        print(f"An error occurred: {e}")
        return None
    
def load_coords(filename):
    try:
        # Use np.loadtxt to efficiently load data from the file
        coords = np.loadtxt(filename, dtype=np.float64)
        # np.loadtxt will automatically infer the shape (num_points, 2)
        return coords
    except FileNotFoundError:
        print(f"Error: Coords file not found at {filename}")
        return None
    except ValueError:
        print(f"Error: Could not convert data in {filename} to float. Ensure the file contains valid numbers.")
        return None
    except Exception as e:
        print(f"An error occurred while loading coords: {e}")
        return None

def load_triangles(filename):
    try:
        # Use np.loadtxt to efficiently load data from the file
        triangles = np.loadtxt(filename, dtype=np.int32)
        # np.loadtxt will automatically infer the shape (num_triangles, 3)
        return triangles
    except FileNotFoundError:
        print(f"Error: Triangles file not found at {filename}")
        return None
    except ValueError:
        print(f"Error: Could not convert data in {filename} to integer. Ensure the file contains valid integers.")
        return None
    except Exception as e:
        print(f"An error occurred while loading triangles: {e}")
        return None


# Example usage:
if __name__ == "__main__":
    # Assuming the C++ code saved the file as 'temperature_output.txt'
    filename = "temperature_output.txt"
    T = load_temperature_vector(filename)

    if T is not None:
        print("Temperature vector loaded successfully:")
        #print(T)
        print(f"Shape of loaded array: {T.shape}")


    # Assuming the C++ code saved files as 'coords.txt' and 'triangles.txt'
    coords = load_coords("coords.txt")
    triangles = load_triangles("triangles.txt")

    if coords is not None:
        print("Coordinates loaded successfully:")
        #print(coords)
        print(f"Shape of loaded coords array: {coords.shape}")

    if triangles is not None:
        print("\nTriangles loaded successfully:")
        #print(triangles)
        print(f"Shape of loaded triangles array: {triangles.shape}")

    # Create a Triangulation object
    triangulation = Triangulation(coords[:, 0], coords[:, 1], triangles)

    # Plot the heat map with the triangles
    plt.figure(figsize=(10, 8))
    plt.tripcolor(triangulation, T, cmap='hot', shading='flat')
    plt.colorbar(label='Temperature')
    plt.triplot(triangulation, color='black', linewidth=0.5)  # Draw triangle edges
    plt.title('Heat Map with Triangular Mesh')
    plt.xlabel('X coordinate')
    plt.ylabel('Y coordinate')
    plt.show()