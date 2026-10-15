// advect.h -- WENO5 advection kernel interface.
#ifndef BUBBLE_ADVECT_H
#define BUBBLE_ADVECT_H

#include <vector>

#include "grid.h"

double wenoFaceValue(double s1, double s2, double s3, double s4, double s5, bool positive);
void advectWeno(const Grid& g, const std::vector<double>& phi, const std::vector<double>& u,
                const std::vector<double>& v, std::vector<double>& rhs);

// Low-precision (FP16/FP32) WENO5 kernels evolved by AlphaEvolve
float wenoFaceValueLowPrec(float s1, float s2, float s3, float s4, float s5, bool positive);
void advectWenoLowPrec(const Grid& g, const std::vector<float>& phi, const std::vector<float>& u,
                       const std::vector<float>& v, std::vector<float>& rhs);

#endif  // BUBBLE_ADVECT_H
