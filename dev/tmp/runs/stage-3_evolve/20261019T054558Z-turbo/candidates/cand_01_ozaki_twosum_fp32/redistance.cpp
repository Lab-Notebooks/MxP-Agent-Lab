// redistance.cpp -- level-set Eikonal redistancing in reduced precision (FP16 / FP32).

#include "redistance.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <immintrin.h>

// EVOLVE-BLOCK-START
float lsPseudoTimestepLowPrec(const Grid& g) {
    const float dx = g.dx_f();
    const float dy = g.dy_f();
    const float minCellDiag = std::sqrt(dx * dx + dy * dy);
    return minCellDiag / 5.0f;
}

float godunovGradMagLowPrec(const Grid& g, const std::vector<float>& phi, int i, int j) {
    const float dx = g.dx_f();
    const float dy = g.dy_f();
    const float sm = phi[g.index(i, j)];
    const float sxl = phi[g.index(i - 1, j)];
    const float sxr = phi[g.index(i + 1, j)];
    const float syl = phi[g.index(i, j - 1)];
    const float syr = phi[g.index(i, j + 1)];

    const float dx_l = (sm - sxl) / dx;
    const float dx_r = (sxr - sm) / dx;
    const float dy_l = (sm - syl) / dy;
    const float dy_r = (syr - sm) / dy;

    const float ap = (dx_l > 0.0f) ? dx_l : 0.0f;
    const float an = (dx_l < 0.0f) ? dx_l : 0.0f;
    const float bp = (dx_r > 0.0f) ? dx_r : 0.0f;
    const float bn = (dx_r < 0.0f) ? dx_r : 0.0f;
    const float cp = (dy_l > 0.0f) ? dy_l : 0.0f;
    const float cn = (dy_l < 0.0f) ? dy_l : 0.0f;
    const float dp = (dy_r > 0.0f) ? dy_r : 0.0f;
    const float dn = (dy_r < 0.0f) ? dy_r : 0.0f;

    if (sm > 0.0f) {
        const float gx2 = (ap * ap > bn * bn) ? (ap * ap) : (bn * bn);
        const float gy2 = (cp * cp > dn * dn) ? (cp * cp) : (dn * dn);
        return std::sqrt(gx2 + gy2);
    }
    if (sm < 0.0f) {
        const float gx2 = (an * an > bp * bp) ? (an * an) : (bp * bp);
        const float gy2 = (cn * cn > dp * dp) ? (cn * cn) : (dp * dp);
        return std::sqrt(gx2 + gy2);
    }
    return 1.0f;
}

long redistanceIterationLowPrec(const Grid& g, std::vector<float>& phi,
                                const std::vector<float>& phiOrig, float lsDT) {
    const std::vector<float> phio = phi;
    const float dx = g.dx_f();
    const float dy = g.dy_f();
    const float kEps = 1.0e-6f;

    long flips = 0;
    for (int j = 0; j < g.ny(); ++j) {
        for (int i = 0; i < g.nx(); ++i) {
            const int c = g.index(i, j);
            const float sm = phio[c];
            const float sxl = phio[g.index(i - 1, j)];
            const float sxr = phio[g.index(i + 1, j)];
            const float syl = phio[g.index(i, j - 1)];
            const float syr = phio[g.index(i, j + 1)];

            const float orig = phiOrig[c];
            const float sgn = orig / (std::fabs(orig) + kEps);

            float value;
            if (sm * sxl < 0.0f || sm * sxr < 0.0f || sm * syl < 0.0f || sm * syr < 0.0f) {
                value = orig;
            } else {
                const float dx_l = (sm - sxl) / dx;
                const float dx_r = (sxr - sm) / dx;
                const float dy_l = (sm - syl) / dy;
                const float dy_r = (syr - sm) / dy;

                const float ap = (dx_l > 0.0f) ? dx_l : 0.0f;
                const float an = (dx_l < 0.0f) ? dx_l : 0.0f;
                const float bp = (dx_r > 0.0f) ? dx_r : 0.0f;
                const float bn = (dx_r < 0.0f) ? dx_r : 0.0f;
                const float cp = (dy_l > 0.0f) ? dy_l : 0.0f;
                const float cn = (dy_l < 0.0f) ? dy_l : 0.0f;
                const float dp = (dy_r > 0.0f) ? dy_r : 0.0f;
                const float dn = (dy_r < 0.0f) ? dy_r : 0.0f;

                float agf;
                if (sm > 0.0f) {
                    const float gx2 = (ap * ap > bn * bn) ? (ap * ap) : (bn * bn);
                    const float gy2 = (cp * cp > dn * dn) ? (cp * cp) : (dn * dn);
                    agf = std::sqrt(gx2 + gy2) - 1.0f;
                } else if (sm < 0.0f) {
                    const float gx2 = (an * an > bp * bp) ? (an * an) : (bp * bp);
                    const float gy2 = (cn * cn > dp * dp) ? (cn * cn) : (dp * dp);
                    agf = std::sqrt(gx2 + gy2) - 1.0f;
                } else {
                    agf = 0.0f;
                }
                value = sm - lsDT * (sgn * agf);
            }

            phi[c] = value;
            const float s_val = (value < 0.0f) ? -1.0f : 1.0f;
            const float s_orig = (orig < 0.0f) ? -1.0f : 1.0f;
            if (s_val != s_orig) ++flips;
        }
    }
    return flips;
}

long redistanceLowPrec(const Grid& g, std::vector<float>& phi, std::vector<float>& phiOrig,
                       int iterations) {
    if (phi.size() != g.size()) phi.resize(g.size(), 0.0f);
    const float lsDT = lsPseudoTimestepLowPrec(g);

    long flips = 0;
    for (int it = 1; it <= iterations; ++it) {
        if (it == 1) phiOrig = phi;
        g.exchange_f(phi);
        flips = redistanceIterationLowPrec(g, phi, phiOrig, lsDT);
    }
    if (iterations > 0) g.exchange_f(phi);

    long global = 0;
    MPI_Allreduce(&flips, &global, 1, MPI_LONG, MPI_SUM, g.comm());
    return global;
}
// EVOLVE-BLOCK-END

double lsPseudoTimestep(const Grid& g) {
    return static_cast<double>(lsPseudoTimestepLowPrec(g));
}

double godunovGradMag(const Grid& g, const std::vector<double>& phi, int i, int j) {
    std::vector<float> phi_f(phi.size());
    for (std::size_t k = 0; k < phi.size(); ++k) {
        phi_f[k] = static_cast<float>(phi[k]);
    }
    return static_cast<double>(godunovGradMagLowPrec(g, phi_f, i, j));
}

long redistanceIteration(const Grid& g, std::vector<double>& phi,
                         const std::vector<double>& phiOrig, double lsDT) {
    std::vector<float> phi_f(phi.size());
    std::vector<float> orig_f(phiOrig.size());
    for (std::size_t k = 0; k < phi.size(); ++k) phi_f[k] = static_cast<float>(phi[k]);
    for (std::size_t k = 0; k < phiOrig.size(); ++k) orig_f[k] = static_cast<float>(phiOrig[k]);
    const long flips = redistanceIterationLowPrec(g, phi_f, orig_f, static_cast<float>(lsDT));
    for (std::size_t k = 0; k < phi.size(); ++k) phi[k] = static_cast<double>(phi_f[k]);
    return flips;
}

long redistance(const Grid& g, std::vector<double>& phi, std::vector<double>& phiOrig,
                int iterations) {
    std::vector<float> phi_f(phi.size());
    std::vector<float> orig_f(phiOrig.size());
    for (std::size_t k = 0; k < phi.size(); ++k) phi_f[k] = static_cast<float>(phi[k]);
    for (std::size_t k = 0; k < phiOrig.size(); ++k) orig_f[k] = static_cast<float>(phiOrig[k]);
    const long flips = redistanceLowPrec(g, phi_f, orig_f, iterations);
    phiOrig.resize(orig_f.size());
    for (std::size_t k = 0; k < phi.size(); ++k) phi[k] = static_cast<double>(phi_f[k]);
    for (std::size_t k = 0; k < orig_f.size(); ++k) phiOrig[k] = static_cast<double>(orig_f[k]);
    return flips;
}
