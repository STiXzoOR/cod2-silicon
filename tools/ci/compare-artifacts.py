#!/usr/bin/env python3
"""Compare complete object sets and both unstripped ELF binaries byte for byte."""
import hashlib
from pathlib import Path
import sys


def artifacts(root):
    objects = {p.relative_to(root): p for p in root.rglob('*.o')
               if p.parts[-2] != 'CompilerIdC'}
    # Compiler probes are not game objects.
    objects = {k: v for k, v in objects.items()
               if any(part.startswith(('cod2_objs.dir', 'cod2_linux.dir', 'cod2_lnxded.dir'))
                      for part in k.parts)}
    if not objects:
        raise ValueError(f'No game objects in {root}')
    for name in ('cod2_lnxded', 'cod2_linux'):
        path = root / name
        if path.read_bytes()[:5] != b'\x7fELF\x01':
            raise ValueError(f'Not a 32-bit ELF binary: {path}')
        objects[Path(name)] = path
    return objects


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    left, right = (artifacts(Path(p)) for p in sys.argv[1:])
    changed = [str(k) for k in sorted(left.keys() | right.keys())
               if k not in left or k not in right or digest(left[k]) != digest(right[k])]
    if changed:
        print('FAIL: differing or missing artifacts:\n' + '\n'.join(changed))
        return 1
    print(f'PASS: {len(left) - 2} objects and both unstripped 32-bit binaries are byte-identical')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        sys.exit(str(error))
