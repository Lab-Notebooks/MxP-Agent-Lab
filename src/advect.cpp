// advect.cpp -- WENO5 advection kernel, ported from
//   Flash-X-Development/Flash-X/source/numericalTools/Stencils/
//     StencilsAdvection/Stencils_advectWeno.F90
//
// The Fortran is written out twice per direction (a "r" high-face block and an
// "l" low-face block) and twice again for the sign of the face velocity, four
// copies of the same algebra per direction.  The only differences are the
// stencil shift and the linear weights, so the algebra lives once in
// wenoFaceValue() here.  The correspondence, face m being the *low* face of
// cell m (MAC convention of the Spec, face(center) = 1 in the Fortran call):
//
//   velocity > 0 ("downwind" branch in the Fortran comments):
//       s1..s5 = phi(m-3), phi(m-2), phi(m-1), phi(m), phi(m+1)
//   velocity <= 0 ("upwind" branch):
//       s1..s5 = phi(m-2), phi(m-1), phi(m), phi(m+1), phi(m+2)
//
// Check against the Fortran: the high face of cell i is m = i+1, so the
// positive branch reads phi(i-2..i+2) and the negative branch phi(i-1..i+3),
// which is exactly lines 75-79 and 102-106 of the Fortran; the low face is
// m = i, giving phi(i-3..i+1) and phi(i-2..i+2), lines 131-135 and 158-162.
//
// Coefficients are transcribed verbatim: 13/12 and 1/4 in the smoothness
// indicators, eps = 1e-15, exponent 2 on (eps + rIS), linear weights
// (1/10, 6/10, 3/10) for a positive face velocity and (3/10, 6/10, 1/10) for a
// negative one, and the three candidate reconstructions
//   positive:  (2 s1 - 7 s2 + 11 s3)/6, (-s2 + 5 s3 + 2 s4)/6, (2 s3 + 5 s4 - s5)/6
//   negative:  (-s1 + 5 s2 + 2 s3)/6,   (2 s2 + 5 s3 - s4)/6,  (11 s3 - 7 s4 + 2 s5)/6
//
// Reduced precision: advectWenoAt() runs the kernel at the Precision chosen by
// the `precision` runtime parameter.  `lowprec` is the native float kernel in
// the EVOLVE-BLOCK below (evolved via dev/workflows/alphaevolve, experiment dev/workflows/evaluator/experiments/advect-mxp); fp32 / tf32 /
// bf16 / fp16 truncate every op of wenoFaceValue() with RAPTOR, available
// when the Makefile compiles this file with the RAPTOR pass (-DBUBBLE_RAPTOR).

#include "advect.h"

#include <cmath>
#include <cstddef>

#ifdef BUBBLE_RAPTOR
#include "raptor/raptor.h"
#endif

namespace {

constexpr double kEps = 1.0e-15;  // Fortran: eps = 1e-15

inline double sqr(double x) { return x * x; }

// The flux-difference loop, with the face reconstruction `weno` as a functor
// so the fp64 path inlines wenoFaceValue() and the RAPTOR paths call the
// truncated clone through a pointer.
template <typename Weno>
void advectWenoWith(const Grid& g, const std::vector<double>& phi, const std::vector<double>& u,
                    const std::vector<double>& v, std::vector<double>& rhs, Weno weno) {
    if (rhs.size() != g.size()) rhs.assign(g.size(), 0.0);

    const int nx = g.nx();
    const int ny = g.ny();
    const double dxi = 1.0 / g.dx();
    const double dyi = 1.0 / g.dy();

    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            // face(center) = 1 in the Fortran: ul/ur and vl/vr are the MAC
            // face velocities themselves, no averaging.
            const double ul = u[g.index(i, j)];
            const double ur = u[g.index(i + 1, j)];
            const double vl = v[g.index(i, j)];
            const double vr = v[g.index(i, j + 1)];

            // ---- x direction -------------------------------------------------
            double flx;
            double frx;
            if (ur > 0.0) {
                frx = weno(phi[g.index(i - 2, j)], phi[g.index(i - 1, j)], phi[g.index(i, j)],
                           phi[g.index(i + 1, j)], phi[g.index(i + 2, j)], true);
            } else {
                frx = weno(phi[g.index(i - 1, j)], phi[g.index(i, j)], phi[g.index(i + 1, j)],
                           phi[g.index(i + 2, j)], phi[g.index(i + 3, j)], false);
            }
            if (ul > 0.0) {
                flx = weno(phi[g.index(i - 3, j)], phi[g.index(i - 2, j)], phi[g.index(i - 1, j)],
                           phi[g.index(i, j)], phi[g.index(i + 1, j)], true);
            } else {
                flx = weno(phi[g.index(i - 2, j)], phi[g.index(i - 1, j)], phi[g.index(i, j)],
                           phi[g.index(i + 1, j)], phi[g.index(i + 2, j)], false);
            }

            // ---- y direction -------------------------------------------------
            double fly;
            double fry;
            if (vr > 0.0) {
                fry = weno(phi[g.index(i, j - 2)], phi[g.index(i, j - 1)], phi[g.index(i, j)],
                           phi[g.index(i, j + 1)], phi[g.index(i, j + 2)], true);
            } else {
                fry = weno(phi[g.index(i, j - 1)], phi[g.index(i, j)], phi[g.index(i, j + 1)],
                           phi[g.index(i, j + 2)], phi[g.index(i, j + 3)], false);
            }
            if (vl > 0.0) {
                fly = weno(phi[g.index(i, j - 3)], phi[g.index(i, j - 2)], phi[g.index(i, j - 1)],
                           phi[g.index(i, j)], phi[g.index(i, j + 1)], true);
            } else {
                fly = weno(phi[g.index(i, j - 2)], phi[g.index(i, j - 1)], phi[g.index(i, j)],
                           phi[g.index(i, j + 1)], phi[g.index(i, j + 2)], false);
            }

            rhs[g.index(i, j)] += -(frx * ur - flx * ul) * dxi - (fry * vr - fly * vl) * dyi;
        }
    }
}

}  // namespace

double wenoFaceValue(double s1, double s2, double s3, double s4, double s5, bool positive) {
    // Smoothness indicators (identical in both branches).
    const double rIS1 = (13.0 / 12.0) * sqr(s1 - 2.0 * s2 + s3) +
                        (1.0 / 4.0) * sqr(s1 - 4.0 * s2 + 3.0 * s3);
    const double rIS2 = (13.0 / 12.0) * sqr(s2 - 2.0 * s3 + s4) +
                        (1.0 / 4.0) * sqr(s2 - s4);
    const double rIS3 = (13.0 / 12.0) * sqr(s3 - 2.0 * s4 + s5) +
                        (1.0 / 4.0) * sqr(3.0 * s3 - 4.0 * s4 + s5);

    // Linear weights flip with the sign of the face velocity.
    const double d1 = positive ? (1.0 / 10.0) : (3.0 / 10.0);
    const double d2 = 6.0 / 10.0;
    const double d3 = positive ? (3.0 / 10.0) : (1.0 / 10.0);

    const double aT1 = d1 / sqr(kEps + rIS1);
    const double aT2 = d2 / sqr(kEps + rIS2);
    const double aT3 = d3 / sqr(kEps + rIS3);

    const double aSum = aT1 + aT2 + aT3;
    const double a1 = aT1 / aSum;
    const double a2 = aT2 / aSum;
    const double a3 = aT3 / aSum;

    double fT1;
    double fT2;
    double fT3;
    if (positive) {
        fT1 = (2.0 / 6.0) * s1 - (7.0 / 6.0) * s2 + (11.0 / 6.0) * s3;
        fT2 = -(1.0 / 6.0) * s2 + (5.0 / 6.0) * s3 + (2.0 / 6.0) * s4;
        fT3 = (2.0 / 6.0) * s3 + (5.0 / 6.0) * s4 - (1.0 / 6.0) * s5;
    } else {
        fT1 = -(1.0 / 6.0) * s1 + (5.0 / 6.0) * s2 + (2.0 / 6.0) * s3;
        fT2 = (2.0 / 6.0) * s2 + (5.0 / 6.0) * s3 - (1.0 / 6.0) * s4;
        fT3 = (11.0 / 6.0) * s3 - (7.0 / 6.0) * s4 + (2.0 / 6.0) * s5;
    }

    return a1 * fT1 + a2 * fT2 + a3 * fT3;
}

void advectWeno(const Grid& g, const std::vector<double>& phi, const std::vector<double>& u,
                const std::vector<double>& v, std::vector<double>& rhs) {
    advectWenoWith(g, phi, u, v, rhs,
                   [](double s1, double s2, double s3, double s4, double s5, bool positive) {
                       return wenoFaceValue(s1, s2, s3, s4, s5, positive);
                   });
}

// Seed: a straight float transcription of wenoFaceValue() / advectWeno().
// EVOLVE-BLOCK-START
float wenoFaceValueLowPrec(float s1, float s2, float s3, float s4, float s5, bool positive) {
    const float kEpsF = 1.0e-15f;

    const float b1a = s1 - 2.0f * s2 + s3;
    const float b1b = s1 - 4.0f * s2 + 3.0f * s3;
    const float b2a = s2 - 2.0f * s3 + s4;
    const float b2b = s2 - s4;
    const float b3a = s3 - 2.0f * s4 + s5;
    const float b3b = 3.0f * s3 - 4.0f * s4 + s5;
    const float rIS1 = (13.0f / 12.0f) * b1a * b1a + 0.25f * b1b * b1b;
    const float rIS2 = (13.0f / 12.0f) * b2a * b2a + 0.25f * b2b * b2b;
    const float rIS3 = (13.0f / 12.0f) * b3a * b3a + 0.25f * b3b * b3b;

    const float d1 = positive ? 0.1f : 0.3f;
    const float d2 = 0.6f;
    const float d3 = positive ? 0.3f : 0.1f;

    const float e1 = kEpsF + rIS1;
    const float e2 = kEpsF + rIS2;
    const float e3 = kEpsF + rIS3;
    const float aT1 = d1 / (e1 * e1);
    const float aT2 = d2 / (e2 * e2);
    const float aT3 = d3 / (e3 * e3);

    const float aSum = aT1 + aT2 + aT3;
    const float a1 = aT1 / aSum;
    const float a2 = aT2 / aSum;
    const float a3 = aT3 / aSum;

    float fT1;
    float fT2;
    float fT3;
    if (positive) {
        fT1 = (2.0f / 6.0f) * s1 - (7.0f / 6.0f) * s2 + (11.0f / 6.0f) * s3;
        fT2 = -(1.0f / 6.0f) * s2 + (5.0f / 6.0f) * s3 + (2.0f / 6.0f) * s4;
        fT3 = (2.0f / 6.0f) * s3 + (5.0f / 6.0f) * s4 - (1.0f / 6.0f) * s5;
    } else {
        fT1 = -(1.0f / 6.0f) * s1 + (5.0f / 6.0f) * s2 + (2.0f / 6.0f) * s3;
        fT2 = (2.0f / 6.0f) * s2 + (5.0f / 6.0f) * s3 - (1.0f / 6.0f) * s4;
        fT3 = (11.0f / 6.0f) * s3 - (7.0f / 6.0f) * s4 + (2.0f / 6.0f) * s5;
    }

    return a1 * fT1 + a2 * fT2 + a3 * fT3;
}

void advectWenoLowPrec(const Grid& g, const std::vector<float>& phi, const std::vector<float>& u,
                       const std::vector<float>& v, float dxi, float dyi, std::vector<float>& rhs) {
    const int nx = g.nx();
    const int ny = g.ny();

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

namespace {

// fp64 <-> float boundary of the lowprec kernel.  Scratch is kept between
// calls so the time loop does not reallocate.
void advectWenoViaLowPrec(const Grid& g, const std::vector<double>& phi,
                          const std::vector<double>& u, const std::vector<double>& v,
                          std::vector<double>& rhs) {
    static std::vector<float> phiF, uF, vF, rhsF;
    phiF.assign(phi.begin(), phi.end());
    uF.assign(u.begin(), u.end());
    vF.assign(v.begin(), v.end());
    rhsF.assign(g.size(), 0.0f);

    advectWenoLowPrec(g, phiF, uF, vF, static_cast<float>(1.0 / g.dx()),
                      static_cast<float>(1.0 / g.dy()), rhsF);

    if (rhs.size() != g.size()) rhs.assign(g.size(), 0.0);
    for (std::size_t k = 0; k < rhs.size(); ++k) rhs[k] += static_cast<double>(rhsF[k]);
}

#ifdef BUBBLE_RAPTOR
using WenoFn = double (*)(double, double, double, double, double, bool);

// RAPTOR clones wenoFaceValue() with every FP op rounded to (exponent,
// mantissa) bits.  The format must be a compile-time constant, hence one call
// site per format; each clone is built on first use.
WenoFn truncatedWeno(Precision m) {
    switch (m) {
        case Precision::kFp32: {
            static WenoFn fn = __raptor_truncate_op_func(wenoFaceValue, 64, 1, 8, 23);
            return fn;
        }
        case Precision::kTf32: {
            static WenoFn fn = __raptor_truncate_op_func(wenoFaceValue, 64, 1, 8, 10);
            return fn;
        }
        case Precision::kBf16: {
            static WenoFn fn = __raptor_truncate_op_func(wenoFaceValue, 64, 1, 8, 7);
            return fn;
        }
        case Precision::kFp16: {
            static WenoFn fn = __raptor_truncate_op_func(wenoFaceValue, 64, 1, 5, 10);
            return fn;
        }
        default:
            return wenoFaceValue;
    }
}
#endif  // BUBBLE_RAPTOR

}  // namespace

bool precisionAvailable(Precision m) {
    if (m == Precision::kFp64 || m == Precision::kLowPrec) return true;
#ifdef BUBBLE_RAPTOR
    return true;
#else
    return false;
#endif
}

void advectWenoAt(const Grid& g, Precision m, const std::vector<double>& phi,
                  const std::vector<double>& u, const std::vector<double>& v,
                  std::vector<double>& rhs) {
    if (m == Precision::kFp64) {
        advectWeno(g, phi, u, v, rhs);
    } else if (m == Precision::kLowPrec) {
        advectWenoViaLowPrec(g, phi, u, v, rhs);
    } else {
#ifdef BUBBLE_RAPTOR
        advectWenoWith(g, phi, u, v, rhs, truncatedWeno(m));
#else
        // Unreachable: main() rejects modes for which !precisionAvailable().
        advectWeno(g, phi, u, v, rhs);
#endif
    }
}
