#!/usr/bin/env python3
"""Check inactive source tokens; real i386 binary parity requires the CI toolchain."""
import argparse
import importlib.util
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('guards', ROOT / 'tools/macos-port/check_legacy_guards.py')
guards = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guards)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--base', default='4802e924f22b15beacc5139a1ac8e239940f7272')
args = parser.parse_args()
paths = subprocess.check_output(['git', 'diff', '--diff-filter=M', '--name-only', args.base,
                                 '--', 'src'], cwd=ROOT, text=True).splitlines()
failures = []
for path in paths:
    if not path.endswith(('.c', '.h', '.inc')):
        continue
    old = subprocess.check_output(['git', 'show', f'{args.base}:{path}'], cwd=ROOT, text=True)
    new = (ROOT / path).read_text()
    for name, defines in guards.CONFIGURATIONS.items():
        if name == 'linux-x86_64-port-on':
            continue
        for patch in (0, 1):
            definitions = defines + [f'COD2_IS_PATCH_13={patch}', 'COD2_PORT_DEBUG=1',
                'COD2_DEBUG_ENV(name)=getenv(name)', 'COD2_DEBUG_ONLY(...)=__VA_ARGS__']
            if guards.body(old, definitions) != guards.body(new, definitions):
                failures.append((path, name, patch))
print(f'{len(paths)} changed source files; 10 legacy configurations; {len(failures)} mismatches')
for failure in failures:
    print(*failure)
raise SystemExit(bool(failures))
