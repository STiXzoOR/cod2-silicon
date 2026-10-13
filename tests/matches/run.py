#!/usr/bin/env python3
"""Exercise local summary output with production engine flags, outside the checkout."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import re
import selectors
import stat

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-commands', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--lifecycle-baseline')
parser.add_argument('--verify-production', action='store_true')
parser.add_argument('--case', action='append', choices=['metrics', 'lifecycle', 'unmeasured', 'overflow',
                    'retention', 'privacy', 'startup-privacy', 'concurrency', 'symlink-file',
                    'symlink-directory', 'symlink-temporary', 'symlink-lock', 'directory-lock', 'fifo-lock'])
args = parser.parse_args()
output = args.output.resolve()
if ROOT == output or ROOT in output.parents:
    parser.error('--output must be outside the checkout')
output.mkdir(parents=True, exist_ok=True)
def selected(case):
    return not args.case or case in args.case
rows = json.loads(args.compile_commands.read_text())
entry = next(row for row in rows if row['file'].endswith('/src/PC/client_mp/cl_main_mp.c')
             and 'cod2_macos.dir' in (row.get('command') or ' '.join(row['arguments'])))
old_root = entry['file'][:-len('/src/PC/client_mp/cl_main_mp.c')]
command = entry.get('arguments') or shlex.split(entry['command'])
flags, skip = [], False
for part in command:
    if skip:
        skip = False
    elif part in ('-o', '-MF', '-MT', '-MQ'):
        skip = True
    elif part not in ('-c', '-MD', '-MMD', '-DNDEBUG', entry['file']):
        flags.append(part.replace(old_root, str(ROOT)))
exe = output / 'summary'
if args.verify_production:
    for path in ['src/PC/client_mp/cl_main_mp.c', 'src/PC/client_mp/cl_parse_mp.c',
                 'src/PC/qcommon/common.c', 'src/PC/qcommon/match_summary.c']:
        reference = 'src/PC/client_mp/cl_main_mp.c' if path.endswith('match_summary.c') else path
        row = next(row for row in rows if row['file'].endswith('/' + reference)
                   and 'cod2_macos.dir' in (row.get('command') or ' '.join(row['arguments'])))
        previous_root = row['file'][:-len('/' + reference)]
        parts = row.get('arguments') or shlex.split(row['command'])
        production_flags, skip = [], False
        for part in parts:
            if skip: skip = False
            elif part in ('-o', '-MF', '-MT', '-MQ'): skip = True
            elif part not in ('-c', '-MD', '-MMD', row['file']):
                production_flags.append(part.replace(previous_root, str(ROOT)))
        subprocess.run(['taskpolicy', '-b', 'nice', '-n', '19', *production_flags,
                        '-Werror=implicit-function-declaration', '-c', str(ROOT / path),
                        '-o', str(output / (Path(path).stem + '-production.o'))], cwd=ROOT, check=True)
        if not path.endswith('match_summary.c'):
            baseline = subprocess.check_output(['git', 'show', '664f1a1:' + path], cwd=ROOT)
            old = subprocess.run(['unifdef', '-UCOD2_X64'], input=baseline, capture_output=True).stdout
            new = subprocess.run(['unifdef', '-UCOD2_X64'], input=(ROOT / path).read_bytes(), capture_output=True).stdout
            before, after = output / (Path(path).stem + '-legacy-before.c'), output / (Path(path).stem + '-legacy-after.c')
            before.write_bytes(old); after.write_bytes(new)
            subprocess.run(['cmp', str(before), str(after)], check=True)
            print('PASS: production TU + native-off cmp', path, flush=True)
        else:
            legacy = subprocess.run(['unifdef', '-UCOD2_X64'], input=(ROOT / path).read_bytes(), capture_output=True).stdout
            assert legacy == b''
            dedicated = output / 'summary-dedicated.o'
            subprocess.run(['taskpolicy', '-b', 'nice', '-n', '19', *production_flags, '-DDEDICATED',
                            '-c', str(ROOT / path), '-o', str(dedicated)], cwd=ROOT, check=True)
            assert not subprocess.check_output(['nm', '-u', str(dedicated)], text=True).strip()
            print('PASS: production summary; native-off empty; dedicated no client imports', flush=True)
source_path = 'src/PC/client_mp/cl_main_mp.c'
source = (subprocess.check_output(['git', 'show', args.lifecycle_baseline + ':' + source_path], cwd=ROOT).decode()
          if args.lifecycle_baseline else (ROOT / source_path).read_text())
def function(name, text=source):
    match = re.search(r'^[^\n;{}]+\b' + re.escape(name) + r'\([^;{]*?\)\s*\{', text, re.M)
    assert match, name
    depth = 0
    for token in re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', text[match.start():]):
        if token[0] == '{': depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth: return text[match.start():match.start() + token.end()] + '\n'
    raise AssertionError(name)
functions = ''.join(function(name) for name in ['CL_Disconnect', 'CL_MapLoading'])
# The fixture keeps stock and CoD2x lifecycle calls while their unrelated subsystems are stubs.
(output / 'lifecycle_source.h').write_text(functions)
if selected('startup-privacy'):
    cmd = (ROOT / 'src/PC/qcommon/cmd.c').read_text()
    dvars = (ROOT / 'src/PC/qcommon/dvar_cmds.c').read_text()
    common = (ROOT / 'src/PC/qcommon/common.c').read_text()
    bridge = (ROOT / 'src/stubs/link_stubs.c').read_text()
    compat = (ROOT / 'src/stubs/macos_compat.c').read_text()
    split = re.search(r'    com_consoleLines\[0\] = commandLine;[\s\S]*?(?=\n    Swap_Init\(\);)', common)
    assert split, 'production +set command-line split'
    startup = re.search(r'^#define DVAR_COMBINED_STRING_SIZE[^\n]+', dvars, re.M)[0] + '\n'
    startup += ''.join(function(name, cmd) for name in
                      ['Cmd_Argc', 'Cmd_Argv', 'Cmd_TokenizeStringInternal', 'Cmd_TokenizeString'])
    startup += ''.join(function(name, dvars) for name in
                       ['Dvar_GetCombinedString_impl', 'Dvar_Set_f', 'Dvar_SetA_f'])
    startup += function('Com_StartupVariable', common) + function('Dvar_RegisterBool_mac', bridge)
    startup += re.search(r'^static unsigned int bsd_rune_data[^\n]+', compat, re.M)[0] + '\n'
    startup += re.search(r'^void \*__DefaultRuneLocale;', compat, re.M)[0] + '\n'
    native_constructor = compat.replace('init_rune_locale(void)\n#endif', 'init_rune_locale(void)')
    startup += function('___maskrune', compat) + function('init_rune_locale', native_constructor)
    startup += 'static void StartupCommandLine(char *commandLine) { char *p;\n' + split[0] + '\n}\n'
    (output / 'startup_source.h').write_text(startup)
    startup_exe = output / 'startup-privacy'
    subprocess.run(['taskpolicy', '-b', 'nice', '-n', '19', *flags,
                    '-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-Wl,-dead_strip', '-I' + str(output), '-I' + str(ROOT / 'src/PC/qcommon'),
                    str(ROOT / 'tests/matches/startup_privacy.c'),
                    str(ROOT / 'src/PC/universal/dvar.c'),
                    str(ROOT / 'src/PC/qcommon/cod2x_policy.c'), '-o', str(startup_exe)], cwd=ROOT, check=True)
    for choice in ['off', 'on', 'default']:
        home = output / ('startup-' + choice)
        home.mkdir(exist_ok=True)
        stats = home / 'main/stats'
        if stats.exists():
            (stats / 'matches.jsonl').unlink(missing_ok=True)
            (stats / '.matches.lock').unlink(missing_ok=True)
            stats.rmdir()
        subprocess.run(['taskpolicy', '-b', 'nice', '-n', '19', str(startup_exe), choice, str(home)], check=True)
        path = home / 'main/stats/matches.jsonl'
        if choice == 'off':
            assert not (home / 'main/stats').exists(), 'Startup opt-out created summary storage'
        else:
            assert len(path.read_text().splitlines()) == 1
        print('PASS: real startup privacy', choice, flush=True)
if args.case == ['startup-privacy']:
    raise SystemExit(0)
subprocess.run(['taskpolicy', '-b', 'nice', '-n', '19', *flags,
                '-UNDEBUG', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                '-I' + str(output), str(ROOT / 'tests/matches/summary.c'), '-o', str(exe)], cwd=ROOT, check=True)
for case in ['metrics', 'lifecycle', 'unmeasured', 'overflow', 'retention', 'privacy', 'symlink-file',
             'symlink-directory', 'symlink-temporary', 'symlink-lock', 'directory-lock', 'fifo-lock']:
    if not selected(case): continue
    home = output / case
    home.mkdir(exist_ok=True)
    path = home / 'main/stats/matches.jsonl'
    if path.parent.is_symlink(): path.parent.unlink()
    path.unlink(missing_ok=True)
    temporary = path.parent / 'matches.jsonl.tmp'
    temporary.unlink(missing_ok=True)
    lock = path.parent / '.matches.lock'
    if lock.is_dir(): lock.rmdir()
    else: lock.unlink(missing_ok=True)
    target = output / (case + '-outside')
    if case.startswith('symlink') or case in ['directory-lock', 'fifo-lock']:
        path.parent.mkdir(parents=True, exist_ok=True)
        if case == 'symlink-directory':
            target.mkdir(exist_ok=True)
            (target / 'matches.jsonl').unlink(missing_ok=True)
            path.parent.rmdir()
            path.parent.symlink_to(target, target_is_directory=True)
        else:
            target.write_text('KEEP\n')
            if case == 'symlink-file': path.symlink_to(target)
            elif case == 'symlink-lock': lock.symlink_to(target)
            elif case == 'directory-lock': lock.mkdir()
            elif case == 'fifo-lock':
                import os
                os.mkfifo(lock, 0o600)
            else: (path.parent / 'matches.jsonl.tmp').symlink_to(target)
    subprocess.run(['taskpolicy', '-b', 'nice', '-n', '19', str(exe), case, str(home)], check=True)
    if case == 'privacy':
        assert not path.exists() and not (home / 'main/stats').exists(), 'Privacy off created summary storage'
        print('PASS:', case)
        continue
    if case.startswith('symlink') or case in ['directory-lock', 'fifo-lock']:
        if case == 'symlink-directory': assert not (target / 'matches.jsonl').exists()
        else: assert target.read_text() == 'KEEP\n', 'Summary write followed a symlink outside stats'
        if case in ['symlink-lock', 'directory-lock', 'fifo-lock']:
            assert not path.exists(), 'Summary write accepted a nonregular or symlink lock'
        print('PASS:', case)
        continue
    assert path.exists(), f'{case}: engine did not write a local match summary'
    records = [json.loads(line) for line in path.read_text().splitlines()]
    if case == 'metrics':
        assert len(records) == 1
        row = records[0]
        assert (row['map'], row['mode'], row['durationSeconds']) == ('mp_toujane', 'sd', 2.301)
        assert row['serverAddress'] == '192.0.2.1:28960' and row['ruleHints']['mod'] == 'zPAM "test"'
        assert (row['frameCount'], row['frameTimeP50MS'], row['frameTimeP99MS'], row['frameTimeMaxMS']) == (100, 3, 7, 2000)
        assert (row['framesUnderCap'], row['capSampleFrames'], row['capFPS']) == (98, 100, 333)
        assert (row['pingAverageMS'], row['pingMaximumMS'], row['pingSamples']) == (40, 50, 3)
        assert (row['packetsReceived'], row['packetsLost'], row['packetLossPercent']) == (3, 2, 40)
        assert row['snapshotsReceived'] == 4
    elif case == 'lifecycle':
        assert len(records) == 2, 'Intermission/map/disconnect duplicated a match or demo recorded one'
        assert [row['endReason'] for row in records] == ['intermission', 'map']
        assert [row['capFPS'] for row in records] == [250, 125]
    elif case == 'unmeasured':
        row = records[0]
        assert row['pingAverageMS'] is None and row['pingMaximumMS'] is None and row['packetLossPercent'] is None
        assert row['capFPS'] == 0 and row['framesUnderCap'] is None and row['capSampleFrames'] == 0
    elif case == 'overflow':
        row = records[0]
        assert row['frameTimeP50MS'] == 3 and row['frameTimeP99MS'] == row['frameTimeMaxMS'] == 20000
        assert row['frameTimeHistogramLimitMS'] == 10000
    else:
        assert len(records) == 200 and all(row['durationSeconds'] == .003 for row in records)
    assert not list((home / 'main/stats').glob('*.tmp')), 'Temporary retention file left behind'
    assert stat.S_IMODE(path.stat().st_mode) == 0o600
    assert stat.S_IMODE(path.parent.stat().st_mode) == 0o700
    assert lock.is_file() and stat.S_IMODE(lock.stat().st_mode) == 0o600
    print('PASS:', case)

def publication_event(process):
    with selectors.DefaultSelector() as selector:
        selector.register(process.stdout, selectors.EVENT_READ)
        assert selector.select(10), 'Concurrent summary process did not reach the publication boundary'
        event = process.stdout.read(1)
        assert event in [b'R', b'L'], f'Unexpected publication event: {event!r}'
        return event

if selected('concurrency'):
    for seed_count in [0, 200]:
        home = output / ('concurrent-' + str(seed_count))
        path = home / 'main/stats/matches.jsonl'
        path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        path.write_text(''.join(json.dumps({'seed': i}) + '\n' for i in range(seed_count)))
        path.chmod(0o600)
        processes = []
        try:
            first = subprocess.Popen(['taskpolicy', '-b', 'nice', '-n', '19', str(exe),
                                      'concurrent-first', str(home)], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
            processes.append(first)
            assert publication_event(first) == b'R'
            second = subprocess.Popen(['taskpolicy', '-b', 'nice', '-n', '19', str(exe),
                                       'concurrent-second', str(home)], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
            processes.append(second)
            event = publication_event(second)
            if event == b'R':
                assert second.wait(timeout=10) == 0
            first.stdin.write(b'\n'); first.stdin.flush()
            assert first.wait(timeout=10) == 0 and second.wait(timeout=10) == 0
        finally:
            for process in processes:
                if process.poll() is None: process.kill(); process.wait()
                process.stdin.close(); process.stdout.close()
        records = [json.loads(line) for line in path.read_text().splitlines()]
        summaries = [row for row in records if 'durationSeconds' in row]
        assert sorted(row['durationSeconds'] for row in summaries) == [.003, .007], \
            'Concurrent publication lost one process summary'
        assert len(records) == min(seed_count + 2, 200)
        if seed_count:
            assert [row['seed'] for row in records if 'seed' in row] == list(range(2, 200))
        lock = path.parent / '.matches.lock'
        assert lock.is_file() and stat.S_IMODE(lock.stat().st_mode) == 0o600
        assert stat.S_IMODE(path.stat().st_mode) == 0o600
        assert not list(path.parent.glob('*.tmp'))
        print('PASS: concurrent publication and retention', seed_count, flush=True)
