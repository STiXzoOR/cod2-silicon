#!/usr/bin/env python3
"""Check real CG impact-mark storage and axes with ASan/UBSan, without GL."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='ws15-impact-marks-') as tmp:
    binary = str(Path(tmp) / 'impact_marks')
    subprocess.run([
        'clang', '-arch', 'arm64', '-std=gnu99', '-g', '-O0', '-fno-common', '-ffp-contract=off',
        '-DCOD2_X64=1', '-fsanitize=address,undefined', '-Wl,-dead_strip',
        '-Wno-typedef-redefinition', '-Wno-duplicate-decl-specifier', '-Wno-ignored-attributes',
        '-Isrc', '-Isrc/headers', 'tests/platform/macos_impact_marks.c',
        'src/PC/cgame_mp/cg_marks_mp.c', 'src/PC/universal/com_math.c', '-o', binary
    ], cwd=root, check=True)
    env = {**os.environ, 'ASAN_OPTIONS': 'symbolize=0', 'UBSAN_OPTIONS': 'halt_on_error=1'}
    for arguments in [[], ['axes']]:
        subprocess.run([binary, *arguments], check=True, env=env)
