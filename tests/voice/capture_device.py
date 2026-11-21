#!/usr/bin/env python3
"""Run the production microphone capture against a fake Core Audio.

Compiles src/platform/macos_record.c with the engine's own flags and links it
with capture_device.c, which defines the AudioQueue and audio-object calls,
the permission check, the talk gate and the playback voices. No audio device
opens and macOS is never asked for microphone permission. Runs twice: with
the address and undefined-behaviour sanitizers, then with the thread
sanitizer.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
BACKGROUND = ['/usr/sbin/taskpolicy', '-b', 'nice', '-n', '19']
SOURCE = Path('src/platform/macos_record.c')
SANITIZERS = {
    'address': ['-fsanitize=address,undefined', '-fno-sanitize-recover=all'],
    'thread': ['-fsanitize=thread'],
}
CASES = ['permission_refused', 'slow_open', 'linger', 'open_failure', 'device_lost',
         'default_input_changed', 'mute', 'passed']


def load_flags(database):
    spec = importlib.util.spec_from_file_location('capture_math_flags', ROOT / 'tests/fidelity/math_reference.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.ROOT = ROOT
    module.SOURCE = SOURCE
    return module.engine_flags(database)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-db', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sanitizer', choices=sorted(SANITIZERS), action='append',
                        help='Run only these sanitizer builds (default: all)')
    parser.add_argument('--baseline', help='Read the capture source from this local commit')
    args = parser.parse_args()
    out = args.output.resolve()
    if out.is_relative_to(ROOT):
        parser.error('Keep evidence outside the repository')
    out.mkdir(parents=True, exist_ok=False)
    flags, evidence = load_flags(args.compile_db)
    (out / 'flags.json').write_text(json.dumps(evidence, indent=2) + '\n')
    env = dict(os.environ, TMPDIR=str(out), CLANG_MODULE_CACHE_PATH=str(out / 'clang-cache'),
               ASAN_OPTIONS='detect_leaks=0:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1',
               TSAN_OPTIONS='halt_on_error=1:second_deadlock_stack=1')
    source = ROOT / SOURCE
    if args.baseline:
        source = out / 'macos_record.c'
        source.write_text(subprocess.check_output(['git', 'show', f'{args.baseline}:{SOURCE}'], cwd=ROOT, text=True))
    results = {}
    for name in args.sanitizer or sorted(SANITIZERS):
        extra = SANITIZERS[name] + ['-fno-omit-frame-pointer', '-g']
        obj, exe = out / f'record-{name}.o', out / f'capture-{name}'
        steps = [
            ('compile', flags + extra + ['-I', str(ROOT / SOURCE.parent), '-c', str(source), '-o', str(obj)]),
            ('link', flags + extra + ['-UNDEBUG', '-Wno-shorten-64-to-32', str(ROOT / 'tests/voice/capture_device.c'),
                                      str(obj), '-o', str(exe)]),
            ('run', [str(exe)]),
        ]
        row = {}
        for step, command in steps:
            # The run sleeps more than it computes, and background priority
            # coalesces its timers by a tenth of a second, so only the
            # compiler runs at low priority.
            prefix = [] if step == 'run' else BACKGROUND
            result = subprocess.run(prefix + command, cwd=out, env=env, capture_output=True, text=True, timeout=300)
            (out / f'{name}-{step}.stdout').write_text(result.stdout)
            (out / f'{name}-{step}.stderr').write_text(result.stderr)
            row[step] = result.returncode
            if result.returncode:
                row['error'] = (result.stderr or result.stdout).strip().splitlines()[-20:]
                break
        if row.get('run') == 0:
            lines = (out / f'{name}-run.stdout').read_text().splitlines()
            row['cases'] = lines
            row['all_cases'] = [line.split()[0] for line in lines] == CASES
            stderr = (out / f'{name}-run.stderr').read_text()
            # One open failure is reported once, then retried quietly.
            row['open_failure_lines'] = stderr.count('the microphone could not be opened')
            row['sanitizer_reports'] = stderr.count('WARNING: ThreadSanitizer') + stderr.count('ERROR: AddressSanitizer')
        row['passed'] = (row.get('run') == 0 and row.get('all_cases') and row.get('open_failure_lines') == 1
                         and row.get('sanitizer_reports') == 0)
        results[name] = row
    passed = all(row['passed'] for row in results.values())
    report = dict(passed=passed, results=results)
    (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
