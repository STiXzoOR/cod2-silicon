#!/usr/bin/env python3
"""Exercise CG_Item using the committed generated count and import definitions."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-commands', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
output = args.output.resolve()
if output == ROOT or ROOT in output.parents:
    parser.error('--output must be outside the checkout')
output.mkdir(parents=True, exist_ok=True)
relative = 'src/PC/cgame_mp/cg_ents_mp.c'
rows = json.loads(args.compile_commands.read_text())
row = next(row for row in rows if row['file'].endswith('/' + relative)
           and 'cod2_macos.dir' in (row.get('command') or ' '.join(row['arguments'])))
old_root = row['file'][:-len(relative)].rstrip('/')
command = row.get('arguments') or shlex.split(row['command'])
command = [part.replace(old_root, str(ROOT)) for part in command]
flags, skip = [], False
for part in command:
    if skip:
        skip = False
    elif part in ('-o', '-MF', '-MT', '-MQ'):
        skip = True
    elif part not in ('-c', '-MD', '-MMD', '-DNDEBUG', str(ROOT / relative)):
        flags.append(part)

source = (ROOT / relative).read_text()
start = source.index('void CG_Item(centity_t *cent)\n{')
end = source.index('\nvoid CG_Missile(', start)
(output / 'item_source.h').write_text(source[start:end])
# Select the actual architecture's generated definitions, preserving the named
# import target. Other unrelated blob objects need not be linked into this test.
scalar = output / 'item_data.c'
definition = []
for filename, symbol in [('literals_native.c', 'bg_numItems'),
                         ('import_pointers_native.c', 'imp_bg_numItems')]:
    text = subprocess.check_output([flags[0], '-E', '-P',
                                   str(ROOT / 'build/lp64_gen' / filename)], text=True)
    matches = [line for line in text.splitlines()
               if re.search(r'\bdg_object_\d+ __asm__', line)
               and '"' + symbol + '"' in line]
    if len(matches) != 1:
        raise ValueError(f'{symbol}: expected exactly one generated definition')
    definition.append(matches[0])
scalar.write_text('#include "typed_types.h"\n' + '\n'.join(definition) + '\n')
sanitize = ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer']
subprocess.run([*flags, '-I' + str(ROOT / 'build/lp64_gen'), *sanitize,
                '-c', str(scalar), '-o', str(output / 'item_data.o')], check=True)
subprocess.run([*flags, *sanitize, '-I' + str(output),
                str(ROOT / 'tests/camera/items.c'), str(output / 'item_data.o'),
                '-o', str(output / 'items')], cwd=ROOT, check=True)
os.environ.setdefault('UBSAN_OPTIONS', 'halt_on_error=1')
subprocess.run([str(output / 'items')], cwd=output, check=True)
