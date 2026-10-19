#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

//SuperLU
#include "slu_ddefs.h"

#define MAX_LEN 5000

int loadMatrix(SuperMatrix *A, SuperMatrix *B, int *m, int *n, char **argv)
{
    const char* s = argv[1];

    // Handle error
    if (s==NULL) {
        printf("Matrix file was not found");
        exit(-1);
    }

    // ---- Get sizeof matrix --------------
    printf("Matrix %s\n", s);
    FILE *file;
    file = fopen(s, "r");
    if (file == NULL) {
        printf("Error opening matrix file\n");
        exit(-1);
    }
    char data[MAX_LEN];
    data[0]='\0';
    double (*arr)[MAX_LEN] = (double (*)[MAX_LEN])malloc(MAX_LEN * MAX_LEN * sizeof(double));
    
    int line=0;
    int cols = 0, rows = 0;
    int nnz = 0;
    while (!feof(file)&& !ferror(file)) {
        if (fgets(data, MAX_LEN, file) != NULL) {
            // Get columns
            char *pt;
            pt = strtok (data,",");
            cols = 0;
            while (pt != NULL) {
                double f = atof(pt);
                //printf("%f ", f);
                arr[line][cols] = f;
                if (f != 0.0)
                    nnz++;
                pt = strtok (NULL, ",");
                cols++;
            }
            data[0]='\0';
            line++;
        }
    }
    fclose(file);
    printf("Rows %d, cols %d\n", line, cols);
    printf("nnz = %d\n", nnz);

    // Iterate on data
    double sum = 0.0;
    for (int i=0; i < line; ++i) {
        for (int j=0; j < cols; ++j) {
            printf("%f | ", arr[i][j]);
            sum += arr[i][j];
        }
        printf("\n");
    }

    if (line != cols) {
        printf("Not square matrix\n");
        exit(0);
    }
    rows = cols;

    // Check all zero matrices
    if (sum == 0.0) {
        printf("All-zero matrix\n");
        exit(0);
    }

    // ================ Create Matrix A =========================
    double *a;
    int_t *asub;
    int_t *xa;
    if ( !(a = doubleMalloc(nnz)) ) ABORT("Malloc fails for a[].");
    if ( !(asub = intMalloc(nnz)) ) ABORT("Malloc fails for asub[].");
    if ( !(xa = intMalloc(cols+1)) ) ABORT("Malloc fails for xa[].");

    int_t current_nnz = 0;
    for (int i = 0; i < cols; ++i) {
        int col_flag = 0;
        for (int j = 0; j < rows; ++j) {
            if (arr[j][i] != 0.0) {
                a[current_nnz] = arr[j][i];
                asub[current_nnz] = j;
                if (col_flag == 0) {
                    xa[i] = current_nnz;
                    col_flag = 1;
                }
                current_nnz++;
            }
        }
    }
    xa[cols] = nnz;
    
    dCreate_CompCol_Matrix(A, rows, cols, nnz, 
                           a, asub, xa, SLU_NC, SLU_D, SLU_GE);

    // ================ Create b ===========================
    /* Create right-hand side matrix B. */
    int nrhs = 1;
    double   *rhs;
    if ( !(rhs = doubleMalloc(cols * nrhs)) ) ABORT("Malloc fails for rhs[].");
    for (int i = 0; i < cols; ++i) {
        rhs[i] = (i+1)/100.0;
        //rhs[i] = 1.0;
    }
    dCreate_Dense_Matrix(B, cols, nrhs, rhs, cols, SLU_DN, SLU_D, SLU_GE);
    
    *m = cols;
    *n = cols;

    free(arr);
    return 1;
}

int main(int argc, char *argv[])
{    
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <matrix.csv>\n", argv[0]);
        exit(1);
    }
    
    SuperMatrix A, L, U, B;
    //double   *a;
    //double   s, u, p, e, r, l;
    //int_t    *asub, *xa;
    int      *perm_r; /* row permutations from partial pivoting */
    int      *perm_c; /* column permutation vector */
    int      m, n;
    int_t    info, nnz;
    superlu_options_t options;
    SuperLUStat_t stat;
    
    loadMatrix(&A, &B, &m, &n, argv);
    
    if ( !(perm_r = int32Malloc(m)) ) ABORT("Malloc fails for perm_r[].");
    if ( !(perm_c = int32Malloc(n)) ) ABORT("Malloc fails for perm_c[].");
    
    /* Set the default input options. */
    set_default_options(&options);
    options.ColPerm = NATURAL;
    
    // ------- Set sub-options -----------------------
    options.SymmetricMode = YES;
    options.DiagPivotThresh = 0.5;
    options.Trans = TRANS;
    options.Equil = NO;
    // -----------------------------------------------
    
    /* Initialize the statistics variables. */
    StatInit(&stat);
    
    /* Solve the linear system. */
    dgssv(&options, &A, perm_c, perm_r, &L, &U, &B, &stat, &info);
    
    dPrint_CompCol_Matrix("A", &A);
    dPrint_CompCol_Matrix("U", &U);
    dPrint_SuperNode_Matrix("L", &L);
    print_int_vec("\nperm_r", m, perm_r);
    printf("\n*** Solution:\n");
    dPrint_Dense_Matrix("B", &B);
    
    /* De-allocate storage */
    //SUPERLU_FREE (rhs);
    SUPERLU_FREE (perm_r);
    SUPERLU_FREE (perm_c);
    Destroy_CompCol_Matrix(&A);
    Destroy_SuperMatrix_Store(&B);
    Destroy_SuperNode_Matrix(&L);
    Destroy_CompCol_Matrix(&U);
    StatFree(&stat);

    return EXIT_SUCCESS;
}

