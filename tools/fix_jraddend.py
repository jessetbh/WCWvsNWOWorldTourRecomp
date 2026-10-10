#!/usr/bin/env python3
"""Post-process RecompiledFuncs: repair jump tables whose INDEX N64Recomp
constant-folded to zero (induction-variable-indexed tables).

N64Recomp models a `jr $v0` jump table as `switch (jr_addend_<jrvram> >> 2)`,
where `jr_addend` must hold the BYTE OFFSET into the table (index*4). It derives
that offset by pattern-matching an in-block `sll idx,2 ; addu base,idx` that forms
the table pointer. When the table pointer is instead a LOOP-CARRIED INDUCTION
POINTER (`addiu $sp_ptr, $sp_ptr, 0x4` on the loop back-edge, base set once above
the loop), there is no in-block shift to match, so N64Recomp emits the degenerate
`gpr jr_addend_<jrvram> = 0;` -- a CONSTANT. The dependent switch then ALWAYS takes
case 0, silently collapsing an N-way dispatch to one arm.

Origin: WWF No Mercy hit this in the championship storyline-condition evaluator (func_800EE1F0,
ovl_a): the mask-select table at 0x801036D0 (masks {0x7,0x3000,0x70,0xF00,0x70000})
always returned 0x7, so every storyline condition whose requirement bit lived in a
higher field failed to match -> a WON title match evaluated to the LOSE/default node
(storyline took the loss path, no prize money). fix_switches does NOT catch this: the
table is fully present (all cases emitted); only the index is wrong.

Signature is unambiguous: a constant `jr_addend_* = 0;` is the ONLY non-register
addend among all of N64Recomp's jump tables (every legitimate one is `= ctx->rN`).

Repair, per site:
  - table base = 3rd arg of the site's `default: switch_error(__func__, <jrpc>, <table>)`,
  - entry-address register = destination of the modeled table load immediately
    before the switch (`ctx->r<D> = ADD32(ctx->r<S>, <off>);`, i.e. &table[index]),
  - insert `jr_addend_<jrvram> = (uint32_t)ctx->r<D> - <table>U;` right before the
    switch so the live induction pointer drives the dispatch ((uint32_t)D - base ==
    index*4). The (uint32_t) cast is mandatory -- ctx->rD is the 64-bit sign-extended
    guest pointer, and a zero-extended base subtraction would overflow into
    switch_error.

Idempotent (the repaired declaration is tagged and skipped on re-run). Runs after
fix_switches in the build loop. FAILS LOUDLY on any site it cannot fully resolve --
a silent skip here would ship a collapsed switch (cf. the session-3 fix_stumps lesson).
"""
import re, os, glob, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TAG = '/* [wcw fix_jraddend] */'

decl_re = re.compile(r'^([ \t]*)gpr jr_addend_([0-9A-Fa-f]+) = 0;[ \t]*$')
sw_re = re.compile(r'^([ \t]*)switch \(jr_addend_([0-9A-Fa-f]+) >> \d+\) \{[ \t]*$')
err_re = re.compile(r'default: switch_error\(__func__, 0x[0-9A-Fa-f]+, (0x[0-9A-Fa-f]+)\);')
load_re = re.compile(r'^[ \t]*ctx->r(\d+) = ADD32\(ctx->r\d+, 0X[0-9A-Fa-f]+\);[ \t]*$')
fixed, failed = 0, 0

for path in sorted(glob.glob(os.path.join(ROOT, 'RecompiledFuncs', 'funcs_*.c'))):
    src = open(path, 'rb').read().decode('utf-8', errors='replace')
    nl = '\r\n' if '\r\n' in src else '\n'
    lines = src.splitlines()            # newline stripped; CRLF-safe
    # map each addend vram declared as constant 0 -> its declaration line index
    decls = {}
    for i, ln in enumerate(lines):
        dm = decl_re.match(ln)
        if dm and TAG not in ln:
            decls[dm.group(2)] = i
    # locate each dependent switch line and repair
    inserts = {}   # line index -> text to prepend before that line
    tag_decls = set()
    for i, ln in enumerate(lines):
        sm = sw_re.match(ln)
        if not sm:
            continue
        jrvram = sm.group(2)
        if jrvram not in decls:
            continue                     # register-derived (healthy) addend
        sw_indent = sm.group(1)
        # table base from the switch_error default arm below
        table = None
        for j in range(i + 1, min(i + 40, len(lines))):
            em = err_re.search(lines[j])
            if em:
                table = em.group(1); break
        if table is None:
            print('FAIL jr_addend_%s: no switch_error/table base after switch' % jrvram)
            failed += 1; continue
        # entry-address register = dest of the modeled table load just above the switch
        dreg = None
        for j in range(i - 1, max(i - 8, 0), -1):
            lm = load_re.match(lines[j])
            if lm:
                dreg = lm.group(1); break
        if dreg is None:
            print('FAIL jr_addend_%s: no modeled table-load line above switch' % jrvram)
            failed += 1; continue
        # (uint32_t) cast is REQUIRED: ctx->rD holds the guest pointer SIGN-EXTENDED
        # to 64 bits (0xFFFFFFFF8xxxxxxx). Subtracting a zero-extended 32-bit base
        # would leave the high bits set and blow the switch into switch_error. The
        # cast collapses to the true 32-bit offset (index*4) before the >> 2.
        inserts[i] = ('%sjr_addend_%s = (uint32_t)ctx->r%s - %sU; %s'
                      % (sw_indent, jrvram, dreg, table, TAG))
        tag_decls.add(decls[jrvram])
        fixed += 1
        print('repaired jr_addend_%s: switch now indexed by ctx->r%s - %s (was const 0)'
              % (jrvram, dreg, table))
    if inserts or tag_decls:
        out = []
        for i, ln in enumerate(lines):
            if i in tag_decls:
                ln = ln.rstrip() + ' ' + TAG      # keep the decl, mark it repaired
            if i in inserts:
                out.append(inserts[i])
            out.append(ln)
        open(path, 'wb').write((nl.join(out) + nl).encode('utf-8'))

print('%d jump-table indices repaired, %d failures' % (fixed, failed))
sys.exit(1 if failed else 0)
