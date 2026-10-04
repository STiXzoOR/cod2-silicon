"""Synthetic harness tests: no game data, engine binary or external packages."""
import importlib.util
import os
import signal
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]


def module(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


parity = module('parity_compare', 'tools/parity/compare.py')
bench = module('benchmark', 'tools/macos/benchmark.py')
artifacts = module('artifacts', 'tools/ci/compare-artifacts.py')


class VerificationTests(unittest.TestCase):
    def setUp(self):
        (ROOT / 'output').mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix='harness test ', dir=ROOT / 'output')
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)

    def trace(self, text):
        path = self.path / 'trace.txt'
        path.write_text(text)
        return parity.read_trace(path)

    def test_normalized_parity_ignores_raw(self):
        a = self.trace('frame 1 simt 50 ents 2 raw 11111111 q 22222222\n')
        b = self.trace('frame 1 simt 50 ents 2 raw 33333333 q 22222222\n')
        self.assertIsNone(parity.compare(a, b))
        self.assertIn('frame 1', parity.compare(a, b, raw=True))

    def test_first_difference_and_truncation(self):
        a = [(1, 50, 2, 'a', 'b'), (2, 100, 2, 'a', 'c')]
        for b in (a[:1], [a[0], (2, 100, 2, 'a', 'd')]):
            self.assertIn('record 2, frame 2', parity.compare(a, b))

    def test_reject_bad_traces(self):
        row = 'frame 1 simt 50 ents 2 raw 11111111 q 22222222\n'
        for value in ('', 'garbage', row + row, row + row.replace('frame 1', 'frame 3')):
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.trace(value)

    def test_frame_stats(self):
        path = self.path / 'times.csv'
        path.write_text('1,2\n2,4\n3,10\n')
        result = bench.summarize('4 frames, 0.1 seconds: 40.0 fps', path)
        self.assertEqual(result['frame_ms_mean'], 16 / 3)
        self.assertEqual(result['frame_ms_p99'], 10)
        self.assertEqual(result['frames_over_4ms'], 1)
        with self.assertRaises(ValueError):
            bench.summarize('5 frames, 0.1 seconds: 50.0 fps', path)
        with self.assertRaises(ValueError):
            bench.summarize('no completion', path)

    def test_objects_require_elf32_and_objects(self):
        root = self.path
        with self.assertRaises(ValueError):
            artifacts.artifacts(root)
        obj = root / 'CMakeFiles/cod2_objs.dir/test.c.o'
        obj.parent.mkdir(parents=True)
        obj.write_bytes(b'object')
        for name in ('cod2_linux', 'cod2_lnxded'):
            (root / name).write_bytes(b'\x7fELF\x01synthetic')
        self.assertEqual(len(artifacts.artifacts(root)), 3)
        (root / 'cod2_linux').write_bytes(b'\x7fELF\x02synthetic')
        with self.assertRaises(ValueError):
            artifacts.artifacts(root)

    def fake(self, body):
        path = self.path / 'fake-engine'
        path.write_text('#!' + sys.executable + '\n' + body)
        path.chmod(0o755)
        data = self.path / 'game data'
        (data / 'main/demos').mkdir(parents=True)
        (data / 'main/synthetic.iwd').write_text('synthetic fixture, not game content')
        (data / 'main/demos/test.dm_1').write_text('synthetic fixture')
        return path, data

    def record(self, body, frames=2):
        binary, data = self.fake(body)
        return subprocess.run([sys.executable, str(ROOT / 'tools/parity/record.py'),
            '--binary', str(binary), '--data', str(data), '--output', str(self.path / 'run'),
            '--frames', str(frames), '--timeout', '.3'], capture_output=True, text=True)

    def test_record_success(self):
        result = self.record('''import os
import signal
from pathlib import Path
Path(os.environ['SYSDIFF_STATEHASH']).write_text('frame 1 simt 50 ents 2 raw 11111111 q 22222222\\nframe 2 simt 100 ents 2 raw 11111111 q 22222222\\n')
Path(os.environ['SYSDIFF_DUMP']).write_text('e0 eq=22222222\\n')
''')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.path / 'run/run.json').is_file())

    def test_record_empty_failure(self):
        result = self.record('pass\n')
        self.assertNotEqual(result.returncode, 0)

    def test_record_timeout(self):
        result = self.record('import time\ntime.sleep(10)\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('exceeded', result.stderr)

    def test_run_preserves_exit_and_paths(self):
        binary, data = self.fake('''import sys
assert any('game data"' in arg for arg in sys.argv)
sys.exit(7)
''')
        result = subprocess.run([str(ROOT / 'tools/macos/run.sh'), str(data)],
            env=dict(os.environ, COD2_BINARY=str(binary), COD2_RUN_DIR=str(self.path / 'run')),
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 7, result.stderr)

    def test_benchmark_completion_and_shutdown(self):
        binary, data = self.fake('''import sys, time
from pathlib import Path
args = sys.argv
home = Path(args[args.index('fs_homepath') + 1].strip('"'))
assert args[args.index('cl_freezeDemo') + 1] == '1'
(home / 'main/demos').mkdir(parents=True)
(home / 'main/demos/timedemo_test_mode_0.csv').write_text('1,2\\n2,4\\n3,10\\n')
(home / 'main/qconsole_mp.log').write_text('4 frames, 0.1 seconds: 40.0 fps\\n')
time.sleep(30)
''')
        result = subprocess.run([str(ROOT / 'tools/macos/bench.sh'), str(data), 'test',
            '--output', str(self.path / 'bench'), '--timeout', '3'],
            # The harness waits up to 10 s for a scripted quit before stopping a hung engine.
            env=dict(os.environ, COD2_BINARY=str(binary)), capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('"stopped_after_completion": true', result.stdout)
        self.assertTrue((self.path / 'bench/results.json').is_file())

    def test_truncated_demo_cannot_pass(self):
        path = self.path / 'times.csv'
        path.write_text('1,2\n2,4\n3,10\n')
        with self.assertRaisesRegex(ValueError, 'truncated'):
            bench.summarize('Demo file was truncated.\n4 frames, 0.1 seconds: 40.0 fps', path)

    def test_incremental_completion_spanning_reads(self):
        monitor = bench.CompletionMonitor(self.path)
        log = self.path / 'console.log'
        log.write_text('4 frames, 0.1 sec')
        self.assertFalse(monitor.completed())
        with log.open('a') as stream:
            stream.write('onds: 40.0 fps\n')
        self.assertTrue(monitor.completed())
        self.assertEqual(monitor.offsets[log], log.stat().st_size)

    def test_timeout_summary_during_cleanup_cannot_pass(self):
        binary, data = self.fake('''import sys, time, signal
from pathlib import Path
args = sys.argv
home = Path(args[args.index('fs_homepath') + 1].strip('"'))
(home / 'main/demos').mkdir(parents=True)
def terminated(*unused):
    (home / 'main/demos/timedemo_test_mode_0.csv').write_text('1,2\\n2,4\\n3,10\\n')
    (home / 'main/qconsole_mp.log').write_text('4 frames, 0.1 seconds: 40.0 fps\\n')
    sys.exit(0)
signal.signal(signal.SIGTERM, terminated)
time.sleep(30)
''')
        result = subprocess.run([str(ROOT / 'tools/macos/bench.sh'), str(data), 'test',
            '--output', str(self.path / 'bench'), '--timeout', '.3'],
            env=dict(os.environ, COD2_BINARY=str(binary)), capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('No completion', result.stderr)
        self.assertFalse((self.path / 'bench/results.json').exists())

    def test_stop_kills_descendant_after_wrapper_exits(self):
        body = '''import os, signal, time
from pathlib import Path
if os.fork() == 0:
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    Path('ready').write_text(str(os.getpid()))
    while True:
        Path('heartbeat').write_text(str(time.monotonic_ns()))
        time.sleep(.02)
else:
    time.sleep(30)
'''
        process = subprocess.Popen([sys.executable, '-c', body], cwd=self.path,
                                   start_new_session=True)
        try:
            deadline = time.monotonic() + 3
            while not (self.path / 'heartbeat').exists() and time.monotonic() < deadline:
                time.sleep(.02)
            self.assertTrue((self.path / 'heartbeat').exists())
            bench.stop(process, grace=.2)
            heartbeat = (self.path / 'heartbeat').read_text()
            time.sleep(.15)
            self.assertEqual((self.path / 'heartbeat').read_text(), heartbeat)
            self.assertIsNotNone(process.poll())
        finally:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()


if __name__ == '__main__':
    unittest.main()
