#include "patches.h"

/*
 * Ray tracing experiment (exp/raytracing): hand RT64 the real camera.
 *
 * WCW composes every MVP on the CPU (object world matrix x D_80042C80, where
 * D_80042C80 = V x P is latched only by func_800016F8 from the camera setup,
 * func_80006860) and submits them via G_FORCEMTX, so RT64 never sees a projection and
 * its "world" transforms are clip-space MVPs. Ray tracing needs true world space.
 *
 * This patch writes the camera into the head of each frame's display list, right
 * after the frame-start commands func_80002B98 emits:
 *   gEXEnable                          (RT64 hook on G_SPNOOP; reset per DL by RT64)
 *   gEXSetViewMatrixFloat(view copy)
 *   gEXSetProjMatrixFloat(proj copy)
 *   gSPBranchList(past the data)       (the two float matrices live inline, so they
 *                                       share the DL buffer's lifetime)
 * On the RT64 side ([wcw rt] in rt64_rsp.cpp) a forceMatrix-only game's float camera
 * loads the projection stack when WCW_RT=1 (world = MVP * inv(VP)), and is consumed
 * and ignored otherwise, so default rendering is unchanged.
 *
 * func_8000329C (main segment, 0xEC bytes) is the per-frame render entry: it starts
 * the DL (func_80002B98), walks the scene into it (func_80002904) and finishes and
 * submits it (func_80002EAC). Reimplemented verbatim from disasm/asm/1050.s with the
 * camera block inserted.
 */

typedef struct {
    u32 w0;
    u32 w1;
} WcwGfx;

// RT64 extended GBI (lib/rt64/include/rt64_extended_gbi.h), F3DEX hook = G_SPNOOP (0x00).
#define WCW_RT64_EXTENDED_OPCODE 0x64
#define WCW_RT64_HOOK_MAGIC 0x525464
#define WCW_RT64_HOOK_OP_ENABLE 0x1
#define WCW_G_EX_SETPROJMATRIXFLOAT_V1 0x00002D
#define WCW_G_EX_SETVIEWMATRIXFLOAT_V1 0x00002E
#define WCW_G_DL 0x06
#define WCW_G_DL_NOPUSH 0x01

extern WcwGfx* func_80002B98(void* cam, s32 noClear);
extern s32 func_80002904(WcwGfx** gfx, s32 a1, s32 a2);
extern void func_80002EAC(void* cam, WcwGfx* gfx, s32 flag, s32 a3);

extern void* D_80041C70[][512];
extern s16 D_80041C6A;

// The camera's projection / view float matrices (written by func_80006860).
extern f32 D_80047B50[4][4];
extern f32 D_80047B90[4][4];

static WcwGfx* wcw_rt_emit_camera(WcwGfx* gfx) {
    // guPerspectiveF writes m[2][3] = -1; zero means no camera has been set up yet.
    if (D_80047B50[2][3] == 0.0f) {
        return gfx;
    }

    WcwGfx* data = gfx + 4;
    WcwGfx* after = data + 16; // two 64-byte float matrices
    f32* view = (f32*)data;
    f32* proj = view + 16;
    for (s32 j = 0; j < 4; j++) {
        for (s32 i = 0; i < 4; i++) {
            view[j * 4 + i] = D_80047B90[j][i];
            proj[j * 4 + i] = D_80047B50[j][i];
        }
    }

    // Mods rebuild the camera matrix after func_800016F8 (AkiMods' camera), so the view is
    // derived from it on the host (src/game/recomp_api.cpp): view = D_80042C80 x P^-1.
    recomp_wcw_rt_camera_view(view, proj);

    gfx[0].w0 = WCW_RT64_HOOK_MAGIC;
    gfx[0].w1 = (WCW_RT64_HOOK_OP_ENABLE << 28) | WCW_RT64_EXTENDED_OPCODE;
    gfx[1].w0 = (WCW_RT64_EXTENDED_OPCODE << 24) | WCW_G_EX_SETVIEWMATRIXFLOAT_V1;
    gfx[1].w1 = (u32)view;
    gfx[2].w0 = (WCW_RT64_EXTENDED_OPCODE << 24) | WCW_G_EX_SETPROJMATRIXFLOAT_V1;
    gfx[2].w1 = (u32)proj;
    gfx[3].w0 = (WCW_G_DL << 24) | (WCW_G_DL_NOPUSH << 16);
    gfx[3].w1 = (u32)after;
    return after;
}

RECOMP_PATCH s32 func_8000329C(void* cam, s32 a1, s32 a2) {
    s16 group = (s16)a1;
    s16 slot = (s16)a2;
    WcwGfx* gfx = func_80002B98(cam, (slot != 0) ? 0 : 1);
    gfx = wcw_rt_emit_camera(gfx);

    s32 first = *(s32*)D_80041C70[group][slot];
    s32 drawn = func_80002904(&gfx, group, slot);
    func_80002EAC(cam, gfx, (drawn < D_80041C6A) ? 0 : 1, (s16)first);
    return (s16)drawn;
}
