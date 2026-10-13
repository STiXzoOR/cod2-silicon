#!/usr/bin/env python3
"""Extract and exercise the actual native listbox handler (no patched copy)."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, default=ROOT / 'build-macos')
p.add_argument('--baseline')
a = p.parse_args()
out = a.build.resolve() / 'ui-listbox-tests'
out.mkdir(exist_ok=True)
entry = next(e for e in json.loads((a.build / 'compile_commands.json').read_text()) if e['file'].endswith('ui_shared_mp.c'))
flags = shlex.split(entry['command']); flags = flags[:flags.index('-o')]
flags = [x for x in flags if x != '-DNDEBUG'] + ['-UNDEBUG', '-w', '-I' + str(out), '-fsanitize=address,undefined']
path = 'src/PC/ui_mp/ui_shared_mp.c'
source = subprocess.check_output(['git', 'show', a.baseline + ':' + path], cwd=ROOT) if a.baseline else (ROOT / path).read_bytes()
source = subprocess.run([*flags, '-E', '-P', '-x', 'c', '-'], input=source, cwd=ROOT, capture_output=True, check=True).stdout.decode()
def function(name):
    match = re.search(r'^.*\b' + name + r'\([^;{]*\)\s*\{', source, re.M)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', source[match.start():]):
        if token[0] == '{': depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth:
                return source[match.start():match.start() + token.end()] + '\n'
    raise ValueError(name)
(out / 'listbox_production.h').write_text(function('Item_ListBox_HandleKey'))
(out / 'listpaint_production.h').write_text(function('Item_List_Paint'))
exe = out / 'listbox'
subprocess.run([*flags, str(ROOT / 'tests/lp64/ui/listbox_test.c'), '-o', str(exe)], cwd=ROOT, check=True)
subprocess.run([str(exe)], cwd=out, check=True)
exe = out / 'listpaint'
subprocess.run([*flags, str(ROOT / 'tests/lp64/ui/listpaint_test.c'), '-o', str(exe)], cwd=ROOT, check=True)
subprocess.run([str(exe)], cwd=out, check=True)
