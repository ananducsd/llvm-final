# if your instrumentation code calls into LLVM libraries, then comment out the above and use these instead:
CPPFLAGS=`llvm-config --cppflags`
LLVMLIBS=`llvm-config --libs`
LDFLAGS=`llvm-config --ldflags`

function runAnalysisOn {
## compile the instrumentation module to bitcode
clang $CPPFLAGS -O0 -emit-llvm -c $INSTRUMENTATION/branchbias/bias_counter.cpp -o bias_counter.bc

## run pass
opt -load $LLVMLIB/CSE231.so -TestBranchBias < $BENCHMARKS/$1/$1.bc > temp.instrumented.bc

## link instrumentation module
llvm-link temp.instrumented.bc bias_counter.bc -o $1.linked.bc

## compile to native object file
llc -filetype=obj $1.linked.bc -o=$1.o

## generate native executable
g++ $1.o $LLVMLIBS $LDFLAGS -o $1.exe

./$1.exe
}

target=$1

echo $target

if [ "${target:(-1)}" = "/" ]
then
        target=`echo ${target:0:(-1)}`
        echo $target
fi


target=`echo $target | awk -F "/" '{printf("%s", $NF) }' `

echo $target
echo "Running Analysis on $target"
if [ "$target" = "hadamard" ]
then
        cp $BENCHMARKS/hadamard/out.gold.dat .
fi
runAnalysisOn $target
