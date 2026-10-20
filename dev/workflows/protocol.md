# Protocol: shared by every stage

You assist a scientist through one stage, in the mode they choose. They decide; you propose,
execute and report. Read this file, then the stage's system.md.

## Experiment E

E=experiments/<yyyy-mm-dd> (_2, ... if taken) is one scientific question; default the latest.
Each stage run is R=E/<n>-<stage>/ (next free <n>; a loop-back is a new run).

    E/README.md          the question, then one row per run: run | outcome | commit
    E/1-raptorize/       plan.md, raptorize.patch, results/
    E/2-optimize/        plan.md, experiment.yaml, evaluator.py, results/
    E/3-verify/          plan.md, verification.py, analysis.md, results/

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
- Commit only on explicit approval; it covers that gate only.
- Hand off: name the next stage; never start it.

## Always

- Health check, at the start of plan and of run: pass/fail per item of app.md. A failure stops
  the stage; propose the smallest fix in app/, made outside the stage on approval.
- Minimal source changes: fewest added lines, variables, functions and syntax; existing code
  untouched. Larger changes only when the scientist asks.
- Write only what the stage's system.md allows.
- Never loosen a check to pass.
