
#include <iostream>
#include <vector>
#include <iomanip> // For formatted output
#include "interface.h"

// Function to print a matrix
void printMatrix(const std::vector<std::vector<Real_t>> &matrix)
{
    std::cout << "Matrix:" << std::endl;
    for (const auto &row : matrix)
    {
        for (Real_t val : row)
        {
            std::cout << std::setw(8) << val << " ";
        }
        std::cout << std::endl;
    }
}

// Function to print a vector
void printVector(const std::vector<Real_t> &vec, const std::string &name)
{
    std::cout << name << ": ";
    for (Real_t val : vec)
    {
        std::cout << val << " ";
    }
    std::cout << std::endl;
}