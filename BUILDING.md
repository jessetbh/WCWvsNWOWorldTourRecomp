# Building Guide

This documents what actually works today and how to reproduce it on this machine. The
full pipeline works end to end: recompile → build → **play the game** (boots, renders,
full matches with sound, keyboard + gamepad input, clean menus, and persistent saves
via emulated Controller Pak).

There are **two separate toolchains**, by design:
- **MinGW GCC** (`tools/env.ps1`) — used only to build the `N64Recomp` tool.
- **VS Build Tools 2022 / clang-cl** (`tools/env-msvc.ps1`) — used to build the port
  (RT64 is D3D12/Vulkan and needs clang-cl + the Windows SDK; MinGW can't substitute).

## 0. Toolchains (already installed on this machine)
- MinGW-w64 GCC 16.1.0 at `C:\Users\selki\toolchains\mingw64` (WinLibs UCRT, portable).
- VS Build Tools 2022: clang-cl 19.1.5, MSVC 14.44, CMake 3.31, Ninja, lld-link.
- Python 3.11 + splat64 in `disasm/.venv` (for disassembly).
- **For the patches build (Phase 4)** — both portable, no admin:
  - **Zig 0.14.0** at `C:\Users\selki\toolchains\zig-windows-x86_64-0.14.0`: `zig cc` is
    the MIPS cross-compiler. (Neither VS BuildTools' LLVM nor the official llvm.org
    *Windows* clang binaries include the MIPS backend — both are trimmed to x86/ARM.
    Zig bundles a full LLVM.)
  - **LLVM 19.1.5** (llvm.org release archive) at
    `C:\Users\selki\toolchains\clang+llvm-19.1.5-x86_64-pc-windows-msvc`: provides
    `ld.lld` for linking patches.elf (lld's MIPS support is unconditional) plus
    `llvm-readelf`/`llvm-objdump` for inspecting the MIPS ELF.

Reinstall references, if ever needed:
```powershell
# MinGW (portable, no admin): download WinLibs UCRT GCC zip, extract to C:\Users\<you>\toolchains
# Zig: https://ziglang.org/download/ zip, extract to toolchains
# LLVM: clang+llvm-<ver>-x86_64-pc-windows-msvc.tar.xz from llvm-project GitHub releases
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
so Display mode produces black/partial menu frames; **Controller Pak emulation** in
`si.cpp` + `save_read_ptr` in `pi.cpp` — WCW saves only to the pak, emulated over raw
joybus and backed by the standard recomp save file). **All of these are checked in as
`lib-patches/*.patch`** — after recloning `lib/`, run `.\lib-patches\apply.ps1`; after
changing anything under `lib/`, run `.\lib-patches\export.ps1` and commit. Manifest of
repo URLs + pinned commits: `lib-patches/README.md`.

## 4b. Patches build  (Phase 4 foundation — WORKS)
Game-behavior patches live in `patches/*.c` as C compiled to MIPS (`RECOMP_PATCH`
overrides a base-game function by its `syms/dump.toml` name; extern data resolves via
`syms/data_dump.toml`). The normal `cmake --build` drives the whole pipeline via the
`PatchesBin` target (zig cc → ld.lld → `N64Recomp patches.toml` → `file_to_c` →
PatchesLib); to run just the MIPS step:
```powershell
cd patches
C:\Users\selki\toolchains\mingw64\bin\mingw32-make.exe    # -> patches.elf
```
Gotchas (details in CLAUDE.md's "PATCHES BUILD" bullet):
- **Build twice after editing a patch** — the first build regenerates RecompiledPatches
  but links the previous table (same quirk BMHero documents).
- PatchesLib must stay listed **before** RecompiledFuncs in `target_link_libraries`;
  that ordering IS the patch mechanism.
- Any runtime API a patch calls needs an address entry in `patches/syms.ld` AND a real
  implementation in the link (librecomp `*_recomp` or `src/game/recomp_api.cpp`).

## 5. Run the game
```powershell
cd build-msvc
.\WCWRecompiled.exe               # must run with build-msvc as the working directory
```
It boots straight into the game: intro → attract match → wrestler cinematics → title,
looping, with sound; press Start (Enter / pad Start) to play. Saves persist to
`build-msvc\saves\<game id>.bin` (emulated Controller Pak). Useful env vars:
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
1. **Phase 4 enhancements** via `patches/` (the foundation — data symbols + patches
   build — is done and runtime-verified, 2026-07-05): widescreen, input options; real
   high-FPS interpolation additionally needs RT64 multi-workload frame detection +
   matrix-group tagging.
2. **Rendering polish** (deferred, still planned): crop the overscan-edge garbage rows
   (thin line at frame top).

Dropped permanently (2026-07-05): upstreaming the general runtime fixes — the drafts
in `upstream/` are kept as documentation only.

(Done and user-verified as of 2026-07-05: audio — RSPRecomp'd stock aspMain, see
`rsp/README.md`; keyboard + gamepad input; clean menus at Framerate=Original; saves
via emulated Controller Pak → `saves/<game id>.bin`.)
