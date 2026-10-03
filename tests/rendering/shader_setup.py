"""Cache verification must reject tampering and incomplete manifests."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("extract", ROOT / "tools/macos-port/extract_shaders.py")
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)


class ShaderSetup(unittest.TestCase):
    def test_manifest_verifies_bytes_and_complete_pairs(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            assets = {"sample.vsa": b"!!ARBvp1.0\nEND", "sample.vc": extract.constant_table(b"0")}
            for name, data in assets.items():
                (cache / name).write_bytes(data)
            manifest = {"binary_sha256": "0" * 64, "assets": {
                name: hashlib.sha256(data).hexdigest() for name, data in assets.items()}}
            (cache / "manifest.json").write_text(json.dumps(manifest))
            self.assertTrue(extract.verify_cache(cache, expected_count=2))
            (cache / "sample.vsa").write_bytes(b"tampered")
            self.assertFalse(extract.verify_cache(cache, expected_count=2))
            (cache / "sample.vsa").write_bytes(assets["sample.vsa"])
            (cache / "sample.vc").unlink()
            self.assertFalse(extract.verify_cache(cache, expected_count=2))

    def test_rejects_manifest_path_escape_and_wrong_count(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            (cache / "manifest.json").write_text(json.dumps({"binary_sha256": "0" * 64,
                "assets": {"../outside.vsa": "0" * 64}}))
            self.assertFalse(extract.verify_cache(cache, expected_count=1))
            self.assertFalse(extract.verify_cache(cache))

    def test_rejects_malformed_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            for manifest in [None, [], {"binary_sha256": "0" * 64, "assets": ["invalid"]}]:
                (cache / "manifest.json").write_text(json.dumps(manifest))
                self.assertFalse(extract.verify_cache(cache, expected_count=1))


if __name__ == "__main__":
    unittest.main()
