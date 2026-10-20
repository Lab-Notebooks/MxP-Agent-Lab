"""Module with implementation of the Report class and its helpers.

An experiment's verification.py (dev/workflows/verify/verification.py) is plain
Python: it builds, runs and checks the candidate through one Report.

    python3 <R>/verification.py      # or: make verify EXP=<R>

Output goes to <R>/results/verify: make logs, one directory per run, and
summary.md with one row per check. Nothing here knows what is in app/.
"""

import math
import os
import shlex
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "optimize"))

# pylint: disable-next=wrong-import-position,import-error,unused-import
from evolve import APP, read_csv

# Repository root, where `make` builds the app
ROOT = Path(__file__).resolve().parents[3]


class Report:
    """Record the checks of one verification and write their summary."""

    def __init__(self, script):
        """
        Constructor

        Parameters
        ----------
        script : path to the experiment's verification.py
        """
        self.script = Path(script).resolve()
        self.dest = self.script.parent / "results" / "verify"
        self.steps = []

        self.dest.mkdir(parents=True, exist_ok=True)

    def __enter__(self):
        return self

    def __exit__(self, kind, error, traceback):
        """
        Record an exception as a failed check, then write the summary
        """
        if isinstance(error, Exception):
            self.check("script", False, f"{kind.__name__}: {error}")

        self.write()
        return isinstance(error, Exception)

    @property
    def passed(self):
        """
        True if every check passed
        """
        return all(status == "PASS" for _, status, _ in self.steps)

    def check(self, name, passed, note=""):
        """
        Record one check

        Returns
        -------
        passed
        """
        self.steps.append((name, "PASS" if passed else "FAIL", note))
        print(f"[verify] {'PASS' if passed else 'FAIL'} {name} {note}", flush=True)
        return passed

    def make(self, tests=True):
        """
        make, then make check if tests, from the repository root
        """
        logfile = self.dest / "make.log"
        exitcode = shell(f"make -j{os.cpu_count() or 4}", logfile)
        self.check("make", exitcode == 0, os.path.relpath(logfile, ROOT))

        if tests:
            logfile = self.dest / "make_check.log"
            exitcode = shell("make check", logfile)
            self.check("make check", exitcode == 0, os.path.relpath(logfile, ROOT))

    def run(self, name, command):
        """
        Run a command in app/, with {out} = <dest>/<name>/out, logged to <dest>/<name>/run.log

        Returns
        -------
        Path of {out}
        """
        rundir = self.dest / name
        rundir.mkdir(parents=True, exist_ok=True)
        outdir = rundir / "out"

        exitcode = shell(command.format(out=outdir), rundir / "run.log", cwd=APP)
        self.check(f"run {name}", exitcode == 0, os.path.relpath(rundir, ROOT))
        return outdir

    def write(self):
        """
        Write summary.md under dest
        """
        lines = [
            "# Verification summary",
            "",
            f"Script: `{os.path.relpath(self.script, ROOT)}`  ",
            f"Overall: **{'PASS' if self.passed else 'FAIL'}**",
            "",
            "| check | status | note |",
            "|:--|:--|:--|",
        ]
        lines.extend(
            f"| {name} | {status} | {note} |" for name, status, note in self.steps
        )

        (self.dest / "summary.md").write_text("\n".join(lines) + "\n")
        print(
            f"[verify] {'PASS' if self.passed else 'FAIL'} -> {self.dest / 'summary.md'}"
        )


def shell(command, logfile, cwd="."):
    """
    Run a command without a shell, in cwd relative to the repository root, tee to logfile

    Returns
    -------
    exit code
    """
    print(f"[verify] $ {command}", flush=True)

    with open(logfile, "w", encoding="utf-8") as log:
        with subprocess.Popen(
            shlex.split(command),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            cwd=ROOT / cwd,
        ) as process:
            for line in process.stdout:
                sys.stdout.write(line)
                log.write(line)

    return process.returncode


def rms(values):
    """
    Root mean square of a list of floats
    """
    return math.sqrt(sum(value * value for value in values) / len(values))
