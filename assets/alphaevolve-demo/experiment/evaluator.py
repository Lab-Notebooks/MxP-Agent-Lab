# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""AlphaEvolve evaluator for Mixed-Precision (FP16/FP32) Deforming Bubble Mini-App."""

import json
import logging
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
from typing import Dict, List, Optional, Tuple
import xml.etree.ElementTree as ET

from alpha_evolve.models import (
    AlphaEvolveEvaluationInsight,
    AlphaEvolveEvaluationInsights,
    AlphaEvolveEvaluationScore,
    AlphaEvolveEvaluationScores,
    AlphaEvolveProgramEvaluation,
)
from alpha_evolve.utils import get_job_id_from_program_name
from alpha_evolve.visualization import get_score

logger = logging.getLogger(__name__)

PRIMARY_METRIC = "trajectory_accuracy_score"
EVOLVED_SOURCES = [
    "src/advect.cpp",
    "src/redistance.cpp",
    "src/init.cpp",
    "src/driver.cpp",
]
EVOLVED_OBJECTS = [
    "build/advect.o",
    "build/redistance.o",
    "build/init.o",
    "build/driver.o",
]

# Forbidden 64-bit types/tokens inside EVOLVE-BLOCK sections
FORBIDDEN_SOURCE_PATTERNS = [
    (re.compile(r"\bdouble\b"), "double (64-bit float)"),
    (re.compile(r"\bfloat64_t\b"), "float64_t"),
    (re.compile(r"\b__float128\b"), "__float128"),
    (re.compile(r"\bMPI_DOUBLE\b"), "MPI_DOUBLE"),
]

# Forbidden 64-bit x86_64 SSE2/AVX scalar & packed double-precision arithmetic instructions
FORBIDDEN_ASM_MNEMONICS = re.compile(
    r"\b(v?add[sp]d|v?sub[sp]d|v?mul[sp]d|v?div[sp]d|v?sqrt[sp]d|"
    r"v?fmadd\d+[sp]d|v?fmsub\d+[sp]d|v?fnmadd\d+[sp]d|v?fnmsub\d+[sp]d)\b"
)


def _strip_comments_and_strings(code: str) -> str:
    """Removes C/C++ comments and string literals before scanning for forbidden types."""
    code = re.sub(r"//.*", "", code)
    code = re.sub(r"/\*.*?\*/", "", code, flags=re.DOTALL)
    code = re.sub(r'"(?:\\.|[^"\\])*"', '""', code)
    return code


def _extract_evolve_blocks(filepath: str) -> List[str]:
    """Extracts all code sections inside // EVOLVE-BLOCK-START ... // EVOLVE-BLOCK-END."""
    if not os.path.isfile(filepath):
        return []
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()
    pattern = re.compile(
        r"^// EVOLVE-BLOCK-START\s*\n(.*?)^// EVOLVE-BLOCK-END\s*$",
        re.MULTILINE | re.DOTALL,
    )
    return [m.group(1) for m in pattern.finditer(content)]


def audit_low_precision_compliance() -> Tuple[bool, List[AlphaEvolveEvaluationInsight], Dict[str, int]]:
    """Verifies that EVOLVE-BLOCKs strictly use FP16/FP32 and no 64-bit double arithmetic."""
    insights: List[AlphaEvolveEvaluationInsight] = []
    type_counts = {"fp16_tokens": 0, "fp32_tokens": 0, "fp64_asm_ops": 0}

    for src_path in EVOLVED_SOURCES:
        blocks = _extract_evolve_blocks(src_path)
        if not blocks:
            insights.append(
                AlphaEvolveEvaluationInsight(
                    label="MISSING_EVOLVE_BLOCK",
                    text=f"File '{src_path}' is missing valid // EVOLVE-BLOCK-START / END markers.",
                )
            )
            return False, insights, type_counts

        for idx, block in enumerate(blocks):
            clean = _strip_comments_and_strings(block)
            for regex, label in FORBIDDEN_SOURCE_PATTERNS:
                match = regex.search(clean)
                if match:
                    insights.append(
                        AlphaEvolveEvaluationInsight(
                            label="FORBIDDEN_FP64_TYPE",
                            text=(
                                f"Forbidden 64-bit type '{label}' detected inside EVOLVE-BLOCK #{idx + 1} "
                                f"of '{src_path}'. Only 16-bit (_Float16) and 32-bit (float) floating-point "
                                f"types are permitted inside EVOLVE-BLOCK sections."
                            ),
                        )
                    )
                    return False, insights, type_counts

            type_counts["fp16_tokens"] += len(re.findall(r"\b_Float16\b", clean))
            type_counts["fp32_tokens"] += len(re.findall(r"\bfloat\b", clean))

    # Disassemble LowPrec functions in object files to ensure no 64-bit FP arithmetic is emitted
    if shutil.which("objdump"):
        for obj_path in EVOLVED_OBJECTS:
            if not os.path.isfile(obj_path):
                continue
            try:
                res = subprocess.run(
                    ["objdump", "-d", "-C", obj_path],
                    capture_output=True,
                    text=True,
                    timeout=10,
                    check=False,
                )
                in_low_prec_fn = False
                current_fn = ""
                for line in res.stdout.splitlines():
                    header_match = re.match(r"^[0-9a-fA-F]+\s+<([^>]+)>:", line)
                    if header_match:
                        current_fn = header_match.group(1)
                        in_low_prec_fn = "LowPrec" in current_fn
                        continue
                    if in_low_prec_fn:
                        asm_match = FORBIDDEN_ASM_MNEMONICS.search(line)
                        if asm_match:
                            type_counts["fp64_asm_ops"] += 1
                            insights.append(
                                AlphaEvolveEvaluationInsight(
                                    label="FORBIDDEN_FP64_INSTRUCTION",
                                    text=(
                                        f"64-bit floating-point instruction '{asm_match.group(1)}' "
                                        f"detected in function '{current_fn}' ({obj_path}). "
                                        f"Ensure all floating-point literals have the 'f' or 'f16' suffix "
                                        f"(e.g. 0.5f instead of 0.5) and use std::sin/cos/sqrt on float."
                                    ),
                                )
                            )
                            return False, insights, type_counts
            except Exception as e:
                logger.warning("objdump check skipped for %s: %s", obj_path, e)

    return True, insights, type_counts


def _read_vti_piece(path: str) -> Tuple[List[int], List[float]]:
    """Reads a single rank's .vti piece and returns (extent, flat dfun values)."""
    root = ET.parse(path).getroot()
    image = root.find("ImageData")
    piece = image.find("Piece")
    ext = [int(v) for v in piece.get("Extent").split()]
    celldata = piece.find("CellData")
    for da in celldata.findall("DataArray"):
        if da.get("Name") == "dfun":
            vals = [float(v) for v in da.text.split()]
            return ext, vals
    raise ValueError(f"No 'dfun' DataArray in {path}")


def read_pvti_dfun(pvti_path: str) -> Tuple[int, int, float, float, List[float]]:
    """Reassembles the global 2D dfun field from a .pvti master file."""
    root = ET.parse(pvti_path).getroot()
    pimage = root.find("PImageData")
    whole = [int(v) for v in pimage.get("WholeExtent").split()]
    spacing = [float(v) for v in pimage.get("Spacing").split()]
    nx = whole[1] - whole[0]
    ny = whole[3] - whole[2]
    dx = spacing[0]

    time_val = 0.0
    fielddata = pimage.find("FieldData")
    if fielddata is not None:
        for da in fielddata.findall("DataArray"):
            if da.get("Name") == "TIME":
                time_val = float(da.text)

    global_dfun = [float("nan")] * (nx * ny)
    folder = os.path.dirname(pvti_path)
    for piece in pimage.findall("Piece"):
        src = os.path.join(folder, piece.get("Source"))
        ext, vals = _read_vti_piece(src)
        i0, i1, j0, j1 = ext[0], ext[1], ext[2], ext[3]
        p_nx = i1 - i0
        p_ny = j1 - j0
        for pj in range(p_ny):
            row_dst = (j0 + pj) * nx + i0
            row_src = pj * p_nx
            global_dfun[row_dst : row_dst + p_nx] = vals[row_src : row_src + p_nx]

    return nx, ny, dx, time_val, global_dfun


def _frame_map(outdir: str) -> Dict[int, str]:
    frames: Dict[int, str] = {}
    if not os.path.isdir(outdir):
        return frames
    frame_re = re.compile(r"^dfun_(\d+)\.pvti$")
    for name in sorted(os.listdir(outdir)):
        m = frame_re.match(name)
        if m:
            frames[int(m.group(1))] = os.path.join(outdir, name)
    return frames


def _parse_area_drift_pct(log_text: str) -> Optional[float]:
    for line in log_text.splitlines():
        if line.startswith("bubble: area"):
            m = re.search(r"drift\s+([-\d.eE+]+)%", line)
            if m:
                return float(m.group(1))
    return None


def _parse_mpi_grid_summary(log_text: str) -> str:
    for line in log_text.splitlines():
        if "process grid" in line:
            return line.split(":", 2)[-1].strip()
    return f"{os.environ.get('BUBBLE_MPI_RANKS', '4')} ranks"


def _mpi_cmd(binary: str, outdir: str, extra_overrides: List[str]) -> List[str]:
    np_ranks = os.environ.get("BUBBLE_MPI_RANKS", "4")
    cmd = [
        "mpirun",
        "--allow-run-as-root",
        "--oversubscribe",
        "-np",
        str(np_ranks),
        binary,
        "params/bubble.ini",
        f"outdir={outdir}",
    ]
    cmd.extend(extra_overrides)
    return cmd


def ensure_reference_trajectory(ref_dir: str, overrides: List[str]) -> Tuple[bool, str]:
    """Ensures the FP64 reference trajectory exists in ref_dir, running ./bubble_ref once if needed."""
    frames = _frame_map(ref_dir)
    log_path = os.path.join(ref_dir, "run.log")
    if len(frames) >= 2 and os.path.isfile(log_path):
        return True, ""

    if not os.path.isfile("./bubble_ref"):
        res = subprocess.run(["make", "bubble_ref"], capture_output=True, text=True, check=False)
        if res.returncode != 0:
            return False, f"Failed to build FP64 reference './bubble_ref':\n{res.stderr}"

    os.makedirs(ref_dir, exist_ok=True)
    cmd = _mpi_cmd("./bubble_ref", ref_dir, overrides)
    logger.info("Generating cached FP64 reference trajectory: %s", " ".join(cmd))
    res = subprocess.run(cmd, capture_output=True, text=True, timeout=300, check=False)
    if res.returncode != 0:
        return False, f"FP64 reference run failed:\n{res.stdout}\n{res.stderr}"

    with open(log_path, "w", encoding="utf-8") as f:
        f.write(res.stdout)
    return True, ""


def evaluate_bubble_mxp() -> dict:
    """Evaluates the candidate low-precision bubble simulation against the FP64 baseline."""
    insights_list: List[AlphaEvolveEvaluationInsight] = []

    # 1. Verify compilation succeeded and check strict low-precision compliance
    if not os.path.isfile("./bubble"):
        res = subprocess.run(["make", "all"], capture_output=True, text=True, check=False)
        if res.returncode != 0:
            insights_list.append(
                AlphaEvolveEvaluationInsight(
                    label="COMPILATION_ERROR",
                    text=f"Compilation failed:\n{res.stdout}\n{res.stderr}",
                )
            )
            scores = [AlphaEvolveEvaluationScore(metric=PRIMARY_METRIC, score=None)]
            return AlphaEvolveProgramEvaluation(
                scores=AlphaEvolveEvaluationScores(scores=scores),
                insights=AlphaEvolveEvaluationInsights(insights=insights_list),
            ).model_dump()

    compliant, audit_insights, type_counts = audit_low_precision_compliance()
    insights_list.extend(audit_insights)
    if not compliant:
        scores = [AlphaEvolveEvaluationScore(metric=PRIMARY_METRIC, score=None)]
        return AlphaEvolveProgramEvaluation(
            scores=AlphaEvolveEvaluationScores(scores=scores),
            insights=AlphaEvolveEvaluationInsights(insights=insights_list),
        ).model_dump()

    # Optional simulation overrides (default runs params/bubble.ini)
    overrides_env = os.environ.get("BUBBLE_EVAL_OVERRIDES", "").strip()
    overrides = overrides_env.split() if overrides_env else []

    ref_dir = "out_ref"
    cand_dir = "out_cand"

    # 2. Ensure FP64 reference trajectory is available
    ref_ok, ref_err = ensure_reference_trajectory(ref_dir, overrides)
    if not ref_ok:
        insights_list.append(
            AlphaEvolveEvaluationInsight(label="REFERENCE_ERROR", text=ref_err)
        )
        scores = [AlphaEvolveEvaluationScore(metric=PRIMARY_METRIC, score=None)]
        return AlphaEvolveProgramEvaluation(
            scores=AlphaEvolveEvaluationScores(scores=scores),
            insights=AlphaEvolveEvaluationInsights(insights=insights_list),
        ).model_dump()

    # 3. Run the candidate low-precision simulation
    if os.path.isdir(cand_dir):
        shutil.rmtree(cand_dir)
    os.makedirs(cand_dir, exist_ok=True)

    cmd = _mpi_cmd("./bubble", cand_dir, overrides)
    logger.info("Running MPI candidate simulation: %s", " ".join(cmd))
    try:
        cand_proc = subprocess.run(cmd, capture_output=True, text=True, timeout=300, check=False)
    except subprocess.TimeoutExpired:
        insights_list.append(
            AlphaEvolveEvaluationInsight(
                label="RUNTIME_TIMEOUT",
                text="Candidate simulation timed out after 300 seconds.",
            )
        )
        scores = [AlphaEvolveEvaluationScore(metric=PRIMARY_METRIC, score=None)]
        return AlphaEvolveProgramEvaluation(
            scores=AlphaEvolveEvaluationScores(scores=scores),
            insights=AlphaEvolveEvaluationInsights(insights=insights_list),
        ).model_dump()

    if cand_proc.returncode != 0:
        insights_list.append(
            AlphaEvolveEvaluationInsight(
                label="RUNTIME_CRASH",
                text=(
                    f"Candidate simulation exited with code {cand_proc.returncode}:\n"
                    f"{cand_proc.stdout}\n{cand_proc.stderr}"
                ),
            )
        )
        scores = [AlphaEvolveEvaluationScore(metric=PRIMARY_METRIC, score=None)]
        return AlphaEvolveProgramEvaluation(
            scores=AlphaEvolveEvaluationScores(scores=scores),
            insights=AlphaEvolveEvaluationInsights(insights=insights_list),
        ).model_dump()

    logger.info("MPI candidate simulation stdout:\n%s", cand_proc.stdout.strip())
    with open(os.path.join(cand_dir, "run.log"), "w", encoding="utf-8") as f:
        f.write(cand_proc.stdout)

    # 4. Compare candidate trajectory frames against FP64 reference frames
    ref_frames = _frame_map(ref_dir)
    cand_frames = _frame_map(cand_dir)
    common_steps = sorted(set(ref_frames.keys()) & set(cand_frames.keys()))

    if len(common_steps) < len(ref_frames):
        insights_list.append(
            AlphaEvolveEvaluationInsight(
                label="INCOMPLETE_TRAJECTORY",
                text=(
                    f"Candidate produced {len(common_steps)} matching frames out of "
                    f"{len(ref_frames)} reference frames. Check computeTimestepLowPrec "
                    f"to ensure dt matches the reference CFL schedule."
                ),
            )
        )
        scores = [AlphaEvolveEvaluationScore(metric=PRIMARY_METRIC, score=None)]
        return AlphaEvolveProgramEvaluation(
            scores=AlphaEvolveEvaluationScores(scores=scores),
            insights=AlphaEvolveEvaluationInsights(insights=insights_list),
        ).model_dump()

    frame_mses: List[Tuple[int, float, float, float, float]] = []
    total_sq_err = 0.0
    total_cells = 0
    interface_sq_err = 0.0
    interface_cells = 0

    for step in common_steps:
        nx, ny, dx, ref_t, ref_dfun = read_pvti_dfun(ref_frames[step])
        c_nx, c_ny, _, _, cand_dfun = read_pvti_dfun(cand_frames[step])
        if (nx, ny) != (c_nx, c_ny):
            insights_list.append(
                AlphaEvolveEvaluationInsight(
                    label="GRID_MISMATCH",
                    text=f"Step {step}: candidate grid {(c_nx, c_ny)} != reference {(nx, ny)}.",
                )
            )
            scores = [AlphaEvolveEvaluationScore(metric=PRIMARY_METRIC, score=None)]
            return AlphaEvolveProgramEvaluation(
                scores=AlphaEvolveEvaluationScores(scores=scores),
                insights=AlphaEvolveEvaluationInsights(insights=insights_list),
            ).model_dump()

        band_width = 5.0 * dx
        step_sq = 0.0
        step_if_sq = 0.0
        step_if_cnt = 0
        step_linf = 0.0

        for r_val, c_val in zip(ref_dfun, cand_dfun):
            if math.isnan(c_val) or math.isinf(c_val):
                insights_list.append(
                    AlphaEvolveEvaluationInsight(
                        label="NAN_OR_INF_DETECTED",
                        text=(
                            f"NaN or Inf detected in candidate dfun field at step {step} (t={ref_t:.4f}). "
                            f"Check WENO smoothness indicator division (kEps underflow) and redistancing sqrt."
                        ),
                    )
                )
                scores = [AlphaEvolveEvaluationScore(metric=PRIMARY_METRIC, score=None)]
                return AlphaEvolveProgramEvaluation(
                    scores=AlphaEvolveEvaluationScores(scores=scores),
                    insights=AlphaEvolveEvaluationInsights(insights=insights_list),
                ).model_dump()

            diff = c_val - r_val
            sq = diff * diff
            adiff = abs(diff)
            step_sq += sq
            if adiff > step_linf:
                step_linf = adiff
            if abs(r_val) <= band_width:
                step_if_sq += sq
                step_if_cnt += 1

        n_cells = nx * ny
        step_mse = step_sq / n_cells
        step_if_mse = (step_if_sq / step_if_cnt) if step_if_cnt > 0 else step_mse
        frame_mses.append((step, ref_t, step_mse, step_if_mse, step_linf))

        total_sq_err += step_sq
        total_cells += n_cells
        interface_sq_err += step_if_sq
        interface_cells += step_if_cnt

    trajectory_mse = total_sq_err / max(1, total_cells)
    trajectory_rmse = math.sqrt(trajectory_mse)
    interface_mse = interface_sq_err / max(1, interface_cells)
    interface_rmse = math.sqrt(interface_mse)

    final_step, final_t, final_mse, _, final_linf = frame_mses[-1]
    worst_step, worst_t, worst_mse, _, worst_linf = max(frame_mses, key=lambda x: x[2])

    with open(os.path.join(ref_dir, "run.log"), "r", encoding="utf-8") as f:
        ref_drift = _parse_area_drift_pct(f.read())
    cand_drift = _parse_area_drift_pct(cand_proc.stdout)

    # Score is -log10(trajectory_mse + 1e-16), which increases as Trajectory MSE decreases
    score_value = -math.log10(trajectory_mse + 1.0e-16)

    drift_msg = ""
    if ref_drift is not None and cand_drift is not None:
        drift_msg = (
            f"Area drift: candidate={cand_drift:.4f}% vs FP64_ref={ref_drift:.4f}% "
            f"(delta={cand_drift - ref_drift:+.4f} pp)."
        )

    mpi_grid_summary = _parse_mpi_grid_summary(cand_proc.stdout)
    insights_list.append(
        AlphaEvolveEvaluationInsight(
            label="TRAJECTORY_MSE_SUMMARY",
            text=(
                f"MPI process grid: {mpi_grid_summary}. "
                f"Evaluated {len(common_steps)} frames up to step {final_step} (t={final_t:.4f}). "
                f"Trajectory MSE = {trajectory_mse:.6e} (RMSE = {trajectory_rmse:.6e}), "
                f"Interface-band MSE (|phi|<=5*dx) = {interface_mse:.6e} (RMSE = {interface_rmse:.6e}), "
                f"Final-frame MSE = {final_mse:.6e}, Final Linf = {final_linf:.6e}. "
                f"Peak frame MSE occurred at step {worst_step} (t={worst_t:.4f}, MSE={worst_mse:.6e}, Linf={worst_linf:.6e}). "
                f"{drift_msg} "
                f"Source token counts in EVOLVE-BLOCKs: _Float16={type_counts['fp16_tokens']}, float={type_counts['fp32_tokens']}."
            ),
        )
    )

    # Save candidate trajectory archive to _CANDIDATE_DIR for post-experiment GIF/plot generation
    _save_candidate_trajectory_artifacts(ref_dir, "out_seed", cand_dir)

    scores = [AlphaEvolveEvaluationScore(metric=PRIMARY_METRIC, score=score_value)]
    return AlphaEvolveProgramEvaluation(
        scores=AlphaEvolveEvaluationScores(scores=scores),
        insights=AlphaEvolveEvaluationInsights(insights=insights_list),
    ).model_dump()


def _pack_dir_tar_gz(src_dir: str, tar_path: Path) -> bool:
    """Packs a trajectory output directory (.pvti, .vti, run.log) into a .tar.gz archive."""
    if not os.path.isdir(src_dir):
        return False
    try:
        tar_path.parent.mkdir(parents=True, exist_ok=True)
        with tarfile.open(tar_path, "w:gz") as tf:
            for entry in os.listdir(src_dir):
                full_entry = os.path.join(src_dir, entry)
                tf.add(full_entry, arcname=entry)
        return True
    except Exception as e:
        logger.warning("Failed to pack trajectory archive %s: %s", tar_path, e)
        return False


def _extract_dir_tar_gz(tar_path: Path, dst_dir: str) -> bool:
    """Extracts a trajectory .tar.gz archive into dst_dir."""
    if not tar_path.is_file():
        return False
    try:
        os.makedirs(dst_dir, exist_ok=True)
        with tarfile.open(tar_path, "r:gz") as tf:
            tf.extractall(path=dst_dir)
        return True
    except Exception as e:
        logger.warning("Failed to extract trajectory archive %s: %s", tar_path, e)
        return False


def _save_candidate_trajectory_artifacts(ref_dir: str, seed_dir: str, cand_dir: str) -> None:
    """Saves candidate and baseline trajectory archives to the shared GCS mount for postprocess()."""
    try:
        metadata_path = "program_candidate_data.json"
        program_name = os.environ.get("_CANDIDATE_PROGRAM_ID", "unknown")
        if os.path.isfile(metadata_path):
            with open(metadata_path, "r", encoding="utf-8") as f:
                metadata = json.load(f)
            raw_name = metadata.get("name", program_name)
            program_name = raw_name.split("/")[-1]

        candidate_dir = os.environ.get("_CANDIDATE_DIR", "")
        if candidate_dir and os.path.isdir(candidate_dir):
            cand_tar = Path(candidate_dir) / f"{program_name}_trajectory.tar.gz"
            if _pack_dir_tar_gz(cand_dir, cand_tar):
                logger.info("Saved candidate trajectory archive to %s", cand_tar)

        mount_path = os.environ.get("_MOUNT_PATH", "")
        user_exp = os.environ.get("_USER_EXPERIMENT_NAME", "")
        if mount_path and user_exp and os.path.isdir(mount_path):
            traj_cache_dir = Path(mount_path) / user_exp / "trajectories"
            traj_cache_dir.mkdir(parents=True, exist_ok=True)

            cand_traj_tar = traj_cache_dir / f"{program_name}_trajectory.tar.gz"
            if not cand_traj_tar.exists():
                _pack_dir_tar_gz(cand_dir, cand_traj_tar)

            ref_tar = traj_cache_dir / "out_ref_trajectory.tar.gz"
            if not ref_tar.exists():
                local_staged_ref = Path("trajectories/out_ref_trajectory.tar.gz")
                if local_staged_ref.is_file():
                    shutil.copy2(local_staged_ref, ref_tar)
                elif os.path.isdir(ref_dir):
                    _pack_dir_tar_gz(ref_dir, ref_tar)

            seed_tar = traj_cache_dir / "out_seed_trajectory.tar.gz"
            if not seed_tar.exists():
                local_staged_seed = Path("trajectories/out_seed_trajectory.tar.gz")
                if local_staged_seed.is_file():
                    shutil.copy2(local_staged_seed, seed_tar)
                elif os.path.isdir(seed_dir):
                    _pack_dir_tar_gz(seed_dir, seed_tar)
    except Exception as e:
        logger.warning("Failed to save trajectory artifacts for postprocessing: %s", e)


def upload_to_gcs(local_path: Path, bucket_name: str, blob_name: str) -> None:
    """Uploads a local file to a GCS bucket."""
    try:
        from google.cloud import storage

        client = storage.Client()
        bucket = client.bucket(bucket_name)
        blob = bucket.blob(blob_name)
        blob.upload_from_filename(str(local_path))
        logger.info("Successfully uploaded %s to GCS: gs://%s/%s", local_path.name, bucket_name, blob_name)
    except Exception as e:
        logger.warning("Failed to upload %s to GCS: %s", local_path.name, e)


def _download_blob_to_path(bucket_name: str, blob_name: str, local_path: Path) -> bool:
    """Downloads a binary blob from GCS to local_path if it exists."""
    try:
        from google.cloud import storage

        client = storage.Client()
        bucket = client.bucket(bucket_name)
        blob = bucket.blob(blob_name)
        if not blob.exists():
            return False
        local_path.parent.mkdir(parents=True, exist_ok=True)
        blob.download_to_filename(str(local_path))
        return True
    except Exception as e:
        logger.warning("Failed to download gs://%s/%s: %s", bucket_name, blob_name, e)
        return False


def _ensure_postprocess_trajectories(
    best_prog: dict,
    best_prog_id: str,
    bucket_name: str,
    user_exp: str,
    ref_dir: str,
    seed_dir: str,
    best_dir: str,
) -> bool:
    """Ensures out_ref, out_seed, and out_ae_best trajectories are available for plot_compare.py."""
    programs_dir = os.getenv("_PROGRAMS_DIR", "program_candidates")
    deployment_name = os.getenv("_DEPLOYMENT_NAME", "alpha-evolve")
    job_prefix = f"{deployment_name}-workers"
    job_id = get_job_id_from_program_name(best_prog_id, job_prefix)

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        # 1. Try restoring cached trajectory archives from GCS if needed
        if bucket_name:
            if not _frame_map(ref_dir):
                ref_tar = tmp_path / "out_ref_trajectory.tar.gz"
                for ref_blob in [
                    f"{user_exp}/trajectories/out_ref_trajectory.tar.gz",
                    f"{user_exp}/source/trajectories/out_ref_trajectory.tar.gz",
                ]:
                    if _download_blob_to_path(bucket_name, ref_blob, ref_tar):
                        _extract_dir_tar_gz(ref_tar, ref_dir)
                        break

            if not _frame_map(seed_dir):
                seed_tar = tmp_path / "out_seed_trajectory.tar.gz"
                for seed_blob in [
                    f"{user_exp}/trajectories/out_seed_trajectory.tar.gz",
                    f"{user_exp}/source/trajectories/out_seed_trajectory.tar.gz",
                ]:
                    if _download_blob_to_path(bucket_name, seed_blob, seed_tar):
                        _extract_dir_tar_gz(seed_tar, seed_dir)
                        break

            if not _frame_map(best_dir):
                best_tar = tmp_path / f"{best_prog_id}_trajectory.tar.gz"
                for prefix in [
                    f"{user_exp}/trajectories",
                    f"{user_exp}/archive/{programs_dir}/{job_id}",
                    f"{user_exp}/{programs_dir}/{job_id}",
                ]:
                    if _download_blob_to_path(bucket_name, f"{prefix}/{best_prog_id}_trajectory.tar.gz", best_tar):
                        _extract_dir_tar_gz(best_tar, best_dir)
                        break

        # 2. If mpicxx and mpirun are available, compile and run any missing trajectory directly
        if shutil.which("mpicxx") and shutil.which("mpirun"):
            overrides_env = os.environ.get("BUBBLE_POSTPROCESS_OVERRIDES", "").strip()
            overrides = overrides_env.split() if overrides_env else []

            if not _frame_map(ref_dir):
                ensure_reference_trajectory(ref_dir, overrides)

            if not _frame_map(seed_dir):
                os.makedirs(seed_dir, exist_ok=True)
                seed_bin = "./bubble_seed"
                if not os.path.isfile(seed_bin):
                    subprocess.run(
                        ["mpicxx", "-std=c++17", "-O3"] + sorted(str(p) for p in Path("src").glob("*.cpp")) + ["-o", seed_bin],
                        check=False,
                        capture_output=True,
                    )
                if os.path.isfile(seed_bin):
                    cmd = _mpi_cmd(seed_bin, seed_dir, overrides)
                    res = subprocess.run(cmd, capture_output=True, text=True, timeout=600, check=False)
                    with open(os.path.join(seed_dir, "run.log"), "w", encoding="utf-8") as f:
                        f.write(res.stdout)

            if not _frame_map(best_dir):
                os.makedirs(best_dir, exist_ok=True)
                best_src_dir = tmp_path / "best_src"
                shutil.copytree("src", best_src_dir, dirs_exist_ok=True)

                files_list = best_prog.get("content", {}).get("files", [])
                for f_entry in files_list:
                    rel_path = f_entry.get("path", "")
                    content = f_entry.get("content", "")
                    if rel_path.startswith("src/") and content:
                        target_file = tmp_path / "best_src" / Path(rel_path).name
                        target_file.write_text(content, encoding="utf-8")

                if not files_list and bucket_name:
                    for src_name in EVOLVED_SOURCES:
                        fname = Path(src_name).name
                        for prefix in [
                            f"{user_exp}/archive/{programs_dir}/{job_id}/src/{fname}",
                            f"{user_exp}/{programs_dir}/{job_id}/src/{fname}",
                        ]:
                            _download_blob_to_path(bucket_name, prefix, best_src_dir / fname)

                best_bin = str(tmp_path / "bubble_ae_best")
                comp = subprocess.run(
                    ["mpicxx", "-std=c++17", "-O3", f"-I{best_src_dir}"]
                    + sorted(str(p) for p in best_src_dir.glob("*.cpp"))
                    + ["-o", best_bin],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                if comp.returncode == 0 and os.path.isfile(best_bin):
                    cmd = _mpi_cmd(best_bin, best_dir, overrides)
                    res = subprocess.run(cmd, capture_output=True, text=True, timeout=600, check=False)
                    with open(os.path.join(best_dir, "run.log"), "w", encoding="utf-8") as f:
                        f.write(res.stdout)

    return bool(_frame_map(ref_dir) and _frame_map(best_dir))


def postprocess(top_programs: list) -> None:
    """Postprocessing hook for Mixed-Precision Deforming Bubble optimization.

    Generates:
      - demo/compare_ae_best.gif
      - demo/compare_seed_vs_ae_best.gif
      - demo/error_vs_time_ae_best.png (and .csv)
      - demo/error_vs_time_seed_vs_ae_best.png (and .csv)
    and uploads them to GCS under both <user_exp>/demo/ and <user_exp>/results/.
    """
    bucket_name = os.getenv("_CLOUD_BUCKET_NAME", "")
    user_exp = os.getenv("_USER_EXPERIMENT_NAME", "bubble")

    best_prog = top_programs[0] if top_programs else {}
    best_prog_id = best_prog.get("name", "local").split("/")[-1]
    best_score = get_score(best_prog, PRIMARY_METRIC) if best_prog else None
    logger.info("BEST PROGRAM FOUND: %s (Score: %s)", best_prog_id, best_score)

    ref_dir = "out_ref"
    seed_dir = "out_seed"
    best_dir = "out_ae_best" if not os.path.isdir("out_cand") else "out_cand"

    if best_prog_id != "local":
        best_dir = "out_ae_best"

    ready = _ensure_postprocess_trajectories(
        best_prog=best_prog,
        best_prog_id=best_prog_id,
        bucket_name=bucket_name,
        user_exp=user_exp,
        ref_dir=ref_dir,
        seed_dir=seed_dir,
        best_dir=best_dir,
    )
    if not ready and os.path.isdir("out_ref") and os.path.isdir("out_cand"):
        best_dir = "out_cand"
        ready = True

    if not ready:
        logger.warning("Skipping postprocess plotting: reference or best trajectory frames not found.")
        return

    demo_dir = Path("demo")
    results_dir = Path("results")
    demo_dir.mkdir(parents=True, exist_ok=True)
    results_dir.mkdir(parents=True, exist_ok=True)

    exp_root = Path(__file__).resolve().parent
    plot_compare_candidates = [
        exp_root / "tools" / "plot_compare.py",
        Path("tools/plot_compare.py"),
        Path("/app/experiment/tools/plot_compare.py"),
    ]
    mount_path = os.getenv("_MOUNT_PATH", "/mnt/disks/share")
    if mount_path and user_exp:
        plot_compare_candidates.append(Path(mount_path) / user_exp / "source" / "tools" / "plot_compare.py")

    plot_compare_script = str(plot_compare_candidates[0])
    for cand_script in plot_compare_candidates:
        if cand_script.is_file():
            plot_compare_script = str(cand_script.resolve())
            break

    plot_env = os.environ.copy()
    plot_env.setdefault("MPLCONFIGDIR", "/tmp/matplotlib_cache")

    generated_files: List[Path] = []

    # 1. Generate FP64 vs. AE Best comparison (compare_ae_best.gif & error_vs_time_ae_best.png)
    out_ae_only = Path("out_compare_ae_best")
    try:
        res1 = subprocess.run(
            [
                "python3",
                plot_compare_script,
                "--ref",
                ref_dir,
                "--cmp",
                f"ae_best(fp32)={best_dir}",
                "--all-steps",
                "--out",
                str(out_ae_only),
            ],
            env=plot_env,
            capture_output=True,
            text=True,
            check=False,
            timeout=300,
        )
        if res1.returncode == 0:
            for src_name, dst_name in [
                ("compare.gif", "compare_ae_best.gif"),
                ("error_vs_time.png", "error_vs_time_ae_best.png"),
                ("error_vs_time.csv", "error_vs_time_ae_best.csv"),
            ]:
                src_p = out_ae_only / src_name
                if src_p.is_file():
                    dst_p = demo_dir / dst_name
                    shutil.copy2(src_p, dst_p)
                    shutil.copy2(src_p, results_dir / dst_name)
                    generated_files.append(dst_p)
            logger.info("Generated compare_ae_best.gif and error_vs_time_ae_best.png in demo/")
        else:
            logger.warning("plot_compare.py (ae_best) failed:\n%s\n%s", res1.stdout, res1.stderr)
    except Exception as e:
        logger.warning("Failed generating ae_best comparison artifacts: %s", e)

    # 2. Generate FP64 vs. Seed vs. AE Best comparison (compare_seed_vs_ae_best.gif & error_vs_time_seed_vs_ae_best.png)
    if _frame_map(seed_dir):
        out_seed_vs_ae = Path("out_compare_seed_vs_ae")
        try:
            res2 = subprocess.run(
                [
                    "python3",
                    plot_compare_script,
                    "--ref",
                    ref_dir,
                    "--cmp",
                    f"seed(fp16/32)={seed_dir}",
                    "--cmp",
                    f"ae_best(fp32)={best_dir}",
                    "--all-steps",
                    "--out",
                    str(out_seed_vs_ae),
                ],
                env=plot_env,
                capture_output=True,
                text=True,
                check=False,
                timeout=300,
            )
            if res2.returncode == 0:
                for src_name, dst_name in [
                    ("compare.gif", "compare_seed_vs_ae_best.gif"),
                    ("error_vs_time.png", "error_vs_time_seed_vs_ae_best.png"),
                    ("error_vs_time.csv", "error_vs_time_seed_vs_ae_best.csv"),
                ]:
                    src_p = out_seed_vs_ae / src_name
                    if src_p.is_file():
                        dst_p = demo_dir / dst_name
                        shutil.copy2(src_p, dst_p)
                        shutil.copy2(src_p, results_dir / dst_name)
                        generated_files.append(dst_p)
                logger.info("Generated compare_seed_vs_ae_best.gif and error_vs_time_seed_vs_ae_best.png in demo/")
            else:
                logger.warning("plot_compare.py (seed_vs_ae_best) failed:\n%s\n%s", res2.stdout, res2.stderr)
        except Exception as e:
            logger.warning("Failed generating seed_vs_ae_best comparison artifacts: %s", e)

    # 3. Upload generated artifacts to GCS (matching airfoil_optimization/evaluator.py)
    if bucket_name:
        for fpath in generated_files:
            if fpath.exists():
                upload_to_gcs(fpath, bucket_name, f"{user_exp}/demo/{fpath.name}")
                upload_to_gcs(fpath, bucket_name, f"{user_exp}/results/{fpath.name}")


if __name__ == "__main__":
    print(json.dumps(evaluate_bubble_mxp(), indent=2))
