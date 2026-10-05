#!/usr/bin/env python3
"""Exercise production binding/multi-choice handlers and debug table allocation."""
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
out = a.build.resolve() / ('ui-lp64-baseline' if a.baseline else 'ui-lp64-tests')
out.mkdir(exist_ok=True)
entry = next(e for e in json.loads((a.build / 'compile_commands.json').read_text()) if e['file'].endswith('ui_shared_mp.c'))
flags = shlex.split(entry['command']); flags = flags[:flags.index('-o')]
flags = [x for x in flags if x != '-DNDEBUG'] + ['-UNDEBUG', '-w', '-I' + str(out), '-fsanitize=address,undefined', '-fno-omit-frame-pointer']

def active(path):
    source = subprocess.check_output(['git', 'show', a.baseline + ':' + path], cwd=ROOT) if a.baseline else (ROOT / path).read_bytes()
    return subprocess.run([*flags, '-E', '-P', '-x', 'c', '-'], input=source, cwd=ROOT, capture_output=True, check=True).stdout.decode()

def function(source, name):
    match = re.search(r'^.*\b' + name + r'\([^;{]*\)\s*\{', source, re.M)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', source[match.start():]):
        if token[0] == '{': depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth: return source[match.start():match.start() + token.end()] + '\n'
    raise ValueError(name)

ui = active('src/PC/ui_mp/ui_shared_mp.c')
handler = function(ui, 'Item_HandleKey')
choices = handler[handler.index('case 0xc:'):handler.index('case 0xe:')]
code = function(ui, 'Item_Bind_HandleKey') + '\nstatic int choice_key(displayContextDef_t *dc, itemDef_t *item, int key) { byte *it = (byte *)item; byte *d = (byte *)dc; switch(item->type) {\n' + choices + '\n} return 0; }\n'
code += function(active('src/PC/script/scr_parser.c'), 'Scr_InitOpcodeLookup')
(out / 'production.h').write_text(code)
exe = out / 'ui'
subprocess.run([*flags, str(ROOT / 'tests/lp64/ui/handlers.c'), '-o', str(exe)], cwd=ROOT, check=True)
subprocess.run([str(exe)], cwd=out, check=True)
print('PASS native binding, multi-choice, enum and script debug allocation')
