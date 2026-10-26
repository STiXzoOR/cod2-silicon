#!/usr/bin/env python3
"""Check production screenshot readback and serialization with synthetic GL pixels."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
SOURCE = 'src/PC/gfx_d3d/r_screenshot.c'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-db', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--baseline', help='read production source from a local commit')
    options = parser.parse_args()
    output = options.output.resolve()
    if output == ROOT or ROOT in output.parents:
        parser.error('--output must be outside the checkout')
    output.mkdir(parents=True, exist_ok=True)
    source = (subprocess.check_output(['git', 'show', f'{options.baseline}:{SOURCE}'],
                                     cwd=ROOT, text=True) if options.baseline
              else (ROOT / SOURCE).read_text())
    (output / 'screenshot_source.h').write_text(source)
    entry = next(row for row in json.loads(options.compile_db.read_text())
                 if row['file'].endswith('/' + SOURCE))
    source_root = entry['file'][:-len('/' + SOURCE)]
    command = entry.get('arguments') or shlex.split(entry['command'])
    flags = []
    skip = False
    for arg in command:
        if skip:
            skip = False
        elif arg in ('-o', '-MF', '-MT', '-MQ'):
            skip = True
        elif arg not in ('-c', '-MD', '-MMD', '-DNDEBUG', entry['file']):
            flags.append(arg.replace(source_root, str(ROOT)))
    binary = output / 'screenshot'
    with (output / 'compile.log').open('w') as log:
        subprocess.run([*flags, '-UNDEBUG', '-I' + str(output),
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-Wl,-dead_strip',
                        str(ROOT / 'tests/lp64/renderer/screenshot.c'), '-o', str(binary)],
                       cwd=ROOT, stdout=log, stderr=log, check=True)
    failures = []
    for case in ['tga', 'jpeg', 'read-failure', 'levelshot', 'savegame', 'upsample',
                 'shrink-x-expand-y', 'expand-x-shrink-y', 'same-size', 'resample-failure',
                 'upsample-endpoint', 'downsample-boundary', 'noninteger-mixed', 'single-pixel']:
        result = subprocess.run([str(binary), case], capture_output=True, text=True,
                                env={**os.environ, 'ASAN_OPTIONS': 'symbolize=0',
                                     'UBSAN_OPTIONS': 'halt_on_error=1'})
        (output / (case + '.log')).write_text(result.stdout + result.stderr)
        print(case + ': ' + ('FAIL' if result.returncode else 'PASS'))
        if result.returncode:
            failures.append(case)
    if failures:
        raise SystemExit('Failed screenshot fixtures: ' + ', '.join(failures))


if __name__ == '__main__':
    main()
