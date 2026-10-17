#!/usr/bin/env python3
"""run.py -- generic AlphaEvolve optimization run for any experiment in this repo.

    python3 dev/workflows/alphaevolve/run.py check    EXPERIMENT_DIR
    python3 dev/workflows/alphaevolve/run.py baseline EXPERIMENT_DIR
    python3 dev/workflows/alphaevolve/run.py bundle   EXPERIMENT_DIR
    python3 dev/workflows/alphaevolve/run.py submit   EXPERIMENT_DIR
    python3 dev/workflows/alphaevolve/run.py apply    EXPERIMENT_DIR [--yes]

Nothing here knows about a particular problem.  An experiment directory holds

    experiment.yaml   title, problem_description, primary_metric, seed_files,
                      evaluator: {script, method, postprocess?}
    <script>.py       the evaluator, `method()` -> {"scores": [...], "insights": [...]}

and is produced by the evaluator workflow (dev/workflows/evaluator/), which is
also what embeds the EVOLVE-BLOCK markers in the seed files.  This runner
requires the markers to be present already and never edits seed files except
in `apply`.

  check     seed files exist and carry balanced // EVOLVE-BLOCK-START/END
            markers; the evaluator script and its method(s) exist
  baseline  check, then run the evaluator on the current tree; writes
            <exp>/results/baseline.json and prints the scores
  bundle    check, then pack the working tree (git-tracked + untracked files not
            ignored, minus submodules) plus the experiment into <exp>/results/bundle.tar.gz for upload
  submit    bundle, then run $ALPHAEVOLVE_SUBMIT_CMD with {experiment},
            {bundle} and {root} substituted (platform-specific; no default)
  apply     copy <exp>/results/best/<seed file> over each seed file (written
            by the evaluator's postprocess hook); shows the diff stat first and
            asks for confirmation unless --yes
"""

import argparse
import importlib.util
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

import yaml

START = re.compile(r"^\s*// EVOLVE-BLOCK-START\s*$")
END = re.compile(r"^\s*// EVOLVE-BLOCK-END\s*$")


def repo_root() -> Path:
    return Path(subprocess.run(["git", "rev-parse", "--show-toplevel"], capture_output=True,
                               text=True, check=True, cwd=Path(__file__).parent).stdout.strip())


def load(exp_dir: Path) -> dict:
    spec = yaml.safe_load((exp_dir / "experiment.yaml").read_text())
    for key in ("title", "problem_description", "primary_metric", "seed_files", "evaluator"):
        if key not in spec:
            raise SystemExit(f"run.py: experiment.yaml missing '{key}'")
    return spec


def evolve_blocks(path: Path):
    """[(start_line, end_line)] 1-based; raises on unbalanced / nested markers."""
    blocks, open_at = [], None
    for n, line in enumerate(path.read_text().splitlines(), 1):
        if START.match(line):
            if open_at is not None:
                raise ValueError(f"{path}:{n}: nested EVOLVE-BLOCK-START (open since line {open_at})")
            open_at = n
        elif END.match(line):
            if open_at is None:
                raise ValueError(f"{path}:{n}: EVOLVE-BLOCK-END without START")
            blocks.append((open_at, n))
            open_at = None
    if open_at is not None:
        raise ValueError(f"{path}:{open_at}: EVOLVE-BLOCK-START never closed")
    return blocks


def check(exp_dir: Path, root: Path) -> dict:
    spec = load(exp_dir)
    problems = []
    for rel in spec["seed_files"]:
        path = root / rel
        if not path.is_file():
            problems.append(f"seed file {rel} not found")
            continue
        try:
            blocks = evolve_blocks(path)
        except ValueError as exc:
            problems.append(str(exc))
            continue
        if not blocks:
            problems.append(f"{rel}: no EVOLVE-BLOCK (embed one with dev/workflows/evaluator/)")
        else:
            print("run.py: %s: %d block(s) %s" % (rel, len(blocks),
                                                  ", ".join("L%d-%d" % b for b in blocks)))
    ev = spec["evaluator"]
    script = exp_dir / ev["script"]
    if not script.is_file():
        problems.append(f"evaluator script {script} not found")
    else:
        src = script.read_text()
        for key in ("method", "postprocess"):
            name = ev.get(key)
            if name and not re.search(r"^def\s+%s\s*\(" % re.escape(name), src, re.MULTILINE):
                problems.append(f"{script.name}: no function '{name}' ({key})")
    if problems:
        for p in problems:
            print("run.py: FAIL " + p, file=sys.stderr)
        raise SystemExit(1)
    print("run.py: %s OK (primary metric: %s)" % (spec["title"], spec["primary_metric"]))
    return spec


def baseline(exp_dir: Path, root: Path) -> None:
    spec = check(exp_dir, root)
    ev = spec["evaluator"]
    mod_spec = importlib.util.spec_from_file_location("experiment_evaluator", exp_dir / ev["script"])
    mod = importlib.util.module_from_spec(mod_spec)
    cwd = os.getcwd()
    os.chdir(root)
    try:
        mod_spec.loader.exec_module(mod)
        result = getattr(mod, ev["method"])()
    finally:
        os.chdir(cwd)
    out = exp_dir / "results" / "baseline.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2, default=str))
    for s in result.get("scores", []):
        mark = "  <- primary" if s.get("metric") == spec["primary_metric"] else ""
        print("run.py: %-28s %s%s" % (s.get("metric"), s.get("score"), mark))
    for i in result.get("insights", []):
        print("run.py: [%s] %s" % (i.get("label"), i.get("text")))
    print("run.py: wrote %s" % out)


def bundle(exp_dir: Path, root: Path) -> Path:
    check(exp_dir, root)
    def git(*args):
        return subprocess.run(["git", *args], capture_output=True, text=True, check=True,
                              cwd=root).stdout.splitlines()

    # tracked + untracked-but-not-ignored; minus submodules (mode 160000, e.g.
    # dev/tools/RAPTOR), which are not part of the program
    submodules = {l.split("\t", 1)[1] for l in git("ls-files", "-s") if l.startswith("160000")}
    files = [f for f in git("ls-files", "--cached", "--others", "--exclude-standard")
             if f not in submodules]
    out = exp_dir / "results" / "bundle.tar.gz"
    out.parent.mkdir(parents=True, exist_ok=True)
    exp_rel = exp_dir.resolve().relative_to(root)
    with tarfile.open(out, "w:gz") as tf:
        for rel in files:
            if (root / rel).is_file():
                tf.add(root / rel, arcname=rel)
    print("run.py: wrote %s (%d files incl. %s)" % (out, len(files), exp_rel))
    return out


def submit(exp_dir: Path, root: Path) -> None:
    cmd = os.environ.get("ALPHAEVOLVE_SUBMIT_CMD")
    if not cmd:
        raise SystemExit("run.py: set ALPHAEVOLVE_SUBMIT_CMD, e.g. "
                         "'alphaevolve launch --config {experiment}/experiment.yaml --source {bundle}'")
    out = bundle(exp_dir, root)
    argv = [a.format(experiment=exp_dir.resolve(), bundle=out, root=root) for a in shlex.split(cmd)]
    print("run.py: " + " ".join(argv))
    raise SystemExit(subprocess.call(argv, cwd=root))


def apply(exp_dir: Path, root: Path, yes: bool) -> None:
    spec = load(exp_dir)
    best = exp_dir / "results" / "best"
    pairs = [(best / rel, root / rel) for rel in spec["seed_files"] if (best / rel).is_file()]
    if not pairs:
        raise SystemExit(f"run.py: nothing in {best} (the evaluator's postprocess writes it)")
    for src, dst in pairs:
        subprocess.call(["git", "--no-pager", "diff", "--no-index", "--stat", str(dst), str(src)])
        evolve_blocks(src)  # refuse to apply a file with broken markers
    if not yes and input("run.py: overwrite %d seed file(s)? [y/N] " % len(pairs)).lower() != "y":
        raise SystemExit("run.py: not applied")
    for src, dst in pairs:
        shutil.copy2(src, dst)
        print("run.py: applied %s" % dst.relative_to(root))


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("command", choices=["check", "baseline", "bundle", "submit", "apply"])
    ap.add_argument("experiment", type=Path)
    ap.add_argument("--yes", action="store_true", help="apply without asking")
    a = ap.parse_args()
    root = repo_root()
    exp = a.experiment.resolve()
    if a.command == "check":
        check(exp, root)
    elif a.command == "baseline":
        baseline(exp, root)
    elif a.command == "bundle":
        bundle(exp, root)
    elif a.command == "submit":
        submit(exp, root)
    else:
        apply(exp, root, a.yes)


if __name__ == "__main__":
    main()
