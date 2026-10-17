"""AlphaEvolve evaluator: reduced-precision WENO5 advection (src/advect.cpp).

Built by the evaluator workflow (dev/workflows/evaluator/) from
assets/alphaevolve-demo/experiment/evaluator.py, simplified to the single-build
workflow of the mini-app; run by the generic dev/workflows/alphaevolve/run.py:

  1. `make`                         one build (advect.cpp through clang + RAPTOR)
  2. precision audit                no 64-bit FP in the EVOLVE-BLOCK source, and no
                                    64-bit FP arithmetic in the disassembly of the
                                    *LowPrec functions in build/advect.o
  3. one run                        ./bubble params/bubble_eval.ini precision=lowprec
                                    -- the binary carries its own FP64 shadow
                                    trajectory, so there is no reference run to
                                    cache and no .pvti reassembly
  4. score                          from <outdir>/history.csv (err_l2 per snapshot)
                                    and the run log (advection wall times)

Scores:
  trajectory_accuracy_score  = -log10(mean_snapshots(err_l2^2) + 1e-30)
  advect_speedup             = t_advect(fp64) / t_advect(lowprec), same run
  combined_score (primary)   = accuracy + SPEED_WEIGHT * log2(speedup), where the
                               speed term only counts while the trajectory RMS
                               error is <= RMS_TOL (so a fast-but-wrong kernel
                               cannot win on speed)

A plain float port already reaches RMS ~1e-9 (accuracy ~18), so accuracy is
mostly float-limited and the speed term is where the headroom is.

Runs standalone (`python3 <this file>` prints JSON) when the `alpha_evolve`
package is not installed.

Environment:
  BUBBLE_ROOT          repo root (default: cwd if it has a Makefile, else the first
                       parent of this file with a Makefile and src/)
  BUBBLE_MPI_RANKS     MPI ranks (default 4)
  BUBBLE_EVAL_PARAMS   parameter file (default params/bubble_eval.ini)
  BUBBLE_EVAL_OVERRIDES  extra key=value overrides, space separated
  BUBBLE_SPEED_WEIGHT  weight of log2(speedup) in combined_score (default 2)
  BUBBLE_RMS_TOL       trajectory RMS above which speed is not rewarded (default 1e-6)
"""

import csv
import json
import logging
import math
import os
import re
import shutil
import subprocess
from pathlib import Path
from typing import Dict, List, Optional, Tuple

try:
    from alpha_evolve.models import (
        AlphaEvolveEvaluationInsight,
        AlphaEvolveEvaluationInsights,
        AlphaEvolveEvaluationScore,
        AlphaEvolveEvaluationScores,
        AlphaEvolveProgramEvaluation,
    )

    def _result(scores: Dict[str, Optional[float]], insights: List[Tuple[str, str]]) -> dict:
        return AlphaEvolveProgramEvaluation(
            scores=AlphaEvolveEvaluationScores(
                scores=[AlphaEvolveEvaluationScore(metric=k, score=v) for k, v in scores.items()]
            ),
            insights=AlphaEvolveEvaluationInsights(
                insights=[AlphaEvolveEvaluationInsight(label=l, text=t) for l, t in insights]
            ),
        ).model_dump()

except ImportError:  # standalone use

    def _result(scores: Dict[str, Optional[float]], insights: List[Tuple[str, str]]) -> dict:
        return {
            "scores": [{"metric": k, "score": v} for k, v in scores.items()],
            "insights": [{"label": l, "text": t} for l, t in insights],
        }


logger = logging.getLogger(__name__)

PRIMARY_METRIC = "combined_score"
ACCURACY_METRIC = "trajectory_accuracy_score"
SPEED_METRIC = "advect_speedup"
METRICS = (PRIMARY_METRIC, ACCURACY_METRIC, SPEED_METRIC)
EVOLVED_SOURCE = "src/advect.cpp"
EVOLVED_OBJECT = "build/advect.o"
OUTDIR = "out_eval"
RUN_TIMEOUT_S = 600

FORBIDDEN_SOURCE_PATTERNS = [
    (re.compile(r"\bdouble\b"), "double"),
    (re.compile(r"\bfloat64_t\b"), "float64_t"),
    (re.compile(r"\blong\s+double\b"), "long double"),
    (re.compile(r"\b__float128\b"), "__float128"),
    (re.compile(r"\bMPI_DOUBLE\b"), "MPI_DOUBLE"),
    (re.compile(r"#\s*include"), "#include"),
]

# x86-64 scalar/packed double-precision arithmetic (SSE2 / AVX / FMA).
FORBIDDEN_ASM = re.compile(
    r"\b(v?add[sp]d|v?sub[sp]d|v?mul[sp]d|v?div[sp]d|v?sqrt[sp]d|v?min[sp]d|v?max[sp]d|"
    r"v?f[n]?m(add|sub)\d+[sp]d)\b"
)


def _fail(label: str, text: str, insights: List[Tuple[str, str]]) -> dict:
    insights.append((label, text))
    return _result({k: None for k in METRICS}, insights)


def _repo_root() -> Path:
    env = os.environ.get("BUBBLE_ROOT")
    if env:
        return Path(env).resolve()
    for d in [Path.cwd()] + list(Path(__file__).resolve().parents):
        if (d / "Makefile").is_file() and (d / "src").is_dir():
            return d
    return Path.cwd()


def _strip_comments_and_strings(code: str) -> str:
    code = re.sub(r"//.*", "", code)
    code = re.sub(r"/\*.*?\*/", "", code, flags=re.DOTALL)
    return re.sub(r'"(?:\\.|[^"\\])*"', '""', code)


def _evolve_blocks(path: Path) -> List[str]:
    pattern = re.compile(
        r"^// EVOLVE-BLOCK-START\s*\n(.*?)^// EVOLVE-BLOCK-END\s*$", re.MULTILINE | re.DOTALL
    )
    return [m.group(1) for m in pattern.finditer(path.read_text(encoding="utf-8"))]


def audit_precision() -> Tuple[bool, List[Tuple[str, str]], Dict[str, int]]:
    """Source + disassembly audit of the EVOLVE-BLOCK.  Returns (ok, insights, counts)."""
    insights: List[Tuple[str, str]] = []
    counts = {"fp16_tokens": 0, "fp32_tokens": 0}

    blocks = _evolve_blocks(Path(EVOLVED_SOURCE))
    if not blocks:
        return False, [("MISSING_EVOLVE_BLOCK", f"{EVOLVED_SOURCE} has no EVOLVE-BLOCK.")], counts
    for idx, block in enumerate(blocks):
        clean = _strip_comments_and_strings(block)
        for regex, label in FORBIDDEN_SOURCE_PATTERNS:
            if regex.search(clean):
                return False, [(
                    "FORBIDDEN_TOKEN",
                    f"'{label}' inside EVOLVE-BLOCK #{idx + 1} of {EVOLVED_SOURCE}. Only float and "
                    f"_Float16 floating point (and no #include) are allowed there.",
                )], counts
        counts["fp16_tokens"] += len(re.findall(r"\b_Float16\b", clean))
        counts["fp32_tokens"] += len(re.findall(r"\bfloat\b", clean))

    if shutil.which("objdump") and Path(EVOLVED_OBJECT).is_file():
        res = subprocess.run(["objdump", "-d", "-C", EVOLVED_OBJECT],
                             capture_output=True, text=True, timeout=30, check=False)
        fn = ""
        for line in res.stdout.splitlines():
            header = re.match(r"^[0-9a-fA-F]+\s+<([^>]+)>:", line)
            if header:
                fn = header.group(1)
                continue
            if "LowPrec" in fn:
                m = FORBIDDEN_ASM.search(line)
                if m:
                    return False, [(
                        "FORBIDDEN_FP64_INSTRUCTION",
                        f"64-bit FP instruction '{m.group(1)}' in '{fn}' ({EVOLVED_OBJECT}). "
                        f"Suffix literals with 'f' and call float overloads (std::sqrt(float)).",
                    )], counts
    else:
        return False, [("AUDIT_UNAVAILABLE",
                        "objdump or build/advect.o missing: the disassembly audit cannot run, "
                        "so the candidate is not scored.")], counts
    return True, insights, counts


def read_history(path: Path) -> Dict[str, List[float]]:
    with path.open() as f:
        rows = list(csv.DictReader(f))
    return {k: [float(r[k]) for r in rows] for k in rows[0]} if rows else {}


def parse_log(text: str) -> Dict[str, float]:
    out: Dict[str, float] = {}
    m = re.search(r"advect time\s*:\s*\S+\s+([\d.eE+-]+) s\s+fp64\s+([\d.eE+-]+) s", text)
    if m:
        out["t_lowprec"], out["t_fp64"] = float(m.group(1)), float(m.group(2))
    m = re.search(r"bubble: area\s*:.*drift\s+([-\d.eE+]+)%", text)
    if m:
        out["area_drift"] = float(m.group(1))
    m = re.search(r"area \(fp64\)\s*:\s*drift\s+([-\d.eE+]+)%", text)
    if m:
        out["area_drift_ref"] = float(m.group(1))
    return out


def evaluate_bubble_mxp() -> dict:
    os.chdir(_repo_root())
    insights: List[Tuple[str, str]] = []

    # 1. build
    res = subprocess.run(["make", "-j4", "bubble"], capture_output=True, text=True, check=False)
    if res.returncode != 0:
        return _fail("COMPILATION_ERROR", f"{res.stdout[-4000:]}\n{res.stderr[-4000:]}", insights)

    # 2. audit
    ok, audit, counts = audit_precision()
    insights.extend(audit)
    if not ok:
        return _result({k: None for k in METRICS}, insights)

    # 3. one run: lowprec advection + FP64 shadow
    params = os.environ.get("BUBBLE_EVAL_PARAMS", "params/bubble_eval.ini")
    overrides = os.environ.get("BUBBLE_EVAL_OVERRIDES", "").split()
    shutil.rmtree(OUTDIR, ignore_errors=True)
    cmd = ["mpirun", "--oversubscribe", "-np", os.environ.get("BUBBLE_MPI_RANKS", "4"),
           "./bubble", params, "precision=lowprec", f"outdir={OUTDIR}"] + overrides
    try:
        run = subprocess.run(cmd, capture_output=True, text=True, timeout=RUN_TIMEOUT_S, check=False)
    except subprocess.TimeoutExpired:
        return _fail("RUNTIME_TIMEOUT", f"Run exceeded {RUN_TIMEOUT_S} s.", insights)
    if run.returncode != 0:
        return _fail("RUNTIME_CRASH", f"exit {run.returncode}:\n{run.stdout[-4000:]}\n{run.stderr[-4000:]}",
                     insights)
    Path(OUTDIR, "run.log").write_text(run.stdout, encoding="utf-8")

    # 4. score
    hist_path = Path(OUTDIR, "history.csv")
    h = read_history(hist_path) if hist_path.is_file() else {}
    if not h or "err_l2" not in h:
        return _fail("NO_HISTORY", f"{hist_path} missing or without err_l2 column.", insights)
    errs = h["err_l2"] + h["err_linf"]
    if any(math.isnan(e) or math.isinf(e) for e in errs):
        bad = next(i for i, e in enumerate(h["err_linf"]) if math.isnan(e) or math.isinf(e))
        return _fail("NAN_OR_INF", f"Non-finite error at step {int(h['step'][bad])} "
                     f"(t={h['time'][bad]:.4f}); check eps underflow / division in WENO weights.",
                     insights)

    traj_mse = sum(e * e for e in h["err_l2"]) / len(h["err_l2"])
    band_mse = sum(e * e for e in h["err_band_l2"]) / len(h["err_band_l2"])
    accuracy = -math.log10(traj_mse + 1.0e-30)
    log = parse_log(run.stdout)
    speedup = (log["t_fp64"] / log["t_lowprec"]) if log.get("t_lowprec") else None
    rms_tol = float(os.environ.get("BUBBLE_RMS_TOL", "1e-6"))
    weight = float(os.environ.get("BUBBLE_SPEED_WEIGHT", "2"))
    speed_counts = speedup is not None and speedup > 0.0 and math.sqrt(traj_mse) <= rms_tol
    combined = accuracy + (weight * math.log2(speedup) if speed_counts else 0.0)
    if not speed_counts:
        insights.append(("SPEED_NOT_REWARDED",
                         f"Trajectory RMS {math.sqrt(traj_mse):.3e} > tolerance {rms_tol:.1e} (or no "
                         f"timing): combined_score = accuracy only. Fix accuracy first."))
    worst = max(range(len(h["err_linf"])), key=lambda i: h["err_linf"][i])

    drift = ""
    if "area_drift" in log and "area_drift_ref" in log:
        drift = (f"Area drift lowprec={log['area_drift']:.4f}% vs fp64={log['area_drift_ref']:.4f}% "
                 f"(delta {log['area_drift'] - log['area_drift_ref']:+.4f} pp). ")
    timing = ""
    if speedup is not None:
        timing = (f"Advection wall time lowprec={log['t_lowprec']:.3f}s vs fp64={log['t_fp64']:.3f}s "
                  f"(speedup {speedup:.3f}x, includes fp64<->float conversion). ")
    insights.append((
        "TRAJECTORY_SUMMARY",
        f"{len(h['step'])} snapshots to step {int(h['step'][-1])} (t={h['time'][-1]:.4f}). "
        f"Trajectory MSE={traj_mse:.6e} (RMS {math.sqrt(traj_mse):.6e}); interface-band MSE="
        f"{band_mse:.6e}; final RMS={h['err_l2'][-1]:.6e}; peak Linf={h['err_linf'][worst]:.6e} at "
        f"step {int(h['step'][worst])}. {drift}{timing}"
        f"EVOLVE-BLOCK tokens: _Float16={counts['fp16_tokens']}, float={counts['fp32_tokens']}.",
    ))
    return _result({PRIMARY_METRIC: combined, ACCURACY_METRIC: accuracy, SPEED_METRIC: speedup},
                   insights)


def postprocess(top_programs: list) -> None:
    """Write the best program's evolved files to <experiment>/results/best/.

    Apply with `python3 dev/workflows/alphaevolve/run.py apply <experiment>`.
    """
    if not top_programs:
        logger.warning("postprocess: no programs")
        return
    best = top_programs[0]
    dest = Path(__file__).resolve().parent / "results" / "best"
    written = []
    for entry in best.get("content", {}).get("files", []):
        rel, content = entry.get("path", ""), entry.get("content", "")
        if rel.startswith("src/") and content:
            out = dest / rel
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_text(content, encoding="utf-8")
            written.append(str(out))
    (dest / "program.json").parent.mkdir(parents=True, exist_ok=True)
    (dest / "program.json").write_text(
        json.dumps({k: v for k, v in best.items() if k != "content"}, indent=2, default=str))
    logger.info("postprocess: best program %s -> %s", best.get("name", "?"), written)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    print(json.dumps(evaluate_bubble_mxp(), indent=2))
