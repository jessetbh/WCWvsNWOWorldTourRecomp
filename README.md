# WCW vs. nWo World Tour: Recompiled

A work-in-progress native PC port of the Nintendo 64 game **WCW vs. nWo World Tour (USA)**,
built by statically recompiling the original game with
[N64Recomp](https://github.com/N64Recomp/N64Recomp) and running it on the
[N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) +
[RT64](https://github.com/rt64/rt64) stack.

> **This repository does not contain game assets.** A legally obtained copy of the
> original ROM is required to build and run.

## Status

**Early scaffolding (Phase 0).** The project structure, configs, and build/patch
scaffolding exist, but the symbol metadata, recompiler output, and runtime submodules
are not in place yet. See [`CLAUDE.md`](CLAUDE.md) for the full state and roadmap, and
[`BUILDING.md`](BUILDING.md) for how the build is meant to work.

## Target ROM

- WCW vs. nWo World Tour (USA), `.z64`, SHA1 `5AD2D8359058C8BB71F08E3D3433B7A50D3BB645`.

## Approach in brief

1. Disassemble the ROM (splat) to produce symbol metadata — *no public decomp exists*.
2. Run N64Recomp to translate the MIPS code into C.
3. Compile that C against the modern runtime + RT64 renderer.
4. Apply behavior fixes and PC enhancements as MIPS-compiled patches.

Modeled closely on **Bomberman Hero: Recompiled**.

## Credits / references

- [N64Recomp](https://github.com/N64Recomp/N64Recomp) and
  [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) by Wiseguy et al.
- [RT64](https://github.com/rt64/rt64) renderer by DarioSamo.
- Bomberman Hero: Recompiled as the reference project.
