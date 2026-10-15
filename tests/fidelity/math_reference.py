#!/usr/bin/env python3
"""Compare bounded synthetic math cases against owned Mac 1.3 instructions.

Requires an actual native engine compile database and the audited private Mac
binary supported by tests/voice/mac_reference.py. No game is launched. Reports
and temporary files must stay outside the checkout. Unicorn is an optional,
already-provisioned dependency; this script never installs it.
"""
import argparse
import collections
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import random
import re
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path('src/PC/universal/com_math.c')
ORACLE_SOURCE = ROOT / 'tests/voice/mac_reference.py'
BACKGROUND = ['taskpolicy', '-b', 'nice', '-n', '19']
SEED = 0xC0D213
SYMBOLS = {
    'AngleNormalize360': '__Z17AngleNormalize360f',
    'AngleNormalize180': '__Z17AngleNormalize180f',
    'AngleSubtract': '__Z13AngleSubtractff',
    'LerpAngle': '__Z9LerpAnglefff',
}
PROBE = r'''
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern const float AngleNormalize360(const float);
extern const float AngleNormalize180(const float);
extern const float AngleSubtract(const float, const float);
extern const float LerpAngle(const float, const float, const float);

static float from_bits(uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

int main(void)
{
    unsigned int index, operation;
    uint32_t a, b, c;
    int fields;
    while ((fields = scanf("%u %u %" SCNx32 " %" SCNx32 " %" SCNx32,
                           &index, &operation, &a, &b, &c)) == 5) {
        float result;
        switch (operation) {
        case 0: result = AngleNormalize360(from_bits(a)); break;
        case 1: result = AngleNormalize180(from_bits(a)); break;
        case 2: result = AngleSubtract(from_bits(a), from_bits(b)); break;
        case 3: result = LerpAngle(from_bits(a), from_bits(b), from_bits(c)); break;
        default: return 2;
        }
        uint32_t bits;
        memcpy(&bits, &result, sizeof(bits));
        printf("{\"case\":%u,\"bits\":\"%08" PRIx32 "\"}\n", index, bits);
    }
    return fields == EOF && !ferror(stdin) && !ferror(stdout) ? 0 : 3;
}
'''


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def float_bits(value):
    return f'{struct.unpack("<I", struct.pack("<f", value))[0]:08x}'


def float_from_bits(value):
    if not isinstance(value, str) or re.fullmatch('[0-9a-f]{8}', value) is None:
        raise ValueError('Expected eight lowercase float32 hexadecimal digits')
    return struct.unpack('<f', struct.pack('<I', int(value, 16)))[0]


def float32(value):
    return float_from_bits(float_bits(value))


def neighbors(value):
    """Include the rounded value and its two finite float32 neighbors."""
    bits = int(float_bits(value), 16)
    candidates = [bits]
    if bits & 0x7fffffff:
        candidates += [bits - 1, bits + 1]
    else:
        candidates += [0x00000001, 0x80000001]
    return [float_from_bits(f'{bits:08x}') for bits in candidates]


def build_cases():
    rng = random.Random(SEED)
    cases, seen = [], set()

    def add(name, *values, scenario=None):
        values = tuple(float32(value) for value in values)
        if not all(math.isfinite(value) and abs(value) <= 1000000 for value in values):
            raise ValueError('Input exceeded the finite math fixture bound')
        if name == 'LerpAngle' and abs(values[2]) > 2:
            raise ValueError('Interpolation fraction exceeded the fixture bound')
        arguments = [float_bits(value) for value in values]
        key = (name, *arguments)
        if key not in seen:
            cases.append(dict(case=len(cases), function=name, arguments=arguments))
            if scenario:
                cases[-1]['scenario'] = scenario
            seen.add(key)

    add('LerpAngle', -180.0, float_from_bits('4333ffff'), 0.75,
        scenario='endpoint rounding at the positive half-turn boundary')
    angles = [0.0, -0.0, 2 ** -149, -(2 ** -149), 2 ** -126, -(2 ** -126)]
    for value in [-999999, -65536, -2880, -1080, -720, -540, -360, -180, -90,
                  -45, -1, -0.0000001, 0.0000001, 1, 45, 90, 180, 360, 540,
                  720, 1080, 2880, 65536, 999999]:
        angles.extend(neighbors(value))
    # Quantization transitions, both sides of zero, half turn and a full turn.
    for step in [-65537, -65536, -32769, -32768, -2, -1, 0, 1, 2,
                 32767, 32768, 65535, 65536, 65537]:
        angles.extend(neighbors(step * (360 / 65536)))
    angles += [rng.uniform(-8192, 8192) for _ in range(256)]
    angles += [rng.uniform(-1000000, 1000000) for _ in range(64)]
    for value in angles:
        add('AngleNormalize360', value)
        add('AngleNormalize180', value)

    anchors = [-720, -540, -360, -180, -0.0, 0.0, 180, 360, 540, 720]
    pairs = [(left, right) for left in anchors for right in anchors]
    for left in [-360, -180, -0.0, 0.0, 180, 360]:
        for delta in [-720, -540, -360, -180, 0, 180, 360, 540, 720]:
            pairs += [(left, value) for value in neighbors(float32(left + delta))]
    pairs += [(value, -value) for value in [2 ** -149, -(2 ** -149), 2 ** -126, -(2 ** -126)]]
    for left, right in pairs:
        add('AngleSubtract', left, right)
        for fraction in [-2, -1, -0.0, 0.0, 0.25, 0.5, 0.75, 1, 2]:
            add('LerpAngle', left, right, fraction)
    for bound, count in [(8192, 256), (65536, 64)]:
        for _ in range(count):
            left, right = rng.uniform(-bound, bound), rng.uniform(-bound, bound)
            add('AngleSubtract', left, right)
            add('LerpAngle', left, right, rng.uniform(-2, 2))
    return cases


def compare_observations(cases, native, original):
    expected = {case['case'] for case in cases}
    if not expected or len(expected) != len(cases):
        raise ValueError('Expected a nonempty set of distinct cases')

    def index(rows, label):
        result = {}
        for row in rows:
            case = row.get('case')
            if type(case) is not int or case not in expected or case in result:
                raise ValueError(f'{label}: duplicate or unexpected case identity')
            value = float_from_bits(row.get('bits'))
            if not math.isfinite(value):
                raise ValueError(f'{label}: nonfinite output for case {case}')
            result[case] = row['bits']
        if result.keys() != expected:
            raise ValueError(f'{label}: incomplete observations ({len(result)}/{len(expected)})')
        return result

    actual, reference = index(native, 'native'), index(original, 'original')
    rows = [dict(**case, native_bits=actual[case['case']], original_bits=reference[case['case']],
                 passed=actual[case['case']] == reference[case['case']]) for case in cases]
    mismatches = sum(not row['passed'] for row in rows)
    return dict(passed=mismatches == 0, mismatches=mismatches, compared=len(rows), observations=rows)


def engine_flags(database):
    entries = json.loads(database.read_text())
    selected = []
    for entry in entries:
        command = entry.get('arguments') or shlex.split(entry['command'])
        if (entry['file'].endswith('/' + str(SOURCE)) and
                'cod2_macos.dir/' in ' '.join(command)):
            selected.append((entry, command))
    if len(selected) != 1:
        raise ValueError(f'Expected one real native engine math compile entry, found {len(selected)}')
    entry, command = selected[0]
    old_root = entry['file'][:-len(str(SOURCE))].rstrip('/')
    command = [part.replace(old_root, str(ROOT)) for part in command]
    flags, skip = [], False
    for part in command:
        if skip:
            skip = False
        elif part in ['-o', '-MF', '-MT', '-MQ']:
            skip = True
        elif part not in ['-c', '-MD', '-MMD', '-MP', str(ROOT / SOURCE)]:
            flags.append(part)
    if ('-DCOD2_X64=1' not in flags or '-ffp-contract=off' not in flags or
            not any(flags[i:i + 2] == ['-arch', 'arm64'] for i in range(len(flags) - 1))):
        raise ValueError('Expected native arm64 engine flags with FP contraction disabled')
    if [part for part in flags if part.startswith('-ffp-contract=')][-1] != '-ffp-contract=off':
        raise ValueError('FP contraction flag was overridden')
    return flags, dict(entry=entry, remapped_flags=flags)


def compile_native(database, directory, sanitizers, metadata):
    flags, description = engine_flags(database)
    extra = ['-ffunction-sections', '-fdata-sections']
    sanitize = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                '-fno-omit-frame-pointer'] if sanitizers else []
    metadata.update(description)
    metadata['commands'] = []

    def run(command):
        command = BACKGROUND + command
        metadata['commands'].append(command)
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
        if result.returncode:
            raise RuntimeError(f'Native compilation failed ({result.returncode}): {result.stderr}')

    probe_source = directory / 'math_probe.c'
    probe_source.write_text(PROBE)
    obj, probe = directory / 'math.o', directory / 'math_probe'
    run([*flags, *extra, *sanitize, '-c', str(ROOT / SOURCE), '-o', str(obj)])
    run([*flags, *extra, *sanitize, str(probe_source), str(obj), '-Wl,-dead_strip', '-o', str(probe)])
    metadata.update(production_object_sha256=sha256(obj), probe_sha256=sha256(probe),
                    compiler_version=subprocess.check_output([flags[0], '--version'], text=True))
    return probe


def observe_native(probe, cases):
    operations = {name: index for index, name in enumerate(SYMBOLS)}
    lines = []
    for case in cases:
        args = (case['arguments'] + ['00000000'] * 3)[:3]
        lines.append(f"{case['case']} {operations[case['function']]} {' '.join(args)}")
    environment = dict(os.environ, UBSAN_OPTIONS='halt_on_error=1', ASAN_OPTIONS='halt_on_error=1')
    result = subprocess.run(BACKGROUND + [str(probe)], input='\n'.join(lines) + '\n',
                            capture_output=True, text=True, env=environment, timeout=60)
    if result.returncode:
        raise RuntimeError(f'Native probe failed ({result.returncode}): {result.stderr}')
    return [json.loads(line) for line in result.stdout.splitlines()]


def load_reference(binary, metadata):
    spec = importlib.util.spec_from_file_location('cod2_math_mac_reference', ORACLE_SOURCE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    reference = module.MacReference(binary)
    reference.self_test()
    # self_test deliberately exercises modeled codec imports, then clears its
    # counter. Snapshot here so they cannot count as math execution evidence.
    metadata.update(reference.metadata())
    baseline = reference.imports.copy()
    selected = {name: dict(symbol=symbol, address=reference.symbols.get(symbol))
                for name, symbol in SYMBOLS.items()}
    addresses = [row['address'] for row in selected.values()]
    if any(type(address) is not int or address <= 0 for address in addresses) or len(set(addresses)) != 4:
        raise ValueError('Original math symbols are missing or aliased')
    # The pinned binary hash identifies the exact symbol table and code image.
    # Also require nm to identify each entry as actual local/global text.
    text_symbols = {}
    for line in subprocess.check_output(['nm', str(binary)], text=True).splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[1] in ['t', 'T']:
            text_symbols[fields[2]] = int(fields[0], 16)
    if any(text_symbols.get(row['symbol']) != row['address'] for row in selected.values()):
        raise ValueError('Original math symbol identity was not confirmed in text')
    metadata.update(symbols=selected, imports_after_self_test=dict(baseline),
                    math_imports={}, imported_models_allowed=False)
    return reference, baseline


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-db', type=Path, required=True)
    parser.add_argument('--mac-binary', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='Private JSON report outside the checkout')
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args(argv)
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        parser.error('Keep binary-derived observations outside the checkout')
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        parser.error('--output must be a new private JSON report')
    report = dict(schema=1, complete=False, passed=False, sanitizers=args.sanitizers,
                  seed=SEED, comparison='Exact float32 bits, including signed zero; no tolerance',
                  native={}, original_oracle={}, cases=[], native_observations=[], original_observations=[],
                  scope='Four scalar angle functions, bounded finite synthetic inputs only; '
                        'no game, vector math, NaN/infinity, out-of-range C float-to-int conversion, '
                        'unbounded AngleSubtract loops, live gameplay or whole-engine fidelity claim',
                  bounds=dict(absolute_angle_max=1000000, angle_subtract_argument_max=65536,
                              lerp_fraction_absolute_max=2, normalization_scaled_absolute_max=182044464,
                              original_call_instruction_limit=3000000, original_call_timeout_us=10000000))
    try:
        if not __debug__:
            raise ValueError('Run without Python -O so the existing oracle self-test assertions execute')
        report['sources'] = {str(path.relative_to(ROOT)): sha256(path) for path in
                             [ROOT / SOURCE, Path(__file__).resolve(), ORACLE_SOURCE]}
        report['probe_source_sha256'] = hashlib.sha256(PROBE.encode()).hexdigest()
        report['compile_db'] = dict(path=str(args.compile_db.resolve()), sha256=sha256(args.compile_db))
        report['mac_binary'] = dict(path=str(args.mac_binary.resolve()), sha256=sha256(args.mac_binary))
        cases = build_cases()
        report['cases'] = cases
        report['case_counts'] = dict(collections.Counter(case['function'] for case in cases))
        reference, imports_baseline = load_reference(args.mac_binary.resolve(), report['original_oracle'])
        with tempfile.TemporaryDirectory(prefix='cod2-math-', dir=output.parent) as temporary:
            probe = compile_native(args.compile_db.resolve(), Path(temporary), args.sanitizers, report['native'])
            report['native_observations'] = observe_native(probe, cases)
            for case in cases:
                value = reference.call(SYMBOLS[case['function']],
                                       *(int(bits, 16) for bits in case['arguments']), floating=True)
                report['original_observations'].append(dict(case=case['case'], bits=float_bits(value)))
                delta = {name: count - imports_baseline.get(name, 0)
                         for name, count in reference.imports.items()
                         if count != imports_baseline.get(name, 0)}
                report['original_oracle']['math_imports'] = delta
                if delta:
                    raise RuntimeError('Math execution reached a modeled import; parity cannot be claimed')
            comparison = compare_observations(cases, report['native_observations'], report['original_observations'])
            report.update(comparison)
            report['complete'] = True
    except Exception as error:
        report['error'] = dict(type=type(error).__name__, message=str(error))
        report['passed'] = False
    output.write_text(json.dumps(report, indent=2, allow_nan=False) + '\n')
    if report['complete']:
        print(f"math original oracle: {report['compared'] - report['mismatches']}/{report['compared']} "
              f"bit-exact cases; private report {output}")
    else:
        print(f"math original oracle: incomplete ({report['error']['type']}); private report {output}")
    return 0 if report['complete'] and report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
