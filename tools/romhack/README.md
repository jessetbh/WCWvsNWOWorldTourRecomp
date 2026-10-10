# ROM hacks as patch mods

A ROM hack can ship as a regular recomp mod (`.nrm`) that holds only a BPS patch against the
stock ROM. Players install it like any other mod; the runtime applies the patch to their own
ROM when the game starts. No ROM data is distributed.

## Making one

```powershell
python tools\romhack\make_patch_mod.py --base vpw2.z64 --hack my_hack.z64 `
    --game-id vpw2 --id my_hack --name "My Hack" --version 1.0.0 --author me `
    --out my_hack.nrm
```

Both ROMs must be big-endian `.z64`. Without `--out` the tool only prints the report.

The report lists every recompiled function the hack changes, read from the section and
function tables in `RecompiledFuncs/recomp_overlays.inl`:

- **Data-only** (no changed functions): works on any runtime with ROM-patch mods
  (N64ModernRuntime since Nov 2025, PR #126).
- **Changed functions**: needs the `wcw-romhack` runtime, which recompiles those functions
  from the patched ROM with the live recompiler at load time (`[romhack]` lines in stderr
  list them). A plain runtime keeps running the stock code for them, and the hack usually
  crashes.
- **Changed bytes outside functions**: code the hack added or moved outside the known
  function boundaries. Patch mods can't run that; it needs a full recompile of the hacked
  ROM (the VPW2 Freem `exp/freem` method).

Only one ROM-patch mod can be enabled at a time. On the `wcw-romhack` runtime a patch mod's
saves go to `saves/<mod id>/`, apart from the stock game's (a plain runtime shares them, so a
hack that changes the save layout could damage them).

## Verified

VPW2 Freem Edition (2024-06-15): 17 changed functions (75 instructions) plus ~31 MB of data.
On the stock build with the patch mod, all 17 are recompiled at load, the game boots into
Freem, and attract-mode demos match the dedicated full-recompile Freem build frame for frame
(with the same save; VPW2 keeps edited costumes in SRAM).
