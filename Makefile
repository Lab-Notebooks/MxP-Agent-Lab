# Deforming Bubble mini-app -- build + check
#
# Layout (see dev/tasks/init-project/spec.md, File map):
#   src/*.cpp   -> ./bubble
#   tests/test_*.cpp -> tests/<name> binaries, run by `make check`
#
# No -ffast-math (breaks the 1e-12 reproducibility tolerance, Spec "Silent traps").
#
# Mixed precision: one build.  src/advect.cpp is compiled by clang with the
# RAPTOR pass (-DBUBBLE_RAPTOR) whenever the RAPTOR install is found, so the
# same ./bubble runs every `precision` mode (params.h) and compares it against
# its own FP64 shadow trajectory.  clang also gives _Float16 on x86, which
# g++ < 12 lacks.  `make RAPTOR=0` builds advect.cpp with $(CXX) instead
# (fp64 and lowprec modes only).

CXX      := mpicxx
# OMPI_SKIP_MPICXX: only the C API is used; the deprecated C++ bindings
# trip -Werror (cast-function-type) with newer compilers.
CXXFLAGS := -std=c++17 -O2 -g -Wall -Wextra -Werror -MMD -MP -DOMPI_SKIP_MPICXX
LDFLAGS  :=
LDLIBS   := -lm

# --- RAPTOR (src/advect.cpp only) -------------------------------------------
RAPTOR_INSTALL_DIR ?= $(HOME)/raptor-install
LLVM_VER           ?= 20
RAPTOR_CXX         ?= clang++-$(LLVM_VER)
RAPTOR_PLUGIN      := $(RAPTOR_INSTALL_DIR)/lib/LLVMRaptor-$(LLVM_VER).so
RAPTOR             ?= $(if $(wildcard $(RAPTOR_PLUGIN)),1,0)

ifeq ($(RAPTOR),1)
ADVECT_CXX      := $(RAPTOR_CXX)
ADVECT_CXXFLAGS := $(CXXFLAGS) -DBUBBLE_RAPTOR=1 -I$(RAPTOR_INSTALL_DIR)/include \
                   $(shell $(CXX) -showme:compile) -fpass-plugin=$(RAPTOR_PLUGIN)
LDLIBS          += -L$(RAPTOR_INSTALL_DIR)/lib -Wl,-rpath,$(RAPTOR_INSTALL_DIR)/lib \
                   -lRaptor-RT-$(LLVM_VER) -lmpfr -lstdc++
else
ADVECT_CXX      := $(CXX)
ADVECT_CXXFLAGS := $(CXXFLAGS)
endif

BIN      := bubble
SRCDIR   := src
OBJDIR   := build
TESTDIR  := tests

SRCS     := $(sort $(wildcard $(SRCDIR)/*.cpp))
OBJS     := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
# everything except main.o, so tests can link the library part
LIBOBJS  := $(filter-out $(OBJDIR)/main.o,$(OBJS))

TESTSRCS := $(sort $(wildcard $(TESTDIR)/test_*.cpp))
TESTBINS := $(patsubst $(TESTDIR)/%.cpp,$(OBJDIR)/%,$(TESTSRCS))

DEPS     := $(OBJS:.o=.d) $(TESTBINS:=.d)

# test_rank_invariance / test_halo need more than one rank
MPIRUN   := mpirun
TIMEOUT  := timeout 600

.PHONY: all clean check debug tests dirs

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -c -o $@ $<

$(OBJDIR)/advect.o: $(SRCDIR)/advect.cpp | $(OBJDIR)
	$(ADVECT_CXX) $(ADVECT_CXXFLAGS) -I$(SRCDIR) -c -o $@ $<

$(OBJDIR)/%: $(TESTDIR)/%.cpp $(LIBOBJS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) $(LDFLAGS) -o $@ $< $(LIBOBJS) $(LDLIBS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

tests: $(TESTBINS)

# Each test binary is an MPI program. Ranks it should run on are declared by the
# test itself through a `# ranks: ...` comment on its first lines; default 1.
check: $(BIN) $(TESTBINS)
	@sh tests/run_tests.sh "$(OBJDIR)" $(TESTBINS)

debug:
	$(MAKE) clean
	$(MAKE) CXXFLAGS="-std=c++17 -O1 -g -Wall -Wextra -Werror -MMD -MP -DOMPI_SKIP_MPICXX -fsanitize=address,undefined" \
	        LDFLAGS="-fsanitize=address,undefined" all tests

clean:
	rm -rf $(OBJDIR) $(BIN)

-include $(DEPS)
