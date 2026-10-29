# dev/: the programmable lab

Three stages take a kernel in app/ from FP64 to a verified low-precision version. A scientist
starts each stage; an agent assists in two modes: plan (write plan.md together) and run (execute
it). See workflows/protocol.md.

    app ─raptorize─▶ modes + seed ─optimize─▶ candidate ─verify─▶ merge
                                 ▲                         │
                                 └──── new constraints ◀───┘

Stages, in workflows/:

- raptorize: adds precision modes, an FP64 shadow, an EVOLVE-BLOCK seed; changes app/.
- optimize: evolves the seed against a score; applies one candidate to the kernel's file.
- verify: runs, checks and compares the candidate; changes nothing.

Stages run on any app/ that meets workflows/app.md. tools/RAPTOR/ is the precision submodule.

A stage folder holds:

- system.md: the contract (questions, rules, checks, checkpoints, boundaries)
- templates, copied into R/ and filled: evaluator.py, experiment.yaml, verification.py
- worked examples (raptorize/kernel.cpp) and code (optimize/evolve.py, verify/verify.py)

A stage is a class, R=E/<n>-<stage>/ one instance, E=experiments/<yyyy-mm-dd>/; what each run
records is in workflows/protocol.md.

## Start a stage

Ask the agent, e.g. "plan raptorize". Commands:

    make check                      # build and test app/
    make image                      # docker image; then bmx (see AGENTS.md)
    cp dev/workflows/optimize/credentials.yaml credentials.yaml   # once; fill in, chmod 600
    bmx make evaluate EXP=$R        # score the seed once (R = E/<n>-optimize)
    bmx make evolve EXP=$R          # run AlphaEvolve
    bmx make verify EXP=$R          # R = E/<n>-verify
