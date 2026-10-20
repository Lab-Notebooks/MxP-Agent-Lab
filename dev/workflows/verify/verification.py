"""Verification of <candidate commit>.

Copy to E/<n>-verify/verification.py and fill every <...>: runs from raptorize's
plan.md, limits from the scientist. bmx make verify EXP=E/<n>-verify
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "dev/workflows/verify"))

# pylint: disable-next=wrong-import-position,import-error
from verify import Report, read_csv, rms


def main():
    """
    Build, run and check the candidate; exit 1 if any check fails
    """
    with Report(__file__) as report:
        report.make(tests=True)

        # Runs: commands in app/, {out} is the run's output directory
        base = report.run("eval", "<run, mode lowprec>")
        other = report.run("alt", "<another configuration>")

        # Limits: set by the scientist before the runs
        error = rms(read_csv(base / "<csv>")["<column>"])
        report.check("eval rms", error <= "<tol>", f"{error:.3e}")

        # Run vs run: max |other - base| over a column
        alt, ref = (read_csv(out / "<csv>")["<column>"] for out in (other, base))
        difference = max(abs(x - y) for x, y in zip(alt, ref))
        report.check("alt vs eval", len(alt) == len(ref) and difference <= "<tol>")

    sys.exit(0 if report.passed else 1)


if __name__ == "__main__":
    main()
