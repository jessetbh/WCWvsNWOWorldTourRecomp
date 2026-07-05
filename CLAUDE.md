# CLAUDE.md — WCW vs. nWo World Tour: Recompiled

This file is the source of truth for this project. Read it before doing work here.

## What this project is

A native PC port of the Nintendo 64 game **WCW vs. nWo World Tour (USA)**, produced
by *statically recompiling* the original MIPS machine code into C using the
[N64Recomp](https://github.com/N64Recomp/N64Recomp) framework, then compiling that C
for modern platforms and running it against a modern runtime (N64ModernRuntime) with
[RT64](https://github.com/rt64/rt64) as the renderer.

This is **not** an emulator and **not** a decompilation. Static recompilation translates
the game's binary one MIPS instruction at a time into equivalent C. No game assets are
distributed; the user supplies their own ROM.

The reference project for everything here is **Bomberman Hero: Recompiled** at
`C:\Users\selki\depot\BMHeroRecomp` — when in doubt, mirror how it does something.

## Status at a glance (Phase 3 in progress)

Done and verified:
- **Phase 0** scaffolding/config. **Phase 1** splat disassembly + overlay system fully
  decoded (two fixed-address overlays at vram `0x80090000`). **Phase 2** N64Recomp built
  (MinGW GCC), symbols generated (symbol-TOML mode, 1763 funcs), **clean recompile → 16 MB
  of C**.
- **Execution proven** by a standalone harness (`run/`): boot + game-thread code run
  faithfully; game thread busy-waits because the real runtime (threads/VI/timers) isn't
  wired yet.
- **Phase 3 foundation**: runtime libs cloned (`lib/`), **VS Build Tools/clang-cl
  installed**, and the **full recompiled output compiles with clang-cl against the real
  N64ModernRuntime headers**. libultra ~60 funcs identified (`disasm/libultra.md`).

### Immediate next steps (GAME FULLY PLAYABLE — next: Phase 4)
**Status (2026-07-05): the port is FULLY PLAYABLE end to end, user-verified** — boots,
renders, full matches with sound, keyboard + gamepad input, clean menus, and **working
saves** (Controller Pak emulation). The 2026-07-04 baseline (complete attract/demo cycle,
audio via the RSPRecomp'd stock aspMain — see `rsp/README.md`) was extended on 2026-07-05
with three fixes, each detailed in the ✅ bullets below: **input** (raw-SI path never
triggered recompinput's poll + single-player mode never enabled), **menu flicker/missing
assets** (RT64 frame interpolation assumes 1 workload = 1 frame, WCW uses several →
Framerate now defaults to Original), and **saves** (WCW saves ONLY to Controller Pak;
si.cpp now emulates a 32 KB pak in port 1 backed by librecomp's save file). Work is now
committed (was: one giant uncommitted tree) and all lib/ fixes are checked in as
`lib-patches/*.patch` (apply.ps1 / export.ps1 — run export after ANY lib/ edit).
Next up: **Phase 4** (`syms/data_dump.toml` + patches build → widescreen,
input options; real high-FPS interpolation needs RT64 multi-workload frame detection +
matrix-group patches). Deferred but still owed: **overscan-edge crop** (thin garbage
line at frame top) — come back to it after Phase 4 is under way. Dropped permanently
(decision 2026-07-05): upstreaming the general runtime bugs — the drafts in `upstream/`
stay as documentation only and will not be filed. Iterate via `tools/cycle.ps1`; `WCW_SAMPLE=<seconds>` dumps all
thread stacks at t+N; `WCW_RDC_T=<seconds>` sets the RenderDoc capture trigger time;
`WCW_PRESENT_LUM=1` + `WCW_BMP_START`/`WCW_BMP_COUNT` dump presented frames.

What got named this session (all in `tools/gen_symbols.py` RENAME, evidence inline + in
`disasm/libultra.md`): VI — `osViSetMode`(80012500), `osViBlack`(80012570), `osViSetEvent`
(80012650, the retrace-heartbeat fix), `osViGetCurrentFramebuffer`(800127E0),
`osViGetNextFramebuffer`(80012820), `osViSwapBuffer`(80012860), `osViSetSpecialFeatures`
(80012FB0); AI — `osAiGetLength`(80016BF0), `osAiSetNextBuffer`(80016B40). Plus raw-SI
(`__osSiRawStartDma`/`__osSiDeviceBusy`) backed by custom PIF emulation in `lib/.../si.cpp`,
the RSP/DP task funcs (`osSpTaskLoad`/`StartGo`/`osDpSetNextBuffer`), and a no-op audio ucode
in `src/main/main.cpp` (audio ucode located: `0x80029C50`/data `0x80037530`/`0x1000` — see
`rsp/README.md`).

The path-to-a-picture is DONE (2026-07-03) and the game is fully playable with saves
(2026-07-05); what follows is the historical integration guide + evidence log.
1. **libultra integration = a NAMING problem** (validated — see `disasm/libultra.md`).
   N64Recomp auto-ignores any function NAMED with a known libultra/libc/ido-math name
   (built-in sets in `N64Recomp/src/symbol_lists.cpp`); the runtime then provides it. So:
   add `func_XXXX → <libultra name>` entries to the `RENAME` map in `tools/gen_symbols.py`,
   regen `syms/dump.toml`, recompile. Do **not** add them to `wcw.toml` `ignored` (errors).
   As each is named, drop it from the placeholder `stubs` list.
   **CORRECTION to earlier scoping:** the claim that VI-display/controller/PI/AI "stay
   recompiled and work via librecomp's hardware handling" is **WRONG** — `recomp.h`'s `MEM_*`
   does not trap MMIO, so any recompiled libultra that touches a hardware register (SI/PI/VI/
   AI/SP/DP/SI) crashes. These device drivers **must** be named too (the runtime implements
   them). **22 named so far** (core OS + the boot-path PI/VI/cart/thread/msg funcs that got the
   game booting). **Next to name:** the VI display setters (osViSetEvent/SwapBuffer/SetMode/
   Black @ `func_8001E280/E30C/E484/E4F8`), osSpTaskLoad/osSpTaskStartGo (→ gfx tasks → RT64),
   osContInit/osContStartReadData (input), and the AI funcs (audio). This is what gets a frame.
3. **`src/main` wiring** → `recomp::start` with gfx/input/audio/RSP/thread/error callbacks
   (adapt BMHero `src/main/main.cpp`); `GameEntry` { entrypoint `0x80000400`,
   `recomp_entrypoint`, ROM hash }.
4. **RSP microcode** recompile with `RSPRecomp` (ucode embedded in main data → IMEM
   `0x84001000`) for the `get_rsp_microcode` callback.
5. **Full CMake build** (clang-cl/ninja) of recompiled libs + ultramodern + librecomp +
   RecompFrontend + RT64; then **boot and debug**.

Two toolchains: `tools/env.ps1` (MinGW GCC, builds N64Recomp) and `tools/env-msvc.ps1`
(clang-cl/MSVC, builds the port). Reproduce the recompile with `tools/recompile.ps1`; the
diagnostic harness with `run/build.ps1`.

## Local machine layout

| Thing | Path |
|-------|------|
| This project | `C:\Users\selki\depot\WcwNwoWorldTour` |
| N64Recomp framework (source, unbuilt) | `C:\Users\selki\depot\N64Recomp` |
| Reference port (Bomberman Hero) | `C:\Users\selki\depot\BMHeroRecomp` |
| ROMs | `C:\Users\selki\depot\roms` |
| WCW ROM | `C:\Users\selki\depot\roms\WCW vs. nWo - World Tour (USA)\WCW vs. nWo - World Tour (USA).z64` |

The environment is Windows 11 + PowerShell. The toolchain (clang, ld.lld, make) targets
the same setup BMHero documents in its `BUILDING.md`.

## Target ROM facts

Verified from the ROM file on this machine:

- **Format**: native big-endian `.z64` (header magic `80 37 12 40`)
- **SHA1**: `5AD2D8359058C8BB71F08E3D3433B7A50D3BB645`
- **MD5**: `203C3BBFDD10C5A0B7C5D0CDB085D853`
- **Size**: 12,582,912 bytes (12 MiB)
- **Entrypoint**: `0x80000400`
- **Internal name**: `WCWvs.NWO:World Tour`
- **Game code**: `NWNE` (N = cart, WN = WCW vs nWo World Tour, E = USA/NTSC)

The recompiler wants a **decompressed** ROM in some games (BMHero ships a
`rom_decompression.cpp` step). Whether WCW needs decompression is **unknown until the
ROM layout is analyzed** — see the roadmap. Players always supply a standard ROM; any
decompression happens internally at load time.

## How N64Recomp works (the essentials)

1. The recompiler reads a **config TOML** (first CLI arg) describing input/output paths
   plus per-function tweaks (stub/ignore/rename, single-instruction patches).
2. It needs **symbol metadata** for the target binary, supplied one of two ways:
   - **ELF mode** (`elf_path`): an ELF with symbols, produced by a disassembly/decomp.
     This is the richest mode — it also enables `func_reference_syms_file` /
     `data_reference_syms_files` for resolving relocations, and is what the *patches*
     build uses.
   - **Symbol-TOML mode** (`symbols_file_path` + `rom_file_path`): a hand-or-tool authored
     TOML that lists sections, functions, relocs, and data symbols. This is what
     BMHero's main recomp uses (`BMHeroSyms/dump.toml`).
3. It emits one C file per function into `output_func_path` (here `RecompiledFuncs/`),
   plus `recomp_overlays.inl`, `funcs.h`, and a section table the runtime consumes.
4. Branch delay slots are duplicated; `jal` → C call; `jalr` → `LOOKUP_FUNC(...)` for
   the runtime to resolve; jump tables → `switch`. Relocatable overlays get
   `RELOC_HI16`/`RELOC_LO16` macros the runtime fixes up at load.
5. **RSP microcode** (audio + any graphics ucode) is recompiled separately with
   `RSPRecomp` against its own TOML (BMHero: `aspMain.toml` → `rsp/n_aspMain.cpp`).

### The symbol TOML format

`dump.toml` (functions) and `data_dump.toml` (data) use this shape — this is exactly what
N64Recomp itself emits when run in ELF mode (`src/main.cpp::dump_context`), so the cleanest
way to produce them is: build a disassembly ELF, run the recompiler once in ELF mode, and
let it dump these files.

```toml
[[section]]
name = "..."
rom  = 0x00001000      # omitted for bss/absolute sections
vram = 0x80000400
size = 0x1234

relocs = [
    { type = "R_MIPS_HI16", vram = 0x80000420, target_vram = 0x80055000 },
    { type = "R_MIPS_LO16", vram = 0x80000424, target_vram = 0x80055000 },
]

functions = [
    { name = "func_80000400", vram = 0x80000400, size = 0x80 },
]
```

Data symbols file:
```toml
[[section]]
name = "..."
vram = 0x80055000
size = 0x2000
symbols = [
    { name = "D_80055000", vram = 0x80055000 },
]
```

## Current project state

**Phase 2 reached: the whole game recompiles to C.** Phases 1–2 are essentially done.

- ✅ **N64Recomp / RSPRecomp built** (GCC 16.1.0 / MinGW). Binaries copied to repo root
  (gitignored, with their MinGW runtime DLLs). See `tools/env.ps1` for the toolchain.
- ✅ **Symbols generated** from the splat disassembly via `tools/gen_symbols.py` →
  `syms/dump.toml` (1763 functions, symbol-TOML mode — no ELF needed since there's no
  MIPS assembler on this machine).
- ✅ **Clean full recompile**: `N64Recomp wcw.toml` exits 0, emitting **16 MB of C across
  36 `funcs_*.c` files** (all 1763 functions) + `funcs.h` + `recomp_overlays.inl` in
  `RecompiledFuncs/`. Reproduce with `. .\tools\env.ps1; .\tools\recompile.ps1`.
- ✅ **Overlays handled**: ambiguous `jal 0x80090000` correctly falls back to runtime
  function lookup (the overlay swap mechanism).

- ✅ **Stage-A build LINKS AND RUNS** (`cmake/coretest/`): the full recompiled game
  (16 MB / 1763 funcs) compiles with clang-cl and **links against the runtime core**
  (ultramodern + librecomp) into an executable that runs and registers the overlays
  (`build-coretest/wcw_coretest.exe` → "linked + registered overlays OK"). This validates
  the libultra integration end-to-end — every `_recomp` shim + recomp runtime API resolves.
  (Stage A caught one bad rename: `__osDispatchThread` is not a librecomp shim and is still
  called by unnamed libultra, so it must stay stubbed, not named.)

Still NOT done (later phases):
- ❌ `syms/data_dump.toml` (data symbols) — not needed yet for the code recompile; needed
  for patches and for naming data refs.
- ✅ **RT64 builds** with clang-cl (`build-rt64/rt64.lib`, 22 MB; configure standalone with
  `-DRT64_STATIC=TRUE`). Full DXC shader compilation (DXIL + SPIR-V) succeeded; deps
  (plume, re-spirv, zstd, nfd, SDL2) all built. The renderer long-pole is done.
- 🔶 **RSP microcode**: the **graphics** ucode (`D_8002AA70`/`D_8002BEA0`, built in
  `func_80002EAC`) is handled by **RT64** — not recompiled (confirmed: it reads RDP
  `DPC_CURRENT`). The **audio** ucode still needs `RSPRecomp`, but is best identified **at
  runtime** by logging the `OSTask` in `get_rsp_microcode` (static hunt didn't pin it down).
  See `rsp/README.md`. A first boot can return `nullptr` for audio (video works, no sound).
- ✅ **STAGE-B BUILDS AND BOOTS.** `build-msvc/WCWRecompiled.exe` (11.9 MB) links the whole
  stack (RecompiledFuncs + ultramodern + librecomp + recompui + recompinput + rt64 + SDL2).
  Build: `. .\tools\env-msvc.ps1; cmake -S . -B build-msvc -G Ninja -DCMAKE_C_COMPILER=clang-cl
  -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release; cmake --build build-msvc --target WCWRecompiled`.
  Build: `. .\tools\env-msvc.ps1; cmake -S . -B build-msvc -G Ninja -DCMAKE_C_COMPILER=clang-cl
  -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release; cmake --build build-msvc --target WCWRecompiled`.
  Getting here required: aligning lib/ submodules to BMHero's pinned commits, SDL2 FetchContent,
  DXC parent-scope vars, `patches/ui_funcs.h` + `recompui_event_structs.h`, and runtime
  contract fixes (RspUcodeFunc, register_config_path, start_game(id,mode), global
  `window`/`supported_games`, GameEntry.display_name, select_rom's game-id input).
- ✅ **RT64 + RENDER CONTEXT NOW INITIALIZE (D3D12, local desktop).** The earlier
  "`0xC0000409` abort in graphics-API interface creation / headless-session" conclusion was
  **WRONG**. The abort was `std::terminate`/`abort` (ucrtbase!abort) from an **uncaught C++
  exception on the gfx thread**, whose marker interleaved with the D3D12 markers and made it
  *look* like `D3D12CreateDevice` crashed. With a `std::set_terminate` handler + backtrace
  (added in `src/main/main.cpp`), `D3D12CreateDevice(Device8)` on the RTX 4070 SUPER
  **succeeds**; RT64 reports the device and `create_render_context` returns OK. The real boot
  blockers were **missing recompui init that BMHero's launcher does and our minimal main.cpp
  skipped** — fixed in `src/main/main.cpp` (in order encountered):
  1. **config tabs**: `recompui::config::create_general_tab/graphics/controls/sound + finalize()`
     — the input/render paths read these; absent → "General config has not been created yet".
  2. **fonts**: `register_primary_font("InterVariable.ttf",...) + register_extra_font(...)`,
     and an **`assets/` folder** in the working dir (copied from BMHero: fonts, `recomp.rcss`,
     `icons/`, `promptfont/`, `NotoEmoji`). `get_asset_path` = `./assets/...` on Windows.
  3. **program identity**: `recompui::programconfig::set_program_name/ set_program_id`.
  4. **`supported_games[0]`**: the entry must be pushed into the global `supported_games`
     vector (not just `register_game`'d) — recompui's `default_launcher_init_callback`
     dereferences `supported_games[0]`; empty → access violation.
- ✅ **THE PORT NOW BOOTS AND RUNS WITH NO CRASH** (2026-06-09). Confirmed by naming the
  boot-path libultra (see `disasm/libultra.md` for the full list + evidence) so ultramodern
  provides them instead of the game's recompiled MMIO-touching copies. The key fixes, in order:
  1. **`entrypoint_address` sign-extension bug** in `src/main/main.cpp`: it's a `gpr` (int64);
     the literal `0x80000400` is `unsigned int` → zero-extends to `0x0000000080000400`, and
     librecomp's `MEM_*` macro (`rdram + (addr - 0xFFFFFFFF80000000)`) then lands the initial
     boot DMA at `rdram + 0x100000400` (+4 GB, uncommitted) → AV. Fixed with
     `(gpr)(int32_t)0x80000400`. (Latent: earlier runs survived by luck.)
  2. **8 new libultra names** (this validated the MMIO theory — each crash was raw register
     I/O or an un-populated `__osRunningThread`/`__osThreadTail`): `osInitialize` (func_800112D0,
     was mis-mapped to the exception handler func_8001CD70 — now stubbed), `osGetThreadPri`,
     `osSetThreadPri`, `osCreateMesgQueue`, `osSetEventMesg`, `osCreatePiManager`,
     `osCartRomInit`, `osCreateViManager`.
  - Iterate with **`tools/cycle.ps1`** (regen symbols → N64Recomp → clang-cl build → run →
    symbolize crash frames against `build-msvc/WCWRecompiled.map`). The recompile+build is
    only a few seconds (incremental).
- ✅ **GAME LOGIC NOW RUNS** (2026-06-09, second pass). +3 more names got the game from "boots"
  to "runs game code": `osEPiStartDma` (**func_80011E20** — the overlay loader spun retrying it
  because the game's PI device-manager thread no longer drains the cmd queue once
  osCreatePiManager is ultramodern's; routing to `osEPiStartDma_recomp` does the DMA directly →
  **overlays load**), `osAiSetFrequency` (**func_80013DA0**), `osContInit` (**func_800172E0**).
  Thread model: `func_800004AC` = idle thread (sets up managers, spawns the worker + game-logic
  threads, lowers its priority, spins a PRNG); `func_80000CBC` = graphics scheduler (registers
  `0x8003CAB8` via osSetEventMesg for OS_EVENT_SP/DP/PRENMI, blocks in osRecvMesg for RSP/RDP
  completion); the driver is the **game-logic/overlay thread** (entry `ovl_swap_loop`). It now
  loads overlays and runs game code through audio init.
- ✅ **Controller reads (raw SI) — SOLVED by PIF emulation.** WCW rolls its own synchronous
  raw-SI controller layer (`func_80023C60` + 13 callers) on the libultra primitives
  `__osSiRawStartDma`=func_80023970 / `__osSiDeviceBusy`=func_800251E0 (raw `SI_STATUS @
  0xA4800018`, untrapped by `MEM_*`). Fixed by **naming both primitives** and giving
  `lib/N64ModernRuntime/librecomp/src/si.cpp` a custom 64-byte PIF/joybus emulation: the
  `*_recomp` shims latch the game's PIF command block, run joybus against host input
  (`osContGetReadData`), copy results back, and `send_si_message`. Carried the game past
  controller init.
- ✅ **KEYBOARD/PAD INPUT REACHES THE GAME (2026-07-05).** Root cause of "no input": nothing
  ever called `recompinput::poll_inputs()` — it latches `SDL_GetKeyboardState` + the controller
  list, and its only caller is `osContStartReadData` (via the `poll_input` callback), which WCW
  never calls (raw-SI path above). **Fix (`[wcw fix]`)**: new `ultramodern::input::poll_input()`
  (ultramodern input.hpp/cpp) invoked from `si.cpp::wcw_process_pif()` before
  `osContGetReadData`. Verified end-to-end by injecting WM_KEYDOWN into the game window: the
  throttled `[input]` log in si.cpp shows `buttons=1000` (Start) for Enter and `stick=(82,0)`
  for held D; the game polls ~60/s. Default keyboard map (recompinput `input_mapping.cpp`):
  Enter=Start, WASD=stick, Space=A, LShift=B, Q=Z, E/R=L/R, arrows=C buttons, IJKL=D-pad.
  **Gamepad also verified working** (same day):
  needed `recompinput::players::set_single_player_mode(true)` in `src/main/main.cpp` (BMHero
  does this; we didn't) — in the default multiplayer mode, controller reads require a pad
  explicitly assigned via the player-assignment UI (never shown), so pads were silently
  ignored while keyboard (which skips assignment) worked. Verified: user's XInput pad
  produces A/D-pad N64 bits end to end. **User confirmed full matches playable end to end
  with menus navigable (2026-07-05).** The temporary `[inpoll]`/`[input]` diagnostics were
  removed after verification (si.cpp / input_state.cpp).
- ✅ **SAVES WORK (2026-07-05, user-verified): Controller Pak emulation.** WCW saves ONLY
  to the Controller Pak (no cart EEPROM/SRAM — unusual; most ported games have batteries),
  and the recomp stack has no pak support (`Pak` enum only has RumblePak; osPfs* is
  unimplemented). But WCW's homegrown raw-SI driver speaks joybus directly through our PIF
  emulation, so `si.cpp` emulates a standard 32 KB pak in port 1: status byte reports
  pak-present (0x01), joybus cmd `0x02`/`0x03` = 32-byte block read/write with the mempak
  data CRC (poly 0x85), bank/ID region (>= 0x8000) reads zeros / acks writes without storing
  (mupen64plus semantics — this is how games distinguish mempak from Rumble Pak's 0x80s).
  Backing store = librecomp's save subsystem: `GameEntry.save_type = SaveType::Sram`
  (exactly 32 KB) in `src/main/main.cpp`, new host-pointer `save_read_ptr` in librecomp
  `pi.cpp`; pak contents persist to `saves/<game id>.bin` with async atomic writes + backup.
  Boot-verified: game runs accessory-detect (write/read `0x8001`), accepts the pak, writes
  filesystem blocks. One-shot `[wcw] first Controller Pak read/write` stderr markers remain.
- ✅ **VI display + AI + RSP-task submission — NAMED; first frame REACHES RT64.** osViSetMode/
  Black/SetEvent/SwapBuffer/GetCurrent+NextFramebuffer/SetSpecialFeatures (0x80012xxx),
  osAiGetLength/SetNextBuffer (0x80016xxx), osSpTaskLoad/StartGo + osDpSetNextBuffer. `vi.mq`
  now registered, mode set, `osSpTaskStartGo type=1` → `submit_rsp_task` → RT64 renders; window
  shows. See `disasm/libultra.md`.
- ✅ **Frame-sync deadlock — SOLVED (idle thread monopolized the cooperative scheduler).** A few
  frames in, ALL game threads blocked in `osRecvMesg` while the VI thread kept enqueuing retrace —
  but ultramodern's scheduler is **non-preemptive**, and the idle thread `func_800004AC` busy-spins
  `jal func_80011BF0(PRNG); b L_80000584` making no syscall, so it never yields → retrace is never
  delivered → deadlock. N64Recomp only injects `pause_self()` for a self-branch (`b .`); WCW's loop
  branches to the jal. **Fix = a one-instruction `wcw.toml` patch** rewriting the branch at
  `0x8000058C` to `b .` (`0x1000FFFF`) so the recompiler emits `pause_self()` (the correct
  cooperative idle-yield). Now the frame pipeline flows continuously and the game **advances A→B
  overlays**. Full analysis in `disasm/libultra.md`.
- ✅ **64-bit math (softfloat) cluster fixed.** `func_800134AC..func_80013918` was fully stubbed
  (garbage). Split: integer group (ddiv/ddivu/dmultu) un-stubbed → real code; FP<->int64 group
  NAMED to IDO `__d_to_ll`/`__f_to_ll`/`__d_to_ull`/`__f_to_ull`/`__ll_to_d`/`__ll_to_f`/
  `__ull_to_d`/`__ull_to_f` (4 missing `_recomp` shims added to librecomp `math_routines.cpp`).
- ✅ **Frozen game clock — FIXED.** `osGetCount` (`func_8001EB30` = `mfc0 $9`) was stubbed → the
  clock never advanced (timed waits frozen). Named `func_8001EB30`→`osGetCount`,
  `func_80023800`→`osGetTime` (removed from stubs). Real fix; didn't change rendering.
- ✅ **GAME-SIDE RENDERING FULLY EXONERATED (2026-06-12).** The "overlay B emits empty display
  lists / 6 frames then stop" conclusion was an artifact of a capped diagnostic log. Hard counters
  prove the game submits **~27 graphics tasks/second continuously** and has been drawing its full
  title screen all along (~30 tris + a screenful of texrects per frame: portrait grid + banners,
  animating fade colors, real textures + palettes, triple-buffered, presented with osViSwapBuffer
  + a healthy frame handshake). Everything game/recomp-side WORKS.
- ✅ **BLACK SCREEN SOLVED (2026-07-03) — THE GAME RENDERS.** Title screen, intro cinematics, and
  full attract-mode 3D (ring + crowd + animated wrestler) all render correctly, verified by
  RenderDoc render-target dumps AND a live-window screenshot. Root cause (found via RenderDoc
  capture + programmatic analysis, see `disasm/libultra.md` "BLACK SCREEN — SOLVED"): every game
  draw's VS output position was NaN. WCW's AKI-engine ucode **never loads a projection matrix** —
  it submits fully composed MVPs via `G_FORCEMTX` only — so RT64's `viewProjMatrixStack` top stays
  at its all-zero reset value, and `RSP::setVertexCommon` computed `inverse(zero)` = NaN, poisoning
  every world transform (`world = MVP × inv(VP)`) and zeroing the uploaded viewProj. **Fix** (in
  `lib/rt64/src/hle/rt64_rsp.cpp`, `[wcw fix]` tags): `isMatrixZero()` guard at the two consumers —
  a never-loaded zero VP is treated as identity, so `world = MVP` and `viewProj = identity`,
  preserving `viewProj × world = MVP`. Affects only forceMatrix-only games. lib/ is gitignored, so
  this patch must be preserved/reapplied if lib/rt64 is ever recloned (carried locally
  for good — upstreaming was declined; see `upstream/README.md`).
  Env `WCW_INSPECTOR=1` auto-opens RT64's frame inspector (renders, but needs input forwarding).
- 🔧 **RenderDoc workflow (validated, fully scriptable — how the black screen was cracked):**
  RenderDoc 1.44 at `C:\Program Files\RenderDoc`. `src/main/main.cpp` self-triggers
  `TriggerMultiFrameCapture(12)` at t+8s when `renderdoc.dll` is injected (multi-frame because the
  game submits ~27 gfx tasks/s vs ~60 presents — single frames can contain only the VI blit).
  Capture: `renderdoccmd.exe capture --working-dir build-msvc --capture-file <dir>\wcw
  --wait-for-exit build-msvc\WCWRecompiled.exe`, kill game after ~20s. Analyze headlessly:
  `qrenderdoc.exe --python script.py` (embedded Python; `pyrenderdoc.LoadCapture(...)` +
  `Replay().BlockInvoke(fn)`; write results to a file, kill qrenderdoc after). Can dump draw
  state, buffers (`GetBufferData`), postVS (`GetPostVSData`), and save render targets to PNG
  (`SaveTexture`) for visual verification without a human watching the screen.
- ✅ **ATTRACT-END DEADLOCK SOLVED (2026-07-04) — the game now runs its demo loop forever.**
  The game froze deterministically at graphics task #331 (end of the attract sequence): every
  game thread parked in `osRecvMesg`, process alive. Root cause was **ultramodern's external-
  message pump** (`wait_for_external_message` in `lib/N64ModernRuntime/ultramodern/src/
  mesgqueue.cpp`, used by `pause_self` — the patched idle thread is the pump): it processed
  ONE message per wake and re-enqueued failed deliveries immediately. moodycamel's
  ConcurrentQueue is not FIFO across producers (consumers prefer their own producer's
  sub-queue), so a few `requeue_if_blocked` messages aimed at full 1-deep queues (SI/PIF
  completions during the scene transition) spun in a closed dequeue→fail→requeue loop that
  **starved every other producer's messages — including the VI retrace** → no wakeups → total
  deadlock. **Fix (`[wcw fix]`)**: drain ALL available messages per wake, requeue failures
  after the drain, 1ms backoff if nothing was deliverable. **This is an upstream
  N64ModernRuntime bug** (any game can hit it). Two hardening fixes went in alongside:
  `osViSetEvent` now writes BOTH double-buffered ViStates (hardware semantics: latest call
  wins globally), and the VI retrace pacing counter reloads with `<= 0` + clamp-to-1 so one
  bad `retrace_count` read can't kill retrace forever (both in `ultramodern/src/events.cpp`).
- ✅ **F3DLX ucode-switch crashes fixed by 2 more libultra names (2026-07-04).** After the
  deadlock fix the game advanced to overlay B's second gfx ucode ("F3DLX 1.23.Rej (Variant)",
  text=0x2BEA0) and crashed twice in the scheduler's yield path — raw SP_STATUS MMIO in
  unnamed recompiled libultra. Named (in `tools/gen_symbols.py`): `func_80012C80` →
  `osSpTaskYield` (`__osSpSetStatus(SP_SET_SIG0)`), `func_80012760` → `osSpTaskYielded`
  (reads `__osSpGetStatus`=func_8001EB40, tests YIELD/YIELDED bits). sp.cpp provides both shims.
- ✅ **Audio: WORKING (2026-07-04).** The audio ucode (ROM `0x2A850`, text size `0xE20` — the
  OSTask's `0x1000` is the rounded DMA size) is RSPRecomp'd via `rsp/wcw_audio.toml` →
  `rsp/wcw_audio.cpp` (`wcw_audio_ucode`), returned by `get_rsp_microcode` for M_AUDTASK
  (no-op kept as fallback for unknown task types — never return `nullptr`, that quick-exits).
  It's stock aspMain, byte-identical to BMHero's, so BMHero's `extra_indirect_branch_targets`
  applied verbatim. CMake: `rsp/wcw_audio.cpp` added to SOURCES + `-march=nehalem` (SSE4.1
  for librecomp's VU intrinsics). Verified via the throttled `[audio]` peak log in
  `queue_samples`. See `rsp/README.md`.
- ✅ **MENU FLICKER + "MISSING ASSETS" ROOT-CAUSED (2026-07-05): RT64 frame interpolation.**
  With Framerate=Display (the old default), swapchain readback showed bursts of pure-black
  presents (same VI origin re-presented 2-6x black after one good frame) in menus/title;
  `WCW_NO_INTERP=1` → **zero** black presents. Cause: RT64's interpolated-frame generation
  assumes **1 workload = 1 game frame** (`rt64_workload_queue.cpp` line ~991 TODO), but WCW
  builds one frame from MULTIPLE RSP tasks — so interpolation re-renders only a slice of a
  frame (often just the pre-clear) → black/partially-drawn presents. Matches usually keep the
  whole 3D scene in one workload, which is why they looked fine. **Fix**: default Framerate
  = `RefreshRate::Original` (`[wcw fix]` in recompui `ui_config_tab_graphics.cpp`; also flipped
  the user's saved `build-msvc/graphics.json` since saved config overrides the default).
  Verified: 0 black presents, title screen pixel-perfect via BMP dump. Display mode remains
  selectable but will flicker until RT64 gets multi-workload frame detection + matrix-group
  patches (Phase 4). Capture tooling upgraded: `WCW_BMP_START`/`WCW_BMP_COUNT`/`WCW_LUM_END`
  env vars aim the present-BMP dump window at any moment (defaults 600/13/1500). Known minor
  artifact: overscan-edge garbage rows (thin line top, speckle bottom-left) — CRT-hidden on
  hardware; polish later.
- ✅ **A/V "FLICKERING" BOTH FIXED (2026-07-04).** Two independent root causes:
  1. **Audio crackle = constant SDL underruns** (~22/s: the device queue hit 0 on half the
     batches; gaps up to 79ms vs 46ms buffered). WCW registers NO AI event (verified:
     osSetEventMesg(OS_EVENT_AI) never called) — its audio thread generates one burst per
     ~33ms game frame sized by polling osAiGetLength, and ultramodern reports the whole host
     queue (hardware reports only the current DMA buffer), so the game kept ~0 headroom.
     **Fix**: `buffer_offset_frames = 6.0` in ultramodern `audio.cpp` (`[wcw fix]` — under-
     report by ~50ms so the game builds real depth; costs ~50ms latency) + SDL device chunk
     512 frames instead of 1024 in `main.cpp`. Verified: 0 underruns over 90s, 450-600-frame
     queue floor.
  2. **Video flicker = one PURE BLACK frame per game frame**, verified by a GPU swapchain
     readback (not screenshots — GDI capture of an MPO-promoted window returns bogus black
     frames; only trust the readback). Steady presented-luminance pattern [content, content,
     BLACK]. **Root cause: `src/main/main.cpp` passed `PresentationMode::PresentEarly`**
     (copied from BMHero) **to create_render_context.** PresentEarly fires a present whenever
     a workload writes a previously-displayed fb — correct for 1-gfx-task-per-frame games,
     but WCW builds each frame from MULTIPLE tasks and the LAST task of each frame PRE-CLEARS
     the next buffer (fbPair1), so RT64 presented the freshly-cleared buffer every frame.
     **Fix: PresentationMode::Console.** Verified: 0 black presents after boot, smooth
     luminance. Supporting fixes kept (all `[wcw fix]`): ultramodern `events.cpp` gfx thread
     now drains its action queue and drops stale ScreenUpdateActions (bounds scanout latency
     under backlog — upstream-relevant); RT64 present queue always locks `workloadMutex`
     before sampling the live target (present blit raced mid-render target states);
     plume `plume_d3d12.cpp` `copyTextureRegion` null-guard for buffer destinations (upstream
     bug — crashed on any texture→buffer readback); RT64 enhancement default Console.
  - **Diagnostics added (env-gated, keep):** `WCW_PRESENT_LUM=1` = GPU readback of every
    presented frame → `build-msvc/wcw_present_lum.csv` (n,ms,origin,addr,mean-luminance) +
    BMP dumps of presents 600-612 + timestamped CSV logs (submit/render/swap/vistate/pring/
    supd/dl) for full pipeline timeline reconstruction — this is how the flicker was found;
    `WCW_NO_INTERP=1` = disable RT64 interpolated-frame generation. `[audio]`/`[present]`
    stderr health lines are always on (1/s).
- 🔧 Diagnostics left in for continued debugging: `src/main/main.cpp` (`std::set_terminate`
  backtrace, `SetUnhandledExceptionFilter` with AV address, and an opt-in all-thread stack
  sampler gated on env `WCW_SAMPLE=1`); plus throttled `[wcw]…` logs in `lib/` (gitignored):
  `pi.cpp` rom-read bounds log, `events.cpp` VI null-guard + per-second tick, `mesgqueue.cpp`
  recv trace. The build links with `/MAP:WCWRecompiled.map`; resolve a Release frame RVA by the
  nearest "Publics by Value" symbol at/below it (preferred base `0x140000000`).
- 🔧 **Diagnostics added this session** (keep until boot is stable): `src/main/main.cpp` has a
  `std::set_terminate` handler and `SetUnhandledExceptionFilter` that print a symbolized
  backtrace (dbghelp). To symbolize Release frames: build links with `/MAP:WCWRecompiled.map`
  (set via `CMAKE_EXE_LINKER_FLAGS`); resolve a frame RVA by finding the nearest "Publics by
  Value" symbol at/below `RVA` (preferred base `0x140000000`). `lldb.exe` in VS LLVM is broken
  (missing `liblldb.dll`); cdb is not installed; WER LocalDumps need admin (HKLM).
- ❌ RSP microcode not yet recompiled (identified: embedded in main data → IMEM `0x84001000`).
- ✅ Runtime libs cloned into `lib/` (N64ModernRuntime, RecompFrontend, rt64) — gitignored,
  but **all local `[wcw fix]` changes are checked in as `lib-patches/*.patch`** (manifest +
  `apply.ps1`/`export.ps1` there). After ANY edit under `lib/`, run `.\lib-patches\export.ps1`
  and commit; after a reclone, run `.\lib-patches\apply.ps1`.
- ✅ Port toolchain installed: **VS Build Tools 2022 (clang-cl 19.1.5, MSVC 14.44, CMake,
  Ninja, lld-link)**. Load it with `. .\tools\env-msvc.ps1`.
- ✅ **The full recompiled output (all 36 `funcs_*.c`) compiles cleanly with clang-cl
  against the real N64ModernRuntime `recomp.h`** — the generated code is buildable with the
  actual port toolchain, not just the harness.
- ❌ Not yet: libultra `ignored` integration, `src/main` wiring to `recomp::start`, RSP
  microcode, and the full RT64/runtime build + link. These are the remaining Phase-3 work.

### Stubs/patches applied to get a clean recompile (see `wcw.toml`)
These are first-pass shortcuts, each documented inline in `wcw.toml`, that need proper
handling before the game runs correctly:
- **9 `cache` instructions** (4 funcs) patched to `nop` — correct on host (kept func logic).
- ~~**15-func softfloat/int64 helper library** stubbed~~ — RESOLVED (2026-06-11): the integer
  group (ddiv/ddivu/dmultu) is un-stubbed real code; the FP<->int64 group is named to the IDO
  softfloat routines (librecomp `math_routines.cpp`, 4 shims added). No longer stubbed.
- **15 libultra OS/exception/TLB funcs** (`mfc0`/`mtc0`/`tlb*`/`eret`) stubbed — runtime
  should provide these (osSetIntMask, exception vectors, thread dispatch, …).
- **2 handwritten-asm funcs** (`func_8001D2B4`, `func_80029B80`) stubbed — mis-split
  boundaries / RSP-IMEM branches in the OS layer.

### Execution proven (standalone harness, see `run/`)
We built a minimal harness (no RT64/runtime) that loads the ROM into a simulated 8 MB
RDRAM and actually executes the recompiled code. Result: it **runs faithfully**.
- Boot (`recomp_entrypoint`) runs cleanly: BSS-clear → `game_main` → libultra init,
  `osCreateThread`(game thread `func_800004AC`), `osStartThread`, return. No crashes.
- The game thread (`func_800004AC`), invoked directly, executes real game code at ~86 M
  recompiled-calls/sec and busy-waits in `func_80011BF0` (a PRNG) — i.e. it polls for a
  hardware/IPC condition the toy harness never signals.
- Conclusion: memory model, calling convention, control flow, and arithmetic all work.
  The only thing blocking further progress is the real runtime (hardware/threads/video),
  i.e. Phase 3. Nothing indicates a fault in the recompiled code itself.

## Roadmap (do these in order)

### Phase 1 — Disassemble the ROM to get symbols
There is no existing decomp, so we make our own symbol context. Standard tool:
[splat](https://github.com/ethteck/splat) (N64 binary splitter) which yields an ELF +
symbol map. Steps:
1. Set up a splat project under `disasm/` (yaml config pointing at the WCW ROM; detect
   compression, segments, code vs data).
2. Run splat to split the ROM and assemble an ELF with symbols.
3. Iterate until the ELF disassembles/links cleanly. This is the bulk of the early work.

Alternatively, author `syms/dump.toml` directly, but for a 12 MiB commercial game the
disassembly route is far more practical.

### Phase 2 — Build the recompiler and run it
1. Build N64Recomp: `cmake` + `cmake --build` in `C:\Users\selki\depot\N64Recomp`
   (needs CMake ≥ 3.20, a C++20 compiler, submodules initialized recursively).
2. Copy `N64Recomp.exe` and `RSPRecomp.exe` to this project root.
3. Run `.\N64Recomp wcw.toml` → populates `RecompiledFuncs/`.
4. Identify the audio (and any graphics) RSP microcode in the ROM; fill in `rsp/aspMain.toml`
   equivalents and run `.\RSPRecomp <ucode>.toml`.

### Phase 3 — Stand up the runtime + build  (IN PROGRESS)
1. ✅ Runtime libs cloned into `lib/` (N64ModernRuntime, RecompFrontend, rt64). Cloned as
   plain repos for now (gitignored); convert to proper submodules later. WCW has no decomp
   lib to add, unlike BMHero's `lib/bmhero`.
2. ⏳ Toolchain: needs **clang/clang-cl + MSVC + Windows SDK** (RT64 is D3D12/Vulkan, built
   with clang-cl — MinGW can't substitute). Install is a UAC-gated VS Build Tools install.
3. Flesh out `src/main/` (main.cpp wiring → `recomp::start` with gfx/input/audio/rsp/thread
   callbacks; model on BMHero's ~800-line `src/main/main.cpp`). `GameEntry` needs
   `entrypoint_address = 0x80000400`, `entrypoint = recomp_entrypoint`, ROM hash.
4. Build with CMake (clang-cl/ninja); boot, debug.

**⚠️ The big Phase-3 prerequisite — libultra identification.** In a normal recomp
(BMHero/Zelda), libultra functions have *names* from the decomp, so the recompiler
`ignored`s them and **ultramodern** provides host-integrated implementations (threading,
DMA, VI retrace, message queues, controller I/O). WCW has **no symbol names** — every
function is `func_XXXX`, so the recompiler recompiled the game's *own* embedded libultra
(`func_80011560` = osCreateThread, `func_800116B0` = osStartThread, …). Those don't
integrate with ultramodern's scheduler/timing — which is exactly why the `run/` experiment's
game thread busy-waits forever. So before the port can boot, we must **identify the
libultra boundary** (signature-match the game's `func_XXXX` against a known libultra
version), give them real names, and mark them `ignored` in `wcw.toml` so ultramodern takes
over the OS layer. This is analysis-only (no toolchain needed) and can proceed in parallel
with the toolchain install. It is the real bulk of Phase 3.

5. Recompile the RSP microcode (audio + graphics ucode) with `RSPRecomp` — required for
   the `get_rsp_microcode` callback and for any rendering/audio. (Ucode already located:
   embedded in main data, DMA'd to IMEM `0x84001000`.)

### Phase 4 — Patches & enhancements
Game-behavior fixes and PC enhancements live in `patches/` as MIPS-compiled C using
`RECOMP_PATCH` / `RECOMP_EXPORT` (see `patches/` + `patches.toml`). Widescreen, high-FPS
interpolation (RT64 matrix groups), input options, etc. — model on BMHero's `patches/`.

## Layout of this repo

```
wcw.toml              Main recompiler config (symbol-TOML mode by default)
patches.toml          Recompiler config for the C patches (single-file output mode)
CMakeLists.txt        Build (template adapted from BMHero — verify before relying on it)
syms/                 Symbol metadata: dump.toml, data_dump.toml (GENERATED — phase 1/2)
disasm/               splat disassembly project (phase 1)
patches/              C patches compiled to MIPS, + linker scripts + Makefile
  patch_helpers.h       RECOMP_PATCH/EXPORT-friendly DECLARE_FUNC macro
  patches.h             pulls in patch_helpers
  patches.ld            places patch sections into reserved extra RAM
  syms.ld               maps recomp_* runtime API calls to dummy vram addresses
  required_patches.c    boot/overlay-loader patches (STUB — fill once overlays known)
  dummy_headers/        empty headers to satisfy includes when cross-compiling
src/
  main/                 launcher + runtime registration glue
  game/                 config / API bridge code (recomp_api etc.)
include/              project C/C++ headers
lib/                  git submodules (runtime, frontend, renderer) — NOT yet added
RecompiledFuncs/      recompiler output (GENERATED, gitignored)
RecompiledPatches/    patch recompiler output (GENERATED, gitignored)
rsp/                  RSP microcode recomp config + output
tools/                helper scripts
```

## Conventions

- C++ namespace for this game's glue code: `wcw` (BMHero uses `banjo`).
- Generated dirs (`RecompiledFuncs/`, `RecompiledPatches/`, build dirs) are gitignored.
- Never commit ROM data or extracted assets.
- Use PowerShell syntax for shell commands on this machine.
- When adapting from BMHero, replace: `BMHeroRecompiled`→`WCWRecompiled`,
  `banjo`/`bk`→`wcw`, `bmhero.z64`→the WCW ROM, `BMHeroSyms`→`syms`.

## Phase 1 progress / known ROM layout

splat is set up under `disasm/` (venv + `wcw.yaml`) and the fixed code segment splits
cleanly. Initial recon (see `disasm/README.md` for detail) established:

- **Compiler: IDO** (auto-detected) — the well-supported case for N64Recomp.
- **Total code ≈ 800 KB / ~2,000 functions**, in two ROM regions only:
  - `0x001050`–`0x02A9B0` (~170 KB): fixed `main` segment — splits cleanly today.
  - `0xA20000`–`0xAC0000` (~640 KB): **overlay region**, all dynamically-loaded code,
    packed contiguously (not scattered).
  - The ~10 MB in between is assets (no code).
- **Overlay system SOLVED** (see `disasm/README.md` for the full map): exactly **two CPU
  overlays** that swap at VRAM `0x80090000`, described by 9-word descriptors at
  `0x80032BA0` / `0x80032BC4`, loaded by `0x8000075C` and entered via `0x800007C8`.
  Both now disassemble cleanly in `wcw.yaml`. They load at a **fixed address** (no
  per-load relocation), which keeps the recomp simpler.
  - Overlay A: ROM `0xA21750`–`0xA69570`; Overlay B: ROM `0xA69570`–`0xAC2970`.
- **RSP microcode** is embedded in the main data section (~`0x8002AC10`) and DMA'd to IMEM
  `0x84001000`. RSPRecomp's job, not a CPU overlay.

## Open questions to resolve early
- Precise RSP microcode bounds (audio) and the graphics ucode variant (F3DEX family).
- Data/rodata/jump-table boundary refinement within each segment (normal splat iteration).
- ~~Overlay table / layout?~~ Solved — two fixed-address swapping overlays at `0x80090000`.
- ~~Are overlays relocatable?~~ No — fixed load address.
- ~~Is the code compressed?~~ No standard compression; the code regions are plain MIPS.
- ~~Which compiler?~~ IDO.
