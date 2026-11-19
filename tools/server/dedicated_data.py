#!/usr/bin/env python3
"""Make a local dedicated view of verified typed data, without retention roots.

Client imports and vtables must be removable by ld64's dead_strip. The client
and the i386 round-trip keep their original `used` annotations. No initializer,
type, alias or symbol changes here; live server references still fail to link if
their owner is missing. Outputs are private build artifacts, never snapshots.
"""
import argparse
import itertools
from pathlib import Path
import re


def dedicated_view(source):
    return source.replace('__attribute__((used, aligned(DG_ALIGN), section(',
                          '__attribute__((aligned(DG_ALIGN), section(')


def split_sections(source, separator):
    """--gc-sections drops input sections, unlike ld64's per-symbol dead_strip. ELF
    groups names by '.', PE by '$'; a PE image allows only so many output sections."""
    counter = itertools.count()
    return re.sub(r'section\((DG_[A-Z]+)\)',
                  lambda match: f'section({match.group(1)} "{separator}dg{next(counter)}")', source)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--split-sections', action='store_true')
    parser.add_argument('--section-separator', default='.')
    parser.add_argument('output', type=Path)
    parser.add_argument('sources', type=Path, nargs='+')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for source in args.sources:
        view = dedicated_view(source.read_text())
        (args.output / source.name).write_text(split_sections(view, args.section_separator)
                                               if args.split_sections else view)
    header = args.sources[0].parent / 'typed_types.h'
    (args.output / header.name).write_bytes(header.read_bytes())


if __name__ == '__main__':
    main()
