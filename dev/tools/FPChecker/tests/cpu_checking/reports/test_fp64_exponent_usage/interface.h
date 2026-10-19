#ifndef INTERFACE_H
#define INTERFACE_H

#include <iostream>
#include <vector>
#include <string>

typedef double Real_t;
//typedef float Real_t;

void printMatrix(const std::vector<std::vector<Real_t>> &matrix);

// Function to print a vector
void printVector(const std::vector<Real_t> &vec, const std::string &name);

Real_t computeNorm(const std::vector<Real_t> &vec);
void matrix_multiply(std::vector<Real_t> &vector, std::vector<Real_t> &resultVector, std::vector<std::vector<Real_t>> &matrix);

#endif // INTERFACE_H
