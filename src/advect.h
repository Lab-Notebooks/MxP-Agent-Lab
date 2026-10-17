// advect.h -- WENO5 advection kernel (U3).
//
// Ported from
//   Flash-X-Development/Flash-X/source/numericalTools/Stencils/
//     StencilsAdvection/Stencils_advectWeno.F90
// called with face(center) = 1 (cell-centred scalar, MAC face velocities),
// NDIM = 2 so the z term is zero.  Spec: dev/tasks/init-project/spec.md,
// "Advection".
//
// Flux-difference form, exactly the Fortran's last statement:
//
//   rhs(i,j) -= ( frx*u(i+1,j) - flx*u(i,j) )/dx
//             + ( fry*v(i,j+1) - fly*v(i,j) )/dy
//
// with frx/flx/fry/fly the WENO5 reconstructions of phi at the four faces of
// cell (i,j).  Note the Fortran *accumulates* into rhs; advectWeno() does the
// same, so the caller zeroes (or pre-loads) rhs.
#ifndef BUBBLE_ADVECT_H
#define BUBBLE_ADVECT_H

#include <vector>

#include "grid.h"
#include "params.h"

// WENO5 reconstruction of phi at one face from the five-point stencil
// s1..s5, with `positive` selecting the upwind branch on the sign of the face
// velocity (Fortran: `if (ur .gt. 0)`).  Linear weights are (1/10, 6/10, 3/10)
// for a positive face velocity and (3/10, 6/10, 1/10) for a negative one;
// smoothness indicators use the 13/12 and 1/4 coefficients, eps = 1e-15 and
// exponent 2.  Exposed so the tests can exercise the reconstruction directly.
double wenoFaceValue(double s1, double s2, double s3, double s4, double s5, bool positive);

// Add the WENO5 advection term to `rhs` over all owned cells.
//
// The caller must have refreshed the guard cells of `phi` (3 layers) before
// calling: the stencil reaches i-3 .. i+3.  `u` and `v` are MAC face
// velocities with u(i,j) the low x-face of cell (i,j) and v(i,j) the low
// y-face; the high faces u(nx,j), v(i,ny) are read as well.  `rhs` is resized
// (and the new cells zeroed) if it does not already match the grid.
void advectWeno(const Grid& g, const std::vector<double>& phi, const std::vector<double>& u,
                const std::vector<double>& v, std::vector<double>& rhs);

// ---- reduced precision -----------------------------------------------------
//
// The AlphaEvolve EVOLVE-BLOCK in advect.cpp: the same reconstruction and
// flux difference with float stencil values, face velocities and rhs.  Must
// not use any 64-bit floating point (the advect-mxp evaluator audits the
// source and the disassembly).  `dxi`, `dyi` are 1/dx, 1/dy.  Accumulates
// into `rhs` like advectWeno().
float wenoFaceValueLowPrec(float s1, float s2, float s3, float s4, float s5, bool positive);
void advectWenoLowPrec(const Grid& g, const std::vector<float>& phi, const std::vector<float>& u,
                       const std::vector<float>& v, float dxi, float dyi, std::vector<float>& rhs);

// True if this build can run `m`: the RAPTOR-emulated formats need advect.cpp
// compiled with the RAPTOR pass (BUBBLE_RAPTOR, see the Makefile).
bool precisionAvailable(Precision m);

// advectWeno() with the kernel run at precision `m` (see Precision in
// params.h).  The fp64 fields are converted at the boundary; the conversion
// is part of the cost of the reduced-precision kernel.
void advectWenoAt(const Grid& g, Precision m, const std::vector<double>& phi,
                  const std::vector<double>& u, const std::vector<double>& v,
                  std::vector<double>& rhs);

#endif  // BUBBLE_ADVECT_H
