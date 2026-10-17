# Stage 3, alphaevolve: write the evaluator, then run AlphaEvolve jobs

Turns the scientist's objective into an experiment, then runs AlphaEvolve
against it. Steps are in [`plan.md`](plan.md).

```text
dev/workflows/alphaevolve/
├── plan.md                    the steps
├── experiment.template.yaml   annotated experiment.yaml (prompt, rules, metric, seed files, evaluator)
├── evaluator.template.py      evaluator skeleton: build -> audit -> run -> score, postprocess
└── experiments/<name>/        written in this stage, one per objective
    ├── experiment.yaml
    ├── evaluator.py           method() -> {"scores": [{metric, score}], "insights": [{label, text}]}
    │                          postprocess(top_programs) -> results/best/<seed path>
    └── results/               baseline.json, bundle.tar.gz, best/   (gitignored)
```

## The evaluator contract

- **Inputs:** none. It runs from the repo root (or `BUBBLE_ROOT`) on the tree
  the platform materialised for one candidate.
- **Output:** a score for every metric, or None for all of them when the
  candidate failed to build, broke a rule, timed out, crashed, or produced
  NaN/Inf. Each insight is a short `{label, text}` that tells the model why.
- **Audit:** a token scan of each EVOLVE-BLOCK, plus a disassembly scan of every
  function whose name carries the audit tag (`LowPrec` by default) in the
  object file, looking for 64-bit FP instructions.
  - Known gap: an FP helper without the tag that the compiler doesn't inline
    isn't disassembled. The prompt must require the naming rule.
- **Determinism:** the same tree gives the same non-timing scores. Timing
  scores are measured within one run (low precision vs FP64 shadow), so the
  machine's speed cancels out.
- **Standalone:** `python3 evaluator.py` prints the JSON. Use this for the
  baseline and the red-team.

## Experiments

| name | seed files | primary metric | baseline |
| --- | --- | --- | --- |
| *(none yet: add one row per experiment)* | | | |
