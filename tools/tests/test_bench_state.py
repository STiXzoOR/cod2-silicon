"""Synthetic lock-state checks for the benchmark evidence recorder."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('bench_state', ROOT / 'tools/macos/bench_state.py')
bench_state = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench_state)


class BenchStateTests(unittest.TestCase):
    def locked(self, registry):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(bench_state.subprocess, 'run', return_value=
                              subprocess.CompletedProcess([], 0, stdout=registry)):
                return bench_state.snapshot(Path(directory), 'test')['screen_locked']

    def test_session_and_console_keys(self):
        for key in ('CGSSessionScreenIsLocked', 'IOConsoleLocked'):
            for token, expected in (('Yes', True), ('No', False), ('true', True), ('false', False)):
                with self.subTest(key=key, token=token):
                    self.assertIs(self.locked(f'"{key}" = {token}'), expected)

    def test_unknown_and_session_precedence(self):
        self.assertIsNone(self.locked('No recognized lock key'))
        self.assertIs(self.locked('"IOConsoleLocked" = Yes\n"CGSSessionScreenIsLocked" = No'), False)
