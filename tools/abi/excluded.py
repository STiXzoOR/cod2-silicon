#!/usr/bin/env python3
"""Inventory source files excluded from the actual native compile database.

These are NOT active definitions and must not be mixed with replacement APIs.
Attempt their ASTs with native flags; retain errors for unsupported SDK files.
Only symbol/type summaries are saved, never reference-binary contents.
"""
import argparse
from concurrent.futures import ProcessPoolExecutor
import hashlib
import json
from pathlib import Path
import shlex

from audit import summarize


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compile_commands', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    database = json.loads(options.compile_commands.read_text())
    included = {Path(e['file']).resolve() for e in database}
    excluded = [p for p in sorted((root / 'src').rglob('*.c')) if p.resolve() not in included]
    seed = next(e for e in database if e['file'].endswith('/common.c') and '-DDEDICATED' not in e['command'])
    entries = []
    for path in excluded:
        args = shlex.split(seed['command'])
        args = [str(path) if a == seed['file'] else a for a in args]
        # Exercise the three excluded CoD2x units as well as their empty default.
        if path.name.startswith('cod2x_'):
            args.insert(1, '-DCOD2_CODX=1')
        entries.append(dict(file=str(path), directory=seed['directory'], arguments=args))
    options.output.parent.mkdir(parents=True, exist_ok=True)
    cache = options.output.parent / 'excluded-cache'
    cache.mkdir(exist_ok=True)
    digest = hashlib.sha256()
    for p in sorted((root / 'src').rglob('*.h')):
        digest.update(p.read_bytes())
    results = []
    with ProcessPoolExecutor(max_workers=4) as pool:
        futures = [pool.submit(summarize, e, root, cache, digest.hexdigest()) for e in entries]
        for f in futures:
            results.append(f.result())
    report = {'excluded_sources': [str(p.relative_to(root)) for p in excluded],
              'note': 'Excluded definitions are not linked; native flags are only a syntax probe.',
              'results': results}
    options.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f'{len(excluded)} excluded sources; {sum("error" not in r for r in results)} parsed; '
          f'{sum("error" in r for r in results)} unsupported/error (see JSON)')


if __name__ == '__main__':
    main()
