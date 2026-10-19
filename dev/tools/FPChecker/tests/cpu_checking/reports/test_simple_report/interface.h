#ifndef INTERFACE_H
#define INTERFACE_H

#include <iostream>
#include <vector>
#include <string>

// Your code goes here
void printMatrix(const std::vector<std::vector<double>> &matrix);

// Function to print a vector
void printVector(const std::vector<double> &vec, const std::string &name);

double computeNorm(const std::vector<double> &vec);
void matrix_multiply(std::vector<double> &vector, std::vector<double> &resultVector, std::vector<std::vector<double>> &matrix);

#endif // INTERFACE_H