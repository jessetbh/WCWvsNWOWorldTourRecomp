#include "patches.h"

/*
 * Required boot/runtime patches for WCW vs. nWo World Tour: Recompiled.
 *
 * Unlike BMHero, WCW needs no overlay-loader patch: librecomp's PI DMA path detects
 * DMAs into the two registered fixed-address overlay sections and loads them itself,
 * which is how the port has run since Phase 3. So this file currently holds only the
 * patches-pipeline verification patch below.
 *
 * Patches use RECOMP_PATCH to override an original function by name (names come from
 * syms/dump.toml; data symbols resolve against syms/data_dump.toml), and RECOMP_EXPORT
 * to expose a symbol to mods. See patches.h.
 */

/*
 * Pipeline verification patch: func_80000644 is a 3-instruction getter,
 *     `return D_8003CCF4;`   (D_8003CCF4 = a main-bss word, symbol in data_dump.toml)
 * called from overlay B (menus/title). The reimplementation is behavior-identical;
 * the one-shot marker proves the patched code (not the original recompiled copy) is
 * what actually runs. Remove once the first real patch lands.
 */
extern s32 D_8003CCF4;

RECOMP_PATCH s32 func_80000644(void) {
    static s32 announced = 0;
    if (!announced) {
        announced = 1;
        recomp_puts("[patches] func_80000644 patch is live\n", 38);
    }
    return D_8003CCF4;
}

/*
 * Second verification patch, guaranteed hot: func_80013BF0 is sqrtf (a leaf
 * `jr $ra; sqrt.s $f0,$f12`) with 9 call sites including the main-segment 3D
 * vector math, so it runs constantly during the attract match. Behavior-identical
 * (__builtin_sqrtf compiles to the same sqrt.s under -ffast-math).
 */
RECOMP_PATCH f32 func_80013BF0(f32 x) {
    static s32 announced = 0;
    if (!announced) {
        announced = 1;
        recomp_puts("[patches] func_80013BF0 (sqrtf) patch is live\n", 46);
    }
    return __builtin_sqrtf(x);
}
