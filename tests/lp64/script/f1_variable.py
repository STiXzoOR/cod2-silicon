#!/usr/bin/env python3
"""Exercise real variable-source behavior with Mac 1.3 expectations."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-db', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--source', type=Path, default=ROOT / 'src/PC/script/scr_variable.c')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
rows = json.loads(args.compile_db.read_text())
row = next(r for r in rows if r['file'].endswith('/scr_variable.c') and
           'cod2_macos.dir/' in (r.get('command') or ' '.join(r['arguments'])))
argv = row.get('arguments') or shlex.split(row['command'])
original = row['file'][:-len('/src/PC/script/scr_variable.c')]
flags = [v.replace(original, str(ROOT)) for v in argv[1:argv.index('-o')]
         if v not in ('-c', '-DNDEBUG')]
binary = args.output / 'test'
command = [argv[0], *flags, '-UNDEBUG', '-fsanitize=address,undefined',
           '-fno-sanitize-recover=all', '-Wl,-dead_strip',
           '-DF1_VARIABLE_SOURCE=' + json.dumps(str(args.source.resolve())),
           str(ROOT / 'tests/lp64/script/f1_variable.c'),
           str(ROOT / 'src/PC/script/scr_memorytree.c'),
           str(ROOT / 'src/PC/qcommon/port_debug.c'), '-o', str(binary)]
result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
(args.output / 'compile.log').write_text(result.stdout + result.stderr)
if result.returncode:
    print(result.stdout + result.stderr)
    raise SystemExit(result.returncode)
failed = []
for name in ['mixed_parent_churn', 'adjacent_collision', 'hash_collision_stress', 'object_sibling_stress', 'packed_notify', 'packed_field', 'variable_object_error', 'field_object_error', 'equality_developer',
             'equality_evaluate', 'evaluate_error_flow', 'array_ref_string', 'entity_array_ref_string',
             'entity_clear_string', 'entity_readonly', 'entity_clear_undefined',
             'entity_array_ref_vector', 'entity_array_writable', 'array_copy_on_write',
             'clear_array_missing',
             'unaligned_vector_alloc', 'unaligned_vector_eval', 'unaligned_vector_plus',
             'unaligned_vector_minus', 'unaligned_vector_equal',
             'clear_detaches_children', 'thread_compare', 'thread_dump']:
    result = subprocess.run([str(binary), name], cwd=ROOT, text=True, capture_output=True)
    (args.output / (name + '.log')).write_text(result.stdout + result.stderr)
    print(f'{name}: {"PASS" if result.returncode == 0 else "FAIL"}')
    if result.returncode:
        failed.append(name)
(args.output / 'result.json').write_text(json.dumps({'failed': failed}, indent=2) + '\n')
raise SystemExit(bool(failed))
