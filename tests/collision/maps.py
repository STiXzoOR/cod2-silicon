#!/usr/bin/env python3
"""Compare old/new production collision on licensed maps, without committing map data."""
import argparse
from collections import Counter
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import time
import zipfile

ROOT = Path(__file__).resolve().parents[2]
RECORD = struct.Struct('<4f5i')
KINDS = ('point_trace', 'capsule_trace', 'point_sight', 'capsule_sight', 'point_contents', 'position')


def function(text, name):
    import re
    match = re.search(r'^static void ' + name + r'\([^;]+?\n\{', text, re.M)
    if not match:
        raise ValueError(name)
    start = match.start()
    end = text.index('\n}', match.end()) + 2
    return text[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--baseline', required=True, help='Local Git revision for old trace/test code')
    parser.add_argument('--data', type=Path, default=Path.home() / 'Games/CoD2')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--queries', type=int, default=120000)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    if out.is_relative_to(ROOT):
        parser.error('Keep maps and captures outside the repository')
    out.mkdir(parents=True, exist_ok=True)
    db = args.build / 'compile_commands.engine.json'
    if not db.exists():
        db = args.build / 'compile_commands.json'
    row = next(x for x in json.loads(db.read_text()) if x['file'].endswith('/src/PC/qcommon/cm_trace.c')
               and 'cod2_macos.dir' in x['command'])
    command = shlex.split(row['command'])
    old_root = str(Path(row['file']).parents[3])
    flags = [x.replace(old_root, str(ROOT)) for x in command[:command.index('-o')]]
    flags += ['-Wl,-dead_strip']
    if args.sanitize:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    trace = (ROOT / 'src/PC/qcommon/cm_trace.c').read_text()
    baseline = subprocess.check_output(['git', 'show', args.baseline + ':src/PC/qcommon/cm_trace.c'], cwd=ROOT, text=True)
    # A diagnostic intermediate changes only the brush clipper. It attributes
    # old/new deltas without using Linux or decompiled source as executable code.
    old_brush = function(baseline, 'CM_TraceThroughBrush')
    new_brush = function(trace, 'CM_TraceThroughBrush')
    brush_only = baseline.replace(old_brush, new_brush)
    baseline_sources = {}
    for name in ['cm_mesh', 'cm_test']:
        baseline_sources[name] = subprocess.check_output(['git', 'show', args.baseline + ':src/PC/qcommon/' + name + '.c'], cwd=ROOT)
    common = [ROOT / 'tests/collision/maps.c', ROOT / 'src/PC/qcommon/cm_tracebox.c',
              ROOT / 'src/PC/qcommon/cm_load_obj.c', ROOT / 'src/PC/universal/com_math.c']
    objects = []
    def compile_object(source, target):
        result = subprocess.run([*flags, '-c', str(source), '-o', str(target)], cwd=ROOT, capture_output=True, text=True)
        (out / (target.stem + '-compile.log')).write_text(result.stdout + result.stderr)
        result.check_returncode()
        return target
    for source in common:
        objects.append(compile_object(source, out / (source.stem + '.o')))
    versions = {}
    for name in baseline_sources:
        for version in ['old', 'new']:
            source = out / (name + '-' + version + '.c')
            source.write_bytes(baseline_sources[name] if version == 'old' else (ROOT / 'src/PC/qcommon' / (name + '.c')).read_bytes())
            versions[name, version] = compile_object(source, source.with_suffix('.o'))
    executables = {}
    for mode, source in [('old', baseline), ('brush_only', brush_only), ('walk', trace), ('new', trace)]:
        copy = out / (mode + '.c')
        copy.write_text(source)
        exe = out / mode
        trace_object = compile_object(copy, copy.with_suffix('.o'))
        version = 'new' if mode == 'new' else 'old'
        result = subprocess.run([*flags, str(trace_object), *map(str, objects),
                                 str(versions['cm_mesh', version]), str(versions['cm_test', version]), '-o', str(exe)],
                                cwd=ROOT, capture_output=True, text=True)
        (out / (mode + '-link.log')).write_text(result.stdout + result.stderr)
        result.check_returncode()
        executables[mode] = exe
    maps = ('mp_toujane', 'mp_carentan', 'mp_dawnville')
    archives = sorted((args.data / 'main').glob('*.iwd'), reverse=True)
    for name in maps:
        member = 'maps/mp/' + name + '.d3dbsp'
        for archive in archives:
            with zipfile.ZipFile(archive) as z:
                if member in z.namelist():
                    (out / (name + '.d3dbsp')).write_bytes(z.read(member))
                    break
        else:
            raise FileNotFoundError(member)
    # Same lease protocol as the engineering wave. Only remove our own lease.
    lock = Path.home() / 'Projects/cod2-native-wt/_agents/bench.lock'
    token = f'F1b {os.getpid()} {time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}\n'
    while True:
        try:
            fd = os.open(lock, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
            os.write(fd, token.encode())
            os.close(fd)
            break
        except FileExistsError:
            owner = lock.read_text().split()
            if len(owner) < 2 or not owner[1].isdigit():
                raise RuntimeError('Unrecognized benchmark lease: ' + str(lock))
            try:
                os.kill(int(owner[1]), 0)
            except ProcessLookupError:
                raise RuntimeError('Stale benchmark lease; only its owner may remove it: ' + str(lock)) from None
            print('Waiting for benchmark lease', flush=True)
            time.sleep(60)
    try:
        screen = subprocess.check_output(['/usr/sbin/ioreg', '-l', '-n', 'Root', '-d', '1'], text=True)
        summary = {'screen_locked': 'CGSSessionScreenIsLocked' in screen and 'CGSSessionScreenIsLocked\"=Yes' in screen.replace(' ', ''),
                   'lease': token.strip(), 'baseline': args.baseline, 'queries_per_map': args.queries,
                   'sanitized': args.sanitize, 'maps': {},
                   'comparison': 'old/new trace, mesh, and point contents on the same current production-loaded BSP; no original i386 execution'}
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
        for name in maps:
            captures, timing = {}, {}
            for mode, exe in executables.items():
                capture = out / (name + '-' + mode + '.bin')
                result = subprocess.run([str(exe), str(out / (name + '.d3dbsp')), str(capture), str(args.queries)],
                                        capture_output=True, text=True, env=env)
                (out / (name + '-' + mode + '.log')).write_text(result.stdout + result.stderr)
                result.check_returncode()
                timing[mode] = json.loads(result.stdout)
                captures[mode] = capture.read_bytes()
            differences, causes, fields, samples = Counter(), Counter(), Counter(), []
            delta_rows = ['query,kind,cause,fields']
            for q in range(args.queries):
                offset = q * RECORD.size
                old, middle, walk, new = [captures[m][offset:offset + RECORD.size] for m in ['old', 'brush_only', 'walk', 'new']]
                if old == new:
                    continue
                kind = KINDS[q % len(KINDS)]
                differences[kind] += 1
                reasons = []
                if old != middle:
                    reasons.append('brush_clipping')
                if middle != walk:
                    reasons.append('traversal_and_setup')
                if walk != new:
                    reasons.append('mesh_and_empty_contents')
                cause = '+'.join(reasons)
                causes[cause] += 1
                old_values, new_values = RECORD.unpack(old), RECORD.unpack(new)
                changed = []
                for key, a, b in zip(['fraction', 'nx', 'ny', 'nz', 'surface', 'contents', 'material', 'solid', 'hint'], old_values, new_values):
                    if a != b:
                        fields[key] += 1
                        changed.append(key)
                delta_rows.append(f'{q},{kind},{cause},{"+".join(changed)}')
                if len(samples) < 8:
                    samples.append({'query': q, 'kind': kind, 'cause': cause, 'old': old_values, 'new': new_values})
            (out / (name + '-differences.csv')).write_text('\n'.join(delta_rows) + '\n')
            summary['maps'][name] = {'differences': dict(differences), 'causes': dict(causes),
                                     'fields': dict(fields), 'runs': timing, 'samples': samples}
            print(name, json.dumps({'differences': dict(differences), 'causes': dict(causes), 'runs': timing}), flush=True)
        (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    finally:
        if lock.read_text() == token:
            lock.unlink()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
