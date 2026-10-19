
#include <iostream>
#include <vector>
#include <iomanip> // For formatted output
#include "interface.h"

// Function to print a matrix
void printMatrix(const std::vector<std::vector<double>> &matrix)
{
    std::cout << "Matrix:" << std::endl;
    for (const auto &row : matrix)
    {
        for (double val : row)
        {
            std::cout << std::setw(8) << val << " ";
        }
        std::cout << std::endl;
    }
}

// Function to print a vector
void printVector(const std::vector<double> &vec, const std::string &name)
{
    std::cout << name << ": ";
    for (double val : vec)
    {
        std::cout << val << " ";
    }
    std::cout << std::endl;
}