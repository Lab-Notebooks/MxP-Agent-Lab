# Staged workflows: from a clean FP64 `src/` to a verified mixed-precision kernel

`src/` starts as the plain FP64 Deforming Bubble mini-app. Four stages change it,
and its build system, step by step. Nothing about the target is decided in
advance: at each stage the scientist chooses the kernel, formats, metric and
acceptance criteria, and Claude produces the change for that choice. Each stage
is started by the scientist asking Claude to run it, follows that stage's
`plan.md`, and ends at a gate the scientist signs off on. No stage starts the
next one.

| # | stage | weight | files | what it changes |
|---|-------|--------|-------|-----------------|
| 1 | [`raptorize/`](raptorize/) | simple | `plan.md`, `example.patch` | `src/`, `Makefile`, `params/`: `precision` modes truncated by RAPTOR + FP64 shadow for the chosen kernel |
| 2 | [`evaluator/`](evaluator/) | simple | `plan.md`, `example.patch` | `src/`, `params/`: native low-precision seed inside `// EVOLVE-BLOCK-START/END` |
| 3 | [`alphaevolve/`](alphaevolve/) | elaborate | `plan.md`, `README.md`, `experiment.template.yaml`, `evaluator.template.py` | `alphaevolve/experiments/<name>/`; runs jobs; `src/` only when a candidate is applied |
| 4 | [`verify/`](verify/) | elaborate | `plan.md`, `README.md`, `verification.yaml`, `run_verification.py`, `verify.py` | nothing in `src/`; reports under the spec's `dest` |

```text
clean FP64 src ──1 raptorize──▶ precision modes + FP64 shadow ──2 evaluator──▶ EVOLVE-BLOCK seed
                                                                                   │
           merge ◀── 4 verify (tests, proofs, analysis) ◀── apply ◀──3 alphaevolve (evaluator.py, jobs)
```

## Examples, not presets

The `example.patch` files show each change on a small toy kernel. They are
patterns, not code for this repo. For the scientist's kernel, Claude writes a
new patch in the same shape. It saves the patch as
`dev/workflows/<stage>/patches/<kernel>.patch` for provenance, then the gate
runs. Likewise, stage 3 fills in its templates and stage 4 fills in its
`verification.yaml` for the chosen kernel and metric.

## Running a stage

Ask Claude in plain language, naming the stage and your choices, e.g.

- "Run the raptorize stage on `<function>` in `src/<file>.cpp` with fp32 and bf16."
- "Run the evaluator stage: evolve `<function>LowPrec` in float."
- "Run the alphaevolve stage: accuracy first, speed only while RMS <= 1e-6; baseline and red-team it."
- "Run the verify stage on the applied candidate with tolerance 1e-6 at np 1 and 4."

After each stage, review the patch or `git diff`, then commit. One commit (or
branch) per stage keeps every step easy to inspect and revert.

## Containers

Stages 1 and 2 edit source and run on the host. Stages 3 and 4 run their
builds, jobs and analysis in the project image, built from the root
[`Dockerfile`](../../Dockerfile):
- Ubuntu 24.04, OpenMPI, clang 20
- RAPTOR, built from `dev/tools/RAPTOR`
- Python with numpy, matplotlib and PyYAML

Building the image runs `make check` on the current tree, so the image itself
is a gate:

```sh
docker build -t bubble-mxp .      # rebuild after each stage that changes src/
alias bmx='docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/work bubble-mxp'
bmx make check
```

The repo is mounted at `/work`, so builds and outputs (`build/`, `bubble`,
`out_*/`, `experiments/*/results/`) land in the working tree. All of these are
gitignored.
