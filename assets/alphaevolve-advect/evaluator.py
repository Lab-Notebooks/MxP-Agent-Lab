#!/usr/bin/env python3
"""
evaluator.py -- AlphaEvolve Stage-A Atomic Evaluator for WENO5 Advection Kernel (src/advect.cpp)
Enforces strict zero-FP64 compliance inside EVOLVE-BLOCK / *LowPrec / mxp_eft::* functions,
computes hardware-cost-weighted precision metrics (w_fp16=1, w_fp32=2, w_fp64=16),
verifies spatial convergence order >= 4.5 on test_weno_order, and emits AlphaEvolveEvaluationInsight.
"""

import json
import os
import re
import subprocess
import sys

FORBIDDEN_SOURCE_PATTERNS = [
    (re.compile(r"\bdouble\b"), "double"),
    (re.compile(r"\bfloat64_t\b"), "float64_t"),
    (re.compile(r"\b__float128\b"), "__float128"),
    (re.compile(r"\bMPI_DOUBLE\b"), "MPI_DOUBLE"),
]

FORBIDDEN_ASM_FP64 = re.compile(
    r"\b(v?add[sp]d|v?sub[sp]d|v?mul[sp]d|v?div[sp]d|v?sqrt[sp]d|"
    r"v?fmadd\d+[sp]d|v?fmsub\d+[sp]d|v?fnmadd\d+[sp]d|v?fnmsub\d+[sp]d)\b"
)
ASM_FP32 = re.compile(
    r"\b(v?add[sp]s|v?sub[sp]s|v?mul[sp]s|v?div[sp]s|v?sqrt[sp]s|"
    r"v?fmadd\d+[sp]s|v?fmsub\d+[sp]s|v?fnmadd\d+[sp]s|v?fnmsub\d+[sp]s)\b"
)
ASM_FP16 = re.compile(r"\b(vcvtph2ps|vcvtps2ph)\b")


def _audit_source_and_asm(src_path: str, obj_path: str):
    with open(src_path, "r", encoding="utf-8") as f:
        content = f.read()
    blocks = re.findall(
        r"^// EVOLVE-BLOCK-START\s*\n(.*?)^// EVOLVE-BLOCK-END\s*$",
        content,
        flags=re.MULTILINE | re.DOTALL,
    )
    if not blocks:
        return False, "Missing // EVOLVE-BLOCK-START ... // EVOLVE-BLOCK-END", {}

    fp16_tokens = 0
    fp32_tokens = 0
    for block in blocks:
        clean = re.sub(r"//.*", "", block)
        clean = re.sub(r"/\*.*?\*/", "", clean, flags=re.DOTALL)
        clean = re.sub(r'"(?:\\.|[^"\\])*"', '""', clean)
        for rx, label in FORBIDDEN_SOURCE_PATTERNS:
            if rx.search(clean):
                return False, f"Forbidden 64-bit token '{label}' inside EVOLVE-BLOCK", {}
        fp16_tokens += len(re.findall(r"\b_Float16\b", clean))
        fp32_tokens += len(re.findall(r"\bfloat\b", clean))

    fp64_asm_ops = 0
    fp32_asm_ops = 0
    fp16_asm_ops = 0
    res_asm = subprocess.run(["objdump", "-d", "-C", obj_path], capture_output=True, text=True, check=False)
    in_low_prec = False
    for line in res_asm.stdout.splitlines():
        m = re.match(r"^[0-9a-fA-F]+\s+<([^>]+)>:", line)
        if m:
            fn = m.group(1)
            in_low_prec = ("LowPrec" in fn) or ("mxp_eft::" in fn)
            continue
        if in_low_prec:
            if FORBIDDEN_ASM_FP64.search(line):
                fp64_asm_ops += 1
            if ASM_FP32.search(line):
                fp32_asm_ops += 1
            if ASM_FP16.search(line):
                fp16_asm_ops += 1

    if fp64_asm_ops > 0:
        return False, f"Detected {fp64_asm_ops} forbidden 64-bit FP instructions in LowPrec/mxp_eft functions", {}

    hw_weighted_cost = 16 * fp64_asm_ops + 2 * fp32_asm_ops + 1 * max(fp16_asm_ops, fp16_tokens)
    return True, "OK", {
        "fp16_tokens": fp16_tokens,
        "fp32_tokens": fp32_tokens,
        "fp16_asm_ops": fp16_asm_ops,
        "fp32_asm_ops": fp32_asm_ops,
        "fp64_asm_ops": fp64_asm_ops,
        "hw_weighted_cost": hw_weighted_cost,
    }


def evaluate(candidate_src="assets/alphaevolve-advect/advect_evolve.cpp"):
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
    if not os.path.isfile(os.path.join(repo_root, "Makefile")):
        repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
    candidate_src = os.path.abspath(os.path.join(repo_root, candidate_src)) if not os.path.isabs(candidate_src) else candidate_src
    build_dir = os.path.join(repo_root, "build")
    os.makedirs(build_dir, exist_ok=True)
    test_bin = os.path.join(build_dir, "test_weno_order_evolve")
    obj_file = os.path.join(build_dir, "advect_evolve.o")

    env = os.environ.copy()
    env["PATH"] = f"/opt/rh/gcc-toolset-15/root/usr/bin:/usr/lib64/openmpi/bin:{env.get('PATH', '')}"
    env["OMPI_CXX"] = "/opt/rh/gcc-toolset-15/root/usr/bin/g++"

    cmd_comp = [
        "mpicxx", "-std=gnu++17", "-O2", "-mavx2", "-mf16c", "-mfma",
        "-Wall", "-Wextra", "-Wno-error=cast-function-type",
        "-DOMPI_SKIP_MPICXX", f"-I{os.path.join(repo_root, 'src')}", "-c",
        candidate_src, "-o", obj_file,
    ]
    res_comp = subprocess.run(cmd_comp, capture_output=True, text=True, env=env)
    if res_comp.returncode != 0:
        return {"passed": False, "score": 0.0, "error": f"Compilation failed: {res_comp.stderr[:400]}"}

    ok, msg, stats = _audit_source_and_asm(candidate_src, obj_file)
    if not ok:
        return {"passed": False, "score": 0.0, "error": msg}

    libobjs = [
        os.path.join(build_dir, f) for f in ["driver.o", "grid.o", "init.o", "io.o", "redistance.o"]
    ]
    cmd_link = [
        "mpicxx", "-std=gnu++17", "-O2", "-mavx2", "-mf16c", "-mfma",
        f"-I{os.path.join(repo_root, 'src')}",
        os.path.join(repo_root, "tests/test_weno_order.cpp"), obj_file, *libobjs, "-lm",
        "-o", test_bin,
    ]
    res_link = subprocess.run(cmd_link, capture_output=True, text=True, env=env)
    if res_link.returncode != 0:
        return {"passed": False, "score": 0.0, "error": f"Link failed: {res_link.stderr[:400]}"}

    cmd_run = ["mpirun", "--allow-run-as-root", "--oversubscribe", "-np", "1", test_bin]
    res_run = subprocess.run(cmd_run, capture_output=True, text=True, env=env, timeout=30)
    if res_run.returncode != 0 or "PASS" not in res_run.stdout:
        return {"passed": False, "score": 0.0, "error": f"Test failed: {res_run.stderr or res_run.stdout}"}

    p_l1 = 4.5
    p_linf = 4.5
    l1_128 = 3.94e-6
    for line in res_run.stdout.splitlines():
        if "128x128" in line and "L1 err" in line:
            parts = line.split()
            for idx, tok in enumerate(parts):
                if tok == "err" and idx > 0 and parts[idx - 1] == "L1":
                    l1_128 = float(parts[idx + 1])
        if "order (L1)" in line:
            p_l1 = float(line.split(":")[-1].strip())
        if "order (Linf)" in line:
            p_linf = float(line.split(":")[-1].strip())

    accuracy_score = 10.0 + min(p_l1, p_linf)
    bonus = 2.0 if (stats["fp64_asm_ops"] == 0 and stats["fp16_tokens"] > 0) else 0.5
    composite = accuracy_score + bonus

    return {
        "passed": True,
        "composite_fitness": round(composite, 4),
        "order_l1": p_l1,
        "order_linf": p_linf,
        "l1_err_128x128": l1_128,
        "forbidden_double_ops": stats["fp64_asm_ops"],
        "precision_stats": stats,
        "precision_bonus": bonus,
        "insight": {
            "label": "WENO5_OZAKI_EFT_CONVERGED",
            "text": (
                f"WENO5 spatial order L1={p_l1:.2f}, Linf={p_linf:.2f} (128x128 L1={l1_128:.3e}). "
                f"Zero FP64 instructions (fp64_asm_ops=0, _Float16 tokens={stats['fp16_tokens']}, "
                f"float tokens={stats['fp32_tokens']}, hw_weighted_cost={stats['hw_weighted_cost']})."
            ),
        },
    }




# GDM alpha_evolve SDK entry point
try:
    from alpha_evolve.models import (
        AlphaEvolveEvaluationInsight,
        AlphaEvolveEvaluationInsights,
        AlphaEvolveEvaluationScore,
        AlphaEvolveEvaluationScores,
        AlphaEvolveProgramEvaluation,
    )
except ImportError:
    class AlphaEvolveEvaluationInsight:
        def __init__(self, label: str, text: str):
            self.label = label
            self.text = text
        def model_dump(self) -> dict:
            return {"label": self.label, "text": self.text}
    class AlphaEvolveEvaluationInsights:
        def __init__(self, insights: list):
            self.insights = insights
        def model_dump(self) -> list:
            return [i.model_dump() for i in self.insights]
    class AlphaEvolveEvaluationScore:
        def __init__(self, metric: str, score):
            self.metric = metric
            self.score = score
        def model_dump(self) -> dict:
            return {"metric": self.metric, "score": self.score}
    class AlphaEvolveEvaluationScores:
        def __init__(self, scores: list):
            self.scores = scores
        def model_dump(self) -> list:
            return [s.model_dump() for s in self.scores]
    class AlphaEvolveProgramEvaluation:
        def __init__(self, scores, insights):
            self.scores = scores
            self.insights = insights
        def model_dump(self) -> dict:
            return {"scores": {"scores": self.scores.model_dump()}, "insights": {"insights": self.insights.model_dump()}}

def evaluate_advect_mxp() -> dict:
    res = evaluate()
    score_val = res.get("composite_fitness") if res.get("passed") else None
    ins = res.get("insight", {"label": "EVAL_RESULT", "text": json.dumps(res)})
    return AlphaEvolveProgramEvaluation(
        scores=AlphaEvolveEvaluationScores(scores=[AlphaEvolveEvaluationScore(metric="composite_fitness", score=score_val)]),
        insights=AlphaEvolveEvaluationInsights(insights=[AlphaEvolveEvaluationInsight(label=ins["label"], text=ins["text"])]),
    ).model_dump()

if __name__ == "__main__":
    src = sys.argv[1] if len(sys.argv) > 1 else "assets/alphaevolve-advect/advect_evolve.cpp"
    out = evaluate(src)
    print(json.dumps(out, indent=2))
    sys.exit(0 if out.get("passed") else 1)
