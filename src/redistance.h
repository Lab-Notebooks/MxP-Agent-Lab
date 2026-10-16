// redistance.h -- level-set redistancing interface.
#ifndef BUBBLE_REDISTANCE_H
#define BUBBLE_REDISTANCE_H

#include <vector>

#include "grid.h"

double lsPseudoTimestep(const Grid& g);
double godunovGradMag(const Grid& g, const std::vector<double>& phi, int i, int j);
long redistanceIteration(const Grid& g, std::vector<double>& phi,
                         const std::vector<double>& phiOrig, double lsDT);
long redistance(const Grid& g, std::vector<double>& phi, std::vector<double>& phiOrig,
                int iterations);

// Low-precision (FP16/FP32) redistancing kernels evolved by AlphaEvolve
float lsPseudoTimestepLowPrec(const Grid& g);
float godunovGradMagLowPrec(const Grid& g, const std::vector<float>& phi, int i, int j);
long redistanceIterationLowPrec(const Grid& g, std::vector<float>& phi,
                                const std::vector<float>& phiOrig, float lsDT);
long redistanceLowPrec(const Grid& g, std::vector<float>& phi, std::vector<float>& phiOrig,
                       int iterations);

#endif  // BUBBLE_REDISTANCE_H
