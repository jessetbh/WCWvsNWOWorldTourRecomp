"""List the recompiled functions the post-recompile pipeline changed (fix_stumps,
fix_switches, fix_backbranches, fix_selfentry, readd_hand_edits, ...).

A ROM patch mod's changed functions are recompiled at load time with the live recompiler,
which produces N64Recomp's raw output: any repair the pipeline made to such a function is
lost. make_patch_mod.py warns when a hack changes one of the functions listed here.

    python tools/romhack/list_repaired_functions.py [--toml vpw2.toml]

Runs N64Recomp.exe on a copy of the config (romhack_raw_recomp.toml, removed afterwards) with
the output redirected to romhack-test/raw_recomp, compares every function body with RecompiledFuncs, and writes the
names of the ones that differ to RecompiledFuncs/repaired_functions.txt.
"""
import argparse, glob, os, re, shutil, subprocess, sys

FUNC_RE = re.compile(r'^RECOMP_FUNC void (\w+)\(uint8_t\* rdram, recomp_context\* ctx\) \{\n(.*?)^;\}', re.S | re.M)


def bodies(folder):
    out = {}
    for path in glob.glob(os.path.join(folder, 'funcs_*.c')):
        text = open(path, encoding='utf-8', errors='replace').read().replace('\r\n', '\n')
        for m in FUNC_RE.finditer(text):
            out[m.group(1)] = '\n'.join(l.rstrip() for l in m.group(2).split('\n'))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--toml', default=None, help='recompiler config (default: the *.toml at the repo root that writes RecompiledFuncs)')
    ap.add_argument('--recompiler', default='N64Recomp.exe')
    args = ap.parse_args()

    # The game's config is the one that writes RecompiledFuncs (World Tour also has patches.toml).
    toml = args.toml or next((t for t in sorted(glob.glob('*.toml'))
                              if re.search(r'^output_func_path\s*=\s*"RecompiledFuncs"', open(t, encoding='utf-8').read(), re.M)), None)
    if toml is None:
        sys.exit('no recompiler config writing RecompiledFuncs found; pass --toml')

    raw = os.path.join('romhack-test', 'raw_recomp')
    shutil.rmtree(raw, ignore_errors=True)
    os.makedirs(raw)
    cfg = open(toml, encoding='utf-8').read()
    cfg, n = re.subn(r'^output_func_path\s*=\s*".*"$', 'output_func_path = "%s"' % raw.replace('\\', '/'), cfg, flags=re.M)
    if n != 1:
        sys.exit('could not redirect output_func_path in %s' % toml)
    # Next to the original: the recompiler resolves the config's paths from its folder.
    tmp_toml = 'romhack_raw_recomp.toml'
    open(tmp_toml, 'w', encoding='utf-8').write(cfg)

    result = subprocess.run([os.path.abspath(args.recompiler), tmp_toml], capture_output=True, text=True)
    if result.returncode != 0:
        os.remove(tmp_toml)
        sys.exit('recompiler failed:\n' + result.stdout[-2000:] + result.stderr[-2000:])

    fixed, raw_bodies = bodies('RecompiledFuncs'), bodies(raw)
    repaired = sorted(name for name, body in fixed.items() if raw_bodies.get(name) != body)
    out = os.path.join('RecompiledFuncs', 'repaired_functions.txt')
    with open(out, 'w', encoding='utf-8') as f:
        f.write('# Functions the post-recompile pipeline changed (list_repaired_functions.py).\n')
        f.writelines(name + '\n' for name in repaired)
    print('%d functions compared, %d repaired by the pipeline -> %s' % (len(fixed), len(repaired), out))
    missing = [n for n in fixed if n not in raw_bodies]
    if missing:
        print('(%d of them exist only in RecompiledFuncs, e.g. %s)' % (len(missing), ', '.join(missing[:5])))
    shutil.rmtree(raw, ignore_errors=True)
    os.remove(tmp_toml)


if __name__ == '__main__':
    main()
