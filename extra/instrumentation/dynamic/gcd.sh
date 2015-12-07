#!/usr/bin/env bash
# linking example

#CPPFLAGS=
#LLVMLIBS=
#LDFLAGS=
# if your instrumentation code calls into LLVM libraries, then comment out the above and use these instead:
CPPFLAGS=`llvm-config --cppflags`
LLVMLIBS=`llvm-config --libs`
LDFLAGS=`llvm-config --ldflags`



## compile the instrumentation module to bitcode
clang $CPPFLAGS -O0 -emit-llvm -c dynamic.cpp -o dynamic.bc

## run pass
opt -load $LLVMLIB/CSE231.so -countDynamic < $BENCHMARKS/gcd/gcd.bc > temp.instrumented.bc

## link instrumentation module
llvm-link temp.instrumented.bc dynamic.bc -o gcd.linked.bc

## compile to native object file
llc -filetype=obj gcd.linked.bc -o=gcd.o

## generate native executable
g++ gcd.o $LLVMLIBS $LDFLAGS -o gcd.exe

./gcd.exe > $OUTPUTLOGS/gcd.dynamic.log

