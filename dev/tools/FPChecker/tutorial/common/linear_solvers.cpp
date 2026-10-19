#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <numeric>

#include "linear_solvers.hpp"
#include "blas.hpp"

using namespace std;

/***********************************************/
/* FP64 API                                    */
/***********************************************/

// -----------------------------------------------------------
// LU factorization with partial pivoting
// -----------------------------------------------------------

tuple<vector<vector<double>>, vector<vector<double>>, vector<vector<double>>>
lu_factorization_partial_pivot(vector<vector<double>> A)
{
    int m = A.size();
    if (m == 0)
    {
        return make_tuple(vector<vector<double>>(), vector<vector<double>>(), vector<vector<double>>());
    }
    int n = A[0].size();
    if (m != n)
    {
        cerr << "Error: Input matrix must be square for LU factorization." << endl;
        return make_tuple(vector<vector<double>>(), vector<vector<double>>(), vector<vector<double>>());
    }

    vector<vector<double>> U = A;
    vector<vector<double>> L(m, vector<double>(m, 0.0));
    vector<vector<double>> P(m, vector<double>(m, 0.0));

    // Initialize L and P as identity matrices
    for (int i = 0; i < m; ++i)
    {
        L[i][i] = 1.0;
        P[i][i] = 1.0;
    }

    /*printf("Initialized L and P:\n");
    print_matrix(L);
    print_matrix(P);*/

    for (int k = 0; k < m - 1; ++k)
    {
        // Find the row with the maximum absolute value in the k-th column (from row k downwards)
        int pivot_row = k;
        for (int i = k + 1; i < m; ++i)
        {
            if (abs(U[i][k]) > abs(U[pivot_row][k]))
            {
                pivot_row = i;
            }
        }

        // printf("k=%d, pivot_row=%d\n", k, pivot_row);

        // Swap rows if a better pivot is found
        if (pivot_row != k)
        {
            if (k <= m)
                swap(U[k], U[pivot_row]);
            if ((k - 1) >= 0)
            {
                // swap(L[k], L[pivot_row]);
                for (int j = 0; j <= (k - 1); ++j)
                {
                    swap(L[k][j], L[pivot_row][j]);
                }
            }

            swap(P[k], P[pivot_row]);
        }

        /*printf("\n ===== Swapping =====\n");
        printf("U:\n");
        print_matrix(U);
        printf("L:\n");
        print_matrix(L);
        printf("P:\n");
        print_matrix(P);*/

        // Perform elimination
        for (int j = k + 1; j < m; ++j)
        {
            /*if (U[k][k] == 0)
            {
                cerr << "Error: Pivot element is zero. LU factorization may not be possible." << endl;
                return make_tuple(vector<vector<double>>(), vector<vector<double>>(), vector<vector<double>>());
            }*/
            L[j][k] = U[j][k] / U[k][k];
            for (int l = k; l < m; ++l)
            {
                U[j][l] = U[j][l] - L[j][k] * U[k][l];
                if (fabs(U[j][l]) < (10 * DBL_EPSILON))
                    U[j][l] = 0.0; // Set value to zero if close to machine epsilon
            }
        }
    }

    return make_tuple(L, U, P);
}

// -----------------------------------------------------------
// Solve Ax = b with LU factorization
// -----------------------------------------------------------

// Apply the permutation matrix P to a vector b
vector<double> apply_permutation(const vector<vector<double>> &P, const vector<double> &b)
{
    int m = P.size();
    vector<double> b_prime(m);
    for (int i = 0; i < m; ++i)
    {
        for (int j = 0; j < m; ++j)
        {
            b_prime[i] += P[i][j] * b[j];
        }
    }
    return b_prime;
}

// Solve Ly = b using forward substitution
vector<double> forward_substitution(const vector<vector<double>> &L, const vector<double> &b_prime)
{
    int m = L.size();
    vector<double> y(m);
    for (int i = 0; i < m; ++i)
    {
        double sum = 0.0;
        for (int j = 0; j < i; ++j)
        {
            sum += L[i][j] * y[j];
        }
        y[i] = b_prime[i] - sum; // Since L has 1s on the diagonal
    }
    return y;
}

// Solve Ux = y using backward substitution
vector<double> backward_substitution(const vector<vector<double>> &U, const vector<double> &y)
{
    int m = U.size();
    vector<double> x(m);
    for (int i = m - 1; i >= 0; --i)
    {
        double sum = 0.0;
        for (int j = i + 1; j < m; ++j)
        {
            sum += U[i][j] * x[j];
        }
        // if (U[i][i] == 0)
        //{
        //     cerr << "Error: Zero pivot in U. System may not have a unique solution." << endl;
        //     return {};
        // }
        x[i] = (y[i] - sum) / U[i][i];
    }
    return x;
}

// Solve Ax = b given PA = LU
vector<double> solve_lu_system_by_substitution(const vector<vector<double>> &L,
                                               const vector<vector<double>> &U,
                                               const vector<vector<double>> &P,
                                               const vector<double> &b)
{
    // 1. Permute the right-hand side: b' = Pb
    vector<double> b_prime = apply_permutation(P, b);

    // 2. Solve Ly = b' using forward substitution
    vector<double> y = forward_substitution(L, b_prime);

    // 3. Solve Ux = y using backward substitution
    vector<double> x = backward_substitution(U, y);

    return x;
}

vector<double> solve_system_with_LU(const vector<vector<double>> &A, const vector<double> &b)
{
    // Perform LU factorization with partial pivoting
    auto [L, U, P] = lu_factorization_partial_pivot(A);

    // Solve the system using forward and backward substitution
    vector<double> x = solve_lu_system_by_substitution(L, U, P, b);

    return x;
}

/***********************************************/
/* FP32 API                                    */
/***********************************************/

tuple<vector<vector<float>>, vector<vector<float>>, vector<vector<float>>>
lu_factorization_partial_pivot(vector<vector<float>> A)
{
    int m = A.size();
    if (m == 0)
    {
        return make_tuple(vector<vector<float>>(), vector<vector<float>>(), vector<vector<float>>());
    }
    int n = A[0].size();
    if (m != n)
    {
        cerr << "Error: Input matrix must be square for LU factorization." << endl;
        return make_tuple(vector<vector<float>>(), vector<vector<float>>(), vector<vector<float>>());
    }

    vector<vector<float>> U = A;
    vector<vector<float>> L(m, vector<float>(m, 0.0));
    vector<vector<float>> P(m, vector<float>(m, 0.0));

    // Initialize L and P as identity matrices
    for (int i = 0; i < m; ++i)
    {
        L[i][i] = 1.0;
        P[i][i] = 1.0;
    }

    /*printf("Initialized L and P:\n");
    print_matrix(L);
    print_matrix(P);*/

    for (int k = 0; k < m - 1; ++k)
    {
        // Find the row with the maximum absolute value in the k-th column (from row k downwards)
        int pivot_row = k;
        for (int i = k + 1; i < m; ++i)
        {
            if (abs(U[i][k]) > abs(U[pivot_row][k]))
            {
                pivot_row = i;
            }
        }

        // printf("k=%d, pivot_row=%d\n", k, pivot_row);

        // Swap rows if a better pivot is found
        if (pivot_row != k)
        {
            if (k <= m)
                swap(U[k], U[pivot_row]);
            if ((k - 1) >= 0)
            {
                // swap(L[k], L[pivot_row]);
                for (int j = 0; j <= (k - 1); ++j)
                {
                    swap(L[k][j], L[pivot_row][j]);
                }
            }

            swap(P[k], P[pivot_row]);
        }

        /*printf("\n ===== Swapping =====\n");
        printf("U:\n");
        print_matrix(U);
        printf("L:\n");
        print_matrix(L);
        printf("P:\n");
        print_matrix(P);*/

        // Perform elimination
        for (int j = k + 1; j < m; ++j)
        {
            /*if (U[k][k] == 0)
            {
                cerr << "Error: Pivot element is zero. LU factorization may not be possible." << endl;
                return make_tuple(vector<vector<float>>(), vector<vector<float>>(), vector<vector<float>>());
            }*/
            L[j][k] = U[j][k] / U[k][k];
            for (int l = k; l < m; ++l)
            {
                U[j][l] = U[j][l] - L[j][k] * U[k][l];
                if (fabs(U[j][l]) < (10 * DBL_EPSILON))
                    U[j][l] = 0.0; // Set value to zero if close to machine epsilon
            }
        }
    }

    return make_tuple(L, U, P);
}

// -----------------------------------------------------------
// Solve Ax = b with LU factorization
// -----------------------------------------------------------

// Apply the permutation matrix P to a vector b
vector<float> apply_permutation(const vector<vector<float>> &P, const vector<float> &b)
{
    int m = P.size();
    vector<float> b_prime(m);
    for (int i = 0; i < m; ++i)
    {
        for (int j = 0; j < m; ++j)
        {
            b_prime[i] += P[i][j] * b[j];
        }
    }
    return b_prime;
}

// Solve Ly = b using forward substitution
vector<float> forward_substitution(const vector<vector<float>> &L, const vector<float> &b_prime)
{
    int m = L.size();
    vector<float> y(m);
    for (int i = 0; i < m; ++i)
    {
        float sum = 0.0;
        for (int j = 0; j < i; ++j)
        {
            sum += L[i][j] * y[j];
        }
        y[i] = b_prime[i] - sum; // Since L has 1s on the diagonal
    }
    return y;
}

// Solve Ux = y using backward substitution
vector<float> backward_substitution(const vector<vector<float>> &U, const vector<float> &y)
{
    int m = U.size();
    vector<float> x(m);
    for (int i = m - 1; i >= 0; --i)
    {
        float sum = 0.0;
        for (int j = i + 1; j < m; ++j)
        {
            sum += U[i][j] * x[j];
        }
        // if (U[i][i] == 0)
        //{
        //     cerr << "Error: Zero pivot in U. System may not have a unique solution." << endl;
        //     return {};
        // }
        x[i] = (y[i] - sum) / U[i][i];
    }
    return x;
}

// Solve Ax = b given PA = LU
vector<float> solve_lu_system_by_substitution(const vector<vector<float>> &L,
                                              const vector<vector<float>> &U,
                                              const vector<vector<float>> &P,
                                              const vector<float> &b)
{
    // 1. Permute the right-hand side: b' = Pb
    vector<float> b_prime = apply_permutation(P, b);

    // 2. Solve Ly = b' using forward substitution
    vector<float> y = forward_substitution(L, b_prime);

    // 3. Solve Ux = y using backward substitution
    vector<float> x = backward_substitution(U, y);

    return x;
}

vector<float> solve_system_with_LU(const vector<vector<float>> &A, const vector<float> &b)
{
    // Perform LU factorization with partial pivoting
    auto [L, U, P] = lu_factorization_partial_pivot(A);

    // Solve the system using forward and backward substitution
    vector<float> x = solve_lu_system_by_substitution(L, U, P, b);

    return x;
}