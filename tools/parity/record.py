#!/usr/bin/env python3
"""Record a bounded deterministic local-server run with an isolated homepath."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import signal
import subprocess
import sys

from compare import read_trace


def sha256(path):
    with path.open('rb') as stream:
        digest = hashlib.sha256()
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='new directory; never reused')
    parser.add_argument('--frames', type=int, default=1000)
    parser.add_argument('--seed', type=int, default=12345)
    parser.add_argument('--map', default='mp_toujane')
    parser.add_argument('--timeout', type=float, default=180)
    parser.add_argument('--port', type=int, help='unique loopback UDP port for parallel runs')
    parser.add_argument('--dedicated', action='store_true', help='use with cod2_lnxded on Linux')
    args = parser.parse_args()
    if args.frames < 1 or not 0 <= args.seed <= 2147483647 or args.timeout <= 0:
        parser.error('positive frames/timeout and a nonnegative int32 seed are required')
    if not re.fullmatch(r'mp_[A-Za-z0-9_]+', args.map):
        parser.error('expected an mp_ map name')
    if args.port is not None and not 1024 < args.port <= 65535:
        parser.error('port must be 1025..65535')
    binary, data, out = args.binary.resolve(), args.data.resolve(), args.output.resolve()
    iwds = sorted((data / 'main').glob('*.iwd'))
    if not binary.is_file() or not os.access(binary, os.X_OK) or not iwds:
        parser.error('need an executable binary and data/main/*.iwd')
    if any(c in str(data) + str(out) for c in '";+\n\r'):
        parser.error('paths cannot contain quotes, semicolons, + or newlines')
    out.mkdir(parents=True, exist_ok=False)
    home = out / 'home'
    home.mkdir()
    command = [str(binary), '+set', 'fs_basepath', f'"{data}"', '+set', 'fs_homepath', f'"{home}"',
               '+set', 'fs_game', '""', '+set', 'dedicated', '1' if args.dedicated else '0',
               '+set', 'logfile', '2', '+set', 'sv_punkbuster', '0',
               '+set', 'net_ip', '127.0.0.1',
               '+set', 'sv_fps', '20', '+set', 'fixedtime', '50',
               '+set', 'g_gametype', 'dm', '+map', args.map]
    if args.port is not None:
        command[1:1] = ['+set', 'net_port', str(args.port)]
    env = dict(os.environ, SYSDIFF_SEED=str(args.seed), SYSDIFF_MAXFRAMES=str(args.frames),
               SYSDIFF_STATEHASH=str(out / 'statehash.txt'), SYSDIFF_DUMP=str(out / 'entities.txt'))
    metadata = dict(command=command, platform=platform.platform(), binary_sha256=sha256(binary),
                    seed=args.seed, frames=args.frames,
                    iwd_sha256={str(p.relative_to(data)): sha256(p) for p in iwds})
    (out / 'run.json').write_text(json.dumps(metadata, indent=2) + '\n')
    with (out / 'console.log').open('w') as log:
        process = subprocess.Popen(command, env=env, cwd=out, stdout=log, stderr=subprocess.STDOUT,
                                   start_new_session=True)
        try:
            code = process.wait(timeout=args.timeout)
        except (subprocess.TimeoutExpired, KeyboardInterrupt):
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            raise ValueError(f'Run interrupted or exceeded {args.timeout}s; inspect {out}')
    if code:
        raise ValueError(f'Engine exited {code}; inspect {out / "console.log"}')
    rows = read_trace(out / 'statehash.txt')
    if len(rows) != args.frames or not any(row[2] > 0 for row in rows):
        raise ValueError('Incomplete or empty-world trace; refusing a false pass')
    if not (out / 'entities.txt').is_file() or not (out / 'entities.txt').stat().st_size:
        raise ValueError('Missing final entity dump')
    print(f'Recorded {len(rows)} frames in {out}')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        sys.exit(str(error))
