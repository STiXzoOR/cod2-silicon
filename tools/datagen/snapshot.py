#!/usr/bin/env python3
"""Verify or explicitly refresh only the five committed production files."""
import argparse
from pathlib import Path
import shutil

FILES = ('data_native.c', 'literals_native.c', 'import_pointers_native.c',
         'bss_native.c', 'typed_types.h')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('generated', type=Path)
    parser.add_argument('snapshot', type=Path)
    parser.add_argument('--update', action='store_true')
    args = parser.parse_args()
    # Read everything first: missing output must not partially refresh a snapshot.
    contents = {name: (args.generated / name).read_bytes() for name in FILES}
    changed = [name for name, data in contents.items()
               if not (args.snapshot / name).is_file() or
               (args.snapshot / name).read_bytes() != data]
    if args.update:
        for name in changed:
            shutil.copyfile(args.generated / name, args.snapshot / name)
        print(f'Typed-data snapshot refreshed: {len(changed)} files changed')
    elif changed:
        # Invalidate the primary CMake output so a failed comparison is retried.
        (args.generated / 'data_native.c').unlink()
        raise SystemExit('Typed-data snapshot differs: ' + ', '.join(changed) +
                         '; review changes, then configure with '
                         '-DCOD2_UPDATE_TYPED_SNAPSHOT=ON to refresh')
    else:
        print('Typed-data snapshot verified: all five files byte-identical')


if __name__ == '__main__':
    main()
