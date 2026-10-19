#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <limits>
#include <iomanip>

std::vector<std::vector<double>> load_matrix_from_csv(const std::string &filename)
{
    std::vector<std::vector<double>> matrix;
    std::ifstream file(filename);
    std::string line;

    if (!file.is_open())
    {
        std::cerr << "Error: Could not open file '" << filename << "'" << std::endl;
        return matrix; // Return an empty matrix in case of an error
    }

    while (std::getline(file, line))
    {
        std::vector<double> row;
        std::stringstream ss(line);
        std::string value;

        while (std::getline(ss, value, ','))
        {
            try
            {
                row.push_back(std::stod(value));
            }
            catch (const std::invalid_argument &e)
            {
                std::cerr << "Error: Invalid argument during conversion in file '" << filename << "': " << e.what() << std::endl;
                matrix.clear(); // Clear the matrix if an error occurs
                file.close();
                return matrix;
            }
            catch (const std::out_of_range &e)
            {
                std::cerr << "Error: Out of range during conversion in file '" << filename << "': " << e.what() << std::endl;
                matrix.clear(); // Clear the matrix if an error occurs
                file.close();
                return matrix;
            }
        }
        matrix.push_back(row);
    }

    file.close();
    return matrix;
}

void save_matrix_to_csv(const std::vector<std::vector<double>> &matrix, const std::string &filename)
{
    std::ofstream outputFile(filename);

    if (!outputFile.is_open())
    {
        std::cerr << "Error: Could not open file '" << filename << "' for writing." << std::endl;
        return;
    }

    // Get the maximum number of digits for a double
    int precision = std::numeric_limits<double>::max_digits10;

    // Set the precision for the output stream
    outputFile << std::fixed << std::setprecision(precision);

    for (size_t i = 0; i < matrix.size(); ++i)
    {
        for (size_t j = 0; j < matrix[i].size(); ++j)
        {
            outputFile << matrix[i][j];
            if (j < matrix[i].size() - 1)
            {
                outputFile << ",";
            }
        }
        outputFile << std::endl;
    }

    outputFile.close();
    std::cout << "Matrix successfully saved to '" << filename << "'" << std::endl;
}
