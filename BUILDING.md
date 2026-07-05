# Building Guide

This documents what actually works today and how to reproduce it on this machine. The
full pipeline works end to end: recompile → build → **run the game** (it boots, renders,
and plays its demo loop **with sound** — the RSP audio ucode is recompiled and working).

There are **two separate toolchains**, by design:
- **MinGW GCC** (`tools/env.ps1`) — used only to build the `N64Recomp` tool.
- **VS Build Tools 2022 / clang-cl** (`tools/env-msvc.ps1`) — used to build the port
  (RT64 is D3D12/Vulkan and needs clang-cl + the Windows SDK; MinGW can't substitute).

## 0. Toolchains (already installed on this machine)
- MinGW-w64 GCC 16.1.0 at `C:\Users\selki\toolchains\mingw64` (WinLibs UCRT, portable).
- VS Build Tools 2022: clang-cl 19.1.5, MSVC 14.44, CMake 3.31, Ninja, lld-link.
- Python 3.11 + splat64 in `disasm/.venv` (for disassembly).

Reinstall references, if ever needed:
```powershell
# MinGW (portable, no admin): download WinLibs UCRT GCC zip, extract to C:\Users\<you>\toolchains
# VS Build Tools (needs UAC):
winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Llvm.Clang --add Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset --add Microsoft.VisualStudio.Component.VC.CMake.Project --includeRecommended"
```

## 1. Provide the ROM
Place the NTSC-U ROM as `wcw.z64` in the repo root (SHA1
`5AD2D8359058C8BB71F08E3D3433B7A50D3BB645`). `disasm/wcw.z64` is a copy used by splat.
The code is **not** compressed, so no decompression step is needed.

## 2. Disassembly → symbols  (Phase 1/2 — WORKS)
The disassembly under `disasm/` (splat) is already generated. To regenerate symbols:
```powershell
.\disasm\.venv\Scripts\python.exe -m splat split disasm\wcw.yaml   # (re)disassemble
python .\tools\gen_symbols.py --overlays                            # -> syms\dump.toml
```
`gen_symbols.py` parses the splat output into N64Recomp's symbol-TOML format (we use
symbol-TOML mode, not ELF mode, since there's no MIPS assembler here). See `syms/README.md`.

## 3. Build N64Recomp + recompile the game  (Phase 2 — WORKS)
```powershell
. .\tools\env.ps1                 # MinGW GCC + CMake + Ninja on PATH
# N64Recomp.exe / RSPRecomp.exe are already built and copied to the repo root.
# To rebuild: cmake -S C:\Users\selki\depot\N64Recomp -B <build> -G Ninja ...; cmake --build <build>
.\tools\recompile.ps1             # gen symbols + run N64Recomp -> RecompiledFuncs\ (16 MB of C)
```

## 4. Build the port  (Phase 3 — WORKS)
```powershell
. .\tools\env-msvc.ps1            # clang-cl + MSVC + Windows SDK + CMake + Ninja
cmake -S . -B build-msvc -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release
cmake --build build-msvc --target WCWRecompiled
```
Links the whole stack (RecompiledFuncs + ultramodern + librecomp + recompui + recompinput
+ RT64 + SDL2) into `build-msvc\WCWRecompiled.exe`.

**IMPORTANT — `lib/` carries required local fixes.** The runtime libs in `lib/`
(N64ModernRuntime, RecompFrontend, rt64) are plain gitignored clones pinned to BMHero's
commits, plus local `[wcw fix]`-tagged patches that the game NEEDS to run (RT64 zero-VP
NaN guard in `rt64_rsp.cpp`; ultramodern external-message-pump drain, `osViSetEvent`
both-states, and retrace-reload guard in `mesgqueue.cpp`/`events.cpp`; audio buffer-depth
under-reporting in `audio.cpp`; RT64 present-queue locking + plume `copyTextureRegion`
null-guard; input poll in the raw-SI path — `si.cpp` calls the new
`ultramodern::input::poll_input()` since WCW never calls `osContStartReadData`, the normal
poller, so host input was never latched; Framerate default `Original` in recompui's
graphics tab — RT64 frame interpolation assumes 1 workload = 1 frame and WCW uses several,
so Display mode produces black/partial menu frames). If you ever reclone `lib/`, reapply
them (grep this repo's docs for `[wcw fix]`; full context in `disasm/libultra.md` and
`CLAUDE.md`).

## 5. Run the game
```powershell
cd build-msvc
.\WCWRecompiled.exe               # must run with build-msvc as the working directory
```
It boots straight into the game: intro → attract match → wrestler cinematics → title,
looping, with sound. Useful env vars:
- `WCW_SAMPLE=<seconds>` — dump all thread stacks N seconds in (stall diagnosis).
- `WCW_RDC_T=<seconds>` — when running under RenderDoc, when to fire the in-app
  multi-frame capture (default 8).
- `WCW_INSPECTOR=1` — auto-open RT64's frame inspector overlay.
- `WCW_PRESENT_LUM=1` — GPU readback of every presented frame →
  `build-msvc\wcw_present_lum.csv` + pipeline-timing CSVs (flicker/present debugging).
- `WCW_NO_INTERP=1` — disable RT64 interpolated-frame generation.

## 6. Fast iteration loop
```powershell
. .\tools\cycle.ps1               # regen symbols -> N64Recomp -> build -> run 20s -> symbolize crashes
```
Release crash frames symbolize against `build-msvc\WCWRecompiled.map` (nearest "Publics by
Value" symbol at/below the RVA; preferred base `0x140000000`) — `cycle.ps1` does this
automatically.

## 7. Standalone execution harness  (historical diagnostic)
A minimal harness that ran the recompiled boot/game-thread code before the real runtime
was wired (`run/README.md`). Superseded by the real port; kept for reference:
```powershell
. .\tools\env.ps1
.\run\build.ps1
.\run\wcw_harness.exe wcw.z64
```

## Remaining work (in order)
1. **Input verification.** Input is wired through recompinput but untested in-game
   (press Start on the title screen, navigate menus, start a match).
2. **Rendering/asset polish** as issues surface, then Phase-4 enhancements (widescreen,
   high-FPS interpolation, input options) via `patches/`.

(Audio is DONE — 2026-07-04: `rsp/wcw_audio.toml` → RSPRecomp → `rsp/wcw_audio.cpp`,
returned by `get_rsp_microcode` for M_AUDTASK; it's the stock aspMain, byte-identical to
BMHero's. See `rsp/README.md`.)
