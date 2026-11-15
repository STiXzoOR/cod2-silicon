#!/usr/bin/env python3
"""snapshots.py reports every failing phase in one run, and can capture one group of screens."""
import pathlib
import subprocess
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from snapshot_phases import GROUPS, Phases, group_of, group_screens  # noqa: E402

LISTING = [line.split() for line in """\
setup-welcome main - -
home main - pages
home-news-long main - pages
servers main - pages
servers-hosting-full main - -
library-demos main - pages
settings-display settings - pages
about settings - pages
configs-guided main configs -
review-controls main configs -
foundations gallery - -
sheet-keyboard-shortcuts main - -
""".splitlines()]


def fail(message):
    raise AssertionError(message)


class PhasesTest(unittest.TestCase):
    def setUp(self):
        self.lines = []
        self.phases = Phases(write=self.lines.append)

    def test_two_failures_are_both_reported_and_the_run_fails(self):
        ran = []
        self.phases.phase("captures", [], lambda: fail("home-dark.png looks blank"))
        self.phases.phase("minimum", [], lambda: ran.append("minimum"))
        self.phases.phase("overlays", [], lambda: subprocess.run(["false"], check=True))
        self.assertEqual(ran, ["minimum"], "a phase after a failure still runs")
        self.assertEqual(self.phases.report(), 1)
        text = "\n".join(self.lines)
        self.assertIn("FAIL captures: AssertionError: home-dark.png looks blank", text)
        self.assertIn("FAIL overlays: CalledProcessError", text)
        self.assertEqual(self.lines[-1], "SNAPSHOTS: 3 phases, 2 failed")

    def test_a_phase_that_needs_a_failed_one_is_skipped_not_run(self):
        ran = []
        self.phases.phase("captures", [], lambda: fail("no window"))
        self.phases.phase("fallback", ["captures"], lambda: ran.append("fallback"))
        self.assertEqual(ran, [])
        self.assertEqual(self.phases.report(), 1)
        self.assertIn("SKIP fallback: needs captures", self.lines)
        self.assertEqual(self.lines[-1], "SNAPSHOTS: 2 phases, 1 failed, 1 skipped")

    def test_a_clean_run_passes(self):
        self.phases.phase("captures", [], lambda: None)
        self.phases.phase("fallback", ["captures"], lambda: None)
        self.assertEqual(self.phases.report(), 0)
        self.assertEqual(self.lines, ["SNAPSHOTS: 2 phases, 0 failed"])

    def test_the_hook_runs_after_each_failure(self):
        released = []
        phases = Phases(write=self.lines.append, after_failure=lambda: released.append(1))
        phases.phase("one", [], lambda: fail("x"))
        phases.phase("two", [], lambda: None)
        phases.phase("three", [], lambda: fail("y"))
        self.assertEqual(len(released), 2)

    def test_a_long_or_multiline_reason_stays_on_one_line(self):
        self.phases.phase("captures", [], lambda: fail("first\nsecond " + "x" * 2000))
        self.assertTrue(all("\n" not in line and len(line) < 700 for line in self.lines))

    def test_ctrl_c_is_not_swallowed(self):
        def interrupt():
            raise KeyboardInterrupt
        with self.assertRaises(KeyboardInterrupt):
            self.phases.phase("captures", [], interrupt)


class GroupsTest(unittest.TestCase):
    def test_every_screen_is_in_exactly_one_group(self):
        chosen = [name for group in GROUPS for name in group_screens(LISTING, group)]
        self.assertEqual(sorted(chosen), sorted(name for name, *_ in LISTING))

    def test_groups(self):
        self.assertEqual(group_screens(LISTING, "servers"), ["servers", "servers-hosting-full"])
        self.assertEqual(group_screens(LISTING, "home"), ["home", "home-news-long"])
        self.assertEqual(group_screens(LISTING, "settings"), ["settings-display", "about"])
        self.assertEqual(group_screens(LISTING, "configs"), ["configs-guided", "review-controls"])
        self.assertEqual(group_screens(LISTING, "shell"), ["setup-welcome", "foundations", "sheet-keyboard-shortcuts"])
        self.assertEqual(group_of("library-screenshots", "-"), "library")

    def test_an_unknown_group_is_an_error(self):
        with self.assertRaises(ValueError):
            group_screens(LISTING, "pages")


if __name__ == "__main__":
    unittest.main()
