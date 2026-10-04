"""Record display, lock and process state without changing user processes."""
import re
import subprocess


def snapshot(out, phase):
    def capture(name, command):
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=15)
        (out / f'{phase}-{name}.txt').write_text(result.stdout)
        return result.stdout
    registry = capture('session', ['ioreg', '-n', 'Root', '-d1'])
    match = re.search(r'"CGSSessionScreenIsLocked"\s*=\s*(Yes|No|true|false)', registry)
    capture('processes', ['ps', '-axo', 'pid,ppid,%cpu,%mem,comm'])
    capture('display', ['system_profiler', 'SPDisplaysDataType'])
    return dict(screen_locked=match.group(1) in ('Yes', 'true') if match else None)
