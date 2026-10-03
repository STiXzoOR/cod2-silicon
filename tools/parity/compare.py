#!/usr/bin/env python3
"""Fail closed on incomplete traces; report the first differing simulation frame."""
import argparse
from itertools import zip_longest
from pathlib import Path
import re
import sys

LINE = re.compile(r'frame (\d+) simt (-?\d+) ents (\d+) raw ([0-9a-fA-F]{8}) q ([0-9a-fA-F]{8})')


def read_trace(path):
    rows = []
    for line_no, line in enumerate(Path(path).read_text().splitlines(), 1):
        match = LINE.fullmatch(line)
        if not match:
            raise ValueError(f'{path}:{line_no}: malformed statehash record')
        frame, simt, ents, raw, quantized = match.groups()
        row = (int(frame), int(simt), int(ents), raw.lower(), quantized.lower())
        if row[0] <= 0 or (rows and row[0] != rows[-1][0] + 1):
            raise ValueError(f'{path}:{line_no}: noncontiguous frames or map restart')
        rows.append(row)
    if not rows:
        raise ValueError(f'{path}: empty trace (no local server simulation?)')
    return rows


def compare(left, right, raw=False):
    for index, (a, b) in enumerate(zip_longest(left, right)):
        if a is None or b is None or a[:3] != b[:3] or a[3 if raw else 4] != b[3 if raw else 4]:
            frame = a[0] if a else b[0]
            return f'FAIL: first difference at record {index + 1}, frame {frame}\nleft: {a}\nright: {b}'
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference')
    parser.add_argument('candidate')
    parser.add_argument('--raw', action='store_true', help='strict same-layout diagnostic; not cross-run parity')
    args = parser.parse_args()
    left, right = read_trace(args.reference), read_trace(args.candidate)
    error = compare(left, right, args.raw)
    print(error or f'PASS: {len(left)} frames match (frame, simt, active entities, {"raw" if args.raw else "q"})')
    return bool(error)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        sys.exit(str(error))
