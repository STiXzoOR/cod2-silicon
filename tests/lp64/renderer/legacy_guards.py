#!/usr/bin/env python3
"""Check COD2_X64=OFF source bodies against the starting branch (no foreign SDK)."""
import argparse
import importlib.util
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--base', default='a2f4477')
args = parser.parse_args()
spec = importlib.util.spec_from_file_location('guards', 'tools/macos-port/check_legacy_guards.py')
guards = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guards)
paths = subprocess.check_output(['git', 'diff', '--name-only', args.base], text=True).splitlines()
paths += [p for p in subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard'], text=True).splitlines() if p.startswith('src/')]
checks = 0
failures = []
for path in sorted(set(paths)):
    if not path.startswith('src/') or not path.endswith(('.c', '.h')):
        continue
    original = subprocess.run(['git', 'show', args.base + ':' + path], capture_output=True, text=True)
    for name, defines in guards.CONFIGURATIONS.items():
        if 'port-on' in name:
            continue
        before = guards.body(original.stdout, defines) if original.returncode == 0 else ''
        after = guards.body(Path(path).read_text(), defines)
        checks += 1
        if before != after:
            failures.append((path, name))
print(f'{checks} inactive source-body comparisons; {len(failures)} mismatches')
for path, name in failures:
    print(path, name)
raise SystemExit(bool(failures))
