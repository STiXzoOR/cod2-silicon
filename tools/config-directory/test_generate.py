"""Authored metadata/config fixtures only. No network or player corpus."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


class GeneratorTests(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory(prefix="directory-tool-", dir=os.environ.get("TMPDIR"))
        self.root = Path(self.scratch.name)
        self.entries = self.root / "entries"
        self.entries.mkdir()

    def tearDown(self):
        self.scratch.cleanup()

    def run_generator(self, success=True):
        command = ["python3", str(Path(__file__).with_name("generate.py")), "--root", str(self.root),
                   "--base-url", "https://fixtures.invalid/", "--revision", "1", "--audit-binary", os.environ["COD2_DIRECTORY_AUDIT"]]
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, success, result.stderr)
        return result

    def fixture(self):
        folder = self.entries / "synthetic"
        folder.mkdir()
        (folder / "config.cfg").write_bytes(b'seta sensitivity "4"\r\nbind w "+forward"\r\n')
        metadata = dict(player="Authored synthetic fixture", era="test", role="test", feel="Test only",
                        story="No real player or permission claim.", styleTags=["fixture"], credit="Test author",
                        sourceURL="https://fixtures.invalid/source", platform="windows",
                        permission=dict(grantedBy="Test author", grantedAt="2026-10-09",
                                        evidenceURL="https://fixtures.invalid/permission", scope="public-redistribution"))
        (folder / "entry.json").write_text(json.dumps(metadata))
        return folder, metadata

    def test_empty_is_honest(self):
        self.run_generator()
        self.assertEqual(json.loads((self.root / "index.json").read_bytes())["entries"], [])
        self.assertFalse((self.root / "index.sig").exists())
        self.assertIn("No permissioned entries", (self.root / "INDEX.md").read_text())

    def test_exact_bytes_and_real_classifier(self):
        folder, _ = self.fixture()
        original = (folder / "config.cfg").read_bytes()
        self.run_generator()
        index = json.loads((self.root / "index.json").read_bytes())
        self.assertEqual(index["entries"][0]["sha256"], hashlib.sha256(original).hexdigest())
        self.assertEqual(index["entries"][0]["byteCount"], len(original))
        self.assertEqual((folder / "config.cfg").read_bytes(), original)
        review = json.loads((self.root / "review.json").read_bytes())
        self.assertEqual(review["synthetic"]["stock"]["blocked"], 0)
        self.assertGreater(review["synthetic"]["stock"]["safe"], 0)

    def test_permission_credit_and_schema_required(self):
        folder, metadata = self.fixture()
        for change in [dict(permission={}), dict(credit=""), dict(extra="unsupported"),
                       dict(permission=dict(metadata["permission"], scope="private-use"))]:
            (folder / "entry.json").write_text(json.dumps(dict(metadata, **change)))
            self.run_generator(False)
        self.assertFalse((self.root / "index.json").exists())

    def test_forbidden_content_never_published(self):
        folder, _ = self.fixture()
        (folder / "config.cfg").write_bytes(b'seta password "synthetic-secret"\n')
        result = self.run_generator(False)
        self.assertNotIn("synthetic-secret", result.stderr + result.stdout)
        self.assertFalse((self.root / "index.json").exists())

    def test_symlink_rejected(self):
        folder, _ = self.fixture()
        (folder / "config.cfg").unlink()
        outside = self.root / "outside.cfg"
        outside.write_bytes(b'seta sensitivity 4\n')
        (folder / "config.cfg").symlink_to(outside)
        self.run_generator(False)

    def test_check_detects_stale_index(self):
        folder, _ = self.fixture()
        self.run_generator()
        command = ["python3", str(Path(__file__).with_name("generate.py")), "--root", str(self.root),
                   "--base-url", "https://fixtures.invalid/", "--revision", "1", "--audit-binary", os.environ["COD2_DIRECTORY_AUDIT"], "--check"]
        self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
        (folder / "config.cfg").write_bytes(b'seta sensitivity 5\n')
        self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)


if __name__ == "__main__":
    unittest.main()
