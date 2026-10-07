#!/usr/bin/env python3
"""Exercise production Speex with independent synthetic packet/PCM vectors."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = Path(__file__).with_name('fixtures.json')
BACKGROUND = ['taskpolicy', '-b', 'nice', '-n', '19']


def compile_native(database, output, sanitizers):
    rows = json.loads(database.read_text())
    entries = [row for row in rows if '/src/PC/speex/' in row['file'] and
               'cod2_macos.dir/' in row.get('command', '')]
    if len(entries) != 15:
        raise ValueError(f'Expected 15 real engine Speex units, found {len(entries)}')
    objects = []
    probe_flags = None
    for entry in entries:
        old_root = entry['file'].split('/src/PC/speex/')[0]
        command = shlex.split(entry['command'])
        flags = [flag.replace(old_root, str(ROOT)) for flag in command[:command.index('-o')]]
        flags += ['-UNDEBUG']
        if '-DCOD2_X64=1' not in flags or '-ffp-contract=off' not in flags:
            raise ValueError('Expected native engine flags with FP contraction disabled')
        if sanitizers:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
        source = ROOT / 'src/PC/speex' / Path(entry['file']).name
        obj = output / (source.name + '.o')
        subprocess.run(BACKGROUND + flags + ['-fPIC', '-c', str(source), '-o', str(obj)], check=True,
                       capture_output=True, text=True)
        objects.append(str(obj))
        probe_flags = flags
    library = output / 'native.dylib'
    link = [probe_flags[0], '-dynamiclib', '-arch', 'arm64']
    if sanitizers:
        link += ['-fsanitize=address,undefined']
    subprocess.run(BACKGROUND + link + objects + ['-o', str(library)], check=True, capture_output=True, text=True)
    probe = output / 'codec_probe'
    subprocess.run(BACKGROUND + probe_flags + [str(ROOT / 'tests/voice/codec_probe.c'), '-o', str(probe)],
                   check=True, capture_output=True, text=True)
    return probe, library


def observe(probe, library, operation, option, packets=None):
    result = subprocess.run([str(probe), str(library), operation, str(option)],
                            input=None if packets is None else '\n'.join(packets) + '\n',
                            text=True, capture_output=True, check=True)
    rows = [json.loads(line) for line in result.stdout.splitlines()]
    return rows[0], rows[1:]


def differences(actual, expected):
    residuals = [a - b for frame, reference in zip(actual, expected)
                 for a, b in zip(frame['pcm'], reference['pcm'])]
    return dict(max_abs=max(map(abs, residuals), default=0),
                rmse=math.sqrt(sum(value * value for value in residuals) / max(1, len(residuals))),
                status_mismatches=sum((frame['result'], frame['remaining']) !=
                                      (reference['result'], reference['remaining'])
                                      for frame, reference in zip(actual, expected)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-db', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path, help='Private JSON report outside the repository')
    parser.add_argument('--sanitizers', action='store_true')
    parser.add_argument('--mac-binary', type=Path, help='Optional original codec oracle; requires task-local Unicorn')
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error('Keep binary-derived observations outside Git')
    fixture = json.loads(FIXTURES.read_text())
    checks, original = [], []
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='cod2-voice-') as directory:
        probe, library = compile_native(args.compile_db.resolve(), Path(directory), args.sanitizers)
        metadata = None
        for packet_group in fixture['packets']:
            metadata, actual = observe(probe, library, 'encode', packet_group['mode'])
            identical = sum(a['packet'] == b['packet'] and a['bits'] == b['bits']
                            for a, b in zip(actual, packet_group['frames']))
            checks.append(dict(name=f"encode mode {packet_group['mode']}", passed=identical == 8,
                               identical_packets=identical, total=8))
        for case in fixture['decode']:
            _, actual = observe(probe, library, 'decode', case['enhancement'], case['sequence'])
            error = differences(actual, case['expected'])
            # Xiph 1.0.4 is an independent regression oracle. This tolerance
            # permits scalar float rounding, and catches either missing loop.
            passed = error['max_abs'] <= 0.25 and error['status_mismatches'] == 0 and len(actual) == 8
            checks.append(dict(name=f"PCM mode {case['mode']} enhancement {case['enhancement']} loss {case['loss']}",
                               passed=passed, **error))
        for packet_group in fixture['modern_packets']:
            sequence = [frame['packet'] for frame in packet_group['frames']]
            _, actual = observe(probe, library, 'decode', 1, sequence)
            bits = [frame['bits'] for frame in packet_group['frames']]
            passed = len(actual) == 8 and all(frame['result'] == 0 and
                                             frame['remaining'] == (-count) % 8
                                             for frame, count in zip(actual, bits))
            checks.append(dict(name=f"Speex 1.2.1 packets mode {packet_group['mode']}", passed=passed))
        for case in fixture['framing_cases']:
            _, actual = observe(probe, library, 'decode', 1, [case['packet']])
            passed = actual[0]['result'] == case['result']
            if 'remaining' in case:
                passed &= actual[0]['remaining'] == case['remaining']
            checks.append(dict(name=f"framing {case['packet'] or 'empty'}", passed=passed,
                               actual_result=actual[0]['result'], expected_result=case['result']))
        _, saturated = observe(probe, library, 'decode', 1, fixture['saturation_sequence'])
        samples = [sample for frame in saturated for sample in frame['pcm']]
        saturation = max(map(abs, samples))
        checks.append(dict(name='PCM saturation', passed=saturation == 32767 and min(samples) == -32767,
                           peak=saturation))
        for name, packet in [('loss', '-'), ('null mode', '00')]:
            _, frames = observe(probe, library, 'decode', 1, fixture['saturation_sequence'] + [packet])
            frame = frames[-1]
            peak = max(map(abs, frame['pcm']))
            checks.append(dict(name=f'PCM {name} saturation',
                               passed=frame['result'] == 0 and peak == 32767 and min(frame['pcm']) == -32767,
                               peak=peak))
        _, first_loss = observe(probe, library, 'decode', 0, ['-'])
        loss_peak = max(abs(sample) for sample in first_loss[0]['pcm'])
        checks.append(dict(name='initial loss noise floor', passed=0.05 < loss_peak < 1.0, peak=loss_peak))
        oracle_metadata = None
        if args.mac_binary:
            from mac_reference import MacReference
            reference = MacReference(args.mac_binary)
            reference.self_test()
            for case in fixture['decode']:
                _, actual = observe(probe, library, 'decode', case['enhancement'], case['sequence'])
                expected = reference.decode(case['sequence'], case['enhancement'])
                error = differences(actual, expected)
                original.append(dict(mode=case['mode'], enhancement=case['enhancement'], loss=case['loss'], **error))
            for case in fixture['framing_cases']:
                expected = reference.decode([case['packet']], 1)[0]
                assert expected['result'] == case['result'], case
            expected = reference.decode(fixture['saturation_sequence'], 1)
            assert max(abs(sample) for frame in expected for sample in frame['pcm']) == 32767
            for packet in ['-', '00']:
                expected = reference.decode(fixture['saturation_sequence'] + [packet], 1)[-1]
                assert expected['result'] == 0 and max(map(abs, expected['pcm'])) == 32767
            expected = reference.decode(['-'], 0)
            assert 0.05 < max(map(abs, expected[0]['pcm'])) < 1.0
            oracle_metadata = reference.metadata()
        result = dict(native=metadata, sanitizers=args.sanitizers, compile_db=str(args.compile_db.resolve()),
                      fixture_sha256=hashlib.sha256(FIXTURES.read_bytes()).hexdigest(), checks=checks,
                      failures=sum(not check['passed'] for check in checks),
                      original_oracle=oracle_metadata, original_pcm=original,
                      original_pcm_bit_parity=False,
                      scope='Narrowband synthetic codec vectors; no capture, live audio, network or game run')
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    failures = [check for check in checks if not check['passed']]
    for check in failures:
        print('FAIL', check)
    print(f"voice codec: {len(checks) - len(failures)}/{len(checks)} checks passed; report {args.output}")
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
