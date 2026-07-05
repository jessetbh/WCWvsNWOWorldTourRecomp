# run/ — standalone "does it execute?" experiment (HISTORICAL — superseded)

> **Superseded**: the real port now builds and runs the game (`build-msvc\WCWRecompiled.exe`
> boots, renders, and plays the full demo loop — see `../BUILDING.md`). This harness is kept
> as a reference for the "prove the recompiled code executes" technique.

This is a throwaway harness to **actually run the recompiled code** without the full
runtime (no RT64, no ultramodern OS, no real DMA/threads/video). It answers "what happens
if we just run it?" and finds the exact point where execution needs something we don't
provide yet.

## What it does
- `harness.cpp` allocates an 8 MB simulated RDRAM, loads the ROM boot segment into it
  (byte-swapped to host order), builds a `vram -> function` table from the generated
  `recomp_overlays.inl`, stubs the handful of runtime hooks `recomp.h` declares
  (`get_function`, `cop0_*`, `do_break`, …), and calls `recomp_entrypoint`.
- `recomp.h` here is a copy of N64Recomp's runtime header, with the memory-access macros
  masked (`& 0x7FFFFF`) so cached/uncached/IO addresses wrap into the 8 MB buffer instead
  of segfaulting.
- The recompiled functions are compiled with `-finstrument-functions` so a watchdog
  thread can report where execution is (to catch infinite poll loops).

## Build & run
```powershell
. .\tools\env.ps1
.\run\build.ps1
.\run\wcw_harness.exe wcw.z64    # (run from repo root, or pass the ROM path)
```

## Result (this is the interesting part)

It runs **real recompiled WCW code, correctly, with no crashes**:

**Phase 1 — boot (`recomp_entrypoint`):** runs the BSS-clear, resolves `0x80000450 ->
game_main` via the function lookup, and executes `game_main`, which does the classic
libultra boot: system init (`func_800112D0`), **osCreateThread** of the game thread
(entry `func_800004AC`) via `func_80011560`, **osStartThread** (`func_800116B0`), then
returns cleanly. (The game thread is "created" but our stubbed scheduler never dispatches
it — exactly the layer the real runtime provides.)

**Phase 2 — game thread (`func_800004AC`, invoked manually):** executes real game code at
~86 million recompiled-function-calls/second and settles into a **busy-wait loop**,
consistently spinning in `func_80011BF0` (a small PRNG at `0x80011BF0`). The game thread
is polling for a hardware/IPC condition (VI retrace, RSP/RDP, or another thread's message)
that this minimal harness never signals, so it loops forever.

### Takeaway
The recompilation **executes faithfully** — memory model, calling convention, control
flow, and arithmetic all work; real boot logic and real game-thread code run. The only
thing stopping further progress is the absence of the real runtime (hardware emulation,
threading, video) — precisely the N64ModernRuntime/RT64 layer that Phase 3 adds. Nothing
here indicates a problem with the recompiled code itself.

This harness is **not** part of the real port; it was a diagnostic. The prediction above
held: once the runtime layer was wired (Phase 3), the same recompiled code booted and ran
the game. The real entry point is `src/main/main.cpp` → `recomp::start`.
