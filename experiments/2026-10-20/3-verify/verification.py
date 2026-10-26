"""Verification of humongous-macaw (2-optimize Gate 2) on 05e8cfa + patches/0-pin.patch.

Runs from 1-raptorize/plan.md, limits from plan.md (agent defaults: no worse than the seed).
bmx make verify EXP=experiments/2026-10-20/3-verify
"""

import sys
from pathlib import Path

import numpy
import sympy

ROOT = Path(__file__).resolve().parents[3]
sys.path[:0] = [str(ROOT / "dev/workflows/verify"), str(ROOT / "app/tools")]

# pylint: disable-next=wrong-import-position,import-error
from verify import Report, read_csv, rms

# pylint: disable-next=wrong-import-position,import-error
from plot_fields import read_frame

E = Path(__file__).resolve().parents[1]
SEED = E / "1-raptorize/results"
RUN = "mpirun {np} ./bubble params/bubble.ini precision=lowprec outdir={{out}} "
EVAL = "nx=128 ny=128 max_steps=100 plot_interval=100"


def face_values():
    """
    Seed and candidate wenoFaceValueLowPrec, transcribed from src/advect.cpp, for one sign

    Returns
    -------
    list of (seed, candidate) sympy expressions, positive then negative branch
    """
    # pylint: disable=invalid-name,too-many-locals
    s1, s2, s3, s4, s5, a1, a3 = sympy.symbols("s1:6 a1 a3")
    a2 = 1 - a1 - a3
    v1, v2, v3, v4 = s1 - s2, s2 - s3, s3 - s4, s4 - s5
    sixth = sympy.Rational(1, 6)
    seed_pos = (
        a1 * (2 * s1 - 7 * s2 + 11 * s3)
        + a2 * (-s2 + 5 * s3 + 2 * s4)
        + a3 * (2 * s3 + 5 * s4 - s5)
    ) * sixth
    seed_neg = (
        a1 * (-s1 + 5 * s2 + 2 * s3)
        + a2 * (2 * s2 + 5 * s3 - s4)
        + a3 * (11 * s3 - 7 * s4 + 2 * s5)
    ) * sixth
    cand_pos = (
        s3
        + (2 * a1 * (v1 - 2 * v2 + v3) + a3 * (v2 - 2 * v3 + v4) - v2 - 2 * v3) * sixth
    )
    cand_neg = (
        s3
        + (-a1 * (v1 - 2 * v2 + v3) + 2 * a3 * (-v2 + 2 * v3 - v4) + 2 * v2 + v3)
        * sixth
    )
    return [(seed_pos, cand_pos), (seed_neg, cand_neg)]


def drift(run, ref, column):
    """
    Relative difference of a column's last value between two history.csv
    """
    last, base = read_csv(run)[column][-1], read_csv(ref)[column][-1]
    return abs(last - base) / abs(base)


def main():
    """
    Build, run and check the candidate; exit 1 if any check fails
    """
    with Report(__file__) as report:
        report.make(tests=True)

        # Runs: commands in app/, {out} is the run's output directory
        runs = {
            "eval": RUN.format(np="-n 1") + EVAL,
            "np2": RUN.format(np="-n 2") + EVAL,
            "np4": RUN.format(np="--bind-to none -n 4") + EVAL,
            "nx256": RUN.format(np="-n 1")
            + "nx=256 ny=256 max_steps=200 plot_interval=200",
            "full": RUN.format(np="--bind-to none -n 2")
            + "nx=128 ny=128 plot_interval=1024",
        }
        out = {name: report.run(name, command) for name, command in runs.items()}

        # Limits: no worse than the seed
        seeds = {
            "eval": SEED / "lowprec",
            "nx256": E / "3-verify/results/seed_nx256",
            "full": SEED / "full_lowprec",
        }
        for name, seed in seeds.items():
            cand, ref = (
                read_csv(d / "history.csv")["err_rms"] for d in (out[name], seed)
            )
            report.check(
                f"{name} rms <= seed",
                rms(cand) <= rms(ref),
                f"{rms(cand):.3e} vs {rms(ref):.3e}",
            )
        cand, ref = (
            read_csv(d / "history.csv")["err_rms"][-1]
            for d in (out["full"], seeds["full"])
        )
        report.check("full final <= seed", cand <= ref, f"{cand:.3e} vs {ref:.3e}")

        # Conservation at t = 2 vs the FP64 run of raptorize
        for column in ("area", "mass"):
            change = drift(
                out["full"] / "history.csv", SEED / "full_fp64/history.csv", column
            )
            report.check(
                f"full {column} vs fp64 <= 1e-5", change <= 1e-5, f"{change:.2e}"
            )

        # Run vs run: dfun bit-identical across ranks, err_rms up to the MPI sum order
        ref = read_csv(out["eval"] / "history.csv")["err_rms"]
        field = read_frame(out["eval"] / "dfun_000100.pvti")[3]["dfun"]
        for name in ("np2", "np4"):
            alt = read_csv(out[name] / "history.csv")["err_rms"]
            gap = max(abs(x - y) / y for x, y in zip(alt[1:], ref[1:]))
            report.check(
                f"{name} vs eval err_rms",
                len(alt) == len(ref) and gap <= 1e-12,
                f"{gap:.1e}",
            )
            other = read_frame(out[name] / "dfun_000100.pvti")[3]["dfun"]
            report.check(
                f"{name} vs eval dfun", numpy.array_equal(other, field), "bit-identical"
            )

        # Property: same scheme in exact arithmetic
        for sign, (seed, cand) in zip(("+", "-"), face_values()):
            report.check(
                f"face value {sign} = seed (sympy)", sympy.expand(seed - cand) == 0
            )

    sys.exit(0 if report.passed else 1)


if __name__ == "__main__":
    main()
