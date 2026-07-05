# libultra identification (Phase 3)

## KEY: the integration is just a NAMING problem (validated)
N64Recomp has **built-in** name sets (`N64Recomp/src/symbol_lists.cpp`):
`reimplemented_funcs`, `ignored_funcs` (essentially all of libultra + exceptions + ido
math), and `renamed_funcs` (libc: memcpy/sinf/malloc/…). When a function is **named** with
one of these, the recompiler *automatically* skips emitting it and the runtime provides it
— **no `ignored` entry in `wcw.toml` is needed** (adding one actually errors, since the
function is auto-renamed to `<name>_recomp` first).

So libultra integration = **give each `func_XXXX` its correct libultra name** in
`tools/gen_symbols.py`'s `RENAME` map → regen `syms/dump.toml` → recompile. As a function
gets named, also remove it from the placeholder `stubs`/cache-nop lists in `wcw.toml`.

**Mechanism verified:** naming `func_80011560` → `osCreateThread` makes the recompile emit
no game `osCreateThread` (it's `_recomp`/dropped) and point the section-table lookup at the
runtime's `osCreateThread`. Recompile stays clean (exit 0).

## SCOPE: the naming task is BOUNDED to ~31 functions (not all of libultra)
The runtime only provides shims (`librecomp/src/ultra_translation.cpp`) for the OS **core**.
**Only name functions in this list** (naming others risks a link error if something still
calls them):
```
__osDisableInt __osInitialize_common __osRestoreInt __osSetFpcCsr
osCreateMesgQueue osCreateThread osDestroyThread osGetCount osGetThreadId osGetThreadPri
osGetTime osInitialize osInvalDCache osInvalICache osJamMesg osRecvMesg osSendMesg
osSetCount osSetEventMesg osSetIntMask osSetThreadPri osSetTime osSetTimer osStartThread
osStopThread osStopTimer osViSetEvent osVirtualToPhysical osWritebackDCache
osWritebackDCacheAll osYieldThread
```
**The VI-display (osViSwapBuffer/osViSetMode/osViBlack), controller (osCont*), PI DMA
(osPiStartDma), and AI (osAi*) functions are NOT named** — they stay recompiled and work
via librecomp's hardware-register handling + the renderer/input callbacks. This shrinks the
job from "~80 libultra funcs" to a bounded ~31.

> ⚠️ **CORRECTION (boot debugging, see CLAUDE.md Stage-B):** the claim above that VI/SI/PI/AI
> device drivers "stay recompiled and work via librecomp's hardware-register handling" is
> **WRONG**. `recomp.h`'s `MEM_W` is `*(int32_t*)(rdram + (addr - 0x80000000))` — it does NOT
> trap MMIO. When the recompiled `func_800251E0` (SI status) reads `0xA4800018`, it indexes
> `rdram + 0x24800018` (out of bounds) → access violation during boot. These device drivers
> do raw register I/O and therefore **must be named/ignored** so ultramodern provides the host
> implementations (as it does for `osViSetMode`/`osContStartReadData`/`osPiStartDma`/`osAi*`,
> all present in `ultramodern`/`librecomp`). The "~31" core list is the *minimum*; the
> boot-path controller/SI/PI/VI drivers must be added too.

**Named & auto-ignored so far (22):** `osCreateThread` (80011560), `osStartThread`
(800116B0), `__osDisableInt` (80012160), `__osRestoreInt` (80012180), `osSendMesg` (80011C20),
`osRecvMesg` (80011F00), `osInvalDCache` (80011D70), `osInvalICache` (80012040),
`osWritebackDCacheAll` (800128B0), `osWritebackDCache` (80013C80), `osSetIntMask` (800126C0),
`osVirtualToPhysical` (80013C00), and **newly verified by boot-debugging (2026-06-09)**:
`osInitialize` (**800112D0** — game_main's first call; sets exception vectors/caches/TLB,
reads osResetType/TvType), `osGetThreadPri` (8001D940), `osCreateMesgQueue` (80011AE0 —
mtqueue/fullqueue=&__osThreadTail), `osCreatePiManager` (80011800 — builds __osPiDevMgr),
`osCartRomInit` (80011990), `osSetThreadPri` (80011B10), `osSetEventMesg` (800125E0 —
__osEventStateTab[e]), `osCreateViManager` (800121A0 — builds __osViDevMgr, VI mgr thread).
All in `tools/gen_symbols.py` RENAME, each with inline evidence.

> **CORRECTION:** `osInitialize` was previously (wrongly) mapped to `func_8001CD70`. That
> address is the handwritten **exception handler** (`__osException`: k0/k1, mfc0 $12/$13,
> saves context to __osRunningThread @ D_80033A90), referenced only by-address inside
> osInitialize to install the exception vectors. It is now **stubbed** in `wcw.toml` (never
> called once osInitialize is ignored). The real `osInitialize` is `func_800112D0`.
> Also dropped the bogus `__osDispatchThread`(8001D4F4) rename — it stays stubbed.

**RESULT: the port BOOTS, loads overlays, and runs GAME LOGIC.** With a sign-extension fix to
`entrypoint_address` (`src/main/main.cpp`; it's a `gpr`, the bare `0x80000400` literal
zero-extended → +4GB rdram offset in the boot DMA) plus the names above, the game boots,
spawns three threads, the **game-logic thread (entry `ovl_swap_loop`, created by
`func_800110D0`) loads overlays via DMA and runs game code** — through audio + controller init.

**Thread model understood:** `func_800004AC` is the **idle thread** (`game_main`'s thread:
sets up PI/VI managers, creates the worker + game-logic threads, lowers its own priority, then
`while(1) func_80011BF0()` (a pure PRNG, no yield)). `func_80000CBC` is the **graphics
scheduler** — it registers queue `0x8003CAB8` via `osSetEventMesg` for **OS_EVENT_SP(4)/
DP(9)/PRENMI(14)** and blocks in `osRecvMesg` for RSP/RDP completion. The real driver is the
**game-logic/overlay thread**.

**3 more names got the game logic running (2026-06-09, second pass):**
`osEPiStartDma` (**func_80011E20** — the overlay loader spun retrying this because the game's
PI device-manager thread no longer drains the cmd queue; routing to librecomp's
`osEPiStartDma_recomp` does the DMA directly → overlays load), `osAiSetFrequency`
(**func_80013DA0**), `osContInit` (**func_800172E0**).

**UPDATE (2026-06-11): controllers (raw SI), VI display, and AI all NAMED — the port now
BOOTS, RENDERS to RT64, and runs continuously.** Progress since the SI frontier below:
- **Raw SI solved by emulation, not just naming.** `__osSiRawStartDma` (func_80023970) and
  `__osSiDeviceBusy` (func_800251E0) are named, and `librecomp/src/si.cpp` was given a custom
  PIF/joybus emulation (`*_recomp` shims that run the 64-byte PIF command block against host
  input via `osContGetReadData`, then `send_si_message`). This carried the game-logic thread
  past `func_80023C60` and through controller init without a real SI MMIO trap.
- **VI display setters/getters NAMED** (the missing retrace heartbeat — `vi.mq` was 0, mode
  NULL, so the whole frame pipeline stalled). All in `0x80012xxx`, operating on the
  `__OSViContext` globals (`__osViNext`@`D_80033B24`, `__osViCurr`@`D_80033B20`; struct: 0x0
  state(u16), 0x2 retraceCount, 0x4 framep, 0x8 modep, 0x10 msgq, 0x14 msg — confirmed against
  `__osViSwapContext` func_8001E7D0): `osViSetMode`(func_80012500), `osViBlack`(func_80012570),
  **`osViSetEvent`**(func_80012650 — registers the retrace queue, THE fix),
  `osViGetCurrentFramebuffer`(func_800127E0), `osViGetNextFramebuffer`(func_80012820),
  `osViSwapBuffer`(func_80012860), `osViSetSpecialFeatures`(func_80012FB0). NOTE: the timer
  funcs `func_8001E280/E30C/E484/E4F8` (operate on `__osTimerList`@`D_80033AB0`) were earlier
  MISLABELED as VI setters — they are NOT; the real VI setters are the `0x80012xxx` list above.
- **AI funcs NAMED** (raw AI MMIO @ `0xA450xxxx` crashed): `osAiGetLength`(func_80016BF0),
  `osAiSetNextBuffer`(func_80016B40). `__osAiDeviceBusy`(func_800211C0) is now dead (only
  osAiSetNextBuffer called it). `osAiSetFrequency`(func_80013DA0) was already named.
- **Audio ucode = no-op (silent) for now.** The game submits an M_AUDTASK RSP task with
  ucode `0x80029C50` / data `0x80037530` / size `0x1000` (located at runtime — see
  `rsp/README.md`). `get_rsp_microcode` returning nullptr made `task_thread_func` FATALLY
  exit; `src/main/main.cpp` now returns a no-op ucode (`RspExitReason::Broke`) so the game
  runs with silent audio. Recompiling this ucode with RSPRecomp is the path to real audio.

**RESULT:** `vi.mq=0x8003CAB8`, mode set, **a gfx task reaches RT64** (`osSpTaskStartGo type=1`
→ `submit_rsp_task` → renderer), the window shows ("WCW vs. nWo World Tour: Recompiled"), and
the process no longer crashes. Screen is currently **black** because of the frontier below.

**SOLVED (2026-06-11): the "frame-sync stall" was the IDLE THREAD MONOPOLIZING the cooperative
scheduler.** Root cause (found by sampling: ALL game threads blocked in `osRecvMesg` a few
frames in, while the VI thread kept ticking — so VI **retrace** was being enqueued ~continuously
to `0x8003CAB8` but never delivered to the blocked scheduler `func_80000CBC`). ultramodern's
scheduler is **cooperative/non-preemptive**: external messages (retrace) are only delivered when
a game thread blocks or makes a message syscall (`dequeue_external_messages` / `wait_for_external_message`).
The idle thread `func_800004AC` (after spawning managers + `osSetThreadPri(0)`) busy-spins
`L_80000584: jal func_80011BF0 (PRNG); b L_80000584` — it makes NO syscall, so it never yields
and monopolizes the CPU once boot setup finishes. On real N64 the idle thread is preempted by
interrupts; here there's none.
N64Recomp only auto-emits `pause_self()` for a **self-branch** (`b .`, where `branch_target ==
instr_vram`; `recompilation.cpp`), and WCW's loop branches to the `jal`, not itself — so no yield
was injected. **Fix: a one-instruction patch in `wcw.toml`** rewrites the branch at `0x8000058C`
into `b .` (`0x1000FFFF`) so the recompiler emits `pause_self()` there — turning the spin into the
correct cooperative idle-yield (perpetually drains external messages + yields to higher-priority
threads). (The idle thread's PRNG churn is lost; harmless — 21 other callers advance the same seed.)
**RESULT:** the frame pipeline now flows **continuously** (frame-ready/done every frame, retrace
delivered every frame), and the game **advances through its overlay logic — it now swaps from
overlay A to overlay B** (first time ever). 5 gfx tasks/20s submitted to RT64. No deadlock.

**REMAINING (2026-06-11): screen still black — the per-frame DRAW PATH is state-gated and rarely
taken.** Investigated extensively; characterized but not yet fixed. Findings:
- The frame loop runs every frame, but a display list is built only ~6 times then never again.
  Timeline: overlay A loads → **6 gfx tasks** (`osSpTaskStartGo type=1`) → overlay B loads → **0
  gfx tasks**. Both overlays exhibit sparse drawing (it's not B-specific).
- **Overlay dispatch is CORRECT** (ruled out): a `get_function(0x80090000)` log proved ENTER #1 →
  `func_80090000` (A) and ENTER #2 → `func_80090000_ovl_b` (B) — overlay B's recompiled code
  really does execute after the swap (func ptrs matched the map RVAs exactly). So it's not an
  A-code-on-B-data bug.
- **Not input-gated** (ruled out): sending keyboard/controller input (Enter/Space/Z/X/arrows via
  SendKeys to the focused window) produced no new gfx tasks or overlay changes.
- **64-bit math now correct** (ruled out as the cause): the softfloat/64-bit helper cluster
  (`func_800134AC..func_80013918`) was fully stubbed (returning garbage). Split + fixed: the
  integer group (ddiv/ddivu/dmultu) is un-stubbed real code; the FP<->int64 group is NAMED to IDO
  `__d_to_ll/__f_to_ll/__d_to_ull/__f_to_ull/__ll_to_d/__ll_to_f/__ull_to_d/__ull_to_f` (4 missing
  `_recomp` shims added to librecomp `math_routines.cpp`). Rendering behaviour was unchanged → not
  the cause, but it's a real correctness fix worth keeping.
- The game does NOT respond to a 64-bit-time or asset stall in any obvious place; audio is no-op
  (silent), which is a candidate (intro/logo timing is often synced to audio playback position via
  osAiGetLength) but unconfirmed.
**Update (2026-06-11, pass 2): clock fixed + graphics pipeline fully mapped; root cause narrowed
to overlay B producing empty display lists.**
- **Frozen clock — FIXED (ruled out as the black-screen cause).** `osGetCount` (`func_8001EB30` =
  `mfc0 $v0,$9`, the COP0 count register) was **stubbed**, so the game clock never advanced and any
  timed wait was frozen. Named `func_8001EB30`→`osGetCount` and `func_80023800`→`osGetTime`
  (removed osGetCount from wcw.toml stubs). Real fix, but rendering behaviour was unchanged.
- **Graphics pipeline mapped** (via a backtrace at `osSpTaskStartGo`): the gfx task is submitted by
  the **scheduler thread `func_80000CBC`** → retrace handler `func_80000E20` / SP handler
  `func_80000EFC` → `func_800011CC` (which calls `osSpTaskLoad`+`osSpTaskStartGo`). `func_80000E20`
  only submits when the scheduler struct's swap-ready flag (`+0x280`, set from `+0x264`) is set AND
  its task list (`+0x274`) is non-empty. The **graphics manager thread `func_80003450`** owns the
  frame-ready/done handshake (`0x80040FC0`/`0x80040FE0`) with the overlay and feeds the scheduler.
- **Root cause narrowed:** overlay A submits **6** gfx tasks then loads overlay B which submits
  **0** — yet the frame-ready/done handshake keeps flowing in B. So overlay B sends frame-ready
  every frame but with an **empty/absent display list**, so the manager builds no task. The overlay
  render fn `func_800C34E8(a0)` has a draw-enable gate: `a0==1` → it **skips the scene draw calls**
  (`0x800C356C-3590`, four `jal 0x800C2E2C`) and jumps to `L_800C3594`; `a0!=1` → it draws. So the
  caller's `a0` decides whether anything is drawn. (The 6 black frames in overlay A are likely a
  legitimate boot framebuffer-clear — RT64 reports no ucode/DL errors, so the renderer is probably
  fine; the issue is the game not emitting geometry.)
**Update (2026-06-11, pass 3): the draw gate is NOT the cause — overlay B's scene is genuinely
empty (upstream state stall).** A temporary instruction patch forced `func_800C34E8`'s scene-draw
path (`0x800C355C` imm `1`->`2`, defeating the `a0==1` skip at `0x800C3564`) — the screen stayed
**black**, so even running the four `jal 0x800C2E2C` draw calls emits no geometry. So the game
isn't merely choosing a no-draw path; it has **no scene content to draw**. The black screen is an
upstream game-state stall in overlay B: its per-frame logic runs but never populates a scene.
- The dispatcher gate `func_80099238` (called by `func_800C3A6C`, return value gates the
  per-frame processing) is a counter routine over overlay-B globals `0x800E02DA` (a byte flag,
  tests bit 0x80) and `0x800E62FC` (a counter) — a transition/tick timer. Whatever advances the
  game's screen-state through this counter isn't completing.
**Update (2026-06-11, pass 4): the game state machine IS advancing — the stall is inside overlay
B's own state, not a hard freeze.** Added a `gamemode` watch to the VI-tick log (reads the
fixed-segment state var `0x80034834`). It reads **0 during overlay A, flips to 1 the moment overlay
B loads (~tick 661), then sticks at 1**. So the top-level mode advances A→B; overlay B then runs
its per-frame code but never advances its internal screen-state to a rendering one. The overlay-B
dispatcher `func_800C3A6C` reads `0x80034834`==1 → takes the `L_800C3AE8` render path → `func_80099238`
(a counter routine over overlay-B bss `0x800E02DA`/`0x800E62FC`) whose return gates the render
(`0x800C3B1C: beq $v1,$zero`→skip). CAUTION: overlay A and B alias the same `0x800E_xxxx` bss, so
those counters can't be reliably watched from the (async) VI thread — only the fixed-segment
`0x80034834` is trustworthy from there. To watch overlay-B-internal state, instrument from the game
thread or use fixed-segment globals.

**Controller-presence — ruled out (but kept the fix).** `osContGetReadData` reports `err_no =
CONT_NO_RESPONSE_ERROR` whenever the input callback returns no response that frame, so `si.cpp`'s
PIF emulation was telling the game "no controller in port 1" when the host pad/keys were idle.
Made port 1 (channel 0) ALWAYS report connected (`si.cpp`, virtual controller always present,
neutral input when idle) — correct behavior, but it did NOT change the stall (still 6 gfx tasks,
stuck at gamemode 1). So WCW isn't gating progression on controller presence here.

**Refined picture:** in overlay A (gamemode 0) the game draws its **6 frames immediately at the
start**, then does ~10 s of non-graphical work (asset loading — the `rom_read` burst) with NO
rendering, then flips to gamemode 1 (overlay B) which renders nothing. So overlay A is a brief
boot/init+load phase and overlay B is the screen that should render but stays empty.

---

## ATTRACT-END DEADLOCK — SOLVED (2026-07-04). THE PORT RUNS THE FULL DEMO LOOP.

**Symptom:** deterministic freeze at graphics task #331 (~12 s in, end of the attract
sequence): every game thread parked in `osRecvMesg` (tid2→0x800478A8, tid3→0x80040F68,
tid4/gfx-sched→0x8003CAB8, tid6/logic→0x80040FE0, tid11→0x80047798), process alive, VI thread
ticking. User-visible as a "frozen ring".

**Method that cracked it:** parked-thread registry (which mq each blocked thread waits on) +
late-window send/recv trace + producer backtraces on `enqueue_external_message` + thread-id→
entry-PC mapping. Key observations: retrace externals `(0x8003CAB8, 0x29A)` kept being
ENQUEUED 60/s, but post-freeze the pump (idle thread tid=1, entry 0x800004AC, sitting in
`pause_self` per our wcw.toml patch) delivered ONLY `(0x80047C40/CCC, 0x0)` at ~360/s —
failed deliveries into full 1-deep queues, cycling forever.

**ROOT CAUSE (upstream N64ModernRuntime bug):** `ultramodern::wait_for_external_message`
processed ONE message per wake and re-enqueued failed `requeue_if_blocked` deliveries
immediately. moodycamel's ConcurrentQueue is NOT FIFO across producers — a consumer drains
its own producer's sub-queue first — so the pump's own requeues formed a closed
dequeue→fail→requeue loop that permanently starved every other producer's messages,
including the VI retrace. No retrace → gfx scheduler never wakes → all threads starve.
**FIX** (`ultramodern/src/mesgqueue.cpp`, `[wcw fix]`): drain ALL available messages per
wake (like `dequeue_external_messages`), requeue failures only after the drain, and sleep
1 ms when nothing was deliverable.

**Hardening fixes found en route (both `ultramodern/src/events.cpp`):**
- `osViSetEvent` only wrote `get_next_state()`; the double-buffered ViStates never copy to
  each other, so a stale registration could survive on the other parity and (via the shared
  `remaining_retraces` counter) starve the real one. Now writes BOTH states (hardware
  semantics: the retrace event is global, latest call wins).
- Retrace pacing: `remaining_retraces == 0` + unclamped reload meant one bad
  `retrace_count=0` read (e.g. mid state-swap) would go negative and kill retrace forever.
  Now `<= 0` + reload clamped to ≥1.
- Also repaired in `mesgqueue.cpp::do_send`: an earlier diagnostic insertion had rebound the
  blocking-send `else { while(FULL) wait }` to a msg-value check.

**Then two crashes at the overlay-B ucode switch** (game loads its second gfx ucode
"F3DLX 1.23.Rej (Variant)" text=0x2BEA0; scheduler yields the running task first): raw
SP_STATUS MMIO in unnamed libultra. Named `func_80012C80` → **osSpTaskYield** (single call
`__osSpSetStatus(0x400 /*SP_SET_SIG0*/)`) and `func_80012760` → **osSpTaskYielded** (reads
`__osSpGetStatus` = func_8001EB40 `lw SP_STATUS`, tests 0x100 YIELDED / 0x80 YIELD, sets the
task yield flag). Both in `tools/gen_symbols.py` RENAME; sp.cpp shims exist.

**Verified (2026-07-04):** 100 s runs with no crash/freeze; dpc 6000+ and climbing at
~90 tasks/s; gamemode cycles 0→1→0 (intro → cinematics/title → attract, looping); RenderDoc
backbuffer dump during the "dark" gamemode-1 phase shows the wrestler intro cinematic (Sting
+ bat over crowd camera flashes) rendering correctly — that phase is mostly-black by design.
The old "audio gates progression" theory is dead: the game loops fine with silent audio.

---

## BLACK SCREEN — SOLVED (2026-07-03). THE PORT RENDERS THE GAME.

**Verified rendering:** RenderDoc render-target dump shows the intro's spinning WCW-logo mat;
a live-window screenshot shows full attract mode (3D ring, blue ropes/turnbuckles, crowd,
animated wrestler taunting). ~9,300 draws/sec executing, stable, no crash.

**ROOT CAUSE:** every game draw's VS output position was NaN (RenderDoc postVS: all `nan`),
so the rasterizer silently produced zero pixels — on both APIs, exactly matching the
"pixels never reach the pixel shader" bisect. The NaN chain:
1. WCW's AKI-engine ucode **never loads a projection matrix** — it submits fully composed
   MVP matrices via `G_FORCEMTX` only (confirmed: `RSP::forceMatrix` receives sane MVPs while
   `viewProjMatrixStack` top is all-zero, its reset value).
2. `RSP::setVertexCommon` (modelViewProjInserted branch) computes
   `invViewProjMatrixStack = inverse(viewProjMatrixStack top)`. `hlslpp::inverse(0)` = NaN.
3. `worldTransforms entry = mul(mul(MVP, invVP=NaN), extended.viewProj)` = **NaN matrices**
   (GPU dump: `worldTransforms[1..7]` all NaN while [0] identity stayed clean); and
   `viewProjTransforms entry = mul(identity, VP=0)` = **zero matrix** (GPU dump: confirmed).
4. `RSPProcessCS` then computes screenPos through those matrices → NaN for every vertex →
   no rasterization, no error. Colors/texcoords don't touch matrices → they were correct
   (which is what pointed at the transform inputs).

**FIX** (`lib/rt64/src/hle/rt64_rsp.cpp`, tagged `[wcw fix]`): `isMatrixZero()` helper +
guards at both consumers — `addCurrentProjection` (view/proj/viewProj drawData entries) and
`setVertexCommon` (invViewProjMatrixStack). A never-loaded all-zero VP is treated as
**identity**, so `world = MVP` and `viewProj = identity`, preserving the invariant
`viewProj × world = MVP`. Only affects forceMatrix-only games (all AKI wrestlers: WCW/WWF —
likely fixes Virtual Pro Wrestling / No Mercy ports too). lib/ is gitignored: reapply this
patch if lib/rt64 is recloned; good candidate for upstreaming to RT64.

**How it was found (repeatable workflow):** RenderDoc 1.44 installed. `src/main/main.cpp`
self-triggers `TriggerMultiFrameCapture(12)` at t+8s when running injected (multi-frame
because game workloads land in only ~27 of 60 present intervals). Capture:
`renderdoccmd.exe capture --working-dir build-msvc --capture-file <out> --wait-for-exit
WCWRecompiled.exe`. Analyze WITHOUT a human: `qrenderdoc.exe --python script.py` (embedded
Python: `pyrenderdoc.LoadCapture(...)`, `pyrenderdoc.Replay().BlockInvoke(fn)`, write
findings to a file, kill qrenderdoc). Used to dump per-draw pipe state, input-layout, VB/IB
contents (`GetBufferData`), postVS (`GetPostVSData`), compute-dispatch bindings via shader
reflection, and render targets to PNG (`SaveTexture`) for visual proof. Note: RenderDoc
shows never-written GPU memory as `0x7FFFFFFF` — that pattern reading back uniformly means
"nothing ever wrote this", which is itself evidence.

**Superseded prime suspect:** the vertex-buffer upload path was innocent — BufferUploader,
input layout, struct packing, delta ranges, and both uploader executions all verified
correct along the way.

---

## (historical) BLACK-SCREEN DEEP DIVE (2026-06-12, Fable session) — GAME FULLY EXONERATED;
## defect isolated to RT64 draw execution. (RESOLVED above — kept for the evidence trail.)

**THE BIG REVERSAL:** the previous session's "game stops rendering after 6 frames / overlay B
draws nothing" conclusion was an ARTIFACT of a log capped at 12 lines. Hard counters proved the
game submits **~27 gfx tasks/sec continuously** (dp_complete=331 in 12s). The game has been
drawing its full title screen the entire time: ~30 tris + a screenful of texrects per frame
(64x48 portrait grid + full-width banners), with healthy fade-animating prim colors, real
textures (nonzero TMEM source data), real palettes (proper G_LOADTLUT, 16- and 256-entry),
combiner G_CC_PRIMITIVE/decal modes, triple-buffered at 0x150000/0x188400/0x1C0800 in perfect
rotation, presented via osViSwapBuffer + frame-ready/done handshake. **All game-side "why isn't
it drawing" investigation is DONE — nothing is wrong with the game or the recomp.**

**VERIFIED GOOD (each by direct probe):** ucode matched by RT64's GBI database ("F3DEX 1.23
(Variant)", text=0x2AA70); interpreter generates 83-107 draw calls/workload with full-screen
drawRect; CPU draw data real (posFloats sum ~10k; raw rect vertices are unit quads; per-draw
screenScale/offset sane, e.g. 0.133x0.129 tiles); pipelines valid (0 null, ubershader→368 then
specialized); GPU viewport 1920x960 (4x scale), scissors full-screen, 0 draws skipped; fbPair
framebuffer storage correct + revision-validated (added a REAL stale-cache check: 0 hits);
workload command list ends with end()/execute()/wait(); present finds the same RenderTarget
pointer the workload rendered into; VI blit math perfect (480x240, videoRes==texRes); swap chain
presents to the visible window.

**VISUAL BISECTS (user-confirmed):** red swap-chain clear → VISIBLE as letterbox bars (present
path works; VI quad covers center). UV-gradient in the VI blit shader → VISIBLE (blit executes,
samples in range → the target content is genuinely black). Green clear of render targets → never
visible. Forced-magenta raster PS → no magenta. Forced-magenta WITH `discard` disabled → still no
magenta (**pixels never reach the pixel shader at all**). Combiner override → invalid test (word
order), but the game's own G_CC_PRIMITIVE + white prim draws were already proof constant-color
draws come out black. RT64 frame inspector (ImGui) renders fine over the game (more proof the
present/shader-library path works) but mouse doesn't reach it (frontend doesn't forward win32
messages; auto-open now gated on env `WCW_INSPECTOR=1`).

**RULED OUT:** launcher/UI overlay (geometry of red bars disproves), scratch-present RDRAM wipe
(0 hits), MSAA (forced 1x, no change; config uses 2x + sample locations), frame interpolation
duty-cycle (forced framesToPresent=1, no change), Vulkan vs D3D12 (**identical on both** —
defect is API-independent), stale framebuffer cache (real check added, 0 hits), empty-scissor
draw skip (0 skipped), degenerate GPU viewport (1920x960 logged), ubershadersVisible (it's only
a red debug tint, not a visibility gate), RDRAM byte-order (DLs/textures parse correctly).

**WHERE THE DEFECT MUST LIVE:** recorded `drawInstanced/drawIndexedInstanced` calls with
verified-good state produce ZERO rasterized pixels (no-discard magenta proof) into the correct
framebuffer, on both APIs, while clears and buffer-less fullscreen-triangle draws (VI blit,
ImGui) work. Remaining candidate space (in likelihood order):
1. **Vertex input path**: the game draws are the only ones using vertex buffers from
   BufferUploader's defaultBuffer + RT64's input slots (3 streams: pos/uv/color). If the GPU-side
   vertex buffer reads zeros (upload copy never landing) OR the input layout/stride mismatches
   our build's struct packing, every primitive is degenerate → no rasterization, no error.
   (CPU-side upload source verified real; the GPU-side content was never verified — the readback
   attempt crashed and needs the plume row-alignment incantations done right.)
2. The RSPProcessor/VertexProcessor GPU compute prepass that produces screenPosBuffer for
   indexed (3D) draws — but RECT draws bypass it (raw unit quads) and also don't show, so compute
   alone can't explain it.
3. A PSO state our build generates subtly wrong everywhere (e.g. depth/rasterizer/dual-source
   blend descriptor) — would have to be common to ubershader AND runtime-specialized PSOs AND
   both APIs.

**NEXT STEPS (decisive, in order):**
1. **RenderDoc one-frame capture** (or PIX): inspect one game draw — input assembler contents,
   VS output positions, pipeline state. This answers everything in minutes. (The RT64 inspector
   renders but needs input forwarding fixed: route win32 messages / SDL events to
   `RT64::Application::windowMessageFilter`/`sdlEventFilter` from the recompui window loop.)
2. **GPU readback of the vertex defaultBuffer** (fix the crashed plume readback: 256-byte row
   alignment, correct RenderTextureCopyLocation usage, do it OUTSIDE an open command list).
3. **Build & run BMHero on this machine** (source at C:\Users\selki\depot\BMHeroRecomp, no exe
   built) — same stack; if IT also renders black here, the problem is this build environment
   (clang-cl 19.1.5 flags / DXC version / driver), not WCW-specific code.
4. Diff our RT64/N64ModernRuntime build flags vs BMHero's documented working configuration.

**Diagnostics left in the tree (all in gitignored lib/, all tagged `[wcw]`):** counters/logs in
rt64 (gbi match, draw exec counts, viewport, msaa, workload fbPairs, rawtris, present/blit/
scratch paths, viswap, frame presentptr) + the REAL stale-framebuffer-cache rebuild fix in
rt64_render_target_manager.cpp + `WCW_INSPECTOR=1` auto-inspector + librecomp/ultramodern logs
from earlier sessions. The si.cpp port-1-always-connected fix and recompui launcher-hide fix
remain.

---

**(older notes below: the pre-2026-06-12 "most promising leads" — now superseded; the audio/
asset/state-counter theories are MOOT since the game was proven to be rendering all along)**

**Most promising next leads (in priority order):**
1. **Audio sync** — implement real audio (RSPRecomp the located ucode `0x80029C50`/data
   `0x80037530`/`0x1000`). Symptom (a few frames then a permanent wait with a silent no-op audio
   task) strongly fits intro/state progression gated on audio playback position (the game polls
   `osAiGetLength`). Cheaper pre-check: log librecomp's `osAiGetLength` return — if it's static,
   any "wait for audio to reach X" loop stalls.
2. **Asset/data wait** — check whether overlay B issues a rom/asset DMA early that returns
   unexpected data (the scene-setup reads it and bails). Watch the `[wcw][rom_read]` log after the
   overlay-B map.
3. **State counter** — trace what writes overlay-B globals `0x800E02DA`/`0x800E62FC` and the
   screen-state variable, and what condition the game is waiting on to advance it.
`tools/cycle.ps1`; `WCW_SAMPLE=1` dumps all thread stacks.

---

**(historical) CONTROLLER FRONTIER — controller reads (raw SI):** the game-logic thread crashed in
`func_80023C60` (a *synchronous* PIF transaction: `__osSiGetAccess` → `__osSiRawStartDma`
(func_80023970, writes `SI_PIF_ADDR_*`) → `osRecvMesg` → … → `__osSiRelAccess`), whose
`__osSiDeviceBusy` (func_800251E0) reads `SI_STATUS @ 0xA4800018` (raw MMIO → crash).
`func_80023C60` has **13 callers** (controller read/query + Pfs pak, e.g. func_80017B60,
func_800188C0, func_80018CC0, func_80019720, func_8002495C, …). librecomp provides only the
**high-level async** `osCont*` (`cont.cpp`: osContInit/StartReadData/GetReadData/StartQuery/
GetQuery/SetCh/Reset + osMotor*/osPfs*) and does **not** trap raw SI registers. WCW's
controller code is synchronous raw-SI, so each high-level caller of `func_80023C60` that maps
to a reimplemented `osCont*`/`osPfs*` must be identified and named (e.g. is func_80017B60
osContStartReadData?), OR librecomp needs SI/PIF emulation. This is the next batch.

**Also still needed for a frame** (once controllers are past): the VI display setters
(osViSetEvent/SwapBuffer/SetMode/Black — manager internals at `func_8001E280/E30C/E4F8`
reference VI-next `D_80033AB0`; the public setters need locating) and osSpTaskLoad/StartGo
(→ RT64). Use `tools/cycle.ps1`; set `WCW_SAMPLE=1` to dump all thread stacks ~4s in.

**Remaining shim targets to find (~17):** `osCreateMesgQueue`, `osJamMesg`,
`osSetEventMesg`, `osViSetEvent` (boot-critical: VI-retrace event for the main loop),
`osGetTime`, `osSetTimer`, `osStopTimer`, `osSetTime`, `osGetCount`, `osSetCount`,
`osDestroyThread`, `osStopThread`, `osYieldThread`, `osGetThreadId`, `osGetThreadPri`,
`osSetThreadPri`, `__osInitialize_common`, `__osSetFpcCsr`.
Located candidates (need precise assignment): event/VI-event in `func_80011990`/`func_80011B10`
(ref VI mgr `D_80033A90`); timer helpers in `func_80012500`/`570`/`5E0`/`650`/`7E0`.
Best finished by signature-matching against an IDO-SDK libultra, or during boot-debug.

**Method used (first pass):** anchor on (a) functions that touch N64 hardware MMIO
registers — spimdisasm labeled these (`PI_STATUS_REG`, `VI_CURRENT_REG`, …) because
`hardware_regs: True` is set; (b) functions using privileged COP0/TLB/`eret` instructions
(thread dispatch + exceptions); (c) the `cache` routines; (d) call-graph anchors from boot.
Precise per-function names still need **signature matching** against the matching libultra
(IDO SDK) version — that's the remaining work to make this list complete and exact.

The libultra layer lives roughly in `0x80011000`–`0x8002A000` (interleaved with some boot
glue like the overlay loader, so it is not a single clean range to blanket-ignore).

## Confirmed anchors
| func | libultra name | evidence |
|------|---------------|----------|
| `func_80011560` | `osCreateThread` | called from `game_main` with thread/entry/stack args |
| `func_800116B0` | `osStartThread` | called right after osCreateThread in boot |

## Device drivers — touch hardware MMIO (high confidence these are libultra)
| func | subsystem | likely libultra role |
|------|-----------|----------------------|
| `func_8001D6E0` | PI | osPiRawStartDma / osPiGetCmdQueue (PI_STATUS_REG, osRomBase) |
| `func_8001D780` `func_8001D960` `func_8001DA40` `func_8001DC70` | PI | osPiStartDma / __osDevMgrMain / osCreatePiManager / osEPiRawStartDma |
| `func_80025210` `func_800258B4` `func_80025BF0` `func_80025C40` | PI | PI/EPI DMA + PFS/pak access |
| `func_8001E680` `func_8001E7D0` | VI | __osViSwapContext / osViGetCurrentLine / __osViInit (VI_CURRENT/STATUS, osTvType) |
| `func_80013DA0` `func_80016B40` `func_80016BF0` `func_800211C0` | AI | osAiSetNextBuffer / osAiGetLength / osAiSetFrequency |
| `func_80023970` `func_800251E0` | SI | __osSiRawStartDma / osContStartReadData (SI_DRAM_ADDR_REG) |
| `func_8001EB40` `func_8001EB50` `func_8001EB60` `func_8001EBA0` `func_8001EC30` | SP | osSpTaskLoad / osSpTaskStartGo / __osSpSetPc / osSpGetStatus (SP_STATUS_REG) |
| `func_80012BD0` `func_8001EC60` | DP | osDpSetStatus / osDpGetStatus |
| `func_800126C0` `func_8001D39C` `func_8001D4F4` | MI | __osSetGlobalIntMask / interrupt handlers (MI_INTR_MASK_REG, `eret`) |
| `func_8001CD70` | init | touches ALL subsystems → `__osInitialize_common` / osInitialize |

## OS core — privileged COP0/TLB/eret (thread dispatch, exceptions, interrupts)
These were already stubbed to get a clean recompile (see `wcw.toml`); they are confirmed
libultra and should become `ignored`:
`func_80012160` `func_80012180` `func_8001CCA0` `func_8001CCB0`
`func_8001D680` (tlbwi) `func_8001EC90` (tlbp/tlbr) `func_80025D30`
`func_80029B88` `func_80029C50` `func_80029D80`
→ osSetIntMask / __osDisableInt / __osRestoreInt / __osDispatchThread / __osEnqueueAndYield
/ __osPopThread / TLB + exception vector setup.

## Cache routines (`cache` instruction; patched to nop for the recompile)
`func_80011D70` `func_80012040` `func_800128B0` `func_80013C80`
→ osInvalDCache / osInvalICache / osWritebackDCache / osWritebackDCacheAll.

## Pass 2 — scheduler / interrupt / message core (call-graph from the anchors)
High confidence, from call-graph tracing + COP0 register usage:

| func | libultra name | evidence |
|------|---------------|----------|
| `func_80012160` | `__osDisableInt` | reads/writes COP0 `$12` (Status); called by osCreateThread/osStartThread and 25 others |
| `func_80012180` | `__osRestoreInt` | writes COP0 `$12` (Status), pairs with above |
| `func_800126C0` | `osSetIntMask` / `__osSetGlobalIntMask` | COP0 `$12` manipulation |
| `func_8001D39C` | `__osEnqueueThread` | called by osStartThread + both message fns |
| `func_8001D49C` | `__osPopThread` / `__osDequeueThread` | adjacent thread-queue primitive |
| `func_8001D4E4` | `__osEnqueueAndYield` | the blocking primitive (called by msg fns) |
| `func_8001D4F4` | `__osDispatchThread` | has `eret` — restores thread context |
| `func_80011F00` | `osSendMesg` **or** `osRecvMesg` | 45 call sites; blocks via EnqueueAndYield, wakes via osStartThread |
| `func_80011C20` | `osRecvMesg` **or** `osSendMesg` | 20 call sites; symmetric to the above |

(To split the two message fns: the one that *stores* into the queue's msg array is
`osSendMesg`; the one that *reads* from it is `osRecvMesg`.)

### Critical-section functions (all 27 callers of `__osDisableInt`) — all libultra
These wrap interrupt-disable around queue/device state, so they're all OS functions to
`ignore` (messages, timers, event-mesg, thread ops, PI/VI/SI drivers):
`func_800009FC func_80011560 func_800116B0 func_80011800 func_80011990 func_80011B10`
`func_80011C20 func_80011F00 func_800121A0 func_80012500 func_80012570 func_800125E0`
`func_80012650 func_800127E0 func_80012820 func_80012860 func_80012FB0 func_80013D00`
`func_8001D780 func_8001E100 func_8001E484 func_8001E4F8 func_80023800 func_80025A90`
`func_80025B90 func_80025C90 func_80025CE0`
The `0x80011xxx`/`0x80012xxx` ones not yet named are the remaining message/timer/event
helpers (`osCreateMesgQueue`, `osSetEventMesg`, `osGetTime`, `osSetTimer`,
`osVirtualToPhysical`, `osGetThreadPri`, …) — name by reading each + signature matching.

## Running tally
~60 libultra functions located so far (28 device drivers + ~12 scheduler/msg/int core +
4 cache + the critical-section set). The OS layer sits in `0x80011000`–`0x8002A000`.
Remaining: precise names for the message/timer/event helpers, and a signature-matching
pass to guarantee completeness before flipping them to `ignored` in `wcw.toml`.

## Next steps
1. Signature-match the OS range against a known IDO-SDK libultra to assign exact names
   (and catch the non-hardware helpers).
2. Add the rename mapping to `disasm/symbol_addrs.txt`, regenerate `syms/dump.toml`.
3. Add the libultra names to `wcw.toml` `ignored = [...]` so ultramodern provides them.
4. Remove the corresponding entries from the current `stubs`/`cache nop` lists (they were
   placeholders to get a clean recompile; ultramodern replaces them properly).

## gu* math cluster — identified for the widescreen patch (2026-07-05)

Found by hunting the projection-matrix source for Phase 4 widescreen. Evidence in
`disasm/asm/1050.s`; the whole cluster is pure math (no MMIO) and recompiles perfectly.

| func | libultra name | evidence |
|------|---------------|----------|
| `func_8001BD90` | `guPerspectiveF` | guMtxIdentF call; fovy × (π/180 double `D_80034380`) / 2; cot = cosf/sinf; `m[0][0]=cot/aspect` (store @+0x00), `m[1][1]=cot` (+0x14), `m[2][3]=-1.0f` (+0x2C); perspNorm `65536*2/(near+far)` computation at the end |
| `func_8001BFC0` | `guPerspective` | thin wrapper: guPerspectiveF into a stack mf + guMtxF2L. **Never called by the game** |
| `func_80013210` | `guMtxF2L` | float→s15.16 GBI pack; `trunc.w.s` + 0x10000 scale |
| `func_80013310` | `guMtxIdentF` | called first by every gu matrix builder |
| `func_8001C020` | `guLookAtF` | called by the camera setup with up=(0,1,0) |
| `func_8001C970` | `sinf` | called in cos/sin pairs by all the rotation builders |
| `func_8001CB30` | `cosf` | idem |
| `func_8001C350` | `guRotateRPYF`-family | 3 × (sinf+cosf) pairs, π/180 float `D_80034390` |
| `func_8001C4F0` | `guPositionF`-family | same shape, constant `D_800343A0`, wrapper `func_8001C6A0` (+guMtxF2L) |

**⚠️ Do NOT add these to the `RENAME` map in `tools/gen_symbols.py`.** N64Recomp
auto-*ignores* any function bearing a known libultra name, and librecomp has **no**
`gu*`/`sinf`/`cosf` runtime shims — renaming them would drop the (perfectly working)
recompiled implementations and break the link. They stay `func_XXXX`; the names live
here and in `patches/widescreen.c` comments only.

### The game's one projection call site
`func_80006860` (main segment) is the camera/projection setup — **the only
guPerspectiveF caller in the entire game** (`jal` at `0x800068B0`):
```
guPerspectiveF(D_80047B50, &norm, fovy=33.0f, aspect=4/3 (0x3FAAAAAB inline), near=100, far=4000, scale=1)
guLookAtF     (D_80047B90, eye=a1..a3, at=stack args, up=(0,1,0))
func_800016F8 (gfx, D_80047B50, D_80047B90)   // latch P and V float matrices
func_80001724 (gfx, perspNorm)
```
Everything 3D goes through it (all composed MVPs inherit this P), which is why
`patches/widescreen.c` RECOMP_PATCHes exactly this function.
