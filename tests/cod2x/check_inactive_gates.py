#!/usr/bin/env python3
"""Compare complete Original 1.3 client/server compiler inputs with a reviewed base.

This is a native stock isolation check, not a legacy-platform or gameplay test.
Both revisions are independently configured, so added/removed modules and flags
are checked too. Includes, Objective-C imports and generated data stay intact.
"""
import argparse
import concurrent.futures
import difflib
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[2]
RECEIPT = Path(__file__).with_name('inactive-baseline.json')
# Diagnostic/build provenance is not CoD2x behavior. Do not normalize other
# tokens: whitespace inside a string literal, for example, remains significant.
FROZEN = ['-D__FILE__="source"', '-D__BASE_FILE__="source"', '-D__LINE__=0',
          '-D__DATE__="fixed"', '-D__TIME__="fixed"', '-D__TIMESTAMP__="fixed"',
          '-DCOD2_GIT_HASH="fixed"', '-Wno-builtin-macro-redefined']


def sha(data):
    return hashlib.sha256(data).hexdigest()


def run(command, **kwargs):
    result = subprocess.run(command, capture_output=True, **kwargs)
    if result.returncode:
        raise RuntimeError(shlex.join(map(str, command)) + '\n' +
                           result.stderr.decode(errors='replace'))
    return result.stdout


def export(base, tree):
    tree.mkdir()
    command = ['git', 'archive', base, 'CMakeLists.txt', 'RELEASE_VERSION',
               'cmake', 'src', 'build/lp64_gen', 'tools']
    process = subprocess.Popen(command, cwd=ROOT, stdout=subprocess.PIPE)
    with tarfile.open(fileobj=process.stdout, mode='r|') as archive:
        archive.extractall(tree, filter='data')
    if process.wait():
        raise RuntimeError('Could not export baseline ' + base)


def configure(tree, build, evidence, side):
    # Explicitly use the verified committed snapshot, not private binary inputs
    # found through the caller's HOME. Each side runs its own dedicated generator.
    command = ['cmake', '-S', str(tree), '-B', str(build),
               '-DCOD2_X64=ON', '-DCMAKE_BUILD_TYPE=Release',
               '-DCOD2_FEATURE_CFLAGS=', '-DCOD2_REGENERATE_TYPED_DATA=OFF',
               '-DCOD2_UPDATE_TYPED_SNAPSHOT=OFF',
               '-DCOD2_STABS_BINARY=' + str(build / 'absent-stabs'),
               '-DCOD2_VALUES_BINARY=' + str(build / 'absent-values')]
    (evidence / (side + '-configure.log')).write_bytes(run(command))
    names = ('data_native.c', 'literals_native.c', 'import_pointers_native.c', 'bss_native.c')
    run(['python3', str(tree / 'tools/server/dedicated_data.py'),
         str(build / 'dedicated_gen'), *[str(tree / 'build/lp64_gen' / n) for n in names]])
    entries = json.loads((build / 'compile_commands.json').read_text())
    result = {}
    for entry in entries:
        source = Path(entry['file'])
        mode = 'dedicated' if '-DDEDICATED' in shlex.split(entry['command']) else 'client'
        path = ('@build/' + str(source.relative_to(build)) if source.is_relative_to(build)
                else str(source.relative_to(tree)))
        key = mode + ':' + path
        if key in result:
            raise RuntimeError('Duplicate compile entry: ' + key)
        result[key] = entry
    return result


def flags(entry, tree, build):
    words = iter(shlex.split(entry['command']))
    kept = []
    for word in words:
        if word == '-o':
            next(words)
        elif word == '-c' or word == entry['file'] or word.startswith(('-DCOD2_GIT_HASH=', '-fdebug-prefix-map=')):
            continue
        else:
            kept.append(word)
    if any(w.startswith('-DCOD2_CODX') for w in kept):
        raise RuntimeError('CoD2x definition in Original 1.3 compile entry: ' + entry['file'])
    normalized = [w.replace(str(build), '@build').replace(str(tree), '@source') for w in kept]
    return kept, normalized


def compare(item, sides, evidence):
    index, key = item
    row = {'unit': key, 'configurations': {}}
    prepared = []
    for tree, build, entries in sides:
        entry = entries[key]
        command, normalized = flags(entry, tree, build)
        source = Path(entry['file'])
        suffix = source.suffix
        command += FROZEN + ['-g0', '-iquote', str(source.parent), '-x',
                             'objective-c' if suffix == '.m' else 'c++' if suffix == '.cpp' else 'c']
        prepared.append((tree, command, normalized, source.read_bytes()))
    if prepared[0][2] != prepared[1][2]:
        row.update(status='changed-flags', flags=[p[2] for p in prepared])
        return row
    for definition in ('undefined', 'zero'):
        outputs = []
        suffix_flags = [] if definition == 'undefined' else ['-DCOD2_CODX=0']
        for tree, command, _, source in prepared:
            outputs.append(run(command + suffix_flags + ['-E', '-P', '-'], input=source, cwd=tree))
        result = {'preprocess_sha256': [sha(output) for output in outputs]}
        if outputs[0] == outputs[1]:
            result['status'] = 'identical-preprocessed'
        else:
            # Private declarations or line layout can differ yet emit exactly
            # the same code and data. Compare the entire optimized assembly.
            outputs = [run(command + suffix_flags + ['-S', '-o', '-', '-'], input=source, cwd=tree)
                       for tree, command, _, source in prepared]
            result['assembly_sha256'] = [sha(output) for output in outputs]
            result['status'] = 'identical-assembly' if outputs[0] == outputs[1] else 'changed-assembly'
            if outputs[0] != outputs[1]:
                difference = ''.join(difflib.unified_diff(
                    outputs[0].decode().splitlines(True), outputs[1].decode().splitlines(True),
                    fromfile='baseline/' + key, tofile='candidate/' + key))
                (evidence / f'{index}-{definition}.diff').write_text(difference)
        row['configurations'][definition] = result
    row['status'] = ('pass' if all(v['status'].startswith('identical-')
                                 for v in row['configurations'].values()) else 'changed')
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', help='Exact pre-change commit; defaults to the reviewed integration receipt')
    parser.add_argument('--output', type=Path, default=ROOT / 'output/inactive-gates')
    parser.add_argument('--jobs', type=int, default=3)
    args = parser.parse_args()
    base = args.base or json.loads(RECEIPT.read_text())['baseline']
    base = run(['git', 'rev-parse', '--verify', base + '^{commit}'], cwd=ROOT).decode().strip()
    evidence = args.output.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    scratch_parent = ROOT / 'output'
    scratch_parent.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='inactive-gate-', dir=scratch_parent) as temporary:
        scratch = Path(temporary)
        baseline = scratch / 'baseline'
        export(base, baseline)
        sides = []
        for name, tree in (('baseline', baseline), ('candidate', ROOT)):
            build = scratch / (name + '-build')
            sides.append((tree, build, configure(tree, build, evidence, name)))
        before, after = (set(s[2]) for s in sides)
        receipt = {'base': base, 'scope': 'Complete native stock Release client and dedicated TUs; COD2_CODX undefined and zero',
                   'removed': sorted(before - after), 'added': sorted(after - before), 'results': []}
        failures = len(receipt['removed']) + len(receipt['added'])
        for label in ('removed', 'added'):
            for key in receipt[label]:
                print(label, key, flush=True)
        with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
            futures = {pool.submit(compare, item, sides, evidence): item
                       for item in enumerate(sorted(before & after))}
            for future in concurrent.futures.as_completed(futures):
                index, key = futures[future]
                try:
                    row = future.result()
                except Exception as error:
                    row = {'unit': key, 'status': 'error', 'diagnostic': str(error)}
                receipt['results'].append(row)
                if row['status'] != 'pass':
                    failures += 1
                    print(row['status'], key, row.get('diagnostic', ''), flush=True)
        receipt['results'].sort(key=lambda row: row['unit'])
        receipt['failures'] = failures
        (evidence / 'comparison.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'{len(before)} baseline / {len(after)} candidate units; two inactive configurations; {failures} failures')
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
