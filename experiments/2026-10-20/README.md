# 2026-10-20

Question: can the WENO5 advection kernel (app/src/advect.cpp) run below FP64 while the level set
stays within a tolerable error of an FP64 run?

| run | outcome | commit |
|-----|---------|--------|
| 1-raptorize | pass: modes fp64/fp32/bf16/lowprec; fp32 = lowprec; bf16 ~400x fp32 (RMS) at t = 2 | uncommitted (scientist commits) |
| 2-optimize | plan written, calibrated (9.272), red-teamed; Gate 1 pending; app restored from raptorize.patch; launch blocked on GCP credentials | |
