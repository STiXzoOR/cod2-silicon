"""Verify the native crash reporter in a subprocess without creating a core file."""
import resource
import signal
import subprocess
import sys

resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
result = subprocess.run([sys.argv[1]], capture_output=True, text=True)
output = result.stdout + result.stderr
assert result.returncode == -signal.SIGABRT, (result.returncode, output)
for field in ("pc", "sp", "fp", "lr", "x0", "x28", "backtrace", "raise"):
    assert field in output, (field, output)
print("native crash: SIGABRT, arm64 registers and symbolized backtrace passed")
