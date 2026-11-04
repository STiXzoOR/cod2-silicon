"""Build one production translation unit with a fixture and run named cases.

Large game modules reference much of the engine through builtin tables. Symbols a fixture
does not provide are bound to inert zeroed storage, so calling one faults at once instead of
silently passing.
"""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def run(description, source, fixture, cases):
    p = argparse.ArgumentParser(description=description)
    p.add_argument('--compile-db', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--baseline', help='Read production source from this local revision')
    p.add_argument('--case', action='append', choices=cases)
    a = p.parse_args()
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    rows = json.loads(a.compile_db.read_text())
    entry = next(row for row in rows if row['file'].endswith('/' + source))
    command = entry.get('arguments') or shlex.split(entry['command'])
    old_root = entry['file'][:-len('/' + source)]
    flags = [arg.replace(old_root, str(ROOT)) for arg in command[:command.index('-o')]
             if arg not in ('-c', '-DNDEBUG')]
    flags += ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
              '-fno-omit-frame-pointer']
    production = out / Path(source).name
    production.write_bytes(subprocess.check_output(['git', 'show', a.baseline + ':' + source], cwd=ROOT)
                           if a.baseline else (ROOT / source).read_bytes())
    objects = []
    for path in [production, ROOT / fixture]:
        obj = out / (path.stem + '.o')
        with (out / (path.stem + '.compile.log')).open('w') as log:
            subprocess.run([*flags, '-c', str(path), '-o', str(obj)], cwd=ROOT,
                           stdout=log, stderr=log, check=True)
        objects.append(str(obj))
    exe = out / Path(fixture).stem
    link = [flags[0], '-arch', 'arm64', '-fsanitize=address,undefined', *objects, '-o', str(exe)]
    first = subprocess.run(link, capture_output=True, text=True)
    if first.returncode:
        names = sorted(set(re.findall(r'^\s+"(_[^"]+)", referenced from:', first.stderr, re.M)))
        if not names:
            raise SystemExit(first.stderr[-4000:])
        stubs = out / 'inert_symbols.s'
        stubs.write_text('.data\n.p2align 4\n' + ''.join(
            f'.globl {name}\n{name}:\n.space 4096\n' for name in names))
        inert = out / 'inert_symbols.o'
        subprocess.run([flags[0], '-arch', 'arm64', '-c', str(stubs), '-o', str(inert)], check=True)
        subprocess.run([*link, str(inert)], check=True)
    failed = []
    for case in a.case or cases:
        with (out / (case + '.log')).open('w') as log:
            result = subprocess.run([str(exe), case], stdout=log, stderr=log, cwd=out)
        print(case + ': ' + ('FAIL' if result.returncode else 'pass'))
        if result.returncode:
            failed.append(case)
    if failed:
        raise SystemExit('Failed cases: ' + ', '.join(failed))
