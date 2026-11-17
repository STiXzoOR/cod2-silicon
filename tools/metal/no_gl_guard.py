#!/usr/bin/env python3
"""Build a local dyld interposer which aborts on imported GL/CGL entry points.

Use only with r_renderer=metal. This observes dependency calls too, not just
engine call sites. It generates no game content and does not modify the binary.
"""
import argparse
import re
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("binary", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--imports-only", action="store_true", help="Guard just the engine imports")
args = parser.parse_args()
names = sorted(set(re.findall(r"^\s*_((?:gl[A-Z]|CGL)[A-Za-z0-9_]*)$",
                             subprocess.check_output(["nm", "-u", str(args.binary)], text=True), re.M)))
imported = len(names)
if not args.imports_only:
    sdk = Path(subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip())
    framework = sdk / "System/Library/Frameworks/OpenGL.framework/Versions/A"
    public = set()
    for stub in [framework / "OpenGL.tbd", framework / "Libraries/libGL.tbd"]:
        public.update(re.findall(r"\b_((?:gl[A-Z]|CGL)[A-Za-z0-9_]*)\b", stub.read_text()))
    names = sorted(set(names) | public)
if not names:
    raise SystemExit("No GL imports found; refusing an empty guard")
args.output.mkdir(parents=True, exist_ok=True)
source = args.output / "no_gl_guard.c"
lines = ["#include <stdio.h>", "#include <stdlib.h>", "#include <execinfo.h>"]
for name in names:
    lines.extend([
        f"extern void {name}(void);",
        f'static void reject_{name}(void) {{ fprintf(stderr, "forbidden GL call: {name}\\n"); void *stack[32]; int n = backtrace(stack, 32); backtrace_symbols_fd(stack, n, 2); abort(); }}',
        '__attribute__((used, section("__DATA,__interpose"))) static const struct { const void *replacement; const void *original; } '
        f"entry_{name} = {{ reject_{name}, {name} }};",
    ])
source.write_text("\n".join(lines) + "\n")
library = args.output / "no_gl_guard.dylib"
subprocess.run(["clang", "-arch", "arm64", "-dynamiclib", str(source), "-framework", "OpenGL", "-o", str(library)], check=True)
print(f"Guard covers {len(names)} SDK/imported GL/CGL symbols ({imported} engine imports): {library.resolve()}")
