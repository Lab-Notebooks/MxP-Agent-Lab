# E: splitting bubble, raptorized advection

Question: change the Deforming Bubble problem so the bubble splits under a
2x2 vortex cell, then raptorize the WENO5 advection kernel for mixed precision.

Runs:

| run | outcome | last patch | HEAD |
|-----|---------|------------|------|
| 1-raptorize | PASS: make check 17/17 (native + bmx), RAPTOR=0/1; fp64 err 0; fp32 2.94e-10/2.12e-9; bf16 2.77e-5/2.13e-4; lowprec=fp32 | 3-results.patch | 060106ec1127d075a3e98775647b7d2678852bac |
