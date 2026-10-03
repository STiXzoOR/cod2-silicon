#!/usr/bin/env python3
"""Run real CG_EntityEvent native FX cases with ASan/UBSan, without GL."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='ws15-fx-events-') as tmp:
    binary = str(Path(tmp) / 'fx_events')
    subprocess.run([
        'clang', '-arch', 'arm64', '-std=gnu99', '-g', '-O0', '-fno-common', '-ffp-contract=off',
        '-DCOD2_X64=1', '-fsanitize=address,undefined', '-Wl,-dead_strip',
        '-Wno-typedef-redefinition', '-Wno-duplicate-decl-specifier', '-Wno-ignored-attributes',
        '-Isrc', '-Isrc/headers', 'tests/platform/macos_fx_events.c',
        'src/PC/cgame_mp/cg_event_mp.c', 'src/PC/universal/com_math.c', '-o', binary
    ], cwd=root, check=True)
    subprocess.run([binary], check=True, env={
        **os.environ, 'ASAN_OPTIONS': 'symbolize=0', 'UBSAN_OPTIONS': 'halt_on_error=1'
    })
