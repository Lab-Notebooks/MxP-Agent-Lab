
CC=/Users/lagunaperalt1/projects/fpchecker/FPChecker/build/install/bin/clang-fpchecker
FFTW_PATH=/tmp/tutorial/examples/fftw/fftw-3.3.10/fftw-install

all:
	$(CC) -c fftw_test.c -I$(FFTW_PATH)/include
	$(CC) fftw_test.o -L$(FFTW_PATH)/lib -lfftw3 -lm -o fftw_test

clean:
	rm -rf fftw_test
