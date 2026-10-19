#include <iostream>
#include <vector>
#include <iomanip> // For formatted output

#include "interface.h"

int main()
{
    // Define a 4x4 matrix
    std::vector<std::vector<float>> matrix = {
        {1.0, 2.0, 3.0, 4.0},
        {5.0, 6.0, 7.0, 8.0},
        {9.0, 10.0, 11.0, 12.0},
        {13.0, 14.0, 15.0, 160.0}};

    // Define a 4x1 vector
    std::vector<float> vector = {1.0, 2.0, 3.0, 1e25};

    // Print the input matrix and vector
    printMatrix(matrix);
    printVector(vector, "Input Vector");

    // Perform matrix-vector multiplication
    std::vector<float> resultVector(4, 0.0);

    matrix_multiply(vector, resultVector, matrix);

    // Print the resulting vector
    printVector(resultVector, "Output Vector");

    // Compute the norm of the resulting vector
    float norm = computeNorm(resultVector);
    std::cout << "Norm of the Output Vector: " << norm << std::endl;

    return 0;
}