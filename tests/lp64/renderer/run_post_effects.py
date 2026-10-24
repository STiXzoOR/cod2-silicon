#!/usr/bin/env python3
"""Check production late post effects with recorded blur and feedback draws."""
import argparse
import json
import os
import re
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
SOURCE = 'src/PC/gfx_d3d/rb_backend.c'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-db', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--baseline', help='read production source from a local commit')
    options = parser.parse_args()
    output = options.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    source = (subprocess.check_output(['git', 'show', f'{options.baseline}:{SOURCE}'],
                                     cwd=ROOT, text=True) if options.baseline
              else (ROOT / SOURCE).read_text())
    functions = []
    for name in ['nextPow2', 'RB_ApplyLatePostEffectsCmd']:
        match = re.search(r'^static (?:inline )?(?:unsigned int|void) ' + name + r'\([^;]*?\)\n\{', source, re.M)
        assert match, name
        depth = 0
        for token in re.finditer(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|[{}]', source[match.start():]):
            if token[0] == '{': depth += 1
            elif token[0] == '}':
                depth -= 1
                if not depth:
                    functions.append(source[match.start():match.start() + token.end()])
                    break
    (output / 'post_effects_source.h').write_text('\n'.join(functions))
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
    binary = output / 'post_effects'
    with (output / 'compile.log').open('w') as log:
        subprocess.run([*flags, '-UNDEBUG', '-I' + str(output),
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                        str(ROOT / 'tests/lp64/renderer/post_effects.c'), '-o', str(binary)],
                       cwd=ROOT, stdout=log, stderr=log, check=True)
    failures = []
    for case in ['native-extent', 'below-threshold', 'above-threshold']:
        result = subprocess.run([str(binary), case], capture_output=True, text=True,
                                env={**os.environ, 'ASAN_OPTIONS': 'symbolize=0',
                                     'UBSAN_OPTIONS': 'halt_on_error=1'})
        (output / (case + '.log')).write_text(result.stdout + result.stderr)
        print(case + ': ' + ('FAIL' if result.returncode else 'PASS'))
        if result.returncode:
            failures.append(case)
    if failures:
        raise SystemExit('Failed post effects fixtures: ' + ', '.join(failures))


if __name__ == '__main__':
    main()
