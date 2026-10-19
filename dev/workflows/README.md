# Staged workflows

The workflows know only this: the code lives in `app/` (`src/`, `tests/`, `params/`, `tools/`),
`make` at the root builds it, and its commands run in `app/`. Each stage is started by a
scientist, ends at a signed-off gate, and records into `<stage>/runs/<name>/`, starting with the
scientist's answers in `decisions.md`.

The agent running a stage follows [`system.md`](system.md) (shared protocol), then the stage's
`system.md` (preconditions, questions, checkpoints, boundaries) and `plan.md` (rules, steps).

| stage | changes | records |
|---|---|---|
| [`raptorize`](raptorize/) | `app/`: precision modes, FP64 shadow, EVOLVE-BLOCK seed | `raptorize.patch`, `kernel.yaml` |
| [`alphaevolve`](alphaevolve/) | `app/src/` only when applying a candidate | `experiment.yaml`, `evaluator.py`, `results/` |
| [`verify`](verify/) | nothing | `verification.yaml`, `check_*.py`, `analysis.md` |

```text
app ──raptorize──▶ modes + seed ──alphaevolve──▶ candidate ──verify──▶ merge
                                    ▲                          │
                                    └──── new constraints ◀────┘
```

Shared code: `dev/tools/RAPTOR/`, `dev/tools/local/evolve.py` (`Evaluator`),
`dev/tools/local/verify.py` (`Verification`). Container: `make image`, then `bmx` (see `AGENTS.md`).
