- params/ are for intializing and running the application.
- dev/ is the agentic layer to orchestrate AI workflows for provenance.
- dev/workflows/ holds the staged workflows (raptorize -> evaluator -> alphaevolve -> verify);
  each is started by a scientist and follows its plan.md (see dev/workflows/README.md).
- tests/ are for src/.
- tools/ are for output visualization.
- docker build -t bubble-mxp . --> builds the docker images
- alias bmx='docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/work bubble-mxp' --> shorthand
- bmx make check

Respect the current directory organization to do your work.
