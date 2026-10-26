#!/usr/bin/env python3
"""Exercise reliable reverb and offhand commands against Mac 1.3 expectations."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-db', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--baseline')
parser.add_argument('--case', action='append')
args = parser.parse_args()
output = args.output.resolve()
if output == ROOT or ROOT in output.parents:
    parser.error('--output must be outside the checkout')
output.mkdir(parents=True, exist_ok=True)
rows = json.loads(args.compile_db.read_text())


def compile_entry(relative):
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
        elif part not in ('-c', '-MD', '-MMD', str(ROOT / relative)):
            flags.append(part)
    return flags


source_path = 'src/PC/cgame_mp/cg_servercmds_mp.c'
source = (subprocess.check_output(['git', 'show', args.baseline + ':' + source_path], cwd=ROOT)
          if args.baseline else (ROOT / source_path).read_bytes())
(output / 'server_commands_source.h').write_bytes(source)
sys.path.insert(0, str(ROOT / 'tools/fidelity'))
from spotcheck_reverb import extract_function
main_source = (ROOT / 'src/PC/cgame_mp/cg_main_mp.c').read_text()
offhand_source = (ROOT / 'src/PC/cgame_mp/offhandweapons_mp.c').read_text()
sound_source = (ROOT / 'src/PC/snd.c').read_text()
# Preserve the real shared-buffer argument reader and state consumers.
helpers = 'static char buffer[1024];\n' + re.search(r'^const char \*CG_Argv\(int arg\)\n\{[^}]+\}', main_source, re.M)[0]
helpers += '\n' + extract_function(offhand_source, 'CG_SetEquippedOffHand')
helpers += '\n' + re.search(r'const char \*snd_roomStrings\[32\] = \{.*?\};', sound_source, re.S)[0]
helpers += '\n' + extract_function(sound_source, 'SND_SetEnvironmentEffects')
(output / 'server_commands_helpers.h').write_text(helpers)
flags = compile_entry(source_path)
subprocess.run([*flags, '-c', str(ROOT / source_path), '-o', str(output / 'server-commands-production.o')],
               cwd=ROOT, check=True)
flags = [flag for flag in flags if flag != '-DNDEBUG']
sanitize = ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections']
executable = output / 'server-commands'
subprocess.run([*flags, *sanitize, '-I' + str(output),
                '-iquote', str(ROOT / 'src/PC/cgame_mp'), '-Wl,-dead_strip',
                str(ROOT / 'tests/camera/server_commands.c'), '-o', str(executable)],
               cwd=ROOT, check=True)
os.environ.setdefault('UBSAN_OPTIONS', 'halt_on_error=1')
cases = args.case or ['reverb-room', 'reverb-fades', 'reverb-arity', 'offhand-reject', 'offhand-unregistered', 'offhand-valid', 'sequence']
failed = []
for case in cases:
    result = subprocess.run([str(executable), case], cwd=output, capture_output=True, text=True)
    (output / (case + '.log')).write_text(result.stdout + result.stderr)
    print(f'{case}: {result.returncode}')
    if result.returncode:
        failed.append(case)
if failed:
    raise SystemExit('FAIL server commands: ' + ', '.join(failed))
