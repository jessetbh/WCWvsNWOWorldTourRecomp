# disasm/ — disassembly project (Phase 1)

Goal: turn the WCW ROM into a symbol context (an ELF and/or `syms/*.toml`) that N64Recomp
can consume, since no public decompilation exists.

## Setup (done)
- Python venv at `disasm/.venv` (gitignored).
- [splat64](https://github.com/ethteck/splat) `0.41.0` installed, plus deps
  (spimdisasm, rabbitizer, n64img, crunch64, pygfxd). Note: those deps did **not**
  auto-resolve from `splat64` alone and had to be installed explicitly.
- ROM copied to `disasm/wcw.z64` (gitignored).
- Config: `wcw.yaml` (cleaned up from splat's `create_config` output, which had a
  colon in the basename that is illegal on Windows). Original auto output kept as
  `wcwvs.nwo_worldtour.yaml` for reference.

Run a split with:
```powershell
.\.venv\Scripts\python.exe -m splat split wcw.yaml
```

## Findings so far (initial recon)

**Compiler:** IDO (auto-detected) — the well-supported case for N64Recomp.

**Code lives in exactly two ROM regions** (measured by `jr $ra` density across the ROM):

| ROM range | Size | What it is |
|-----------|------|------------|
| `0x001050`–`0x02A9B0` | ~170 KB | Fixed `main` code segment (splits cleanly today) |
| `0x030000`–`0xA20000` | ~10 MB | Assets only (textures/audio/models/data), ~zero code |
| `0xA20000`–`0xAC0000` | ~640 KB | **Overlay code region** — all dynamically-loaded code |

So **total code ≈ 800 KB, ~2,000 functions** — a medium, tractable size. The fixed
segment disassembles correctly (the entrypoint BSS-clear + jump-to-`main` decodes
perfectly).

## Overlay system (SOLVED)

The game uses a simple, recomp-friendly overlay design: a fixed segment plus **exactly
two CPU overlays that swap at the same VRAM**.

**Descriptor table** — 9 words (0x24 bytes) each, in the main data section:
- `ovl_a_descriptor` @ `0x80032BA0`
- `ovl_b_descriptor` @ `0x80032BC4`

Descriptor layout: `romStart, romEnd, entry, <section-boundary words...>, bssEnd`.
The text+data size equals `romEnd - romStart` exactly (verified for both).

**Loader/runtime functions** (in the fixed segment):
- `ovl_load_sections` @ `0x8000075C` — DMAs each overlay section from ROM, clears BSS.
- `ovl_load_and_enter` @ `0x800007C8` — loads, then calls the overlay entry `0x80090000`.
- `ovl_swap_loop` @ `0x80011134` — alternates loading overlay A / overlay B.

**Overlay map** (both load to VRAM `0x80090000`, mutually exclusive):

| Overlay | ROM range | .text (vram) | .data (vram) | .bss (vram, size) |
|---------|-----------|--------------|--------------|-------------------|
| A (`0x80032BA0`) | `0xA21750`–`0xA69570` | `0x80090000`–`0x800C3BC0` | `0x800C3BC0`–`0x800D7E20` | `0x800D7E20` (`0x16000`) |
| B (`0x80032BC4`) | `0xA69570`–`0xAC2970` | `0x80090000`–`0x800E5F70` | `0x800E5F70`–`0x800E9400` | `0x800E9400` (`0xF6C0`) |

These two account for the entire `0xA20000–0xAC0000` code region. Both are now encoded in
`wcw.yaml` (segments `ovl_a` / `ovl_b`, tied via `exclusive_ram_id: ovl_90000`) and
**disassemble cleanly** — `func_80090000` decodes as the overlay entry point and its init
code correctly references its own BSS at `D_800D7E20`. (Likely semantics, unconfirmed:
one overlay is the front-end/menus, the other the in-ring match engine.)

**RSP microcode** is embedded as data in the main segment (around vram `0x8002AC10`) and
DMA'd to IMEM (`0x84001000` = physical `0x04001000`). The earlier `func_84001xxx` /
`0x00A2xxxx` hits were immediates *inside* this ucode, not CPU overlays. This is
RSPRecomp's job, handled separately in `../rsp/`.

## Next steps (in order)
1. **Refine data/rodata splits** within each segment (splat's `find_file_boundaries`
   gives a start; jump tables and pointers need iteration).
2. **Identify the RSP microcode** precisely (audio + which F3DEX graphics variant) and set
   `gfx_ucode:` in `wcw.yaml`; build `../rsp/` configs.
3. **Build the ELF** (`splat`'s linker script + assembling the asm) and feed it to
   N64Recomp in ELF mode to dump `../syms/dump.toml` and `../syms/data_dump.toml`.
4. **Wire the overlay loader** into `../patches/required_patches.c` (hook
   `ovl_load_and_enter` / the swap so the runtime maps overlays — mirrors BMHero).

## Still to determine
- Exact overlay table format and the list of overlays + their VRAMs.
- Whether overlay code is relocatable (needs reloc handling) or fixed-address.
- Graphics microcode variant (F3DEX family) for `gfx_ucode:` in `wcw.yaml`.
- RSP audio microcode location.

Keep large generated disassembly (`asm/`, `build/`, `.venv/`) out of git.
