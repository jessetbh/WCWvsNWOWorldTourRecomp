# rsp/ — RSP microcode

The N64 RSP runs two kinds of microcode: **graphics** (F3DEX family) and **audio**.

## Graphics ucode — handled by RT64 (do NOT recompile) — both variants CONFIRMED WORKING
RT64 implements the F3DEX-family graphics microcode itself, so the game's graphics ucode is
**not** recompiled here. The graphics task is built in `func_80002EAC`, which selects
between two graphics ucodes by a flag; **both are matched by RT64's GBI database at runtime
and render correctly** (2026-07-04):
- ucode A: text `D_8002AA70` (vram `0x8002AA70`, ROM `0x2B670`), data `D_800377F0`
  → RT64 identifies it as **"F3DEX 1.23 (Variant)"** (used by boot/attract).
- ucode B: text `D_8002BEA0`, data `D_80037FF0`
  → RT64 identifies it as **"F3DLX 1.23.Rej (Variant)"** (loaded at the overlay-B switch,
  used by the wrestler cinematics/title). The switch goes through `osSpTaskYield`/
  `osSpTaskYielded` (func_80012C80/func_80012760 — named in `tools/gen_symbols.py`).
- both `ucode_size = 0x1000`, `ucode_data_size = 0x800`

Confirmed graphics (not audio): the ucode reads RDP register `DPC_CURRENT` (RSP `mfc0 $10`)
and the OSTask uses `output_buff` as the RDP command FIFO. RSPRecomp correctly refused it
(`Unhandled mfc0: 10`) — that's expected for a graphics ucode, which is RT64's job.

NOTE: these AKI ucodes never load a projection matrix (composed MVPs via `G_FORCEMTX`
only) — this required a zero-VP→identity guard in RT64 (`[wcw fix]` in
`lib/rt64/src/hle/rt64_rsp.cpp`; see `../disasm/libultra.md` "BLACK SCREEN — SOLVED").

## Audio ucode — RECOMPILED AND WORKING (2026-07-04) — the game has sound
The audio ucode is required for `recomp::start`'s mandatory `rsp_callbacks.get_rsp_microcode`.

**DONE.** `wcw_audio.toml` (text_offset `0x2A850`, text_size `0xE20`, text_address
`0x04001080`) → `RSPRecomp` → `rsp/wcw_audio.cpp` (`wcw_audio_ucode`), compiled into
`WCWRecompiled` (CMake; needs `-march=nehalem` for librecomp's SSE4.1 RSP vector-unit
intrinsics) and returned by `get_rsp_microcode` for `M_AUDTASK`. Verified by a peak-
amplitude log in `queue_samples` (`[audio]` lines): real nonzero samples flow through the
whole attract loop. Two key findings:
- **The ucode is byte-identical to BMHero's stock aspMain** (0/0xE20 bytes differ; WCW ROM
  `0x2A850` vs BMHero ROM `0x4A060`) — so BMHero's `extra_indirect_branch_targets` list
  applies verbatim (copied into `wcw_audio.toml`; no iteration was needed).
- The OSTask's `ucode_size = 0x1000` is the rounded-up DMA size, not the text size. Using
  `0x1000` makes RSPRecomp run past the audio text into graphics ucode A (ROM `0x2B670` =
  `0x2A850 + 0xE20`) and abort on its `mfc0 DPC_CURRENT`. The real text size is `0xE20`.

The sections below document how the ucode was located and the no-op fallback era, kept
for reference.

**LOCATED (2026-06-11) by the runtime-logging method below.** The game's first M_AUDTASK reports:
- `task->t.type = 2` (M_AUDTASK)
- **`ucode` (text)   = `0x80029C50`**
- **`ucode_data`     = `0x80037530`**
- **`ucode_size`     = `0x1000`**

(Map RAM→ROM via the main segment: vram `0x80000400` = rom `0x1000`, so ucode text is at rom
`0x2A850`, data at rom `0x38130` — verify before feeding RSPRecomp.) Fill these into
`wcw_audio.toml` and run `.\RSPRecomp.exe rsp\wcw_audio.toml`.

**Historical note — the no-op era:** before the recompile, `get_rsp_microcode` returned a
no-op ucode that reported a clean RSP break so the audio task "completed" → silent audio.
That was necessary because returning `nullptr` makes `task_thread_func` (ultramodern
`events.cpp`) call `ULTRAMODERN_QUICK_EXIT` — i.e. a missing audio ucode *kills the process*
a few seconds into boot. The no-op (`wcw_null_ucode`) is kept as the fallback for unexpected
task types; never return `nullptr`.

**Method (how it was found).** Once the port boots far enough to submit RSP tasks, the
`get_rsp_microcode(task)` callback receives the real `OSTask`. Log `task->t.type`,
`task->t.ucode`, `task->t.ucode_data`, and `task->t.ucode_size` there to get the audio
ucode's exact RAM address (→ ROM offset) and size — far easier than static hunting. Then:
1. Fill in `wcw_audio.toml` with the real `text_offset` / `text_size` / `text_address`.
2. `.\RSPRecomp.exe rsp\wcw_audio.toml` — it reports unresolved indirect jump targets; add
   them to `extra_indirect_branch_targets` and re-run until clean.
3. Have `get_rsp_microcode` return the recompiled function for the audio task type.

The game runs its entire demo loop fine with or without audio; audio does not gate game
progression (verified 2026-07-04).
