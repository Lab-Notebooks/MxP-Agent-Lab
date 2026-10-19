#include <fftw3.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    int N = 8; // Size of the transform
    fftw_complex *in, *out;
    fftw_plan plan_forward, plan_backward;

    // Allocate memory for the input and output arrays
    // FFTW requires dynamically allocated arrays for planning
    in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
    out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);

    if (in == NULL || out == NULL) {
        fprintf(stderr, "Error: Could not allocate memory for FFTW arrays.\n");
        return 1;
    }

    // Initialize the input array with some sample data
    // For simplicity, we'll use a simple impulse here
    for (int i = 0; i < N; ++i) {
        in[i][0] = (i == 0) ? 1.0 : 0.0; // Real part
        in[i][1] = 0.0;                 // Imaginary part
    }

    printf("Input array:\n");
    for (int i = 0; i < N; ++i) {
        printf("%3d: %g + %gi\n", i, in[i][0], in[i][1]);
    }

    // Create a plan for the forward DFT (complex-to-complex)
    // FFTW_ESTIMATE is a planning flag that is faster but may result in a suboptimal plan
    // For best performance, especially for repeated transforms of the same size, use FFTW_MEASURE
    plan_forward = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    // Execute the forward transform
    fftw_execute(plan_forward);

    printf("\nOutput array (Forward DFT):\n");
    for (int i = 0; i < N; ++i) {
        printf("%3d: %g + %gi\n", i, out[i][0], out[i][1]);
    }

    // Create a plan for the inverse DFT (complex-to-complex)
    // The inverse transform result will be scaled by N
    plan_backward = fftw_plan_dft_1d(N, out, in, FFTW_BACKWARD, FFTW_ESTIMATE);

    // Execute the inverse transform
    fftw_execute(plan_backward);

    printf("\nOutput array (Inverse DFT - scaled by N):\n");
    for (int i = 0; i < N; ++i) {
        printf("%3d: %g + %gi\n", i, in[i][0], in[i][1]);
    }

    // You would typically scale the result of the inverse transform by 1/N
    printf("\nOutput array (Inverse DFT - original scale):\n");
    for (int i = 0; i < N; ++i) {
        printf("%3d: %g + %gi\n", i, in[i][0] / N, in[i][1] / N);
    }


    // Clean up FFTW plans and allocated memory
    fftw_destroy_plan(plan_forward);
    fftw_destroy_plan(plan_backward);
    fftw_free(in);
    fftw_free(out);

    return 0;
}
