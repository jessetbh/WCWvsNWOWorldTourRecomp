# Contributing

Thanks for your interest in improving WCW vs. nWo World Tour: Recompiled!

## Ground rules

- **Never commit ROM data or extracted game assets.** PRs containing game data will be
  closed. CI builds fetch the ROM from a private repository; fork PRs build without it.
- This project is licensed under GPLv3 (see `COPYING`); contributions are accepted
  under the same license.
- Start with `CLAUDE.md` (contributor instructions and the project's hard-won
  invariants) and `BUILDING.md` (how to build). The development history in
  `docs/devlog.md` documents why things are the way they are — check it before
  re-litigating a design decision or re-investigating a known issue.

## Practical notes

- The runtime libraries (N64ModernRuntime, RecompFrontend, RT64) carry project-local
  fixes tagged `[wcw fix]`. If you change anything under `lib/`, export the change with
  `.\lib-patches\export.ps1` and include the updated patch file in your PR.
- Game-behavior changes belong in `patches/` (C cross-compiled to MIPS via
  `RECOMP_PATCH`), not in hand-edits to recompiler output.
- Symbol identifications (naming a `func_XXXX`) need evidence — add it to
  `disasm/libultra.md` alongside the `RENAME` entry in `tools/gen_symbols.py`.
- Test on a real build before opening a PR: boot, play a match, open the config menu.

## Reporting bugs

Use the issue template. Always include your GPU + driver version, OS, the log output,
and your ROM's SHA1 (the only supported ROM is the US release, SHA1
`5AD2D8359058C8BB71F08E3D3433B7A50D3BB645`).
