# Study 2: Precision Conversion Study (`FP64` down to `FP8` vs. Evolved MxP)

| Precision / Kernel Variant | Mantissa Bits | 20,000-Step Trajectory RMSE | Throughput Speedup | Invariant Gate ($\le 10^{-7}$) |
| :--- | :---: | :---: | :---: | :---: |
| **FP64 Baseline** | 53 bits | `0.00e+00` (Ref) | `1.00×` | **PASS** |
| **Evolved Best (Shifted WENO5 + Knuth TwoSum)** | **24+24 bits (EFT)** | **`3.09e-11`** | **`2.18×`** | **PASS (>3,000× margin)** |
| **Seed MxP (Uncompensated FP32 Flux)** | 24 bits | `4.18e-06` | `1.84×` | FAIL (Secular drift) |
| **Naive FP32** | 24 bits | `1.92e-04` | `1.95×` | FAIL (Smoothness cancellation) |
| **TF32 (10-bit Mantissa)** | 10 bits | `6.40e-03` | `3.10×` | FAIL |
| **FP16 (IEEE Half)** | 10 bits | `8.15e-03` | `3.40×` | FAIL (Underflow in $IS_k$) |
| **BF16 (Brain Float)** | 7 bits | `4.82e-02` | `3.35×` | FAIL |
| **FP8 (E4M3)** | 3 bits | `2.95e-01` | `4.80×` | FAIL (Interface loss) |
