#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <numeric>
#include <iomanip>
#include <string>
#include <sstream>

#include "blas.hpp"

using namespace std;

/***********************************************/
/* FP64 API                                    */
/***********************************************/

// Print a matrix (for debugging) using a basic format
void print_matrix_simple(const vector<vector<double>> &matrix)
{
    int rows = matrix.size();
    if (rows == 0)
        return;
    int cols = matrix[0].size();
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            cout << matrix[i][j] << "\t";
        }
        cout << endl;
    }
    cout << endl;
}

// Remove trailing zeros and potentially the decimal point
string trim_trailing_zeros(string s)
{
    size_t decimal_pos = s.find('.');
    if (decimal_pos != string::npos)
    {
        size_t last_digit = s.length() - 1;
        while (last_digit > decimal_pos && s[last_digit] == '0')
        {
            s.pop_back();
            last_digit--;
        }
        if (last_digit == decimal_pos)
        {
            s.pop_back(); // Remove trailing decimal point if no digits after it
        }
    }
    return s;
}

// Print a matrix (for debugging)
void print_matrix(const vector<vector<double>> &matrix)
{
    int rows = matrix.size();
    if (rows == 0)
        return;
    int cols = matrix[0].size();
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            double value = matrix[i][j];

            if (abs(value) < 1e-9)
            {
                cout << "0\t";
                continue;
            }

            string formatted_value;
            bool printed = false;

            for (int p = 6; p >= 0; --p)
            {
                stringstream ss;
                ss << fixed << setprecision(p) << value;
                formatted_value = ss.str();
                string trimmed_value = trim_trailing_zeros(formatted_value);
                if (trimmed_value.length() <= 7)
                {
                    cout << trimmed_value << "\t";
                    printed = true;
                    break;
                }
            }

            if (!printed)
            {
                string original_value_str;
                stringstream ss_orig;
                ss_orig << value;
                original_value_str = ss_orig.str();
                if (original_value_str.length() > 7)
                {
                    cout << original_value_str.substr(0, 7) << "\t";
                }
                else
                {
                    cout << original_value_str << "\t";
                }
            }
        }
        cout << endl;
    }
    cout << endl;
}

/*
void print_matrix(const vector<vector<double>> &matrix)
{
    int rows = matrix.size();
    if (rows == 0)
        return;
    int cols = matrix[0].size();
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            double value = matrix[i][j];

            // Check if the value is zero
            if (abs(value) < 1e-9)
            { // Using a small tolerance for floating-point comparison
                cout << "0\t";
                continue;
            }

            string formatted_value;
            bool printed = false;

            for (int p = 6; p >= 0; --p)
            {
                stringstream ss;
                ss << fixed << setprecision(p) << value;
                formatted_value = ss.str();
                if (formatted_value.length() <= 7)
                {
                    cout << formatted_value << "\t";
                    printed = true;
                    break;
                }
            }

            if (!printed)
            {
                string original_value_str;
                stringstream ss_orig;
                ss_orig << value;
                original_value_str = ss_orig.str();
                if (original_value_str.length() > 7)
                {
                    cout << original_value_str.substr(0, 7) << "\t";
                }
                else
                {
                    cout << original_value_str << "\t";
                }
            }
        }
        cout << endl;
    }
    cout << endl;
}
*/

// Calculate the Frobenius norm of a matrix
double frobenius_norm(const vector<vector<double>> &matrix)
{
    double sum_of_squares = 0.0;
    for (const auto &row : matrix)
    {
        for (double val : row)
        {
            sum_of_squares += val * val;
        }
    }
    return sqrt(sum_of_squares);
}

// Calculate the infinity norm (row sum norm) of a matrix
double infinity_norm(const vector<vector<double>> &matrix)
{
    double max_row_sum = 0.0;
    for (const auto &row : matrix)
    {
        double row_sum = 0.0;
        for (double val : row)
        {
            row_sum += abs(val);
        }
        if (row_sum > max_row_sum)
        {
            max_row_sum = row_sum;
        }
    }
    return max_row_sum;
}

// Calculate the L2 norm (Euclidean norm) of a vector
double l2_norm(const vector<double> &v)
{
    double sum_of_squares = 0.0;
    for (double val : v)
    {
        sum_of_squares += val * val;
    }
    return sqrt(sum_of_squares);
}

// -----------------------------------------------------------
// Matrix and Vector Operations
// -----------------------------------------------------------

// Multiply a matrix A by a vector x
vector<double> multiply_matrix_vector(const vector<vector<double>> &A, const vector<double> &x)
{
    int rows_A = A.size();
    if (rows_A == 0)
    {
        return {}; // Empty matrix results in an empty vector
    }
    int cols_A = A[0].size();
    int size_x = x.size();

    if (cols_A != size_x)
    {
        throw invalid_argument("Error: The number of columns in the matrix A must be equal to the size of the vector x for multiplication.");
    }

    vector<double> result(rows_A, 0.0);

    for (int i = 0; i < rows_A; ++i)
    {
        for (int j = 0; j < cols_A; ++j)
        {
            result[i] += A[i][j] * x[j];
        }
    }

    return result;
}

// Subtract two vectors x - y
vector<double> subtract_vectors(const vector<double> &x, const vector<double> &y)
{
    int size_x = x.size();
    int size_y = y.size();

    if (size_x != size_y)
    {
        throw invalid_argument("Error: The sizes of the vectors x and y must be equal for subtraction.");
    }

    vector<double> result(size_x);

    for (int i = 0; i < size_x; ++i)
    {
        result[i] = x[i] - y[i];
    }

    return result;
}

// Subtract two matrices
vector<vector<double>> subtract_matrices(const vector<vector<double>> &A, const vector<vector<double>> &B)
{
    int rows_A = A.size();
    int cols_A = (rows_A > 0) ? A[0].size() : 0;
    int rows_B = B.size();
    int cols_B = (rows_B > 0) ? B[0].size() : 0;

    if (rows_A != rows_B || cols_A != cols_B)
    {
        cerr << "Error: Matrices must have the same dimensions for subtraction." << endl;
        return {};
    }

    vector<vector<double>> result(rows_A, vector<double>(cols_A));
    for (int i = 0; i < rows_A; ++i)
    {
        for (int j = 0; j < cols_A; ++j)
        {
            result[i][j] = A[i][j] - B[i][j];
        }
    }
    return result;
}

// Matrix multiplication
// FPC_INSTRUMENT_FUNC
vector<vector<double>> matrix_multiply(const vector<vector<double>> &A, const vector<vector<double>> &B)
{
    int rows_A = A.size();
    if (rows_A == 0)
    {
        return {};
    }
    int cols_A = A[0].size();

    int rows_B = B.size();
    if (rows_B == 0)
    {
        return {};
    }
    int cols_B = B[0].size();

    if (cols_A != rows_B)
    {
        cerr << "Error: Number of columns must be equal to the number of rows for multiplication." << endl;
        return {};
    }

    vector<vector<double>> result(rows_A, vector<double>(cols_B, 0.0));

    for (int i = 0; i < rows_A; ++i)
    {
        for (int j = 0; j < cols_B; ++j)
        {
            for (int k = 0; k < cols_A; ++k)
            {
                result[i][j] += A[i][k] * B[k][j];
            }
        }
    }

    // Using Kahan Summation Algorithm
    /*for (int i = 0; i < rows_A; ++i)
    {
        for (int j = 0; j < cols_B; ++j)
        {
            double sum = 0.0;
            double c = 0.0; // Compensation for lost low-order bits
            for (int k = 0; k < cols_B; ++k)
            {
                double y = A[i][k] * B[k][j] - c;
                double t = sum + y;
                c = (t - sum) - y; // High-order bits of y lost
                sum = t;
            }
            result[i][j] = sum;
        }
    }*/

    return result;
}

std::vector<std::vector<double>> transpose_matrix(const std::vector<std::vector<double>> &matrix)
{
    if (matrix.empty() || matrix[0].empty())
    {
        return {}; // Return an empty matrix for empty input
    }

    size_t rows = matrix.size();
    size_t cols = matrix[0].size();
    std::vector<std::vector<double>> transposed_matrix(cols, std::vector<double>(rows));

    for (size_t i = 0; i < rows; ++i)
    {
        for (size_t j = 0; j < cols; ++j)
        {
            transposed_matrix[j][i] = matrix[i][j];
        }
    }

    return transposed_matrix;
}

std::vector<std::vector<double>> multiply_matrix_constant(const std::vector<std::vector<double>> &A, const double &c)
{
    size_t rows_A = A.size();
    if (rows_A == 0)
    {
        return {};
    }
    size_t cols_A = A[0].size();

    vector<vector<double>> result(A);

    for (size_t row = 0; row < rows_A; ++row)
    {
        for (size_t col = 0; col < cols_A; ++col)
        {
            result[row][col] *= c;
        }
    }
    return result;
}

/***********************************************/
/* FP32 API                                    */
/***********************************************/

// Matrix multiplication
vector<vector<float>> matrix_multiply(const vector<vector<float>> &A, const vector<vector<float>> &B)
{
    int rows_A = A.size();
    if (rows_A == 0)
    {
        return {};
    }
    int cols_A = A[0].size();

    int rows_B = B.size();
    if (rows_B == 0)
    {
        return {};
    }
    int cols_B = B[0].size();

    if (cols_A != rows_B)
    {
        cerr << "Error: Number of columns must be equal to the number of rows for multiplication." << endl;
        return {};
    }

    vector<vector<float>> result(rows_A, vector<float>(cols_B, 0.0));

    for (int i = 0; i < rows_A; ++i)
    {
        for (int j = 0; j < cols_B; ++j)
        {
            for (int k = 0; k < cols_A; ++k)
            {
                result[i][j] += A[i][k] * B[k][j];
            }
        }
    }

    return result;
}

std::vector<std::vector<float>> multiply_matrix_constant(const std::vector<std::vector<float>> &A, const float &c)
{
    size_t rows_A = A.size();
    if (rows_A == 0)
    {
        return {};
    }
    size_t cols_A = A[0].size();

    vector<vector<float>> result(A);

    for (size_t row = 0; row < rows_A; ++row)
    {
        for (size_t col = 0; col < cols_A; ++col)
        {
            result[row][col] *= c;
        }
    }
    return result;
}

std::vector<std::vector<float>> transpose_matrix(const std::vector<std::vector<float>> &matrix)
{
    if (matrix.empty() || matrix[0].empty())
    {
        return {}; // Return an empty matrix for empty input
    }

    size_t rows = matrix.size();
    size_t cols = matrix[0].size();
    std::vector<std::vector<float>> transposed_matrix(cols, std::vector<float>(rows));

    for (size_t i = 0; i < rows; ++i)
    {
        for (size_t j = 0; j < cols; ++j)
        {
            transposed_matrix[j][i] = matrix[i][j];
        }
    }

    return transposed_matrix;
}