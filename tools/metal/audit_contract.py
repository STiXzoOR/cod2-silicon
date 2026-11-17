#!/usr/bin/env python3
"""Inventory native engine COM calls and summarize private draw traces.

Requires an existing CMake compile_commands.json and the configured compiler.
No binaries are built. JSON goes to stdout; keep trace results outside git.
"""

import argparse
from collections import Counter
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
INTERFACES = {
    "IDirect3D9": "CDirect3D",
    "IDirect3DDevice9": "CDirect3DDevice",
    "IDirect3DVertexBuffer9": "CDirect3DVertexBuffer",
    "IDirect3DIndexBuffer9": "CDirect3DIndexBuffer",
    "IDirect3DTexture9": "CDirect3DTexture",
    "IDirect3DCubeTexture9": "CDirect3DCubeTexture",
    "IDirect3DVolumeTexture9": "CDirect3DVolumeTexture",
    "IDirect3DSurface9": "CDirect3DSurface",
    "IDirect3DSwapChain9": "CDirect3DSwapChain",
    "IDirect3DVertexDeclaration9": "CDirect3DVertexDeclaration",
    "IDirect3DVertexShader9": "CDirect3DVertexShader",
    "IDirect3DPixelShader9": "CDirect3DPixelShader",
}


def walk(node):
    yield node
    for child in node.get("inner", []):
        yield from walk(child)


def integer(node):
    """Evaluate only AST integer constants used in virtual table subscripts."""
    if node.get("kind") in ("IntegerLiteral", "ConstantExpr") and "value" in node:
        return int(node["value"])
    children = node.get("inner", [])
    if node.get("kind") == "BinaryOperator" and len(children) == 2:
        left, right = map(integer, children)
        operations = {
            "/": lambda: left // right, "+": lambda: left + right,
            "-": lambda: left - right, "*": lambda: left * right,
            "<<": lambda: left << right, ">>": lambda: left >> right,
        }
        return operations[node["opcode"]]()
    if node.get("kind") == "DeclRefExpr":
        value = node.get("referencedDecl", {}).get("value")
        if value is not None:
            return int(value)
    if len(children) == 1:
        return integer(children[0])
    raise ValueError("nonconstant slot")


def position(point):
    return point.get("expansionLoc", point.get("spellingLoc", point))


def snippet(node, source):
    bounds = node.get("range", {})
    begin = position(bounds.get("begin", {}))
    end = position(bounds.get("end", {}))
    if "offset" not in begin or "offset" not in end:
        return ""
    return source[begin["offset"]:end["offset"] + end.get("tokLen", 1)]


def tables():
    result = {}
    for file in sorted((ROOT / "src/Mac/DirectX_9").glob("*.c")):
        source = file.read_text()
        for match in re.finditer(r"(?:fnptr_t|void\s*\*)\s+vtbl_(\w+)\s*\[[^]]*\]\s*=\s*\{([^}]+)\}", source, re.S):
            entries = re.findall(r"\(fnptr_t\)\s*(\w+)", match[2])
            if entries:
                result[match[1]] = entries
    return result


def identify(callee, receiver, receiver_source):
    for node in walk(callee):
        if node.get("kind") in ("CStyleCastExpr", "ImplicitCastExpr"):
            signature = node.get("type", {})
            signature = signature.get("desugaredQualType", signature.get("qualType", ""))
            # Extract the first parameter, rather than a returned object type.
            match = re.search(r"\(\s*(?:__attribute__\(\(.*?\)\)\s*)?\*[^)]*\)\s*\(\s*(IDirect3D\w+)", signature)
            if match and match[1] in INTERFACES:
                return INTERFACES[match[1]]
    receiver_type = receiver.get("type", {})
    for interface, name in INTERFACES.items():
        if interface in receiver_type.get("qualType", ""):
            return name
    if re.fullmatch(r"(?:device|dev|dxDevice|d3dDevice|devicePtr|direct3DDevice)", receiver_source.strip()):
        return "CDirect3DDevice"
    return None


def command_for(entry):
    command = entry.get("arguments") or shlex.split(entry["command"])
    args = []
    skip = False
    for arg in command:
        if skip:
            skip = False
        elif arg in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
        elif arg not in ("-c", "-MD", "-MMD", "-MP"):
            args.append(arg)
    return args + ["-w", "-fsyntax-only", "-Xclang", "-ast-dump=json"]


def audit_entry(entry, vtables):
    file = Path(entry["file"]).resolve()
    source = file.read_text()
    run = subprocess.run(command_for(entry), cwd=entry["directory"], capture_output=True, text=True)
    if run.returncode:
        return [], {"file": str(file.relative_to(ROOT)), "error": run.stderr[-4000:]}
    tree = json.loads(run.stdout)
    calls = []
    for function in tree.get("inner", []):
        if function.get("kind") != "FunctionDecl" or function.get("loc", {}).get("includedFrom"):
            continue
        body = next((n for n in function.get("inner", []) if n.get("kind") == "CompoundStmt"), None)
        if not body:
            continue
        for call in walk(body):
            if call.get("kind") != "CallExpr" or len(call.get("inner", [])) < 2:
                continue
            callee, receiver = call["inner"][:2]
            subscript = next((n for n in walk(callee) if n.get("kind") == "ArraySubscriptExpr"), None)
            if not subscript or len(subscript.get("inner", [])) != 2:
                continue
            try:
                slot = integer(subscript["inner"][1])
            except (ValueError, KeyError, ZeroDivisionError):
                slot = None
            expression = snippet(call, source)
            receiver_source = snippet(receiver, source)
            interface = identify(callee, receiver, receiver_source)
            method = None
            if interface in vtables and slot is not None and 0 <= slot < len(vtables[interface]):
                method = vtables[interface][slot]
            begin = position(call.get("range", {}).get("begin", {})).get("offset", 0)
            calls.append({
                "file": str(file.relative_to(ROOT)), "line": source.count("\n", 0, begin) + 1,
                "function": function.get("name"), "slot": slot, "interface": interface,
                "method": method, "receiver": receiver_source,
                "expression": re.sub(r"\s+", " ", expression),
            })
    return calls, None


def trace_summary(file):
    draws = []
    legacy_nonfinite = 0
    for line in file.read_text().splitlines():
        if not line.strip():
            continue
        # Older WS15 traces used printf for constants and emitted nan/inf.
        # Current lp64_draw_trace.h writes JSON null. Match numeric array
        # positions only; never rewrite shader/material label strings.
        line, count = re.subn(r"(?<=[\[,])\s*-?(?:nan|inf)(?=\s*[,\]])", "null", line)
        legacy_nonfinite += count
        draws.append(json.loads(line))
    states = {}
    for draw in draws:
        for state, value in draw.get("states", {}).items():
            states.setdefault(state, set()).add(value)
    return {
        "path": str(file), "draws": len(draws),
        "legacy_nonfinite_values_normalized": legacy_nonfinite,
        "primitive_types": sorted({d["type"] for d in draws}),
        "two_dimensional_draws": sum(bool(d.get("2d")) for d in draws),
        "primitive_count": sum(d["primitives"] for d in draws),
        "render_state_values": {key: sorted(values) for key, values in sorted(states.items(), key=lambda x: int(x[0]))},
        "texture_targets": sorted({t["target"] for d in draws for t in d.get("textures", []) if t.get("id")}),
        "unique_recorded_state_sets": len({json.dumps(d.get("states", {}), sort_keys=True) for d in draws}),
        "fields": sorted(set().union(*(set(draw) for draw in draws))) if draws else [],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compile-commands", type=Path, default=ROOT / "build-macos/compile_commands.json")
    parser.add_argument("--file", action="append", help="restrict to matching TU path suffix; repeatable")
    parser.add_argument("--trace", type=Path, action="append", default=[])
    parser.add_argument("--trace-only", action="store_true")
    args = parser.parse_args()
    calls, failures = [], []
    units = []
    if not args.trace_only:
        if not args.compile_commands.is_file():
            parser.error("configure CMake with CMAKE_EXPORT_COMPILE_COMMANDS=ON first")
        database = json.loads(args.compile_commands.read_text())
        seen = set()
        for entry in database:
            file = Path(entry["file"]).resolve()
            if file in seen or not file.is_relative_to(ROOT / "src/PC/gfx_d3d"):
                continue
            if args.file and not any(str(file).endswith(suffix) for suffix in args.file):
                continue
            source = file.read_text()
            if not re.search(r"\]\s*\)\s*\(", source):
                continue
            seen.add(file)
            units.append(str(file.relative_to(ROOT)))
            print(f"audit {units[-1]}", file=sys.stderr)
            found, error = audit_entry(entry, tables())
            calls.extend(found)
            if error:
                failures.append(error)
    result = {
        "scope": "native preprocessed renderer TUs with indexed indirect calls",
        "translation_units": units, "compiler_failures": failures,
        "method_counts": dict(sorted(Counter(c["method"] for c in calls if c["method"]).items())),
        "calls": calls,
        "unresolved_calls": [c for c in calls if not c["method"]],
        "traces": [trace_summary(file) for file in args.trace],
    }
    print(json.dumps(result, indent=2))
    return bool(failures)


if __name__ == "__main__":
    sys.exit(main())
