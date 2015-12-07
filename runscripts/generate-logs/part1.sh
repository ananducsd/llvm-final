#!/usr/bin/env bash
# linking example

#CPPFLAGS=
#LLVMLIBS=
#LDFLAGS=
# if your instrumentation code calls into LLVM libraries, then comment out the above and use these instead:
source $CSE231ROOT/startenv.sh
CPPFLAGS=`llvm-config --cppflags`
LLVMLIBS=`llvm-config --libs`
LDFLAGS=`llvm-config --ldflags`

function runAnalysisOn {
## compile the module pass
make -C $CSE231ROOT

## run pass
opt -analyze -load $LLVMLIB/CSE231.so -countStatic < $BENCHMARKS/$1/$1.bc > $OUTPUTLOGS/$1.static.log
}

cp $BENCHMARKS/hadamard/out.gold.dat .
echo "Part 1 Script - generating output logs"
for f in `ls $BENCHMARKS`
do
    #echo "OK"
    if test -d $BENCHMARKS/$f
    then
        echo "-------------------------------"
        echo "Running on " $f
        echo
        runAnalysisOn $f
        echo
    fi
done

