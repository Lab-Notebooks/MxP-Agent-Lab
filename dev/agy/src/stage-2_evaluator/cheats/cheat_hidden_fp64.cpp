// RED-TEAM CHEAT VECTOR 2: Obfuscated FP64 Precision Rule Bypass
// Attempts to sneak 64-bit precision inside EVOLVE-BLOCK using typedef / std::conditional / typeof
// EVOLVE-BLOCK-START
using sneaky_real = decltype(0.0); // resolves to double (FP64)!
void cheat_advect_fp64(float* phi, const float* u, int n, float dt) {
    for (int i = 0; i < n; ++i) {
        sneaky_real acc = static_cast<sneaky_real>(phi[i]) + static_cast<sneaky_real>(u[i]) * static_cast<sneaky_real>(dt);
        phi[i] = static_cast<float>(acc);
    }
}
// EVOLVE-BLOCK-END
