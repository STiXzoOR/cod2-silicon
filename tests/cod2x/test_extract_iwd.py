import importlib.util
import io
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location("extract_iwd", Path(__file__).parents[2] / "tools/cod2x/extract_iwd.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def archive(files):
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, "w") as z:
        for name, value in files.items():
            z.writestr(name, value)
    return stream.getvalue()


class ExtractTests(unittest.TestCase):
    def setUp(self):
        self.payload = archive({"ui_mp/main.menu": "fixture", "materials/radar_player_arrow": "fixture"})
        header = bytearray(128)
        header[:2] = b"MZ"
        struct.pack_into("<I", header, 60, 64)
        header[64:68] = b"PE\0\0"
        self.dll = bytes(header) + self.payload + b"trailing section data"

    def test_embedded_dll_and_nested_release(self):
        self.assertEqual(module.extract_payload(self.dll), self.payload)
        release = archive({"bin/windows/mss32.dll": self.dll, "readme.txt": "fixture"})
        self.assertEqual(module.extract_payload(release), self.payload)

    def test_direct_iwd_release(self):
        self.assertEqual(module.extract_payload(archive({module.IWD_NAME: self.payload})), self.payload)

    def test_reject_ambiguous_or_corrupt_input(self):
        with self.assertRaises(ValueError):
            module.extract_payload(b"not a DLL or release archive")
        with self.assertRaises(ValueError):
            module.extract_payload(self.dll + self.payload)

    def test_no_overwrite_without_force(self):
        with tempfile.TemporaryDirectory() as d:
            game = Path(d)
            output = module.install_payload(self.payload, game, False)
            self.assertEqual(output.read_bytes(), self.payload)
            output.write_bytes(b"preserve")
            with self.assertRaises(FileExistsError):
                module.install_payload(self.payload, game, False)
            self.assertEqual(output.read_bytes(), b"preserve")


if __name__ == "__main__":
    unittest.main()
