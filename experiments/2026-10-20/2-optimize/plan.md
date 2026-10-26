# Plan: optimize advectWenoLowPrec

## Objective

Evolve the float seed advectWenoLowPrec (app/src/advect.cpp EVOLVE-BLOCK) with AlphaEvolve for
lower error vs the FP64 shadow, score each candidate locally, and apply the one the scientist picks.

## Health check (app.md + preconditions), 2026-10-20

- app.md: pass (1-raptorize/plan.md); `bmx make check` 17/17 with the raptorize change.
- One EVOLVE-BLOCK in src/advect.cpp: pass. Seed has no FP64 instruction in *LowPrec* functions
  (objdump of build/advect.o; wenoFaceValueLowPrec is inlined).
- `ae`, objdump, PyYAML in bmx: pass.
- Raptorize committed: FAIL by design; per AGENTS.md the scientist commits, the agent never does.
- `bmx gcloud auth application-default print-access-token`: FAIL. Scientist runs
  `gcloud auth application-default login` on the host. Blocks only the launch (steps 5-7).

## Decisions

- score: -log10 of the RMS over rows of `err_rms` (history.csv), mode lowprec, the eval run of
  1-raptorize/plan.md (128², 100 steps, 1 rank). Seed ≈ 9.27 (1-raptorize results/lowprec).
  The interface-band error suggested in the report needs a new app column, i.e. a raptorize
  loop-back (E/3-raptorize); not taken here.
- speed: not counted (template default; float vs fp64 runtimes are equal at this size).
- extra rules: none beyond the template; rule 4 fixes the contract below.
- hints: none.
- GCP: project <redacted>, engine
  <redacted>, models gemini-3-flash-preview (= preset GEMINI_V3P0_MIXTURE), location global.
- job: max_programs 10 (scientist, 2026-10-22; was 20), concurrency 4, timeout 600 s.

## Source change

None in app/ before Gate 2. New in R = experiments/2026-10-20/2-optimize:

- experiment.yaml from the template: seed `app/src/advect.cpp`, language cpp, the job above;
  problem: evolve the block; EVALUATION (build, lowprec run, score); RULES 1-4 of the template and
  5. `advectWenoLowPrec(const Grid&, float dxi, float dyi, const float* phi, const float* u,
  const float* v, float* rhs)` keeps its signature, adds the advection term into rhs (+=) over
  owned cells, reads phi only within 3 guard cells.
- evaluator.py from the template: source src/advect.cpp, objfile build/advect.o, command
  `mpirun -n 1 ./bubble params/bubble.ini nx=128 ny=128 max_steps=100 plot_interval=0
  precision=lowprec outdir={OUTDIR}`, ERRORS = OUTDIR/history.csv, column err_rms; metrics
  score, accuracy (equal).

## Steps

1. Write experiment.yaml and evaluator.py as above.
2. Calibrate (after `mkdir -p $R/results`: the app creates only the last level): `bmx make evaluate EXP=$R` twice; pass: identical scores, ≈ 9.27; save
   results/baseline.json.
3. Red-team in /tmp/canary (`rsync -a --exclude .git ./ /tmp/canary/`), one edit at a time:
   a double, `* 1.1`, an #include, untagged helpers (float; double via `auto`), skipping the
   y-direction, and that edit outside the block. Pass: each
   scores null or below the seed.
4. Gate 1: show files, baseline, red-team, job. Submit only on approval.
5. Launch (needs credentials and GCP fields): `bmx make evolve EXP=$R`.
6. Monitor: `AE_CONFIG_DIR=$R/results/ae ae experiment describe <nick>`, `ae results best <nick>`.
7. Candidates: `ae program show <prog> --output-file results/<prog>.cpp`, diff vs seed, re-score in
   /tmp/canary; apply the chosen one over the seed, then `bmx make check`.

## Checkpoints

1. Calibration: both baseline scores side by side; does the score mean what you intend?
   Outcome: 9.272190599222778 twice (results/baseline.json); scientist keeps the whole-grid score.
2. Red-team: cheat -> result; any that scores gets a tightened rule.
   Results (seed 9.272; cheats in /tmp/redteam):
   double variable -> null (objdump: mulsd); `* 1.1` -> null (mulsd); #include -> 9.272, scores;
   untagged helper, float vars -> 9.083 (below); untagged noinline helper computing in double via
   `auto` -> 9.327, scores; skip y-direction in block -> 2.225 (below); same edit also applied
   outside the block (to the FP64 reference) -> 9.408, scores.
   Outcome: three holes fixed in dev/workflows/optimize/evolve.py, generally (scientist): comment
   regex blinded the token scan; untagged block functions escaped objdump; code outside the block
   was unchecked. Rules 2-4 updated in both experiment.yaml. Rerun: every cheat null (double,
   #include: FORBIDDEN_TOKEN; `* 1.1`, tagged `auto` helper: FP64 instruction; untagged helpers:
   UNTAGGED_FUNCTION; outside edit: CHANGED_OUTSIDE_BLOCK) or below (skip 2.225); seed 9.272,
   recalibrated identical.
3. Gate 1: experiment.yaml, evaluator.py, baseline, red-team, job. Submit?
   Outcome (2026-10-22): approved with max_programs 10. app/ re-applied from raptorize.patch;
   seed re-scored 9.272190599222778; ADC via container login; launched as exp-free-hyena.
   evolve.py crashed after `experiment start` (non-JSON stdout); the run loop was started by
   hand; fixed in evolve.py (`_call`, not `_json`).
   exp-free-hyena: 9/9 candidates null, an evaluator bug: ae runs a copy of evaluator.py in a
   temp dir, so OUTDIR's parent was missing and the app crashed (prog-broad-agama scores
   9.201 when run by hand). Fixed in evolve.py (prepare creates OUTDIR's parent); relaunched.
4. Progress: best score per generation; flag jumps. Continue or stop?
   Results (exp-ginger-chimpanzee, 10 programs, seed 9.272): 9.165, 9.206, null
   (COMPILATION_ERROR: unused sqrLowPrec, -Werror), 9.271, 9.388, 9.247, 9.246, 9.210, 9.388,
   9.388. Jump +0.116 (RMS error 1.31x lower) at prog-nano-chihuahua; pink-guppy equal,
   humongous-macaw 9.388239. It writes the WENO face value as s3 + (1/6) * weighted
   differences of neighbours, instead of a weighted sum of three full-size stencils.
5. Gate 2: top candidates as diff vs seed, re-score, one line on numerics each. Which to apply?
   Results (re-scored in /tmp/canary; t = 2: full run, 10236 steps, 2 ranks; seed full_lowprec):
   - nano-chihuahua 9.388321 (t = 2: RMS over steps 2.80e-7, final 6.20e-7): only
     wenoFaceValueLowPrec changes; face value as s3 + (1/6) * weighted neighbour differences.
   - pink-guppy 9.388321 (= nano at t = 2): nano's face value, rhs update split in two.
   - humongous-macaw 9.388239 (t = 2: 2.62e-7, final 5.83e-7): same idea, a2 eliminated
     (a2 = 1 - a1 - a3); x and y fluxes summed before one rhs subtraction.
   Seed at t = 2: 3.06e-7, final 6.31e-7. The 100-step gain (1.31x) shrinks over a full run to
   1.10x (nano) and 1.17x (macaw).
   Outcome (scientist: verify these results; agent default): apply humongous-macaw, best at
   t = 2, tied at 100 steps. `bmx make check` 17/17.

## Runner

Single agent: steps 1-4 and 6-7 (all local, in bmx). AlphaEvolve is the swarm in step 5: GCP
generates candidates, evaluator.py scores them one at a time in the container.
