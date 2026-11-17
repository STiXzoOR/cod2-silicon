#!/usr/bin/env python3
"""Derive an incomplete observed-state set from local native Toujane JSONL.

Capture with the existing GL shim hook in lp64_draw_trace.h: before launching
with r_renderer gl, set COD2_MAC_DRAW_TRACE to an absolute request filename under
Application Support. Once Toujane reaches the matched camera, write the absolute
external JSONL filename and a newline into that request file. The hook consumes
the request and captures the next frame. Repeat for courtyard, rooftop and sky
from docs/macos-port/metal-design.md. The clear-only Metal core bypasses this GL
draw hook; this is not a native Metal draw replay. Follow PLAN's process/home/
port/timeout rules for any fresh game launch.

Example (refresh after capturing each matched view):
  python3 tools/metal/observe_states.py --trace <courtyard.jsonl> \
      --trace <rooftop.jsonl> --trace <sky.jsonl> \
      --output "$HOME/Library/Application Support/CoD2-native/Metal/observed.json"

Output schema 1 groups shader labels, topology classes and recorded state only.
It cannot certify complete PSO coverage: lp64_draw_trace.h omits declarations,
strides, attachment formats and other inputs. Names never identify shader bytes;
loading must supply verified source/metadata digests and missing state before
building real pipeline keys. A synthetic shader projection can test this set's
prewarm lookup behavior without claiming original Toujane rendering coverage.

Refresh from new captures after material, state or renderer changes. Compare
provenance SHA-256 values to identify the exact input captures; no timestamps or
object addresses participate in grouping. Derived sets stay in Application
Support and must never be committed or bundled. Missing local captures exit 77.
"""

import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import sys


ROOT = Path(__file__).resolve().parents[2]
RENDER_STATES = (7, 14, 23, 27, 19, 20, 22, 168, 15, 24, 25, 171, 206)
MISSING_FIELDS = (
    "shader_source_metadata_digests", "vertex_declaration", "stream_strides",
    "target_formats_samples", "separate_alpha_207_209", "stencil",
    "depth_bias", "viewport", "transforms", "texture_identity_types",
)
# D3D9 enum zero is invalid for these fields. Legacy zero-initialized arrays
# cannot distinguish uninitialized/unused state. These are declared assumptions,
# not recovered values. Valid zero MIPFILTER=NONE and arguments=DIFFUSE remain.
RENDER_ZERO_DEFAULTS = {19: 2, 20: 1, 22: 3, 23: 4, 25: 8, 171: 1}
SAMPLER_ZERO_DEFAULTS = {1: 1, 2: 1, 3: 1, 5: 1, 6: 1, 10: 1}
NONFINITE = re.compile(r"(?<=[\[,])\s*-?(?:nan|inf)(?=\s*[,\]])")
JSON_STRING = re.compile(r'"(?:[^"\\]|\\.)*"')


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False)


def uint32(value, field):
    if type(value) is not int or not 0 <= value <= 0xffffffff:
        raise ValueError(f"{field} must be a uint32")
    return value


def array(value, rows, columns, field):
    if not isinstance(value, list) or len(value) != rows:
        raise ValueError(f"{field} must have {rows} rows")
    result = []
    for slot, row in enumerate(value):
        if not isinstance(row, list) or len(row) != columns:
            raise ValueError(f"{field}[{slot}] must have {columns} values")
        result.append([uint32(v, f"{field}[{slot}][{index}]")
                       for index, v in enumerate(row)])
    return result


def normalize(draw):
    if not isinstance(draw, dict):
        raise ValueError("draw must be an object")
    primitive = uint32(draw.get("type"), "type")
    if primitive not in range(1, 7):
        raise ValueError("type must be D3D point/line/triangle (1..6)")
    representative, topology = ((1, "point") if primitive == 1 else
                                (2, "line") if primitive <= 3 else (4, "triangle"))
    raw_states = draw.get("states")
    if not isinstance(raw_states, dict):
        raise ValueError("states must be an object")
    states = {str(state): uint32(raw_states.get(str(state)), f"states.{state}")
              for state in RENDER_STATES}
    samplers = array(draw.get("samplers"), 16, 14, "samplers")
    stages = array(draw.get("stages"), 8, 33, "stages")
    assumptions = []

    def substitute(container, index, default, field):
        if container[index] == 0:
            container[index] = default
            assumptions.append({"field": field, "captured": 0, "default": default,
                                "reason": "invalid_zero_enum_or_uninitialized_slot"})

    for state, default in RENDER_ZERO_DEFAULTS.items():
        substitute(states, str(state), default, f"states.{state}")
    for slot, sampler in enumerate(samplers):
        for state, default in SAMPLER_ZERO_DEFAULTS.items():
            substitute(sampler, state, default, f"samplers[{slot}][{state}]")
    for slot, stage in enumerate(stages):
        substitute(stage, 1, 4 if slot == 0 else 1, f"stages[{slot}][1]")
        substitute(stage, 4, 2 if slot == 0 else 1, f"stages[{slot}][4]")
    names = {}
    for field in ("vs_name", "ps_name"):
        name = draw.get(field)
        if name is not None and (not isinstance(name, str) or not name or len(name) > 1024):
            raise ValueError(f"{field} must be a nonempty shader label")
        names[field] = name
    record = {**names, "type": representative, "topology_class": topology,
              "states": states, "samplers": samplers, "stages": stages}
    return record, assumptions, primitive


def legacy_json(line):
    # Older printf traces have nan/inf in numeric arrays. Never rewrite a label
    # containing those spellings, including escaped quotes/backslashes.
    parts = []
    position = count = 0
    for match in JSON_STRING.finditer(line):
        normalized, changed = NONFINITE.subn("null", line[position:match.start()])
        parts.extend((normalized, match.group()))
        count += changed
        position = match.end()
    normalized, changed = NONFINITE.subn("null", line[position:])
    parts.append(normalized)
    return json.loads("".join(parts), parse_constant=lambda value: None), count + changed


def observe(paths):
    grouped = {}
    provenance = []
    missing = set(MISSING_FIELDS)
    draws = 0
    for path in sorted({p.expanduser().resolve() for p in paths}):
        content = path.read_bytes()
        digest = hashlib.sha256(content).hexdigest()
        source_draws = nonfinite = 0
        for number, line in enumerate(content.decode("utf-8").splitlines(), 1):
            if not line.strip():
                continue
            try:
                draw, changed = legacy_json(line)
                record, assumptions, primitive = normalize(draw)
            except (ValueError, TypeError) as error:
                raise ValueError(f"{path.name}:{number}: {error}") from error
            key = canonical(record)
            if key not in grouped:
                grouped[key] = {"record": record, "draws": 0, "types": set(),
                                "assumptions": {}, "sources": []}
            entry = grouped[key]
            entry["draws"] += 1
            entry["types"].add(primitive)
            for assumption in assumptions:
                entry["assumptions"][assumption["field"]] = assumption
            entry["sources"].append({"sha256": digest, "line": number})
            if record["vs_name"] is None or record["ps_name"] is None:
                missing.add("shader_names")
            source_draws += 1
            nonfinite += changed
        draws += source_draws
        provenance.append({"path": str(path), "sha256": digest, "draws": source_draws,
                           "legacy_nonfinite_values_normalized": nonfinite})
    if not draws:
        raise ValueError("captures contain no draws")
    records = []
    for key, entry in sorted(grouped.items()):
        record = entry["record"]
        record.update(draws=entry["draws"], primitive_types=sorted(entry["types"]),
                      default_assumptions=[entry["assumptions"][field]
                                           for field in sorted(entry["assumptions"])],
                      provenance_sha256=hashlib.sha256(
                          canonical(sorted(entry["sources"], key=canonical)).encode()).hexdigest())
        records.append(record)
    return {"schema_version": 1, "coverage": "incomplete", "shader_identity": "labels_only",
            "missing_fields": sorted(missing), "draws": draws, "records": records,
            "provenance": provenance,
            "provenance_sha256": hashlib.sha256(canonical(provenance).encode()).hexdigest(),
            "default_policy": "D3D9 invalid enum zero defaults; valid zero values retained",
            "coverage_note": "Observed subset only. Supply verified shader digests and missing "
                             "draw state before preparing real pipeline keys. GL fallback "
                             "texture IDs do not prove a sampler slot is used or unused."}


def destination_path(output):
    path = output.expanduser().absolute()
    support = Path.home() / "Library/Application Support"
    # Check the lexical and resolved paths, including existing symlinks. The
    # writer additionally opens each directory with O_NOFOLLOW against races.
    candidates = (path, path.resolve())
    for candidate in candidates:
        if (not candidate.is_relative_to(support) or candidate.is_relative_to(ROOT)
                or any(part.lower().endswith((".app", ".bundle")) for part in candidate.parts)
                or any((parent / ".git").exists() for parent in candidate.parents)):
            raise ValueError("Refusing output outside Application Support, in a repository or bundle")
    if any(parent.is_symlink() for parent in (path, *path.parents)):
        raise ValueError("Refusing symlink output path")
    relative = path.relative_to(support)
    if not relative.parts or any(part in (".", "..") for part in relative.parts):
        raise ValueError("Refusing noncanonical output path")
    return support, relative


def write_state_set(output, value):
    support, relative = destination_path(output)
    flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
    directory = os.open(support, flags)
    lock = temporary = None
    try:
        for part in relative.parts[:-1]:
            try:
                os.mkdir(part, mode=0o700, dir_fd=directory)
            except FileExistsError:
                pass
            child = os.open(part, flags, dir_fd=directory)
            os.close(directory)
            directory = child
        name = relative.name
        lock = os.open(f".{name}.lock", os.O_WRONLY | os.O_CREAT | os.O_NOFOLLOW,
                       0o600, dir_fd=directory)
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        temporary = f".{name}.{secrets.token_hex(8)}.tmp"
        descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                             0o600, dir_fd=directory)
        with os.fdopen(descriptor, "w") as file:
            json.dump(value, file, sort_keys=True, indent=2, allow_nan=False)
            file.write("\n")
            file.flush()
            os.fsync(file.fileno())
        os.replace(temporary, name, src_dir_fd=directory, dst_dir_fd=directory)
        temporary = None
        os.fsync(directory)
    finally:
        if temporary is not None:
            os.unlink(temporary, dir_fd=directory)
        if lock is not None:
            os.close(lock)
        os.close(directory)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trace", type=Path, action="append", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        destination_path(args.output)
        for path in args.trace:
            if not path.expanduser().is_file():
                print(f"SKIP: local draw capture absent: {path}")
                return 77
            if path.expanduser().resolve() == args.output.expanduser().resolve():
                raise ValueError("Refusing to overwrite an input capture")
        result = observe(args.trace)
        write_state_set(args.output, result)
    except (OSError, ValueError) as error:
        print(f"Observed-state set failed: {error}", file=sys.stderr)
        return 1
    print(f"Observed-state schema 1: draws={result['draws']} "
          f"sets={len(result['records'])} coverage=incomplete; output={args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
