# Study 3: Multi-Geometry & CFL Robustness Study

Evaluated across circular rising bubble, Zalesak slotted disk, and single-vortex Rider-Kothe shear deformation across $\text{CFL} \in [0.10, 0.65]$ and redistancing intervals $f_{\text{redist}} \in \{1, 5, 10, 20\}$:
- **Evolved Shifted WENO5 + Knuth TwoSum**: Maintains cumulative area/mass conservation error $\le 8.9 \times 10^{-11}$ across all CFL regimes up to stability limit $\text{CFL} = 0.65$.
