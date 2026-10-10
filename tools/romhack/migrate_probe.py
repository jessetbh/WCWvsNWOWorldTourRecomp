"""How many base functions can be located in a relinked hack (one whose functions moved)
by their instruction shape, with address fields masked? A probe for automating symbol
migration, the first step of recompiling such a hack without redoing the symbol work.

    python tools/romhack/migrate_probe.py <base.z64> <recomp_overlays.inl> <hack.z64>...

Prints how many base functions (>= 12 words) were located, how many uniquely, and the most
common ROM shifts per section (a relinked hack shows a few constant shifts).
"""
import collections, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_patch_mod import load_sections


def masked(word):
    op = word >> 26
    if op in (2, 3):              # j / jal: target
        return word & 0xFC000000
    if op in (0x0F, 0x09, 0x08, 0x0D, 0x0C, 0x0A, 0x0B, 0x0E,          # lui/addiu/addi/ori/andi/slti/sltiu/xori
              0x20, 0x21, 0x23, 0x24, 0x25, 0x27, 0x28, 0x29, 0x2B, 0x31, 0x35, 0x39, 0x3D, 0x37, 0x3F):  # loads/stores
        return word & 0xFFFF0000  # immediates (addresses, offsets)
    return word


def words(data, start, count):
    return tuple(masked(int.from_bytes(data[start + 4 * i:start + 4 * i + 4], 'big')) for i in range(count))


base = open(sys.argv[1], 'rb').read()
secs = load_sections(sys.argv[2])
K = 12  # words in the signature
for hack_path in sys.argv[3:]:
    hack = open(hack_path, 'rb').read()
    # Index every aligned K-word window of the hack's first 8 MB by its masked signature.
    index = collections.defaultdict(list)
    limit = min(len(hack), 8 << 20) - 4 * K
    for off in range(0, limit, 4):
        index[words(hack, off, K)].append(off)
    found = unique = 0
    total = 0
    shifts = collections.Counter()
    for name, rom, ram, size, funcs in secs:
        for fn, off, sz in funcs:
            if sz < 4 * K:
                continue
            total += 1
            hits = index.get(words(base, rom + off, K), [])
            if hits:
                found += 1
                if len(hits) == 1:
                    unique += 1
                    shifts[(name, hits[0] - (rom + off))] += 1
    print('%s: %d functions >= %d words; located %d (%d unique)' % (os.path.basename(hack_path)[:40], total, K, found, unique))
    for (sec, d), n in shifts.most_common(10):
        print('    %-26s shift %+7d: %d' % (sec, d, n))
