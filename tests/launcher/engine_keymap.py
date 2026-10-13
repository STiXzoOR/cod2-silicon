#!/usr/bin/env python3
"""Compile the production SDL conversion functions as a synthetic oracle.

No engine objects, windows, game data or private reference output are needed.
The fixture fails closed if the functions cannot be extracted/compiled.
"""
import pathlib
import re
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[2]
scratch = pathlib.Path(sys.argv[1])
source = (root / "src/unix/linux_input.c").read_text()
defines = "\n".join(re.findall(r"^#define K_.*$", source, re.M))
functions = []
for name in ("sdl_to_keynum", "sdl_button_to_keynum"):
    match = re.search(r"static int " + name + r"\([^)]*\)\n\{.*?\n\}", source, re.S)
    if not match:
        raise SystemExit("Cannot extract production mapping: " + name)
    functions.append(match[0])
fixture = scratch / "engine-keymap.c"
fixture.write_text("#include <SDL.h>\n#include <stdio.h>\n" + defines + "\n" + "\n".join(functions) + r'''
int main(void) {
    for (int k = 0; k < 256; ++k) printf("key %d %d\n", k, sdl_to_keynum(k));
    const int unicode[] = {0x3b1, 0x3c2, 0x20ac, 0x3042, 0x1f600};
    for (unsigned i = 0; i < sizeof(unicode)/sizeof(unicode[0]); ++i)
        printf("key %d %d\n", unicode[i], sdl_to_keynum(unicode[i]));
    for (int k = 0; k < 512; ++k)
        printf("key %d %d\n", (1 << 30) | k, sdl_to_keynum((1 << 30) | k));
    for (int b = 0; b < 256; ++b) printf("button %d %d\n", b, sdl_button_to_keynum(b));
    return 0;
}
''')
flags = subprocess.check_output(["sdl2-config", "--cflags"], text=True).split()
binary = scratch / "engine-keymap"
subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", "xcrun", "clang",
                "-target", "arm64-apple-macos13", "-Wall", "-Wextra", "-Werror",
                *flags, str(fixture), "-o", str(binary)], check=True)
with (scratch / "engine-keymap.tsv").open("w") as output:
    subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", str(binary)], stdout=output, check=True)
    keys_source = (root / "src/PC/client_mp/cl_keys_mp.c").read_text()
    table = re.search(r"keyname_t keynames\[99\] = \{(.*?)\n\};", keys_source, re.S)
    if not table:
        raise SystemExit("Cannot extract production key names")
    names = re.findall(r"\{ \(char \*\)&(str_\w+), (0x[0-9a-f]+) \}", table[1])
    if len(names) != 98:
        raise SystemExit("Unexpected production key-name count")
    for symbol, number in names:
        literal = re.search(r"const char " + symbol + r'\[\] = "([A-Za-z0-9_]+)";', keys_source)
        if not literal:
            raise SystemExit("Cannot extract named key literal: " + symbol)
        output.write("name " + literal[1] + " " + str(int(number, 16)) + "\n")
