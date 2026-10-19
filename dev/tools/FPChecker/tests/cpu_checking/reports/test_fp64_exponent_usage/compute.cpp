#include <vector>
#include <cmath>
#include "interface.h"

void matrix_multiply(std::vector<Real_t> &vector, std::vector<Real_t> &resultVector, std::vector<std::vector<Real_t>> &matrix)
{
    for (int i = 0; i < vector.size(); ++i)
    {
        for (int j = 0; j < vector.size(); ++j)
        {
            resultVector[i] += matrix[i][j] * vector[j];
        }
    }
}

// Function to compute the norm of a vector
Real_t computeNorm(const std::vector<Real_t> &vec)
{
    Real_t sum = 0.0;
    for (Real_t val : vec)
    {
        sum += val * val;
    }
    return std::sqrt(sum);
}