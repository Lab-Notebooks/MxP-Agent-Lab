# Optimize report: advectWenoLowPrec on the splitting bubble

## Result

AlphaEvolve improved the float seed.

- Baseline (seed): score 9.5552, RMS error vs FP64 shadow 2.785e-10.
- Best candidate: score 9.6942 (prog-hulking-chipmunk, prog-clever-mouflon tied).
- Improvement: +0.1390 score, i.e. ~1.38x lower RMS error (~2.0e-10).
- Run: 19 programs evaluated, 17 succeeded, 2 failed.

## Red-team

All cheats scored null or below seed: double variable, `* 1.1`, `#include`,
untagged double helper, skipped work (2.2969), outside-block edit.

## Notes

- Candidate was not applied to app/; RAPTOR logic reverted from app/ during cleanup.
- Experiment data retained under experiments/2026-10-26/2-optimize/.
