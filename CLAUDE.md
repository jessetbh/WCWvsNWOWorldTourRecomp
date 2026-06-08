# CLAUDE.md — WCW vs. nWo World Tour: Recompiled

This file is the source of truth for this project. Read it before doing work here.

## What this project is

A native PC port of the Nintendo 64 game **WCW vs. nWo World Tour (USA)**, produced
by *statically recompiling* the original MIPS machine code into C using the
[N64Recomp](https://github.com/N64Recomp/N64Recomp) framework, then compiling that C
for modern platforms and running it against a modern runtime (N64ModernRuntime) with
[RT64](https://github.com/rt64/rt64) as the renderer.

This is **not** an emulator and **not** a decompilation. Static recompilation translates
the game's binary one MIPS instruction at a time into equivalent C. No game assets are
distributed; the user supplies their own ROM.

The reference project for everything here is **Bomberman Hero: Recompiled** at
`C:\Users\selki\depot\BMHeroRecomp` — when in doubt, mirror how it does something.

## Local machine layout

| Thing | Path |
|-------|------|
| This project | `C:\Users\selki\depot\WcwNwoWorldTour` |
| N64Recomp framework (source, unbuilt) | `C:\Users\selki\depot\N64Recomp` |
| Reference port (Bomberman Hero) | `C:\Users\selki\depot\BMHeroRecomp` |
| ROMs | `C:\Users\selki\depot\roms` |
| WCW ROM | `C:\Users\selki\depot\roms\WCW vs. nWo - World Tour (USA)\WCW vs. nWo - World Tour (USA).z64` |

The environment is Windows 11 + PowerShell. The toolchain (clang, ld.lld, make) targets
the same setup BMHero documents in its `BUILDING.md`.

## Target ROM facts

Verified from the ROM file on this machine:

- **Format**: native big-endian `.z64` (header magic `80 37 12 40`)
- **SHA1**: `5AD2D8359058C8BB71F08E3D3433B7A50D3BB645`
- **MD5**: `203C3BBFDD10C5A0B7C5D0CDB085D853`
- **Size**: 12,582,912 bytes (12 MiB)
- **Entrypoint**: `0x80000400`
- **Internal name**: `WCWvs.NWO:World Tour`
- **Game code**: `NWNE` (N = cart, WN = WCW vs nWo World Tour, E = USA/NTSC)

The recompiler wants a **decompressed** ROM in some games (BMHero ships a
`rom_decompression.cpp` step). Whether WCW needs decompression is **unknown until the
ROM layout is analyzed** — see the roadmap. Players always supply a standard ROM; any
decompression happens internally at load time.

## How N64Recomp works (the essentials)

1. The recompiler reads a **config TOML** (first CLI arg) describing input/output paths
   plus per-function tweaks (stub/ignore/rename, single-instruction patches).
2. It needs **symbol metadata** for the target binary, supplied one of two ways:
   - **ELF mode** (`elf_path`): an ELF with symbols, produced by a disassembly/decomp.
     This is the richest mode — it also enables `func_reference_syms_file` /
     `data_reference_syms_files` for resolving relocations, and is what the *patches*
     build uses.
   - **Symbol-TOML mode** (`symbols_file_path` + `rom_file_path`): a hand-or-tool authored
     TOML that lists sections, functions, relocs, and data symbols. This is what
     BMHero's main recomp uses (`BMHeroSyms/dump.toml`).
3. It emits one C file per function into `output_func_path` (here `RecompiledFuncs/`),
   plus `recomp_overlays.inl`, `funcs.h`, and a section table the runtime consumes.
4. Branch delay slots are duplicated; `jal` → C call; `jalr` → `LOOKUP_FUNC(...)` for
   the runtime to resolve; jump tables → `switch`. Relocatable overlays get
   `RELOC_HI16`/`RELOC_LO16` macros the runtime fixes up at load.
5. **RSP microcode** (audio + any graphics ucode) is recompiled separately with
   `RSPRecomp` against its own TOML (BMHero: `aspMain.toml` → `rsp/n_aspMain.cpp`).

### The symbol TOML format

`dump.toml` (functions) and `data_dump.toml` (data) use this shape — this is exactly what
N64Recomp itself emits when run in ELF mode (`src/main.cpp::dump_context`), so the cleanest
way to produce them is: build a disassembly ELF, run the recompiler once in ELF mode, and
let it dump these files.

```toml
[[section]]
name = "..."
rom  = 0x00001000      # omitted for bss/absolute sections
vram = 0x80000400
size = 0x1234

relocs = [
    { type = "R_MIPS_HI16", vram = 0x80000420, target_vram = 0x80055000 },
    { type = "R_MIPS_LO16", vram = 0x80000424, target_vram = 0x80055000 },
]

functions = [
    { name = "func_80000400", vram = 0x80000400, size = 0x80 },
]
```

Data symbols file:
```toml
[[section]]
name = "..."
vram = 0x80055000
size = 0x2000
symbols = [
    { name = "D_80055000", vram = 0x80055000 },
]
```

## Current project state

**Phase 0 — scaffolding (this is where we are).** Directory structure, config templates,
patch/build scaffolding, and this guide exist. The hard prerequisites are NOT yet done:

- ❌ N64Recomp / RSPRecomp binaries are **not built** (source only, at `..\N64Recomp`).
- ❌ No symbol metadata exists. There is **no public WCW decomp/disassembly**, so
  `syms/dump.toml` + `syms/data_dump.toml` must be **generated from scratch**.
- ❌ Submodules under `lib/` (N64ModernRuntime, RecompFrontend, rt64) are **not present**.
- ❌ RSP microcode entrypoints/offsets are unknown (need ROM analysis).
- ⚠️ `CMakeLists.txt`, `wcw.toml`, `patches.toml`, and the `patches/` files are **templates
  adapted from BMHero** and will need real values once symbols exist.

## Roadmap (do these in order)

### Phase 1 — Disassemble the ROM to get symbols
There is no existing decomp, so we make our own symbol context. Standard tool:
[splat](https://github.com/ethteck/splat) (N64 binary splitter) which yields an ELF +
symbol map. Steps:
1. Set up a splat project under `disasm/` (yaml config pointing at the WCW ROM; detect
   compression, segments, code vs data).
2. Run splat to split the ROM and assemble an ELF with symbols.
3. Iterate until the ELF disassembles/links cleanly. This is the bulk of the early work.

Alternatively, author `syms/dump.toml` directly, but for a 12 MiB commercial game the
disassembly route is far more practical.

### Phase 2 — Build the recompiler and run it
1. Build N64Recomp: `cmake` + `cmake --build` in `C:\Users\selki\depot\N64Recomp`
   (needs CMake ≥ 3.20, a C++20 compiler, submodules initialized recursively).
2. Copy `N64Recomp.exe` and `RSPRecomp.exe` to this project root.
3. Run `.\N64Recomp wcw.toml` → populates `RecompiledFuncs/`.
4. Identify the audio (and any graphics) RSP microcode in the ROM; fill in `rsp/aspMain.toml`
   equivalents and run `.\RSPRecomp <ucode>.toml`.

### Phase 3 — Stand up the runtime + build
1. Add submodules under `lib/` (mirror BMHero's `.gitmodules`): N64ModernRuntime,
   RecompFrontend, rt64. (WCW has no decomp lib to add, unlike BMHero's `lib/bmhero`.)
2. Flesh out `src/main/` (main, register_overlays, register_patches) and `src/game/`.
3. Build with CMake (clang/ninja) per `BUILDING.md`.
4. Boot, fix crashes, stub problem libultra funcs (see `wcw.toml [patches]`).

### Phase 4 — Patches & enhancements
Game-behavior fixes and PC enhancements live in `patches/` as MIPS-compiled C using
`RECOMP_PATCH` / `RECOMP_EXPORT` (see `patches/` + `patches.toml`). Widescreen, high-FPS
interpolation (RT64 matrix groups), input options, etc. — model on BMHero's `patches/`.

## Layout of this repo

```
wcw.toml              Main recompiler config (symbol-TOML mode by default)
patches.toml          Recompiler config for the C patches (single-file output mode)
CMakeLists.txt        Build (template adapted from BMHero — verify before relying on it)
syms/                 Symbol metadata: dump.toml, data_dump.toml (GENERATED — phase 1/2)
disasm/               splat disassembly project (phase 1)
patches/              C patches compiled to MIPS, + linker scripts + Makefile
  patch_helpers.h       RECOMP_PATCH/EXPORT-friendly DECLARE_FUNC macro
  patches.h             pulls in patch_helpers
  patches.ld            places patch sections into reserved extra RAM
  syms.ld               maps recomp_* runtime API calls to dummy vram addresses
  required_patches.c    boot/overlay-loader patches (STUB — fill once overlays known)
  dummy_headers/        empty headers to satisfy includes when cross-compiling
src/
  main/                 launcher + runtime registration glue
  game/                 config / API bridge code (recomp_api etc.)
include/              project C/C++ headers
lib/                  git submodules (runtime, frontend, renderer) — NOT yet added
RecompiledFuncs/      recompiler output (GENERATED, gitignored)
RecompiledPatches/    patch recompiler output (GENERATED, gitignored)
rsp/                  RSP microcode recomp config + output
tools/                helper scripts
```

## Conventions

- C++ namespace for this game's glue code: `wcw` (BMHero uses `banjo`).
- Generated dirs (`RecompiledFuncs/`, `RecompiledPatches/`, build dirs) are gitignored.
- Never commit ROM data or extracted assets.
- Use PowerShell syntax for shell commands on this machine.
- When adapting from BMHero, replace: `BMHeroRecompiled`→`WCWRecompiled`,
  `banjo`/`bk`→`wcw`, `bmhero.z64`→the WCW ROM, `BMHeroSyms`→`syms`.

## Phase 1 progress / known ROM layout

splat is set up under `disasm/` (venv + `wcw.yaml`) and the fixed code segment splits
cleanly. Initial recon (see `disasm/README.md` for detail) established:

- **Compiler: IDO** (auto-detected) — the well-supported case for N64Recomp.
- **Total code ≈ 800 KB / ~2,000 functions**, in two ROM regions only:
  - `0x001050`–`0x02A9B0` (~170 KB): fixed `main` segment — splits cleanly today.
  - `0xA20000`–`0xAC0000` (~640 KB): **overlay region**, all dynamically-loaded code,
    packed contiguously (not scattered).
  - The ~10 MB in between is assets (no code).
- **Overlay system SOLVED** (see `disasm/README.md` for the full map): exactly **two CPU
  overlays** that swap at VRAM `0x80090000`, described by 9-word descriptors at
  `0x80032BA0` / `0x80032BC4`, loaded by `0x8000075C` and entered via `0x800007C8`.
  Both now disassemble cleanly in `wcw.yaml`. They load at a **fixed address** (no
  per-load relocation), which keeps the recomp simpler.
  - Overlay A: ROM `0xA21750`–`0xA69570`; Overlay B: ROM `0xA69570`–`0xAC2970`.
- **RSP microcode** is embedded in the main data section (~`0x8002AC10`) and DMA'd to IMEM
  `0x84001000`. RSPRecomp's job, not a CPU overlay.

## Open questions to resolve early
- Precise RSP microcode bounds (audio) and the graphics ucode variant (F3DEX family).
- Data/rodata/jump-table boundary refinement within each segment (normal splat iteration).
- ~~Overlay table / layout?~~ Solved — two fixed-address swapping overlays at `0x80090000`.
- ~~Are overlays relocatable?~~ No — fixed load address.
- ~~Is the code compressed?~~ No standard compression; the code regions are plain MIPS.
- ~~Which compiler?~~ IDO.
