#include "patches.h"

/*
 * Widescreen cull (aki-roadmap#47 / #5).
 *
 * func_8000252C is the game's per-object view cull: it returns 0 (culled) when an object's center is past
 * |x| / (depth + 150) > 1.5 or |y| / (depth + 150) > 1.5 in view space. That is about 3.8x the 4:3 screen
 * edge, but with Aspect Ratio = Expand RT64 shows a wider view, and large set pieces whose center is past the
 * line while their geometry reaches into the extra width were culled at the screen edges (edge pop-in).
 *
 * The test runs on the host (recomp_wcw_cull_visible, src/game/recomp_api.cpp): the original with only the
 * horizontal limit scaled to the screen width, unchanged at Original. Not done here because the patch
 * toolchain puts floats in odd FPU registers, which N64Recomp's FR=0 model splits from the arithmetic that
 * reads them (the same reason as recomp_wcw_rt_camera_view).
 */
RECOMP_PATCH s32 func_8000252C(void *node) {
    return recomp_wcw_cull_visible(node);
}
