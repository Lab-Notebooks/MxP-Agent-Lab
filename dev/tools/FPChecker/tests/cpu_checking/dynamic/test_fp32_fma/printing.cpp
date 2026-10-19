
#include <iostream>
#include <vector>
#include <iomanip> // For formatted output
#include "interface.h"

// Function to print a matrix
void printMatrix(const std::vector<std::vector<float>> &matrix)
{
    std::cout << "Matrix:" << std::endl;
    for (const auto &row : matrix)
    {
        for (float val : row)
        {
            std::cout << std::setw(8) << val << " ";
        }
        std::cout << std::endl;
    }
}

// Function to print a vector
void printVector(const std::vector<float> &vec, const std::string &name)
{
    std::cout << name << ": ";
    for (float val : vec)
    {
        std::cout << val << " ";
    }
    std::cout << std::endl;
}