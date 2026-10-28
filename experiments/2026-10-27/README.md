# E: bf16 WENO5 advection of a splitting level-set bubble

Question: can the WENO5 advection kernel (app/src/advect.cpp) run in bf16 (RAPTOR-truncated)
while the level set stays within a tolerable error of an FP64 shadow run, on the splitting bubble?

Runs:

| run | outcome | last patch | HEAD |
|-----|---------|------------|------|
| 1-raptorize | RAPTOR=1 build with fp64/fp32/bf16/lowprec modes; bf16 truncates advectWeno | 2-diff.patch | 060106ec1127d075a3e98775647b7d2678852bac |
| 2-optimize | baseline only: score 3.775, final spatial err_rms 1.679e-4; stopped before creating the GCP experiment (credentials removed) | (none) | 060106ec1127d075a3e98775647b7d2678852bac |
