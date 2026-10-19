# Compiler and flags
#CXX = clang++-fpchecker
CXX = mpic++-fpchecker
CXXFLAGS = -Wno-format -std=c++11 -g -O2

# Source file
SRC = heat_mpi.cpp
OBJ = $(SRC:.cpp=.o)
EXECUTABLE_NAME = heat_mpi

# Default target
all: $(EXECUTABLE_NAME)

# Rule to compile the source file
$(OBJ): $(SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to link with the library
$(EXECUTABLE_NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Clean target
clean:
	rm -rf $(OBJ) $(EXECUTABLE_NAME) fpc-report .fpc_logs

.PHONY: all clean
