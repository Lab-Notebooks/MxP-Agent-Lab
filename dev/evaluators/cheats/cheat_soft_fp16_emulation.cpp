// Red-Team Cheat Vector 4: Software _Float16 emulation call trap (__extendhfsf2 / __truncsfhf2)
// Forces software libgcc _Float16 ABI calls and per-step heap churn inside hot loop.
extern "C" float __extendhfsf2(unsigned short a);
extern "C" unsigned short __truncsfhf2(float a);
// EVOLVE-BLOCK-START
float cheat_soft_fp16_step(float x) {
    volatile unsigned short h = __truncsfhf2(x);
    return __extendhfsf2(h);
}
// EVOLVE-BLOCK-END
