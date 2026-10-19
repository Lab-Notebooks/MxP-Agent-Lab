
NAME=superlu_test

CXX=clang-fpchecker
SU_PATH=/tmp/tutorial/examples/superlu/build/install
OP=-Wno-deprecated-declarations -g

all:
	$(CXX) -c $(NAME).c $(OP) -I$(SU_PATH)/include
	$(CXX) $(NAME).o -o $(NAME) -L$(SU_PATH)/lib -lsuperlu -lblas -llapack -lm

clean:
	rm -rf *.o $(NAME) fpc-report .fpc_logs
