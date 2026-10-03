#!/usr/bin/env python3
"""Check WS6 source bodies stay unchanged when COD2_X64 is off.

This deliberately excludes includes; actual i386 object parity still needs the
reference CI toolchain. The nested weapon .inc is included in this guard check.
"""
import argparse
import importlib.util
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location(
    'legacy_guards', ROOT / 'tools/macos-port/check_legacy_guards.py')
guards = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guards)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--base', default='a2f44778c8f76acedb6c74407f905a1681ffb732')
args = parser.parse_args()
paths = subprocess.check_output(
    ['git', 'diff', '--name-only', args.base, '--', 'src'], cwd=ROOT, text=True).splitlines()
paths = [path for path in paths if path.endswith(('.c', '.h', '.inc'))]
configurations = {name: definitions for name, definitions in guards.CONFIGURATIONS.items()
                  if name != 'linux-x86_64-port-on'}
failures = []
for path in paths:
    original = subprocess.check_output(
        ['git', 'show', f'{args.base}:{path}'], cwd=ROOT, text=True)
    current = (ROOT / path).read_text()
    for name, definitions in configurations.items():
        for patch in (0, 1):
            # CMake's OFF branch omits COD2_X64; upstream tests defined(), not its value.
            defines = definitions + [f'COD2_IS_PATCH_13={patch}']
            if guards.body(original, defines) != guards.body(current, defines):
                failures.append((path, name, patch))
print(f'{len(paths)} source files; {len(configurations) * 2} inactive configurations; '
      f'{len(failures)} mismatches')
for failure in failures:
    print(*failure)
raise SystemExit(bool(failures))
