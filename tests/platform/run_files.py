#!/usr/bin/env python3
"""Check native filesystem lookup and handle behavior against Mac 1.3."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--baseline')
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
rows = json.loads(a.compile_db.read_text())
entry = next(r for r in rows if r['file'].endswith('/src/PC/universal/com_files.c') and 'cod2_macos.dir' in r['command'])
source_root = entry['file'][:-len('/src/PC/universal/com_files.c')]
flags = entry.get('arguments') or shlex.split(entry['command'])
flags = [f.replace(source_root, str(ROOT)) for f in flags[:flags.index('-o')] if f != '-DNDEBUG']
flags += ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
          '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections',
          '-I' + str(out), '-iquote', str(ROOT / 'src/PC/universal')]
path = 'src/PC/universal/com_files.c'
data = subprocess.check_output(['git', 'show', f'{a.baseline}:{path}'], cwd=ROOT) if a.baseline else (ROOT / path).read_bytes()
(out / 'files_source.h').write_bytes(data)
for mode, compression in [('stored', zipfile.ZIP_STORED), ('deflated', zipfile.ZIP_DEFLATED)]:
    with zipfile.ZipFile(out / ('seek-' + mode + '.iwd'), 'w', compression=compression) as archive:
        archive.writestr('sample.bin', b'0123456789' * 900)
subprocess.run([*flags, str(ROOT / 'tests/platform/macos_files.c'),
                str(ROOT / 'src/PC/zlib/unzip.c'), '-lz', '-Wl,-dead_strip',
                '-o', str(out / 'files')], cwd=ROOT, check=True)
subprocess.run([str(out / 'files')], cwd=out, check=True)
