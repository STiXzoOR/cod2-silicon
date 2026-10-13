#!/usr/bin/env python3
"""Run the real environment command and state consumer without an audio device."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/fidelity'))
from spotcheck_reverb import extract_function

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', required=True, type=Path)
a = p.parse_args()
rows = json.loads((a.build / 'compile_commands.engine.json').read_text())
entry = next(r for r in rows if r['file'].endswith('/src/PC/snd.c') and 'cod2_macos.dir' in r['command'])
flags = shlex.split(entry['command'])
flags = [f for f in flags[:flags.index('-o')] if f != '-DNDEBUG']
flags += ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
out = a.build.resolve() / 'audio-contract'
out.mkdir(parents=True, exist_ok=True)
source = (ROOT / 'src/PC/snd.c').read_text()
table = re.search(r'const char \*snd_roomStrings\[32\] = \{.*?\};', source, re.S)[0]
try:
    command = extract_function(source, 'SND_SetEnvironmentEffects_f')
except ValueError:
    command = extract_function((ROOT / 'src/stubs/link_stubs.c').read_text(), 'SND_SetEnvironmentEffects_f')
(out / 'environment_source.h').write_text(table + '\n' + extract_function(source, 'SND_SetEnvironmentEffects') + '\n' + command)
exe = out / 'environment'
subprocess.run(flags + ['-I' + str(out), str(ROOT / 'tests/audio/environment.c'), '-o', str(exe)], cwd=entry['directory'], check=True)
subprocess.run([str(exe)], check=True)
with tempfile.TemporaryDirectory(prefix='cod2-audio-') as tmp:
    result = Path(tmp) / 'reverb.json'
    probe = subprocess.run([sys.executable, str(ROOT / 'tools/fidelity/spotcheck_reverb.py'),
                            '--build', str(a.build.resolve()), '--output', str(result)],
                           capture_output=True, text=True)
    if probe.returncode:
        print(probe.stdout + probe.stderr)
        probe.check_returncode()
    observed = json.loads(result.read_text())
    print(f"reverb: {observed['total']} reference cases, {observed['differences']} differences")
