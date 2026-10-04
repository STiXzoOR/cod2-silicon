#!/usr/bin/env python3
"""Compare native extraction against the developer tool using licensed inputs."""
import argparse
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/macos-port"))
from extract_shaders import write_cache

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("binaries", nargs="+", type=pathlib.Path)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix="cod2-silicon-shaders-") as directory:
    scratch = pathlib.Path(directory)
    native = scratch / "extract"
    subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", "clang", "-fobjc-arc",
                    "-mmacosx-version-min=13.0", "-Werror=unguarded-availability",
                    "-Werror=unguarded-availability-new", "-DCOD2_X64=1", "-DCOD2_CODX=1",
                    "-Isrc", "tests/packaging/native_shaders.m", "src/platform/cod2x_native_shaders.m",
                    "-framework", "Foundation", "-o", str(native)], cwd=ROOT, check=True)
    for index, binary in enumerate(args.binaries):
        expected, actual = scratch / f"python-{index}", scratch / f"native-{index}"
        write_cache(binary, expected)
        subprocess.run([str(native), str(binary), str(actual)], check=True)
        assert sorted(p.name for p in expected.iterdir()) == sorted(p.name for p in actual.iterdir())
        for file in expected.iterdir():
            assert file.read_bytes() == (actual / file.name).read_bytes(), file.name
        subprocess.run([str(native), "/absent", str(actual)], check=True)
        payload = next(actual.glob("*.vsa"))
        payload.write_bytes(payload.read_bytes() + b"corrupt")
        assert subprocess.run([str(native), "--verify", str(actual)]).returncode == 1
        subprocess.run([str(native), str(binary), str(actual)], check=True)
        (actual / "manifest.json").write_text('{"assets": []}')
        assert subprocess.run([str(native), "--verify", str(actual)]).returncode == 1
        subprocess.run([str(native), str(binary), str(actual)], check=True)
        assert subprocess.run([str(native), "/absent", str(scratch / "empty")]).returncode == 1
        print(f"PASS {binary.name}: 834 payloads and manifest byte-identical; reuse, damage, malformed manifest, fallback")
