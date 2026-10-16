// init.h -- initial condition and prescribed face-velocity field.
#ifndef BUBBLE_INIT_H
#define BUBBLE_INIT_H

#include <vector>

#include "grid.h"
#include "params.h"

void initLevelSet(const Grid& g, const Params& p, std::vector<double>& dfun);
double levelSetAnalytic(const Params& p, double x, double y);
void computeFaceVelocity(const Grid& g, const Params& p, double time, std::vector<double>& u,
                         std::vector<double>& v);
double maxDivergence(const Grid& g, const std::vector<double>& u, const std::vector<double>& v);
void maxFaceSpeeds(const Grid& g, const std::vector<double>& u, const std::vector<double>& v,
                   double& umax, double& vmax);

// Low-precision (FP16/FP32) entry points evolved by AlphaEvolve
void computeFaceVelocityLowPrec(const Grid& g, bool reverse_time, float tmax_f, float time_f,
                                std::vector<float>& u, std::vector<float>& v);
void maxFaceSpeedsLowPrec(const Grid& g, const std::vector<float>& u, const std::vector<float>& v,
                          float& umax, float& vmax);

#endif  // BUBBLE_INIT_H
