# WCW vs. nWo World Tour: Recompiled

A work-in-progress native PC port of the Nintendo 64 game **WCW vs. nWo World Tour (USA)**,
built by statically recompiling the original game with
[N64Recomp](https://github.com/N64Recomp/N64Recomp) and running it on the
[N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) +
[RT64](https://github.com/rt64/rt64) stack.

> **This repository does not contain game assets.** A legally obtained copy of the
> original ROM is required to build and run.

## Status — FULLY PLAYABLE: matches, sound, input, menus, and saves (2026-07-05)

The port builds, boots to a window, and plays the game's complete attract/demo cycle
indefinitely: intro (spinning WCW mat), attract match (3D ring + crowd + animated
wrestlers), wrestler intro cinematics, title screen — looping, stable, no crashes.
**Audio works** (the RSP audio ucode is recompiled — it turned out to be the stock
Nintendo aspMain, byte-identical to Bomberman Hero's), and the audio crackle + video
flicker that followed are fixed (audio buffer-depth reporting; presentation mode
Console instead of PresentEarly — WCW frames span multiple gfx tasks that pre-clear
the next framebuffer, which PresentEarly then presented as a black frame). **Input works**
(keyboard + gamepad; two fixes: the raw-SI path never triggered recompinput's poll, and
single-player mode was never enabled) — **full matches verified playable end to end**.
**Menus render cleanly** (frame interpolation now defaults off — RT64 assumes one RSP
workload per frame, WCW uses several, so interpolated frames came out black/partial).
**Saves work**: WCW saves only to the Controller Pak, which the recomp stack doesn't
support — but WCW's raw joybus driver goes through our PIF emulation, which now emulates
a 32 KB pak in port 1 backed by the standard recomp save file.

| Phase | What | State |
|------:|------|-------|
| 0 | Scaffolding, configs, project structure | ✅ done |
| 1 | splat disassembly; **overlay system fully mapped** (2 fixed-addr overlays) | ✅ done |
| 2 | Build N64Recomp; generate symbols; **clean recompile → 16 MB of C (1763 funcs)** | ✅ done |
| 3 | libultra→ultramodern integration (~40 funcs named), `src/main` wiring, RT64 build, **BOOTS + RENDERS**, full demo loop stable | ✅ done |
| 3 | **Audio** — RSPRecomp'd (`rsp/wcw_audio.toml`), real sound verified | ✅ done |
| 3 | In-game **input** (keyboard + gamepad) — **matches playable end to end** | ✅ done |
| 3 | **Menu flicker/missing assets fixed** (RT64 interpolation off by default; user-verified) | ✅ done |
| 3 | **Saves** — Controller Pak emulated over raw joybus, persists to the recomp save file | ✅ done |
| 4 | **Patches build foundation** (data symbols, MIPS cross-compile, RECOMP_PATCH verified in-game) | ✅ done |
| 4 | **Widescreen** (game-side projection patch + host aspect API, Aspect Ratio: Expand) | ✅ done |
| 4 | **Overscan-edge crop** (CRT-style per-edge crop; garbage line at frame top gone) | ✅ done |
| 4 | More PC enhancements (high-FPS interpolation, input options) | 🔶 next |

Getting here required fixing two runtime-stack bugs that likely affect other recomp
projects too (documented in `disasm/libultra.md`, fixes tagged `[wcw fix]` in `lib/`):
- **RT64**: games that only submit composed MVPs via `G_FORCEMTX` (all AKI wrestling
  titles) never load a projection matrix; RT64 inverted the zero matrix → NaN vertex
  positions → nothing rasterized (black screen).
- **N64ModernRuntime**: the external-message pump processed one message per wake and
  immediately re-enqueued failed deliveries; moodycamel's producer-biased dequeue then
  starved all other messages (VI retrace included) → total game deadlock.

See [`CLAUDE.md`](CLAUDE.md) for the full state, technical detail, and the step-by-step
plan; [`BUILDING.md`](BUILDING.md) for how to build and run it today.

## What works today
- **The whole game**: `build-msvc\WCWRecompiled.exe` boots to the title screen and plays
  full matches end to end — hardware-accelerated rendering (RT64, D3D12/Vulkan) at 4x
  resolution, audio (recompiled RSP audio ucode → SDL), keyboard + gamepad input, clean
  menus, and persistent saves (emulated Controller Pak → `saves/<game id>.bin`).
- **Widescreen**: with Aspect Ratio set to Expand, 3D renders at the window's aspect
  with a correctly widened FOV (the game's single projection-setup function is patched
  to take the display ratio; 2D stays 4:3-anchored). The first real gameplay patch —
  `patches/widescreen.c`.
- `splat` disassembly of the ROM (`disasm/`), with the overlay table decoded.
- `N64Recomp` recompiles the whole game to C with `tools/recompile.ps1`; the fast
  edit-compile-run loop is `tools/cycle.ps1`.
- A scriptable RenderDoc capture/analysis workflow for GPU debugging (see `CLAUDE.md`).

## Not working yet
- **Frame interpolation** (Framerate: Display) produces black/partial frames in menus —
  RT64's frame matching assumes one RSP workload per frame, WCW uses several. The default
  is now Framerate: Original (native pacing, renders correctly); interpolation is a
  Phase-4 item (needs multi-workload frame detection + matrix-group patches).

## Target ROM
- WCW vs. nWo World Tour (USA), `.z64`, SHA1 `5AD2D8359058C8BB71F08E3D3433B7A50D3BB645`,
  internal name `WCWvs.NWO:World Tour`, game code `NWNE`, entrypoint `0x80000400`, IDO compiler.

## Approach in brief
1. Disassemble the ROM (splat) to produce symbol metadata — *no public decomp exists*, so
   we generate it ourselves (`tools/gen_symbols.py` → `syms/dump.toml`).
2. Run N64Recomp to translate the MIPS code into C.
3. Identify the game's embedded **libultra** and hand it to ultramodern (the runtime).
4. Compile that C against the modern runtime + RT64 renderer; boot.
5. Apply behavior fixes and PC enhancements as MIPS-compiled patches.

Modeled closely on **Bomberman Hero: Recompiled**.

## Credits / references
- [N64Recomp](https://github.com/N64Recomp/N64Recomp) and
  [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) by Wiseguy et al.
- [RT64](https://github.com/rt64/rt64) renderer by DarioSamo.
- [splat](https://github.com/ethteck/splat) / spimdisasm for disassembly.
- Bomberman Hero: Recompiled as the reference project.
