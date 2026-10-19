# Compiler and flags
CXX = clang++-fpchecker
CXXFLAGS = -Wno-format -std=c++11 -g -O2

# Source file
SRC = lu_solve.cpp
OBJ = $(SRC:.cpp=.o)
EXECUTABLE_NAME = lu_solve

# Library location and name
LIB_DIR = ../../common
LIB_NAME = solverslib
LIB_FILE = $(LIB_DIR)/lib$(LIB_NAME).a

# Default target
all: $(EXECUTABLE_NAME)

# Rule to compile the source file
$(OBJ): $(SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to link with the library
$(EXECUTABLE_NAME): $(OBJ) $(LIB_FILE)
	$(CXX) $(CXXFLAGS) -o $@ $^ -L$(LIB_DIR) -l$(LIB_NAME)

# Clean target
clean:
	rm -rf $(OBJ) $(EXECUTABLE_NAME) .fpc_logs fpc-report

.PHONY: all clean
