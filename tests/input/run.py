#!/usr/bin/env python3
"""Real SDL input QA. Evidence and private configs belong outside the repository."""
import argparse
import json
import math
import os
from pathlib import Path
import re
import shlex
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = Path.home() / 'Library/Application Support/CoD2-native-ws42'


def build(output, compile_db, binary=None):
    database = json.loads(compile_db.read_text())
    entry = next(row for row in database if row['file'].endswith('/linux_input.c'))
    args = entry.get('arguments') or shlex.split(entry['command'])
    flags = []
    n = 1
    while n < len(args):
        arg = args[n]
        if arg in ('-arch', '-isystem', '-F', '-I', '-isysroot'):
            flags.extend(args[n:n + 2]); n += 2; continue
        if arg.startswith(('-D', '-I', '-F', '-std=', '-mmacosx-version-min=')):
            flags.append(arg)
        n += 1
    flags += ['-O2', '-g', '-ffp-contract=off', '-Wno-typedef-redefinition',
              '-Wno-duplicate-decl-specifier', '-Werror=implicit-function-declaration']
    sdl_flags = shlex.split(subprocess.check_output(['sdl2-config', '--cflags', '--libs'], text=True))
    probe = output / 'input-probe.dylib'
    subprocess.run(['clang', *flags, *sdl_flags, '-dynamiclib',
                    str(ROOT / 'tests/input/probe.c'), str(ROOT / 'tests/input/native_events.c'),
                    str(ROOT / 'tests/input/window_metrics.m'),
                    '-o', str(probe)], check=True)
    fixture = output / 'input-characterize'
    fixture_flags = [flag for flag in flags if not flag.startswith('-DCOD2_CODX=')]
    subprocess.run(['clang', *fixture_flags, '-DCOD2_CODX=0', *sdl_flags,
                    '-Wl,-dead_strip', str(ROOT / 'tests/input/characterize.c'),
                    str(ROOT / 'src/unix/linux_input.c'), '-o', str(fixture)], check=True)
    def sdl_name(path):
        links = subprocess.check_output(['otool', '-L', str(path)], text=True)
        return next(line.strip().split(' (')[0] for line in links.splitlines() if '/libSDL2' in line)
    install_name = sdl_name(probe)
    assert sdl_name(fixture) == install_name, 'Fixture and injector use different SDL2 libraries'
    if binary:
        assert sdl_name(binary) == install_name, 'Game and injector use different SDL2 install names'
    (output / 'build.json').write_text(json.dumps({'SDL2': install_name, 'compileDatabase': str(compile_db)}, indent=2) + '\n')
    return probe, fixture


class Driver:
    def __init__(self, process, output):
        self.process, self.output = process, output
        self.sequence = 0
        self.writer = None
        self.events = []
        self.last_snapshot = None
        self.preserved_snapshots = set()
        self.wait(lambda: self.acks().get(0, {}).get('status') == 1, 15)
        self.writer = (output / 'input.fifo').open('w', buffering=1)

    def healthy(self):
        if self.process.poll() is not None:
            raise RuntimeError(f'Process exited {self.process.returncode}; inspect {self.output}')

    def wait(self, predicate, seconds=5):
        deadline = time.monotonic() + seconds
        while not predicate():
            self.healthy()
            if time.monotonic() > deadline:
                raise AssertionError(f'Timed out after {seconds}s; inspect {self.output}')
            time.sleep(.01)

    def acks(self):
        path = self.output / 'input.jsonl'
        rows = []
        if path.exists():
            for line in path.read_text(errors='replace').splitlines():
                try: rows.append(json.loads(line))
                except json.JSONDecodeError: pass  # The writer may be in the middle of a line.
        self.events = [row for row in rows if 'observed' in row]
        return {row['id']: row for row in rows if 'id' in row}

    def queue(self, operation):
        self.sequence += 1
        self.writer.write(f'{self.sequence} {operation}\n')
        return self.sequence

    def ack(self, sequence, expected=1):
        self.wait(lambda: sequence in self.acks())
        row = self.acks()[sequence]
        assert row['status'] == expected, f'Injector rejected {row}'
        return row

    def command(self, operation):
        return self.ack(self.queue(operation))

    def settle(self, milliseconds=80):
        self.queue(f'wait {milliseconds}')
        return self.command('barrier')

    def snapshot(self, keep=False):
        sequence = self.queue('snapshot')
        self.ack(sequence)
        path = self.output / f'snapshot-{sequence}.json'
        state = json.loads(path.read_text())
        if self.last_snapshot and self.last_snapshot not in self.preserved_snapshots:
            self.last_snapshot.unlink(missing_ok=True)
        self.last_snapshot = path
        if keep: self.preserved_snapshots.add(path)
        return state

    def key(self, name, milliseconds=35):
        self.queue(f'key {name} 1')
        self.queue(f'wait {milliseconds}')
        self.queue(f'key {name} 0')
        self.settle()

    def text(self, text):
        # A single UTF-8 character cannot overflow SDL2's fixed 32-byte text field.
        for character in text:
            self.queue('text ' + character.encode('utf-8').hex())
            self.queue('wait 15')
        self.settle()

    def move(self, point, state=None):
        state = state or self.snapshot()
        catcher = state.get('uiContext', {}).get('keyCatchers')
        assert isinstance(catcher, int) and catcher >= 0 and catcher & 8, 'The observed UI does not own input before tested motion'
        x, y = point
        rw, rh = state['render']; ww, wh = state['window']
        assert min(rw, rh, ww, wh) > 0, f'Invalid window/render dimensions: {state}'
        if state.get('mouse', {}).get('relative'):
            dx = round(x * 640 / rw) - state['cursor'][0]
            dy = round(y * 480 / rh) - state['cursor'][1]
        else: dx = dy = 0
        self.command(f'motion {round(x * ww / rw)} {round(y * wh / rh)} {dx} {dy}')
        self.settle(35)
        actual = self.snapshot()['cursor']
        expected = [round(x * 640 / rw), round(y * 480 / rh)]
        assert all(abs(actual[n] - expected[n]) <= 1 for n in range(2)), f'Input coordinate mapping incorrect: expected UI {expected}, got {actual}'

    def relative(self, dx, dy):
        state = self.snapshot()
        rw, rh = state['render']; ww, wh = state['window']
        x, y = state['cursorPixels']
        self.command(f'motion {round(x * ww / rw)} {round(y * wh / rh)} {int(dx)} {int(dy)}')
        self.settle(35)

    def click_point(self, point, state=None, double=False):
        self.move(point, state)
        downs = []
        for n in range(2 if double else 1):
            downs.append(self.queue(f'button 1 1 {n + 1}'))
            self.queue('wait 35')
            self.queue(f'button 1 0 {n + 1}')
            self.queue('wait 85' if double and n == 0 else 'wait 35')
        self.command('barrier')
        if double:
            delta = self.acks()[downs[1]]['tick'] - self.acks()[downs[0]]['tick']
            assert 50 <= delta <= 300, f'Double click timing was {delta}ms'
        self.settle()

    def click(self, menu, item, double=False):
        state = self.snapshot()
        found = find_item(state, menu, item)
        assert found['visible'] and found['enabled'], f'Item cannot be clicked: {menu}/{item}'
        point = center(found['hitRect'])
        self.move(point, state)
        hovered = self.snapshot(keep=True)
        transfer = not find_item(hovered, menu, item)['focused']
        if transfer:
            # Sibling panels retain focus until the normal OOB press transfers
            # it. A control in the already focused panel must hover normally.
            assert not find_menu(hovered, menu)['focused'] and menu in hovered['uiContext']['stack'], f'Pointer did not focus intended control: {menu}/{item}'
        self.click_point(point, hovered, double)
        after = self.snapshot()
        if transfer and find_menu(after, menu)['visible']:
            assert find_item(after, menu, item)['focused'], f'Press did not transfer focus to intended control: {menu}/{item}'
        return after

    def wheel(self, precise_y, flipped=False, repeats=1):
        for _ in range(repeats):
            self.queue(f'wheel 0 {precise_y} {int(flipped)}')
            self.queue('wait 20')
        self.settle()

    def close(self):
        if self.writer: self.writer.close()


def center(rect):
    x, y, width, height = rect
    assert all(math.isfinite(n) for n in rect) and width > 0 and height > 0, f'Invalid hit rect: {rect}'
    return x + width / 2, y + height / 2


def find_menu(state, name):
    matches = [menu for menu in state['menus'] if menu and menu['name'] == name]
    assert len(matches) == 1, f'Menu {name!r} not found uniquely'
    return matches[0]


def find_item(state, menu, name):
    matches = [item for item in find_menu(state, menu)['items'] if item and (item['index'] == name if isinstance(name, int) else item['name'] == name)]
    assert len(matches) == 1, f'Item {menu}/{name} not found uniquely'
    return matches[0]


def environment(output, probe, sdl3, trace=False):
    os.mkfifo(output / 'input.fifo', 0o600)
    env = dict(os.environ, DYLD_INSERT_LIBRARIES=str(probe),
               COD2_TEST_INPUT_FIFO=str(output / 'input.fifo'),
               COD2_TEST_INPUT_OUTPUT=str(output), COD2_TEST_INPUT_ACK=str(output / 'input.jsonl'),
               COD2_TEST_INPUT_SDL3=str(sdl3),
               COD2_MAC_SHADER_CACHE=str(Path.home() / 'Library/Application Support/CoD2 Silicon/shaders'))
    if trace:
        env['COD2_TEST_INPUT_TRACE'] = '1'
        env['COD2_TEST_INPUT_TRACE_TEXT'] = '1'  # Windowless known-character fixture only.
    return env


def stop(process):
    try: os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError: process.wait(); return
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        process.poll()
        try: os.killpg(process.pid, 0)
        except ProcessLookupError: process.wait(); return
        time.sleep(.05)
    # timeout may exit before its game child finishes shutdown. Only the
    # process group created by this driver is eligible for cleanup.
    try: os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError: pass
    process.wait()


def self_test(output, probe, fixture, sdl3):
    # Tests the real installed SDL bridge before any desktop or game run.
    for mode in ('sdl', 'engine'):
        case = output / mode; case.mkdir()
        with (case / 'characterize.jsonl').open('w') as log:
            command = [str(fixture)] + (['--sdl-only'] if mode == 'sdl' else [])
            process = subprocess.Popen(command, env=environment(case, probe, sdl3, True),
                                       stdout=log, stderr=log, start_new_session=True)
            driver = None
            try:
                driver = Driver(process, case)
                driver.command('motion 23 41 7 -4')
                driver.queue('button 1 1 1'); driver.queue('wait 35'); driver.queue('button 1 0 1')
                driver.queue('wait 85'); driver.queue('button 1 1 2'); driver.queue('wait 35'); driver.queue('button 1 0 2')
                driver.key('F8')
                driver.text('A€')
                driver.wheel(.25, repeats=4)
                driver.wheel(-.25, flipped=True, repeats=4)
                driver.command('barrier'); driver.settle(100); driver.acks()
                observed = driver.events
                assert any(row.get('x') == 23 and row.get('dx') == 7 and row.get('dy') == -4 for row in observed), 'SDL lost motion fields'
                assert [row['text'] for row in observed if 'text' in row] == ['A', '€'], 'SDL3 text did not reach SDL2'
                wheels = [row for row in observed if 'preciseY' in row]
                assert len(wheels) == 8 and [row['preciseY'] for row in wheels] == [.25] * 4 + [-.25] * 4, 'SDL bridge lost fractional wheel values'
                assert [row['direction'] for row in wheels] == [0] * 4 + [1] * 4, 'SDL bridge changed scroll direction'
                if mode == 'engine':
                    rows = [json.loads(line) for line in (case / 'characterize.jsonl').read_text().splitlines() if line.startswith('{')]
                    events = [row for row in rows if row.get('engine') == 'event']
                    assert any(row.get('engine') == 'absolute' and row['x'] == 23 and row['y'] == 41 for row in rows), 'Production pump did not dispatch absolute motion'
                    assert any(row.get('value') == ord('€') for row in events), 'Production pump did not decode UTF-8'
                    assert [row['down'] for row in events if row['value'] == 0xc8] == [1, 0, 1, 0], 'Production pump lost button edges'
                    assert any(row['value'] == 0xae and row['down'] for row in events), 'Production pump did not translate F8'
                    assert [row['value'] for row in events if row['value'] in (0xcd, 0xce)] == [0xce, 0xce, 0xcd, 0xcd], 'Production pump did not accumulate fractional wheel events'
                driver.queue('key Invalid_Key_Name 1')
                driver.ack(driver.sequence, -1)
                print(f'PASS {mode}: motion, buttons, key, native text, precise wheel, direction', flush=True)
            finally:
                if driver: driver.close()
                stop(process)
    # Inserting the library without its explicit environment is inert.
    inert = output / 'inert'; inert.mkdir()
    env = {key: value for key, value in os.environ.items() if not key.startswith('COD2_TEST_INPUT_')}
    env['DYLD_INSERT_LIBRARIES'] = str(probe)
    with (inert / 'characterize.jsonl').open('w') as log:
        process = subprocess.Popen([str(fixture)], env=env, cwd=inert,
                                   stdout=log, stderr=log, start_new_session=True)
        try:
            deadline = time.monotonic() + 3
            while 'ready' not in (inert / 'characterize.jsonl').read_text():
                assert process.poll() is None and time.monotonic() < deadline, 'Inert fixture failed'
                time.sleep(.01)
            time.sleep(.1)
            assert sorted(path.name for path in inert.iterdir()) == ['characterize.jsonl'], 'Dylib wrote diagnostics without opt-in'
        finally: stop(process)
    (output / 'results.json').write_text(json.dumps({'completed': True, 'checks': ['SDL bridge', 'production input pump', 'inert without opt-in']}, indent=2) + '\n')
    print('PASS inert: no injector diagnostics without opt-in', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path, nargs='?')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compile-db', type=Path, default=ROOT / 'build-macos/compile_commands.json')
    parser.add_argument('--sdl3', type=Path, default=Path('/opt/homebrew/lib/libSDL3.0.dylib'))
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--base', type=Path, default=Path.home() / 'Games/CoD2')
    parser.add_argument('--port', type=int, default=29942)
    parser.add_argument('--server-address', default='')
    parser.add_argument('--server-binary', type=Path)
    parser.add_argument('--server-port', type=int, default=29952)
    parser.add_argument('--fullscreen', action='store_true')
    parser.add_argument('--native-trace', action='store_true')
    parser.add_argument('--scenario', choices=['inventory', 'menus', 'browser', 'join', 'all', 'options', 'controls', 'maps', 'dialogs'], default='menus')
    args = parser.parse_args()
    output = args.output.expanduser().resolve()
    if EVIDENCE != output and EVIDENCE not in output.parents:
        parser.error(f'Evidence must be under {EVIDENCE}')
    if not 1024 <= args.port <= 65535: parser.error('Use a private unprivileged net_port')
    if not 1024 <= args.server_port <= 65535 or args.server_port == args.port:
        parser.error('Use a distinct private server port')
    if args.server_address and args.server_binary: parser.error('Choose an existing server address or an owned server binary')
    if args.fullscreen and args.scenario not in ('inventory', 'browser'):
        parser.error('Fullscreen QA is limited to the inventory or browser scenario (60 seconds)')
    if not args.self_test and not args.build_only and not args.binary: parser.error('A game binary is required')
    output.mkdir(parents=True, exist_ok=False)
    probe, fixture = build(output, args.compile_db, args.binary)
    if args.build_only:
        print(f'Built {probe} and {fixture}'); return
    if args.self_test:
        self_test(output, probe, fixture, args.sdl3); return
    from scenarios import live
    live(args, output, probe)


if __name__ == '__main__':
    main()
