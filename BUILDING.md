# Building Guide

**Building is NOT required to play** — download a release zip instead (see the
README). This guide is for contributors building from source on Windows (the beta
is Windows-only; CI in `.github/workflows/validate.yml` runs these exact steps).

The pipeline has two halves, each with its own toolchain role:

1. **Recompile** — run `N64Recomp` on your ROM to generate `RecompiledFuncs/`
   (~16 MB of C), and `RSPRecomp` to generate the audio-microcode CPU translation.
2. **Port build** — compile the generated C plus the runtime stack
   (ultramodern + librecomp + RecompFrontend + RT64) with **clang-cl** into
   `WCWRecompiled.exe`.

## 1. Prerequisites

- **Git**
- **VS Build Tools 2022** with clang-cl, CMake, and Ninja (needs UAC):

  ```powershell
  winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Llvm.Clang --add Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset --add Microsoft.VisualStudio.Component.VC.CMake.Project --includeRecommended"
  ```

  RT64 is D3D12/Vulkan and needs clang-cl + the Windows SDK; MinGW cannot
  substitute for the port build.
- **A MIPS-capable C compiler** for the `patches/` build. The VS BuildTools clang
  and the llvm.org Windows *installer* are trimmed to x86/ARM backends, so use
  **either**:
  - the clang inside the **portable llvm.org release archive**
    (`LLVM-<ver>-Windows-X64.tar.xz` from
    [llvm-project releases](https://github.com/llvm/llvm-project/releases) — the
    tar.xz ships the full backend set; verified with 19.1.3, and it's what CI
    uses), **or**
  - **Zig** (`zig cc`; Zig bundles a full LLVM) — portable zip from
    [ziglang.org/download](https://ziglang.org/download/).
- **ld.lld** — any LLVM build works (MIPS support in lld is unconditional). VS
  Build Tools already ships one at `VC\Tools\Llvm\x64\bin\ld.lld.exe`.
- **GNU make** — e.g. `mingw32-make` from a portable
  [WinLibs](https://winlibs.com/) MinGW zip, or any other `make` on PATH.
- *(Optional — regenerating symbols only)* Python 3 with a splat venv, see §7.

## 2. Clone

```powershell
git clone --recursive https://github.com/jessetbh/WCWvsNWOWorldTourRecomp.git
cd WCWvsNWOWorldTourRecomp
```

`--recursive` matters: `lib/` contains the runtime-stack forks (with required
`[wcw fix]` changes — see `lib-patches/README.md` for the diff-vs-upstream record)
and `WCWSyms/` contains the generated symbol TOMLs.

**Windows path-length warning**: the nested submodule tree produces long internal
git paths, and cloning into a deep directory (OneDrive Documents, etc.) can fail
with `Filename too long`. Either clone into a short path (e.g. `C:\src\`) or enable
long-path support first:

```powershell
git config --global core.longpaths true
```

## 3. Provide the ROM

Place the **WCW vs. nWo World Tour (USA)** ROM as `wcw.z64` in the repo root —
SHA1 `5AD2D8359058C8BB71F08E3D3433B7A50D3BB645`, big-endian. It is used only by
the recompiler steps below; at runtime the launcher asks for (and remembers) a
ROM on first start. The code is not compressed, so there is no decompression step.

## 4. Build N64Recomp + RSPRecomp, then recompile

Build the recompiler CLI from upstream at the pinned commit
(`ffb39cdad1da5de07eaaa48bd1db4a89a7986771` — the commit `RecompiledFuncs/` was
last generated with; any toolchain works, VS is fine):

```powershell
git clone --recurse-submodules https://github.com/N64Recomp/N64Recomp.git N64RecompSource
git -C N64RecompSource checkout ffb39cdad1da5de07eaaa48bd1db4a89a7986771
git -C N64RecompSource submodule update --init --recursive
cmake -S N64RecompSource -B N64RecompSource/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build N64RecompSource/build --target N64RecompCLI RSPRecomp
copy N64RecompSource\build\N64Recomp.exe .
copy N64RecompSource\build\RSPRecomp.exe .
```

(Not to be confused with the `jessetbh/N64Recomp` fork pinned inside
`lib/N64ModernRuntime` — that one only supplies the `recomp.h` header the port
compiles against.)

Then recompile the game and the audio microcode (from the repo root):

```powershell
.\N64Recomp.exe wcw.toml              # -> RecompiledFuncs/
.\RSPRecomp.exe rsp\wcw_audio.toml    # -> rsp/wcw_audio.cpp
```

## 5. Build the port

Tell CMake where your patches toolchain lives — either put the tools on PATH, or
create an uncommitted `local-config.cmake` in the repo root:

```cmake
# local-config.cmake (machine-local, gitignored)
set(PATCHES_C_COMPILER "C:/tools/LLVM-19.1.3-Windows-X64/bin/clang.exe")   # or "C:/tools/zig/zig.exe cc"
set(PATCHES_LD         "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/x64/bin/ld.lld.exe")
set(PATCHES_MAKE       "C:/tools/mingw64/bin/mingw32-make.exe")
```

Then configure and build with clang-cl (`tools/env-msvc.ps1` puts the VS tools on
PATH for the current shell):

```powershell
. .\tools\env-msvc.ps1
cmake -S . -B build-msvc -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release
cmake --build build-msvc --target WCWRecompiled
```

The exe lands at `build-msvc\WCWRecompiled.exe`, with `assets/`,
`recompcontrollerdb.txt`, and the SDL2/DXC DLLs copied next to it — that folder is
a complete, runnable install.

Build types: `Release` is the shipping configuration (windowed app; stdio goes to
`%LOCALAPPDATA%\WCWRecompiled\WCWRecompiled.log`, or launch with `--show-console`);
`Debug` keeps the console.

## 6. Run

Double-click `WCWRecompiled.exe`. First run: **Load ROM** → pick your ROM → it is
validated and stored → **Start Game**. Saves and config live in
`%LOCALAPPDATA%\WCWRecompiled\` (create an empty `portable.txt` next to the exe
for a portable install).

Dev conveniences:

- `WCW_AUTOBOOT=<path|1>` — skip the launcher and boot straight into the game
  (`1` uses the stored ROM). `tools\cycle.ps1` sets it.
- `. .\tools\cycle.ps1` — regen symbols → recompile → build → 20 s run →
  symbolize any crash against `build-msvc\WCWRecompiled.map` (always emitted;
  release backtraces print RVAs that resolve against it).
- More diagnostics (env-gated) are listed in `CLAUDE.md`.

## 7. Working on patches (`patches/*.c`)

Game-behavior patches are C compiled to MIPS (`RECOMP_PATCH` overrides a base-game
function by its `WCWSyms/dump.toml` name). The normal `cmake --build` drives the
whole pipeline (make → `N64Recomp patches.toml` → `file_to_c` → `PatchesLib`).
Gotchas:

- **Build twice after editing a patch** — the first build regenerates
  `RecompiledPatches/` but links the previous table (same quirk BMHero documents).
- `PatchesLib` must stay listed **before** `RecompiledFuncs` in
  `target_link_libraries` — that ordering IS the patch mechanism.
- Any runtime API a patch calls needs an address entry in `patches/syms.ld` AND a
  real implementation in the link (librecomp `*_recomp` or
  `src/game/recomp_api.cpp`).

## 8. Regenerating symbols (contributors)

`WCWSyms/` (submodule) holds the generated `dump.toml`/`data_dump.toml`; the
regeneration tooling is the splat project in `disasm/` + `tools/gen_symbols.py`
(the libultra `RENAME` map lives there; evidence log in `disasm/libultra.md`).
Requires Python and a splat venv, plus the ROM copied to `disasm\wcw.z64`:

```powershell
# splat64's deps don't all auto-resolve — install them explicitly (see disasm/README.md)
python -m venv disasm\.venv
disasm\.venv\Scripts\pip install splat64 spimdisasm rabbitizer n64img crunch64 pygfxd
disasm\.venv\Scripts\python.exe -m splat split disasm\wcw.yaml   # (re)disassemble
python tools\gen_symbols.py --overlays --data                    # -> WCWSyms/*.toml
```

`tools\recompile.ps1` wraps gen_symbols + N64Recomp for the fast loop
(`tools\env.ps1` provides its MinGW/python environment; see the env-var overrides
at the top of that script for machine-specific paths).

## 9. Changing anything under `lib/`

`lib/` submodules are jessetbh forks carrying the `[wcw fix]` set on `wcw`
branches. After ANY edit under `lib/`: commit on that repo's `wcw` branch, push to
the fork, bump the submodule pin in the superproject, and rerun
`.\lib-patches\export.ps1` to refresh the diff-vs-upstream record. Details:
`lib-patches/README.md` and `CLAUDE.md`.
