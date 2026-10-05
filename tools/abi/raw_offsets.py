#!/usr/bin/env python3
"""Gate literal byte offsets/strides in the actual native compile source set.

Clang's preprocessor selects active source lines using each database entry's
flags (including CoD2x). Scan their original spelling, so macro expansion cannot
hide a literal. The reviewed manifest keys exact source text, not line numbers;
new occurrences also fail. This is a conservative source checker, not a proof
that arbitrary pointer arithmetic is valid. See the WS38 report for its scope.
"""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = Path(__file__).with_name('raw-offsets.json')
NUMBER = r'(?:0[xX][0-9a-fA-F]+|[0-9]+)'
LEX = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
CAST = r'\(\s*(?:const\s+)?(?:unsigned\s+)?(?:byte|char|uint8_t|void)\s*\*\s*\)'
PATTERNS = [
    re.compile(CAST + r'.*?(?:[+-]\s*' + NUMBER + r'|\*\s*' + NUMBER + r')'),
    re.compile(r'\*\s*\([^;]*?\*\s*\)\s*\([^;]*?[+-]\s*' + NUMBER),
    re.compile(r'\b\w+\s*\*\s*0[xX][0-9a-fA-F]+\b'),
    re.compile(r'\b\w+\s*\*\s*(?:41|140|164|276|9992)\b'),
    re.compile(r'^\s*#\s*define\s+\w*(?:STRIDE|OFFSET|OFF_)\w*\s+\(?\s*' + NUMBER),
    re.compile(r'\bArchive(?:Int|Byte|Vec3)\s*\([^;]*?,\s*' + NUMBER + r'\s*\)'),
    re.compile(r'\b(?:malloc|Z_MallocInternal|Hunk_AllocInternal)\s*\(\s*' + NUMBER + r'\s*\)'),
    re.compile(r'\b(?:memcpy|memset|Com_Memcpy|Com_Memset|qsort|malloc|calloc|Z_MallocInternal|Hunk_AllocInternal)\s*\([^;]*?(?:,\s*' + NUMBER + r'\s*\)|\(\s*' + NUMBER + r'\s*\)|\*\s*' + NUMBER + r'\s*\))'),
]


def scrub(source):
    return LEX.sub(lambda m: '\n' * m[0].count('\n') if m[0].startswith(('/', '"', "'")) else m[0], source)


def candidates(source):
    clean = scrub(source)
    # Also follow byte-buffer names, e.g. entry + 0x13f4 with no cast.
    buffers = set(re.findall(r'\b(?:byte|char|uint8_t)\s*\*\s*(\w+)', clean))
    buffer_pattern = re.compile(r'\b(?:' + '|'.join(map(re.escape, sorted(buffers))) + r')\s*[+-]\s*' + NUMBER) if buffers else None
    original = source.splitlines()
    return {i: original[i - 1].strip() for i, line in enumerate(clean.splitlines(), 1)
            if any(p.search(line) for p in PATTERNS) or (buffer_pattern and buffer_pattern.search(line))}


def active_lines(entry, source_text=None):
    args = entry.get('arguments') or shlex.split(entry['command'])
    command = []
    skip = False
    for arg in args:
        if skip:
            skip = False
        elif arg in ('-o', '-MF', '-MT', '-MQ'):
            skip = True
        elif arg not in ('-c', '-MD', '-MMD'):
            command.append(arg)
    source = Path(entry['file']).resolve()
    if source_text is not None:
        command = [arg for arg in command if arg != entry['file']]
        language = {'.m': 'objective-c', '.mm': 'objective-c++', '.cpp': 'c++'}.get(source.suffix, 'c')
        command += ['-iquote', str(source.parent), '-x', language, '-']
        source_text = f'#line 1 "{source}"\n' + source_text
    proc = subprocess.run(command + ['-E', '-dD', '-w'], input=source_text,
                          cwd=entry['directory'], capture_output=True, text=True, errors='replace')
    if proc.returncode:
        raise RuntimeError(f"Preprocessing failed: {entry['file']}\n{proc.stderr}")
    selected, file, number = set(), None, 0
    for line in proc.stdout.splitlines():
        marker = re.match(r'# (\d+) "([^"]+)"', line)
        if marker:
            number = int(marker[1])
            file = Path(marker[2]).resolve() if not marker[2].startswith('<') else None
        else:
            if file == source and line.strip():
                selected.add(number)
            number += 1
    return source, selected


def inventory(database, base=None):
    entries = json.loads(Path(database).read_text())
    entries = {entry['file']: entry for entry in entries
               if 'cod2_macos.dir' in (entry.get('command') or ' '.join(entry['arguments']))}
    if not entries:
        raise RuntimeError('No cod2_macos sources in compile database')
    sites = []
    def scan(entry):
        path = Path(entry['file']).resolve()
        source = path.read_text(errors='replace')
        if base:
            relative = path.relative_to(ROOT)
            old = subprocess.run(['git', 'show', f'{base}:{relative}'], cwd=ROOT,
                                 capture_output=True, text=True, errors='replace')
            if old.returncode:
                # CMake-generated TUs are identical inputs for this source-only comparison.
                if not str(relative).startswith('build'):
                    raise RuntimeError(f'Missing baseline source: {relative}')
            else:
                source = old.stdout
        _, active = active_lines(entry, source if base else None)
        return path, source, active
    with ThreadPoolExecutor(max_workers=3) as pool:
        for path, source, active in pool.map(scan, entries.values()):
            for line, text in candidates(source).items():
                if line in active:
                    sites.append(dict(file=str(path.relative_to(ROOT)), line=line, text=text))
    return dict(sources=len(entries), sites=sorted(sites, key=lambda s: (s['file'], s['line'])))


def check(result, manifest):
    allowed = {(s['file'], s['text']): s for s in manifest}
    counts = Counter((s['file'], s['text']) for s in result['sites'])
    failures = []
    for key, count in counts.items():
        row = allowed.get(key)
        if not row or count > row['count'] or row['classification'] not in ('safe', 'dead/unreachable') or not row['reason']:
            failures.append(dict(file=key[0], text=key[1], count=count))
    return failures


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('database', type=Path)
    p.add_argument('--output', type=Path)
    p.add_argument('--inventory-only', action='store_true', help='Do not gate; print the unreviewed inventory')
    p.add_argument('--base', help='Inventory historical sources using current unchanged headers/flags; not an artifact comparison')
    a = p.parse_args()
    result = inventory(a.database, a.base)
    if a.output:
        a.output.parent.mkdir(parents=True, exist_ok=True)
        a.output.write_text(json.dumps(result, indent=2) + '\n')
    if a.inventory_only:
        print(json.dumps(result, indent=2))
        return 0
    failures = check(result, json.loads(MANIFEST.read_text()))
    print(f"Raw offsets: {result['sources']} sources, {len(result['sites'])} sites, {len(failures)} unreviewed/unsafe")
    for row in failures:
        print(f"{row['file']}: {row['text']} ({row['count']} occurrences)")
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
