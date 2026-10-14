// advect.cpp -- WENO5 advection kernel in reduced precision (FP16 / FP32).

#include "advect.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <immintrin.h>

// EVOLVE-BLOCK-START
float wenoFaceValueLowPrec(float s1, float s2, float s3, float s4, float s5, bool positive) {
    // Centered differences relative to center value s3
    const float v1 = s1 - s3;
    const float v2 = s2 - s3;
    const float v4 = s4 - s3;
    const float v5 = s5 - s3;

    const float d1a = v1 - 2.0f * v2;
    const float d1b = v1 - 4.0f * v2;
    const float d2a = v2 + v4;
    const float d2b = v2 - v4;
    const float d3a = -2.0f * v4 + v5;
    const float d3b = -4.0f * v4 + v5;

    const float rIS1 = (13.0f / 12.0f) * (d1a * d1a) + 0.25f * (d1b * d1b);
    const float rIS2 = (13.0f / 12.0f) * (d2a * d2a) + 0.25f * (d2b * d2b);
    const float rIS3 = (13.0f / 12.0f) * (d3a * d3a) + 0.25f * (d3b * d3b);

    const float kEps = 1.0e-6f;
    const float d1 = positive ? 0.1f : 0.3f;
    const float d2 = 0.6f;
    const float d3 = positive ? 0.3f : 0.1f;

    const float denom1 = kEps + rIS1;
    const float denom2 = kEps + rIS2;
    const float denom3 = kEps + rIS3;

    const float aT1 = d1 / (denom1 * denom1);
    const float aT2 = d2 / (denom2 * denom2);
    const float aT3 = d3 / (denom3 * denom3);

    const float invSum = 1.0f / (aT1 + aT2 + aT3);
    const float a1 = aT1 * invSum;
    const float a2 = aT2 * invSum;
    const float a3 = aT3 * invSum;

    float dfT1;
    float dfT2;
    float dfT3;
    if (positive) {
        dfT1 = (2.0f * v1 - 7.0f * v2) / 6.0f;
        dfT2 = (-v2 + 2.0f * v4) / 6.0f;
        dfT3 = (5.0f * v4 - v5) / 6.0f;
    } else {
        dfT1 = (-v1 + 5.0f * v2) / 6.0f;
        dfT2 = (2.0f * v2 - v4) / 6.0f;
        dfT3 = (-7.0f * v4 + 2.0f * v5) / 6.0f;
    }

    return s3 + (a1 * dfT1 + a2 * dfT2 + a3 * dfT3);
}

void advectWenoLowPrec(const Grid& g, const std::vector<float>& phi, const std::vector<float>& u,
                       const std::vector<float>& v, std::vector<float>& rhs) {
    if (rhs.size() != g.size()) rhs.assign(g.size(), 0.0f);

    const int nx = g.nx();
    const int ny = g.ny();
    const float dxi = 1.0f / g.dx_f();
    const float dyi = 1.0f / g.dy_f();

    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            const float ul = u[g.index(i, j)];
            const float ur = u[g.index(i + 1, j)];
            const float vl = v[g.index(i, j)];
            const float vr = v[g.index(i, j + 1)];

            float flx;
            float frx;
            if (ur > 0.0f) {
                frx = wenoFaceValueLowPrec(phi[g.index(i - 2, j)], phi[g.index(i - 1, j)],
                                           phi[g.index(i, j)], phi[g.index(i + 1, j)],
                                           phi[g.index(i + 2, j)], true);
            } else {
                frx = wenoFaceValueLowPrec(phi[g.index(i - 1, j)], phi[g.index(i, j)],
                                           phi[g.index(i + 1, j)], phi[g.index(i + 2, j)],
                                           phi[g.index(i + 3, j)], false);
            }
            if (ul > 0.0f) {
                flx = wenoFaceValueLowPrec(phi[g.index(i - 3, j)], phi[g.index(i - 2, j)],
                                           phi[g.index(i - 1, j)], phi[g.index(i, j)],
                                           phi[g.index(i + 1, j)], true);
            } else {
                flx = wenoFaceValueLowPrec(phi[g.index(i - 2, j)], phi[g.index(i - 1, j)],
                                           phi[g.index(i, j)], phi[g.index(i + 1, j)],
                                           phi[g.index(i + 2, j)], false);
            }

            float fly;
            float fry;
            if (vr > 0.0f) {
                fry = wenoFaceValueLowPrec(phi[g.index(i, j - 2)], phi[g.index(i, j - 1)],
                                           phi[g.index(i, j)], phi[g.index(i, j + 1)],
                                           phi[g.index(i, j + 2)], true);
            } else {
                fry = wenoFaceValueLowPrec(phi[g.index(i, j - 1)], phi[g.index(i, j)],
                                           phi[g.index(i, j + 1)], phi[g.index(i, j + 2)],
                                           phi[g.index(i, j + 3)], false);
            }
            if (vl > 0.0f) {
                fly = wenoFaceValueLowPrec(phi[g.index(i, j - 3)], phi[g.index(i, j - 2)],
                                           phi[g.index(i, j - 1)], phi[g.index(i, j)],
                                           phi[g.index(i, j + 1)], true);
            } else {
                fly = wenoFaceValueLowPrec(phi[g.index(i, j - 2)], phi[g.index(i, j - 1)],
                                           phi[g.index(i, j)], phi[g.index(i, j + 1)],
                                           phi[g.index(i, j + 2)], false);
            }

            rhs[g.index(i, j)] += -(frx * ur - flx * ul) * dxi - (fry * vr - fly * vl) * dyi;
        }
    }
}
// EVOLVE-BLOCK-END

double wenoFaceValue(double s1, double s2, double s3, double s4, double s5, bool positive) {
    return static_cast<double>(wenoFaceValueLowPrec(
        static_cast<float>(s1), static_cast<float>(s2), static_cast<float>(s3),
        static_cast<float>(s4), static_cast<float>(s5), positive));
}

void advectWeno(const Grid& g, const std::vector<double>& phi, const std::vector<double>& u,
                const std::vector<double>& v, std::vector<double>& rhs) {
    std::vector<float> phi_f(phi.size());
    std::vector<float> u_f(u.size());
    std::vector<float> v_f(v.size());
    std::vector<float> rhs_f(g.size(), 0.0f);
    for (std::size_t k = 0; k < phi.size(); ++k) {
        phi_f[k] = static_cast<float>(phi[k]);
        u_f[k] = static_cast<float>(u[k]);
        v_f[k] = static_cast<float>(v[k]);
    }
    if (rhs.size() == g.size()) {
        for (std::size_t k = 0; k < rhs.size(); ++k) {
            rhs_f[k] = static_cast<float>(rhs[k]);
        }
    }
    advectWenoLowPrec(g, phi_f, u_f, v_f, rhs_f);
    rhs.resize(rhs_f.size());
    for (std::size_t k = 0; k < rhs_f.size(); ++k) {
        rhs[k] = static_cast<double>(rhs_f[k]);
    }
}
