#!/usr/bin/env python3
"""Exercise connection text using production functions and the engine ABI."""
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
database = Path(sys.argv[1])
entry = next(e for e in json.loads(database.read_text())
             if e['file'].endswith('/src/PC/ui_mp/ui_main_mp.c'))
flags = shlex.split(entry['command'])
flags = flags[:flags.index('-o')]
flags += ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
source = (root / 'src/PC/ui_mp/ui_main_mp.c').read_text()
with tempfile.TemporaryDirectory(prefix='ws14-online-') as tmp:
    out = Path(tmp)
    functions = []
    for name in ['UI_ReplaceConversions', 'UI_ReplaceConversionString']:
        match = re.search(r'^const char \*' + name + r'\([^;]*?\)\n\{', source, re.M)
        depth = 0
        for token in re.finditer(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|[{}]',
                                 source[match.start():]):
            if token[0] == '{':
                depth += 1
            elif token[0] == '}':
                depth -= 1
                if not depth:
                    functions.append(source[match.start():match.start() + token.end()])
                    break
    (out / 'ui_conversion_source.h').write_text('\n'.join(functions))
    obj = out / 'ui.o'
    subprocess.run([*flags, '-I' + tmp, '-c', str(root / 'tests/online/ui_conversion.c'),
                    '-o', str(obj)], check=True, cwd=root)
    exe = out / 'ui'
    subprocess.run(['clang', '-fsanitize=address,undefined', str(obj), '-o', str(exe)],
                   check=True)
    subprocess.run([str(exe)], check=True, timeout=20,
                   env={**os.environ, 'ASAN_OPTIONS': 'symbolize=0'})
