#!/usr/bin/env python3
"""Generate original pixel patterns and IWD archives; no game data is read."""
import pathlib
import io
import struct
import sys
import zipfile


def iwi(fmt, width, height, payload, flags=2):
    return struct.pack("<3sBBB3H4I", b"IWi", 5, fmt, flags, width, height, 1,
                       len(payload) + 28, 0, 0, 0) + payload


def main():
    root = pathlib.Path(sys.argv[1])
    (root / "main").mkdir(parents=True)
    samples = {
        "argb": iwi(1, 2, 1, bytes([11, 22, 33, 44, 55, 66, 77, 88])),
        "dxt1": iwi(11, 4, 4, struct.pack("<HHI", 0xF800, 0x07E0, 0xE4E4E4E4)),
        "dxt1-alpha": iwi(11, 3, 2, struct.pack("<HHI", 0, 0xFFFF, 0xFFFFFFFF)),
        "dxt3": iwi(12, 4, 4, bytes([0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE])
                    + struct.pack("<HHI", 0xF800, 0x07E0, 0)),
        "dxt5": iwi(13, 4, 4, bytes([255, 0]) + (sum((i % 8) << (3 * i) for i in range(16))).to_bytes(6, "little")
                    + struct.pack("<HHI", 0xF800, 0x07E0, 0)),
        "dxt5-alpha": iwi(13, 4, 4, bytes([10, 20]) + (sum((i % 8) << (3 * i) for i in range(16))).to_bytes(6, "little")
                    + struct.pack("<HHI", 0, 0xFFFF, 0xFFFFFFFF)),
        "mips": iwi(1, 2, 2, bytes([0, 0, 255, 255]) + bytes([255, 0, 0, 255]) * 4, flags=0),
        "quadrants": iwi(1, 2, 2, bytes([0, 0, 255, 255, 0, 255, 0, 255,
                                       255, 0, 0, 255, 255, 255, 255, 255])),
    }
    for name, data in samples.items():
        (root / (name + ".iwi")).write_bytes(data)
    with zipfile.ZipFile(root / "main/iw_09.iwd", "w", compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("images/loadscreen_mp_toujane.iwi", samples["mips"])
        archive.writestr("images/loadscreen_mp_carentan.iwi", samples["dxt1"])
        archive.writestr("images/loadscreen_mp_dawnville.iwi", samples["quadrants"])
        archive.writestr("unrelated.txt", "Synthetic fixture only")
        archive.comment = b"Synthetic ZIP comment PK\x05\x06 with no trailer"
    with zipfile.ZipFile(root / "stored.iwd", "w", compression=zipfile.ZIP_STORED) as archive:
        archive.writestr("images/loadscreen_mp_toujane.iwi", samples["argb"])
    corrupt = bytearray((root / "stored.iwd").read_bytes())
    offset = corrupt.index(samples["argb"])
    corrupt[offset + 30] ^= 1
    (root / "corrupt.iwd").write_bytes(corrupt)
    class NonSeekable(io.BytesIO):
        def seek(self, *args):
            raise io.UnsupportedOperation("synthetic streaming ZIP")
    stream = NonSeekable()
    with zipfile.ZipFile(stream, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("images/loadscreen_mp_toujane.iwi", samples["argb"])
    (root / "descriptor.iwd").write_bytes(stream.getvalue())
    for name, offset, value in [("encrypted", 8, 1), ("oversized", 24, 0x7FFFFFFF), ("method", 10, 99)]:
        data = bytearray((root / "stored.iwd").read_bytes())
        central = data.index(b"PK\x01\x02")
        struct.pack_into("<I" if name == "oversized" else "<H", data, central + offset, value)
        (root / (name + ".iwd")).write_bytes(data)
    data = bytearray((root / "stored.iwd").read_bytes())
    data[30] ^= 1
    (root / "name-mismatch.iwd").write_bytes(data)
    data = bytearray((root / "stored.iwd").read_bytes())
    struct.pack_into("<I", data, data.index(b"PK\x01\x02") + 42, 0xFFFFFFFE)
    (root / "bad-offset.iwd").write_bytes(data)
    data = (root / "stored.iwd").read_bytes()
    for cut in [0, 21, 40, len(data) - 1]:
        (root / ("truncated-" + str(cut) + ".iwd")).write_bytes(data[:cut])


if __name__ == "__main__":
    main()
