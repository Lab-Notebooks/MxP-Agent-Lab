# Stage 4, verify: tests, proofs and analysis of a candidate

Decides whether a candidate `src/` (usually the candidate applied in stage 3) is correct,
and explains why it behaves as it does, from data. Runs in the project
container. It writes nothing to `src/`, only to the run's output directory or
to the `--dest` you choose.

Steps are in [`plan.md`](plan.md). Acceptance criteria and runs are in a copy of
[`verification.yaml`](verification.yaml), executed by
[`run_verification.py`](run_verification.py):

```text
dev/workflows/verify/
├── plan.md                 the steps
├── verification.yaml       scientist-owned spec: runs, comparisons, cells, acceptance criteria
├── run_verification.py     make check + runs + verify.py --criteria + comparisons -> <dest>/summary.md
├── verify.py               analysis + proofs of one run (or run vs --ref run) -> report.md, metrics.json, PNGs
└── proofs/                 kernel-specific property checkers written in stage 4 (when needed)
```

Single-run use, in the container:

```sh
bmx python3 dev/workflows/verify/verify.py out_eval                                   # -> out_eval/verify/
bmx python3 dev/workflows/verify/verify.py out_a --ref out_b                          # two separate runs
bmx python3 dev/workflows/verify/verify.py out_eval --cell 64 90                      # schematic at (i,j)
bmx python3 dev/workflows/verify/verify.py out_eval --criteria dev/workflows/verify/verification.yaml
```

`make check` runs every test on the rank counts declared in its source
(`// ranks: 1 2 4`), so halo exchange and rank invariance are covered at np =
1, 2 and 4. `verify.py` also works on a RAPTOR run (`precision=fp32|tf32|bf16|fp16`).
When a run has no lowprec kernel, it reports the lowprec stencil checks as INFO.

`verify.py` reads the run's `history.csv` and `dfun_*.pvti/.vti` (reassembled
across ranks), and the source with `--src`. It writes the files below. The
error analysis works for any kernel whose stage-1 patch writes the FP64 shadow
(`dfun_ref`), or for any pair of runs compared with `--ref`. The proofs and the
schematic are specific to the WENO5 kernels in `src/advect.cpp`.

| file | content |
| --- | --- |
| `report.md` | the full analysis below, as tables and prose |
| `metrics.json` | the same numbers, machine-readable |
| `norms_vs_time.png` | L1, L2 (RMS), Linf of phi - phi_fp64 per snapshot |
| `error_map.png` | signed error at the final frame with both phi = 0 contours and the (i,j) cell |
| `stencil_schematic.png` | cell (i,j): cell-centre vs face-centre variables and every WENO5 stencil |

## Proofs (machine-checked)

Built in, for WENO5 (`src/advect.cpp`). They come from the source, in exact
rational arithmetic, and must PASS:

- **Stencils.** The `g.index(i+a, j+b)` offsets of each WENO5 call in the FP64
  and lowprec kernels are parsed and checked: identical in both kernels,
  upwind-biased for the sign of the face velocity, and reaching at most NGUARD = 3.
- **Consistency and order.** The candidate polynomials and linear weights are
  checked exactly with fractions. Each candidate is consistent and exact to
  degree 2. The optimal combination is exact to degree 4, i.e. 5th order.

If AlphaEvolve restructures the lowprec kernel beyond the parser, the check
reports INFO instead of PASS. Then the scientist must check those properties by
hand, or extend the parser. For any other kernel, stage 4 writes the property
checker under `proofs/`, using `analyse_stencils` as the pattern. Any further argument, such as a floating-point
error bound for the evolved kernel, goes in the report labeled as either
machine-checked (with the script that checks it) or informal.

## Analysis

1. **Error norms.** ||e||_1, ||e||_2 (RMS) and ||e||_inf per snapshot,
   recomputed from the fields and cross-checked against `history.csv`. Also
   reported:
   - the localisation ratio Linf/L2
   - the interface-band L2 (|phi_fp64| <= 5h)
   - where the max error is, and how far it lies from the interface
   - Linf in FP32 ulps
   - an exponential growth fit of L2(t)
2. **Cell (i,j).** Defaults to the cell of max final error. Where each variable sits:
   - `dfun`, `rhs`, `dfun_ref` and `err` are at cell centres.
   - Solver `u` and `v` are at MAC face centres: u(i,j) on the low face i-1/2,
     and v(i,j) on the low face j-1/2.
   - The `u` and `v` written to the .vti are face averages moved to the centre,
     not face values.

Every number in the scientist-facing write-up must cite the file it came from
(`metrics.json`, `report.md`, `history.csv`, or the run log).
