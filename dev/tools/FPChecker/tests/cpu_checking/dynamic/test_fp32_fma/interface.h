#ifndef INTERFACE_H
#define INTERFACE_H

#include <iostream>
#include <vector>
#include <string>

// Your code goes here
void printMatrix(const std::vector<std::vector<float>> &matrix);

// Function to print a vector
void printVector(const std::vector<float> &vec, const std::string &name);

float computeNorm(const std::vector<float> &vec);
void matrix_multiply(std::vector<float> &vector, std::vector<float> &resultVector, std::vector<std::vector<float>> &matrix);

#endif // INTERFACE_H