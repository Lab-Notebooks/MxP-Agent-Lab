#include <iostream>
#include <vector>

#include "compute.h"

using namespace std;

int main()
{
    vector<vector<double>> A = {{1, 2}, {3, 4}};
    vector<vector<double>> B = {{5, 6}, {7, 8}};
    vector<vector<double>> C = matrix_multiply(A, B);

    for (const auto &row : C)
    {
        for (const auto &elem : row)
        {
            cout << elem << " ";
        }
        cout << endl;
    }

    auto D = subtract_matrices(A, C);

    cout << "Subtraction result:" << endl;
    for (const auto &row : D)
    {
        for (const auto &elem : row)
        {
            cout << elem << " ";
        }
        cout << endl;
    }

    return 0;
}