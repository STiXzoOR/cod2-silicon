import json, shlex, subprocess, sys, re
from concurrent.futures import ThreadPoolExecutor
import argparse
p = argparse.ArgumentParser(description="Reject numeric conversions of type-punned float/integer bits")
p.add_argument('database')
p.add_argument('--jobs', type=int, default=3)
a = p.parse_args()
db = [e for e in json.load(open(a.database)) if '/dedicated_gen/' not in e['file'] and '-DDEDICATED' not in e.get('command', '') and '-DDEDICATED' not in e.get('arguments', [])]
FLTPTR = re.compile(r"CStyleCastExpr .*'(?:const )?(?:float|double|vec_t) \*'.*<BitCast>")
INTPTR = re.compile(r"CStyleCastExpr .*'(?:const )?(?:unsigned int|int|uint32_t|int32_t|DWORD|unsigned long|long|uint|UINT|LONG|dword|signed int|unsigned) \*'.*<BitCast>")
def run(e):
    f = e['file']
    if not f.endswith(('.c','.m','.cpp','.mm')): return []
    args = e.get('arguments') or shlex.split(e['command'])
    a, skip = [], False
    for x in args:
        if skip: skip = False; continue
        if x in ('-o', '-MF', '-MT', '-MQ'): skip = True; continue
        if x == '-c': continue
        a.append(x)
    a += ['-fsyntax-only', '-w', '-Xclang', '-ast-dump', '-fno-color-diagnostics']
    p = subprocess.Popen(a, cwd=e['directory'], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, errors='replace')
    out = []
    curfile, curline = None, None
    window = []  # (indent, kind, loc)
    lines = p.stdout
    pending = []  # list of [indent, loc, remaining]
    locre = re.compile(r"<(?:(/[^:>]+):(\d+):\d+|line:(\d+):\d+|col:\d+)")
    for line in lines:
        m = locre.search(line)
        if m:
            if m.group(1): curfile, curline = m.group(1), int(m.group(2))
            elif m.group(3): curline = int(m.group(3))
        ind = len(line) - len(line.lstrip(' |`-'))
        # resolve pending
        for pd in pending:
            if ind <= pd[0]: pd[2] = 0; continue
            if pd[2] > 0:
                pd[2] -= 1
                if pd[5]:
                    if re.search(r"UnaryOperator .*'(?:const )?" + (r"(?:float|double|vec_t)" if pd[4] == "I2F" else r"(?:unsigned int|int|uint32_t|int32_t|DWORD|unsigned long|long|uint|UINT|LONG|dword|signed int|unsigned)") + r" \*' prefix '&'", line):
                        out.append((pd[1], pd[3])); pd[2] = 0
                    pd[5] = False
                elif (INTPTR if pd[4]=='I2F' else FLTPTR).search(line):
                    pd[5] = True
        pending = [pd for pd in pending if pd[2] > 0]
        if '<IntegralToFloating>' in line:
            pending.append([ind, (curfile, curline), 9, line.strip()[:120], 'I2F', False])
        if '<FloatingToIntegral>' in line:
            pending.append([ind, (curfile, curline), 9, line.strip()[:120], 'F2I', False])
    if p.wait():
        raise RuntimeError('Clang AST failed for ' + f)
    return out
with ThreadPoolExecutor(a.jobs) as ex:
    res = list(ex.map(run, db))
seen = set()
for r in res:
    for (loc, txt) in r:
        if loc in seen: continue
        seen.add(loc)
        print(f'{loc[0]}:{loc[1]} | {txt}')

print(f'bit-conversion audit: {len(seen)} violations')
sys.exit(bool(seen))
