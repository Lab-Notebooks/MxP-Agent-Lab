# Stage 4, verify: plan

**Goal:** decide, from data, whether a candidate `src/` is correct and should be
merged, and explain why it behaves as it does. Runs in the project container and
writes only under the spec's `dest`. Reference material (outputs, proofs,
analysis) is in [`README.md`](README.md).

**Scientist decides** (in a copy of `verification.yaml`):
- the candidate
- the runs and studies
- the cells to inspect
- the run-vs-run comparisons
- the properties to prove
- the acceptance limits

## Steps

0. **Set up.** Pin the candidate: `git log -1`, and `git diff --stat` against
   the stage-2 commit.
   ```sh
   docker build -t bubble-mxp .      # the image build runs make check on the candidate
   alias bmx='docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/work bubble-mxp'
   ```
1. **Write the spec.** Copy `verification.yaml` to
   `verification.<candidate>.yaml` and fill every `<...>` with the scientist:
   - the runs: at least the eval run and a second rank count
   - the comparisons and the cells
   - the criteria
   - any extra studies, added as runs: a finer grid, another geometry,
     `reverse_time=true`, or a RAPTOR format for context
2. **Run everything:**
   ```sh
   bmx python3 dev/workflows/verify/run_verification.py verification.<candidate>.yaml
   ```
   This runs `make check`, every run (with its log in `run.log`),
   `verify.py --criteria` on each run, the cell schematics and the
   comparisons. It writes `<dest>/summary.md`, and exits 1 if anything fails.
3. **Prove the properties.** `verify.py` machine-checks the WENO5 stencils,
   consistency and order of accuracy, in exact rational arithmetic, wherever it
   can parse the kernel. If the candidate's kernel is something else, or the
   checks come back INFO:
   - write a property checker for it, e.g. exactness on polynomials,
     conservation, symmetry, or a forward error bound evaluated exactly. Use
     `analyse_stencils` in `verify.py` as the pattern, put it at
     `dev/workflows/verify/proofs/<property>.py`, and run it in the container.
   - otherwise, argue it by hand and label it **informal** in the analysis.
4. **Read the data.** Open every PNG and read `report.md` and `metrics.json`.
   Check:
   - where the max error sits relative to the interface (`argmax_dist_cells`)
   - whether the error grows (`growth_fit`)
   - that the history cross-check is consistent
   - cell-centre vs face-centre quantities at the cells examined
   - the area drift and timings from `run.log`
5. **Write the analysis** to `<dest>/analysis.md`, answering the scientist's
   questions point by point. Every number must cite its file (`metrics.json`
   field, `report.md` table, `history.csv` column, or a `run.log` line). Don't
   quote numbers from memory or from another run.
6. **Gate.** The scientist reads `summary.md` and `analysis.md`, then decides:
   - merge: open a PR from the stage branch, or
   - send back to stage 3 with new constraints (tighter rules, a different metric).

## Must not

- Edit `src/`, `tests/`, `params/*.ini` or the `Makefile`.
- Loosen a criterion to make a run pass. Only the scientist changes limits.
