#!/bin/bash -x

clang++ -c main.cpp -O2
clang++ -c compute.cpp -O2
clang++ -o main compute.o main.o
