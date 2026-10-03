#!/usr/bin/env python3
"""Replay game/server CMake commands and count scoped diagnostics, including .inc files."""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DIRECTORIES = ('server_mp', 'game_mp', 'game', 'bgame', 'botlib')
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=ROOT / 'build-macos')
parser.add_argument('--logs', type=Path, help='count an existing replay directory without compiling')
args = parser.parse_args()
build = args.build.resolve()
output = args.logs or build / 'ws6-after'
output.mkdir(parents=True, exist_ok=True)

if not args.logs:
    entries = json.loads((build / 'compile_commands.json').read_text())
    entries = [entry for entry in entries
               if any('/src/PC/' + directory + '/' in entry['file'] for directory in DIRECTORIES)]

    def probe(entry):
        flags = shlex.split(entry['command'])
        flags = flags[:flags.index('-o')] + ['-fsyntax-only', '-fno-color-diagnostics', entry['file']]
        result = subprocess.run(flags, cwd=entry['directory'], capture_output=True, text=True)
        target = 'dedicated' if 'cod2_macos_ded.dir' in entry['command'] else 'client'
        log = output / (target + '__' + Path(entry['file']).name + '.log')
        log.write_text(result.stdout + result.stderr)
        return {'source': entry['file'], 'target': target, 'exit_code': result.returncode}

    with ThreadPoolExecutor(max_workers=12) as pool:
        results = list(pool.map(probe, entries))
    (output / 'manifest.json').write_text(json.dumps(results, indent=2) + '\n')
    failures = [entry for entry in results if entry['exit_code']]
    print(f'{len(results)} compile commands; {len(failures)} failures')
    if failures:
        print(failures)
        raise SystemExit(1)

diagnostics = set()
for log in output.glob('client__*.log'):
    for line in log.read_text().splitlines():
        if re.search(r':\d+:\d+: (warning|error):', line):
            diagnostics.add(line.replace(str(ROOT) + '/', ''))
counts = Counter()
categories = Counter()
for line in diagnostics:
    match = re.match(r'src/PC/(\w+)/', line)
    if match and match[1] in DIRECTORIES:
        counts[match[1]] += 1
        flag = re.search(r'\[(-W[^\]]+)\]$', line)
        categories[flag[1] if flag else 'unclassified'] += 1
for directory in DIRECTORIES:
    print(f'{directory}: {counts[directory]} unique client diagnostics')
print(f'total: {sum(counts.values())}')
for category, count in sorted(categories.items()):
    print(category, count)
(output / 'diagnostics.txt').write_text('\n'.join(sorted(diagnostics)) + '\n')
