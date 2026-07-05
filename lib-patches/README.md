# lib-patches/ — required local fixes for the gitignored `lib/` clones

The runtime libraries in `lib/` (N64ModernRuntime, RecompFrontend, rt64) are **plain
gitignored clones**, pinned to the same commits BMHero uses, **plus local `[wcw fix]`
patches the game NEEDS to boot, render, hear, and take input**. This directory checks
those patches into the repo so a reclone of `lib/` can't silently lose them.

Regenerate after changing anything under `lib/`: `.\lib-patches\export.ps1`
Reapply onto fresh clones: `.\lib-patches\apply.ps1`

## Manifest (base commits the patches apply onto)

| Repo | URL | Base commit | Patch |
|------|-----|-------------|-------|
| `lib/N64ModernRuntime` | https://github.com/N64Recomp/N64ModernRuntime.git | `ca568b6ad79b9029d14077f0c3ffa757727c5559` | `N64ModernRuntime.patch` |
| `lib/N64ModernRuntime/N64Recomp` (submodule) | (recursive submodule) | `2b6f05688de2abc7d86da5b4a89b84c2c6acbabe` | `N64ModernRuntime-N64Recomp.patch` |
| `lib/RecompFrontend` | https://github.com/N64Recomp/RecompFrontend.git | `b3b7ebb4ec1a8a763c0191486f1b3329f9499a48` | `RecompFrontend.patch` |
| `lib/rt64` | https://github.com/rt64/rt64.git | `f647df1a084ae67897dba9806c0d467aa0852894` | `rt64.patch` |
| `lib/rt64/src/contrib/plume` (submodule) | (recursive submodule) | `51b1ad443b9f202c5cfc930ae25345d3f2ba7716` | `rt64-plume.patch` |

Clone with `git clone --recursive <url> <dir> && git -C <dir> checkout --recurse-submodules <commit>`.

## What the patches contain (full context: `CLAUDE.md`, `disasm/libultra.md`)

- **N64ModernRuntime**: external-message-pump drain fix (deadlock; upstream-relevant),
  `osViSetEvent` both-ViStates + retrace-reload guard, audio buffer-depth under-reporting
  (`buffer_offset_frames`), 4 IDO softfloat `_recomp` shims, **new `librecomp/src/si.cpp`**
  (raw-SI/PIF joybus emulation for WCW's homegrown controller layer: controller reads,
  input-poll hook `ultramodern::input::poll_input()`, and **32 KB Controller Pak emulation**
  — WCW's only save medium — backed by the save buffer via a new `save_read_ptr` in
  `pi.cpp`), assorted `[wcw]` diagnostics (pi/events/mesgqueue).
- **N64ModernRuntime-N64Recomp**: `include/recomp.h` declares the `__osSiRawStartDma_recomp`
  / `__osSiDeviceBusy_recomp` shims (implemented in the new si.cpp; needed by every
  recompiled TU since these names are in N64Recomp's ignored set).
- **RecompFrontend**: Framerate default `Original` (RT64 interpolation assumes 1 workload =
  1 frame; WCW uses several → black/partial menu frames), plus render-context/ui-state edits.
- **rt64**: zero-VP→identity guard in `rt64_rsp.cpp` (G_FORCEMTX-only games; upstream-
  relevant), present-queue workloadMutex locking + `WCW_PRESENT_LUM` swapchain-readback
  diagnostics (`WCW_BMP_START`/`WCW_BMP_COUNT`/`WCW_LUM_END`), `WCW_NO_INTERP` gate,
  enhancement default PresentationMode Console, gfx action-queue drain.
- **rt64-plume**: `plume_d3d12.cpp` `copyTextureRegion` null-guard for buffer destinations
  (upstream bug; crashes any texture→buffer readback).

## Notes
- Patches are exact snapshots of the working trees (verified by reverse-apply at export).
- If a patch no longer applies after moving a lib to a newer commit, resolve manually and
  re-export; consider upstreaming the general fixes so this directory shrinks.
