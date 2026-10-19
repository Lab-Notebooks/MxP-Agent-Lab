- app/ is the code being optimized; it changes per project:
  - app/src/ the code, app/tests/ its tests
  - app/params/ for initializing and running the application
  - app/tools/ for output visualization and app-specific analysis
  - app/Makefile builds and tests it; its commands run in app/
- Makefile (root) holds generic entry points and the RAPTOR toolchain.
- experiments/<yyyy-mm-dd>[_N]/ holds every record of one pass through dev/workflows;
  generated output in its results/ (gitignored).
- dev/ is the agentic layer to orchestrate AI workflows for provenance. It is
  app/-agnostic:
  - dev/tools/RAPTOR: precision tool (submodule)
  - dev/tools/local: shared python (evolve.py Evaluator, verify.py Verification)
  - dev/workflows: staged workflows (raptorize -> alphaevolve -> verify); each is
    started by a scientist. Follow dev/workflows/system.md, then the stage's system.md
    (how to interact) and plan.md (what to do).
- Python style: black, pylint-clean, numpy-style docstrings,
  classes only for state + behaviour, plain functions otherwise.
- make image --> builds the docker image (bubble-mxp)
- alias bmx='mkdir -p ~/.config/gcloud && docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/work -v "$HOME/.config/gcloud":/tmp/.config/gcloud bubble-mxp' --> shorthand; container HOME is /tmp
- bmx make check
- Keep code and docs succinct: every line is reviewed.

Respect the current directory organization to do your work.
