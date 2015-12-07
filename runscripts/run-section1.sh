# if your instrumentation code calls into LLVM libraries, then comment out the above and use these instead:
CPPFLAGS=`llvm-config --cppflags`
LLVMLIBS=`llvm-config --libs`
LDFLAGS=`llvm-config --ldflags`

function runAnalysisOn {
## compile the module pass
make -C $CSE231ROOT

## run pass
opt -analyze -load $LLVMLIB/CSE231.so -countStatic < $BENCHMARKS/$1/$1.bc

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
