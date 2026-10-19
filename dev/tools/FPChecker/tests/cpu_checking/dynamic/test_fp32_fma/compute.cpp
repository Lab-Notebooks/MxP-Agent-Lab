#include <vector>
#include <cmath>
#include "interface.h"

void matrix_multiply(std::vector<float> &vector, std::vector<float> &resultVector, std::vector<std::vector<float>> &matrix)
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
float computeNorm(const std::vector<float> &vec)
{
    float sum = 0.0;
    for (float val : vec)
    {
        sum += val * val;
    }
    return std::sqrt(sum);
}