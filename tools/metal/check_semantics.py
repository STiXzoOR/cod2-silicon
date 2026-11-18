#!/usr/bin/env python3
"""Run real headless ARB/Metal comparisons using local shaders, never repo assets."""
import argparse
import json
import platform
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_EVIDENCE = Path.home() / "Library/Application Support/CoD2-native-ws24/evidence/semantics"
DECLARATIONS = {"ATTRIB", "OUTPUT", "PARAM", "TEMP", "ADDRESS", "ALIAS", "OPTION", "END"}


def instruction_set(source):
    """Keep saturation distinct and see instructions sharing the same line."""
    source = re.sub(r"#[^\n]*", "", source)
    source = re.sub(r"!!ARB[^\n]+", "", source)
    instructions = set()
    for statement in source.split(";"):
        match = re.match(r"\s*([A-Z][A-Z0-9_]*)\b", statement)
        if match and match[1] not in DECLARATIONS:
            instructions.add(match[1])
    return tuple(sorted(instructions))


def representatives(cache):
    groups = {}
    programs = sorted([*cache.glob("*.vsa"), *cache.glob("*.pse")])
    for program in programs:
        key = (program.suffix, instruction_set(program.read_text()))
        groups.setdefault(key, []).append(program)
    return programs, groups


def run(command):
    return subprocess.run([str(part) for part in command], text=True, capture_output=True)


def build_harness(output):
    command = ["xcrun", "clang++", "-std=c++17", "-fobjc-arc", "-ffp-contract=off",
               "-Wno-deprecated-declarations", ROOT / "tests/metal/semantic.mm",
               "-framework", "Foundation", "-framework", "Metal", "-framework", "OpenGL", "-o", output]
    result = run(command)
    if result.returncode:
        raise RuntimeError("harness build failed:\n" + result.stdout + result.stderr)


def compare(program, translator, harness, evidence, reference=None):
    msl = evidence / (program.name + ".metal")
    metadata = evidence / (program.name + ".json")
    translated = run([translator, program, msl])
    if translated.returncode:
        return {"program": program.name, "status": "fail", "error": "translation failed: " + translated.stderr}
    try:
        description = json.loads(translated.stdout)
    except ValueError:
        return {"program": program.name, "status": "fail", "error": "translator returned invalid metadata: " + translated.stdout}
    metadata.write_text(json.dumps(description, indent=2) + "\n")
    execution = run([harness, reference or program, msl, metadata])
    try:
        report = json.loads(execution.stdout)
    except ValueError:
        report = {"program": program.name, "status": "fail", "error": "harness returned invalid JSON", "stdout": execution.stdout}
    if execution.stderr:
        report["stderr"] = execution.stderr
    if reference:
        report["program"] = program.name
        report["reference_program"] = reference.name
        report["reference_method"] = "independently written synthetic ARB arithmetic lowering; not the original instruction stream"
    report["returncode"] = execution.returncode
    report["instructions"] = list(instruction_set(program.read_text()))
    (evidence / (program.name + ".result.json")).write_text(json.dumps(report, indent=2) + "\n")
    return report


def self_test(translator, harness, evidence):
    """Original synthetic probes validate the test itself, including rejection."""
    cases = {
        "probe.vsa": "!!ARBvp1.0\nOUTPUT p = result.position;\nOUTPUT t = result.texcoord[0];\n"
                     "ATTRIB a = vertex.attrib[0];\nPARAM e = program.env[1];\n"
                     "MAD p, a, e, {0.1, 0.2, 0.3, 0.4};\nMOV t.yw, a;\nEND\n",
        "probe.pse": "!!ARBfp1.0\nOUTPUT c = result.color;\nATTRIB t = fragment.texcoord[0];\n"
                     "TEMP r;\nTEX r, t, texture[0], 2D;\nMUL c, r, program.env[1];\nEND\n",
        "address.vsa": "!!ARBvp1.0\nOUTPUT p = result.position;\nADDRESS a;\n"
                       "PARAM rows[4] = {program.env[0..3]};\nPARAM idx = {1.2,0,0,0};\nTEMP r;\n"
                       "ARL a.x, idx.x;\nMOV r, rows[a.x+1];\nMOV p, -r.wzyx;\nEND\n",
        "predicate.pse": "!!ARBfp1.0\nOUTPUT c = result.color;\nTEMP r,s;\n"
                         "SLT r, {-0.1,0.2,0.3,0.4}, {0,0.1,0.4,0.3};\n"
                         "SGE s, {-0.1,0.2,0.3,0.4}, {0,0.1,0.4,0.3};\nADD_SAT c, r, -s.zyxw;\nEND\n",
        "abs_swizzle.pse": "!!ARBfp1.0\nOUTPUT c = result.color;\nTEMP r;\n"
                           "ABS r, -fragment.texcoord[0].wzyx;\nMAD_SAT c, r, {-2,0.5,2,1}, {0.4,0,0,0};\nEND\n",
        "projected.pse": "!!ARBfp1.0\nOUTPUT c = result.color;\nATTRIB t = fragment.texcoord[0];\n"
                         "TEMP r,s;\nTXP r, t, texture[0], 2D;\nTEX s, t, texture[1], CUBE;\nADD c, r, s;\nEND\n",
        "local_state.pse": "!!ARBfp1.0\nOUTPUT c = result.color;\nPARAM f = state.fog.color;\n"
                           "PARAM l = program.local[3];\nADD c, f, l;\nEND\n",
        "position.pse": "!!ARBfp1.0\nOUTPUT c = result.color;\nMOV c, fragment.position;\nEND\n",
        "volume.pse": "!!ARBfp1.0\nOUTPUT c = result.color;\nATTRIB t = fragment.texcoord[0];\n"
                      "TEX c, t, texture[0], 3D;\nEND\n",
        "discard.pse": "!!ARBfp1.0\nOUTPUT c = result.color;\nKIL {-1,1,1,1};\nMOV c, {1,0,0,1};\nEND\n",
    }
    reports = []
    for name, source in cases.items():
        program = evidence / name
        program.write_text(source)
        report = compare(program, translator, harness, evidence)
        reports.append(report)
        print(f"self-test {name}: {report['status']}", flush=True)
    # GL rejects these spellings. Separate synthetic arithmetic oracles test
    # our explicit interpretation without concealing original-cache failures.
    oracles = {
        "dp2add.pse": (
            "!!ARBfp1.0\nOUTPUT c = result.color; ATTRIB a = fragment.texcoord[0]; "
            "ATTRIB b = fragment.color; DP2ADD c, a.xzyw, b, program.local[4].y; END\n",
            "!!ARBfp1.0\nOUTPUT c = result.color; ATTRIB a = fragment.texcoord[0]; "
            "ATTRIB b = fragment.color; TEMP r; MUL r.xy, a.xzyw, b; "
            "ADD r.x, r.x, r.y; ADD c, r.x, program.local[4].y; END\n"),
        "normalize.vsa": (
            "!!ARBvp1.0\nOUTPUT p = result.position; ATTRIB a = vertex.attrib[0]; "
            "TEMP r; MOV r.w, 0.7; NRM r.xyz, -a; MOV p, r; END\n",
            "!!ARBvp1.0\nOUTPUT p = result.position; ATTRIB a = vertex.attrib[0]; "
            "TEMP r,n; DP3 n.x, a, a; RSQ n.x, n.x; MUL r.xyz, -a, n.x; "
            "MOV r.w, 0.7; MOV p, r; END\n"),
        "undeclared_abs.pse": (
            "!!ARBfp1.0\nOUTPUT c = result.color; TEMP r; "
            "MOV r, -fragment.texcoord[0]; MOV c, -r_abs.wzyx; END\n",
            "!!ARBfp1.0\nOUTPUT c = result.color; TEMP r; "
            "MOV r, -fragment.texcoord[0]; ABS r, r; MOV c, -r.wzyx; END\n"),
    }
    for name, (source, lowering) in oracles.items():
        program = evidence / name
        reference = evidence / ("lowered_" + name)
        program.write_text(source)
        reference.write_text(lowering)
        report = compare(program, translator, harness, evidence, reference)
        reports.append(report)
        print(f"self-test synthetic arithmetic oracle {name}: {report['status']}", flush=True)
    # A corrupt GPU result must fail the differential comparison.
    program = evidence / "probe.pse"
    msl = evidence / "probe.pse.metal"
    source = msl.read_text()
    source, count = re.subn(r"(outputs\[id\]\s*=\s*[^;]+;)",
                            r"\1 outputs[id].color[0] += float4(0.25f);", source)
    if count != 1:
        raise RuntimeError("self-test cannot locate translated compute output assignment")
    original = msl.read_text()
    msl.write_text(source)
    try:
        corrupted = run([harness, program, msl, evidence / "probe.pse.json"])
    finally:
        msl.write_text(original)
    corrupt_report = json.loads(corrupted.stdout)
    if corrupt_report.get("status") != "fail" or not corrupt_report.get("failures"):
        raise RuntimeError("self-test failed to detect deliberate Metal output corruption")
    (evidence / "deliberate_corruption.result.json").write_text(json.dumps(corrupt_report, indent=2) + "\n")
    print("self-test deliberate GPU output corruption: detected", flush=True)
    return all(item["status"] == "pass" for item in reports)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path)
    parser.add_argument("--translator", type=Path, default=ROOT / "build-macos/metal/arb_to_msl")
    parser.add_argument("--harness", type=Path)
    parser.add_argument("--evidence", type=Path, default=DEFAULT_EVIDENCE)
    parser.add_argument("--all", action="store_true", help="compare every program, instead of one per stage/opcode set")
    parser.add_argument("--limit", type=int, help="development only; report remains marked partial")
    parser.add_argument("--self-test", action="store_true", help="run original GPU probes and a deliberate corruption check first")
    args = parser.parse_args()
    if platform.system() != "Darwin":
        print("SKIP: headless ARB/Metal semantic tests require macOS and the Apple frameworks")
        return 0
    candidates = [Path.home() / f"Library/Application Support/{name}/shaders" for name in ("CoD2x Native", "CoD2 Unbound")]
    cache = args.cache or next((path for path in candidates if path.is_dir()), None)
    programs, groups = representatives(cache) if cache and cache.is_dir() else ([], {})
    if not programs and not args.self_test:
        print("SKIP: local shader cache has no .vsa or .pse programs")
        return 0
    if not args.translator.is_file():
        raise RuntimeError(f"translator unavailable: {args.translator}; build the standalone Metal translator first")
    evidence = args.evidence.expanduser().resolve()
    if evidence == ROOT or ROOT in evidence.parents:
        raise RuntimeError("derived shaders/results must be outside the repository (PLAN rule 9)")
    evidence.mkdir(parents=True, exist_ok=True)
    harness = args.harness or evidence / "semantic"
    if args.harness is None:
        build_harness(harness)
    if args.self_test and not self_test(args.translator, harness, evidence):
        raise RuntimeError("original synthetic GPU self-test failed")
    if not programs:
        print("SKIP: local extracted shader cache absent; original synthetic GPU probes completed")
        return 0
    selected = programs if args.all else sorted(group[0] for group in groups.values())
    if args.limit is not None:
        selected = selected[:args.limit]
    normalized = {tuple(sorted({op.removesuffix("_SAT") for op in instruction_set(p.read_text())})) for p in programs}
    raw = {instruction_set(p.read_text()) for p in programs}
    print(f"cache: {len(programs)} programs, {len(raw)} raw opcode sets, {len(normalized)} normalized sets, {len(groups)} stage/opcode sets", flush=True)
    reports = []
    for program in selected:
        report = compare(program, args.translator, harness, evidence)
        reports.append(report)
        detail = report.get("error", f"abs={report.get('max_abs', 0):.8g} rel={report.get('max_rel', 0):.8g} components={report.get('comparisons', 0)} failures={len(report.get('failures', []))}")
        print(f"{program.name}: {report['status']} {detail}", flush=True)
    summary = {"programs_in_cache": len(programs), "raw_opcode_sets": len(raw), "normalized_opcode_sets": len(normalized),
               "stage_opcode_sets": len(groups), "selected": len(selected), "all_programs": args.all,
               "partial": args.limit is not None, "passed": sum(r["status"] == "pass" for r in reports),
               "failed": sum(r["status"] != "pass" for r in reports), "results": reports,
               "benchmark": False, "method": "eight deterministic inputs; identical RGBA32F nearest textures; vertex output relay; original FP instructions"}
    (evidence / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"semantic comparisons: {summary['passed']}/{len(selected)} passed; failures={summary['failed']}; evidence={evidence}")
    return 1 if summary["failed"] else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
