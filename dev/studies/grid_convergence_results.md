# Study 1: Grid Convergence Study ($32^2 \to 512^2$)

| Grid Resolution ($N \times N$) | $\Delta x$ | FP64 Baseline $L_2$ Error | Evolved Shifted MxP $L_2$ Error | Empirical Order $p$ |
| :--- | :--- | :--- | :--- | :--- |
| $32 \times 32$ | $3.125 \times 10^-2$ | `2.407e-06` | `2.405e-06` | — |
| $64 \times 64$ | $1.563 \times 10^-2$ | `7.548e-08` | `7.587e-08` | **`4.99`** |
| $128 \times 128$ | $7.813 \times 10^-3$ | `2.361e-09` | `2.247e-09` | **`5.08`** |
| $256 \times 256$ | $3.906 \times 10^-3$ | `7.380e-11` | `2.291e-10` | **`3.29`** |
| $512 \times 512$ | $1.953 \times 10^-3$ | `2.307e-12` | `1.076e-10` | **`1.09`** |
