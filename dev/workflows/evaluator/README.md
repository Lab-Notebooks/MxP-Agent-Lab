# Evaluator workflow: turn a task into an AlphaEvolve experiment

Builds what the generic runner (`dev/workflows/alphaevolve/`) needs for a new
optimization target:

1. **Embed the EVOLVE-BLOCK** around the functions to evolve:

   ```sh
   python3 dev/workflows/evaluator/embed_evolve_block.py src/<file>.cpp --functions fnA fnB
   python3 dev/workflows/evaluator/embed_evolve_block.py src/<file>.cpp --list
   ```

   The functions must be contiguous. The script is idempotent and leaves the
   file alone if they are already inside a block. Code outside the block is the
   fixed harness (signatures, conversion at the boundary, reference
   implementation); keep it compiling and `make check` green.

2. **Write `experiments/<name>/experiment.yaml`**: `title`, `problem_description`
   (rules the evaluator enforces, plus numerical guidance), `primary_metric`,
   `seed_files` (the files with blocks), and `evaluator: {script, method, postprocess}`.

3. **Write `experiments/<name>/evaluator.py`**: `method()` builds, audits the
   block, runs, and returns `{"scores": [...], "insights": [...]}` (the
   `alpha_evolve.models` types when that package is installed, plain dicts
   otherwise). `postprocess(top_programs)` writes the best program's files to
   `experiments/<name>/results/best/<seed path>`. Start from `experiments/advect-mxp/`.

4. **Validate**: `python3 dev/workflows/alphaevolve/run.py baseline experiments/<name>`.

## Experiments

### `advect-mxp`: reduced-precision WENO5 advection

- Block: `wenoFaceValueLowPrec`, `advectWenoLowPrec` in `src/advect.cpp` (seed:
  a plain float transcription of the FP64 kernel).
- One build (`make`, advect.cpp through clang + RAPTOR), one run
  (`params/bubble_eval.ini`, `precision=lowprec`). The binary carries its own
  FP64 shadow trajectory, so the error comes straight from `history.csv`.
- Audit: no 64-bit FP tokens in the block, and no `*sd`/`*pd` arithmetic in the
  `*LowPrec` functions of `build/advect.o`.
- Scores: `trajectory_accuracy_score = -log10(mean RMS^2 + 1e-30)`,
  `advect_speedup` (FP64 / lowprec advection wall time, same run), and
  `combined_score = accuracy + 2 log2(speedup)` (primary), where speed counts only
  while trajectory RMS <= 1e-6. Tune with `BUBBLE_SPEED_WEIGHT` and `BUBBLE_RMS_TOL`.
