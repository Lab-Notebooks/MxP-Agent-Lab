#include <iostream>
#include <vector>

#include "linear_solvers.hpp"
#include "blas.hpp"
#include "io.hpp"

using namespace std;

int main()
{
    // Example matrix 1
    vector<vector<double>> A1 =
        {{1, 2, 3},
         {4, 5, 6},
         {7, 8, 9}};

    vector<vector<double>> A2 =
        {{0.4, 3, 0, 0.2},
         {0.1, 17, 1, 0.1},
         {21, 0.3, 0.5, 0.1},
         {1, 1, 1, 0.1}};

    vector<vector<double>> A3 =
        {{2, -1, 0, 0},
         {-1, 2, -1, 0},
         {0, -1, 2, -1},
         {1, 0, -1, 2}};

    vector<vector<vector<double>>> matrix_list = {A1, A2, A3};

    cout << "-----------------------------------------------------------" << endl;
    std::vector<std::vector<double>> T = transpose_matrix(A1);
    cout << "Matrix A:" << endl;
    print_matrix(A1);
    cout << "Matrix T:" << endl;
    print_matrix(T);
    cout << "-----------------------------------------------------------" << endl;

    for (auto A : matrix_list)
    {
        auto [L, U, P] = lu_factorization_partial_pivot(A);

        cout << "Matrix A:" << endl;
        print_matrix(A);
        cout << "Matrix L:" << endl;
        print_matrix(L);
        cout << "Matrix U:" << endl;
        print_matrix(U);
        cout << "Matrix P:" << endl;
        print_matrix(P);

        vector<vector<double>> LU = matrix_multiply(L, U);
        cout << "Matrix LU:" << endl;
        print_matrix(LU);

        vector<vector<double>> PA = matrix_multiply(P, A);
        cout << "Matrix PA:" << endl;
        print_matrix(PA);

        vector<vector<double>> difference = subtract_matrices(PA, LU);
        double frobenius_norm_diff = frobenius_norm(difference);
        cout << ">>> Frobenius norm of the difference PA-LU: " << frobenius_norm_diff << endl;

        // Solve inear system Ax = 1
        vector<double> b(A.size(), 1.0);
        vector<double> x = solve_lu_system_by_substitution(L, U, P, b);
        for (size_t i = 0; i < x.size(); ++i)
            cout << "x[" << i << "]: " << x[i] << endl;

        // Residual
        auto a = multiply_matrix_vector(A, x);
        auto residual = subtract_vectors(a, b);
        auto norm = l2_norm(residual);
        cout << "Residual norm: " << norm << endl;
    }

    cout << "-----------------------------------------------------------" << endl;
    cout << "Testing matrix loading from CSV file" << endl;
    cout << "-----------------------------------------------------------" << endl;
    // Load matrix from CSV file
    const std::string &filename = "matrix.csv";
    vector<vector<double>> A = load_matrix_from_csv(filename);
    print_matrix(A);

    // Solve inear system Ax = 1
    vector<double> b(A.size(), 1.0);
    // vector<double> x = solve_system_with_LU(A, b);
    auto [L, U, P] = lu_factorization_partial_pivot(A);
    cout << "Matrix L:" << endl;
    print_matrix(L);
    save_matrix_to_csv(L, "L_matrix.csv");
    cout << "Matrix U:" << endl;
    print_matrix(U);
    save_matrix_to_csv(U, "U_matrix.csv");
    cout << "Matrix P:" << endl;
    print_matrix(P);
    save_matrix_to_csv(P, "P_matrix.csv");

    vector<double> x = solve_lu_system_by_substitution(L, U, P, b);
    for (size_t i = 0; i < x.size(); ++i)
        cout << "x[" << i << "]: " << x[i] << endl;

    // Residual
    auto a = multiply_matrix_vector(A, x);
    auto residual = subtract_vectors(a, b);
    auto norm = l2_norm(residual);
    cout << "Residual norm: " << norm << endl;

    cout << "-----------------------------------------------------------" << endl;
    cout << "Loading Bad U matrix" << endl;
    vector<vector<double>> U_loaded = load_matrix_from_csv("U_bad_matrix.csv");
    cout << "Matrix U (bad) loaded from CSV file:" << endl;
    x = solve_lu_system_by_substitution(L, U_loaded, P, b);
    for (size_t i = 0; i < x.size(); ++i)
        cout << "x[" << i << "]: " << x[i] << endl;

    cout << "\nReconstruct A" << endl;
    auto P_T = transpose_matrix(P);
    auto L_P_T = matrix_multiply(P_T, L);
    auto A_reconstructed = matrix_multiply(L_P_T, U_loaded);
    cout << "Matrix A reconstructed:" << endl;
    print_matrix(A_reconstructed);

    vector<vector<double>> difference = subtract_matrices(A, A_reconstructed);
    norm = frobenius_norm(difference);
    cout << ">>> Norm of (A - A_bad): " << norm << endl;

    cout << "Solve system with bad A" << endl;
    x = solve_system_with_LU(A_reconstructed, b);
    for (size_t i = 0; i < x.size(); ++i)
        cout << "x[" << i << "]: " << x[i] << endl;
    // Residual
    auto a_reconstructed = multiply_matrix_vector(A_reconstructed, x);
    auto residual_reconstructed = subtract_vectors(a_reconstructed, b);
    auto norm_reconstructed = l2_norm(residual_reconstructed);
    cout << "Residual norm: " << norm_reconstructed << endl;
    save_matrix_to_csv(A_reconstructed, "bad_matrix.csv");

    cout << "-----------------------------------------------------------" << endl;

    // cout << "Loading Bad L matrix" << endl;
    A = load_matrix_from_csv("bad_matrix.csv");
    print_matrix(A);

    // Solve linear system Ax = 1
    x = solve_system_with_LU(A, b);

    // Print solution
    cout << "Solution x:" << endl;
    for (size_t i = 0; i < x.size(); ++i)
        cout << "x[" << i << "]: " << x[i] << endl;
    cout << "-----------------------------------------------------------" << endl;

    // Residual
    auto ax = multiply_matrix_vector(A, x); // calculate Ax
    residual = subtract_vectors(ax, b);     // calculate Ax - b
    norm = l2_norm(residual);               // calculate ||Ax - b||
    cout << "Residual norm ||Ax - b||: " << norm << endl;

    return 0;
}