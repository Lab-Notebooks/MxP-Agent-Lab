
OP = 	-O0 -g 
CXX = FPC_INSTRUMENT=1 clang++-fpchecker 

all:
	$(CXX) -c main.cpp $(OP)
	$(CXX) -c compute.cpp $(OP)
	$(CXX) -c printing.cpp $(OP)
	$(CXX) -o main compute.o main.o printing.o -ldl

clean:
	rm -rf *.o main __pycache__ .fpc_logs .fpc_log.txt ./fpc-report
