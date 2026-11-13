#!/usr/bin/env python3
"""Check native filesystem handles and Windows case-insensitive path semantics."""
import argparse
import json
import os
import tempfile
from pathlib import Path
import shlex
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compile-db', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--baseline')
p.add_argument('--fixture-root', type=Path, help='Private fixture directory on the volume to check')
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
fixture = a.fixture_root.resolve() if a.fixture_root else out / 'fixture'
fixture.mkdir(parents=True, exist_ok=True)
fixture = Path(tempfile.mkdtemp(prefix='case-paths-', dir=fixture))
private_home = out / 'private-home'
private_home.mkdir(exist_ok=True)
fixture_env = dict(os.environ, HOME=str(private_home), CFFIXED_USER_HOME=str(private_home))
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
    with zipfile.ZipFile(fixture / ('seek-' + mode + '.iwd'), 'w', compression=compression) as archive:
        archive.writestr('sample.bin', b'0123456789' * 900)
# Keep the production close implementation while retaining the existing close-count check.
subprocess.run([*flags, '-DFS_FileClose=FS_RealFileClose', '-c',
                str(ROOT / 'src/PC/qcommon/com_fileaccess.c'),
                '-o', str(out / 'fileaccess.o')], cwd=ROOT, check=True)
subprocess.run([*flags, str(ROOT / 'tests/platform/macos_files.c'),
                str(ROOT / 'src/PC/zlib/unzip.c'), str(ROOT / 'src/Mac/Main/mac_common.c'),
                str(ROOT / 'src/PC/qcommon/files.c'), str(ROOT / 'src/PC/universal/com_shared.c'),
                str(ROOT / 'src/PC/qcommon/dl_main.c'), str(ROOT / 'src/Mac/Tools/MacFiles.c'),
                str(out / 'fileaccess.o'), '-lz', '-lcurl', '-framework', 'CoreFoundation',
                '-framework', 'CoreServices', '-Wl,-dead_strip',
                '-o', str(out / 'files')], cwd=ROOT, check=True)
# A single sentinel determines whether distinct case aliases can exist here.
probe = fixture / 'VolumeCaseProbe'
probe.write_text('probe')
sensitive = not (fixture / 'volumecaseprobe').exists()
probe.unlink()
base = fixture / 'MixedRoot' / 'MoD'
(base / 'SubDir').mkdir(parents=True, exist_ok=True)
(base / 'Packs').mkdir(exist_ok=True)
(base / 'Fork').mkdir(exist_ok=True)
for name, content in [('SubDir/Case.cfg', 'ORIGINAL'), ('SubDir/Blocked.str', 'PURE'),
                      ('Twin.cfg', 'UPPER'), ('Fork/Item.cfg', 'UPPER-DIR'), ('Café.cfg', 'UNICODE'),
                      ('Straße.cfg', 'SHARP-S'), ('Strasse.cfg', 'DOUBLE-S')]:
    (base / name).write_text(content)
(base / ('é' * 130 + '.cfg')).write_text('LONG-UNICODE')
if sensitive:
    (base / 'twin.cfg').write_text('lower-shadow')
    (base / 'fork').mkdir(exist_ok=True)
    (base / 'fork' / 'Item.cfg').write_text('lower-dir-shadow')
    (base / 'Packs' / 'case.iwd').write_bytes(b'shadow is not an archive')
for name, target in [('Link', 'SubDir'), ('SubDir/Cycle', '..')]:
    link = base / name
    if not link.is_symlink(): link.symlink_to(target, target_is_directory=True)
with zipfile.ZipFile(base / 'Packs' / 'Case.IWD', 'w') as archive:
    archive.writestr('SubDir/FromPack.cfg', b'ARCHIVE')
print('fixture directory=' + str(fixture), flush=True)
print('fixture filesystem case_sensitive=' + str(sensitive), flush=True)
subprocess.run([str(out / 'files')], cwd=fixture, env=fixture_env, check=True)
# Check physical names/content as well as the virtual view: no accidental aliases.
assert (base / 'Twin.cfg').read_text() == 'CANONICAL'
assert (base / 'SubDir' / 'CASE.CFG').read_text() == 'TEXT'
if sensitive:
    assert (base / 'twin.cfg').read_text() == 'lower-shadow'
    assert (base / 'fork' / 'Item.cfg').read_text() == 'lower-dir-shadow'
    assert not (fixture / 'mixedroot').exists()
    assert not (base / 'subdir').exists()
    assert not (base / 'SubDir' / 'Case.cfg').exists()
print('physical names and shadow contents: unchanged', flush=True)
