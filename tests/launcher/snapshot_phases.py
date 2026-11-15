"""Phase bookkeeping and screen groups for snapshots.py.

Kept apart from snapshots.py so it can be tested without building the review app
(tests/launcher/test_snapshots_script.py).
"""

# The groups `snapshots.py --group` takes. A change to one page needs only its group's captures.
GROUPS = ("home", "servers", "library", "configs", "settings", "shell")
_PREFIXES = (("home", "home"), ("servers", "servers"), ("library", "library"), ("settings", "settings"), ("about", "settings"))


def group_of(name, configs_column):
    """The group a screen belongs to, from its name and the harness listing's Config Studio column."""
    if configs_column == "configs":
        return "configs"
    for prefix, group in _PREFIXES:
        if name.startswith(prefix):
            return group
    return "shell"


def group_screens(listing, group):
    """The screens of one group, in the harness's order. `listing` is `--list` split into words."""
    if group not in GROUPS:
        raise ValueError(f"no group named {group}; the groups are {', '.join(GROUPS)}")
    return [name for name, _, column, _ in listing if group_of(name, column) == group]


class Phases:
    """Runs each phase, keeps going after a failure, and reports all of them at the end."""

    def __init__(self, write=print, after_failure=None):
        self.results = []
        self._write = write
        self._after_failure = after_failure

    def status(self, name):
        return next((status for phase, status, _ in self.results if phase == name), None)

    def phase(self, name, needs, body):
        """Runs `body` unless a phase it needs did not pass. Returns whether it passed."""
        missing = [need for need in needs if self.status(need) != "passed"]
        if missing:
            self.results.append((name, "skipped", "needs " + ", ".join(missing)))
            self._write(f"SKIP {name}: needs {', '.join(missing)}")
            return False
        try:
            body()
        except Exception as error:  # every kind of failure is reported; Ctrl-C still stops the run
            reason = " ".join(f"{type(error).__name__}: {error}".split())[:600]
            self.results.append((name, "failed", reason))
            self._write(f"FAIL {name}: {reason}")
            if self._after_failure:
                self._after_failure()
            return False
        self.results.append((name, "passed", ""))
        return True

    def report(self):
        """Writes every failure and skip again, then the summary line. Returns the exit status."""
        failed = [(name, reason) for name, status, reason in self.results if status == "failed"]
        skipped = [(name, reason) for name, status, reason in self.results if status == "skipped"]
        for name, reason in failed:
            self._write(f"FAIL {name}: {reason}")
        for name, reason in skipped:
            self._write(f"SKIP {name}: {reason}")
        summary = f"SNAPSHOTS: {len(self.results)} phases, {len(failed)} failed"
        self._write(summary + (f", {len(skipped)} skipped" if skipped else ""))
        return 1 if failed or skipped else 0
