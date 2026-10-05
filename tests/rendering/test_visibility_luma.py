"""Prove the diagnostic measures the lower image, using synthetic pixels only."""
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import zlib


class FloorRegionTests(unittest.TestCase):
    def test_lower_region_orientation(self):
        def chunk(kind, data):
            return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
        with tempfile.TemporaryDirectory(prefix='cod2-floor-luma-') as temp:
            out = Path(temp)
            image, analyzer = out / 'synthetic.png', out / 'luma'
            raw = b''.join(b'\0' + bytes([200 if y < 20 else 30] * 80 * 3) for y in range(40))
            image.write_bytes(b'\x89PNG\r\n\x1a\n' +
                chunk(b'IHDR', struct.pack('>IIBBBBB', 80, 40, 8, 2, 0, 0, 0)) +
                chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
            subprocess.run(['taskpolicy', '-b', 'nice', '-n', '19', 'swiftc',
                            str(Path(__file__).with_name('visibility_luma.swift')), '-o', str(analyzer)], check=True)
            result = json.loads(subprocess.check_output([str(analyzer), str(image)], text=True))
            self.assertAlmostEqual(result['floor_luma'], 30)
            self.assertEqual((result['width'], result['height']), (80, 40))


if __name__ == '__main__':
    unittest.main()
