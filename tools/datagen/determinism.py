#!/usr/bin/env python3
"""Check a second generation against hashes of every generated text output."""
import hashlib
from pathlib import Path
import subprocess
import sys

binary, output, compiler, values_binary = sys.argv[1:]
out = Path(output)
paths = sorted(p for p in out.iterdir() if p.suffix in ('.c', '.h', '.json', '.txt'))
before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
subprocess.run([sys.executable, str(Path(__file__).with_name('generate.py')), '--binary', binary,
                '--values-binary', values_binary,
                '--output', output, '--clang', compiler], check=True, stdout=subprocess.DEVNULL)
after = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
if before != after:
    raise SystemExit('FAIL: generation changed: ' + ', '.join(name for name in before if before[name] != after[name]))
print('%d generated text files identical after regeneration' % len(paths))
