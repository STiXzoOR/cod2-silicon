#!/usr/bin/env python3
"""Run production voice initialization and client reception into the native mixer."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/audio'))
from output import extract_function

BACKGROUND = ['taskpolicy', '-b', 'nice', '-n', '19']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-db', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--baseline', help='Read production source from this local commit')
    parser.add_argument('--rate-contract', action='store_true', help='Measure synthetic voice duration and pitch')
    args = parser.parse_args()
    out = args.output.resolve()
    if out.is_relative_to(ROOT):
        parser.error('Keep evidence outside the repository')
    out.mkdir(parents=True, exist_ok=False)
    rows = json.loads(args.compile_db.read_text())
    packet = json.loads((ROOT / 'tests/voice/fixtures.json').read_text())['packets'][4]['frames'][0]['packet']

    def source(path):
        if args.baseline:
            return subprocess.check_output(['git', 'show', args.baseline + ':' + path], cwd=ROOT, text=True)
        return (ROOT / path).read_text()

    def flags(path):
        row = next(row for row in rows if row['file'].endswith('/' + path)
                   and 'cod2_macos.dir/' in row.get('command', ' '.join(row.get('arguments', []))))
        old_root = row['file'][:-len('/' + path)]
        command = row.get('arguments') or shlex.split(row['command'])
        result = [word.replace(old_root, str(ROOT)) for word in command[:command.index('-o')]
                  if word not in ('-DNDEBUG', '-c')]
        assert '-DCOD2_X64=1' in result and '-ffp-contract=off' in result
        return result + ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                         '-fno-omit-frame-pointer']

    commands = []

    def run(command, name):
        commands.append(shlex.join(command))
        result = subprocess.run(BACKGROUND + command, cwd=ROOT, capture_output=True, text=True)
        (out / (name + '.log')).write_text(result.stdout + result.stderr)
        if result.returncode:
            print(result.stdout + result.stderr)
            result.check_returncode()

    with tempfile.TemporaryDirectory(prefix='build-', dir=out) as temporary:
        build = Path(temporary)
        client = source('src/PC/client_mp/cl_main_mp.c')
        inputs = source('src/PC/client_mp/cl_input.c')
        (build / 'client_receive.h').write_text(extract_function(client, 'CL_VoicePacket') + '\n' +
            '\n'.join(extract_function(inputs, name) for name in
                      ('IN_KeyDown', 'IN_KeyUp', 'IN_TalkDown', 'IN_TalkUp', 'IsTalking')))
        paths = ['src/PC/win32/win_voice.c', 'src/platform/macos_voice.c',
                 'src/PC/groupvoice/play.c', 'src/PC/groupvoice/encode.c', 'src/PC/groupvoice/decode.c',
                 'src/PC/qcommon/msg_mp.c']
        paths += [str(Path(row['file']).relative_to(Path(row['file'].split('/src/')[0]))) for row in rows
                  if '/src/PC/speex/' in row['file'] and
                  'cod2_macos.dir/' in row.get('command', ' '.join(row.get('arguments', [])))]
        objects = []
        for index, path in enumerate(paths):
            unit = build / Path(path).name
            unit.write_text(source(path))
            extra = []
            if path == 'src/platform/macos_voice.c':
                names = ['Record_Init', 'Record_NewSample', 'Record_Start', 'Record_Stop', 'Record_Frame',
                         'Record_Shutdown', 'mixerGetRecordSource', 'mixerSetRecordSource',
                         'mixerGetRecordLevel', 'mixerSetRecordLevel', 'mixerSetMicrophoneMute']
                extra = ['-D' + name + '=Native_' + name for name in names]
            obj = build / (str(index) + '.o')
            run(flags(path) + extra + ['-iquote', str((ROOT / path).parent), '-c', str(unit), '-o', str(obj)],
                'compile-' + Path(path).stem)
            objects.append(str(obj))
        for path, engine_flags, extra in [
            ('tests/voice/rate_render.c' if args.rate_contract else 'tests/audio/render.c',
             'src/platform/macos_audio.c',
             ['-DAIL_open_digital_driver=VoiceTest_UnusedOpenDigitalDriver']),
            ('tests/voice/playback_rate.c' if args.rate_contract else 'tests/voice/playback_init.c',
             'src/PC/win32/win_voice.c', ['-I' + str(build)])
        ]:
            obj = build / (Path(path).stem + '.o')
            run(flags(engine_flags) + extra + ['-c', str(ROOT / path), '-o', str(obj)], 'compile-' + Path(path).stem)
            objects.append(str(obj))
        exe = build / 'playback-init'
        run([flags(paths[0])[0], '-arch', 'arm64', '-fsanitize=address,undefined', '-Wl,-dead_strip',
             *objects, '-framework', 'AudioToolbox', '-framework', 'CoreAudio', '-o', str(exe)], 'link')
        notice = 'Native microphone capture is unavailable; voice recording disabled.'
        observations = []
        passed = True
        cases = ['default', 'zero', 'negative'] if args.rate_contract else ['init']
        if args.rate_contract:
            tone = json.loads((ROOT / 'tests/voice/tone_packets.json').read_text())
            (build / 'packets.txt').write_text('\n'.join(tone['packets']) + '\n')
        for case in cases:
            command = [str(exe), str(build / 'packets.txt'), case] if args.rate_contract else [str(exe), packet]
            commands.append(shlex.join(command))
            result = subprocess.run(BACKGROUND + command, cwd=build, capture_output=True, text=True)
            log_name = 'run-' + case if args.rate_contract else 'run'
            (out / (log_name + '.log')).write_text(result.stdout + result.stderr)
            passed &= result.returncode == 0 and result.stderr.count(notice) == 1
            observations.append(dict(case=case, returncode=result.returncode,
                                     notice_count=result.stderr.count(notice), stdout=result.stdout))
            print(result.stdout + result.stderr)
        (out / 'result.json').write_text(json.dumps(dict(
            passed=passed, returncode=result.returncode, notice_count=result.stderr.count(notice),
            observations=observations,
            baseline=args.baseline, compile_db=str(args.compile_db.resolve()),
            scope='Production receive/codec/queue/mixer; controlled output device and capture-success boundary; no game or UDP socket'
        ), indent=2) + '\n')
    (out / 'commands.txt').write_text('\n'.join(commands) + '\n')
    return int(not passed)


if __name__ == '__main__':
    raise SystemExit(main())
