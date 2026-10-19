# Compiler and flags
CXX = clang++-fpchecker
CXXFLAGS = -Wno-format -std=c++17 -g -O2
AR = ar
ARFLAGS = rcs

# Project names
LIB_NAME = solverslib
EXECUTABLE_NAME = tests_LU_solver

# Source files
SRCS = blas.cpp linear_solvers.cpp io.cpp
TEST_SRC = tests_LU_solver.cpp

# Header files (optional, but good to list)
HDRS = blas.hpp linear_solvers.hpp io.hpp

# Object files
OBJS = $(SRCS:.cpp=.o)
TEST_OBJ = $(TEST_SRC:.cpp=.o)

# Library file name
LIB_FILE = lib$(LIB_NAME).a

# Default target
all: $(LIB_FILE) $(EXECUTABLE_NAME)

# Rule to create the library
$(LIB_FILE): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

# Rule to compile source files into object files
%.o: %.cpp $(HDRS) # Added header dependency
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to compile the test file and link with the library
$(EXECUTABLE_NAME): $(TEST_OBJ) $(LIB_FILE)
	$(CXX) $(CXXFLAGS) -o $@ $^ -L. -l$(LIB_NAME)

# Rule to compile the test source file into object file
$(TEST_OBJ): $(TEST_SRC) $(HDRS) # Added header dependency
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(OBJS) $(TEST_OBJ) $(LIB_FILE) $(EXECUTABLE_NAME)

.PHONY: all clean
