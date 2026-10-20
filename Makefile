# Generic entry points of the lab.  This file knows only that the code under
# study lives in app/ and is built by app/Makefile.  App-specific build logic
# goes in app/Makefile; the stages in dev/ never edit this file.
#
#   make [all|check|clean|debug]   delegated to app/Makefile
#   make RAPTOR=0 ...              build without the RAPTOR plugin
#   make image                     docker image for the containerized stages
#   make evaluate EXP=<dir>        python3 <dir>/evaluator.py     (optimize stage)
#   make evolve EXP=<dir>          launch the ae experiment       (optimize stage)
#   make verify EXP=<dir>          python3 <dir>/verification.py  (verify stage)
#   make patch OUT=<file>          save app/'s changes since HEAD, untracked included

IMAGE ?= bubble-mxp

# RAPTOR toolchain (dev/tools/RAPTOR).  app/Makefile applies these only to the
# files the raptorize stage marks.  RAPTOR=1 when the plugin is installed.
RAPTOR_INSTALL_DIR ?= $(HOME)/raptor-install
LLVM_VER           ?= 20
RAPTOR_PLUGIN      := $(RAPTOR_INSTALL_DIR)/lib/LLVMRaptor-$(LLVM_VER).so
RAPTOR             ?= $(if $(wildcard $(RAPTOR_PLUGIN)),1,0)
RAPTOR_CXX         := clang++-$(LLVM_VER)
RAPTOR_CXXFLAGS    := -DBUBBLE_RAPTOR=1 -I$(RAPTOR_INSTALL_DIR)/include -include raptor/raptor.h -fpass-plugin=$(RAPTOR_PLUGIN)
RAPTOR_LDLIBS      := -L$(RAPTOR_INSTALL_DIR)/lib -lRaptor-RT-$(LLVM_VER) -lmpfr -lstdc++

export RAPTOR RAPTOR_CXX RAPTOR_CXXFLAGS RAPTOR_LDLIBS

.PHONY: all check clean debug image evaluate evolve verify patch

all check clean debug:
	$(MAKE) -C app $@

image:
	docker build -t $(IMAGE) .

evaluate:
	python3 $(EXP)/evaluator.py

evolve:
	python3 dev/workflows/optimize/evolve.py $(EXP)

verify:
	python3 $(EXP)/verification.py

# Throwaway index: the real index is never touched.
patch:
	@i=$$(mktemp -u) && export GIT_INDEX_FILE=$$i && git read-tree HEAD && git add -A app \
	  && git diff --cached --binary HEAD -- app > $(OUT); r=$$?; rm -f $$i; exit $$r
