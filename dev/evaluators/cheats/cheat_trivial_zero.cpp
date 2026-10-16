// RED-TEAM CHEAT VECTOR 3: Trivial Zero-Update / Frozen Field Degenerate Physics
// Attempts to return immediately without evolving the level-set interface.
// EVOLVE-BLOCK-START
void cheat_advect_zero(float* phi, int n) {
    return; // Zero-cost no-op
}
// EVOLVE-BLOCK-END
