#!/usr/bin/env python3
"""Build the network-parser fuzz targets from a native compile database.

Each target compiles the real engine translation units with the flags CMake
recorded for them, adds ASan/UBSan and SanitizerCoverage, and links them with
a harness under tests/fuzz/targets plus the portable driver (tests/fuzz/driver.c).
Dead stripping drops everything the harness does not reach, so a harness only
stubs what its parser path really calls. With --libfuzzer the targets link
against a real libFuzzer runtime (clang -fsanitize=fuzzer) instead.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
HERE = ROOT / 'tests/fuzz'

# flavor: 'ded' uses the cod2_macos_ded (DEDICATED) commands, 'client' cod2_macos.
TARGETS = {
    'msg': {
        'flavor': 'client',
        'sources': [
            'src/PC/qcommon/msg_mp.c', 'src/PC/qcommon/huffman.c',
            'src/PC/universal/q_shared.c', 'src/PC/universal/com_shared.c',
        ],
        'max_len': 70000,
    },
}

SANITIZERS = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
              # The wire format is unaligned by design and arm64 loads tolerate it;
              # `-1 << bits` sign extension is the engine-wide idiom for signed fields.
              '-fno-sanitize=alignment,shift-base',
              '-fno-omit-frame-pointer']
COVERAGE = ['-fsanitize-coverage=inline-8bit-counters,trace-cmp']


def engine_flags(database, flavor):
    marker = 'cod2_macos_ded.dir' if flavor == 'ded' else 'cod2_macos.dir'
    for entry in database:
        command = entry.get('command') or shlex.join(entry['arguments'])
        if marker in command and entry['file'].endswith('/src/PC/qcommon/msg_mp.c'):
            argv = shlex.split(command)
            flags = argv[1:argv.index('-o')]
            flags = ['-UNDEBUG' if f == '-DNDEBUG' else f for f in flags]
            flags = ['-O1' if f.startswith('-O') else f for f in flags]
            return argv[0], flags
    raise SystemExit(f'{marker} commands for msg_mp.c are missing from the database')


def run(command, log):
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    log.write_text(' '.join(shlex.quote(c) for c in command) + '\n' + result.stdout + result.stderr)
    if result.returncode:
        sys.stderr.write(result.stdout + result.stderr)
        raise SystemExit(f'failed: {log}')


def build_target(name, spec, database, out, libfuzzer, jobs):
    compiler, flags = engine_flags(database, spec['flavor'])
    codx = '-DCOD2_CODX=1' in flags
    sources = list(spec['sources']) + (spec.get('codx_sources', []) if codx else [])
    objdir = out / 'obj' / name
    objdir.mkdir(parents=True, exist_ok=True)
    instrument = COVERAGE if not libfuzzer else ['-fsanitize=fuzzer-no-link']
    common = [*flags, *SANITIZERS, '-ffunction-sections', '-fdata-sections']

    def compile_engine(source):
        obj = objdir / (Path(source).stem + '.o')
        run([compiler, *common, *instrument, '-c', str(ROOT / source), '-o', str(obj)],
            obj.with_suffix('.log'))
        return obj

    with ThreadPoolExecutor(max_workers=jobs) as pool:
        objects = list(pool.map(compile_engine, sources))
    harness = objdir / 'harness.o'
    run([compiler, *common, *instrument, '-I' + str(HERE), '-Wno-unused-parameter',
         '-c', str(HERE / 'targets' / (name + '.c')), '-o', str(harness)],
        objdir / 'harness.log')
    objects.append(harness)
    link = [compiler, '-arch', 'arm64', *SANITIZERS, '-Wl,-dead_strip']
    if libfuzzer:
        link.append('-fsanitize=fuzzer')
    else:
        driver = objdir / 'driver.o'
        run([compiler, '-arch', 'arm64', '-O1', '-g', '-std=gnu99', '-Wall', '-Wextra',
             *SANITIZERS, '-c', str(HERE / 'driver.c'), '-o', str(driver)],
            objdir / 'driver.log')
        objects.append(driver)
    exe = out / name
    run([*link, *map(str, objects), '-o', str(exe)], objdir / 'link.log')
    (out / (name + '.max_len')).write_text(str(spec['max_len']) + '\n')
    return exe


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build-macos',
                        help='CMake build directory with compile_commands.json')
    parser.add_argument('--out', type=Path, help='output directory (default BUILD/fuzz)')
    parser.add_argument('--target', action='append', choices=sorted(TARGETS),
                        help='build only this target (repeatable)')
    parser.add_argument('--libfuzzer', action='store_true',
                        help='link with clang -fsanitize=fuzzer instead of driver.c')
    parser.add_argument('--jobs', type=int, default=3)
    args = parser.parse_args()
    build = args.build.resolve()
    database_path = build / 'compile_commands.engine.json'
    if not database_path.exists():
        database_path = build / 'compile_commands.json'
    database = json.loads(database_path.read_text())
    out = (args.out or build / 'fuzz').resolve()
    out.mkdir(parents=True, exist_ok=True)
    for name in args.target or sorted(TARGETS):
        exe = build_target(name, TARGETS[name], database, out, args.libfuzzer, args.jobs)
        print(exe)


if __name__ == '__main__':
    main()
