# dev/: the programmable lab

Three stages take a kernel in `app/` from FP64 to a verified low-precision version. A scientist
starts each stage; an agent assists in two modes, **plan** (write `plan.md` together) and **run**
(execute it). See [`protocol.md`](workflows/protocol.md).

```text
app ──raptorize──▶ modes + seed ────optimize───▶ candidate ──verify──▶ merge
                                    ▲                          │
                                    └──── new constraints ◀────┘
```

| stage | does | changes | records in `R/` |
|---|---|---|---|
| [`raptorize/`](workflows/raptorize/) | adds precision modes, an FP64 shadow, an EVOLVE-BLOCK seed | `app/` | `plan.md`, `raptorize.patch` |
| [`optimize/`](workflows/optimize/) | evolves the seed against a score, applies one candidate | `app/src/` on apply | `plan.md`, `experiment.yaml`, `evaluator.py` |
| [`verify/`](workflows/verify/) | runs, checks and compares the candidate | nothing | `plan.md`, `verification.py`, `analysis.md` |

Each stage folder in `workflows/` holds everything about it:

- `system.md`: the contract (questions to ask, rules, checks, checkpoints, boundaries)
- templates, copied into `R/` and filled (`evaluator.py`, `experiment.yaml`, `verification.py`)
- code shapes (`raptorize/kernel.cpp-mc`) and code (`optimize/evolve.py`, `verify/verify.py`)

A stage is a class and `R=E/<n>-<stage>/` one instance, with `E=experiments/<yyyy-mm-dd>/`;
`E/README.md` indexes the runs. Layout: [`protocol.md`](workflows/protocol.md).

## Start a stage

Ask the agent, e.g. "plan raptorize". Commands:

```sh
make check                          # build and test app/
make image                          # docker image; then bmx (see AGENTS.md)
bmx make evaluate EXP=$R            # score the seed once (R = E/<n>-optimize)
bmx make evolve EXP=$R              # run AlphaEvolve
bmx make verify EXP=$R              # R = E/<n>-verify
```

The stages know only that the code lives in `app/` (`src/`, `tests/`, `params/`, `tools/`), that
`make` at the root builds it, and that its commands run in `app/`. `tools/RAPTOR/` is the
precision submodule.
