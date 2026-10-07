#!/usr/bin/env python3
"""Compare production PmoveSingle with an external readable 1.3 reference.

Run run.py first. No reference source, binaries or outputs enter the repository.
The reference shares collision, jump/mantle, animation and weapon services with
the production fixture. Mac disassembly remains the authority for differences.
"""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, default=ROOT / 'build-macos')
p.add_argument('--reference', type=Path, required=True, help='External lnxded-1.3/src/bgame/bg_pmove.cpp')
p.add_argument('--output', type=Path, required=True, help='Private directory outside this worktree')
a = p.parse_args()
out = a.output.expanduser().resolve()
if out == ROOT or ROOT in out.parents:
    p.error('Reference code and evidence must stay outside the repository')
out.mkdir(parents=True, exist_ok=True)
source = a.reference.expanduser().resolve()
data = source.read_text()
data = data.replace('#include "../qcommon/qcommon.h"', '#include "' + str(source.parent.parent / 'qcommon/qcommon.h') + '"')
data = data.replace('#include "bg_public.h"', '#include "' + str(source.parent / 'bg_public.h') + '"')
names = ['pmoveHandlers', 'viewLerp_CrouchProne', 'viewLerp_CrouchProneLinear',
         'viewLerp_CrouchStand', 'viewLerp_ProneCrouch', 'viewLerp_StandCrouch']
bridge = out / 'rev-bridge.cpp'
bridge.write_text(''.join(f'#define {name} ws43_rev_{name}\n' for name in names) + data +
                  '\nextern "C" void ws43_reference_single(void *pm) { PmoveSingle((pmove_t *)pm); }\n')
obj = out / 'rev-bridge.o'
subprocess.run(['clang++', '-arch', 'arm64', '-O3', '-ffp-contract=off',
                '-fno-strict-aliasing', '-Wno-register', '-DByte=unsigned char',
                '-c', str(bridge), '-o', str(obj)], check=True)
aliases = []
for symbol in subprocess.check_output(['nm', '-u', str(obj)], text=True).splitlines():
    match = re.match(r'__Z(\d+)(.*)', symbol)
    if match:
        name = match[2][:int(match[1])]
        service = {'CG_Trace': 'CG_TraceCapsule', 'va': 'ws43_fixture_va'}.get(name, name)
        aliases.append('-Wl,-alias,_' + service + ',' + symbol)
build = a.build.resolve()
fixture = build / 'movement-tests'
rows = json.loads((build / 'compile_commands.json').read_text())
entry = next(row for row in rows if row['file'].endswith('/src/PC/bgame/bg_pmove.c'))
flags = shlex.split(entry['command'])
flags = [x for x in flags[:flags.index('-o')] if x != '-DNDEBUG']
flags += ['-UNDEBUG', '-UCOD2_CODX', '-I' + str(fixture), '-DMOVEMENT_REFERENCE=1',
          '-Dva=ws43_fixture_va', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
native = out / 'reference-fixture.o'
subprocess.run([*flags, '-c', str(ROOT / 'tests/movement/collision.c'), '-o', str(native)], check=True)
exe = out / 'differential'
subprocess.run(['clang++', '-arch', 'arm64', '-fsanitize=address,undefined',
                '-Wl,-dead_strip', *aliases, str(native), str(obj),
                str(fixture / 'com_math.o'), str(fixture / 'q_shared.o'),
                str(fixture / 'port_debug.o'), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
