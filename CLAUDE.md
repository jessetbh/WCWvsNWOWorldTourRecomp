# CLAUDE.md — WCW vs. nWo World Tour: Recompiled

Contributor/agent instructions for this repo. The full development history (every
root-caused bug, evidence logs, decision records) lives in
[`docs/devlog.md`](docs/devlog.md); read it before re-investigating anything that
looks mysterious — odds are it's already been solved and documented there.

## What this project is

A native PC port of the Nintendo 64 game **WCW vs. nWo World Tour (USA)**, produced by
*statically recompiling* the original MIPS machine code into C with
[N64Recomp](https://github.com/N64Recomp/N64Recomp), running on N64ModernRuntime
(ultramodern + librecomp) with [RT64](https://github.com/rt64/rt64) as the renderer.
Not an emulator, not a decompilation; no game assets are distributed — the user
supplies their own ROM (US release, SHA1
`5AD2D8359058C8BB71F08E3D3433B7A50D3BB645`, entrypoint `0x80000400`, IDO compiler).

**Status: v0.1.0 public beta released 2026-07-07** —
https://github.com/jessetbh/WCWvsNWOWorldTourRecomp/releases. Fully playable:
boots, renders, full matches with sound, keyboard + gamepad input, menus, 4-player
local multiplayer, rumble, and working saves. Post-release: watch issues, friend
beta, then Phase-4 enhancements / mod support (`docs/mod-support-plan.md`). The
reference project for structure and conventions is **Bomberman Hero: Recompiled**
(BMHeroRecomp) — when in doubt, mirror it.

**Sister project**: WCW/nWo Revenge recomp bootstrap at
`C:\Users\selki\depot\WcwRevengeRecomp` (same engine family, same fork submodules —
its CLAUDE.md cross-references this repo's docs). Changes under `lib/` affect BOTH
projects: after the fork workflow below, update the submodule pin in each.

## Build

Two toolchains (both loaded by dot-sourcing, PowerShell):

| Script | Toolchain | Used for |
|---|---|---|
| `tools/env.ps1` | MinGW GCC + python | building N64Recomp, `gen_symbols.py`, recompiling |
| `tools/env-msvc.ps1` | VS Build Tools (clang-cl, CMake, Ninja) | building the port itself |

```powershell
# Full recompile (symbols -> N64Recomp -> RecompiledFuncs/)
. .\tools\env.ps1; .\tools\recompile.ps1

# Port build
. .\tools\env-msvc.ps1
cmake -S . -B build-msvc -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release
cmake --build build-msvc --target WCWRecompiled

# Fast iterate: regen symbols -> recompile -> build -> run -> symbolize any crash
. .\tools\cycle.ps1
```

Machine-local toolchain paths (MIPS-capable `zig cc`, `ld.lld`, `mingw32-make` for the
patches build) go in an uncommitted `local-config.cmake` at the repo root or on PATH —
see `BUILDING.md`.

## Hard-won invariants — do not break these

- **`lib/` = submodules of the jessetbh forks** (N64ModernRuntime, RecompFrontend,
  rt64; nested N64Recomp + plume forks inside them), each with the `[wcw fix]` set on
  a `wcw` branch. **After ANY edit under `lib/`: commit on that repo's `wcw` branch,
  push to the fork, bump the submodule pin in the superproject, and rerun
  `.\lib-patches\export.ps1`** (keeps the diff-vs-upstream record current).
- **libultra naming**: game functions are mapped to libultra names via the `RENAME`
  map in `tools/gen_symbols.py` (N64Recomp auto-ignores known names; the runtime
  provides them). Do **not** add them to `wcw.toml` `ignored`. Do **not** rename
  gu*/sinf/cosf math (guPerspectiveF etc.) — librecomp has no shims for those; they
  must stay recompiled. Evidence for every named function is in `disasm/libultra.md`.
- **Patch link order**: `PatchesLib` MUST precede `RecompiledFuncs` in
  `target_link_libraries` — patches win by static-lib symbol resolution order.
  After editing `patches/*.c`, **build twice** (first build links the stale table).
- **Framerate is locked to Original** (UI disabled + saved-config coerced). Frame
  interpolation warps geometry because WCW is G_FORCEMTX-only; the full analysis and
  a working-but-reverted implementation are at commit `53000eb`. Don't re-enable
  without game-side matrix-group patches.
- **Saves = emulated Controller Pak** (32 KB, port 1) inside librecomp `si.cpp`'s
  custom PIF/joybus emulation — WCW rolls its own raw-SI driver and saves ONLY to the
  pak. The same pak is a hybrid rumble/mempak identity state machine. Don't touch the
  bank-region semantics without reading the devlog section first.
- **Config loader gotcha**: `read_json_with_backups` falls back to `*.json.bak` — to
  test fresh defaults you must delete BOTH the json and the .bak.
- **PresentationMode must stay `Console`** — WCW builds each frame from multiple RSP
  tasks and pre-clears the next framebuffer; `PresentEarly` presents black frames.

## Diagnostics (env-gated, all in place)

`WCW_SAMPLE=<s>` all-thread stack dump at t+N; `WCW_PRESENT_LUM=1` per-present GPU
luminance readback + BMP dumps (`WCW_BMP_START`/`WCW_BMP_COUNT`/`WCW_LUM_END`);
`WCW_RDC_T=<s>` RenderDoc auto-capture time (renderdoccmd + headless qrenderdoc
analysis workflow in devlog); `WCW_NO_INTERP=1` disable RT64 interpolation;
`WCW_CROP=L,T,R,B` overscan crop override; `WCW_VPADS=<n>` attach SDL virtual pads;
`WCW_INPUT_LOG=1` per-channel input log; `WCW_INSPECTOR=1` RT64 frame inspector.
Crashes print symbolized backtraces (dbghelp + `/MAP`); resolve Release RVAs against
`build-msvc/WCWRecompiled.map`.

## Layout

```
wcw.toml / patches.toml   recompiler configs (game / patches)
WCWSyms/                  submodule: generated symbol TOMLs (dump.toml, data_dump.toml)
tools/gen_symbols.py      splat output -> symbol TOMLs; the RENAME map lives here
disasm/                   splat project + libultra.md (function-ID evidence log)
patches/                  C patches cross-compiled to MIPS (widescreen.c, ...)
rsp/                      RSPRecomp config + recompiled audio ucode
src/main, src/game        launcher wiring, recomp_api host functions
lib/ (gitignored)         runtime library clones; fixes tracked in lib-patches/
assets/                   tracked runtime UI assets (copied next to exe post-build)
docs/                     devlog.md, beta-release-plan.md, upstream/ bug write-ups
run/, cmake/coretest/     standalone execution harness + Stage-A link test (kept)
```

## Conventions

- C++ namespace for game glue: `wcw`. Generated dirs (`RecompiledFuncs/`,
  `RecompiledPatches/`, build dirs) are gitignored.
- **Never commit ROM data or extracted assets.**
- PowerShell syntax for shell commands on Windows.
- When adapting from BMHero: `BMHeroRecompiled`→`WCWRecompiled`, `banjo`/`bk`→`wcw`,
  `BMHeroSyms`→`WCWSyms`.
