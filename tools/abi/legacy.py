#!/usr/bin/env python3
"""Check every changed C/header body with COD2_X64 disabled.

Uses the port's existing include-free guard checker. This is a preprocessing
proof, not a claim that this Mac can link Linux/Windows i386 executables.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import importlib.util
import json
from pathlib import Path
import re
import shlex
import subprocess


def sdk_compare(entry, original, current):
    args = entry.get('arguments') or shlex.split(entry['command'])
    output, skip = [], False
    for arg in args:
        if skip:
            skip = False
            continue
        if arg in ('-o', '-MF', '-MT', '-MQ'):
            skip = True
        elif arg not in ('-c', '-MD', '-MMD', '-DCOD2_X64=1', entry['file']):
            output.append(arg)
    output += ['-E', '-P', '-x', 'c', '-I' + str(Path(entry['file']).parent), '-']
    results = [subprocess.run(output, input=source, cwd=entry['directory'],
                              capture_output=True, text=True) for source in (original, current)]
    if any(r.returncode for r in results):
        return 'preprocessing error: ' + next(r.stderr for r in results if r.returncode)
    # Ignore layout whitespace only; retain string literals and __LINE__ values.
    def tokens(text):
        return re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\s+',
                      lambda m: ' ' if m[0].isspace() else m[0], text).strip()
    return None if tokens(results[0].stdout) == tokens(results[1].stdout) else 'compiler input differs'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', required=True)
    parser.add_argument('--compile-commands', type=Path,
                        help='Also compare exact SDK preprocessing with COD2_X64 removed')
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    spec = importlib.util.spec_from_file_location('legacy_guards', root / 'tools/macos-port/check_legacy_guards.py')
    guards = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(guards)
    configurations = {k: v for k, v in guards.CONFIGURATIONS.items() if 'COD2_X64=1' not in v}
    paths = subprocess.check_output(['git', 'diff', '--diff-filter=M', '--name-only', options.base],
                                    cwd=root, text=True).splitlines()
    paths = [p for p in paths if p.endswith(('.c', '.h'))]
    failures = []
    sources = {}
    for path in paths:
        original = subprocess.check_output(['git', 'show', options.base + ':' + path], cwd=root, text=True)
        current = (root / path).read_text()
        sources[str(root / path)] = (original, current)
        for name, defines in configurations.items():
            if guards.body(original, defines) != guards.body(current, defines):
                failures.append((path, name))
    print(f'{len(paths)} files; {len(configurations)} COD2_X64-off configurations; {len(failures)} mismatches')
    for path, name in failures:
        print(path, name)
    if options.compile_commands:
        entries = [e for e in json.loads(options.compile_commands.read_text())
                   if e['file'] in sources and e['file'].endswith('.c')]
        with ThreadPoolExecutor(max_workers=4) as pool:
            results = list(pool.map(lambda e: sdk_compare(e, *sources[e['file']]), entries))
        sdk_failures = [(e['file'], result) for e, result in zip(entries, results) if result]
        print(f'{len(entries)} actual SDK commands with COD2_X64 removed; '
              f'{len(sdk_failures)} mismatches/errors')
        for path, reason in sdk_failures:
            print(path, reason)
        failures.extend(sdk_failures)
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
