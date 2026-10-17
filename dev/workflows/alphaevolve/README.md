# AlphaEvolve: generic optimization run

Runs an AlphaEvolve optimization for **any** experiment in this repo. It knows
nothing about a particular problem: the problem lives in an experiment directory,
and the code to evolve is whatever already sits between `// EVOLVE-BLOCK-START`
and `// EVOLVE-BLOCK-END` in that experiment's seed files. Experiments, and the
EVOLVE-BLOCK markers in the source, are produced by the evaluator workflow
(`dev/workflows/evaluator/`). This workflow is triggered independently of the
analysis workflow (`dev/workflows/verify/`).

```text
experiment dir (from dev/workflows/evaluator/experiments/<name>/)
├── experiment.yaml   title, problem_description, primary_metric, seed_files,
│                     evaluator: {script, method, postprocess?}
├── evaluator.py      method() -> {"scores": [{metric, score}], "insights": [{label, text}]}
└── results/          written here: baseline.json, bundle.tar.gz, best/ (gitignored)
```

## Steps

```sh
X=dev/workflows/evaluator/experiments/advect-mxp
R=dev/workflows/alphaevolve/run.py

python3 $R check    $X   # seed files have balanced EVOLVE-BLOCKs, evaluator + methods exist
python3 $R baseline $X   # score the current tree -> $X/results/baseline.json
python3 $R bundle   $X   # working tree (minus submodules) -> $X/results/bundle.tar.gz
ALPHAEVOLVE_SUBMIT_CMD='<your platform CLI> ... {experiment} {bundle}' \
python3 $R submit   $X   # bundle + launch on the AlphaEvolve platform
python3 $R apply    $X   # copy $X/results/best/<seed files> (from postprocess) into the repo
```

- `submit` has no default command: the launch CLI depends on your AlphaEvolve
  deployment. `{experiment}`, `{bundle}` and `{root}` are substituted.
- `apply` shows a diff stat and asks before overwriting. Afterwards, run the
  analysis workflow (`dev/workflows/verify/`) and `make check` on the result.

## Current experiments

| name | seed files | primary metric | baseline |
| --- | --- | --- | --- |
| `advect-mxp` | `src/advect.cpp` | `combined_score` (accuracy vs FP64 + speedup, gated) | 18.02 (accuracy 18.0, speedup 1.0x) |
