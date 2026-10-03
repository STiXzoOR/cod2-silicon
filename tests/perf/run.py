#!/usr/bin/env python3
"""Exercise native item lookup from production source at Release optimization."""
import argparse
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline')
args = parser.parse_args()
path = 'src/PC/bgame/bg_misc.c'
source = subprocess.check_output(['git', 'show', f'{args.baseline}:{path}'], cwd=root, text=True) if args.baseline else (root / path).read_text()
start = source.index('const gitem_t *G_FindItem(const char *pickupName)\n{')
end = source.index('\nvoid BG_AddPredictableEvent', start)
out = root / 'output/ws13/items-test'
out.mkdir(parents=True, exist_ok=True)
(out / 'find_item.h').write_text(source[start:end])
exe = out / 'items'
subprocess.run(['clang', '-DCOD2_X64=1', '-O3', '-ffp-contract=off',
                '-fsanitize=address,undefined', '-Wno-typedef-redefinition',
                '-I' + str(root / 'src'), '-I' + str(root / 'src/headers'),
                '-I' + str(out), str(root / 'tests/perf/items.c'), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
print('PASS native item lookup (O3, ASan/UBSan)')
