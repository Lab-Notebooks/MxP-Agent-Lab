# E: 8-bit emulation of advect and redistance kernels

Question: can the WENO5 advection and level-set redistance kernels run in 8-bit
(RAPTOR-emulated) while the level set stays within a tolerable error of an FP64
shadow run, on the deforming bubble?

Runs:

| run | outcome | last patch | HEAD |
|-----|---------|------------|------|
| 1-raptorize | fp8 froze after ~20 steps (redistance rounded phi to fp8); RAPTOR=1 build with fp64/fp8e4m3/fp8e5m2/lowprec modes; advect and redistance raptorized | 2-diff.patch | (current) |
| 2-raptorize | loop-back: fp8 redistance truncates only its tendency, phi stays fp64; fp8 no longer frozen, splits like fp64 (err_rms 3.3e-2 at t = 2) | 3-results.patch | ce40ca6 |
| 3-raptorize | advectWeno only in fp8, redistance fp64: step-50 err_rms 9.1e-4 (50x below run 2); t = 2 err_rms 3.2e-2, same as run 2 | 3-results.patch | ce40ca6 |
| 4-raptorize | advectWeno only, interface error columns: step-50 fp8e4m3 err_rms 9.15e-4, err_band 1.32e-3, sign_mismatch 0; t = 2 err_band 2.0e-2, sign_mismatch 16% | 3-results.patch | ce40ca6 |
| 5-optimize | applied prog-amphibian-bat: err_band 4.799e-9 -> 2.653e-9 (score 8.319 -> 8.576); make check 17/17 | 5-gate-2.patch | ce40ca6 |
