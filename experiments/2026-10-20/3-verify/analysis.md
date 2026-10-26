# Analysis: humongous-macaw vs the float seed

All 19 checks pass (results/verify/summary.md). Errors are vs the FP64 shadow run; "full" is the
run to t = 2 (10236 steps, 2 ranks); seed numbers from 1-raptorize/results.

## Does the gain hold?

| run | seed RMS err_rms | candidate | gain | file |
|:--|--:|--:|--:|:--|
| eval (128², 100 steps) | 5.34e-10 | 4.09e-10 | 1.31x | summary.md |
| nx256 (256², 200 steps) | 5.54e-10 | 4.36e-10 | 1.27x | summary.md; results/seed_nx256 |
| full, t < 1.452 (before the jump) | 9.12e-9 | 3.34e-9 | 2.73x | verify/full/out/history.csv |
| full, after both jumps | 4.87e-7 | 5.27e-7 | 0.92x | same |
| full, all steps | 3.06e-7 | 2.62e-7 | 1.17x | summary.md |

- Ranks: dfun bit-identical at 1, 2, 4 ranks; err_rms differs by 1.9e-14 (MPI sum order).
- Conservation at t = 2: area 2.2e-6, mass 3.5e-6 off FP64 (seed 1.6e-6, 8.2e-6; bf16 2.6e-3).
- Interface band (|phi| < 3 dx, figures/verify_band.png): up to t = 1.4 the candidate is 2-3x
  lower near the interface and away from it alike; after the jump the two are comparable.

So yes while the flow is smooth, at either resolution and any rank count; no after t ≈ 1.45.

## Why the rewrite helps (informal)

Both are the same WENO5 scheme: the face values agree in exact arithmetic (sympy check). The
seed sums three candidate stencils of size |phi| with weights a1, a2, a3; their large parts cancel
to give a value near s3, so rounding error scales with |phi|. The candidate writes the face value
as s3 + (1/6)·Σ a_k·(differences of neighbours), using a2 = 1 - a1 - a3. Near a smooth field
the differences are O(dx·|∇phi|), so the rounded part is small and s3 is carried exactly.

## Why the gain vanishes after t ≈ 1.45

At step 7432 (t = 1.452) the seed's error jumps in one step from 1.7e-7 to 5.5e-4 (err_max).
A rerun of seed and FP64 to steps 7431/7432 (in /tmp/canary) shows the cause: cell (93, 39)
crosses zero, phi = +1.18e-8 in FP64 and -3.25e-9 in float. The FP64 redistance's interface test
(app/src/redistance.cpp:70, `sm * sxl < 0.0 || ...`) then takes the other branch, and the
neighbour (92, 39) differs by 5.5e-4: a 1.5e-8 difference amplified 3.7e4x. The candidate hits
the same switch at step 7707 (t = 1.506, err_max 5.5e-4). After that, sign flips of near-zero
cells dominate the error in every low-precision run (fp32 emulated has the seed's 62 spikes, the
candidate 28), and kernel rounding no longer decides it.

## Recommendation (the scientist decides)

The candidate is the same scheme, more accurate while the flow is smooth and no worse at
t = 2 overall: merge it. A further optimize run should use a score the discrete switch
cannot decide:

- score over t < 1.4 or 100-200 steps (as now), or at 2x resolution;
- or count the sign-flip events, or score on area/mass at t = 2 instead of pointwise error.
