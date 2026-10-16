# Provenance Runbook — MxP-MiniApp-Bubble Mixed-Precision Optimization

1. **Local Antigravity Pre-Evaluators**:
   - `python3 dev/evaluators/eval_weno.py`
   - `python3 dev/evaluators/eval_twosum.py`
   - `python3 dev/evaluators/eval_redistance.py`
2. **Argolis Cloud C++ / MPI / RAPTOR Build (`agentproject-511205`)**:
   - `bash tools/build_raptor.sh && bash tools/run_fp_sweep.sh`
   - `make clean && make CXX=mpicxx CXXFLAGS="-O3 -march=native" test`
