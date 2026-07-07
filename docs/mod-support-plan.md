# Mod support — post-release roadmap

User-facing mod support à la Zelda64Recomp (README lists it as planned). Drafted
2026-07-07 at beta release; sequenced so each phase delivers something usable.

## What already exists (dormant, from the shared runtime stack)

- librecomp ships the mod runtime: it already creates `mods/` + `mod_config/` and
  `mods.json` in the app folder on every install.
- The launcher framework has a ready Mods menu (`add_mods_option()`) — deliberately
  omitted from our launcher in C1 "until mod support exists" (src/main/main.cpp).
- `GameEntry.mod_game_id` is already declared.
- The pinned jessetbh/N64Recomp fork is from the mod-tool release line: RecompModTool,
  OfflineModRecomp, and RecompModMerger build from the source we already use.
- Mods compile against the game's symbol context = WCWSyms (public since release).

## Phase M1 — prove the pipeline (first post-release project)

1. Build the mod tools from the pinned N64Recomp fork.
2. Re-enable the launcher Mods option.
3. Port the widescreen patch into a `.nrm` mod as the proof-of-concept: the code is
   known-good, so any failure isolates the mod pipeline itself.
4. Flush out WCW-specific unknowns: mod function replacement vs the two
   swap-at-same-VRAM CPU overlays, and vs the custom raw-SI/Controller Pak layer
   (si.cpp).

Exit criterion: drop the .nrm into `mods/`, toggle it in the launcher, widescreen
works; remove it, base game unchanged.

## Phase M2 — modder SDK

- `WCWModTemplate` repo: makefile (portable-LLVM-or-zig toolchain story from
  BUILDING.md), `mod.toml` manifest wired to our game id, symbol context via WCWSyms.
- Docs: how to override a function (RECOMP_PATCH-style), how to call runtime APIs,
  the patches/syms.ld equivalent for mods.

## Phase M3 — ergonomics (ongoing)

- The structural gap vs Zelda's ecosystem: no decompilation, so modders write
  against `func_80xxxxxx` names. Ergonomics grow with every named function — name
  and document the systems modders actually want (wrestler stats, move tables,
  match logic entry points) as they're identified, via the normal
  `gen_symbols.py` RENAME + `disasm/libultra.md` evidence flow.
- **Symbol-name stability policy from M1 onward**: renames in WCWSyms break
  published mods. Additive naming only; deprecation notes for unavoidable changes.

## Phase M4 — distribution (later)

- `mods/` folder drop-in works from M1 with zero infrastructure.
- Thunderstore community (Zelda64Recomp precedent) only if/when a real modding
  community materializes.
