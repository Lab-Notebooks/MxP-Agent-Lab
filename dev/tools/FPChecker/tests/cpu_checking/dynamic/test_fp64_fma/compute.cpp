#include <vector>
#include <cmath>
#include "interface.h"

void matrix_multiply(std::vector<double> &vector, std::vector<double> &resultVector, std::vector<std::vector<double>> &matrix)
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
double computeNorm(const std::vector<double> &vec)
{
    double sum = 0.0;
    for (double val : vec)
    {
        sum += val * val;
    }
    return std::sqrt(sum);
}