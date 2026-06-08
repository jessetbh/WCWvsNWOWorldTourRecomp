# Building Guide

> The build is **not yet functional** — these are the intended steps, adapted from
> Bomberman Hero: Recompiled. Several prerequisites are still missing (see `CLAUDE.md`,
> "Current project state" and "Roadmap"). This document is the target workflow.

## 1. Prerequisites

### Windows (primary environment here)
Install [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) with:
- Desktop development with C++
- C++ Clang Compiler for Windows
- C++ CMake tools for Windows

Plus `make` (e.g. `choco install make`) for building the MIPS patches.

### Linux (for reference)
```bash
sudo apt-get install cmake ninja-build libsdl2-dev libgtk-3-dev lld llvm clang make
```

## 2. Provide the ROM
Place the NTSC-U WCW vs. nWo World Tour ROM in the repo root as `wcw.z64`
(SHA1 `5AD2D8359058C8BB71F08E3D3433B7A50D3BB645`). If the game's code turns out to be
compressed, a decompressed copy may be required for the recompiler step (TBD).

## 3. Generate symbol metadata (Phase 1 — not done yet)
There is no public WCW decompilation, so symbols must be generated. Set up a
[splat](https://github.com/ethteck/splat) project under `disasm/`, split the ROM, and
build an ELF with symbols. Feed that ELF to N64Recomp to emit `syms/dump.toml` and
`syms/data_dump.toml` (or author them directly). See `syms/README.md` and `disasm/README.md`.

## 4. Build the recompiler
```powershell
cd C:\Users\selki\depot\N64Recomp
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
Copy `N64Recomp.exe` and `RSPRecomp.exe` from the build folder into this repo's root.

## 5. Recompile the game
```powershell
.\N64Recomp wcw.toml          # -> RecompiledFuncs/
.\RSPRecomp rsp\aspMain.toml   # -> rsp/ (once the audio ucode is identified)
```

## 6. Add the runtime submodules (Phase 3)
```powershell
git submodule update --init --recursive   # pulls lib/N64ModernRuntime, lib/RecompFrontend, lib/rt64
```

## 7. Build the project
```powershell
cmake -S . -B build-cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --target WCWRecompiled -j
```
Run the resulting `WCWRecompiled` executable from the repo root (or copy assets next to it).
