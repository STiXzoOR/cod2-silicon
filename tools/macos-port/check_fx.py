#!/usr/bin/env python3
"""Audit stock FX parsing with the headless fixture; retain licensed data outside git."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('main', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--parser', type=Path, required=True, dest='executable')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = args.output.expanduser().resolve()
    if output == root or root in output.parents:
        parser.error('licensed FX data must be outside the repository')
    output.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for archive in sorted(args.main.expanduser().glob('iw_*.iwd')):
        with zipfile.ZipFile(archive) as data:
            for name in data.namelist():
                key = name.lower()  # Engine IWD lookups are case insensitive.
                if not key.startswith('fx/') or not key.endswith('.efx'):
                    continue
                if '..' in Path(key).parts:
                    raise ValueError('invalid archive path')
                source = data.read(name)
                dest = output / key
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(source)
                manifest[key] = dict(archive=archive.name, sha256=hashlib.sha256(source).hexdigest())
    (output / 'effects.txt').write_text('\n'.join(sorted(manifest)) + '\n')
    result = subprocess.run([str(args.executable.resolve()), str(output)], capture_output=True, text=True, timeout=120)
    (output / 'parser.log').write_text(result.stdout + result.stderr)
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(result.stdout, end='')
    if result.returncode:
        raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
