#!/usr/bin/env python3
"""Fail on any allocated section byte, relocation, or original symbol drift."""
import argparse
from pathlib import Path
from elf32 import ELF


def compare(original, generated):
    a, b = ELF(original), ELF(generated)
    ai, bi = a.image(), b.image()
    if ai.keys() != bi.keys():
        raise AssertionError('allocated sections differ: %s != %s' % (list(ai), list(bi)))
    for section, (data, relocs) in ai.items():
        other, other_relocs = bi[section]
        if data != other:
            offset = next((i for i, (x, y) in enumerate(zip(data, other)) if x != y), min(len(data), len(other)))
            raise AssertionError('%s: bytes differ at %#x (sizes %d / %d)' % (section, offset, len(data), len(other)))
        if relocs != other_relocs:
            offset = next(i for i in sorted(relocs.keys() | other_relocs.keys()) if relocs.get(i) != other_relocs.get(i))
            raise AssertionError('%s: relocation at %#x differs: %s / %s' % (section, offset, relocs.get(offset), other_relocs.get(offset)))
        print('%s: %d bytes and %d R_386_32 relocations identical' % (section, len(data), len(relocs)))
    original_names = {}
    for s in a.symbols:
        if s.name and 0 < s.section < len(a.sections) and a.sections[s.section].name in ai:
            original_names[s.name] = (a.sections[s.section].name, s.value, s.info >> 4)
    generated_names = {}
    for s in b.symbols:
        if s.name and 0 < s.section < len(b.sections) and b.sections[s.section].name in bi:
            generated_names[s.name] = (b.sections[s.section].name, s.value, s.info >> 4)
    for name, address in original_names.items():
        if generated_names.get(name) != address:
            raise AssertionError('symbol %s differs: %s / %s' % (name, address, generated_names.get(name)))
    print('%d original symbol addresses/bindings identical (including aliases)' % len(original_names))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original', type=Path)
    parser.add_argument('generated', type=Path)
    args = parser.parse_args()
    compare(args.original, args.generated)
