#include "patches.h"

/*
 * Widescreen (Phase 4).
 *
 * WCW builds every 3D matrix on the CPU and submits only fully composed MVPs via
 * G_FORCEMTX, so RT64 can't widen the projection itself — the fix has to happen where
 * the projection is born. There is exactly ONE guPerspectiveF call in the whole game
 * (main segment, func_80006860 at 0x800068B0): the camera setup that builds
 *   P = guPerspectiveF(D_80047B50, &norm, fovy=33.0, aspect=4/3, near=100, far=4000, scale=1)
 *   V = guLookAtF(D_80047B90, eye, at, up=(0,1,0))
 * and hands both to the engine (func_800016F8) plus the perspNorm (func_80001724).
 *
 * This patch reimplements that function verbatim but asks the host for the target
 * aspect ratio (recomp_get_target_aspect_ratio → window ratio when the user picks
 * Aspect Ratio: Expand, else the original 4/3). With a wider aspect the projection
 * pre-squeezes X in NDC exactly the way RT64's Expand mode expects: full-width
 * perspective projections get the wide viewport (rt64_framebuffer_renderer.cpp,
 * useWideViewport), which stretches NDC back out — net effect: wider horizontal FOV
 * at correct proportions. 2D texrects are unaffected (RT64 keeps them 4:3-anchored).
 *
 * Function identification evidence (disasm/asm/1050.s):
 *  - func_8001BD90 = guPerspectiveF: guMtxIdentF call, fovy * (pi/180 double
 *    D_80034380) / 2, cot = cosf(func_8001CB30)/sinf(func_8001C970),
 *    m[0][0] = cot/aspect, m[1][1] = cot, m[2][3] = -1.0, perspNorm computation.
 *  - func_8001BFC0 = guPerspective (wraps it + guMtxF2L = func_80013210) — never
 *    called by the game.
 *  - func_8001C020 = guLookAtF: called right after with up = (0, 1, 0).
 * Do NOT add these names to gen_symbols RENAME: N64Recomp auto-ignores known
 * libultra names and librecomp has no gu* math shims — they must stay recompiled.
 */

// guPerspectiveF — O32: mf/perspNorm in a0/a1, fovy/aspect ride in a2/a3, rest stack
// (matches both the original call site and what clang emits for this prototype).
extern void func_8001BD90(f32 mf[4][4], u16* perspNorm, f32 fovy, f32 aspect,
                          f32 near, f32 far, f32 scale);
// guLookAtF
extern void func_8001C020(f32 mf[4][4], f32 xEye, f32 yEye, f32 zEye,
                          f32 xAt, f32 yAt, f32 zAt, f32 xUp, f32 yUp, f32 zUp);
// Engine: latch projection+view float matrices / perspNorm for this frame.
extern void func_800016F8(void* gfx, f32 projMf[4][4], f32 viewMf[4][4]);
extern void func_80001724(void* gfx, u16 perspNorm);

// The game's projection / view float-matrix buffers (main bss).
extern f32 D_80047B50[4][4];
extern f32 D_80047B90[4][4];

RECOMP_PATCH void func_80006860(void* gfx, f32 xEye, f32 yEye, f32 zEye,
                                f32 xAt, f32 yAt, f32 zAt) {
    u16 perspNorm;
    f32 aspect = recomp_get_target_aspect_ratio(4.0f / 3.0f);

    func_8001BD90(D_80047B50, &perspNorm, 33.0f, aspect, 100.0f, 4000.0f, 1.0f);
    func_8001C020(D_80047B90, xEye, yEye, zEye, xAt, yAt, zAt, 0.0f, 1.0f, 0.0f);
    func_800016F8(gfx, D_80047B50, D_80047B90);
    func_80001724(gfx, perspNorm);
}
