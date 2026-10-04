#!/usr/bin/env python3
"""Fallback cleanup for a client identified by its fresh observer PID and home."""
from pathlib import Path
import os
import signal
import subprocess
import sys
import time

out = Path(sys.argv[1]).resolve()
pid_file = out / 'observer.pid'
if not pid_file.is_file():
    raise SystemExit(0)
pid = int(pid_file.read_text())


def owned():
    result = subprocess.run(['ps', '-p', str(pid), '-o', 'command='],
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    return result.returncode == 0 and str(out / 'home') in result.stdout


if not owned():
    raise SystemExit(0)
print(f'Fallback cleanup of owned client PID {pid}')
os.kill(pid, signal.SIGTERM)
deadline = time.monotonic() + 10
while owned() and time.monotonic() < deadline:
    time.sleep(.1)
if owned():
    os.kill(pid, signal.SIGKILL)
