# syms/ — symbol metadata

N64Recomp needs to know where the game's functions and data live. This directory holds:

- `dump.toml` — sections + functions (+ relocs) for the base game.
- `data_dump.toml` — data symbols for the base game.

**These do not exist yet.** WCW vs. nWo World Tour has no public decompilation, so they
must be generated (CLAUDE.md, Phase 1/2). The two routes:

1. **Recommended:** build a disassembly ELF (see `../disasm/`), run N64Recomp in ELF mode
   once, and let it emit these files (it writes exactly this format — see
   `N64Recomp/src/main.cpp::dump_context`).
2. Author them by hand (only sane for tiny binaries).

Format reference is in `../CLAUDE.md` under "The symbol TOML format".

`wcw.toml` points `symbols_file_path` at `syms/dump.toml`; `patches.toml` points its
reference syms at both files.
