#!/usr/bin/env python3
"""Make a local dedicated view of verified typed data, without retention roots.

Client imports and vtables must be removable by ld64's dead_strip. The client
and the i386 round-trip keep their original `used` annotations. No initializer,
type, alias or symbol changes here; live server references still fail to link if
their owner is missing. Outputs are private build artifacts, never snapshots.
"""
import argparse
from pathlib import Path


def dedicated_view(source):
    return source.replace('__attribute__((used, aligned(DG_ALIGN), section(',
                          '__attribute__((aligned(DG_ALIGN), section(')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('sources', type=Path, nargs='+')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for source in args.sources:
        (args.output / source.name).write_text(dedicated_view(source.read_text()))
    header = args.sources[0].parent / 'typed_types.h'
    (args.output / header.name).write_bytes(header.read_bytes())


if __name__ == '__main__':
    main()
