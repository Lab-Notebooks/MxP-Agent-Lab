#!/usr/bin/env python3
"""run_verification.py -- run everything a verification.yaml asks for.

    python3 dev/workflows/verify/run_verification.py dev/workflows/verify/verification.yaml

Meant for the project container (see the root Dockerfile).  In order:

  1. `make` and, if `tests: true`, `make check`
  2. each entry of `runs`: mpirun -np <ranks> ./bubble <params> <overrides>
     outdir=<dest>/<name>/out, stdout teed to <dest>/<name>/run.log; then
     verify.py on it with --criteria (plus one extra schematic per `cells` entry)
  3. each entry of `compare`: verify.py <run> --ref <ref> with that entry's criteria
  4. <dest>/summary.md: one line per step with its status, and the exit status
     is 1 if any step failed

Never writes outside `dest` (apart from the build in build/ and ./bubble).
"""

import json
import os
import subprocess
import sys
from pathlib import Path

import yaml

HERE = Path(__file__).resolve().parent
VERIFY = HERE / "verify.py"


def sh(cmd, log=None):
    print("run_verification: $ " + " ".join(cmd), flush=True)
    if log is None:
        return subprocess.call(cmd)
    with open(log, "w") as f:
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for line in p.stdout:
            sys.stdout.write(line)
            f.write(line)
        return p.wait()


def criteria_file(criteria, where):
    """Write the criteria a step is checked against next to its output (provenance)."""
    path = os.path.join(where, "criteria.json")
    with open(path, "w") as f:
        json.dump({"criteria": criteria}, f)
    return path


def main(argv):
    if len(argv) != 2:
        sys.stderr.write(__doc__)
        return 2
    spec_path = argv[1]
    spec = yaml.safe_load(Path(spec_path).read_text())
    dest = Path(spec.get("dest", "out_verify"))
    dest.mkdir(parents=True, exist_ok=True)
    results = []

    def record(step, rc, where=""):
        results.append((step, "PASS" if rc == 0 else "FAIL (exit %d)" % rc, where))

    rc = sh(["make", "-j%d" % (os.cpu_count() or 4)])
    record("make", rc)
    if rc != 0:
        return finish(dest, spec, spec_path, results)
    if spec.get("tests", True):
        record("make check", sh(["make", "check"], log=dest / "make_check.log"),
               str(dest / "make_check.log"))

    default_criteria = spec.get("criteria", {})
    outdirs = {}
    for run in spec.get("runs", []):
        name = run["name"]
        rdir = dest / name
        rdir.mkdir(parents=True, exist_ok=True)
        out = rdir / "out"
        cmd = ["mpirun", "-np", str(run.get("ranks", 4)), "./bubble", run["params"],
               *run.get("overrides", []), "outdir=%s" % out]
        rc = sh(cmd, log=rdir / "run.log")
        record("run %s" % name, rc, str(rdir / "run.log"))
        if rc != 0:
            continue
        outdirs[name] = out
        crit = criteria_file(run.get("criteria", default_criteria), str(rdir))
        rc = sh([sys.executable, str(VERIFY), str(out), "--dest", str(rdir / "verify"),
                 "--criteria", crit])
        record("verify %s" % name, rc, str(rdir / "verify" / "report.md"))
        for i, j in run.get("cells", []):
            cdest = rdir / ("verify_cell_%d_%d" % (i, j))
            rc = sh([sys.executable, str(VERIFY), str(out), "--dest", str(cdest),
                     "--cell", str(i), str(j)])
            record("verify %s cell (%d,%d)" % (name, i, j), rc, str(cdest / "report.md"))

    for cmp_ in spec.get("compare", []):
        name, run, ref = cmp_["name"], cmp_["run"], cmp_["ref"]
        if run not in outdirs or ref not in outdirs:
            record("compare %s" % name, 1, "run %s or %s missing" % (run, ref))
            continue
        cdest = dest / ("compare_" + name)
        cdest.mkdir(parents=True, exist_ok=True)
        crit = criteria_file(cmp_.get("criteria", {}), str(cdest))
        rc = sh([sys.executable, str(VERIFY), str(outdirs[run]), "--ref", str(outdirs[ref]),
                 "--dest", str(cdest), "--criteria", crit])
        record("compare %s" % name, rc, str(cdest / "report.md"))

    return finish(dest, spec, spec_path, results)


def finish(dest, spec, spec_path, results):
    ok = all(s == "PASS" for _, s, _ in results)
    L = ["# Verification summary", "",
         "Candidate: %s" % spec.get("candidate", "?"), "",
         "Spec: `%s`" % spec_path, "",
         "Overall: **%s**" % ("PASS" if ok else "FAIL"), "",
         "| step | status | output |", "|:---|:---|:---|"]
    L += ["| %s | %s | %s |" % (step, status, ("`%s`" % where) if where else "")
          for step, status, where in results]
    (dest / "summary.md").write_text("\n".join(L) + "\n")
    print("run_verification: %s -> %s" % ("PASS" if ok else "FAIL", dest / "summary.md"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
