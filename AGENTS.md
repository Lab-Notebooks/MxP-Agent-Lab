- app/ is the code being optimized; it changes per project, so learn its layout.

- Makefile (root) holds generic entry points and the RAPTOR toolchain.

- dev/ is the app/-agnostic agentic layer for provenance:
  * README.md: start here
  * workflows/: protocol.md (plan and run modes, shared), and one folder per stage,
    raptorize → optimize → verify, each started by a scientist; a stage folder holds
    its contract (system.md), templates, code shapes and code
  * tools/RAPTOR: precision submodule

- Stages are classes: dev/workflows/<stage>/ is the class, E/<n>-<stage>/ an instance (E = experiments/<yyyy-mm-dd>).
  * Lower, never raise, the scientist's cognitive burden; they may or may not be expert.
    Study the code before proposing, and explain choices so a non-expert can follow.
  * Two modes: plan (write plan.md with the scientist, proposing defaults instead of
    asking) and run (execute it). Plans are succinct and self-contained: nothing to infer.

- Python: black, pylint-clean, numpy docstrings, classes only for state + behaviour.

- Docker: `make image` builds bubble-mxp; `bmx make check` tests, with (container HOME is /tmp):

  ```sh
  alias bmx='mkdir -p ~/.config/gcloud && docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/work -v "$HOME/.config/gcloud":/tmp/.config/gcloud bubble-mxp'
  ```

- Keep code and docs succinct: every line is reviewed. Respect the directory layout.
