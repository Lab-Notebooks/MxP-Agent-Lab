# Mixed-precision comparison for src/advect.cpp

One build, one run, one plot:

```sh
make                                                   # ./bubble, advect.cpp built with the RAPTOR pass
mpirun -np 4 ./bubble params/bubble.ini precision=lowprec outdir=out
python3 tools/plot_history.py out                      # -> out/history_precision.png (+ the usual plots)
python3 tools/plot_fields.py out                       # phi = 0 contour, fp64 shadow dashed on top
```

`precision` (params/*.ini or `key=value` on the command line) selects how the
WENO5 advection kernel runs; everything else stays FP64:

| mode | kernel | needs |
| --- | --- | --- |
| `fp64` | reference `wenoFaceValue` (no shadow, err columns are 0) | - |
| `lowprec` | native `advectWenoLowPrec` (float / `_Float16`), the EVOLVE-BLOCK | - |
| `fp32` `tf32` `bf16` `fp16` | `wenoFaceValue` with every op truncated by RAPTOR (MPFR, slow) | RAPTOR build |

Any mode other than `fp64` advances an FP64 shadow copy of the level set through
the same steps, so the comparison needs no separate reference run:

- `history.csv` gains `err_l2` (RMS over the domain), `err_linf`,
  `err_band_l2` (RMS over |phi_fp64| <= 5h) and `area_ref`.
- each `.vti` gains `dfun_ref` and `err = dfun - dfun_ref`.
- the run log reports trajectory RMS / final RMS / max Linf, the area drift of
  both trajectories, and the advection wall time of each kernel (speedup).

Build notes:
- The Makefile uses the RAPTOR pass whenever `$(RAPTOR_INSTALL_DIR)/lib/LLVMRaptor-$(LLVM_VER).so`
  exists (defaults `~/raptor-install`, 20). `make RAPTOR=0` builds advect.cpp with
  `mpicxx` instead; then only `fp64` and `lowprec` run (and g++ < 12 has no `_Float16`).
- RAPTOR emulation runs ~10^2-10^3x slower than native; use `max_steps=` for quick looks.
