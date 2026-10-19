#ifndef BLAS_HPP
#define BLAS_HPP

#include <vector>

/***********************************************/
/* FP64 API                                    */
/***********************************************/

// Printing
void print_matrix_simple(const std::vector<std::vector<double>> &matrix);
void print_matrix(const std::vector<std::vector<double>> &matrix);

// Norms
double frobenius_norm(const std::vector<std::vector<double>> &matrix);
double infinity_norm(const std::vector<std::vector<double>> &matrix);
double l2_norm(const std::vector<double> &v);

// Matrix operations
std::vector<double> multiply_matrix_vector(const std::vector<std::vector<double>> &A, const std::vector<double> &x);
std::vector<std::vector<double>> multiply_matrix_constant(const std::vector<std::vector<double>> &A, const double &c);
std::vector<std::vector<double>> subtract_matrices(const std::vector<std::vector<double>> &A, const std::vector<std::vector<double>> &B);
std::vector<std::vector<double>> matrix_multiply(const std::vector<std::vector<double>> &A, const std::vector<std::vector<double>> &B);
std::vector<std::vector<double>> transpose_matrix(const std::vector<std::vector<double>> &matrix);

// Vector operations
std::vector<double> subtract_vectors(const std::vector<double> &x, const std::vector<double> &y);

/***********************************************/
/* FP32 API                                    */
/***********************************************/

std::vector<std::vector<float>> matrix_multiply(const std::vector<std::vector<float>> &A, const std::vector<std::vector<float>> &B);
std::vector<std::vector<float>> multiply_matrix_constant(const std::vector<std::vector<float>> &A, const float &c);
std::vector<std::vector<float>> transpose_matrix(const std::vector<std::vector<float>> &matrix);

#endif // BLAS_HPP