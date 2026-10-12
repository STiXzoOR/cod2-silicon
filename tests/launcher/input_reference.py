#!/usr/bin/env python3
"""Compare key-name facts to the privately owned Mac 1.3 reference.

Print a verification receipt only, never extracted strings or binary bytes.
This optional check is separate from the synthetic, data-free model runner.
"""
from pathlib import Path
import re
import struct
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / "tools/datagen"))
from macho32 import MachO32

image = MachO32(Path(sys.argv[1]))
data, _ = image.read_object("keynames", 99 * 8)


def string_at(address):
    for section in image.sections:
        if section.address <= address < section.address + section.size:
            start = section.offset + address - section.address
            end = section.offset + section.size
            zero = image.data.find(b"\0", start, end)
            if zero < 0:
                raise ValueError("Unterminated reference key name")
            return image.data[start:zero].decode("ascii")
    raise ValueError("Reference key name outside sections")


reference = {}
for offset in range(0, len(data), 8):
    pointer, number = struct.unpack_from("<II", data, offset)
    if not pointer:
        break
    reference[string_at(pointer)] = number
source = (root / "src/PC/client_mp/cl_keys_mp.c").read_text()
table = re.search(r"keyname_t keynames\[99\] = \{(.*?)\n\};", source, re.S)
if not table:
    raise SystemExit("FAIL: missing engine key-name table")
current = {}
for symbol, number in re.findall(r"\{ \(char \*\)&(str_\w+), (0x[0-9a-f]+) \}", table[1]):
    literal = re.search(r"const char " + symbol + r'\[\] = "([A-Za-z0-9_]+)";', source)
    if not literal:
        raise SystemExit("FAIL: missing engine key-name literal")
    current[literal[1]] = int(number, 16)
if len(reference) != 98 or reference != current or reference.get("F1") != 0xa7:
    raise SystemExit("FAIL: engine key-name facts differ from Mac 1.3")
print("PASS: all 98 engine key names/numbers match Mac 1.3; key 0xa7 is F1")
