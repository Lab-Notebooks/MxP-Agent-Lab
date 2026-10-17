# Stage 3, alphaevolve: plan

**Goal:** an evaluator whose score means what the scientist intends; AlphaEvolve
jobs run against it; one candidate chosen and applied. Needs stage 2 (an
EVOLVE-BLOCK and a low-precision `precision` mode). Runs in the project
container. Writes only to `experiments/<name>/`, and to `src/` only when a
candidate is applied.

**Scientist decides:**
- the error measure the score uses, from the columns stage 1 records:
  - global: a field norm of state minus shadow (RMS, Linf, band-restricted), averaged over snapshots or taken at the final time
  - scalar: a quantity-of-interest error, e.g. |area - area_ref|

  If the measure isn't recorded, go back to stage 1 and add it.
- the primary metric, its formula and gates
- the hard rules
- the hints allowed in the prompt
- the eval configuration and time limit
- the job budget and stop criteria
- which candidate to apply

## Steps

0. **Set up:**
   ```sh
   docker build -t bubble-mxp .      # the image build runs make check on the stage-2 tree
   alias bmx='docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/work bubble-mxp'
   X=dev/workflows/alphaevolve/experiments/<name>; mkdir -p $X/results
   ```
1. **Write the experiment** with the scientist:
   - `$X/experiment.yaml`, from `experiment.template.yaml`. Fill every `<...>`.
     `problem_description` states exactly the rules the evaluator enforces.
   - `$X/evaluator.py`, from `evaluator.template.py`. Fill every `TODO`:
     - the evolved file and object file
     - the eval params and the run command for the stage-2 mode
     - the scientist's metric formula and gates
     - the method name, which must match `experiment.yaml`

     Keep the template's safety checks:
     - build failure -> null scores
     - source token audit + disassembly audit -> null scores
     - timeout, crash, NaN -> null scores
2. **Check:**
   - `seed_files` exist and each has exactly one balanced pair of markers:
     `grep -n "EVOLVE-BLOCK-" <file>`.
   - `primary_metric` is one of the evaluator's metrics.
   - The method and `postprocess` named in `experiment.yaml` exist in `evaluator.py`.
3. **Calibrate:**
   ```sh
   bmx python3 $X/evaluator.py | tee $X/results/baseline.json   # twice
   ```
   - Scores that don't depend on timing must match exactly; timing-based ones
     within a few percent.
   - Record the seed's scores in `$X/results/baseline.json` and in the
     Experiments table of `README.md`.
4. **Red-team.** Never do this in the repo:
   ```sh
   rsync -a --exclude .git --exclude build --exclude 'out*' ./ /tmp/canary_<name>/
   docker run --rm --user "$(id -u):$(id -g)" -v /tmp/canary_<name>:/work bubble-mxp python3 $X/evaluator.py
   ```
   Make one edit at a time in the copy's block. Each must come back with null
   scores, or, for the last one, a score below the seed's:
   - a `double` local
   - an unsuffixed literal such as `x * 0.1` on a float
   - an `#include`
   - an FP helper whose name lacks the audit tag and isn't inlined
   - skipping the work (output left untouched)

   Then delete `/tmp/canary_<name>`. Any cheat that scores means the rules are
   too loose: tighten them and go back to step 3.
5. **Gate 1, before any job.** The scientist reviews `experiment.yaml`,
   `evaluator.py`, the baseline and the red-team results, and confirms the job
   settings.
6. **Submit and monitor:**
   ```sh
   tar --exclude-vcs --exclude=build --exclude='out*' --exclude=dev/tools/RAPTOR \
       --exclude='*/results' -czf $X/results/bundle.tar.gz .
   <platform CLI> ... $X $X/results/bundle.tar.gz      # depends on the AlphaEvolve deployment
   ```
   Report the score history per generation. Point out jumps much larger than
   the trend for the scientist: they usually mean a loophole. When the job
   ends, `postprocess` writes the best program to `$X/results/best/src/...`.
7. **Review candidates.** For each top candidate:
   - show `diff` against the seed
   - re-score it in a scratch copy with the same evaluator
   - summarise what changed numerically

   Candidates stay in `$X/results/`, never in `src/`.
8. **Gate 2: the scientist picks one.** Apply it:
   ```sh
   diff -u src/<file>.cpp $X/results/best/src/<file>.cpp | less   # show the scientist first
   cp $X/results/best/src/<file>.cpp src/<file>.cpp
   ```
   Commit on the stage branch, then go to stage 4 (`dev/workflows/verify/plan.md`).

## Must not

- Edit `src/`, `tests/`, `Makefile` or `params/` (except applying the chosen
  candidate, and a new eval ini the scientist agreed to).
- Change a rule or weight after seeing job results without the scientist's
  agreement.
