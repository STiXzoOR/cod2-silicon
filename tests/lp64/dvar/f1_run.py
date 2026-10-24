#!/usr/bin/env python3
"""Compile only the F1 production dvar fixture, outside the checkout."""
import argparse
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
exe = a.output.resolve() / 'f1_domain'
for codx in (False, True):
    subprocess.run(['clang', '-DCOD2_X64=1', *(['-DCOD2_CODX=1'] if codx else []),
        '-I' + str(root / 'src'), '-I' + str(root / 'src/headers'), '-I' + str(root),
        '-ffp-contract=off', '-g', '-O1', '-Wno-typedef-redefinition',
        '-Wno-duplicate-decl-specifier', '-Wno-incompatible-pointer-types',
        '-Wl,-dead_strip', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
        str(root / 'tests/lp64/dvar/f1_domain.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
