"""Native first-run setup validation with original synthetic cache payloads."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PREPARE = ROOT / "build-macos/metal/prepare_shaders"


@unittest.skipUnless(PREPARE.exists(), "SKIP: build the native shader setup tool with tools/metal/run.sh")
class Setup(unittest.TestCase):
    def test_native_cache_boundary(self):
        support = Path.home() / "Library/Application Support"
        support.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="cod2-metal-synthetic-", dir=support) as directory:
            root = Path(directory)
            cache = root / "input"
            output = root / "output"
            cache.mkdir()

            def prepare(destination=output):
                return subprocess.run([str(PREPARE), str(cache), str(destination)],
                                      capture_output=True, text=True)

            absent = prepare()
            self.assertEqual(absent.returncode, 77)
            self.assertIn("SKIP", absent.stdout)
            refused = prepare(ROOT / "build-macos/metal/refused-derived-output")
            self.assertEqual(refused.returncode, 1)
            self.assertIn("Refusing", refused.stdout)
            self.assertFalse((ROOT / "build-macos/metal/refused-derived-output").exists())

            # Independent one-register CTAB record, string and type information.
            table = (struct.pack("<7I", 28, 0, 0, 1, 28, 0, 0)
                     + struct.pack("<I4H2I", 64, 0, 1, 1, 0, 48, 0)
                     + struct.pack("<6HI", 1, 3, 1, 4, 1, 0, 0) + b"synthetic\0")
            assets = {}
            for stage, count, signature, result in [
                ("v", 208, "vp", "position"), ("p", 209, "fp", "color")
            ]:
                for index in range(count):
                    program = f"sample_{index}.{'vsa' if stage == 'v' else 'pse'}"
                    assets[program] = f"!!ARB{signature}1.0\nMOV result.{result}, {{0,0,0,1}}; END\n".encode()
                    assets[f"sample_{index}.{stage}c"] = table

            def save():
                for name, data in assets.items():
                    (cache / name).write_bytes(data)
                (cache / "manifest.json").write_text(json.dumps({
                    "binary_sha256": "0" * 64,
                    "assets": {name: hashlib.sha256(data).hexdigest() for name, data in assets.items()}
                }))

            save()
            accepted = prepare()
            self.assertEqual(accepted.returncode, 0, accepted.stdout)
            self.assertIn("417/417 programs; 417/417 metadata; failures=0", accepted.stdout)
            self.assertEqual(len(list(output.glob("*.metal"))), 417)
            (cache / "sample_0.vsa").write_bytes(b"tampered")
            rejected = prepare()
            self.assertEqual(rejected.returncode, 1)
            self.assertIn("digest mismatch", rejected.stdout)
            assets["sample_0.vc"] = table[:12] + struct.pack("<I", 257) + table[16:]
            save()
            rejected = prepare()
            self.assertEqual(rejected.returncode, 1)
            self.assertIn("Invalid constant records", rejected.stdout)


if __name__ == "__main__":
    unittest.main()
