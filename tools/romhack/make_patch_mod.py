"""Package a ROM hack as a recomp ROM-patch mod (.nrm).

The mod holds only a BPS patch against the stock ROM (no ROM data): players install it like
any other mod, and the runtime applies the patch to their own ROM when the game starts.

    python tools/romhack/make_patch_mod.py --base vpw2.z64 --hack hack.z64 \\
        --game-id vpw2 --id my_hack --name "My Hack" --version 1.0.0 --author me \\
        --out my_hack.nrm

Both ROMs must be big-endian .z64 (the byte order the runtime stores ROMs in).

The tool also reports which recompiled functions the hack changes, from the section and
function tables in RecompiledFuncs/recomp_overlays.inl. Data-only hacks (no changed
functions) work on any runtime that supports ROM-patch mods. Changed functions need a
runtime that recompiles them from the patched ROM at load time (the wcw-romhack runtime
does); a plain runtime silently keeps running the stock code for them.
"""
import argparse, io, json, re, sys, zipfile, zlib


# ---------------------------------------------------------------------------- BPS encoder

def bps_number(n):
    """BPS variable-length number."""
    out = bytearray()
    while True:
        x = n & 0x7F
        n >>= 7
        if n == 0:
            out.append(0x80 | x)
            break
        out.append(x)
        n -= 1
    return bytes(out)


def bps_encode(source, target, metadata=b''):
    """Linear BPS patch: SourceRead for runs that match the source in place, TargetRead for
    everything else. Small matching gaps are folded into TargetRead (an action costs a few
    bytes). Simple and exact; not the smallest possible patch for moved data."""
    SOURCE_READ, TARGET_READ = 0, 1
    MIN_MATCH = 8
    out = bytearray(b'BPS1')
    out += bps_number(len(source)) + bps_number(len(target)) + bps_number(len(metadata)) + metadata

    def action(kind, length):
        out.extend(bps_number(((length - 1) << 2) | kind))

    n = len(target)
    common = min(len(source), n)
    pos = 0
    while pos < n:
        # Length of the in-place match starting here.
        match = 0
        while pos + match < common and source[pos + match] == target[pos + match]:
            match += 1
        if match >= MIN_MATCH or (match > 0 and pos + match == n):
            action(SOURCE_READ, match)
            pos += match
            continue

        # Literal run: until the next in-place match of at least MIN_MATCH bytes.
        start = pos
        pos += max(match, 1)
        while pos < n:
            if pos < common and source[pos] == target[pos]:
                run = 0
                while pos + run < common and run < MIN_MATCH and source[pos + run] == target[pos + run]:
                    run += 1
                if run >= MIN_MATCH or pos + run == n:
                    break
                pos += run
            else:
                pos += 1
        action(TARGET_READ, pos - start)
        out += target[start:pos]

    out += zlib.crc32(source).to_bytes(4, 'little')
    out += zlib.crc32(target).to_bytes(4, 'little')
    out += zlib.crc32(bytes(out)).to_bytes(4, 'little')
    return bytes(out)


def bps_apply(source, patch):
    """Reference decoder (SourceRead/TargetRead/SourceCopy/TargetCopy), used to verify output."""
    def number(p):
        data, shift = 0, 1
        while True:
            x = patch[p]
            p += 1
            data += (x & 0x7F) * shift
            if x & 0x80:
                return data, p
            shift <<= 7
            data += shift

    assert patch[:4] == b'BPS1'
    p = 4
    src_size, p = number(p)
    tgt_size, p = number(p)
    meta_size, p = number(p)
    p += meta_size
    out = bytearray()
    src_rel = tgt_rel = 0
    end = len(patch) - 12
    while p < end:
        cmd, p = number(p)
        kind, length = cmd & 3, (cmd >> 2) + 1
        if kind == 0:
            out += source[len(out):len(out) + length]
        elif kind == 1:
            out += patch[p:p + length]
            p += length
        else:
            off, p = number(p)
            off = (-1 if off & 1 else 1) * (off >> 1)
            if kind == 2:
                src_rel += off
                out += source[src_rel:src_rel + length]
                src_rel += length
            else:
                tgt_rel += off
                for _ in range(length):
                    out.append(out[tgt_rel])
                    tgt_rel += 1
    assert len(out) == tgt_size and src_size == len(source)
    return bytes(out)


# ---------------------------------------------------------------- changed-function report

SECTION_RE = re.compile(r'\{ \.rom_addr = 0x([0-9A-Fa-f]+), \.ram_addr = 0x([0-9A-Fa-f]+), \.size = 0x([0-9A-Fa-f]+), \.funcs = (\w+),')
FUNC_RE = re.compile(r'\{ \.func = (\w+), \.offset = 0x([0-9A-Fa-f]+), \.rom_size = 0x([0-9A-Fa-f]+) \}')


def load_sections(overlays_inl):
    """[(name, rom, ram, size, [(func, offset, rom_size)])] from recomp_overlays.inl."""
    text = open(overlays_inl, encoding='utf-8', errors='replace').read()
    tables = {}
    for m in re.finditer(r'static FuncEntry (\w+)\[\] = \{(.*?)\n\};', text, re.S):
        tables[m.group(1)] = [(f.group(1), int(f.group(2), 16), int(f.group(3), 16)) for f in FUNC_RE.finditer(m.group(2))]
    sections = []
    for m in SECTION_RE.finditer(text):
        sections.append((m.group(4), int(m.group(1), 16), int(m.group(2), 16), int(m.group(3), 16), tables.get(m.group(4), [])))
    return sections


def changed_functions(sections, base, hack):
    """Functions whose ROM bytes differ, plus changed bytes in code sections outside any function."""
    funcs, loose = [], 0
    for name, rom, ram, size, entries in sections:
        covered = bytearray(size)
        for func, offset, rom_size in entries:
            if rom_size == 0:
                continue
            a = rom + offset
            covered[offset:offset + rom_size] = b'\x01' * rom_size
            if base[a:a + rom_size] != hack[a:a + rom_size]:
                words = sum(1 for i in range(0, rom_size, 4) if base[a + i:a + i + 4] != hack[a + i:a + i + 4])
                funcs.append((name, func, ram + offset, rom_size, words))
        for i in range(size):
            if not covered[i] and base[rom + i:rom + i + 1] != hack[rom + i:rom + i + 1]:
                loose += 1
    return funcs, loose


def escaping_branches(sections, hack, funcs):
    """Control flow in the hack's changed functions that leaves the function anywhere but a
    known function start: [(func, pc, target, kind)]. Recompiling a function at load time
    handles branches within it and calls/tail calls to known functions only; a hack that
    restructures code across function boundaries (or jumps into code it added) needs a full
    recompile of the hacked ROM."""
    starts = {ram + off for _, rom, ram, size, entries in sections for _, off, sz in entries}
    by_name = {(name, func): (rom, ram, off, sz) for name, rom, ram, size, entries in sections for func, off, sz in entries}
    out = []
    for section, func, vram, size, words in funcs:
        rom, ram, off, sz = by_name[(section, func)]
        lo, hi = ram + off, ram + off + sz
        for i in range(0, sz, 4):
            pc = lo + i
            w = int.from_bytes(hack[rom + off + i:rom + off + i + 4], 'big')
            op, rs, rt = w >> 26, (w >> 21) & 0x1F, (w >> 16) & 0x1F
            simm = (w & 0xFFFF) - 0x10000 if w & 0x8000 else (w & 0xFFFF)
            target, kind = None, None
            if op in (0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17) or                (op == 0x01 and rt in (0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13)) or                (op == 0x11 and rs == 0x08):
                target, kind = (pc + 4 + (simm << 2)) & 0xFFFFFFFF, 'branch'
            elif op == 0x02:
                target, kind = ((pc + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2), 'jump'
            elif op == 0x03:
                target, kind = ((pc + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2), 'call'
            if target is None:
                continue
            if kind == 'call':
                if target not in starts:
                    out.append((func, pc, target, kind))
            elif not (lo <= target < hi) and target not in starts:
                out.append((func, pc, target, kind))
    return out


# ------------------------------------------------------------------------------------ main

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--base', required=True, help='stock ROM (.z64)')
    ap.add_argument('--hack', required=True, help='hacked ROM (.z64)')
    ap.add_argument('--game-id', required=True, help="the port's mod game id (e.g. vpw2)")
    ap.add_argument('--id', required=True, help='mod id')
    ap.add_argument('--name', required=True, help='display name')
    ap.add_argument('--version', default='1.0.0')
    ap.add_argument('--author', action='append', default=[])
    ap.add_argument('--description', default='')
    ap.add_argument('--minimum-recomp-version', default='0.1.0')
    ap.add_argument('--overlays', default='RecompiledFuncs/recomp_overlays.inl', help='generated section tables for the report')
    ap.add_argument('--repaired', default='RecompiledFuncs/repaired_functions.txt',
                    help='functions the post-recompile pipeline changed (tools/romhack/list_repaired_functions.py)')
    ap.add_argument('--out', help='output .nrm (omit to only print the report)')
    ap.add_argument('--force', action='store_true', help='write the mod even when the report says it needs a full recompile')
    args = ap.parse_args()

    base = open(args.base, 'rb').read()
    hack = open(args.hack, 'rb').read()
    for label, rom in (('base', base), ('hack', hack)):
        if rom[:4] != b'\x80\x37\x12\x40':
            sys.exit('%s ROM is not big-endian .z64 (starts %s)' % (label, rom[:4].hex()))

    sections = load_sections(args.overlays)
    funcs, loose = changed_functions(sections, base, hack)
    code_bytes = sum(s[3] for s in sections)
    diff = sum(1 for a, b in zip(base, hack) if a != b) + abs(len(base) - len(hack))
    print('ROM: %d bytes -> %d bytes, %d bytes differ' % (len(base), len(hack), diff))
    print('code: %d sections, %d bytes; %d changed function(s), %d changed byte(s) outside functions'
          % (len(sections), code_bytes, len(funcs), loose))
    repaired = set()
    try:
        repaired = {l.strip() for l in open(args.repaired, encoding='utf-8') if l.strip() and not l.startswith('#')}
    except FileNotFoundError:
        print('(no %s: run tools/romhack/list_repaired_functions.py to check for pipeline repairs)' % args.repaired)
    for section, func, vram, size, words in funcs:
        print('  %-12s %-28s 0x%08X  %5d bytes, %d word(s) changed%s' % (section, func, vram, size, words,
              '  [REPAIRED by the pipeline: recompiling it at load drops the repair]' if func in repaired else ''))
    risky = [f for f in funcs if f[1] in repaired]
    if risky:
        print('WARNING: %d changed function(s) carry pipeline repairs or hand-edits; test them closely.' % len(risky))
    escapes = escaping_branches(sections, hack, funcs)
    for func, pc, target, kind in escapes[:20]:
        print('  ESCAPES: %s %s at 0x%08X -> 0x%08X (not in the function or a known function start)' % (func, kind, pc, target))
    if escapes:
        print('BLOCKED: %d branch(es)/call(s) leave their function: the hack restructures code across'
              ' function boundaries or calls code it added, so it needs a full recompile.' % len(escapes))
    if escapes or loose:
        print('-> needs a full recompile of the hacked ROM; a patch mod would not run it correctly.')
    elif funcs:
        print('-> changes code: needs a runtime that recompiles changed functions from the patch.')
    else:
        print('-> data-only: works with any runtime that supports ROM-patch mods.')

    if not args.out:
        return
    if (escapes or loose) and not args.force:
        sys.exit('not writing %s: the hack needs a full recompile (use --force to write it anyway)' % args.out)

    patch = bps_encode(base, hack)
    assert bps_apply(base, patch) == hack, 'BPS round trip failed'
    manifest = {
        'game_id': args.game_id,
        'id': args.id,
        'display_name': args.name,
        'description': args.description or ('ROM hack patch: %s.' % args.name),
        'short_description': 'ROM hack (patches your own ROM when the game starts).',
        'version': args.version,
        'authors': args.author or ['unknown'],
        'minimum_recomp_version': args.minimum_recomp_version,
    }
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('mod.json', json.dumps(manifest, indent=4))
        z.writestr('patch.bps', patch)
    open(args.out, 'wb').write(buf.getvalue())
    print('wrote %s (patch %d bytes, mod %d bytes)' % (args.out, len(patch), len(buf.getvalue())))


if __name__ == '__main__':
    main()
