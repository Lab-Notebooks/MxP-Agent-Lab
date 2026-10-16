// RED-TEAM CHEAT VECTOR 1: Cached / Precomputed Output Table Bypass
// Attempts to skip stencil physics and copy initial/cached state to game wall-clock speed.
// EVOLVE-BLOCK-START
void cheat_advect_cached(float* phi, const float* phi_init_cache, int n) {
    static bool initialized = false;
    for (int i = 0; i < n; ++i) {
        phi[i] = phi_init_cache[i]; // Bypass time integration
    }
}
// EVOLVE-BLOCK-END
