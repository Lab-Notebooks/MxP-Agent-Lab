# 2026-10-20

Question: can the WENO5 advection kernel (app/src/advect.cpp) run below FP64 while the level set
stays within a tolerable error of an FP64 run?

| run | outcome | commit |
|-----|---------|--------|
| 1-raptorize | pass: modes fp64/fp32/bf16/lowprec; fp32 = lowprec; bf16 ~400x fp32 (RMS) at t = 2 | uncommitted (scientist commits) |
| 2-optimize | pass: exp-ginger-chimpanzee, 10 programs, best 9.388 vs seed 9.272; applied humongous-macaw (t = 2: 1.17x lower RMS error); 2 evolve.py fixes | 05e8cfa + patches/5-gate-2.patch |
| 3-verify | 19/19 PASS: 1.31x (eval), 1.27x (256²), 2.73x before t = 1.45, 1.17x to t = 2; gate pending | 05e8cfa + patches/3-gate.patch |
