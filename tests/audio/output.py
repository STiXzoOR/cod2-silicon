#!/usr/bin/env python3
"""Exercise production 3D sound bodies through the native float output mixer."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/fidelity'))
from spotcheck_reverb import LEX


def extract_function(source, name):
    clean = LEX.sub(lambda match: ' ' * len(match[0]), source)
    start = re.search(r'^(?:static\s+)?(?:void|int|float|Bool)\s+' + re.escape(name) + r'\s*\([^;{]*\)\s*\{', clean, re.M)
    if not start:
        raise ValueError('Missing function: ' + name)
    begin = clean.index('{', start.start())
    depth, end = 1, begin + 1
    while depth and end < len(clean):
        depth += (clean[end] == '{') - (clean[end] == '}')
        end += 1
    if depth:
        raise ValueError('Unterminated function: ' + name)
    return source[start.start():end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-db', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--case', choices=['position', 'update', 'timeline', 'footsteps', 'all'], default='all')
    parser.add_argument('--wave', type=Path, help='Optional private PCM footstep WAV; never copied into the repository')
    args = parser.parse_args()
    if args.wave and args.case != 'footsteps':
        parser.error('--wave is only supported with --case footsteps')
    output = args.output.resolve()
    if output.is_relative_to(ROOT):
        parser.error('Keep build and licensed-audio evidence outside the repository')
    output.mkdir(parents=True, exist_ok=True)
    rows = json.loads(args.compile_db.read_text())

    def flags(suffix):
        entry = next(r for r in rows if r['file'].endswith('/' + suffix) and 'cod2_macos.dir' in r['command'])
        original_root = str(Path(entry['file']).parents[len(Path(suffix).parts) - 1])
        command = shlex.split(entry['command'])
        result = [part.replace(original_root, str(ROOT)) for part in command[:command.index('-o')] if part != '-DNDEBUG']
        return result + ['-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']

    driver = (ROOT / 'src/PC/win32/snd_driver.c').read_text()
    sound = (ROOT / 'src/PC/snd.c').read_text()
    cgame = (ROOT / 'src/PC/cgame_mp/cg_main_mp.c').read_text()
    header = '\n'.join(extract_function(source, name) for source, names in [
        (cgame, ['CG_GetEntityOrientation', 'CG_PlayEntitySoundAlias']),
        (sound, ['SND_GetListenerIndexNearestToOrigin', 'SND_GetLerpedSlavePercentage', 'SND_Attenuate', 'SND_IsAliasChannel3D',
                 'SND_SetListener', 'SND_GetCurrent3DPosition', 'SND_SetChannelInfo']),
        (driver, ['SND_Set3DPosition', 'SND_StartAlias3DSample', 'SND_Update3DChannel']),
    ] for name in names)
    (output / 'output_source.h').write_text(header + '\n')
    backend_object = output / 'render.o'
    subprocess.run(flags('src/platform/macos_audio.c') + ['-c', str(ROOT / 'tests/audio/render.c'), '-o', str(backend_object)], check=True)
    binary = output / 'audio-output'
    subprocess.run(flags('src/PC/win32/snd_driver.c') + ['-I' + str(output), '-Wl,-dead_strip',
                   str(ROOT / 'tests/audio/output.c'), str(ROOT / 'src/PC/cgame_mp/cg_event_mp.c'),
                   str(ROOT / 'src/PC/universal/com_math.c'), str(ROOT / 'src/PC/qcommon/port_debug.c'),
                   str(backend_object), '-framework', 'AudioToolbox', '-framework', 'CoreAudio', '-o', str(binary)], check=True)
    cases = ['position', 'update', 'timeline', 'footsteps'] if args.case == 'all' else [args.case]
    result = []
    for case in cases:
        command = [str(binary), case]
        if args.wave:
            command.append(str(args.wave.resolve()))
        run = subprocess.run(command, capture_output=True, text=True, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        text = run.stdout + run.stderr
        (output / (case + '.log')).write_text(text)
        print(text, end='')
        result.append({'case': case, 'returncode': run.returncode})
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    return int(any(r['returncode'] for r in result))


if __name__ == '__main__':
    raise SystemExit(main())
