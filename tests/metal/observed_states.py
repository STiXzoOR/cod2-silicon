"""Synthetic JSONL observations; no licensed shaders or game data required."""

import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/metal/observe_states.py"
SUPPORT = Path.home() / "Library/Application Support"


def draw(primitive=4):
    return {
        "draw": 0, "type": primitive, "primitives": 1,
        "vs_name": "synthetic.vsa", "ps_name": "synthetic.pse",
        "vs": "0x1", "ps": "0x2", "material": "synthetic",
        "states": {"7": 1, "14": 1, "23": 4, "27": 0, "19": 2,
                   "20": 1, "22": 3, "168": 15, "15": 0, "24": 0,
                   "25": 8, "171": 1, "206": 0},
        "samplers": [[0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0]
                     for _ in range(16)],
        "stages": [[0] * 33 for _ in range(8)],
    }


class ObservedStates(unittest.TestCase):
    def setUp(self):
        base = SUPPORT / "CoD2-native-ws31/tests"
        base.mkdir(parents=True, exist_ok=True)
        self.directory = tempfile.TemporaryDirectory(prefix="observations-", dir=base)
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name)

    def capture(self, name, records):
        path = self.path / name
        path.write_text("".join(json.dumps(r) + "\n" for r in records))
        return path

    def run_tool(self, traces, output):
        self.assertTrue(TOOL.is_file(), "observed-state tool has not been implemented")
        args = [sys.executable, str(TOOL), "--output", str(output)]
        for trace in traces:
            args.extend(["--trace", str(trace)])
        return subprocess.run(args, capture_output=True, text=True)

    def observe(self, traces):
        output = self.path / "observed.json"
        run = self.run_tool(traces, output)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        return json.loads(output.read_bytes()), output.read_bytes()

    def test_deduplication_topology_and_deterministic_output(self):
        records = [draw(t) for t in (1, 2, 3, 4, 5, 6, 4)]
        records[-1].update(vs="0xabcd", ps="0xef01", material="ignored", draw=100)
        first = self.capture("a.jsonl", records)
        other = draw()
        other["ps_name"] = "other-synthetic.pse"
        second = self.capture("b.jsonl", [other])
        result, data = self.observe([first, second])
        repeated, repeated_data = self.observe([second, first, first])
        self.assertEqual(data, repeated_data)
        self.assertEqual(result["schema_version"], 1)
        self.assertEqual(result["coverage"], "incomplete")
        self.assertEqual(result["draws"], 8)
        self.assertEqual(len(result["records"]), 4)
        sample = [r for r in result["records"] if r["ps_name"] == "synthetic.pse"]
        self.assertEqual({r["type"]: r["draws"] for r in sample}, {1: 1, 2: 2, 4: 4})
        self.assertEqual({r["topology_class"] for r in sample}, {"point", "line", "triangle"})
        self.assertEqual(result["provenance"][0]["sha256"], hashlib.sha256(first.read_bytes()).hexdigest())
        self.assertEqual(len(repeated["provenance"]), 2)
        self.assertNotIn("shader_sha256", data.decode())
        self.assertNotIn("0xabcd", data.decode())

    def test_invalid_enum_zero_defaults_are_explicit(self):
        record = draw()
        record["states"].update({"7": 0, "19": 0, "23": 0})
        record["samplers"][15] = [0] * 14
        result, _ = self.observe([self.capture("zero.jsonl", [record])])
        state = result["records"][0]
        self.assertEqual(state["states"]["7"], 0)
        self.assertEqual(state["states"]["19"], 2)
        self.assertEqual(state["states"]["23"], 4)
        self.assertEqual(state["samplers"][15], [0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0])
        self.assertEqual(state["stages"][0][1], 4)
        self.assertEqual(state["stages"][0][4], 2)
        self.assertEqual(state["stages"][1][1], 1)
        self.assertEqual(state["stages"][1][4], 1)
        self.assertEqual(state["stages"][0][0], 0)
        self.assertEqual(state["stages"][0][2], 0)
        assumptions = {a["field"]: a["default"] for a in state["default_assumptions"]}
        self.assertEqual(assumptions["samplers[15][5]"], 1)
        self.assertEqual(assumptions["states.19"], 2)
        self.assertIn("vertex_declaration", result["missing_fields"])
        self.assertIn("separate_alpha_207_209", result["missing_fields"])

    def test_missing_shader_names_are_not_invented(self):
        record = draw()
        del record["vs_name"]
        del record["ps_name"]
        result, _ = self.observe([self.capture("legacy.jsonl", [record])])
        self.assertIsNone(result["records"][0]["vs_name"])
        self.assertIsNone(result["records"][0]["ps_name"])
        self.assertIn("shader_names", result["missing_fields"])

    def test_legacy_nonfinite_constants_do_not_rewrite_names(self):
        record = draw()
        record["vs_name"] = "synthetic[nan].vsa"
        record["vs_constants"] = [None, None, None]
        trace = self.capture("nonfinite.jsonl", [record])
        trace.write_text(trace.read_text().replace("[null, null, null]", "[nan, -inf, inf]"))
        result, _ = self.observe([trace])
        self.assertEqual(result["records"][0]["vs_name"], "synthetic[nan].vsa")
        self.assertEqual(result["provenance"][0]["legacy_nonfinite_values_normalized"], 3)

    def test_invalid_shapes_and_state_values_fail_before_writing(self):
        cases = []
        for name, field, value in [("sampler-count", "samplers", [[0] * 14]),
                                   ("stage-count", "stages", [[0] * 33]),
                                   ("topology", "type", 7)]:
            record = draw()
            record[field] = value
            cases.append((name, record))
        record = draw()
        record["samplers"][0].append(0)
        cases.append(("sampler-width", record))
        record = draw()
        record["stages"][0].pop()
        cases.append(("stage-width", record))
        for invalid in (-1, 4294967296, True, 1.5):
            record = draw()
            record["states"]["7"] = invalid
            cases.append((f"value-{invalid}", record))
        record = draw()
        del record["states"]["7"]
        cases.append(("missing-render", record))
        for name, record in cases:
            with self.subTest(name=name):
                output = self.path / f"{name}.json"
                run = self.run_tool([self.capture(f"{name}.jsonl", [record])], output)
                self.assertEqual(run.returncode, 1, run.stdout + run.stderr)
                self.assertFalse(output.exists())

    def test_repository_bundle_and_symlink_escape_outputs_are_refused(self):
        trace = self.capture("synthetic.jsonl", [draw()])
        escaped = self.path / "escape"
        escaped.symlink_to(ROOT, target_is_directory=True)
        bundle = self.path / "Test.app/Contents/Resources/observed.json"
        destinations = [ROOT / "output/refused-state-observations.json",
                        self.path / "outside/../../../../../../tmp/refused-state-observations.json",
                        escaped / "output/refused-state-observations.json", bundle,
                        self.path / "Test.bundle/Resources/observed.json"]
        outside = self.path / "outside.json"
        link = self.path / "linked.json"
        link.symlink_to(outside)
        destinations.append(link)
        for destination in destinations:
            with self.subTest(destination=str(destination)):
                run = self.run_tool([trace], destination)
                self.assertEqual(run.returncode, 1, run.stdout + run.stderr)
                self.assertIn("Refusing", run.stderr)
        self.assertFalse(outside.exists())
        self.assertFalse(bundle.parent.exists())

    def test_missing_capture_skips_cleanly(self):
        output = self.path / "absent.json"
        run = self.run_tool([self.path / "absent.jsonl"], output)
        self.assertEqual(run.returncode, 77)
        self.assertIn("SKIP", run.stdout)
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
