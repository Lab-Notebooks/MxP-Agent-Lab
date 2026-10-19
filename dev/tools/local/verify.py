"""Module with implementation of the Verification class.

    python3 dev/tools/local/verify.py <spec.yaml>     # or: make verify SPEC=<spec.yaml>

The spec format is dev/workflows/verify/verification.template.yaml. Output goes to
<dest> = <spec dir>/results/verify. In order:

    1. make, then make check if tests: true (from the repo root)
    2. each run: its cmd in app/, with {out} = <dest>/<name>/out (absolute) and
       output in <dest>/<name>/run.log; every metric is then read from its output
       and checked against the criteria
    3. each compare: max |run - ref| over CSV columns, against its max
    4. each check: a command from the repo root that passes with exit 0

Writes <dest>/metrics.json and <dest>/summary.md, exits 1 if any step failed.
Run from the repo root. Nothing here knows what is in app/.
"""

import json
import math
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

import yaml

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from evolve import APP, read_csv  # pylint: disable=wrong-import-position


class Verification:
    """Run the steps of a verification spec and record their status."""

    def __init__(self, specfile):
        """
        Constructor

        Parameters
        ----------
        specfile : path to the verification spec (yaml)
        """
        with open(specfile, encoding="utf-8") as stream:
            self.spec = yaml.safe_load(stream)

        self.specfile = specfile
        self.dest = Path(specfile).resolve().parent / "results" / "verify"
        self.steps = []
        self.metrics = {}
        self.outdirs = {}

        self.dest.mkdir(parents=True, exist_ok=True)

    def record(self, step, passed, note=""):
        """
        Record the status of one step, a path note is shown relative to the repo root
        """
        if isinstance(note, Path):
            note = os.path.relpath(note)

        self.steps.append((step, "PASS" if passed else "FAIL", note))

    @property
    def passed(self):
        """
        True if every recorded step passed
        """
        return all(status == "PASS" for _, status, _ in self.steps)

    def build(self):
        """
        make, then make check if the spec asks for tests
        """
        logfile = self.dest / "make.log"
        self.record(
            "make", shell(f"make -j{os.cpu_count() or 4}", logfile) == 0, logfile
        )

        if self.spec.get("tests", True):
            logfile = self.dest / "make_check.log"
            self.record("make check", shell("make check", logfile) == 0, logfile)

    def runs(self):
        """
        Execute each run, read its metrics and check them against the criteria
        """
        for run in self.spec.get("runs", []):
            rundir = self.dest / run["name"]
            rundir.mkdir(parents=True, exist_ok=True)
            outdir, logfile = rundir / "out", rundir / "run.log"

            if shell(run["cmd"].format(out=outdir), logfile, cwd=APP) != 0:
                self.record(f"run {run['name']}", False, logfile)
                continue

            self.outdirs[run["name"]] = outdir
            self.metrics[run["name"]] = measure(
                self.spec.get("metrics", {}), outdir, logfile
            )

            criteria = run.get("criteria", self.spec.get("criteria", {}))
            messages = failures(self.metrics[run["name"]], criteria)
            self.record(
                f"run {run['name']}", not messages, "; ".join(messages) or logfile
            )

    def compares(self):
        """
        Max |run - ref| over the CSV columns of each compare entry
        """
        for entry in self.spec.get("compare", []):
            try:
                run = read_csv(self.outdirs[entry["run"]] / entry["csv"])
                ref = read_csv(self.outdirs[entry["ref"]] / entry["csv"])
                difference = max_difference(run, ref, entry["columns"])

            except (OSError, KeyError, ValueError) as error:
                print(f"[verify.compare] {entry['name']}: {error}")
                difference = math.nan

            self.metrics[f"compare {entry['name']}"] = {"max_abs_diff": difference}

            note = (
                f"max abs({entry['run']} - {entry['ref']}) = {difference:.3e} "
                + f"(max {entry['max']})"
            )
            self.record(f"compare {entry['name']}", difference <= entry["max"], note)

    def checks(self):
        """
        Run each check command, pass is exit 0
        """
        for check in self.spec.get("checks", []):
            logfile = self.dest / f"check_{check['name']}.log"
            self.record(
                f"check {check['name']}", shell(check["cmd"], logfile) == 0, logfile
            )

    def write(self):
        """
        Write metrics.json and summary.md under dest
        """
        lines = [
            "# Verification summary",
            "",
            f"Candidate: {self.spec.get('candidate', '?')}  ",
            f"Spec: `{self.specfile}`  ",
            f"Overall: **{'PASS' if self.passed else 'FAIL'}**",
            "",
            "| step | status | note |",
            "|:--|:--|:--|",
        ]
        lines.extend(
            f"| {step} | {status} | {note} |" for step, status, note in self.steps
        )

        (self.dest / "metrics.json").write_text(json.dumps(self.metrics, indent=2))
        (self.dest / "summary.md").write_text("\n".join(lines) + "\n")

        print(
            f"[verify] {'PASS' if self.passed else 'FAIL'} -> {self.dest / 'summary.md'}"
        )


def shell(command, logfile, cwd=None):
    """
    Run a command without a shell, tee stdout + stderr to logfile, in cwd

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
            cwd=cwd,
        ) as process:
            for line in process.stdout:
                sys.stdout.write(line)
                log.write(line)

    return process.returncode


def measure(metrics, outdir, logfile):
    """
    Read the metrics of one run

    Parameters
    ----------
    metrics : dictionary {name : {csv, column, reduce} or {log}} from the spec
    outdir  : output directory of the run
    logfile : run.log of the run

    Returns
    -------
    dictionary {name : value}, NaN when a metric cannot be read
    """
    values = {}

    for name, metric in metrics.items():
        try:
            if "csv" in metric:
                column = read_csv(outdir / metric["csv"])[metric["column"]]
                values[name] = reduce_values(column, metric.get("reduce", "last"))
            else:
                matches = re.findall(metric["log"], logfile.read_text(encoding="utf-8"))
                values[name] = float(matches[-1])

        except (OSError, KeyError, IndexError, ValueError, ZeroDivisionError):
            values[name] = math.nan

    return values


def reduce_values(values, how):
    """
    Reduce a column to one number

    Parameters
    ----------
    values : list of floats
    how    : 'last', 'maxabs', 'mean' or 'rms'
    """
    if how == "last":
        return values[-1]

    if how == "maxabs":
        return max(abs(value) for value in values)

    if how == "mean":
        return sum(values) / len(values)

    if how == "rms":
        return math.sqrt(sum(value * value for value in values) / len(values))

    raise ValueError(f"[verify.reduce_values] unknown reduce '{how}'")


def failures(values, criteria):
    """
    Check values against criteria <metric>_max / <metric>_min

    Returns
    -------
    list of failure messages, a missing or NaN metric fails
    """
    messages = []

    for key, limit in criteria.items():
        metric, bound = key.rsplit("_", 1)
        value = values.get(metric, math.nan)

        if (
            math.isnan(value)
            or (bound == "max" and value > limit)
            or (bound == "min" and value < limit)
        ):
            messages.append(f"{metric}={value:.3e} ({bound} {limit})")

    return messages


def max_difference(run, ref, columns):
    """
    Max |run - ref| over columns of two CSV tables of the same length
    """
    difference = 0.0

    for column in columns:
        if len(run[column]) != len(ref[column]):
            raise ValueError(f"column '{column}' differs in length")

        for value, reference in zip(run[column], ref[column]):
            difference = max(difference, abs(value - reference))

    return difference


def main(specfile):
    """
    Run every step of the spec

    Returns
    -------
    0 if every step passed, else 1
    """
    verification = Verification(specfile)

    verification.build()
    verification.runs()
    verification.compares()
    verification.checks()
    verification.write()

    return 0 if verification.passed else 1


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)

    sys.exit(main(sys.argv[1]))
