#!/usr/bin/env python3
"""
run_all_evaluators.py -- Master test runner for all 4 AlphaEvolve hotspots:
1. assets/alphaevolve-advect/evaluator.py
2. assets/alphaevolve-redistance/evaluator.py
3. assets/alphaevolve-init/evaluator.py
4. assets/alphaevolve-driver/evaluator.py
"""

import json
import os
import subprocess
import sys
import time

def run():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
    hotspots = [
        ("advect", "assets/alphaevolve-advect/evaluator.py"),
        ("redistance", "assets/alphaevolve-redistance/evaluator.py"),
        ("init", "assets/alphaevolve-init/evaluator.py"),
        ("driver", "assets/alphaevolve-driver/evaluator.py")
    ]

    results = {}
    all_passed = True
    start_total = time.time()

    for name, script in hotspots:
        script_path = os.path.join(repo_root, script)
        t0 = time.time()
        res = subprocess.run([sys.executable, script_path], capture_output=True, text=True)
        dt = time.time() - t0
        try:
            data = json.loads(res.stdout)
        except Exception:
            data = {"passed": False, "raw_output": res.stdout, "stderr": res.stderr}

        data["duration_sec"] = round(dt, 2)
        results[name] = data
        if not data.get("passed", False):
            all_passed = False

    summary = {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%SZ", time.gmtime()),
        "total_duration_sec": round(time.time() - start_total, 2),
        "all_hotspots_passed": all_passed,
        "hotspots": results
    }

    print(json.dumps(summary, indent=2))
    return 0 if all_passed else 1

if __name__ == "__main__":
    sys.exit(run())
