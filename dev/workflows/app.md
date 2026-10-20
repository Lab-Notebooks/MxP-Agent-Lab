# app/ spec

The stages run on any app/ that meets this spec. It fixes only what the stages touch, not the
science or the layout; most time-stepping solvers already meet it. Every stage checks it first
(protocol.md). Paths are relative to app/; raptorize's plan.md records the ones this app uses.

Parts:

    Makefile   app/Makefile; `make` and `make check` at the root delegate to it
    sources    the code, the kernel's file among them; raptorize and optimize edit it
    tests      run by `make check`; no stage edits them
    inputs     input files of the runs; raptorize may edit them
    tools      post-processing scripts; verify may use them

Roles in the code, as in raptorize/kernel.cpp:

    config   parsed options
    state    fields the run evolves
    step     advances the state by one dt
    loop     picks dt, calls the step, writes output
    kernel   the FP function to lower

Requirements, each followed by what needs it.

Build

1.  app/ is C++ in git; build and run output are gitignored. Raptorize starts from a clean
    `git status app` and records its diff.
2.  `make` and `make check` at the root build with -j and pass, natively and in bmx (the image
    holds every dependency). Every stage.
3.  The kernel's file compiles alone to its own object file, x86-64 code without LTO. The
    evaluator scans it with objdump.
4.  The kernel's file also builds with RAPTOR_CXX (clang), RAPTOR_CXXFLAGS and RAPTOR_LDLIBS from
    the root Makefile; `make RAPTOR=0` still builds. The RAPTOR plugin.

Run

5.  One command, run in app/ without a shell (no pipes, redirects or `VAR=` prefixes), optionally
    after `mpirun -n <ranks>`. It takes key=value options, including the output directory and a
    step cap, and accepts new keys. The eval run per mode.
6.  It creates the output directory, exits nonzero on failure, and runs a small case in under a
    minute. The evaluator deletes the directory first and stops at 600 s.
7.  It writes a CSV, a header row then numbers, one row per step or interval, into the output
    directory. Error columns, scores.
8.  Two fp64 runs of the same case give identical output. The "fp64 unchanged" gate, calibration.

Code

9.  Options parse into one copyable config, passed to the step. The precision option, shadow.
10. The state is copyable; everything else derives from it, the config and the time. Shadow.
11. The step takes dt from the loop and writes no output. Shadow, with the same dt.
12. The kernel is a free function on FP64 arrays passed as arguments, its FP callees in the same
    file; no globals, communication or I/O inside. RAPTOR truncation, the seed.
13. With MPI, the loop has a communicator to reduce over. Errors over ranks; verify reruns at
    another rank count.
