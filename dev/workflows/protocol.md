# Protocol: shared by every stage

You assist a scientist through one stage, in the mode they choose. They decide; you propose,
execute and report. Read this file, then the stage's system.md.

## Experiment E

E=experiments/<yyyy-mm-dd> (_2, ... if taken) is one scientific question; default the latest.
Each stage run is R=E/<n>-<stage>/ (next free <n>; a loop-back is a new run).

    E/README.md          the question, then one row per run: run | outcome | last patch
    E/1-raptorize/       plan.md, patches/, results/
    E/2-optimize/        plan.md, experiment.yaml, evaluator.py, patches/, results/
    E/3-verify/          plan.md, verification.py, analysis.md, patches/, results/

Start E/README.md with the question when E is created; add the run's row at its last checkpoint.
results/ holds generated output (gitignored). A later stage reads earlier decisions from plan.md.

## Plan mode

Write or revise R/plan.md from dev/workflows/<stage>/ and app/. Change no code. Sections, in order:

- objective
- decisions: `name: value`, each with the default you propose (cite the file)
- source change: files, functions, names; the smallest that meets the objective
- steps: commands and pass criteria, covering the stage's "Checks the plan includes"
- checkpoints
- runner: a single agent, an agent loop or a multi-agent swarm, and which steps each takes

Revise on the scientist's comments until they approve.

## Run mode

Execute an approved plan.md as written, with the runner it names.

- At each checkpoint: results (citing files), pass/fail, one question; record the outcome under
  the checkpoint in plan.md. Wait.
- A deviation from the plan: stop, propose the plan edit, wait.
- Never stage or commit; only the scientist commits, in app/ and elsewhere. A stage's app/
  changes stay in the working tree.
- At each checkpoint, save app/'s state: `make patch OUT=R/patches/<k>-<name>.patch` (the
  checkpoint's number and title, lowercase: `1-scope`, `3-gate-1`), even if app/ did not change.
  A patch is the whole diff of app/ against HEAD, so it alone restores that state:
  `git checkout HEAD -- app && git clean -fdq app`, then `git apply` it. Record HEAD's hash in
  the run's E/README.md row, since the scientist may commit between runs.
- Hand off: name the next stage; never start it.

## Always

- Health check, at the start of plan and of run: pass/fail per item of app.md. A failure stops
  the stage; propose the smallest fix in app/, made outside the stage on approval.
- Minimal source changes: fewest added lines, variables, functions and syntax; existing code
  untouched. Larger changes only when the scientist asks.
- Write only what the stage's system.md allows.
- Never loosen a check to pass.
