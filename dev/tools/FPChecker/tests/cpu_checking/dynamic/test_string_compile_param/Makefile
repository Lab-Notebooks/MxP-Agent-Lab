
#OP = 	-O2 -DCOMPILE_STRING=\\\"fpchecker\\\"
OP = 	-O2 -DCOMPILE_STRING=\"fpchecker\"
CXX = FPC_INSTRUMENT=1 clang++-fpchecker 

all:
	$(CXX) -c main.cpp $(OP)
	$(CXX) -c compute.cpp $(OP)
	$(CXX) -o main compute.o main.o

clean:
	rm -rf *.o main __pycache__ .fpc_logs .fpc_log.txt
