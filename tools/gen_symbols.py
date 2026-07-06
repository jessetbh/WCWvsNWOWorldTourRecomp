#!/usr/bin/env python3
"""Generate an N64Recomp symbol TOML (dump.toml) from splat/spimdisasm .s output.

We use N64Recomp's symbol-TOML input mode (symbols_file_path) instead of ELF mode,
because this machine has no MIPS assembler to build an ELF. spimdisasm conveniently
emits a `nonmatching <name>, <size>` line before every function's `glabel`, and the
first instruction comment gives `/* ROM VRAM HEX */`, so we can recover name/vram/size
for every function directly from the disassembly.

Usage: python gen_symbols.py [--overlays] [--data]
  (default: fixed segment only — entry + main; --overlays also adds ovl_a/ovl_b;
   --data ALSO emits WCWSyms/data_dump.toml from the splat data/bss disassembly —
   needed by the patches build, see patches.toml `data_reference_syms_files`)
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ASM = ROOT / "disasm" / "asm"
OUT = ROOT / "WCWSyms" / "dump.toml"
DATA_OUT = ROOT / "WCWSyms" / "data_dump.toml"

# (section name, rom_start, vram_start, text_size, asm_file)
FIXED = [
    ("entry", 0x1000, 0x80000400, 0x50, "1000.s"),
    ("main", 0x1050, 0x80000450, 0x2A9B0 - 0x1050, "1050.s"),
]
OVERLAYS = [
    ("ovl_a", 0xA21750, 0x80090000, 0x33BC0, "A21750.s"),
    ("ovl_b", 0xA69570, 0x80090000, 0x55F70, "A69570.s"),
]

# Renames applied when emitting dump.toml.
#  * `main` collides with the host entry point (and `::main` must return int in C++).
#  * libultra functions that ultramodern PROVIDES are renamed to their real libultra names
#    and marked `ignored` in wcw.toml, so the runtime supplies host-integrated versions
#    instead of the game's recompiled ones. Only rename functions ultramodern actually
#    implements (threads/messages/timers/VI/controllers/osVirtualToPhysical/osInitialize) —
#    NOT the device drivers (PI/AI/SP/DP/cache/interrupts), which stay recompiled.
#    See disasm/libultra.md for the identification + evidence.
RENAME = {
    "main": "game_main",

    # --- libultra: naming a function makes N64Recomp auto-ignore it (built-in set in
    #     N64Recomp/src/symbol_lists.cpp) so the runtime provides it. Just name correctly.
    # Threads (call-graph confirmed from game_main):
    "func_80011560": "osCreateThread",   # game_main creates the game thread
    "func_800116B0": "osStartThread",    # game_main starts it
    # Interrupts (confirmed: read/write COP0 $12 Status):
    "func_80012160": "__osDisableInt",   # reads Status, masks IE, returns old
    "func_80012180": "__osRestoreInt",   # writes Status back
    # NOTE: __osDispatchThread (func_8001D4F4) is intentionally NOT named — it is an
    # internal scheduler function librecomp does NOT provide a shim for, and it's still
    # called by not-yet-named libultra (e.g. func_80025A90), so naming it causes an
    # undefined-symbol link error. It stays stubbed in wcw.toml instead.
    # Message queue (high confidence: C20 does the queue-full check early = send;
    # F00 loads a message + advances `first` = recv):
    "func_80011C20": "osSendMesg",
    "func_80011F00": "osRecvMesg",
    # Cache ops (assigned by `cache` operation code; runtime makes them no-ops on host):
    "func_80011D70": "osInvalDCache",        # cache 0x11 Hit_Invalidate_D / 0x15 Hit_WB_Inval_D
    "func_80012040": "osInvalICache",        # cache 0x10 Hit_Invalidate_I / 0x00 Index_Inval_I
    "func_800128B0": "osWritebackDCacheAll", # cache 0x01 Index_WB_Inval over whole D-cache
    "func_80013C80": "osWritebackDCache",    # cache 0x19 Hit_Writeback_D
    # Init + interrupt mask (touch-all-HW init; COP0 $12 global mask):
    # game_main's first call (jal func_800112D0 at 0x80000458). Installs exception vectors
    # (writes func_8001CD70 = __osException), inits caches/TLB, reads osResetType/osTvType/
    # osAppNMIBuffer, does PIF/SI reads. This is osInitialize — naming it lets ultramodern
    # provide the OS init and removes all that boot-time raw MMIO (which crashed: the recompiled
    # SI read of 0xA4800018 indexed out of RDRAM). (Earlier this was mis-assigned to the
    # handwritten exception handler func_8001CD70, which is now stubbed in wcw.toml.)
    "func_800112D0": "osInitialize",
    "func_800126C0": "osSetIntMask",         # COP0 $12 (Status) global interrupt mask
    # TLB virtual->physical (definitive: KSEG0/KSEG1 & 0x1FFFFFFF + TLB fallback):
    "func_80013C00": "osVirtualToPhysical",

    # --- thread accessors (leaves the game thread calls; reimplemented by ultramodern).
    #     These read the game's __osRunningThread (D_80033A90) when arg==NULL, which
    #     ultramodern never populates → NULL deref. Naming them hands the query to
    #     ultramodern's thread tracking.
    # func_8001D940: `if(!a0) a0=*D_80033A90; return a0->0x4` = thread->priority → osGetThreadPri.
    "func_8001D940": "osGetThreadPri",

    # --- message queue + device-manager creation (game thread func_800004AC calls these
    #     early; they touch the game's __osThreadTail/__osRunningThread which ultramodern
    #     doesn't populate → NULL derefs. Reimplemented by the runtime).
    # func_80011AE0: sets mq->{mtqueue,fullqueue}=&__osThreadTail(D_80033A80), validCount=first=0,
    #   msgCount=a2, msg=a1  → osCreateMesgQueue(mq,msg,count).
    "func_80011AE0": "osCreateMesgQueue",
    # func_80011800: builds __osPiDevMgr at D_80032C10 (stores PI dma/edma/raw handlers
    #   func_8001D960/DA40/DC70), creates the cmd queue + device-manager thread; sig
    #   (pri,cmdQ,cmdBuf,cmdMsgCnt) → osCreatePiManager. Removes the scheduler-internal
    #   (func_80011B10) __osRunningThread NULL deref.
    "func_80011800": "osCreatePiManager",
    # func_80011990: sets cart base 0xB0000000 (D_8005DBBC), reads PI domain (latency/pulse/
    #   pageSize/relDuration) via the raw PI func (func_8001D6E0 = PI_STATUS @ 0xA4600010),
    #   returns &CartRomHandle → osCartRomInit. Removes the raw PI MMIO read.
    "func_80011990": "osCartRomInit",
    # func_80011B10: __osDisableInt; if(!t) t=__osRunningThread(D_80033A90); if(t->pri(0x4)!=pri)
    #   { t->pri=pri; reschedule via __osDequeue/EnqueueThread (func_8001D740/D49C/D39C),
    #   compare against __osRunQueue(D_80033A88) }; __osRestoreInt → osSetThreadPri(t,pri).
    #   (Crashed: __osRunningThread NULL → t->pri read at rdram+0x80000004.)
    "func_80011B10": "osSetThreadPri",
    # func_800125E0: __osDisableInt; __osEventStateTab[e].{mq=a1,msg=a2} (D_8005EE50 + e*8);
    #   __osRestoreInt → osSetEventMesg(event,mq,msg). (osCreatePiManager passes event 8 = PI.)
    "func_800125E0": "osSetEventMesg",
    # func_800121A0: builds __osViDevMgr at D_80032C50, creates retrace mesg queue, registers
    #   VI events (osSetEventMesg), __osViInit (func_8001E680, VI MMIO), creates+starts the VI
    #   manager thread (entry func_80012328 = __osViMgrMain) → osCreateViManager. Removes the
    #   raw VI_CURRENT (0xA4400010) access.
    "func_800121A0": "osCreateViManager",
    # func_80011E20: checks __osPiDevMgr(D_80032C10) active, fills the OSIoMesg (piHandle,
    #   type by direction), gets the PI cmd queue (osPiGetCmdQueue) and sends the request to
    #   it; sig (pihandle, mb, direction) → osEPiStartDma. CRITICAL: the overlay loader spins
    #   retrying this (func_80000650) because the game's PI device-manager thread no longer
    #   drains the cmd queue (osCreatePiManager is now ultramodern's). Naming routes it to
    #   librecomp's osEPiStartDma_recomp, which performs the DMA directly.
    "func_80011E20": "osEPiStartDma",
    # func_80013DA0: reads osViClockRate (D_80032BF8), computes clockRate/frequency as the AI
    #   DAC rate, writes AI_DACRATE/AI_BITRATE → osAiSetFrequency. (Crashed writing AI reg
    #   0xA4500010 raw.) The overlay/game thread now runs game code and sets up audio.
    "func_80013DA0": "osAiSetFrequency",
    # func_800172E0: one-time-init flag (D_80033A50 = __osContInitialized), osGetTime
    #   (func_80023800) + PIF post-boot timing delay, osCreateMesgQueue, then __osSiRawStartDma
    #   (func_80023970) x2 with osRecvMesg between (PIF write-request / read-response controller
    #   detect) → osContInit. Removes the raw-SI subtree (func_80023970 __osSiRawStartDma,
    #   func_800251E0 __osSiDeviceBusy) that crashed on SI_STATUS @ 0xA4800018.
    "func_800172E0": "osContInit",
    # --- raw SI primitives. WCW's controller layer is built on these (not osCont*), and they
    #     do raw SI MMIO (SI_PIF_ADDR_* / SI_STATUS @ 0xA48000xx) which recomp.h's MEM_* won't
    #     trap → crash. They're in N64Recomp's ignored set, so naming drops the recompiled
    #     bodies; librecomp/src/si.cpp provides *_recomp shims (joybus/PIF emulation using host
    #     input). func_80023970 reads SI_STATUS busy then sets SI_DRAM_ADDR + SI_PIF_ADDR_RD/
    #     WR64B (dir=a0, dramAddr=a1) = __osSiRawStartDma; func_800251E0 reads SI_STATUS & 3
    #     = __osSiDeviceBusy.
    "func_80023970": "__osSiRawStartDma",
    "func_800251E0": "__osSiDeviceBusy",

    # --- RSP/RDP task submission (the graphics thread func_80003450 -> func_800011CC submits
    #     the gfx display list). These do raw SP/DP MMIO (SP_MEM/DRAM/STATUS/PC, DPC_STATUS)
    #     and are reimplemented by librecomp (sp.cpp/dp.cpp), which routes the graphics task to
    #     RT64. Naming them cuts the raw SP/DP primitives (func_8001EBA0/EB40/EB50/EB60/EC30,
    #     func_8001EC60) out of the call graph.
    # func_800129FC: DMAs the task ucode/data into SP (func_8001EBA0 x3) + __osSpSetPc → osSpTaskLoad.
    "func_800129FC": "osSpTaskLoad",
    # func_80012B8C: SP_STATUS check (func_8001EC30) + start (func_8001EB50) → osSpTaskStartGo.
    "func_80012B8C": "osSpTaskStartGo",
    # func_80012BD0: sets the RDP command buffer + DPC_STATUS_REG → osDpSetNextBuffer.
    "func_80012BD0": "osDpSetNextBuffer",
    # func_80012C80: single call `func_8001EB50(0x400)` = __osSpSetStatus(SP_SET_SIG0), the RSP
    #   yield request → osSpTaskYield. Only hit at the overlay-B ucode switch (F3DLX load): the
    #   scheduler yields the running gfx task first. Unnamed it crashed writing SP_STATUS raw
    #   (post-attract AV in func_80000CBC → func_80012C80 → func_8001EB50). sp.cpp provides the
    #   no-op osSpTaskYield_recomp.
    "func_80012C80": "osSpTaskYield",
    # func_80012760: reads SP_STATUS via func_8001EB40 (__osSpGetStatus), returns (status &
    #   0x100 /*YIELDED*/) and ORs the yield flag into the task header when 0x80 /*YIELD*/ is
    #   set → osSpTaskYielded(task). Crashed next after osSpTaskYield was named (READ of raw
    #   SP_STATUS in the same overlay-B ucode-switch path). sp.cpp: osSpTaskYielded_recomp.
    "func_80012760": "osSpTaskYielded",

    # --- VI display setters/getters. These write the game's __OSViContext globals
    #     (__osViNext @ D_80033B24, __osViCurr @ D_80033B20; struct: 0x0 state(u16),
    #     0x2 retraceCount(u16), 0x4 framep, 0x8 modep, 0x10 msgq, 0x14 msg — confirmed
    #     against __osViSwapContext func_8001E7D0 which osVirtualToPhysical's 0x4=framep and
    #     indexes 0x8=modep as an OSViMode table). Since osCreateViManager is now ultramodern's,
    #     the game's recompiled setters update game globals the runtime's VI thread never reads
    #     → no retrace heartbeat (vi.mq stayed 0) and update_vi skipped (mode NULL), stalling the
    #     whole frame pipeline (all gfx/scheduler threads blocked in osRecvMesg). Naming them
    #     routes to ultramodern's osVi*_recomp so next_state->{mq,msg,mode,framep} get set and the
    #     VI thread posts retrace messages each frame. (libultra.md "VI display setters" earlier
    #     pointed at func_8001E280/E30C/E484/E4F8 — those are actually TIMER funcs on the
    #     __osTimerList @ D_80033AB0; the real VI setters are here in 0x80012xxx.)
    # func_80012500: __osDisableInt; __osViNext->modep(0x8)=a0; state(0x0)=1 → osViSetMode(mode).
    "func_80012500": "osViSetMode",
    # func_80012570: __osDisableInt; if(arg) state|=0x20 else state&=~0x20 → osViBlack(active).
    "func_80012570": "osViBlack",
    # func_80012650: __osDisableInt; __osViNext->msgq(0x10)=a0, msg(0x14)=a1, retraceCount(0x2)=a2
    #   → osViSetEvent(mq,msg,count). THE missing retrace-queue registration (vi.mq=0 root cause).
    "func_80012650": "osViSetEvent",
    # func_800127E0: returns __osViCurr->framep(0x4) → osViGetCurrentFramebuffer.
    "func_800127E0": "osViGetCurrentFramebuffer",
    # func_80012820: returns __osViNext->framep(0x4) → osViGetNextFramebuffer.
    "func_80012820": "osViGetNextFramebuffer",
    # func_80012860: __osDisableInt; __osViNext->framep(0x4)=a0; state|=0x10 → osViSwapBuffer(buf).
    "func_80012860": "osViSwapBuffer",
    # func_80012FB0: __osDisableInt; bit-tests a0 (OS_VI_* feature flags), sets __osViNext state/
    #   feature bits → osViSetSpecialFeatures(features).
    "func_80012FB0": "osViSetSpecialFeatures",

    # --- AI (audio) registers. The audio manager thread (func_8000455C -> func_80004750)
    #     polls/sets the AI DMA via these, which read/write raw AI MMIO @ 0xA450xxxx — recomp.h's
    #     MEM_* doesn't trap it → crash (READ AI_LEN @ 0xA4500004 out of RDRAM). librecomp's
    #     ai.cpp provides the host implementations. osAiSetFrequency (func_80013DA0) already named.
    # func_80016BF0: `lw v0, 0x4(0xA4500000)` (AI_LEN_REG) leaf → osAiGetLength.
    "func_80016BF0": "osAiGetLength",
    # func_80016B40: double-buffer bookkeeping (0x2000/0x3FFF AI DMA alignment) + __osAiDeviceBusy
    #   check (func_800211C0, reads AI_STATUS @ 0xA450000C — now dead, only this calls it) +
    #   osVirtualToPhysical + writes AI_DRAM_ADDR/AI_LEN → osAiSetNextBuffer(buf,size).
    "func_80016B40": "osAiSetNextBuffer",

    # --- clock: the game's timed waits (logo/intro delays) were frozen because osGetCount was
    #     STUBBED. func_8001EB30 = `mfc0 $v0,$9` (the COP0 count register) = osGetCount; stubbing
    #     it returned garbage, so osGetTime never advanced and any "wait until time > T" stalled.
    #     func_80023800 = osGetTime (calls osGetCount, accumulates into __osCurrentTime). Name BOTH
    #     to ultramodern so the clock is self-consistent (ultramodern's osGetTime uses its own
    #     osGetCount, not the game's uninitialized __osBaseCounter globals). Removed func_8001EB30
    #     from wcw.toml stubs.
    "func_8001EB30": "osGetCount",
    "func_80023800": "osGetTime",

    # --- IDO softfloat FP<->int64 conversions. N64Recomp can't emit trunc.l.*/cvt.*.l, so these
    #     were stubbed (returning garbage for live conversions). They're in N64Recomp's ido-math
    #     ignored set, so NAMING them makes the recompiler skip them and link librecomp's
    #     `<name>_recomp` (math_routines.cpp). Mapped by their FP op (trunc=signed truncate;
    #     cvt.l with a sign-check branch = unsigned; cvt.d.l/cvt.s.l = s64->double/float;
    #     dmtc1+sign-check+cvt = u64->double/float). 4 of the 8 shims were missing from
    #     math_routines.cpp and were added there (__d_to_ll/__d_to_ull/__f_to_ull/__ll_to_d).
    "func_80013740": "__d_to_ll",    # trunc.l.d  : double -> s64 (truncate)
    "func_8001375C": "__f_to_ll",    # trunc.l.s  : float  -> s64
    "func_80013778": "__d_to_ull",   # cvt.l.d x2 + sign-check : double -> u64
    "func_80013818": "__f_to_ull",   # cvt.l.s x2 + sign-check : float  -> u64
    "func_800138B4": "__ll_to_d",    # cvt.d.l    : s64 -> double
    "func_800138CC": "__ll_to_f",    # cvt.s.l    : s64 -> float
    "func_800138E4": "__ull_to_d",   # dmtc1 + sign-check + cvt.d.l : u64 -> double
    "func_80013918": "__ull_to_f",   # dmtc1 + sign-check + cvt.s.l : u64 -> float
}

# Data/bss sections for --data (syms/data_dump.toml). Derived from disasm/wcw.yaml:
# vram = first dlabel in the file; size = subsegment extent (data: next rom - rom;
# bss: the yaml bss_size). `rom` is intentionally OMITTED when emitting: N64Recomp then
# treats the symbols as absolute (SectionAbsolute), which is correct here — nothing in
# this game is relocatable (both overlays load at a fixed vram).
# (section name, vram, size, asm file under disasm/asm/asm/data/)
DATA_SECTIONS = [
    ("main_data",  0x80029DB0, 0x393F0 - 0x2A9B0,   "2A9B0.data.s"),
    ("main_bss",   0x800387F0, 0x298A0,             "393F0.bss.s"),
    ("ovl_a_data", 0x800C3BC0, 0xA69570 - 0xA55310, "A55310.data.s"),
    ("ovl_a_bss",  0x800D7E20, 0x16000,             "A69570.bss.s"),
    ("ovl_b_data", 0x800E5F70, 0xAC2970 - 0xABF4E0, "ABF4E0.data.s"),
    ("ovl_b_bss",  0x800E9400, 0xF6C0,              "AC2970.bss.s"),
]

NONMATCH = re.compile(r"^nonmatching\s+(\S+),\s*(0x[0-9A-Fa-f]+)")
GLABEL = re.compile(r"^glabel\s+(\S+)")
COMMENT = re.compile(r"/\*\s*[0-9A-Fa-f]+\s+([0-9A-Fa-f]{8})\s")


def parse_functions(path: Path):
    """Return list of (name, vram, size) in file order."""
    funcs = []
    pending_size = None
    cur_name = None
    cur_size = None
    awaiting_vram = False
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = NONMATCH.match(line)
        if m:
            pending_size = int(m.group(2), 16)
            continue
        g = GLABEL.match(line)
        if g:
            cur_name = g.group(1)
            cur_size = pending_size
            pending_size = None
            awaiting_vram = True
            continue
        if awaiting_vram:
            c = COMMENT.search(line)
            if c:
                funcs.append([cur_name, int(c.group(1), 16), cur_size])
                awaiting_vram = False
    # Fill any missing sizes from the next function's vram delta.
    for i, fn in enumerate(funcs):
        if fn[2] is None:
            if i + 1 < len(funcs):
                fn[2] = funcs[i + 1][1] - fn[1]
            else:
                fn[2] = 4
    return funcs


DLABEL = re.compile(r"^dlabel\s+(\S+)")
HEXTOK = re.compile(r"/\*\s*((?:[0-9A-Fa-f]+\s+)*[0-9A-Fa-f]+)\s*\*/")


def parse_data_symbols(path: Path):
    """Return list of (name, vram) in file order.

    spimdisasm data lines carry `/* ROM VRAM [VALUE] */` comments (VALUE only for
    .word); bss lines carry `/* VRAM */`. The vram of a dlabel = the vram of the first
    commented line after it (2nd hex token if the comment has >= 2 tokens, else the 1st).
    """
    syms = []
    cur_name = None
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        d = DLABEL.match(line)
        if d:
            cur_name = d.group(1)
            continue
        if cur_name:
            c = HEXTOK.search(line)
            if c:
                toks = c.group(1).split()
                vram = int(toks[1] if len(toks) >= 2 else toks[0], 16)
                syms.append((cur_name, vram))
                cur_name = None
    return syms


def gen_data():
    seen = {}
    lines = ["# Autogenerated from splat disassembly by tools/gen_symbols.py --data",
             "# Data reference symbols for the patches build (patches.toml).",
             "# Sections omit `rom` on purpose: N64Recomp treats them as absolute,",
             "# which is correct — this game has no relocatable sections.", ""]
    total = 0
    for name, vram, size, fname in DATA_SECTIONS:
        syms = parse_data_symbols(ASM / "asm" / "data" / fname)
        end = vram + size
        for sym_name, sym_vram in syms:
            if not (vram <= sym_vram < end):
                sys.exit(f"ERROR: {sym_name} @ 0x{sym_vram:08X} outside {name} "
                         f"[0x{vram:08X}, 0x{end:08X})")
        lines += ["[[section]]", f'name = "{name}"',
                  f"vram = 0x{vram:08X}", f"size = 0x{size:X}", "", "symbols = ["]
        for sym_name, sym_vram in syms:
            # Disambiguate names colliding across sections (ovl_a bss overlaps ovl_b
            # data in vram, so a few D_* names exist in both; first-seen keeps the
            # plain name, mirroring the function-section behavior above).
            out_name = sym_name
            if out_name in seen and seen[out_name] != (name, sym_vram):
                out_name = f"{sym_name}_{name}"
            seen[out_name] = (name, sym_vram)
            lines.append(f'    {{ name = "{out_name}", vram = 0x{sym_vram:08X} }},')
        lines += ["]", ""]
        total += len(syms)
        print(f"  {name:10} {len(syms):4} data symbols  (vram 0x{vram:08X}, size 0x{size:X})")
    DATA_OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {total} data symbols across {len(DATA_SECTIONS)} sections -> {DATA_OUT}")


def main():
    sections = FIXED + (OVERLAYS if "--overlays" in sys.argv else [])
    seen = {}
    lines = ["# Autogenerated from splat disassembly by tools/gen_symbols.py", ""]
    total = 0
    for name, rom, vram, size, fname in sections:
        funcs = parse_functions(ASM / fname)
        lines += [f"[[section]]", f'name = "{name}"',
                  f"rom = 0x{rom:08X}", f"vram = 0x{vram:08X}",
                  f"size = 0x{size:X}", "", "functions = ["]
        for fn_name, fn_vram, fn_size in funcs:
            fn_name = RENAME.get(fn_name, fn_name)
            # Disambiguate names that collide across overlay sections.
            out_name = fn_name
            if out_name in seen and seen[out_name] != (name, fn_vram):
                out_name = f"{fn_name}_{name}"
            seen[out_name] = (name, fn_vram)
            lines.append(
                f'    {{ name = "{out_name}", vram = 0x{fn_vram:08X}, size = 0x{fn_size:X} }},'
            )
        lines += ["]", ""]
        total += len(funcs)
        print(f"  {name:6} {len(funcs):4} functions  (rom 0x{rom:06X}, vram 0x{vram:08X})")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {total} functions across {len(sections)} sections -> {OUT}")
    if "--data" in sys.argv:
        gen_data()


if __name__ == "__main__":
    main()
