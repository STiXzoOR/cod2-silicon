#!/usr/bin/env python3
"""Replay CMake C commands as syntax probes, preserving per-TU diagnostics.

The full CMake build remains the object/assembler/link check. These probes avoid
interleaved parallel make output and retain diagnostics for the LP64 inventory.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=Path('build-macos'))
    parser.add_argument('--jobs', type=int, default=12)
    args = parser.parse_args()
    root = Path.cwd()
    build = args.build.resolve()
    commands = json.loads((build / 'compile_commands.json').read_text())
    destination = build / 'lp64-probes'

    def probe(entry):
        argv = shlex.split(entry['command'])
        target = 'dedicated' if 'cod2_macos_ded.dir' in entry['command'] else 'client'
        # CMake emits -o OBJECT -c SOURCE last. Keep all actual compile flags.
        argv = argv[:argv.index('-o')] + ['-fsyntax-only', '-fno-color-diagnostics', entry['file']]
        result = subprocess.run(argv, cwd=entry['directory'], capture_output=True, text=True)
        source = str(Path(entry['file']).relative_to(root))
        output = destination / target / (source.replace('/', '__') + '.log')
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(result.stdout + result.stderr)
        return {'target': target, 'source': source, 'exit_code': result.returncode,
                'command': argv, 'log': str(output.relative_to(build))}

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(probe, commands))
    (destination / 'manifest.json').write_text(json.dumps(results, indent=2) + '\n')
    failures = [r for r in results if r['exit_code']]
    print(f'{len(results)} translation units; {len(failures)} failures; logs: {destination}')
    for entry in failures:
        print(entry['target'], entry['source'], entry['exit_code'])
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
