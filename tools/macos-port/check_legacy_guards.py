#!/usr/bin/env python3
"""Compare modified C/header bodies with a baseline under inactive port guards.

Includes are removed so the Mac can preprocess Linux/Windows/wasm source bodies
without foreign SDKs. This checks guard discipline, not ABI or binary parity.
"""
import argparse
from pathlib import Path
import re
import subprocess

CONFIGURATIONS = {
    'linux-i386': ['__GNUC__=4', '__linux__=1', '__i386__=1'],
    'apple-i386-port-off': ['__GNUC__=4', '__APPLE__=1', '__APPLE_CC__=1', '__i386__=1'],
    'linux-x86_64-port-on': ['__GNUC__=4', '__linux__=1', '__x86_64__=1', 'COD2_X64=1'],
    'mingw-i386': ['__GNUC__=4', '_WIN32=1', '__i386__=1'],
    'wasm': ['__GNUC__=4', '__clang__=1', '__EMSCRIPTEN__=1', '__wasm__=1'],
    'clang-i386': ['__GNUC__=4', '__clang__=1', '__i386__=1'],
}


def body(source, defines):
    source = re.sub(r'^\s*#\s*include\b[^\n]*', '', source, flags=re.M)
    result = subprocess.run(
        ['clang', '-E', '-P', '-x', 'c', '-undef', '-DCOD2_APPLE_SDK=0',
         '-D__LINE__=1', '-D__DATE__="Jan  1 2000"', '-D__TIME__="00:00:00"',
         '-DSDL_VERSION_ATLEAST(x,y,z)=1', '-Wno-builtin-macro-redefined',
         *('-D' + value for value in defines), '-'],
        input=source, capture_output=True, text=True, check=True)
    return re.sub(r'\s+', ' ', result.stdout).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', required=True, help='the pre-port commit in this checkout')
    args = parser.parse_args()
    paths = subprocess.check_output(['git', 'diff', '--name-only', args.base], text=True).splitlines()
    paths = [p for p in paths if p.endswith(('.c', '.h'))]
    failures = []
    for path in paths:
        original = subprocess.check_output(['git', 'show', args.base + ':' + path], text=True)
        current = Path(path).read_text()
        for name, defines in CONFIGURATIONS.items():
            if body(original, defines) != body(current, defines):
                failures.append((path, name))
    print(f'{len(paths)} files; {len(CONFIGURATIONS)} inactive configurations; {len(failures)} mismatches')
    for path, configuration in failures:
        print(path, configuration)
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
