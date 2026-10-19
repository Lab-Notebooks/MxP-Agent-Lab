

all:
	clang -o test_exec test_exec.c
	clang -shared -o libmy_exec_override.so -fPIC my_exec_override.c -ldl
