"""Module with implementation of the Evaluator class and its helpers.

An experiment's evaluator.py (dev/workflows/optimize/evaluator.py)
creates one Evaluator with its settings and adds only the score formula; the
ae CLI runs it (see Evaluator.main). launch() starts the ae experiment:

    python3 dev/workflows/optimize/evolve.py <experiment dir>    # or: make evolve EXP=<dir>

Nothing here knows what is in app/: it is built with `make` from the repo root,
and its commands and paths are relative to app/.
"""

import argparse
import csv
import json
import math
import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

# Directory, under the repo root, of the code being optimized
APP = "app"

# Tokens not allowed inside an EVOLVE-BLOCK, after comments and strings are removed
FORBIDDEN_TOKENS = [
    r"\bdouble\b",
    r"\bfloat64_t\b",
    r"\b__float128\b",
    r"\bMPI_DOUBLE\b",
    r"(?m)^\s*#",  # any preprocessor directive: #include, or a #define that hides a name
]

# An EVOLVE-BLOCK, its body captured
BLOCK = re.compile(
    r"^\s*// EVOLVE-BLOCK-START\s*\n(.*?)^\s*// EVOLVE-BLOCK-END",
    re.MULTILINE | re.DOTALL,
)

# Comments and string literals, removed before the source scans
COMMENTS = re.compile(r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\])*\"", re.DOTALL)

# A function definition: name, parameter list (one level of nested parentheses), then {
DEFINITION = re.compile(
    r"(?<![\w\]])([A-Za-z_]\w*)\s*\((?:[^(){};]|\([^(){};]*\))*\)\s*(?:const\s*)?"
    r"(?:noexcept\s*)?\{"
)
KEYWORDS = {"if", "for", "while", "switch", "catch", "return", "sizeof"}

# x86-64 scalar/packed double-precision arithmetic (SSE2 / AVX / FMA)
FORBIDDEN_ASM = re.compile(
    r"\b(v?(add|sub|mul|div|sqrt|min|max)[sp]d|v?f[n]?m(add|sub)\d+[sp]d)\b"
)


class Evaluator:  # pylint: disable=too-many-instance-attributes
    """Build, audit and run one candidate, and format its result."""

    def __init__(
        self,
        root,
        source,
        objfile,
        command,
        outdir,
        metrics,
        tag="LowPrec",
        timeout=600,
    ):  # pylint: disable=too-many-arguments
        """
        Constructor

        Parameters
        ----------
        root    : repository root, where `make` builds the app
        source  : the kernel's file, with the EVOLVE-BLOCK, relative to app/
        objfile : its object file, for the disassembly audit, relative to app/
        command : run command, string, run in app/
        outdir  : run output directory, relative to app/ or absolute; removed before each run
        metrics : list of metric names, primary first
        tag     : string carried by the names of the audited functions
        timeout : seconds allowed for the run
        """
        self.root = Path(root)
        self.appdir = self.root / APP
        self.source = source
        self.objfile = objfile
        self.command = command
        self.outdir = outdir
        self.metrics = metrics
        self.tag = tag
        self.timeout = timeout
        self.output = ""

    def main(self, evaluate):
        """
        Command-line entry of evaluator.py

        The ae CLI calls `evaluator.py --program-dir <dir> --output-file <json>`:
        <dir>/initial_program.py (the candidate source) replaces app/<source> for
        one evaluate() and is then restored. Without arguments the current tree
        is evaluated and printed.

        Parameters
        ----------
        evaluate : function () -> result dictionary
        """
        parser = argparse.ArgumentParser()
        parser.add_argument("--program-dir")
        parser.add_argument("--output-file")
        args = parser.parse_args()

        if not args.program_dir:
            print(json.dumps(evaluate(), indent=2))
            return

        source = self.appdir / self.source
        original = source.read_text(encoding="utf-8")
        candidate = Path(args.program_dir, "initial_program.py").read_text(
            encoding="utf-8"
        )

        if BLOCK.sub("", candidate) != BLOCK.sub("", original):
            result = self.fail(
                "CHANGED_OUTSIDE_BLOCK",
                f"{self.source}: only code inside an EVOLVE-BLOCK may change.",
            )
        else:
            source.write_text(candidate, encoding="utf-8")
            try:
                result = evaluate()
            finally:
                source.write_text(original, encoding="utf-8")

        Path(args.output_file).write_text(json.dumps(result), encoding="utf-8")

    def prepare(self):
        """
        Build, audit, then run the candidate

        Returns
        -------
        None on success (run output in self.output), else the failed result
        """
        # Build
        exitcode, output = run(["make", "-j4"], 900, cwd=self.root)
        if exitcode:
            return self.fail("COMPILATION_ERROR", output[-4000:])

        # Audit
        problems = self.audit()
        if problems:
            return self.fail(*problems[0])

        # Run
        shutil.rmtree(self.appdir / self.outdir, ignore_errors=True)
        (self.appdir / self.outdir).parent.mkdir(parents=True, exist_ok=True)
        exitcode, self.output = run(self.command, self.timeout, cwd=self.appdir)
        if exitcode:
            label = "RUNTIME_TIMEOUT" if exitcode == 124 else "RUNTIME_CRASH"
            return self.fail(label, self.output[-4000:])

        return None

    def audit(self):
        """
        Check that the EVOLVE-BLOCK holds no 64-bit floating point

        1. token scan of every EVOLVE-BLOCK in self.source
        2. every function defined in a block carries self.tag in its name
        3. disassembly scan of every function in self.objfile whose name contains self.tag

        With the code outside the blocks fixed (main), 2 and 3 cover all the block's code:
        what is not inlined into a tagged function is a tagged function itself.

        Returns
        -------
        empty list if clean, else [(label, text)]
        """
        blocks = evolve_blocks(self.appdir / self.source)
        if not blocks:
            return [("MISSING_EVOLVE_BLOCK", f"{self.source} has no EVOLVE-BLOCK.")]

        # Token and naming scans
        for index, block in enumerate(blocks, start=1):
            code = COMMENTS.sub("", block)
            for token in FORBIDDEN_TOKENS:
                if re.search(token, code):
                    text = f"'{token}' inside EVOLVE-BLOCK #{index} of {self.source}."
                    return [("FORBIDDEN_TOKEN", text)]
            for name in DEFINITION.findall(code):
                if name not in KEYWORDS and self.tag not in name:
                    text = f"function '{name}' in EVOLVE-BLOCK #{index}: name it *{self.tag}*."
                    return [("UNTAGGED_FUNCTION", text)]

        # Disassembly scan
        objfile = self.appdir / self.objfile
        if not shutil.which("objdump") or not objfile.is_file():
            text = f"objdump or {self.objfile} missing: not scored."
            return [("AUDIT_UNAVAILABLE", text)]

        _, assembly = run(["objdump", "-d", "-C", str(objfile)], 60)

        function = ""
        for line in assembly.splitlines():
            header = re.match(r"^[0-9a-fA-F]+\s+<(.+)>:\s*$", line)
            if header:
                function = header.group(1)
                continue

            if self.tag in function:
                match = FORBIDDEN_ASM.search(line)
                if match:
                    text = (
                        f"'{match.group(1)}' in '{function}' ({self.objfile}). "
                        + "Suffix literals with 'f' and call float overloads."
                    )
                    return [("FORBIDDEN_FP64_INSTRUCTION", text)]

        return []

    @staticmethod
    def fail(label, text):
        """
        Failed result: ae records its failure score and shows the reason to the model
        """
        return {"score": None, "insights": [{"label": label, "text": text}]}

    def result(self, scores, insights):
        """
        Format a result for the ae CLI, which takes the first score as primary

        Parameters
        ----------
        scores   : dictionary {metric : value or None}, keys from self.metrics
        insights : list of (label, text) tuples that explain the scores

        Returns
        -------
        dictionary {"scores": [{metric, score}], "insights": [{label, text}]}
        """
        if set(scores) != set(self.metrics):
            raise ValueError(
                f"[evolve.Evaluator] scores {list(scores)} != {self.metrics}"
            )

        return {
            "scores": [
                {"metric": metric, "score": score} for metric, score in scores.items()
            ],
            "insights": [{"label": label, "text": text} for label, text in insights],
        }


def run(command, timeout, cwd=None):
    """
    Run a command without a shell

    Parameters
    ----------
    command : string or list of arguments
    timeout : seconds
    cwd     : working directory, default current

    Returns
    -------
    (exit code, stdout + stderr), exit code is 124 on timeout
    """
    if isinstance(command, str):
        command = shlex.split(command)

    try:
        process = subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False,
            cwd=cwd,
        )
    except subprocess.TimeoutExpired:
        return 124, f"timed out after {timeout} s"

    return process.returncode, process.stdout + process.stderr


def evolve_blocks(source):
    """
    List of code blocks between // EVOLVE-BLOCK-START and // EVOLVE-BLOCK-END
    """
    return BLOCK.findall(Path(source).read_text(encoding="utf-8"))


def read_csv(filename):
    """
    Read a CSV file with a header row

    Returns
    -------
    dictionary {column : list of floats}, NaN for cells that are not numbers
    """
    with open(filename, newline="", encoding="utf-8") as csvfile:
        rows = list(csv.DictReader(csvfile))

    columns = {}
    for key in rows[0] if rows else []:
        columns[key.strip()] = [_to_float(row[key]) for row in rows]

    return columns


def _to_float(value):
    """
    Float of a CSV cell, NaN if it is not a number
    """
    try:
        return float(value)
    except (TypeError, ValueError):
        return math.nan


def launch(expdir):
    """
    Create, start and run an ae experiment from <expdir>/experiment.yaml

    Writes into <expdir>/results/: ae/ (ae profile and cache), program/initial_program.py
    (the seed), problem.md, and experiment.json (the ae experiment record).
    """
    import yaml  # pylint: disable=import-outside-toplevel

    expdir = Path(expdir).resolve()
    with open(expdir / "experiment.yaml", encoding="utf-8") as stream:
        spec = yaml.safe_load(stream)

    results = expdir / "results"
    (results / "program").mkdir(parents=True, exist_ok=True)
    shutil.copy(spec["seed"], results / "program" / "initial_program.py")
    (results / "problem.md").write_text(spec["problem"], encoding="utf-8")

    # ae profile in results/ae, so `AE_CONFIG_DIR=<expdir>/results/ae ae ...` monitors it
    env = {**os.environ, "AE_CONFIG_DIR": str(results / "ae")}
    _call(
        ["ae", "config", f"--project={spec['project']}", f"--engine={spec['engine']}"]
        + [f"--location={spec['location']}"],
        env,
    )

    # Baseline: the seed's primary score
    baseline = _json([sys.executable, str(expdir / "evaluator.py")], env)
    if "scores" not in baseline:
        raise SystemExit(f"[evolve.launch] seed does not score: {baseline['insights']}")
    score = baseline["scores"][0]["score"]

    models = [arg for model in spec["models"] for arg in ["--models", model]]
    created = _json(
        ["ae", "--json", "experiment", "create", "--title", spec["title"]]
        + ["--problem-file", str(results / "problem.md")]
        + ["--language", spec["language"], *models]
        + ["--max-programs", str(spec["max_programs"])]
        + ["--concurrency", str(spec["concurrency"])],
        env,
    )
    (results / "experiment.json").write_text(json.dumps(created, indent=2))

    nickname = created["nickname"]
    _call(
        ["ae", "--json", "experiment", "start", nickname]
        + ["--program-dir", str(results / "program"), "--score", str(score)],
        env,
    )

    # Blocking: ae acquires each candidate, runs evaluator.py on it, submits the score
    subprocess.run(
        ["ae", "experiment", "run", nickname, "--backend", "local"]
        + [
            "--evaluator",
            str(expdir / "evaluator.py"),
            "--timeout",
            str(spec["timeout"]),
        ],
        env=env,
        check=True,
    )


def _call(command, env):
    """
    Run a command, exit on failure

    Returns
    -------
    stdout
    """
    print(f"[evolve] $ {' '.join(command)}", flush=True)
    process = subprocess.run(
        command, capture_output=True, text=True, env=env, check=False
    )
    if process.returncode:
        raise SystemExit(f"[evolve] failed:\n{process.stdout}{process.stderr}")

    return process.stdout


def _json(command, env):
    """
    Run a command and parse its stdout as JSON, exit on failure
    """
    return json.loads(_call(command, env))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)

    launch(sys.argv[1])
