#!/usr/bin/env python3
"""Verify explicit repairs of GL-invalid local programs against legal ARB math.

This supplements check_semantics.py; it never relabels a repaired reference as
execution of the rejected original. Every source/output stays local and external.
"""
import argparse
import json
import re
from pathlib import Path
import check_semantics as semantic


def split_operands(text):
    result, start, depth = [], 0, 0
    for index, char in enumerate(text):
        depth += char in "[{"
        depth -= char in "]}"
        if char == "," and depth == 0:
            result.append(text[start:index].strip())
            start = index + 1
    return result + [text[start:].strip()]


def lower(source):
    text = re.sub(r"#[^\n]*", "", source)
    header, text = text.strip().split("\n", 1)
    symbols = set(re.findall(r"\bTEMP\s+([^;]+);", text))
    declared = {name.strip() for group in symbols for name in group.split(",")}
    scratch, statements, repairs = [], [], []

    def temporary():
        name = f"ws32_reference_{len(scratch)}"
        if re.search(rf"\b{name}\b", source):
            raise ValueError("reference temporary collision")
        scratch.append(name)
        return name

    for statement in text.split(";"):
        statement = statement.strip()
        if not statement:
            continue
        match = re.fullmatch(r"(\w+)\s+(.*)", statement, re.S)
        if not match or match[1] in semantic.DECLARATIONS:
            statements.append(statement)
            continue
        opcode, operands = match.groups()
        prefix = []

        def absolute(match):
            if match[0] in declared:
                return match[0]
            target = temporary()
            prefix.append(f"ABS {target}, {match[1]}")
            repairs.append("undeclared_abs")
            return target

        operands = re.sub(r"\b([A-Za-z_]\w*)_abs\b", absolute, operands)
        statements.extend(prefix)
        args = split_operands(operands)
        if opcode.removesuffix("_SAT") == "DP2ADD":
            destination, first, second, third = args
            # D3D dp2add's third operand requires a replicate swizzle. The
            # licensed programs all use one; reject guessing a component.
            if not re.search(r"\.[xyzwrgba]$", third):
                raise ValueError("DP2ADD reference needs a scalar third operand")
            target = temporary()
            statements.extend([f"MUL {target}.xy, {first}, {second}",
                               f"ADD {target}.x, {target}.x, {target}.y",
                               f"ADD{'_SAT' if opcode.endswith('_SAT') else ''} {destination}, {target}.x, {third}"])
            repairs.append("dp2add")
        elif opcode == "NRM":
            destination, operand = args
            if not destination.endswith(".xyz"):
                raise ValueError("NRM reference is limited to the observed XYZ mask")
            target = temporary()
            statements.extend([f"DP3 {target}.x, {operand}, {operand}",
                               f"RSQ {target}.x, {target}.x",
                               f"MUL {destination}, {operand}, {target}.x"])
            repairs.append("nrm_xyz")
        else:
            statements.append(f"{opcode} {operands}")
    # ARB requires OPTION declarations before all register declarations.
    if scratch:
        position = 0
        while position < len(statements) and statements[position].startswith("OPTION "):
            position += 1
        statements.insert(position, "TEMP " + ",".join(scratch))
    return header + "\n" + ";\n".join(statements) + "\n", repairs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, default=Path.home() / "Library/Application Support/CoD2 Silicon/shaders")
    parser.add_argument("--evidence", type=Path, default=Path.home() / "Library/Application Support/CoD2-native-ws32/invalid")
    parser.add_argument("--translator", type=Path, default=semantic.ROOT / "build-macos/metal/arb_to_msl")
    args = parser.parse_args()
    if not args.cache.is_dir():
        print("SKIP: local licensed shader cache is absent")
        return 0
    evidence = args.evidence.resolve()
    if evidence == semantic.ROOT or semantic.ROOT in evidence.parents:
        raise ValueError("derived references must remain outside the repository")
    evidence.mkdir(parents=True, exist_ok=True)
    harness = evidence / "semantic"
    semantic.build_harness(harness)
    reports = []
    for program in sorted([*args.cache.glob("*.vsa"), *args.cache.glob("*.pse")]):
        repaired, repairs = lower(program.read_text())
        if not repairs:
            continue
        reference = evidence / ("repaired_" + program.name)
        reference.write_text(repaired)
        report = semantic.compare(program, args.translator, harness, evidence, reference)
        report["repairs"] = repairs
        report["reference_method"] = "explicit legal-ARB lowering; original Mac uploaded rejected text unchanged; not original-driver parity"
        reports.append(report)
        print(program.name, report["status"], sorted(set(repairs)), report.get("error", ""), flush=True)
    summary = {"selected": len(reports), "passed": sum(r["status"] == "pass" for r in reports),
               "results": reports, "original_driver_parity": False}
    (evidence / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"Explicit repairs: {summary['passed']}/{len(reports)}; original-driver parity remains unavailable")
    return 0 if len(reports) == 17 and summary["passed"] == 17 else 1


if __name__ == "__main__":
    raise SystemExit(main())
