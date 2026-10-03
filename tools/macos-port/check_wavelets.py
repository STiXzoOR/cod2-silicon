#!/usr/bin/env python3
"""Decode every stock wavelet image with the ASan fixture; keep assets outside git."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('main', type=Path, help='user-owned CoD2 main directory')
    parser.add_argument('output', type=Path)
    parser.add_argument('--decoder', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.expanduser().resolve()
    root = Path(__file__).resolve().parents[2]
    if output == root or root in output.parents:
        parser.error('licensed inputs and decoded output must be outside the repository')
    output.mkdir(parents=True, exist_ok=True)
    entries = {}
    for archive in sorted(args.main.expanduser().glob('iw_*.iwd')):
        with zipfile.ZipFile(archive) as data:
            for name in data.namelist():
                if not name.lower().endswith('.iwi'):
                    continue
                with data.open(name) as image:
                    header = image.read(28)
                if len(header) == 28 and header[:4] == b'IWi\x05' and header[4] in (6, 7):
                    entries[name] = (archive, data.read(name))
    manifest = {}
    for index, (name, (archive, image)) in enumerate(sorted(entries.items())):
        source = output / f'{index:02d}.iwi'
        decoded = output / f'{index:02d}.bgra-mips'
        source.write_bytes(image)
        result = subprocess.run([str(args.decoder.resolve()), str(source), str(decoded)],
                                capture_output=True, text=True, timeout=30)
        if result.returncode:
            sys.stderr.write(result.stderr)
            raise RuntimeError(f'decoder failed for {name}: {result.returncode}')
        print(name + ': ' + result.stdout.split(': ', 1)[1].strip())
        manifest[name] = dict(archive=archive.name, input_sha256=hashlib.sha256(image).hexdigest(),
                              decoded_sha256=hashlib.sha256(decoded.read_bytes()).hexdigest(),
                              decoder=result.stdout.strip())
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Validated {len(manifest)} wavelet images')


if __name__ == '__main__':
    main()
