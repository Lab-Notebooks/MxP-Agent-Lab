# Verify workflow: analysis of a run for human examination

```sh
make
mpirun -np 4 ./bubble params/bubble_eval.ini precision=lowprec outdir=out_eval   # any precision != fp64
python3 dev/workflows/verify/verify.py out_eval                                   # -> out_eval/verify/
python3 dev/workflows/verify/verify.py out_a --ref out_b                          # two separate runs instead
python3 dev/workflows/verify/verify.py out_eval --cell 64 90                      # schematic at a chosen (i,j)
```

Reads the run's `history.csv` and `dfun_*.pvti/.vti` (reassembled across ranks)
and the kernels in `src/advect.cpp`, and writes:

| file | content |
| --- | --- |
| `report.md` | the full analysis below, as tables and prose |
| `metrics.json` | the same numbers, machine-readable (used by `.claude/workflows/myloop.js`) |
| `norms_vs_time.png` | L1, L2 (RMS), Linf of phi - phi_fp64 per snapshot |
| `error_map.png` | signed error at the final frame with both phi = 0 contours and the (i,j) cell |
| `stencil_schematic.png` | cell (i,j): cell-centre vs face-centre variables and every WENO5 stencil |

1. **Error norms.** ||e||_1, ||e||_2 (RMS), ||e||_inf per snapshot, recomputed
   from the fields and cross-checked against `history.csv`. Also reported: the
   localisation ratio Linf/L2, the interface-band L2 (|phi_fp64| <= 5h), where
   the max error is and how far it lies from the interface, Linf in FP32 ulps,
   and an exponential growth fit of L2(t).
2. **Stencils.** The `g.index(i+a, j+b)` offsets of each WENO5 call in the FP64
   and lowprec kernels are parsed and checked: identical in both kernels,
   upwind-biased for the sign of the face velocity, reach <= NGUARD = 3. The
   candidate polynomials and linear weights are checked exactly with
   fractions: each candidate is consistent and exact to degree 2, and the
   optimal combination is exact to degree 4 (5th order). If AlphaEvolve
   restructures the lowprec kernel beyond the parser, the check reports INFO
   instead of PASS.
3. **Cell (i,j).** Defaults to the cell of max final error. Variable placement:
   `dfun`, `rhs`, `dfun_ref`, `err` sit at cell centres. Solver `u` and `v` sit
   at MAC face centres, with u(i,j) on the low face i-1/2 and v(i,j) on the low
   face j-1/2. The `u` and `v` written to the .vti are face averages moved to
   the centre, not face values.

Exit status is 1 if any stencil check FAILs.
