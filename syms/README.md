# syms/ — symbol metadata

N64Recomp needs to know where the game's functions and data live.

## `dump.toml` — GENERATED, present ✅
Sections + functions for the base game, in N64Recomp's symbol-TOML format. Contains all
**1763 functions** across 4 sections (entry, main, ovl_a, ovl_b). Produced by
`../tools/gen_symbols.py --overlays`, which parses the splat disassembly (`../disasm/asm/`)
— we use **symbol-TOML mode**, not ELF mode, because this machine has no MIPS assembler.

`wcw.toml` points `symbols_file_path` at `dump.toml`. Regenerate any time the disassembly
or function boundaries change.

Notes:
- The `RENAME` map in `gen_symbols.py` is the **libultra integration mechanism**: mapping
  `func_XXXX` → a known libultra name makes N64Recomp skip recompiling it, and the runtime
  (ultramodern/librecomp) provides the host implementation instead. ~40 functions are
  named this way (threads, messages, VI, AI, PI/EPI DMA, SP/DP tasks incl. osSpTaskYield/
  Yielded, raw SI, clock, IDO softfloat) — this is what makes the port boot and run.
  Evidence for each name is inline in `gen_symbols.py` and in `../disasm/libultra.md`.
- The game's function literally named `main` is renamed to `game_main` (it collides
  with the C/C++ entry point).
- Both overlays load to vram `0x80090000`; the recompiler resolves the ambiguous
  `jal 0x80090000` via runtime function lookup (the overlay swap mechanism).
- After editing `RENAME`, regenerate + recompile + rebuild — `../tools/cycle.ps1` does the
  whole loop.

## `data_dump.toml` — not generated yet ⬜
Data symbols for the base game. Not required for the code recompile (it succeeds without
them), but needed for the patches build (`patches.toml` references it) and for naming data
references. Generate later from the splat data sections.

## Format reference
See `../CLAUDE.md` → "The symbol TOML format". The `[[section]]` blocks carry
`rom`/`vram`/`size` and a `functions = [{ name, vram, size }, …]` array; data symbol files
carry `symbols = [{ name, vram }, …]`.
