# Formal Error & Stability Proof: WENO5 Advection Kernel (`src/advect.cpp`)

## 1. Kernel Specification & Zero-FP64 Invariant
The evolved kernel `wenoFaceValueLowPrec` and `advectWenoLowPrec` inside `src/advect.cpp` (`// EVOLVE-BLOCK-START ... // EVOLVE-BLOCK-END`) computes 5th-order Weighted Essentially Non-Oscillatory (WENO5) numerical fluxes on a staggered MAC grid using **exclusively 16-bit (`_Float16`) and 32-bit (`float`)** arithmetic (`fp64_asm_ops = 0`).

### Theorem 1.1 (Translation Invariance & Shift-by-$s_3$ Cancellation Elimination)
Let $s_1, s_2, s_3, s_4, s_5$ be the 5-point stencil values represented as Ozaki/Dekker float-float pairs $\hat{s}_k = (s_k^{\text{hi}}, s_k^{\text{lo}}) \in \mathbb{F}_{32} \times \mathbb{F}_{32}$ with $|s_k^{\text{lo}}| \le \frac{1}{2}\text{ulp}(s_k^{\text{hi}})$. Define the center-shifted stencil differences:
$$v_k = \text{ff\_sub}(\hat{s}_k, \hat{s}_3), \qquad k \in \{1, 2, 4, 5\}.$$

1. **Exactness of Shift**: By Sterbenz's Lemma and Knuth's `TwoSum` theorem (`two_sum`), for neighboring grid cells where $\frac{1}{2} \le s_k^{\text{hi}} / s_3^{\text{hi}} \le 2$, the leading subtraction $s_k^{\text{hi}} - s_3^{\text{hi}}$ is **exact** in IEEE-754 binary32 (`e = 0`), and the full float-float difference $v_k = \hat{s}_k - \hat{s}_3$ satisfies:
   $$|v_k - (s_k - s_3)| \le 2 u_{32}^2 (|s_k| + |s_3|) = O(u_{32}^2), \qquad u_{32} = 2^{-24} \approx 5.96 \times 10^{-8}.$$
   Empirically, LLNL `FPChecker` (`dev/reports/fpchecker_traces/fpc_0.json`) recorded **3,840 catastrophic cancellations per 10 timesteps** at `src_ref/advect.cpp:13,14,16` (`s1 - 2*s2 + s3`, `s1 - 4*s2 + 3*s3`) in the unshifted formulation. Center-shifting by $s_3$ reduces the operand magnitude from $O(1)$ to $O(\Delta x)$ prior to linear combination, eliminating leading-order cancellation.

2. **Polynomial Reconstruction Identity**: The three candidate 3rd-order stencil polynomials $p_1, p_2, p_3$ satisfy the exact algebraic identity:
   $$p_m(s_1,\dots,s_5) = s_3 + \frac{1}{6}\widehat{df}_{6,m}(v_1, v_2, v_4, v_5), \qquad m \in \{1, 2, 3\},$$
   where:
   - For $u_{i+1/2} > 0$: $\widehat{df}_{6,1} = 2v_1 - 7v_2$, $\widehat{df}_{6,2} = 2v_4 - v_2$, $\widehat{df}_{6,3} = 5v_4 - v_5$.
   - For $u_{i+1/2} \le 0$: $\widehat{df}_{6,1} = 5v_2 - v_1$, $\widehat{df}_{6,2} = 2v_2 - v_4$, $\widehat{df}_{6,3} = 2v_5 - 7v_4$.
   Since $\sum_{m=1}^3 w_m = 1$, the reconstructed face value is:
   $$f_{i+1/2} = s_3 + \frac{\sum_{m=1}^3 \tilde{\alpha}_m \widehat{df}_{6,m}}{6 \sum_{m=1}^3 \tilde{\alpha}_m}.$$
   This factors out all three intermediate divisions by $6$ and all three weight normalizations $\alpha_m / \sum \alpha_j$ into a **single float-float division** at the end of the kernel.

---

### Theorem 1.2 (Exact Power-of-Two Scaling $S = 2^{32}$ Prevents Subnormal Underflow)
In the Jiang-Shu smoothness indicators $\beta_m = \frac{13}{12}(d_{m,a})^2 + \frac{1}{4}(d_{m,b})^2$, smooth regions satisfy $\beta_m = O(\Delta x^4)$ (or $\beta_m = 0$ outside the narrow band). With $\epsilon = 10^{-15}$, the unscaled denominator $(\epsilon + \beta_m)^2 \approx 10^{-30}$ has an error term in `ff_sqr` of magnitude $\sim 10^{-30} \times 2^{-24} \approx 6 \times 10^{-38} < 1.175 \times 10^{-38}$, which underflows into IEEE-754 binary32 **subnormal (`denormal`)** range and triggers 150-cycle x86 microcode traps.

By scaling $e_m = \epsilon + \beta_m$ by the exact power of two $S = 2^{32} = 4294967296.0\text{f}$ prior to squaring:
$$\tilde{D}_m = \big(2^{32}(\epsilon + \beta_m)\big)^2 = 2^{64}(\epsilon + \beta_m)^2 \ge 1.84 \times 10^{-11} \gg 1.175 \times 10^{-38},$$
multiplication by $2^{32}$ only increments the binary32 exponent field by $+32$ with **zero mantissa rounding error**, and the common factor $2^{-64}$ in $\tilde{\alpha}_m = \frac{10 d_m}{\tilde{D}_m}$ cancels identically in the homogeneous ratio $\frac{\sum \tilde{\alpha}_m \widehat{df}_{6,m}}{6 \sum \tilde{\alpha}_m}$.

---

### Empirical Verification (`test_weno_order`)
- **32×32**: $L_1 = 3.901702 \times 10^{-3}$, $L_\infty = 5.389357 \times 10^{-2}$
- **64×64**: $L_1 = 1.306375 \times 10^{-4}$, $L_\infty = 1.805703 \times 10^{-3}$ ($p_{L_1} = 4.90$, $p_{L_\infty} = 4.90$)
- **128×128**: $L_1 = 3.941136 \times 10^{-6}$, $L_\infty = 6.359460 \times 10^{-5}$ ($p_{L_1} = 5.05 \ge 4.50$, $p_{L_\infty} = 4.83 \ge 4.50$)
- **Disassembly Audit (`objdump -d -C build/advect.o`)**: `fp64_asm_ops = 0`, `_Float16` tokens = 8.
