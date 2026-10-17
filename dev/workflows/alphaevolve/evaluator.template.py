"""evaluator.py template: copy it to experiments/<name>/evaluator.py and fill in
every TODO with the scientist.  The scientist's spec decides the metric; this
skeleton only fixes the contract and the safety checks every experiment needs.

Contract (AlphaEvolve platform):
  evaluate() -> {"scores": [{metric, score}], "insights": [{label, text}]}
                (alpha_evolve.models types when that package is installed)
  postprocess(top_programs) -> writes the best program to results/best/<seed path>

Pipeline: build -> audit the EVOLVE-BLOCK -> one run -> score.  Any failure
returns every metric as None, with the reason as an insight, so a broken or
cheating candidate can never score.

Standalone: `python3 evaluator.py` (from the repo root, or in the container)
prints the JSON for the current tree, i.e. the seed's baseline.
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

# ---- TODO: experiment settings ------------------------------------------------
EVOLVED_SOURCE = "src/<file>.cpp"          # file with the EVOLVE-BLOCK (= seed_files)
EVOLVED_OBJECT = "build/<file>.o"          # its object file, for the disassembly audit
AUDIT_NAME_TAG = "LowPrec"                 # functions whose names contain this are audited
BUILD_CMD = ["make", "-j4", "bubble"]
EVAL_PARAMS = os.environ.get("BUBBLE_EVAL_PARAMS", "params/<name>_eval.ini")
RANKS = os.environ.get("BUBBLE_MPI_RANKS", "4")
OUTDIR = "out_eval_<name>"
RUN_TIMEOUT_S = 600

PRIMARY_METRIC = "combined_score"          # must equal experiment.yaml primary_metric
METRICS = (PRIMARY_METRIC, "accuracy", "speedup")

# Rules enforced on the EVOLVE-BLOCK source (state each one in problem_description).
FORBIDDEN_SOURCE_PATTERNS = [
    (re.compile(r"\bdouble\b"), "double"),
    (re.compile(r"\blong\s+double\b"), "long double"),
    (re.compile(r"\bfloat64_t\b"), "float64_t"),
    (re.compile(r"\b__float128\b"), "__float128"),
    (re.compile(r"\bMPI_DOUBLE\b"), "MPI_DOUBLE"),
    (re.compile(r"#\s*include"), "#include"),
]
# x86-64 scalar/packed double-precision arithmetic (SSE2 / AVX / FMA).
FORBIDDEN_ASM = re.compile(
    r"\b(v?add[sp]d|v?sub[sp]d|v?mul[sp]d|v?div[sp]d|v?sqrt[sp]d|v?min[sp]d|v?max[sp]d|"
    r"v?f[n]?m(add|sub)\d+[sp]d)\b"
)


# ---- helpers ------------------------------------------------------------------

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


def audit() -> Tuple[bool, List[Tuple[str, str]]]:
    """Source scan of every EVOLVE-BLOCK + disassembly scan of the audited functions."""
    blocks = _evolve_blocks(Path(EVOLVED_SOURCE))
    if not blocks:
        return False, [("MISSING_EVOLVE_BLOCK", f"{EVOLVED_SOURCE} has no EVOLVE-BLOCK.")]
    for idx, block in enumerate(blocks):
        clean = _strip_comments_and_strings(block)
        for regex, label in FORBIDDEN_SOURCE_PATTERNS:
            if regex.search(clean):
                return False, [("FORBIDDEN_TOKEN",
                                f"'{label}' inside EVOLVE-BLOCK #{idx + 1} of {EVOLVED_SOURCE}.")]
    if not (shutil.which("objdump") and Path(EVOLVED_OBJECT).is_file()):
        return False, [("AUDIT_UNAVAILABLE", f"objdump or {EVOLVED_OBJECT} missing: not scored.")]
    res = subprocess.run(["objdump", "-d", "-C", EVOLVED_OBJECT],
                         capture_output=True, text=True, timeout=30, check=False)
    fn = ""
    for line in res.stdout.splitlines():
        header = re.match(r"^[0-9a-fA-F]+\s+<([^>]+)>:", line)
        if header:
            fn = header.group(1)
            continue
        if AUDIT_NAME_TAG in fn:
            m = FORBIDDEN_ASM.search(line)
            if m:
                return False, [("FORBIDDEN_FP64_INSTRUCTION",
                                f"64-bit FP instruction '{m.group(1)}' in '{fn}' ({EVOLVED_OBJECT}). "
                                f"Suffix literals with 'f' and call float overloads.")]
    return True, []


def read_history(path: Path) -> Dict[str, List[float]]:
    with path.open() as f:
        rows = list(csv.DictReader(f))
    return {k: [float(r[k]) for r in rows] for k in rows[0]} if rows else {}


# ---- the method ---------------------------------------------------------------

def evaluate() -> dict:                     # TODO: rename to match experiment.yaml evaluator.method
    os.chdir(_repo_root())
    insights: List[Tuple[str, str]] = []

    # 1. build
    res = subprocess.run(BUILD_CMD, capture_output=True, text=True, check=False)
    if res.returncode != 0:
        return _fail("COMPILATION_ERROR", f"{res.stdout[-4000:]}\n{res.stderr[-4000:]}", insights)

    # 2. audit
    ok, found = audit()
    insights.extend(found)
    if not ok:
        return _result({k: None for k in METRICS}, insights)

    # 3. one run (TODO: the low-precision mode added in stage 2)
    shutil.rmtree(OUTDIR, ignore_errors=True)
    cmd = ["mpirun", "--oversubscribe", "-np", RANKS, "./bubble", EVAL_PARAMS,
           "precision=lowprec", f"outdir={OUTDIR}"] + os.environ.get("BUBBLE_EVAL_OVERRIDES", "").split()
    try:
        run = subprocess.run(cmd, capture_output=True, text=True, timeout=RUN_TIMEOUT_S, check=False)
    except subprocess.TimeoutExpired:
        return _fail("RUNTIME_TIMEOUT", f"Run exceeded {RUN_TIMEOUT_S} s.", insights)
    if run.returncode != 0:
        return _fail("RUNTIME_CRASH", f"exit {run.returncode}:\n{run.stdout[-4000:]}\n{run.stderr[-4000:]}",
                     insights)
    Path(OUTDIR, "run.log").write_text(run.stdout, encoding="utf-8")

    # 4. score.  TODO: the scientist's error measure, from the stage-1 history columns:
    #    global field error  e.g. mean over snapshots of err_l2^2 (below), final err_linf, band norm
    #    scalar QoI error    e.g. abs(h["area"][-1] - h["area_ref"][-1]) / h["area_ref"][-1]
    h = read_history(Path(OUTDIR, "history.csv")) if Path(OUTDIR, "history.csv").is_file() else {}
    if not h or "err_l2" not in h:
        return _fail("NO_HISTORY", f"{OUTDIR}/history.csv missing or without err_l2.", insights)
    if any(not math.isfinite(e) for e in h["err_l2"]):
        return _fail("NAN_OR_INF", "Non-finite error vs the FP64 shadow.", insights)
    mse = sum(e * e for e in h["err_l2"]) / len(h["err_l2"])
    accuracy = -math.log10(mse + 1.0e-30)

    # TODO: the regex for the kernel timing line the stage-1 main.cpp prints
    m = re.search(r"speedup\s+([\d.eE+-]+)x", run.stdout)
    speedup = float(m.group(1)) if m else None

    combined = accuracy                       # TODO: e.g. + w * log2(speedup) while RMS <= tol
    insights.append(("SUMMARY", f"{len(h['err_l2'])} snapshots; trajectory RMS {math.sqrt(mse):.3e}; "
                                f"speedup {speedup if speedup is not None else 'n/a'}."))
    return _result({PRIMARY_METRIC: combined, "accuracy": accuracy, "speedup": speedup}, insights)


def postprocess(top_programs: list) -> None:
    """Write the best program's files to <experiment>/results/best/<path>."""
    if not top_programs:
        logger.warning("postprocess: no programs")
        return
    best = top_programs[0]
    dest = Path(__file__).resolve().parent / "results" / "best"
    for entry in best.get("content", {}).get("files", []):
        rel, content = entry.get("path", ""), entry.get("content", "")
        if rel.startswith("src/") and content:
            out = dest / rel
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_text(content, encoding="utf-8")
    dest.mkdir(parents=True, exist_ok=True)
    (dest / "program.json").write_text(
        json.dumps({k: v for k, v in best.items() if k != "content"}, indent=2, default=str))


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    print(json.dumps(evaluate(), indent=2))
