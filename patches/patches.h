#ifndef __PATCHES_H__
#define __PATCHES_H__

// Section attributes the patch recompiler keys on (mirrors BMHero patches.h):
// RECOMP_PATCH replaces the base-game function with the same name; RECOMP_FORCE_PATCH
// does so even if the recompiler thinks it's unsafe; RECOMP_EXPORT exposes a function
// to mods; RECOMP_DECLARE_EVENT declares a mod-visible event hook.
#define RECOMP_EXPORT __attribute__((section(".recomp_export")))
#define RECOMP_PATCH __attribute__((section(".recomp_patch")))
#define RECOMP_FORCE_PATCH __attribute__((section(".recomp_force_patch")))
#define RECOMP_DECLARE_EVENT(func) \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"") \
    __attribute__((noinline, weak, used, section(".recomp_event"))) void func {} \
    _Pragma("GCC diagnostic pop")

#include "patch_helpers.h"

// Runtime API bridges (addresses assigned in syms.ld; the recompiler routes calls to
// the matching *_recomp / recomp_* runtime function).
void recomp_puts(const char* data, u32 size);
// Returns the aspect ratio to render 3D at: `original` when the user's Aspect Ratio
// setting is Original, else the window's ratio (never below `original`).
f32 recomp_get_target_aspect_ratio(f32 original);
// Overwrites the 4x4 float `view` with the camera matrix (D_80042C80) x inverse(`proj`),
// so view x proj matches the matrix the game draws with even when a mod rebuilt it.
void recomp_wcw_rt_camera_view(f32* view, const f32* proj);
// The game's per-object view cull (func_8000252C) with the horizontal limit scaled to the screen width:
// 1 = draw the object, 0 = culled. See patches/widescreen_cull.c.
s32 recomp_wcw_cull_visible(void* node);

// Project-wide patch declarations for WCW vs. nWo World Tour: Recompiled.
// Add shared extern declarations for game functions/data you patch here.

#endif
