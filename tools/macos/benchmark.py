#!/usr/bin/env python3
"""Run a CoD2 timedemo, capture completion and summarize engine frame times."""
import argparse
import csv
import json
import math
import os
from pathlib import Path
import re
import signal
import statistics
import subprocess
import sys
import time

SUMMARY = re.compile(r'(\d+) frames,\s*([0-9.]+) seconds:\s*([0-9.]+) fps')


def summarize(log, csv_path):
    if 'Demo file was truncated.' in log:
        raise ValueError('Engine reported a truncated demo')
    matches = list(SUMMARY.finditer(log))
    if not matches:
        raise ValueError('No completed timedemo FPS summary')
    frames, seconds, fps = matches[-1].groups()
    values = []
    previous = None
    with csv_path.open(newline='') as stream:
        for row in csv.reader(stream):
            if len(row) != 2:
                raise ValueError('Expected headerless frame,milliseconds CSV')
            frame, msec = map(int, row)
            if msec < 0 or (previous is not None and frame != previous + 1):
                raise ValueError('Negative timing or noncontiguous CSV frames')
            previous = frame
            values.append(msec)
    if not values or int(frames) <= 0 or float(fps) <= 0:
        raise ValueError('Empty or invalid timedemo result')
    # The engine omits the initial timestamp-only frame.
    if len(values) != int(frames) - 1:
        raise ValueError('Frame CSV is incomplete or does not match the FPS summary')
    ordered = sorted(values)
    slow = ordered[-max(1, math.ceil(len(values) * .01)):]
    return dict(frames=int(frames), seconds=float(seconds), fps=float(fps),
                samples=len(values), frame_ms_mean=statistics.mean(values),
                one_percent_low=1000 / statistics.mean(slow) if any(slow) else None,
                frame_ms_median=statistics.median(values),
                frame_ms_p95=ordered[math.ceil(len(values) * .95) - 1],
                frame_ms_p99=ordered[math.ceil(len(values) * .99) - 1],
                frame_ms_max=max(values), frames_over_4ms=sum(v > 4 for v in values),
                timing_resolution_ms=1)


def stop(process, grace=5):
    # A wrapper can exit before its engine child. Track the whole process group.
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        process.wait()
        return
    deadline = time.monotonic() + grace
    while time.monotonic() < deadline:
        process.poll()  # reap the group leader if it has exited
        try:
            os.killpg(process.pid, 0)
        except ProcessLookupError:
            process.wait()
            return
        time.sleep(.05)
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait()


class CompletionMonitor:
    """Read newly appended log bytes without repeatedly scanning whole logs."""
    def __init__(self, out):
        self.out = out
        self.offsets = {out / 'console.log': 0}
        self.tails = {}
        self.have_engine_log = False

    def completed(self):
        if not self.have_engine_log:
            for path in (self.out / 'home').rglob('qconsole_mp.log'):
                self.offsets.setdefault(path, 0)
                self.have_engine_log = True
        for path in self.offsets:
            try:
                with path.open('rb') as stream:
                    if path.stat().st_size < self.offsets[path]:
                        self.offsets[path] = 0
                        self.tails[path] = b''
                    stream.seek(self.offsets[path])
                    chunk = self.tails.get(path, b'') + stream.read()
                    self.offsets[path] = stream.tell()
            except FileNotFoundError:
                continue
            self.tails[path] = chunk[-256:]
            if SUMMARY.search(chunk.decode(errors='replace')):
                return True
        return False


def logs(out):
    return '\n'.join(p.read_text(errors='replace') for p in
                     [out / 'console.log', *sorted((out / 'home').rglob('qconsole_mp.log'))]
                     if p.exists())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data', type=Path)
    parser.add_argument('demo', help='basename in main/demos, with optional .dm_1')
    parser.add_argument('--output', type=Path, required=True, help='new directory')
    parser.add_argument('--timeout', type=float, default=180)
    parser.add_argument('--maxfps', type=int, default=0, help='0 uncapped; 250 for pacing check')
    parser.add_argument('--resolution', help='render resolution, e.g. 1920x1080')
    parser.add_argument('--window-mode', choices=['windowed', 'fullscreen', 'borderless'])
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_-]+(?:\.dm_1)?', args.demo):
        parser.error('use a simple demo basename')
    if args.timeout <= 0 or not 0 <= args.maxfps <= 1000:
        parser.error('positive timeout and maxfps in [0,1000] required')
    if args.resolution and not re.fullmatch(r'[1-9][0-9]{2,4}x[1-9][0-9]{2,4}', args.resolution):
        parser.error('resolution must be WIDTHxHEIGHT')
    data, out = args.data.resolve(), args.output.resolve()
    demo = args.demo if args.demo.endswith('.dm_1') else args.demo + '.dm_1'
    if not (data / 'main' / 'demos' / demo).is_file():
        parser.error(f'missing {data / "main" / "demos" / demo}')
    out.mkdir(parents=True, exist_ok=False)
    # A fresh home has no renderer config. Apply latched modes before startup;
    # restarting a renderer after loading assets adds unrelated reset work.
    if args.resolution or args.window_mode:
        profile = out / 'home/raw/players/default'
        profile.mkdir(parents=True)
        settings = []
        if args.resolution:
            settings.append(f'seta r_mode "{args.resolution}"')
        if args.window_mode:
            settings += [f'seta r_fullscreen "{int(args.window_mode != "windowed")}"',
                         f'seta r_borderless "{int(args.window_mode == "borderless")}"']
        (profile / 'config_mp.cfg').write_text('\n'.join(settings) + '\n')
    env = dict(os.environ, COD2_RUN_DIR=str(out))
    command = [str(Path(__file__).with_name('run.sh')), str(data),
               '+set', 'com_maxfps', str(args.maxfps), '+set', 'r_swapInterval', '0',
               '+set', 'logfile', '0',
               # Current reconstruction gates CSV/50ms stepping on this cvar.
               '+set', 'cl_freezeDemo', '1']
    if args.resolution:
        command += ['+set', 'r_mode', args.resolution]
    if args.window_mode:
        command += ['+set', 'r_fullscreen', str(int(args.window_mode != 'windowed')),
                    '+set', 'r_borderless', str(int(args.window_mode == 'borderless'))]
    command += ['+timedemo', args.demo]
    (out / 'benchmark.json').write_text(json.dumps(dict(command=command,
        maxfps=args.maxfps, resolution=args.resolution, window_mode=args.window_mode,
        metal_hud=env.get('MTL_HUD_ENABLED', '0')), indent=2) + '\n')
    deadline = time.monotonic() + args.timeout
    monitor = CompletionMonitor(out)
    with (out / 'wrapper.log').open('w') as stream:
        process = subprocess.Popen(command, env=env, stdout=stream, stderr=subprocess.STDOUT,
                                   start_new_session=True, cwd=out)
        completed = False
        try:
            while time.monotonic() < deadline:
                if monitor.completed():
                    # Completion prints just before closing the CSV. Allow close/flush.
                    time.sleep(.5)
                    completed = True
                    break
                if process.poll() is not None:
                    completed = monitor.completed()
                    break
                time.sleep(.1)
        finally:
            natural_exit = process.poll()
            stop(process)
    if natural_exit not in (None, 0):
        raise ValueError(f'Engine/wrapper failed ({natural_exit}); inspect {out}')
    if not completed:
        raise ValueError(f'No completion before exit/timeout; inspect {out}')
    candidates = list((out / 'home').rglob('timedemo_*_mode_*.csv'))
    if len(candidates) != 1:
        raise ValueError(f'Expected one engine timing CSV, found {len(candidates)}; inspect {out}')
    result = summarize(logs(out), candidates[0])
    result['csv'] = str(candidates[0])
    result.update(resolution=args.resolution, window_mode=args.window_mode, maxfps=args.maxfps)
    result['stopped_after_completion'] = natural_exit is None
    (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        sys.exit(str(error))
