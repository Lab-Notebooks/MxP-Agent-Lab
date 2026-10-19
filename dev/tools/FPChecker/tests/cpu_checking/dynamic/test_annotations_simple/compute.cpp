#include <iostream>
#include <vector>

using namespace std;

#ifdef FUNC_1
__attribute__((annotate("_two_"))) FPC_INSTRUMENT_FUNC __attribute__((annotate("_one_")))
#endif
vector<vector<double>>
matrix_multiply(const vector<vector<double>> &A, const vector<vector<double>> &B)
{
    int rows_A = A.size();
    int cols_A = A[0].size();
    int rows_B = B.size();
    int cols_B = B[0].size();

    vector<vector<double>> result(rows_A, vector<double>(cols_B, 0.0));

    for (int i = 0; i < rows_A; ++i)
    {
        for (int j = 0; j < cols_B; ++j)
        {
            for (int k = 0; k < cols_A; ++k)
            {
#ifdef BLOCK_1
                FPC_INSTRUMENT_BLOCK;
#endif
                result[i][j] += A[i][k] * B[k][j];
            }
        }
    }

    return result;
}

// Subtract two matrices
#ifdef FUNC_2
FPC_INSTRUMENT_FUNC
#endif
vector<vector<double>>
subtract_matrices(const vector<vector<double>> &A, const vector<vector<double>> &B)
{
    int rows_A = A.size();
    int cols_A = (rows_A > 0) ? A[0].size() : 0;
    int rows_B = B.size();
    int cols_B = (rows_B > 0) ? B[0].size() : 0;

    vector<vector<double>> result(rows_A, vector<double>(cols_A));
    for (int i = 0; i < rows_A; ++i)
    {
        for (int j = 0; j < cols_A; ++j)
        {
#ifdef BLOCK_2
            FPC_INSTRUMENT_BLOCK;
#endif
            result[i][j] = A[i][j] - B[i][j];
        }
    }

#ifdef BLOCK_3
    FPC_INSTRUMENT_BLOCK;
#endif
    // Exception
    result[0][0] = (result[0][0] - result[0][0]) / (result[0][0] - result[0][0]);

    return result;
}