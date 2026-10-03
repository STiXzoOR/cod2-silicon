"""Verify the native crash reporter in a subprocess without creating a core file."""
import resource
import signal
import subprocess
import sys

resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
for sig, args in ((signal.SIGABRT, []), (signal.SIGTRAP, ["trap"])):
    result = subprocess.run([sys.argv[1], *args], capture_output=True, text=True)
    output = result.stdout + result.stderr
    assert result.returncode == -sig, (result.returncode, output)
    for field in ("pc", "sp", "fp", "lr", "x0", "x28", "backtrace", "raise"):
        assert field in output, (field, output)
print("native crash: SIGABRT/SIGTRAP, arm64 registers and symbolized backtrace passed")
