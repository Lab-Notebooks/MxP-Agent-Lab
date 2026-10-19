// init.cpp -- initial condition and prescribed MAC face-velocity field.

#include "init.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <immintrin.h>
#include <vector>

// EVOLVE-BLOCK-START
void computeFaceVelocityLowPrec(const Grid& g, bool reverse_time, float tmax_f, float time_f,
                                std::vector<float>& u, std::vector<float>& v) {
    if (u.size() != g.size()) u.assign(g.size(), 0.0f);
    if (v.size() != g.size()) v.assign(g.size(), 0.0f);

    constexpr float kPiF = 3.14159265f;
    const float dx = g.dx_f();
    const float dy = g.dy_f();
    const float scale = reverse_time ? std::cos(kPiF * time_f / tmax_f) : 1.0f;
    const int ng = g.ng();

    for (int j = -ng; j < g.ny() + ng; ++j) {
        const float yc = g.yCenter_f(j);
        const float dcos = -2.0f * std::sin(2.0f * kPiF * yc) * std::sin(kPiF * dy);
        for (int i = -ng; i < g.nx() + ng; ++i) {
            const float xf = g.xFace_f(i);
            const float s = std::sin(kPiF * xf);
            u[g.index(i, j)] = scale * (-(s * s) * dcos / (2.0f * kPiF * dy));
        }
    }

    for (int j = -ng; j < g.ny() + ng; ++j) {
        const float yf = g.yFace_f(j);
        const float s = std::sin(kPiF * yf);
        const float s2 = s * s;
        for (int i = -ng; i < g.nx() + ng; ++i) {
            const float xc = g.xCenter_f(i);
            const float dcos = -2.0f * std::sin(2.0f * kPiF * xc) * std::sin(kPiF * dx);
            v[g.index(i, j)] = scale * (s2 * dcos / (2.0f * kPiF * dx));
        }
    }
}

void maxFaceSpeedsLowPrec(const Grid& g, const std::vector<float>& u, const std::vector<float>& v,
                          float& umax, float& vmax) {
    float lu = 0.0f;
    float lv = 0.0f;
    for (int j = 0; j < g.ny(); ++j) {
        for (int i = 0; i <= g.nx(); ++i) {
            const float a = std::fabs(u[g.index(i, j)]);
            if (a > lu) lu = a;
        }
    }
    for (int j = 0; j <= g.ny(); ++j) {
        for (int i = 0; i < g.nx(); ++i) {
            const float a = std::fabs(v[g.index(i, j)]);
            if (a > lv) lv = a;
        }
    }
    float in[2] = {lu, lv};
    float out[2] = {0.0f, 0.0f};
    MPI_Allreduce(in, out, 2, MPI_FLOAT, MPI_MAX, g.comm());
    umax = out[0];
    vmax = out[1];
}
// EVOLVE-BLOCK-END

double levelSetAnalytic(const Params& p, double x, double y) {
    const double rx = x - p.bubble_x;
    const double ry = y - p.bubble_y;
    return p.bubble_r - std::sqrt(rx * rx + ry * ry);
}

void initLevelSet(const Grid& g, const Params& p, std::vector<double>& dfun) {
    if (dfun.size() != g.size()) dfun.assign(g.size(), 0.0);

    for (int j = 0; j < g.ny(); ++j) {
        const double y = g.yCenter(j);
        for (int i = 0; i < g.nx(); ++i) {
            dfun[g.index(i, j)] = levelSetAnalytic(p, g.xCenter(i), y);
        }
    }
    g.exchange(dfun);
}

void computeFaceVelocity(const Grid& g, const Params& p, double time, std::vector<double>& u,
                         std::vector<double>& v) {
    std::vector<float> uf;
    std::vector<float> vf;
    computeFaceVelocityLowPrec(g, p.reverse_time, static_cast<float>(p.tmax),
                               static_cast<float>(time), uf, vf);
    u.resize(uf.size());
    v.resize(vf.size());
    for (std::size_t k = 0; k < uf.size(); ++k) {
        u[k] = static_cast<double>(uf[k]);
        v[k] = static_cast<double>(vf[k]);
    }
}

double maxDivergence(const Grid& g, const std::vector<double>& u, const std::vector<double>& v) {
    double local = 0.0;
    for (int j = 0; j < g.ny(); ++j) {
        for (int i = 0; i < g.nx(); ++i) {
            const double div = (u[g.index(i + 1, j)] - u[g.index(i, j)]) / g.dx() +
                               (v[g.index(i, j + 1)] - v[g.index(i, j)]) / g.dy();
            const double a = std::fabs(div);
            if (a > local) local = a;
        }
    }
    double global = 0.0;
    MPI_Allreduce(&local, &global, 1, MPI_DOUBLE, MPI_MAX, g.comm());
    return global;
}

void maxFaceSpeeds(const Grid& g, const std::vector<double>& u, const std::vector<double>& v,
                   double& umax, double& vmax) {
    std::vector<float> uf(u.size());
    std::vector<float> vf(v.size());
    for (std::size_t k = 0; k < u.size(); ++k) {
        uf[k] = static_cast<float>(u[k]);
        vf[k] = static_cast<float>(v[k]);
    }
    float umax_f = 0.0f;
    float vmax_f = 0.0f;
    maxFaceSpeedsLowPrec(g, uf, vf, umax_f, vmax_f);
    umax = static_cast<double>(umax_f);
    vmax = static_cast<double>(vmax_f);
}
