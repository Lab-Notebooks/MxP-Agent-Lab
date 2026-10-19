# Compiler and flags
CXX = clang++-fpchecker
CXXFLAGS = -Wall -Wextra -std=c++11 -g -O2

# Source file
SRC = van_der_pol.cpp
OBJ = $(SRC:.cpp=.o)
EXECUTABLE_NAME = van_der_pol

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
